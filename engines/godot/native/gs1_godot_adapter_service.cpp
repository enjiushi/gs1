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

constexpr std::uint64_t k_phone_signature_offset = 14695981039346656037ULL;
constexpr std::uint64_t k_phone_signature_prime = 1099511628211ULL;

std::uint64_t mix_phone_signature(std::uint64_t signature, std::uint64_t value) noexcept
{
    return (signature ^ value) * k_phone_signature_prime;
}

std::uint64_t phone_task_snapshot_signature(const Gs1SiteStateView& site) noexcept
{
    std::uint64_t signature =
        mix_phone_signature(
            mix_phone_signature(k_phone_signature_offset, site.task_count),
            site.site_id);
    for (std::uint32_t index = 0; index < site.task_count; ++index)
    {
        const Gs1TaskView& task = site.tasks[index];
        signature = mix_phone_signature(signature, task.task_instance_id);
        signature = mix_phone_signature(signature, task.task_template_id);
        signature = mix_phone_signature(signature, task.publisher_faction_id);
        signature = mix_phone_signature(signature, task.current_progress_amount);
        signature = mix_phone_signature(signature, task.target_amount);
        signature = mix_phone_signature(signature, task.required_count);
        signature = mix_phone_signature(signature, task.runtime_list_kind);
        signature = mix_phone_signature(signature, task.reward_draft_option_count);
    }
    return signature;
}

std::uint64_t phone_listing_snapshot_signature(
    const Gs1SiteStateView& site,
    std::uint8_t listing_kind) noexcept
{
    std::uint64_t signature =
        mix_phone_signature(k_phone_signature_offset, listing_kind);
    for (std::uint32_t index = 0; index < site.phone_listing_count; ++index)
    {
        const Gs1PhoneListingView& listing = site.phone_listings[index];
        if (listing.listing_kind != listing_kind)
        {
            continue;
        }

        signature = mix_phone_signature(signature, listing.item_or_unlockable_id);
        signature = mix_phone_signature(signature, listing.listing_id);
        signature = mix_phone_signature(signature, static_cast<std::uint64_t>(listing.price_cash_points));
        signature = mix_phone_signature(signature, listing.quantity);
        signature = mix_phone_signature(signature, listing.stock_refresh_generation);
        signature = mix_phone_signature(signature, listing.occupied);
        signature = mix_phone_signature(signature, listing.generated_from_stock);
    }
    return signature;
}

std::uint32_t count_task_list_entries(const Gs1SiteStateView& site, std::uint8_t runtime_list_kind) noexcept
{
    std::uint32_t count = 0U;
    for (std::uint32_t index = 0; index < site.task_count; ++index)
    {
        if (site.tasks[index].runtime_list_kind == runtime_list_kind)
        {
            count += 1U;
        }
    }
    return count;
}

