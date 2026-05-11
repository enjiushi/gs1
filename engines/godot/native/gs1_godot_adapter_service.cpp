#include "gs1_godot_adapter_service.h"
#include "gs1_godot_engine_message_policy.h"

#include "host/adapter_metadata_catalog.h"

#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <string_view>

using namespace godot;

namespace
{
std::filesystem::path globalize_res_path(std::string_view res_path)
{
    ProjectSettings* project_settings = ProjectSettings::get_singleton();
    if (project_settings == nullptr)
    {
        return std::filesystem::path {res_path};
    }

    const String absolute_path = project_settings->globalize_path(
        String::utf8(res_path.data(), static_cast<int64_t>(res_path.size())));
    return std::filesystem::path {absolute_path.utf8().get_data()};
}

}

void Gs1GodotAdapterService::fail_runtime_session(const char* fallback_error_message)
{
    if (last_error_.empty() && fallback_error_message != nullptr && fallback_error_message[0] != '\0')
    {
        last_error_ = fallback_error_message;
    }

#ifndef NDEBUG
    UtilityFunctions::push_error(
        String("GS1 Godot adapter runtime failure: ") + String(last_error_.c_str()));
    assert(false && "GS1 Godot adapter hit an unexpected runtime/session failure; inspect last_error_.");
#endif

    runtime_session_.stop();
    notify_runtime_message_reset();
}

Gs1GodotAdapterService::~Gs1GodotAdapterService()
{
    runtime_session_.stop();
}

void Gs1GodotAdapterService::begin_engine_message_buffering()
{
    engine_message_buffering_active_ = true;
}

void Gs1GodotAdapterService::flush_buffered_engine_messages()
{
    engine_message_buffering_active_ = false;
    if (buffered_engine_messages_.empty())
    {
        return;
    }

    std::vector<Gs1EngineMessage> buffered_messages {};
    buffered_messages.swap(buffered_engine_messages_);
    for (Gs1EngineMessage& message : buffered_messages)
    {
        dispatch_engine_message(std::move(message));
    }
}

void Gs1GodotAdapterService::clear_buffered_engine_messages() noexcept
{
    engine_message_buffering_active_ = false;
    buffered_engine_messages_.clear();
}

void Gs1GodotAdapterService::begin_frame(double delta_seconds)
{
    ensure_debug_http_server_initialized();

    if (!runtime_session_.is_running())
    {
        ensure_runtime_started();
    }
    if (!runtime_session_.is_running())
    {
        return;
    }

    if (phase2_pending_)
    {
        return;
    }

    if (!drain_debug_http_commands())
    {
        fail_runtime_session("Failed to drain queued Godot debug HTTP commands.");
        return;
    }

    Gs1Phase1Result phase1 {};
    pending_phase1_delta_seconds_ = delta_seconds > 0.0 ? delta_seconds : (1.0 / 60.0);
    if (!runtime_session_.run_phase1(pending_phase1_delta_seconds_, phase1))
    {
        last_error_ = runtime_session_.last_error();
        fail_runtime_session("Runtime session phase 1 failed.");
        return;
    }

    if (!drain_projection_messages())
    {
        fail_runtime_session("Failed to drain runtime phase 1 projection messages.");
        return;
    }

    phase2_pending_ = true;
    last_error_.clear();
}

void Gs1GodotAdapterService::finish_frame()
{
    if (!runtime_session_.is_running() || !phase2_pending_)
    {
        return;
    }

    if (!drain_debug_http_commands())
    {
        fail_runtime_session("Failed to drain queued Godot debug HTTP commands before phase 2.");
        return;
    }

    Gs1Phase2Result phase2 {};
    if (!runtime_session_.run_phase2(phase2))
    {
        last_error_ = runtime_session_.last_error();
        fail_runtime_session("Runtime session phase 2 failed.");
        return;
    }

    phase2_pending_ = false;
    last_error_.clear();
}

