#include "gs1_godot_adapter_service.h"

#include "host/adapter_metadata_catalog.h"

#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/variant/string.hpp>

#include <algorithm>
#include <cassert>
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

[[nodiscard]] bool is_exclusive_snapshot_message(Gs1EngineMessageType type) noexcept
{
    switch (type)
    {
    case GS1_ENGINE_MESSAGE_BEGIN_REGIONAL_MAP_SNAPSHOT:
    case GS1_ENGINE_MESSAGE_REGIONAL_MAP_SITE_UPSERT:
    case GS1_ENGINE_MESSAGE_REGIONAL_MAP_SITE_REMOVE:
    case GS1_ENGINE_MESSAGE_REGIONAL_MAP_LINK_UPSERT:
    case GS1_ENGINE_MESSAGE_REGIONAL_MAP_LINK_REMOVE:
    case GS1_ENGINE_MESSAGE_END_REGIONAL_MAP_SNAPSHOT:
    case GS1_ENGINE_MESSAGE_BEGIN_REGIONAL_MAP_HUD_SNAPSHOT:
    case GS1_ENGINE_MESSAGE_REGIONAL_MAP_HUD_SITE_UPSERT:
    case GS1_ENGINE_MESSAGE_REGIONAL_MAP_HUD_SITE_REMOVE:
    case GS1_ENGINE_MESSAGE_REGIONAL_MAP_HUD_LINK_UPSERT:
    case GS1_ENGINE_MESSAGE_REGIONAL_MAP_HUD_LINK_REMOVE:
    case GS1_ENGINE_MESSAGE_END_REGIONAL_MAP_HUD_SNAPSHOT:
    case GS1_ENGINE_MESSAGE_BEGIN_REGIONAL_SUMMARY_SNAPSHOT:
    case GS1_ENGINE_MESSAGE_REGIONAL_SUMMARY_SITE_UPSERT:
    case GS1_ENGINE_MESSAGE_REGIONAL_SUMMARY_SITE_REMOVE:
    case GS1_ENGINE_MESSAGE_REGIONAL_SUMMARY_LINK_UPSERT:
    case GS1_ENGINE_MESSAGE_REGIONAL_SUMMARY_LINK_REMOVE:
    case GS1_ENGINE_MESSAGE_END_REGIONAL_SUMMARY_SNAPSHOT:
    case GS1_ENGINE_MESSAGE_BEGIN_REGIONAL_SELECTION_SNAPSHOT:
    case GS1_ENGINE_MESSAGE_REGIONAL_SELECTION_SITE_UPSERT:
    case GS1_ENGINE_MESSAGE_REGIONAL_SELECTION_SITE_REMOVE:
    case GS1_ENGINE_MESSAGE_REGIONAL_SELECTION_LINK_UPSERT:
    case GS1_ENGINE_MESSAGE_REGIONAL_SELECTION_LINK_REMOVE:
    case GS1_ENGINE_MESSAGE_END_REGIONAL_SELECTION_SNAPSHOT:
    case GS1_ENGINE_MESSAGE_BEGIN_SITE_SNAPSHOT:
    case GS1_ENGINE_MESSAGE_SITE_TILE_UPSERT:
    case GS1_ENGINE_MESSAGE_SITE_WORKER_UPDATE:
    case GS1_ENGINE_MESSAGE_SITE_CAMP_UPDATE:
    case GS1_ENGINE_MESSAGE_SITE_WEATHER_UPDATE:
    case GS1_ENGINE_MESSAGE_SITE_INVENTORY_SLOT_UPSERT:
    case GS1_ENGINE_MESSAGE_SITE_TASK_UPSERT:
    case GS1_ENGINE_MESSAGE_SITE_TASK_REMOVE:
    case GS1_ENGINE_MESSAGE_SITE_PHONE_LISTING_UPSERT:
    case GS1_ENGINE_MESSAGE_SITE_PHONE_LISTING_REMOVE:
    case GS1_ENGINE_MESSAGE_END_SITE_SNAPSHOT:
    case GS1_ENGINE_MESSAGE_SITE_INVENTORY_STORAGE_UPSERT:
    case GS1_ENGINE_MESSAGE_SITE_INVENTORY_VIEW_STATE:
    case GS1_ENGINE_MESSAGE_SITE_MODIFIER_LIST_BEGIN:
    case GS1_ENGINE_MESSAGE_SITE_MODIFIER_UPSERT:
    case GS1_ENGINE_MESSAGE_SITE_PLANT_VISUAL_UPSERT:
    case GS1_ENGINE_MESSAGE_SITE_PLANT_VISUAL_REMOVE:
    case GS1_ENGINE_MESSAGE_SITE_DEVICE_VISUAL_UPSERT:
    case GS1_ENGINE_MESSAGE_SITE_DEVICE_VISUAL_REMOVE:
    case GS1_ENGINE_MESSAGE_BEGIN_SITE_SCENE_SNAPSHOT:
    case GS1_ENGINE_MESSAGE_SITE_SCENE_TILE_UPSERT:
    case GS1_ENGINE_MESSAGE_SITE_SCENE_WORKER_UPDATE:
    case GS1_ENGINE_MESSAGE_SITE_SCENE_PLANT_VISUAL_UPSERT:
    case GS1_ENGINE_MESSAGE_SITE_SCENE_PLANT_VISUAL_REMOVE:
    case GS1_ENGINE_MESSAGE_SITE_SCENE_DEVICE_VISUAL_UPSERT:
    case GS1_ENGINE_MESSAGE_SITE_SCENE_DEVICE_VISUAL_REMOVE:
    case GS1_ENGINE_MESSAGE_END_SITE_SCENE_SNAPSHOT:
    case GS1_ENGINE_MESSAGE_BEGIN_SITE_INVENTORY_PANEL_SNAPSHOT:
    case GS1_ENGINE_MESSAGE_SITE_INVENTORY_PANEL_STORAGE_UPSERT:
    case GS1_ENGINE_MESSAGE_SITE_INVENTORY_PANEL_SLOT_UPSERT:
    case GS1_ENGINE_MESSAGE_SITE_INVENTORY_PANEL_VIEW_STATE:
    case GS1_ENGINE_MESSAGE_END_SITE_INVENTORY_PANEL_SNAPSHOT:
    case GS1_ENGINE_MESSAGE_BEGIN_SITE_PHONE_PANEL_SNAPSHOT:
    case GS1_ENGINE_MESSAGE_SITE_PHONE_PANEL_LISTING_UPSERT:
    case GS1_ENGINE_MESSAGE_SITE_PHONE_PANEL_LISTING_REMOVE:
    case GS1_ENGINE_MESSAGE_END_SITE_PHONE_PANEL_SNAPSHOT:
    case GS1_ENGINE_MESSAGE_BEGIN_SITE_TASK_PANEL_SNAPSHOT:
    case GS1_ENGINE_MESSAGE_SITE_TASK_PANEL_TASK_UPSERT:
    case GS1_ENGINE_MESSAGE_SITE_TASK_PANEL_TASK_REMOVE:
    case GS1_ENGINE_MESSAGE_SITE_TASK_PANEL_MODIFIER_LIST_BEGIN:
    case GS1_ENGINE_MESSAGE_SITE_TASK_PANEL_MODIFIER_UPSERT:
    case GS1_ENGINE_MESSAGE_END_SITE_TASK_PANEL_SNAPSHOT:
    case GS1_ENGINE_MESSAGE_BEGIN_SITE_SUMMARY_SNAPSHOT:
    case GS1_ENGINE_MESSAGE_END_SITE_SUMMARY_SNAPSHOT:
    case GS1_ENGINE_MESSAGE_BEGIN_SITE_HUD_SNAPSHOT:
    case GS1_ENGINE_MESSAGE_SITE_HUD_STORAGE_UPSERT:
    case GS1_ENGINE_MESSAGE_END_SITE_HUD_SNAPSHOT:
    case GS1_ENGINE_MESSAGE_BEGIN_UI_PANEL:
    case GS1_ENGINE_MESSAGE_UI_PANEL_TEXT_UPSERT:
    case GS1_ENGINE_MESSAGE_UI_PANEL_SLOT_ACTION_UPSERT:
    case GS1_ENGINE_MESSAGE_UI_PANEL_LIST_ITEM_UPSERT:
    case GS1_ENGINE_MESSAGE_UI_PANEL_LIST_ACTION_UPSERT:
    case GS1_ENGINE_MESSAGE_END_UI_PANEL:
    case GS1_ENGINE_MESSAGE_CLOSE_UI_PANEL:
    case GS1_ENGINE_MESSAGE_BEGIN_REGIONAL_SELECTION_UI_PANEL:
    case GS1_ENGINE_MESSAGE_REGIONAL_SELECTION_UI_PANEL_TEXT_UPSERT:
    case GS1_ENGINE_MESSAGE_REGIONAL_SELECTION_UI_PANEL_SLOT_ACTION_UPSERT:
    case GS1_ENGINE_MESSAGE_REGIONAL_SELECTION_UI_PANEL_LIST_ITEM_UPSERT:
    case GS1_ENGINE_MESSAGE_REGIONAL_SELECTION_UI_PANEL_LIST_ACTION_UPSERT:
    case GS1_ENGINE_MESSAGE_END_REGIONAL_SELECTION_UI_PANEL:
    case GS1_ENGINE_MESSAGE_CLOSE_REGIONAL_SELECTION_UI_PANEL:
    case GS1_ENGINE_MESSAGE_BEGIN_UI_SETUP:
    case GS1_ENGINE_MESSAGE_UI_ELEMENT_UPSERT:
    case GS1_ENGINE_MESSAGE_END_UI_SETUP:
    case GS1_ENGINE_MESSAGE_CLOSE_UI_SETUP:
    case GS1_ENGINE_MESSAGE_BEGIN_PROGRESSION_VIEW:
    case GS1_ENGINE_MESSAGE_PROGRESSION_ENTRY_UPSERT:
    case GS1_ENGINE_MESSAGE_END_PROGRESSION_VIEW:
    case GS1_ENGINE_MESSAGE_CLOSE_PROGRESSION_VIEW:
        return true;
    default:
        return false;
    }
}
}