std::uint32_t count_phone_list_entries(const Gs1SiteStateView& site, std::uint8_t listing_kind) noexcept
{
    std::uint32_t count = 0U;
    for (std::uint32_t index = 0; index < site.phone_listing_count; ++index)
    {
        if (site.phone_listings[index].listing_kind == listing_kind)
        {
            count += 1U;
        }
    }
    return count;
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

void Gs1GodotAdapterService::reset_ui_session_state() noexcept
{
    ui_session_state_ = {};
}

void Gs1GodotAdapterService::bump_ui_session_revision(std::uint32_t dirty_flags)
{
    ui_session_state_.revision += 1U;

    if (dirty_flags == GS1_PRESENTATION_DIRTY_NONE)
    {
        return;
    }

    Gs1EngineMessage message {};
    message.type = GS1_ENGINE_MESSAGE_PRESENTATION_DIRTY;
    auto& payload = message.emplace_payload<Gs1EngineMessagePresentationDirtyData>();
    payload.dirty_flags = dirty_flags;
    payload.reserved0 = 0U;
    payload.reserved1 = 0U;
    dispatch_or_buffer_engine_message(std::move(message));
}

void Gs1GodotAdapterService::sync_phone_badges_from_view(const Gs1GameStateView& view)
{
    if (view.has_active_site == 0U || view.active_site == nullptr)
    {
        ui_session_state_.phone.notification_state_initialized = false;
        ui_session_state_.phone.task_snapshot_signature = 0U;
        ui_session_state_.phone.buy_snapshot_signature = 0U;
        ui_session_state_.phone.sell_snapshot_signature = 0U;
        ui_session_state_.phone.service_snapshot_signature = 0U;
        ui_session_state_.phone.badge_flags = 0U;
        return;
    }

    const Gs1SiteStateView& site = *view.active_site;
    auto& phone = ui_session_state_.phone;
    const std::uint64_t next_task_signature = phone_task_snapshot_signature(site);
    const std::uint64_t next_buy_signature =
        phone_listing_snapshot_signature(site, GS1_PHONE_LISTING_PRESENTATION_BUY_ITEM);
    const std::uint64_t next_sell_signature =
        phone_listing_snapshot_signature(site, GS1_PHONE_LISTING_PRESENTATION_SELL_ITEM);
    const std::uint64_t next_service_signature =
        phone_listing_snapshot_signature(site, GS1_PHONE_LISTING_PRESENTATION_PURCHASE_UNLOCKABLE) ^
        phone_listing_snapshot_signature(site, GS1_PHONE_LISTING_PRESENTATION_HIRE_CONTRACTOR);

    if (phone.notification_state_initialized)
    {
        std::uint32_t new_badge_flags = 0U;
        if (phone.task_snapshot_signature != next_task_signature &&
            !(phone.open && phone.active_section == GS1_PHONE_PANEL_SECTION_TASKS))
        {
            new_badge_flags |= GS1_PHONE_PANEL_FLAG_TASKS_BADGE;
        }
        if (phone.buy_snapshot_signature != next_buy_signature &&
            !(phone.open && phone.active_section == GS1_PHONE_PANEL_SECTION_BUY))
        {
            new_badge_flags |= GS1_PHONE_PANEL_FLAG_BUY_BADGE;
        }
        if (phone.sell_snapshot_signature != next_sell_signature &&
            !(phone.open && phone.active_section == GS1_PHONE_PANEL_SECTION_SELL))
        {
            new_badge_flags |= GS1_PHONE_PANEL_FLAG_SELL_BADGE;
        }
        if (phone.service_snapshot_signature != next_service_signature &&
            !(phone.open && phone.active_section == GS1_PHONE_PANEL_SECTION_HIRE))
        {
            new_badge_flags |= GS1_PHONE_PANEL_FLAG_HIRE_BADGE;
        }

        if (new_badge_flags != 0U)
        {
            phone.badge_flags |= new_badge_flags | GS1_PHONE_PANEL_FLAG_LAUNCHER_BADGE;
        }
    }

    phone.task_snapshot_signature = next_task_signature;
    phone.buy_snapshot_signature = next_buy_signature;
    phone.sell_snapshot_signature = next_sell_signature;
    phone.service_snapshot_signature = next_service_signature;
    phone.notification_state_initialized = true;
}

void Gs1GodotAdapterService::sync_ui_session_from_view(const Gs1GameStateView& view)
{
    ui_session_state_.app_state = view.app_state;
    sync_phone_badges_from_view(view);

    if (view.has_active_site == 0U || view.active_site == nullptr)
    {
        ui_session_state_.inventory.worker_pack_open = false;
        return;
    }
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
    reset_ui_session_state();
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
    if (!runtime_session_.is_running())
    {
        last_error_ = "Runtime session is not ready to drain engine messages.";
        return false;
    }

    Gs1EngineMessage message {};
    while (true)
    {
        if (!runtime_session_.pop_engine_message(message))
        {
            const std::string& session_error = runtime_session_.last_error();
            if (session_error.empty())
            {
                break;
            }
            last_error_ = session_error;
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
    reset_ui_session_state();
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
    if (handle_local_ui_action(action))
    {
        last_error_.clear();
        return true;
    }

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
    if (storage_id > 0 &&
        handle_local_storage_view(
            static_cast<std::uint32_t>(storage_id),
            static_cast<Gs1InventoryViewEventKind>(event_kind)))
    {
        last_error_.clear();
        return true;
    }

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

bool Gs1GodotAdapterService::handle_local_storage_view(
    std::uint32_t storage_id,
    Gs1InventoryViewEventKind event_kind)
{
    Gs1GameStateView view {};
    if (!runtime_session_.get_game_state_view(view) ||
        view.has_active_site == 0U ||
        view.active_site == nullptr)
    {
        return false;
    }

    const Gs1SiteStateView& site = *view.active_site;
    if (storage_id == site.worker_pack_storage_id)
    {
        if (event_kind == GS1_INVENTORY_VIEW_EVENT_OPEN_SNAPSHOT)
        {
            ui_session_state_.inventory.worker_pack_open = true;
            ui_session_state_.phone.open = false;
            ui_session_state_.regional_tech.open = false;
            ui_session_state_.protection.selector_open = false;
            ui_session_state_.protection.overlay_mode = GS1_SITE_PROTECTION_OVERLAY_NONE;
            bump_ui_session_revision(GS1_PRESENTATION_DIRTY_SITE | GS1_PRESENTATION_DIRTY_HUD);
            return true;
        }

        if (event_kind == GS1_INVENTORY_VIEW_EVENT_CLOSE)
        {
            ui_session_state_.inventory.worker_pack_open = false;
            bump_ui_session_revision(GS1_PRESENTATION_DIRTY_SITE | GS1_PRESENTATION_DIRTY_HUD);
            return true;
        }
    }

    return false;
}

bool Gs1GodotAdapterService::handle_local_ui_action(const Gs1UiAction& action)
{
    switch (action.type)
    {
    case GS1_UI_ACTION_SET_PHONE_PANEL_SECTION:
    {
        if (action.arg0 > static_cast<std::uint64_t>(GS1_PHONE_PANEL_SECTION_CART))
        {
            return false;
        }

        auto& phone = ui_session_state_.phone;
        const auto requested_section = static_cast<Gs1PhonePanelSection>(action.arg0);
        const std::uint32_t clear_badge_mask = [&]() -> std::uint32_t
        {
            switch (requested_section)
            {
            case GS1_PHONE_PANEL_SECTION_TASKS:
                return GS1_PHONE_PANEL_FLAG_TASKS_BADGE;
            case GS1_PHONE_PANEL_SECTION_BUY:
                return GS1_PHONE_PANEL_FLAG_BUY_BADGE;
            case GS1_PHONE_PANEL_SECTION_SELL:
                return GS1_PHONE_PANEL_FLAG_SELL_BADGE;
            case GS1_PHONE_PANEL_SECTION_HIRE:
                return GS1_PHONE_PANEL_FLAG_HIRE_BADGE;
            default:
                return 0U;
            }
        }();

        phone.open = true;
        phone.active_section = requested_section;
        phone.badge_flags &= ~(GS1_PHONE_PANEL_FLAG_LAUNCHER_BADGE | clear_badge_mask);
        ui_session_state_.inventory.worker_pack_open = false;
        ui_session_state_.protection.selector_open = false;
        ui_session_state_.protection.overlay_mode = GS1_SITE_PROTECTION_OVERLAY_NONE;
        ui_session_state_.regional_tech.open = false;
        bump_ui_session_revision(
            GS1_PRESENTATION_DIRTY_PHONE |
            GS1_PRESENTATION_DIRTY_HUD |
            GS1_PRESENTATION_DIRTY_SITE);
        return true;
    }

    case GS1_UI_ACTION_CLOSE_PHONE_PANEL:
        ui_session_state_.phone.open = false;
        ui_session_state_.phone.badge_flags &= ~GS1_PHONE_PANEL_FLAG_LAUNCHER_BADGE;
        bump_ui_session_revision(GS1_PRESENTATION_DIRTY_PHONE | GS1_PRESENTATION_DIRTY_HUD);
        return true;

    case GS1_UI_ACTION_OPEN_REGIONAL_MAP_TECH_TREE:
        ui_session_state_.regional_tech.open = true;
        ui_session_state_.phone.open = false;
        ui_session_state_.inventory.worker_pack_open = false;
        ui_session_state_.protection.selector_open = false;
        ui_session_state_.protection.overlay_mode = GS1_SITE_PROTECTION_OVERLAY_NONE;
        bump_ui_session_revision(GS1_PRESENTATION_DIRTY_REGIONAL_MAP | GS1_PRESENTATION_DIRTY_SITE);
        return true;

    case GS1_UI_ACTION_CLOSE_REGIONAL_MAP_TECH_TREE:
        ui_session_state_.regional_tech.open = false;
        bump_ui_session_revision(GS1_PRESENTATION_DIRTY_REGIONAL_MAP);
        return true;

    case GS1_UI_ACTION_SELECT_TECH_TREE_FACTION_TAB:
        if (action.target_id == 0U)
        {
            return false;
        }
        ui_session_state_.regional_tech.open = true;
        ui_session_state_.regional_tech.selected_faction_id = action.target_id;
        bump_ui_session_revision(GS1_PRESENTATION_DIRTY_REGIONAL_MAP);
        return true;

    case GS1_UI_ACTION_OPEN_SITE_PROTECTION_SELECTOR:
        ui_session_state_.protection.selector_open = true;
        ui_session_state_.protection.overlay_mode = GS1_SITE_PROTECTION_OVERLAY_NONE;
        ui_session_state_.phone.open = false;
        ui_session_state_.inventory.worker_pack_open = false;
        ui_session_state_.regional_tech.open = false;
        bump_ui_session_revision(GS1_PRESENTATION_DIRTY_SITE | GS1_PRESENTATION_DIRTY_HUD);
        return true;

    case GS1_UI_ACTION_CLOSE_SITE_PROTECTION_UI:
        ui_session_state_.protection.selector_open = false;
        ui_session_state_.protection.overlay_mode = GS1_SITE_PROTECTION_OVERLAY_NONE;
        bump_ui_session_revision(GS1_PRESENTATION_DIRTY_SITE | GS1_PRESENTATION_DIRTY_HUD);
        return true;

    case GS1_UI_ACTION_SET_SITE_PROTECTION_OVERLAY_MODE:
        ui_session_state_.protection.selector_open = false;
        ui_session_state_.protection.overlay_mode =
            static_cast<Gs1SiteProtectionOverlayMode>(action.arg0);
        bump_ui_session_revision(GS1_PRESENTATION_DIRTY_SITE | GS1_PRESENTATION_DIRTY_HUD);
        return true;

    default:
        return false;
    }
}

bool Gs1GodotAdapterService::get_game_state_view(Gs1GameStateView& out_view)
{
    if (!runtime_session_.get_game_state_view(out_view))
    {
        last_error_ = runtime_session_.last_error();
        return false;
    }

    sync_ui_session_from_view(out_view);
    last_error_.clear();
    return true;
}

bool Gs1GodotAdapterService::query_site_tile_view(std::uint32_t tile_index, Gs1SiteTileView& out_tile)
{
    if (!runtime_session_.query_site_tile_view(tile_index, out_tile))
    {
        last_error_ = runtime_session_.last_error();
        return false;
    }

    last_error_.clear();
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