void Gs1GodotAdapterService::subscribe(Gs1EngineMessageType type, IGs1GodotEngineMessageSubscriber& subscriber)
{
    remember_subscriber(subscriber);
    auto& bucket = subscribers_by_message_[bucket_index(type)];
    if (std::find(bucket.begin(), bucket.end(), &subscriber) == bucket.end())
    {
#ifndef NDEBUG
        if (gs1_godot_engine_message_requires_exclusive_subscriber(type))
        {
            assert(bucket.empty() && "Snapshot-style engine messages must have exactly one subscribing controller.");
        }
#endif
        bucket.push_back(&subscriber);
    }
}

void Gs1GodotAdapterService::subscribe_matching_messages(IGs1GodotEngineMessageSubscriber& subscriber)
{
    remember_subscriber(subscriber);
    for (std::size_t type_value = 0; type_value < k_message_bucket_count; ++type_value)
    {
        const auto type = static_cast<Gs1EngineMessageType>(type_value);
        if (!subscriber.handles_engine_message(type))
        {
            continue;
        }
        auto& bucket = subscribers_by_message_[type_value];
        if (std::find(bucket.begin(), bucket.end(), &subscriber) == bucket.end())
        {
#ifndef NDEBUG
            if (gs1_godot_engine_message_requires_exclusive_subscriber(type))
            {
                assert(bucket.empty() && "Snapshot-style engine messages must have exactly one subscribing controller.");
            }
#endif
            bucket.push_back(&subscriber);
        }
    }
}

void Gs1GodotAdapterService::unsubscribe(Gs1EngineMessageType type, IGs1GodotEngineMessageSubscriber& subscriber)
{
    auto& bucket = subscribers_by_message_[bucket_index(type)];
    bucket.erase(std::remove(bucket.begin(), bucket.end(), &subscriber), bucket.end());
}

void Gs1GodotAdapterService::unsubscribe_matching_messages(IGs1GodotEngineMessageSubscriber& subscriber)
{
    for (auto& bucket : subscribers_by_message_)
    {
        bucket.erase(std::remove(bucket.begin(), bucket.end(), &subscriber), bucket.end());
    }
}

void Gs1GodotAdapterService::unsubscribe_all(IGs1GodotEngineMessageSubscriber& subscriber)
{
    unsubscribe_matching_messages(subscriber);
    known_subscribers_.erase(&subscriber);
}

void Gs1GodotAdapterService::ensure_debug_http_server_initialized()
{
    if (debug_http_server_checked_)
    {
        return;
    }

    debug_http_server_checked_ = true;

    const char* raw_port = std::getenv("GS1_GODOT_DEBUG_HTTP_PORT");
    if (raw_port == nullptr || raw_port[0] == '\0')
    {
        return;
    }

    char* parse_end = nullptr;
    const unsigned long configured_port = std::strtoul(raw_port, &parse_end, 10);
    if (parse_end == raw_port || parse_end == nullptr || parse_end[0] != '\0' || configured_port > 65535UL)
    {
        UtilityFunctions::push_warning(
            String("GS1_GODOT_DEBUG_HTTP_PORT must be an integer in the 0-65535 range."));
        return;
    }

    std::string start_error {};
    if (!debug_http_server_.start(
            static_cast<std::uint16_t>(configured_port),
            [this](std::string_view path, const std::string& body, std::string& out_error) {
                return queue_debug_http_command(path, body, out_error);
            },
            start_error))
    {
        UtilityFunctions::push_warning(
            String("GS1 Godot debug HTTP control failed to start: ") + String(start_error.c_str()));
        return;
    }

    UtilityFunctions::print(
        String("GS1 Godot debug HTTP control listening at http://127.0.0.1:") +
        String::num_int64(static_cast<std::int64_t>(debug_http_server_.port())));
}