Gs1GodotAdapterService::~Gs1GodotAdapterService()
{
    runtime_session_.stop();
}

void Gs1GodotAdapterService::process_frame(double delta_seconds)
{
    if (!runtime_session_.is_running())
    {
        ensure_runtime_started();
    }
    if (!runtime_session_.is_running())
    {
        return;
    }

    Gs1Phase1Result phase1 {};
    Gs1Phase2Result phase2 {};
    if (!runtime_session_.update(delta_seconds > 0.0 ? delta_seconds : (1.0 / 60.0), phase1, phase2))
    {
        last_error_ = runtime_session_.last_error();
        runtime_session_.stop();
        notify_runtime_message_reset();
        return;
    }

    if (!drain_projection_messages())
    {
        runtime_session_.stop();
        notify_runtime_message_reset();
        return;
    }

    last_error_.clear();
}

void Gs1GodotAdapterService::subscribe(Gs1EngineMessageType type, IGs1GodotEngineMessageSubscriber& subscriber)
{
    remember_subscriber(subscriber);
    auto& bucket = subscribers_by_message_[bucket_index(type)];
    if (std::find(bucket.begin(), bucket.end(), &subscriber) == bucket.end())
    {
#ifndef NDEBUG
        if (is_exclusive_snapshot_message(type))
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
            if (is_exclusive_snapshot_message(type))
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
        runtime_session_.stop();
        notify_runtime_message_reset();
        return;
    }

    last_error_.clear();
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

        dispatch_engine_message(std::move(message));
    }

    return true;
}

void Gs1GodotAdapterService::notify_runtime_message_reset()
{
    for (IGs1GodotEngineMessageSubscriber* subscriber : known_subscribers_)
    {
        if (subscriber != nullptr)
        {
            subscriber->handle_runtime_message_reset();
        }
    }
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