void Gs1GodotAdapterService::ensure_runtime_started()
{
    if (runtime_session_.is_running())
    {
        return;
    }

    if (gameplay_dll_path_.empty())
    {
        refresh_gameplay_dll_path();
    }
    if (project_config_root_.empty())
    {
        refresh_project_config_root();
    }

    adapter_config_ = load_adapter_config_blob(
        project_config_root_,
        globalize_res_path("res://gs1/godot_config"));

    notify_runtime_message_reset();
    if (!runtime_session_.start(gameplay_dll_path_, project_config_root_, &adapter_config_))
    {
        last_error_ = runtime_session_.last_error();
        return;
    }

    if (!drain_projection_messages())
    {
        fail_runtime_session("Failed to drain startup projection messages.");
        return;
    }

    last_error_.clear();
}

bool Gs1GodotAdapterService::drain_debug_http_commands()
{
    std::vector<Gs1GodotDebugHttpCommand> pending_commands {};
    {
        std::scoped_lock lock {pending_debug_http_commands_mutex_};
        pending_commands.swap(pending_debug_http_commands_);
    }

    for (const Gs1GodotDebugHttpCommand& command : pending_commands)
    {
        bool ok = false;
        switch (command.kind)
        {
        case Gs1GodotDebugHttpCommandKind::UiAction:
            ok = submit_ui_action(command.action_type, command.target_id, command.arg0, command.arg1);
            break;
        case Gs1GodotDebugHttpCommandKind::SiteAction:
            ok = submit_site_action_request(
                command.action_kind,
                command.flags,
                command.quantity,
                command.tile_x,
                command.tile_y,
                command.primary_subject_id,
                command.secondary_subject_id,
                command.item_id);
            break;
        case Gs1GodotDebugHttpCommandKind::SiteActionCancel:
            ok = submit_site_action_cancel(command.action_id, command.flags);
            break;
        case Gs1GodotDebugHttpCommandKind::SiteStorageView:
            ok = submit_site_storage_view(command.storage_id, command.event_kind);
            break;
        case Gs1GodotDebugHttpCommandKind::SiteInventorySlotTap:
            ok = submit_site_inventory_slot_tap(
                command.storage_id,
                command.container_kind,
                command.slot_index,
                command.item_instance_id);
            break;
        case Gs1GodotDebugHttpCommandKind::SiteContextRequest:
            ok = submit_site_context_request(command.tile_x, command.tile_y, command.flags);
            break;
        case Gs1GodotDebugHttpCommandKind::SiteMoveDirection:
            ok = submit_move_direction(command.world_move_x, command.world_move_y, command.world_move_z);
            break;
        default:
            last_error_ = "Unknown queued Godot debug HTTP command kind.";
            return false;
        }

        if (!ok)
        {
            return false;
        }
    }

    return true;
}

bool Gs1GodotAdapterService::drain_projection_messages()
{
    const Gs1RuntimeApi* api = runtime_session_.api();
    if (api == nullptr || api->pop_engine_message == nullptr || runtime_session_.runtime() == nullptr)
    {
        last_error_ = "Runtime session is not ready to drain engine messages.";
        return false;
    }

    Gs1EngineMessage message {};
    while (true)
    {
        const Gs1Status status = api->pop_engine_message(runtime_session_.runtime(), &message);
        if (status == GS1_STATUS_BUFFER_EMPTY)
        {
            break;
        }
        if (status != GS1_STATUS_OK)
        {
            last_error_ = "gs1_pop_engine_message failed with status " + std::to_string(static_cast<unsigned>(status));
            return false;
        }

        dispatch_or_buffer_engine_message(std::move(message));
    }

    return true;
}

void Gs1GodotAdapterService::notify_runtime_message_reset()
{
    clear_buffered_engine_messages();
    phase2_pending_ = false;
    for (IGs1GodotEngineMessageSubscriber* subscriber : known_subscribers_)
    {
        if (subscriber != nullptr)
        {
            subscriber->handle_runtime_message_reset();
        }
    }
}

void Gs1GodotAdapterService::dispatch_or_buffer_engine_message(Gs1EngineMessage&& message)
{
    if (engine_message_buffering_active_)
    {
        buffered_engine_messages_.push_back(std::move(message));
        return;
    }

    dispatch_engine_message(std::move(message));
}

void Gs1GodotAdapterService::dispatch_engine_message(Gs1EngineMessage&& message)
{
    const std::vector<IGs1GodotEngineMessageSubscriber*> subscribers =
        subscribers_by_message_[bucket_index(message.type)];
    for (IGs1GodotEngineMessageSubscriber* subscriber : subscribers)
    {
        if (subscriber != nullptr)
        {
            subscriber->handle_engine_message(message);
        }
    }
}

bool Gs1GodotAdapterService::submit_ui_action(const Gs1UiAction& action)
{
    Gs1HostEvent event {};
    event.type = GS1_HOST_EVENT_UI_ACTION;
    event.payload.ui_action.action = action;
    if (!runtime_session_.submit_host_events(&event, 1U))
    {
        last_error_ = runtime_session_.last_error();
        return false;
    }
    return true;
}

bool Gs1GodotAdapterService::submit_ui_action(std::int64_t action_type, std::int64_t target_id, std::int64_t arg0, std::int64_t arg1)
{
    Gs1UiAction action {};
    action.type = static_cast<Gs1UiActionType>(action_type);
    action.target_id = static_cast<std::uint32_t>(target_id);
    action.arg0 = static_cast<std::uint64_t>(arg0);
    action.arg1 = static_cast<std::uint64_t>(arg1);
    return submit_ui_action(action);
}

bool Gs1GodotAdapterService::submit_move_direction(double world_move_x, double world_move_y, double world_move_z)
{
    Gs1HostEvent event {};
    event.type = GS1_HOST_EVENT_SITE_MOVE_DIRECTION;
    event.payload.site_move_direction.world_move_x = static_cast<float>(world_move_x);
    event.payload.site_move_direction.world_move_y = static_cast<float>(world_move_y);
    event.payload.site_move_direction.world_move_z = static_cast<float>(world_move_z);
    if (!runtime_session_.submit_host_events(&event, 1U))
    {
        last_error_ = runtime_session_.last_error();
        return false;
    }
    return true;
}

bool Gs1GodotAdapterService::submit_site_context_request(std::int64_t tile_x, std::int64_t tile_y, std::int64_t flags)
{
    Gs1HostEvent event {};
    event.type = GS1_HOST_EVENT_SITE_CONTEXT_REQUEST;
    event.payload.site_context_request.tile_x = static_cast<std::int32_t>(tile_x);
    event.payload.site_context_request.tile_y = static_cast<std::int32_t>(tile_y);
    event.payload.site_context_request.flags = static_cast<std::uint32_t>(flags);
    if (!runtime_session_.submit_host_events(&event, 1U))
    {
        last_error_ = runtime_session_.last_error();
        return false;
    }
    return true;
}

bool Gs1GodotAdapterService::submit_site_action_request(
    std::int64_t action_kind,
    std::int64_t flags,
    std::int64_t quantity,
    std::int64_t tile_x,
    std::int64_t tile_y,
    std::int64_t primary_subject_id,
    std::int64_t secondary_subject_id,
    std::int64_t item_id)
{
    Gs1HostEvent event {};
    event.type = GS1_HOST_EVENT_SITE_ACTION_REQUEST;
    auto& request = event.payload.site_action_request;
    request.action_kind = static_cast<Gs1SiteActionKind>(action_kind);
    request.flags = static_cast<std::uint8_t>(flags);
    request.quantity = static_cast<std::uint16_t>(quantity);
    request.target_tile_x = static_cast<std::int32_t>(tile_x);
    request.target_tile_y = static_cast<std::int32_t>(tile_y);
    request.primary_subject_id = static_cast<std::uint32_t>(primary_subject_id);
    request.secondary_subject_id = static_cast<std::uint32_t>(secondary_subject_id);
    request.item_id = static_cast<std::uint32_t>(item_id);
    if (!runtime_session_.submit_host_events(&event, 1U))
    {
        last_error_ = runtime_session_.last_error();
        return false;
    }
    return true;
}

bool Gs1GodotAdapterService::submit_site_action_cancel(std::int64_t action_id, std::int64_t flags)
{
    Gs1HostEvent event {};
    event.type = GS1_HOST_EVENT_SITE_ACTION_CANCEL;
    event.payload.site_action_cancel.action_id = static_cast<std::uint32_t>(action_id);
    event.payload.site_action_cancel.flags = static_cast<std::uint32_t>(flags);
    if (!runtime_session_.submit_host_events(&event, 1U))
    {
        last_error_ = runtime_session_.last_error();
        return false;
    }
    return true;
}

bool Gs1GodotAdapterService::submit_site_storage_view(std::int64_t storage_id, std::int64_t event_kind)
{
    Gs1HostEvent event {};
    event.type = GS1_HOST_EVENT_SITE_STORAGE_VIEW;
    event.payload.site_storage_view.storage_id = static_cast<std::uint32_t>(storage_id);
    event.payload.site_storage_view.event_kind = static_cast<Gs1InventoryViewEventKind>(event_kind);
    if (!runtime_session_.submit_host_events(&event, 1U))
    {
        last_error_ = runtime_session_.last_error();
        return false;
    }
    return true;
}

bool Gs1GodotAdapterService::submit_site_inventory_slot_tap(
    std::int64_t storage_id,
    std::int64_t container_kind,
    std::int64_t slot_index,
    std::int64_t item_instance_id)
{
    Gs1HostEvent event {};
    event.type = GS1_HOST_EVENT_SITE_INVENTORY_SLOT_TAP;
    event.payload.site_inventory_slot_tap.storage_id = static_cast<std::uint32_t>(storage_id);
    event.payload.site_inventory_slot_tap.container_kind = static_cast<Gs1InventoryContainerKind>(container_kind);
    event.payload.site_inventory_slot_tap.slot_index = static_cast<std::uint16_t>(slot_index);
    event.payload.site_inventory_slot_tap.item_instance_id = static_cast<std::uint32_t>(item_instance_id);
    if (!runtime_session_.submit_host_events(&event, 1U))
    {
        last_error_ = runtime_session_.last_error();
        return false;
    }
    return true;
}

bool Gs1GodotAdapterService::submit_site_scene_ready()
{
    Gs1HostEvent event {};
    event.type = GS1_HOST_EVENT_SITE_SCENE_READY;
    if (!runtime_session_.submit_host_events(&event, 1U))
    {
        last_error_ = runtime_session_.last_error();
        return false;
    }
    return true;
}

bool Gs1GodotAdapterService::queue_debug_http_command(
    std::string_view path,
    const std::string& body,
    std::string& out_error)
{
    Gs1GodotDebugHttpCommand command {};
    if (!gs1_parse_godot_debug_http_command(path, body, command, &out_error))
    {
        return false;
    }

    std::scoped_lock lock {pending_debug_http_commands_mutex_};
    pending_debug_http_commands_.push_back(command);
    return true;
}

void Gs1GodotAdapterService::refresh_gameplay_dll_path()
{
    gameplay_dll_path_ = std::filesystem::path {compute_default_gameplay_dll_path()};
}

void Gs1GodotAdapterService::refresh_project_config_root()
{
    project_config_root_ = std::filesystem::path {compute_default_project_config_root()};
    load_adapter_metadata_catalog_from_project_root(project_config_root_);
}

std::string Gs1GodotAdapterService::compute_default_gameplay_dll_path() const
{
    return globalize_res_path("res://bin/win64/gs1_game.dll").string();
}

std::string Gs1GodotAdapterService::compute_default_project_config_root() const
{
    return globalize_res_path("res://gs1/runtime").string();
}

void Gs1GodotAdapterService::remember_subscriber(IGs1GodotEngineMessageSubscriber& subscriber)
{
    if (known_subscribers_.insert(&subscriber).second)
    {
        subscriber.handle_runtime_message_reset();
    }
}

std::size_t Gs1GodotAdapterService::bucket_index(Gs1EngineMessageType type) noexcept
{
    return static_cast<std::size_t>(type);
}
