#include "runtime/game_runtime.h"
#include "runtime/runtime_clock.h"
#include "content/defs/faction_defs.h"
#include "content/defs/item_defs.h"
#include "content/defs/task_defs.h"
#include "content/defs/technology_defs.h"
#include "content/prototype_content.h"
#include "site/inventory_storage.h"
#include "site/site_projection_update_flags.h"
#include "site/task_board_state.h"
#include "site/site_world_access.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

namespace
{
using gs1::GameMessage;
using gs1::GameMessageType;
using gs1::GameRuntime;
using gs1::InventoryItemUseRequestedMessage;
using gs1::PhoneListingPurchaseRequestedMessage;
using gs1::SelectDeploymentSiteMessage;
using gs1::SiteId;
using gs1::StartNewCampaignMessage;
using gs1::StartSiteAttemptMessage;
using gs1::TaskAcceptRequestedMessage;
using gs1::TileCoord;

constexpr float k_float_epsilon = 0.001f;
constexpr double k_default_delivery_real_seconds =
    (30.0 / gs1::k_runtime_minutes_per_real_second) + (1.0 / 60.0);

bool approx_equal(float lhs, float rhs, float epsilon = k_float_epsilon)
{
    return std::fabs(lhs - rhs) <= epsilon;
}
}

namespace gs1
{
struct GameRuntimeProjectionTestAccess
{
    static GamePresentationRuntimeContext presentation_context(GameRuntime& runtime)
    {
        return GamePresentationRuntimeContext {
            runtime.state_.app_state,
            runtime.state_.campaign,
            runtime.state_.active_site_run,
            runtime.state_.site_protection_presentation,
            runtime.state_.message_queue,
            runtime.state_.runtime_messages,
            runtime.state_.fixed_step_seconds};
    }

    static std::optional<CampaignState>& campaign(GameRuntime& runtime)
    {
        return runtime.state_.campaign;
    }

    static std::optional<SiteRunState>& active_site_run(GameRuntime& runtime)
    {
        return runtime.state_.active_site_run;
    }

    static void mark_tile_dirty(GameRuntime& runtime, TileCoord coord)
    {
        auto context = presentation_context(runtime);
        runtime.presentation_.mark_site_tile_projection_dirty(context, coord);
    }

    static void mark_all_tiles_dirty(GameRuntime& runtime)
    {
        auto context = presentation_context(runtime);
        runtime.presentation_.mark_site_projection_update_dirty(context, 1ULL << 0);
    }

    static void flush_projection(GameRuntime& runtime)
    {
        auto context = presentation_context(runtime);
        runtime.presentation_.flush_site_presentation_if_dirty(context);
    }

    static void mark_projection_dirty(GameRuntime& runtime, std::uint64_t dirty_flags)
    {
        auto context = presentation_context(runtime);
        runtime.presentation_.mark_site_projection_update_dirty(context, dirty_flags);
    }

    static bool site_protection_selector_open(const GameRuntime& runtime)
    {
        return runtime.state_.site_protection_presentation.selector_open;
    }

    static Gs1SiteProtectionOverlayMode site_protection_overlay_mode(const GameRuntime& runtime)
    {
        return runtime.state_.site_protection_presentation.overlay_mode;
    }

};
}  // namespace gs1

namespace
{

GameMessage make_start_campaign_message()
{
    GameMessage message {};
    message.type = GameMessageType::StartNewCampaign;
    message.set_payload(StartNewCampaignMessage {42ULL, 30U});
    return message;
}

GameMessage make_start_site_attempt_message(std::uint32_t site_id)
{
    GameMessage message {};
    message.type = GameMessageType::StartSiteAttempt;
    message.set_payload(StartSiteAttemptMessage {site_id});
    return message;
}

void seed_runtime_test_task(GameRuntime& runtime, std::uint32_t site_completion_target)
{
    auto& site_run = gs1::GameRuntimeProjectionTestAccess::active_site_run(runtime).value();
    site_run.task_board.visible_tasks.clear();
    site_run.task_board.accepted_task_ids.clear();
    site_run.task_board.completed_task_ids.clear();
    site_run.task_board.claimed_task_ids.clear();
    site_run.task_board.task_pool_size = 1U;
    site_run.task_board.visible_tasks.push_back(gs1::TaskInstanceState {
        gs1::TaskInstanceId {1U},
        gs1::TaskTemplateId {gs1::k_task_template_site1_restore_patch},
        gs1::FactionId {gs1::k_faction_forestry_bureau},
        1U,
        site_completion_target,
        0U,
        0U});
    gs1::GameRuntimeProjectionTestAccess::mark_projection_dirty(
        runtime,
        gs1::SITE_PROJECTION_UPDATE_TASKS |
            gs1::SITE_PROJECTION_UPDATE_PHONE |
            gs1::SITE_PROJECTION_UPDATE_HUD);
    gs1::GameRuntimeProjectionTestAccess::flush_projection(runtime);
}

GameMessage make_select_site_message(std::uint32_t site_id)
{
    GameMessage message {};
    message.type = GameMessageType::SelectDeploymentSite;
    message.set_payload(SelectDeploymentSiteMessage {site_id});
    return message;
}

std::vector<Gs1RuntimeMessage> drain_runtime_messages(GameRuntime& runtime)
{
    std::vector<Gs1RuntimeMessage> messages {};
    Gs1RuntimeMessage message {};
    while (runtime.pop_runtime_message(message) == GS1_STATUS_OK)
    {
        messages.push_back(message);
    }
    return messages;
}

Gs1HostMessage make_site_action_request_event(
    Gs1SiteActionKind action_kind,
    TileCoord target_tile,
    std::uint32_t primary_subject_id,
    std::uint32_t item_id = 0U,
    std::uint8_t flags = 0U)
{
    Gs1HostMessage event {};
    event.type = GS1_HOST_EVENT_SITE_ACTION_REQUEST;
    event.payload.site_action_request.action_kind = action_kind;
    event.payload.site_action_request.flags = flags != 0U
        ? flags
        : (item_id != 0U
            ? GS1_SITE_ACTION_REQUEST_FLAG_HAS_ITEM
            : GS1_SITE_ACTION_REQUEST_FLAG_HAS_PRIMARY_SUBJECT);
    event.payload.site_action_request.quantity = 1U;
    event.payload.site_action_request.target_tile_x = target_tile.x;
    event.payload.site_action_request.target_tile_y = target_tile.y;
    event.payload.site_action_request.primary_subject_id = primary_subject_id;
    event.payload.site_action_request.item_id = item_id;
    return event;
}

Gs1HostMessage make_site_context_request_event(TileCoord target_tile)
{
    Gs1HostMessage event {};
    event.type = GS1_HOST_EVENT_SITE_CONTEXT_REQUEST;
    event.payload.site_context_request.tile_x = target_tile.x;
    event.payload.site_context_request.tile_y = target_tile.y;
    event.payload.site_context_request.flags = 0U;
    return event;
}

Gs1HostMessage make_site_action_cancel_event(std::uint32_t flags)
{
    Gs1HostMessage event {};
    event.type = GS1_HOST_EVENT_SITE_ACTION_CANCEL;
    event.payload.site_action_cancel.action_id = 0U;
    event.payload.site_action_cancel.flags = flags;
    return event;
}

Gs1HostMessage make_ui_action_event(const Gs1UiAction& action)
{
    Gs1HostMessage event {};
    event.type = GS1_HOST_EVENT_UI_ACTION;
    event.payload.ui_action.action = action;
    return event;
}

Gs1HostMessage make_site_scene_ready_event()
{
    Gs1HostMessage event {};
    event.type = GS1_HOST_EVENT_SITE_SCENE_READY;
    return event;
}

Gs1HostMessage make_storage_view_event(
    std::uint32_t storage_id,
    Gs1InventoryViewEventKind event_kind)
{
    Gs1HostMessage event {};
    event.type = GS1_HOST_EVENT_SITE_STORAGE_VIEW;
    event.payload.site_storage_view.storage_id = storage_id;
    event.payload.site_storage_view.event_kind = event_kind;
    return event;
}

Gs1HostMessage make_inventory_slot_tap_event(
    std::uint32_t storage_id,
    Gs1InventoryContainerKind container_kind,
    std::uint16_t slot_index,
    std::uint32_t item_instance_id = 0U)
{
    Gs1HostMessage event {};
    event.type = GS1_HOST_EVENT_SITE_INVENTORY_SLOT_TAP;
    event.payload.site_inventory_slot_tap.storage_id = storage_id;
    event.payload.site_inventory_slot_tap.container_kind = container_kind;
    event.payload.site_inventory_slot_tap.slot_index = slot_index;
    event.payload.site_inventory_slot_tap.item_instance_id = item_instance_id;
    return event;
}

Gs1HostMessage make_move_direction_event(float world_move_x, float world_move_y, float world_move_z)
{
    Gs1HostMessage event {};
    event.type = GS1_HOST_EVENT_SITE_MOVE_DIRECTION;
    event.payload.site_move_direction.world_move_x = world_move_x;
    event.payload.site_move_direction.world_move_y = world_move_y;
    event.payload.site_move_direction.world_move_z = world_move_z;
    return event;
}

void run_phase1(GameRuntime& runtime, double real_delta_seconds, Gs1Phase1Result& out_result)
{
    Gs1Phase1Request request {};
    request.struct_size = sizeof(Gs1Phase1Request);
    request.real_delta_seconds = real_delta_seconds;

    out_result = {};
    assert(runtime.run_phase1(request, out_result) == GS1_STATUS_OK);
}

void run_phase1(GameRuntime& runtime, double real_delta_seconds)
{
    Gs1Phase1Result result {};
    run_phase1(runtime, real_delta_seconds, result);
}

void run_phase2(GameRuntime& runtime, Gs1Phase2Result& out_result)
{
    Gs1Phase2Request request {};
    request.struct_size = sizeof(Gs1Phase2Request);

    out_result = {};
    assert(runtime.run_phase2(request, out_result) == GS1_STATUS_OK);
}

void complete_site_loading(GameRuntime& runtime)
{
    const auto ready_event = make_site_scene_ready_event();
    assert(runtime.submit_host_messages(&ready_event, 1U) == GS1_STATUS_OK);
    Gs1Phase2Result ready_result {};
    run_phase2(runtime, ready_result);
}

float raw_meter_test_value(float value)
{
    return (value > 0.0f && value <= 1.0f) ? value * 100.0f : value;
}

void set_tile_sand_burial(gs1::SiteRunState& site_run, TileCoord coord, float sand_burial)
{
    auto ecology = gs1::site_world_access::tile_ecology(site_run, coord);
    ecology.sand_burial = raw_meter_test_value(sand_burial);
    gs1::site_world_access::set_tile_ecology(site_run, coord, ecology);
}

void set_tile_plant_state(
    gs1::SiteRunState& site_run,
    TileCoord coord,
    gs1::PlantId plant_id,
    float plant_density)
{
    auto ecology = gs1::site_world_access::tile_ecology(site_run, coord);
    ecology.plant_id = plant_id;
    ecology.ground_cover_type_id = 0U;
    ecology.plant_density = raw_meter_test_value(plant_density);
    gs1::site_world_access::set_tile_ecology(site_run, coord, ecology);
}

void set_tile_local_wind(gs1::SiteRunState& site_run, TileCoord coord, float local_wind)
{
    auto local_weather = gs1::site_world_access::tile_local_weather(site_run, coord);
    local_weather.wind = local_wind;
    gs1::site_world_access::set_tile_local_weather(site_run, coord, local_weather);
}

void set_tile_soil_visual_state(
    gs1::SiteRunState& site_run,
    TileCoord coord,
    float moisture,
    float soil_fertility,
    float soil_salinity)
{
    auto ecology = gs1::site_world_access::tile_ecology(site_run, coord);
    ecology.moisture = raw_meter_test_value(moisture);
    ecology.soil_fertility = raw_meter_test_value(soil_fertility);
    ecology.soil_salinity = raw_meter_test_value(soil_salinity);
    gs1::site_world_access::set_tile_ecology(site_run, coord, ecology);
}

void set_tile_excavation_state(
    gs1::SiteRunState& site_run,
    TileCoord coord,
    gs1::ExcavationDepth depth)
{
    auto excavation = gs1::site_world_access::tile_excavation(site_run, coord);
    excavation.depth = depth;
    gs1::site_world_access::set_tile_excavation(site_run, coord, excavation);
}

void set_tile_protection_visual_state(
    gs1::SiteRunState& site_run,
    TileCoord coord,
    float wind_protection,
    float heat_protection,
    float dust_protection)
{
    auto contribution = site_run.site_world->tile_plant_weather_contribution(coord);
    contribution.wind_protection = raw_meter_test_value(wind_protection);
    contribution.heat_protection = raw_meter_test_value(heat_protection);
    contribution.dust_protection = raw_meter_test_value(dust_protection);
    contribution.fertility_improve = 0.0f;
    contribution.salinity_reduction = 0.0f;
    contribution.irrigation = 0.0f;
    site_run.site_world->set_tile_plant_weather_contribution(coord, contribution);

    auto device_contribution = site_run.site_world->tile_device_weather_contribution(coord);
    device_contribution.wind_protection = 0.0f;
    device_contribution.heat_protection = 0.0f;
    device_contribution.dust_protection = 0.0f;
    device_contribution.fertility_improve = 0.0f;
    device_contribution.salinity_reduction = 0.0f;
    device_contribution.irrigation = 0.0f;
    site_run.site_world->set_tile_device_weather_contribution(coord, device_contribution);
}

void set_tile_device_state(
    gs1::SiteRunState& site_run,
    TileCoord coord,
    gs1::StructureId structure_id,
    float device_integrity)
{
    auto device = gs1::site_world_access::tile_device(site_run, coord);
    device.structure_id = structure_id;
    device.device_integrity = std::clamp(device_integrity, 0.0f, 1.0f);
    device.device_efficiency = device.device_integrity;
    device.fixed_integrity = false;
    gs1::site_world_access::set_tile_device(site_run, coord, device);
}

std::vector<Gs1RuntimeMessage> flush_tile_delta_for(
    GameRuntime& runtime,
    TileCoord coord,
    float sand_burial)
{
    auto& site_run = gs1::GameRuntimeProjectionTestAccess::active_site_run(runtime).value();
    set_tile_sand_burial(site_run, coord, sand_burial);
    gs1::GameRuntimeProjectionTestAccess::mark_tile_dirty(runtime, coord);
    gs1::GameRuntimeProjectionTestAccess::flush_projection(runtime);
    return drain_runtime_messages(runtime);
}

std::vector<const Gs1RuntimeMessage*> collect_messages_of_type(
    const std::vector<Gs1RuntimeMessage>& messages,
    Gs1EngineMessageType type)
{
    std::vector<const Gs1RuntimeMessage*> matches {};
    for (const auto& message : messages)
    {
        if (message.type == type)
        {
            matches.push_back(&message);
        }
    }
    return matches;
}

std::vector<const Gs1RuntimeMessage*> collect_inventory_slot_messages(
    const std::vector<Gs1RuntimeMessage>& messages)
{
    return collect_messages_of_type(messages, GS1_ENGINE_MESSAGE_SITE_INVENTORY_SLOT_UPSERT);
}

const Gs1RuntimeMessage* find_plant_visual_message(
    const std::vector<Gs1RuntimeMessage>& messages,
    std::uint32_t plant_type_id,
    TileCoord anchor)
{
    for (const auto& message : messages)
    {
        if (message.type != GS1_ENGINE_MESSAGE_SITE_PLANT_VISUAL_UPSERT)
        {
            continue;
        }

        const auto& payload = message.payload_as<Gs1EngineMessageSitePlantVisualData>();
        if (payload.plant_type_id == plant_type_id &&
            approx_equal(payload.anchor_tile_x, static_cast<float>(anchor.x)) &&
            approx_equal(payload.anchor_tile_y, static_cast<float>(anchor.y)))
        {
            return &message;
        }
    }

    return nullptr;
}

const Gs1RuntimeMessage* find_device_visual_message(
    const std::vector<Gs1RuntimeMessage>& messages,
    std::uint32_t structure_type_id,
    TileCoord anchor)
{
    for (const auto& message : messages)
    {
        if (message.type != GS1_ENGINE_MESSAGE_SITE_DEVICE_VISUAL_UPSERT)
        {
            continue;
        }

        const auto& payload = message.payload_as<Gs1EngineMessageSiteDeviceVisualData>();
        if (payload.structure_type_id == structure_type_id &&
            approx_equal(payload.anchor_tile_x, static_cast<float>(anchor.x)) &&
            approx_equal(payload.anchor_tile_y, static_cast<float>(anchor.y)))
        {
            return &message;
        }
    }

    return nullptr;
}

const Gs1RuntimeMessage* find_inventory_storage_message(
    const std::vector<Gs1RuntimeMessage>& messages,
    Gs1InventoryContainerKind container_kind,
    TileCoord tile)
{
    for (const auto& message : messages)
    {
        if (message.type != GS1_ENGINE_MESSAGE_SITE_INVENTORY_STORAGE_UPSERT)
        {
            continue;
        }

        const auto& payload = message.payload_as<Gs1EngineMessageInventoryStorageData>();
        if (payload.container_kind == container_kind &&
            payload.tile_x == tile.x &&
            payload.tile_y == tile.y)
        {
            return &message;
        }
    }

    return nullptr;
}

const Gs1RuntimeMessage* find_inventory_slot_message(
    const std::vector<Gs1RuntimeMessage>& messages,
    Gs1InventoryContainerKind container_kind,
    std::uint32_t storage_id,
    std::uint16_t slot_index)
{
    for (const auto& message : messages)
    {
        if (message.type != GS1_ENGINE_MESSAGE_SITE_INVENTORY_SLOT_UPSERT)
        {
            continue;
        }

        const auto& payload = message.payload_as<Gs1EngineMessageInventorySlotData>();
        if (payload.container_kind == container_kind &&
            payload.storage_id == storage_id &&
            payload.slot_index == slot_index)
        {
            return &message;
        }
    }

    return nullptr;
}

const Gs1RuntimeMessage* find_task_message(
    const std::vector<Gs1RuntimeMessage>& messages,
    std::uint32_t task_instance_id)
{
    for (const auto& message : messages)
    {
        if (message.type != GS1_ENGINE_MESSAGE_SITE_TASK_UPSERT)
        {
            continue;
        }

        const auto& payload = message.payload_as<Gs1EngineMessageTaskData>();
        if (payload.task_instance_id == task_instance_id)
        {
            return &message;
        }
    }

    return nullptr;
}

const Gs1RuntimeMessage* find_inventory_view_message(
    const std::vector<Gs1RuntimeMessage>& messages,
    std::uint32_t storage_id,
    Gs1InventoryViewEventKind event_kind)
{
    for (const auto& message : messages)
    {
        if (message.type != GS1_ENGINE_MESSAGE_SITE_INVENTORY_VIEW_STATE)
        {
            continue;
        }

        const auto& payload = message.payload_as<Gs1EngineMessageInventoryViewData>();
        if (payload.storage_id == storage_id && payload.event_kind == event_kind)
        {
            return &message;
        }
    }

    return nullptr;
}

const Gs1RuntimeMessage* find_close_ui_setup_message(
    const std::vector<Gs1RuntimeMessage>& messages,
    Gs1UiSetupId setup_id)
{
    for (const auto& message : messages)
    {
        if (message.type != GS1_ENGINE_MESSAGE_CLOSE_UI_SETUP)
        {
            continue;
        }

        const auto& payload = message.payload_as<Gs1EngineMessageCloseUiSetupData>();
        if (payload.setup_id == setup_id)
        {
            return &message;
        }
    }

    return nullptr;
}

const Gs1RuntimeMessage* find_ui_surface_visibility_message(
    const std::vector<Gs1RuntimeMessage>& messages,
    Gs1UiSurfaceId surface_id,
    bool visible)
{
    for (const auto& message : messages)
    {
        if (message.type != GS1_ENGINE_MESSAGE_SET_UI_SURFACE_VISIBILITY)
        {
            continue;
        }

        const auto& payload = message.payload_as<Gs1EngineMessageUiSurfaceVisibilityData>();
        if (payload.surface_id == surface_id && payload.visible == (visible ? 1U : 0U))
        {
            return &message;
        }
    }

    return nullptr;
}

bool contains_phone_listing_message(
    const std::vector<Gs1RuntimeMessage>& messages,
    Gs1EngineMessageType message_type,
    std::uint32_t listing_id)
{
    return std::any_of(messages.begin(), messages.end(), [&](const Gs1RuntimeMessage& message) {
        return message.type == message_type &&
            message.payload_as<Gs1EngineMessagePhoneListingData>().listing_id == listing_id;
    });
}

std::uint32_t starter_storage_id(gs1::SiteRunState& site_run)
{
    return gs1::inventory_storage::storage_id_for_container(
        site_run,
        gs1::inventory_storage::starter_storage_container(site_run));
}

std::uint32_t delivery_box_owner_id(const gs1::SiteRunState& site_run)
{
    return static_cast<std::uint32_t>(site_run.site_world->device_entity_id(site_run.camp.delivery_box_tile));
}

std::uint32_t delivery_box_id(gs1::SiteRunState& site_run)
{
    return gs1::inventory_storage::storage_id_for_container(
        site_run,
        gs1::inventory_storage::delivery_box_container(site_run));
}

TileCoord starter_workbench_tile(const gs1::SiteRunState& site_run)
{
    return TileCoord {site_run.camp.camp_anchor_tile.x + 1, site_run.camp.camp_anchor_tile.y};
}

std::uint32_t starter_workbench_id(gs1::SiteRunState& site_run)
{
    const auto tile = starter_workbench_tile(site_run);
    const auto device_entity_id = site_run.site_world->device_entity_id(tile);
    return gs1::inventory_storage::storage_id_for_container(
        site_run,
        gs1::inventory_storage::find_device_storage_container(site_run, device_entity_id));
}

std::uint16_t find_delivery_box_slot_index(
    gs1::SiteRunState& site_run,
    gs1::ItemId item_id,
    std::uint32_t quantity)
{
    const auto container = gs1::inventory_storage::delivery_box_container(site_run);
    const auto slot_count = gs1::inventory_storage::slot_count_in_container(site_run, container);
    for (std::uint32_t slot_index = 0U; slot_index < slot_count; ++slot_index)
    {
        const auto item = gs1::inventory_storage::item_entity_for_slot(site_run, container, slot_index);
        const auto* stack = gs1::inventory_storage::stack_data(site_run, item);
        if (stack != nullptr && stack->item_id == item_id && stack->quantity == quantity)
        {
            return static_cast<std::uint16_t>(slot_index);
        }
    }

    return std::numeric_limits<std::uint16_t>::max();
}

const Gs1EngineMessageUiElementData* find_ui_element_by_content(
    const std::vector<Gs1RuntimeMessage>& messages,
    std::uint8_t expected_content_kind,
    std::uint32_t expected_primary_id = 0U,
    std::uint32_t expected_secondary_id = 0U,
    std::uint32_t expected_quantity = 0U)
{
    for (const auto& message : messages)
    {
        if (message.type != GS1_ENGINE_MESSAGE_UI_ELEMENT_UPSERT)
        {
            continue;
        }

        const auto& payload = message.payload_as<Gs1EngineMessageUiElementData>();
        if (payload.content_kind == expected_content_kind &&
            payload.primary_id == expected_primary_id &&
            payload.secondary_id == expected_secondary_id &&
            payload.quantity == expected_quantity)
        {
            return &payload;
        }
    }

    return nullptr;
}

bool contains_progression_view_begin(
    const std::vector<Gs1RuntimeMessage>& messages,
    Gs1ProgressionViewId view_id)
{
    for (const auto& message : messages)
    {
        if (message.type != GS1_ENGINE_MESSAGE_BEGIN_PROGRESSION_VIEW)
        {
            continue;
        }

        const auto& payload = message.payload_as<Gs1EngineMessageProgressionViewData>();
        if (payload.view_id == view_id)
        {
            return true;
        }
    }

    return false;
}

const Gs1EngineMessageProgressionEntryData* find_progression_entry(
    const std::vector<Gs1RuntimeMessage>& messages,
    Gs1ProgressionEntryKind entry_kind,
    std::uint16_t reputation_requirement,
    std::uint8_t faction_id = 0U)
{
    for (const auto& message : messages)
    {
        if (message.type != GS1_ENGINE_MESSAGE_PROGRESSION_ENTRY_UPSERT)
        {
            continue;
        }

        const auto& payload = message.payload_as<Gs1EngineMessageProgressionEntryData>();
        if (payload.entry_kind == entry_kind &&
            payload.reputation_requirement == reputation_requirement &&
            payload.faction_id == faction_id)
        {
            return &payload;
        }
    }

    return nullptr;
}

const Gs1EngineMessageUiPanelTextData* find_ui_panel_text_message(
    const std::vector<Gs1RuntimeMessage>& messages,
    std::uint8_t text_kind,
    std::uint32_t primary_id = 0U,
    std::uint32_t secondary_id = 0U,
    std::uint32_t quantity = 0U)
{
    for (const auto& message : messages)
    {
        if (message.type != GS1_ENGINE_MESSAGE_UI_PANEL_TEXT_UPSERT &&
            message.type != GS1_ENGINE_MESSAGE_REGIONAL_SELECTION_UI_PANEL_TEXT_UPSERT)
        {
            continue;
        }

        const auto& payload = message.payload_as<Gs1EngineMessageUiPanelTextData>();
        if (payload.text_kind == text_kind &&
            payload.primary_id == primary_id &&
            payload.secondary_id == secondary_id &&
            payload.quantity == quantity)
        {
            return &payload;
        }
    }

    return nullptr;
}

const Gs1EngineMessageUiPanelSlotActionData* find_ui_panel_slot_action_message(
    const std::vector<Gs1RuntimeMessage>& messages,
    Gs1UiPanelSlotId slot_id,
    std::uint8_t label_kind)
{
    for (const auto& message : messages)
    {
        if (message.type != GS1_ENGINE_MESSAGE_UI_PANEL_SLOT_ACTION_UPSERT &&
            message.type != GS1_ENGINE_MESSAGE_REGIONAL_SELECTION_UI_PANEL_SLOT_ACTION_UPSERT)
        {
            continue;
        }

        const auto& payload = message.payload_as<Gs1EngineMessageUiPanelSlotActionData>();
        if (payload.slot_id == slot_id && payload.label_kind == label_kind)
        {
            return &payload;
        }
    }

    return nullptr;
}

const Gs1EngineMessageUiPanelListItemData* find_ui_panel_list_item_message(
    const std::vector<Gs1RuntimeMessage>& messages,
    Gs1UiPanelListId list_id,
    std::uint8_t primary_kind,
    std::uint32_t primary_id,
    std::uint32_t secondary_id = 0U,
    std::uint32_t quantity = 0U)
{
    for (const auto& message : messages)
    {
        if (message.type != GS1_ENGINE_MESSAGE_UI_PANEL_LIST_ITEM_UPSERT &&
            message.type != GS1_ENGINE_MESSAGE_REGIONAL_SELECTION_UI_PANEL_LIST_ITEM_UPSERT)
        {
            continue;
        }

        const auto& payload = message.payload_as<Gs1EngineMessageUiPanelListItemData>();
        if (payload.list_id == list_id &&
            payload.primary_kind == primary_kind &&
            payload.primary_id == primary_id &&
            payload.secondary_id == secondary_id &&
            payload.quantity == quantity)
        {
            return &payload;
        }
    }

    return nullptr;
}

const Gs1RuntimeMessage* find_regional_map_site_message(
    const std::vector<Gs1RuntimeMessage>& messages,
    std::uint32_t site_id)
{
    for (const auto& message : messages)
    {
        if (message.type != GS1_ENGINE_MESSAGE_REGIONAL_MAP_SITE_UPSERT)
        {
            continue;
        }

        const auto& payload = message.payload_as<Gs1EngineMessageRegionalMapSiteData>();
        if (payload.site_id == site_id)
        {
            return &message;
        }
    }

    return nullptr;
}

bool contains_regional_map_link(
    const std::vector<Gs1RuntimeMessage>& messages,
    std::uint32_t from_site_id,
    std::uint32_t to_site_id)
{
    return std::any_of(messages.begin(), messages.end(), [&](const Gs1RuntimeMessage& message) {
        if (message.type != GS1_ENGINE_MESSAGE_REGIONAL_MAP_LINK_UPSERT)
        {
            return false;
        }

        const auto& payload = message.payload_as<Gs1EngineMessageRegionalMapLinkData>();
        return payload.from_site_id == from_site_id && payload.to_site_id == to_site_id;
    });
}

}  // namespace

int main()
{
    Gs1RuntimeCreateDesc create_desc {};
    create_desc.struct_size = sizeof(Gs1RuntimeCreateDesc);
    create_desc.api_version = gs1::k_api_version;
    create_desc.fixed_step_seconds = 1.0 / 60.0;
    create_desc.adapter_config_json_utf8 = nullptr;

    GameRuntime runtime {create_desc};

    assert(runtime.handle_message(make_start_campaign_message()) == GS1_STATUS_OK);
    assert(gs1::GameRuntimeProjectionTestAccess::campaign(runtime).has_value());
    const auto initial_map_messages = drain_runtime_messages(runtime);
    {
        const auto* site_one_message = find_regional_map_site_message(initial_map_messages, 1U);
        const auto* site_two_message = find_regional_map_site_message(initial_map_messages, 2U);
        const auto* site_three_message = find_regional_map_site_message(initial_map_messages, 3U);
        const auto* site_four_message = find_regional_map_site_message(initial_map_messages, 4U);
        assert(site_one_message != nullptr);
        assert(site_two_message != nullptr);
        assert(site_three_message != nullptr);
        assert(site_four_message != nullptr);
        assert(site_one_message->payload_as<Gs1EngineMessageRegionalMapSiteData>().map_x == 0);
        assert(site_one_message->payload_as<Gs1EngineMessageRegionalMapSiteData>().map_y == 0);
        assert(site_one_message->payload_as<Gs1EngineMessageRegionalMapSiteData>().site_state == GS1_SITE_STATE_AVAILABLE);
        assert(site_two_message->payload_as<Gs1EngineMessageRegionalMapSiteData>().site_state == GS1_SITE_STATE_LOCKED);
        assert(site_three_message->payload_as<Gs1EngineMessageRegionalMapSiteData>().site_state == GS1_SITE_STATE_LOCKED);
        assert(site_four_message->payload_as<Gs1EngineMessageRegionalMapSiteData>().site_state == GS1_SITE_STATE_LOCKED);
        assert(contains_regional_map_link(initial_map_messages, 1U, 2U));
        assert(contains_regional_map_link(initial_map_messages, 2U, 3U));
        assert(contains_regional_map_link(initial_map_messages, 3U, 4U));
    }

    const auto campaign_site_id = gs1::GameRuntimeProjectionTestAccess::campaign(runtime)->sites.front().site_id.value;
    assert(runtime.handle_message(make_select_site_message(campaign_site_id)) == GS1_STATUS_OK);
    const auto loadout_ui_messages = drain_runtime_messages(runtime);
    assert(find_ui_panel_list_item_message(loadout_ui_messages, GS1_UI_PANEL_LIST_REGIONAL_LOADOUT, 6U, 1U, 0U, 1U) != nullptr);
    assert(find_ui_panel_list_item_message(loadout_ui_messages, GS1_UI_PANEL_LIST_REGIONAL_LOADOUT, 6U, gs1::k_item_basic_straw_checkerboard, 0U, 8U) != nullptr);
    assert(find_ui_panel_slot_action_message(loadout_ui_messages, GS1_UI_PANEL_SLOT_PRIMARY, 1U) != nullptr);

    assert(runtime.handle_message(make_start_site_attempt_message(campaign_site_id)) == GS1_STATUS_OK);
    complete_site_loading(runtime);
    drain_runtime_messages(runtime);

    auto& completed_site_run = gs1::GameRuntimeProjectionTestAccess::active_site_run(runtime).value();
    completed_site_run.run_status = gs1::SiteRunStatus::Completed;
    completed_site_run.result_newly_revealed_site_count = 0U;

    GameMessage site_completed_message {};
    site_completed_message.type = GameMessageType::SiteAttemptEnded;
    site_completed_message.set_payload(gs1::SiteAttemptEndedMessage {
        campaign_site_id,
        GS1_SITE_ATTEMPT_RESULT_COMPLETED});
    assert(runtime.handle_message(site_completed_message) == GS1_STATUS_OK);
    drain_runtime_messages(runtime);

    GameMessage return_to_map_message {};
    return_to_map_message.type = GameMessageType::ReturnToRegionalMap;
    return_to_map_message.set_payload(gs1::ReturnToRegionalMapMessage {});
    assert(runtime.handle_message(return_to_map_message) == GS1_STATUS_OK);
    const auto reopened_map_messages = drain_runtime_messages(runtime);
    {
        const auto* site_one_message = find_regional_map_site_message(reopened_map_messages, 1U);
        const auto* site_two_message = find_regional_map_site_message(reopened_map_messages, 2U);
        const auto* site_three_message = find_regional_map_site_message(reopened_map_messages, 3U);
        const auto* site_four_message = find_regional_map_site_message(reopened_map_messages, 4U);
        assert(site_one_message != nullptr);
        assert(site_two_message != nullptr);
        assert(site_three_message != nullptr);
        assert(site_four_message != nullptr);
        assert(site_two_message->payload_as<Gs1EngineMessageRegionalMapSiteData>().map_x == 160);
        assert(site_two_message->payload_as<Gs1EngineMessageRegionalMapSiteData>().map_y == 0);
        assert(site_two_message->payload_as<Gs1EngineMessageRegionalMapSiteData>().site_state == GS1_SITE_STATE_AVAILABLE);
        assert(contains_regional_map_link(reopened_map_messages, 1U, 2U));
        assert(contains_regional_map_link(reopened_map_messages, 2U, 3U));
        assert(contains_regional_map_link(reopened_map_messages, 3U, 4U));
    }

    GameMessage open_tech_tree_message {};
    open_tech_tree_message.type = GameMessageType::OpenRegionalMapTechTree;
    open_tech_tree_message.set_payload(gs1::OpenRegionalMapTechTreeMessage {});
    assert(runtime.handle_message(open_tech_tree_message) == GS1_STATUS_OK);
    const auto tech_tree_messages = drain_runtime_messages(runtime);
    assert(contains_progression_view_begin(
        tech_tree_messages,
        GS1_PROGRESSION_VIEW_REGIONAL_MAP_TECH_TREE));
    {
        const auto* unlock_entry = find_progression_entry(
            tech_tree_messages,
            GS1_PROGRESSION_ENTRY_REPUTATION_UNLOCK,
            100U);
        assert(unlock_entry != nullptr);
        assert(unlock_entry->entry_id == 5001U);
        assert(unlock_entry->content_id == 21U);
    }
    {
        const auto* village_node = find_progression_entry(
            tech_tree_messages,
            GS1_PROGRESSION_ENTRY_TECHNOLOGY_NODE,
            200U,
            static_cast<std::uint8_t>(gs1::k_faction_village_committee));
        assert(village_node != nullptr);
        assert(village_node->action.arg0 == gs1::k_faction_village_committee);
        assert(village_node->tech_node_id ==
            gs1::base_technology_node_id(gs1::FactionId {gs1::k_faction_village_committee}, 1U));
    }

    GameMessage select_forestry_tab_message {};
    select_forestry_tab_message.type = GameMessageType::SelectRegionalMapTechTreeFaction;
    select_forestry_tab_message.set_payload(gs1::SelectRegionalMapTechTreeFactionMessage {
        gs1::k_faction_forestry_bureau});
    assert(runtime.handle_message(select_forestry_tab_message) == GS1_STATUS_OK);
    const auto forestry_messages = drain_runtime_messages(runtime);
    assert(contains_progression_view_begin(
        forestry_messages,
        GS1_PROGRESSION_VIEW_REGIONAL_MAP_TECH_TREE));

    GameMessage select_university_tab_message {};
    select_university_tab_message.type = GameMessageType::SelectRegionalMapTechTreeFaction;
    select_university_tab_message.set_payload(gs1::SelectRegionalMapTechTreeFactionMessage {
        gs1::k_faction_agricultural_university});
    assert(runtime.handle_message(select_university_tab_message) == GS1_STATUS_OK);
    const auto university_messages = drain_runtime_messages(runtime);
    assert(contains_progression_view_begin(
        university_messages,
        GS1_PROGRESSION_VIEW_REGIONAL_MAP_TECH_TREE));

    GameMessage faction_reputation_award {};
    faction_reputation_award.type = GameMessageType::FactionReputationAwardRequested;
    faction_reputation_award.set_payload(gs1::FactionReputationAwardRequestedMessage {
        gs1::k_faction_village_committee,
        40});
    assert(runtime.handle_message(faction_reputation_award) == GS1_STATUS_OK);

    GameMessage total_reputation_award {};
    total_reputation_award.type = GameMessageType::CampaignReputationAwardRequested;
    total_reputation_award.set_payload(gs1::CampaignReputationAwardRequestedMessage {10});
    assert(runtime.handle_message(total_reputation_award) == GS1_STATUS_OK);

    (void)drain_runtime_messages(runtime);

    GameMessage select_village_tab_message {};
    select_village_tab_message.type = GameMessageType::SelectRegionalMapTechTreeFaction;
    select_village_tab_message.set_payload(gs1::SelectRegionalMapTechTreeFactionMessage {
        gs1::k_faction_village_committee});
    assert(runtime.handle_message(select_village_tab_message) == GS1_STATUS_OK);
    const auto awarded_messages = drain_runtime_messages(runtime);
    assert(contains_progression_view_begin(
        awarded_messages,
        GS1_PROGRESSION_VIEW_REGIONAL_MAP_TECH_TREE));

    GameMessage claim_tech_node_message {};
    claim_tech_node_message.type = GameMessageType::TechnologyNodeClaimRequested;
    claim_tech_node_message.set_payload(gs1::TechnologyNodeClaimRequestedMessage {
        gs1::base_technology_node_id(gs1::FactionId {gs1::k_faction_village_committee}, 1U),
        gs1::k_faction_village_committee});
    assert(runtime.handle_message(claim_tech_node_message) == GS1_STATUS_OK);
    const auto claimed_messages = drain_runtime_messages(runtime);
    assert(contains_progression_view_begin(
        claimed_messages,
        GS1_PROGRESSION_VIEW_REGIONAL_MAP_TECH_TREE));
    {
        const auto* claimed_node = find_progression_entry(
            claimed_messages,
            GS1_PROGRESSION_ENTRY_TECHNOLOGY_NODE,
            200U,
            static_cast<std::uint8_t>(gs1::k_faction_village_committee));
        assert(claimed_node != nullptr);
        assert((claimed_node->flags & GS1_PROGRESSION_ENTRY_FLAG_UNLOCKED) == 0U);
    }

    GameRuntime support_runtime {create_desc};
    assert(support_runtime.handle_message(make_start_campaign_message()) == GS1_STATUS_OK);
    assert(gs1::GameRuntimeProjectionTestAccess::campaign(support_runtime).has_value());
    auto& support_campaign = gs1::GameRuntimeProjectionTestAccess::campaign(support_runtime).value();
    support_campaign.sites[0].site_state = GS1_SITE_STATE_COMPLETED;
    support_campaign.sites[1].site_state = GS1_SITE_STATE_AVAILABLE;
    support_campaign.regional_map_state.revealed_site_ids = {gs1::SiteId {1U}, gs1::SiteId {2U}};
    support_campaign.regional_map_state.available_site_ids = {gs1::SiteId {2U}};
    support_campaign.regional_map_state.completed_site_ids = {gs1::SiteId {1U}};
    drain_runtime_messages(support_runtime);
    assert(support_runtime.handle_message(make_select_site_message(2U)) == GS1_STATUS_OK);
    const auto support_loadout_messages = drain_runtime_messages(support_runtime);
    assert(find_ui_panel_list_item_message(support_loadout_messages, GS1_UI_PANEL_LIST_REGIONAL_LOADOUT, 6U, gs1::k_item_white_thorn_seed_bundle, 0U, 4U) != nullptr);
    assert(find_ui_panel_list_item_message(support_loadout_messages, GS1_UI_PANEL_LIST_REGIONAL_LOADOUT, 6U, gs1::k_item_wood_bundle, 0U, 2U) != nullptr);
    assert(find_ui_panel_text_message(support_loadout_messages, 4U, 0U, 0U, 1U) != nullptr);
    assert(find_ui_panel_text_message(support_loadout_messages, 5U, 0U, 0U, 1U) != nullptr);
    {
        const auto* contributor_message = find_regional_map_site_message(support_loadout_messages, 1U);
        assert(contributor_message != nullptr);
        const auto& payload = contributor_message->payload_as<Gs1EngineMessageRegionalMapSiteData>();
        assert(payload.support_package_id == 1001U);
        assert(payload.support_preview_mask != 0U);
    }
    assert(support_runtime.handle_message(make_start_site_attempt_message(2U)) == GS1_STATUS_OK);
    complete_site_loading(support_runtime);
    assert(gs1::GameRuntimeProjectionTestAccess::active_site_run(support_runtime).has_value());
    auto& supported_site_run = gs1::GameRuntimeProjectionTestAccess::active_site_run(support_runtime).value();
    assert(supported_site_run.modifier.active_nearby_aura_modifier_ids.size() == 1U);
    assert(gs1::inventory_storage::available_item_quantity_in_container(
               supported_site_run,
               gs1::inventory_storage::starter_storage_container(supported_site_run),
               gs1::ItemId {gs1::k_item_white_thorn_seed_bundle}) == 4U);
    assert(gs1::inventory_storage::available_item_quantity_in_container(
               supported_site_run,
               gs1::inventory_storage::starter_storage_container(supported_site_run),
               gs1::ItemId {gs1::k_item_wood_bundle}) == 2U);
    drain_runtime_messages(support_runtime);

    assert(support_runtime.handle_message(open_tech_tree_message) == GS1_STATUS_OK);
    const auto site_tech_tree_messages = drain_runtime_messages(support_runtime);
    assert(contains_progression_view_begin(
        site_tech_tree_messages,
        GS1_PROGRESSION_VIEW_REGIONAL_MAP_TECH_TREE));

    assert(support_runtime.handle_message(select_forestry_tab_message) == GS1_STATUS_OK);
    const auto site_forestry_messages = drain_runtime_messages(support_runtime);
    assert(contains_progression_view_begin(
        site_forestry_messages,
        GS1_PROGRESSION_VIEW_REGIONAL_MAP_TECH_TREE));

    GameRuntime bootstrap_runtime {create_desc};
    assert(bootstrap_runtime.handle_message(make_start_campaign_message()) == GS1_STATUS_OK);
    drain_runtime_messages(bootstrap_runtime);

    const auto first_site_id = gs1::GameRuntimeProjectionTestAccess::campaign(bootstrap_runtime)->sites.front().site_id.value;
    const auto* first_site_content = gs1::find_prototype_site_content(gs1::SiteId {first_site_id});
    assert(first_site_content != nullptr);
    assert(bootstrap_runtime.handle_message(make_select_site_message(first_site_id)) == GS1_STATUS_OK);
    drain_runtime_messages(bootstrap_runtime);
    assert(bootstrap_runtime.handle_message(make_start_site_attempt_message(first_site_id)) == GS1_STATUS_OK);
    complete_site_loading(bootstrap_runtime);
    assert(gs1::GameRuntimeProjectionTestAccess::active_site_run(bootstrap_runtime).has_value());

    auto& bootstrap_site_run = gs1::GameRuntimeProjectionTestAccess::active_site_run(bootstrap_runtime).value();
    assert(gs1::site_world_access::width(bootstrap_site_run) == 32U);
    assert(gs1::site_world_access::height(bootstrap_site_run) == 32U);
    assert(bootstrap_site_run.inventory.worker_pack_slots.size() == 8U);
    assert(!bootstrap_site_run.inventory.worker_pack_slots[0].occupied);
    assert(gs1::inventory_storage::available_item_quantity_in_container(
               bootstrap_site_run,
               gs1::inventory_storage::starter_storage_container(bootstrap_site_run),
               gs1::ItemId {gs1::k_item_water_container}) == 1U);
    assert(gs1::inventory_storage::available_item_quantity_in_container(
               bootstrap_site_run,
               gs1::inventory_storage::starter_storage_container(bootstrap_site_run),
               gs1::ItemId {gs1::k_item_basic_straw_checkerboard}) == 8U);
    assert(bootstrap_site_run.task_board.visible_tasks.size() == 1U);
    assert(bootstrap_site_run.task_board.accepted_task_ids.size() == 1U);
    assert(bootstrap_site_run.economy.current_cash == 2000);
    assert(bootstrap_site_run.economy.available_phone_listings.size() >= 5U);

    const auto bootstrap_messages = drain_runtime_messages(bootstrap_runtime);
    const auto storage_messages =
        collect_messages_of_type(bootstrap_messages, GS1_ENGINE_MESSAGE_SITE_INVENTORY_STORAGE_UPSERT);
    const auto weather_messages =
        collect_messages_of_type(bootstrap_messages, GS1_ENGINE_MESSAGE_SITE_WEATHER_UPDATE);
    assert(!collect_messages_of_type(bootstrap_messages, GS1_ENGINE_MESSAGE_SITE_INVENTORY_SLOT_UPSERT).empty());
    assert(storage_messages.size() == 3U);
    assert(weather_messages.size() == 1U);
    assert(!collect_messages_of_type(bootstrap_messages, GS1_ENGINE_MESSAGE_SITE_TASK_UPSERT).empty());
    const auto bootstrap_phone_panel_messages =
        collect_messages_of_type(bootstrap_messages, GS1_ENGINE_MESSAGE_SITE_PHONE_PANEL_STATE);
    assert(!bootstrap_phone_panel_messages.empty());
    assert(!collect_messages_of_type(bootstrap_messages, GS1_ENGINE_MESSAGE_SITE_PHONE_LISTING_UPSERT).empty());
    {
        const auto& phone_panel_payload =
            bootstrap_phone_panel_messages.front()->payload_as<Gs1EngineMessagePhonePanelData>();
        assert(phone_panel_payload.active_section == GS1_PHONE_PANEL_SECTION_HOME);
        assert(phone_panel_payload.visible_task_count == 0U);
        assert(phone_panel_payload.accepted_task_count == 1U);
        assert(phone_panel_payload.completed_task_count == 0U);
        assert(phone_panel_payload.claimed_task_count == 0U);
        assert(phone_panel_payload.buy_listing_count >= 3U);
        assert((phone_panel_payload.flags & GS1_PHONE_PANEL_FLAG_LAUNCHER_BADGE) == 0U);
        assert((phone_panel_payload.flags & GS1_PHONE_PANEL_FLAG_TASKS_BADGE) == 0U);
    }
    {
        const auto& weather_payload =
            weather_messages.front()->payload_as<Gs1EngineMessageWeatherData>();
        assert(approx_equal(weather_payload.heat, first_site_content->default_weather_heat));
        assert(approx_equal(weather_payload.wind, first_site_content->default_weather_wind));
        assert(approx_equal(weather_payload.dust, first_site_content->default_weather_dust));
        assert(approx_equal(
            weather_payload.wind_direction_degrees,
            first_site_content->default_weather_wind_direction_degrees));
        assert(weather_payload.event_template_id == 0U);
        assert(weather_payload.event_start_time_minutes == 0.0f);
        assert(weather_payload.event_peak_time_minutes == 0.0f);
        assert(weather_payload.event_peak_duration_minutes == 0.0f);
        assert(weather_payload.event_end_time_minutes == 0.0f);
    }
    {
        const auto* delivery_box_message = find_inventory_storage_message(
            bootstrap_messages,
            GS1_INVENTORY_CONTAINER_DEVICE_STORAGE,
            bootstrap_site_run.camp.delivery_box_tile);
        assert(delivery_box_message != nullptr);
        const auto& delivery_payload =
            delivery_box_message->payload_as<Gs1EngineMessageInventoryStorageData>();
        assert(delivery_payload.storage_id == delivery_box_id(bootstrap_site_run));
        assert(delivery_payload.owner_entity_id == delivery_box_owner_id(bootstrap_site_run));
        assert((delivery_payload.flags & GS1_INVENTORY_STORAGE_FLAG_DELIVERY_BOX) != 0U);
        assert((delivery_payload.flags & GS1_INVENTORY_STORAGE_FLAG_RETRIEVAL_ONLY) != 0U);
        assert(delivery_payload.slot_count == 10U);
    }
    {
        const auto workbench_tile = starter_workbench_tile(bootstrap_site_run);
        const auto* workbench_storage_message = find_inventory_storage_message(
            bootstrap_messages,
            GS1_INVENTORY_CONTAINER_DEVICE_STORAGE,
            workbench_tile);
        assert(workbench_storage_message != nullptr);
        const auto& workbench_payload =
            workbench_storage_message->payload_as<Gs1EngineMessageInventoryStorageData>();
        assert(workbench_payload.storage_id == starter_workbench_id(bootstrap_site_run));
        assert(workbench_payload.owner_entity_id ==
               static_cast<std::uint32_t>(bootstrap_site_run.site_world->device_entity_id(workbench_tile)));
        assert(workbench_payload.slot_count == 8U);
    }

    seed_runtime_test_task(bootstrap_runtime, bootstrap_site_run.counters.site_completion_tile_threshold);
    const auto seeded_task_messages = drain_runtime_messages(bootstrap_runtime);
    assert(!collect_messages_of_type(seeded_task_messages, GS1_ENGINE_MESSAGE_SITE_TASK_UPSERT).empty());
    {
        const auto* seeded_task_message = find_task_message(seeded_task_messages, 1U);
        assert(seeded_task_message != nullptr);
        const auto& payload = seeded_task_message->payload_as<Gs1EngineMessageTaskData>();
        assert(payload.task_template_id == gs1::k_task_template_site1_restore_patch);
        assert(payload.current_progress == 0U);
        assert(payload.target_progress == bootstrap_site_run.counters.site_completion_tile_threshold);
        assert(payload.list_kind == GS1_TASK_PRESENTATION_LIST_VISIBLE);
    }

    GameMessage accept_task {};
    accept_task.type = GameMessageType::TaskAcceptRequested;
    accept_task.set_payload(TaskAcceptRequestedMessage {1U});
    assert(bootstrap_runtime.handle_message(accept_task) == GS1_STATUS_OK);
    assert(bootstrap_site_run.task_board.accepted_task_ids.size() == 1U);

    {
        auto& accepted_task = bootstrap_site_run.task_board.visible_tasks.front();
        accepted_task.task_template_id = gs1::TaskTemplateId {gs1::k_task_template_site1_onboarding_buy_water};
        accepted_task.runtime_list_kind = gs1::TaskRuntimeListKind::Accepted;
        accepted_task.current_progress_amount = 1U;
        accepted_task.target_amount = 3U;

        gs1::TaskInstanceState completed_task {};
        completed_task.task_instance_id = gs1::TaskInstanceId {2U};
        completed_task.task_template_id = gs1::TaskTemplateId {gs1::k_task_template_site1_onboarding_buy_food};
        completed_task.publisher_faction_id = gs1::FactionId {gs1::k_faction_village_committee};
        completed_task.task_tier_id = 1U;
        completed_task.target_amount = 1U;
        completed_task.current_progress_amount = 1U;
        completed_task.runtime_list_kind = gs1::TaskRuntimeListKind::PendingClaim;
        bootstrap_site_run.task_board.visible_tasks.push_back(completed_task);
        bootstrap_site_run.task_board.completed_task_ids.push_back(gs1::TaskInstanceId {2U});

        gs1::GameRuntimeProjectionTestAccess::mark_projection_dirty(
            bootstrap_runtime,
            gs1::SITE_PROJECTION_UPDATE_TASKS | gs1::SITE_PROJECTION_UPDATE_PHONE);
        gs1::GameRuntimeProjectionTestAccess::flush_projection(bootstrap_runtime);
        const auto mixed_task_messages = drain_runtime_messages(bootstrap_runtime);
        {
            const auto* accepted_task_message = find_task_message(mixed_task_messages, 1U);
            assert(accepted_task_message != nullptr);
            const auto& payload = accepted_task_message->payload_as<Gs1EngineMessageTaskData>();
            assert(payload.task_template_id == gs1::k_task_template_site1_onboarding_buy_water);
            assert(payload.current_progress == 1U);
            assert(payload.target_progress == 3U);
            assert(payload.list_kind == GS1_TASK_PRESENTATION_LIST_ACCEPTED);
        }
        {
            const auto* completed_task_message = find_task_message(mixed_task_messages, 2U);
            assert(completed_task_message != nullptr);
            const auto& payload = completed_task_message->payload_as<Gs1EngineMessageTaskData>();
            assert(payload.task_template_id == gs1::k_task_template_site1_onboarding_buy_food);
            assert(payload.current_progress == 1U);
            assert(payload.target_progress == 1U);
            assert(payload.list_kind == GS1_TASK_PRESENTATION_LIST_PENDING_CLAIM);
        }
    }

    bootstrap_site_run.inventory.worker_pack_slots[4].occupied = true;
    bootstrap_site_run.inventory.worker_pack_slots[4].item_id = gs1::ItemId {gs1::k_item_medicine_pack};
    bootstrap_site_run.inventory.worker_pack_slots[4].item_quantity = 1U;
    bootstrap_site_run.inventory.worker_pack_slots[4].item_condition = 1.0f;
    bootstrap_site_run.inventory.worker_pack_slots[4].item_freshness = 1.0f;

    auto worker = gs1::site_world_access::worker_conditions(bootstrap_site_run);
    worker.health = 70.0f;
    gs1::site_world_access::set_worker_conditions(bootstrap_site_run, worker);
    GameMessage use_item {};
    use_item.type = GameMessageType::InventoryItemUseRequested;
    use_item.set_payload(InventoryItemUseRequestedMessage {
        gs1::k_item_medicine_pack,
        bootstrap_site_run.inventory.worker_pack_storage_id,
        1U,
        4U});
    assert(bootstrap_runtime.handle_message(use_item) == GS1_STATUS_OK);
    assert(!bootstrap_site_run.inventory.worker_pack_slots[4].occupied);
    assert(gs1::site_world_access::worker_conditions(bootstrap_site_run).health > 70.0f);

    const auto bought_listing_before = std::find_if(
        bootstrap_site_run.economy.available_phone_listings.begin(),
        bootstrap_site_run.economy.available_phone_listings.end(),
        [](const gs1::PhoneListingState& listing) {
            return listing.listing_id == 1U;
        });
    assert(bought_listing_before != bootstrap_site_run.economy.available_phone_listings.end());
    const auto listing_quantity_before = bought_listing_before->quantity;
    GameMessage buy_listing {};
    buy_listing.type = GameMessageType::PhoneListingPurchaseRequested;
    buy_listing.set_payload(PhoneListingPurchaseRequestedMessage {1U, 1U, 0U});
    assert(bootstrap_runtime.handle_message(buy_listing) == GS1_STATUS_OK);
    assert(bootstrap_site_run.economy.current_cash == 1280);
    const auto bought_listing_after = std::find_if(
        bootstrap_site_run.economy.available_phone_listings.begin(),
        bootstrap_site_run.economy.available_phone_listings.end(),
        [](const gs1::PhoneListingState& listing) {
            return listing.listing_id == 1U;
        });
    assert(bought_listing_after != bootstrap_site_run.economy.available_phone_listings.end());
    assert(
        listing_quantity_before == 0U ||
        bought_listing_after->quantity == (listing_quantity_before - 1U));
    gs1::GameRuntimeProjectionTestAccess::flush_projection(bootstrap_runtime);
    drain_runtime_messages(bootstrap_runtime);

    GameMessage sell_listing {};
    sell_listing.type = GameMessageType::PhoneListingSaleRequested;
    sell_listing.set_payload(gs1::PhoneListingSaleRequestedMessage {1001U, 1U, 0U});
    assert(bootstrap_runtime.handle_message(sell_listing) == GS1_STATUS_OK);
    assert(bootstrap_runtime.handle_message(sell_listing) == GS1_STATUS_OK);
    Gs1Phase1Result sell_result {};
    run_phase1(bootstrap_runtime, 1.0 / 60.0, sell_result);
    gs1::GameRuntimeProjectionTestAccess::flush_projection(bootstrap_runtime);
    const auto sell_projection_messages = drain_runtime_messages(bootstrap_runtime);
    assert(contains_phone_listing_message(
        sell_projection_messages,
        GS1_ENGINE_MESSAGE_SITE_PHONE_LISTING_UPSERT,
        1001U));

    Gs1Phase1Result delivery_result {};
    run_phase1(bootstrap_runtime, k_default_delivery_real_seconds, delivery_result);
    const auto delivered_slot_index = find_delivery_box_slot_index(
        bootstrap_site_run,
        gs1::ItemId {gs1::k_item_water_container},
        1U);
    assert(delivered_slot_index != std::numeric_limits<std::uint16_t>::max());
    gs1::GameRuntimeProjectionTestAccess::flush_projection(bootstrap_runtime);
    const auto delivery_messages = drain_runtime_messages(bootstrap_runtime);
    const auto delivery_inventory_messages = collect_inventory_slot_messages(delivery_messages);
    assert(delivery_inventory_messages.empty());

    const auto first_messages = flush_tile_delta_for(bootstrap_runtime, TileCoord {3, 2}, 0.25f);
    const auto first_tiles = collect_messages_of_type(first_messages, GS1_ENGINE_MESSAGE_SITE_TILE_UPSERT);
    assert(!first_messages.empty());
    assert(first_messages.front().type == GS1_ENGINE_MESSAGE_BEGIN_SITE_SNAPSHOT);
    assert(std::any_of(first_messages.begin(), first_messages.end(), [](const auto& message) {
        return message.type == GS1_ENGINE_MESSAGE_END_SITE_SNAPSHOT;
    }));
    assert(first_tiles.size() == 1U);
    {
        const auto& payload = first_tiles.front()->payload_as<Gs1EngineMessageSiteTileData>();
        assert(payload.x == 3U);
        assert(payload.y == 2U);
        assert(approx_equal(payload.sand_burial, 25.0f));
    }

    auto& site_run = gs1::GameRuntimeProjectionTestAccess::active_site_run(bootstrap_runtime).value();
    const auto repeated_coord = TileCoord {5, 4};
    set_tile_sand_burial(site_run, repeated_coord, 0.5f);
    gs1::GameRuntimeProjectionTestAccess::mark_tile_dirty(bootstrap_runtime, repeated_coord);
    gs1::GameRuntimeProjectionTestAccess::mark_tile_dirty(bootstrap_runtime, repeated_coord);
    set_tile_sand_burial(site_run, TileCoord {1, 1}, 0.75f);
    gs1::GameRuntimeProjectionTestAccess::mark_tile_dirty(bootstrap_runtime, TileCoord {1, 1});
    gs1::GameRuntimeProjectionTestAccess::flush_projection(bootstrap_runtime);

    const auto second_messages = drain_runtime_messages(bootstrap_runtime);
    const auto second_tiles = collect_messages_of_type(second_messages, GS1_ENGINE_MESSAGE_SITE_TILE_UPSERT);
    assert(!second_messages.empty());
    assert(second_tiles.size() == 2U);
    {
        const auto& first_tile = second_tiles[0]->payload_as<Gs1EngineMessageSiteTileData>();
        const auto& second_tile = second_tiles[1]->payload_as<Gs1EngineMessageSiteTileData>();
        assert(first_tile.x == 1U && first_tile.y == 1U);
        assert(approx_equal(first_tile.sand_burial, 75.0f));
        assert(second_tile.x == 5U && second_tile.y == 4U);
        assert(approx_equal(second_tile.sand_burial, 50.0f));
    }

    const auto density_coord = TileCoord {6, 6};
    set_tile_plant_state(site_run, density_coord, gs1::PlantId {gs1::k_plant_straw_checkerboard}, 0.1875f);
    gs1::GameRuntimeProjectionTestAccess::mark_tile_dirty(bootstrap_runtime, density_coord);
    gs1::GameRuntimeProjectionTestAccess::flush_projection(bootstrap_runtime);
    const auto density_first_messages = drain_runtime_messages(bootstrap_runtime);
    const auto density_first_tiles =
        collect_messages_of_type(density_first_messages, GS1_ENGINE_MESSAGE_SITE_TILE_UPSERT);
    assert(density_first_tiles.size() == 1U);

    set_tile_plant_state(site_run, density_coord, gs1::PlantId {gs1::k_plant_straw_checkerboard}, 0.19f);
    gs1::GameRuntimeProjectionTestAccess::mark_tile_dirty(bootstrap_runtime, density_coord);
    gs1::GameRuntimeProjectionTestAccess::flush_projection(bootstrap_runtime);
    const auto density_second_messages = drain_runtime_messages(bootstrap_runtime);
    const auto density_second_tiles =
        collect_messages_of_type(density_second_messages, GS1_ENGINE_MESSAGE_SITE_TILE_UPSERT);
    assert(density_second_tiles.empty());

    set_tile_plant_state(site_run, density_coord, gs1::PlantId {gs1::k_plant_straw_checkerboard}, 0.22f);
    gs1::GameRuntimeProjectionTestAccess::mark_tile_dirty(bootstrap_runtime, density_coord);
    gs1::GameRuntimeProjectionTestAccess::flush_projection(bootstrap_runtime);
    const auto density_third_messages = drain_runtime_messages(bootstrap_runtime);
    const auto density_third_tiles =
        collect_messages_of_type(density_third_messages, GS1_ENGINE_MESSAGE_SITE_TILE_UPSERT);
    assert(density_third_tiles.size() == 1U);
    {
        const auto& payload = density_third_tiles.front()->payload_as<Gs1EngineMessageSiteTileData>();
        assert(payload.x == static_cast<std::uint32_t>(density_coord.x));
        assert(payload.y == static_cast<std::uint32_t>(density_coord.y));
        assert(payload.plant_type_id == gs1::k_plant_straw_checkerboard);
        assert(approx_equal(payload.plant_density, 22.0f));
    }
    const auto density_visual =
        find_plant_visual_message(density_third_messages, gs1::k_plant_straw_checkerboard, density_coord);
    assert(density_visual != nullptr);
    {
        const auto& payload = density_visual->payload_as<Gs1EngineMessageSitePlantVisualData>();
        assert(payload.visual_id != 0U);
        assert(payload.footprint_width == 2U);
        assert(payload.footprint_height == 2U);
        assert(payload.height_scale >= 0.16f);
        assert(payload.height_scale < 0.30f);
        assert(payload.density_quantized > 0U);
        assert(payload.height_class == static_cast<std::uint8_t>(gs1::PlantHeightClass::None));
        assert(payload.focus == static_cast<std::uint8_t>(gs1::PlantFocus::Setup));
        assert((payload.flags & GS1_SITE_PLANT_VISUAL_FLAG_HAS_AURA) != 0U);
        assert((payload.flags & GS1_SITE_PLANT_VISUAL_FLAG_GROWABLE) == 0U);
    }

    const auto device_coord = TileCoord {7, 6};
    set_tile_device_state(site_run, device_coord, gs1::StructureId {gs1::k_structure_workbench}, 0.625f);
    gs1::GameRuntimeProjectionTestAccess::mark_tile_dirty(bootstrap_runtime, device_coord);
    gs1::GameRuntimeProjectionTestAccess::flush_projection(bootstrap_runtime);
    const auto device_first_messages = drain_runtime_messages(bootstrap_runtime);
    const auto device_first_tiles =
        collect_messages_of_type(device_first_messages, GS1_ENGINE_MESSAGE_SITE_TILE_UPSERT);
    assert(device_first_tiles.size() == 1U);
    {
        const auto& payload = device_first_tiles.front()->payload_as<Gs1EngineMessageSiteTileData>();
        assert(payload.structure_type_id == gs1::k_structure_workbench);
        assert(payload.device_integrity_quantized > 0U);
        const float projected_device_integrity =
            static_cast<float>(payload.device_integrity_quantized) * (100.0f / 128.0f);
        assert(approx_equal(projected_device_integrity, 62.5f, 1.0f));
    }

    set_tile_device_state(site_run, device_coord, gs1::StructureId {gs1::k_structure_workbench}, 0.629f);
    gs1::GameRuntimeProjectionTestAccess::mark_tile_dirty(bootstrap_runtime, device_coord);
    gs1::GameRuntimeProjectionTestAccess::flush_projection(bootstrap_runtime);
    const auto device_second_messages = drain_runtime_messages(bootstrap_runtime);
    const auto device_second_tiles =
        collect_messages_of_type(device_second_messages, GS1_ENGINE_MESSAGE_SITE_TILE_UPSERT);
    assert(device_second_tiles.empty());

    set_tile_device_state(site_run, device_coord, gs1::StructureId {gs1::k_structure_workbench}, 0.68f);
    gs1::GameRuntimeProjectionTestAccess::mark_tile_dirty(bootstrap_runtime, device_coord);
    gs1::GameRuntimeProjectionTestAccess::flush_projection(bootstrap_runtime);
    const auto device_third_messages = drain_runtime_messages(bootstrap_runtime);
    const auto device_third_tiles =
        collect_messages_of_type(device_third_messages, GS1_ENGINE_MESSAGE_SITE_TILE_UPSERT);
    assert(device_third_tiles.size() == 1U);
    {
        const auto& payload = device_third_tiles.front()->payload_as<Gs1EngineMessageSiteTileData>();
        const float projected_device_integrity =
            static_cast<float>(payload.device_integrity_quantized) * (100.0f / 128.0f);
        assert(approx_equal(projected_device_integrity, 68.0f, 1.0f));
    }
    const auto device_visual =
        find_device_visual_message(device_third_messages, gs1::k_structure_workbench, device_coord);
    assert(device_visual != nullptr);
    {
        const auto& payload = device_visual->payload_as<Gs1EngineMessageSiteDeviceVisualData>();
        assert(payload.visual_id != 0U);
        assert(approx_equal(payload.integrity_normalized, 0.68f, 0.02f));
        assert(payload.height_scale > 0.5f);
        assert(payload.footprint_width == 1U);
        assert(payload.footprint_height == 1U);
        assert((payload.flags & GS1_SITE_DEVICE_VISUAL_FLAG_IS_CRAFTING_STATION) != 0U);
    }

    set_tile_local_wind(site_run, density_coord, 14.0f);
    gs1::GameRuntimeProjectionTestAccess::mark_tile_dirty(bootstrap_runtime, density_coord);
    gs1::GameRuntimeProjectionTestAccess::flush_projection(bootstrap_runtime);
    const auto wind_first_messages = drain_runtime_messages(bootstrap_runtime);
    const auto wind_first_tiles =
        collect_messages_of_type(wind_first_messages, GS1_ENGINE_MESSAGE_SITE_TILE_UPSERT);
    assert(wind_first_tiles.size() == 1U);
    {
        const auto& payload = wind_first_tiles.front()->payload_as<Gs1EngineMessageSiteTileData>();
        assert(approx_equal(payload.local_wind, 14.0f));
    }

    set_tile_local_wind(site_run, density_coord, 15.0f);
    gs1::GameRuntimeProjectionTestAccess::mark_tile_dirty(bootstrap_runtime, density_coord);
    gs1::GameRuntimeProjectionTestAccess::flush_projection(bootstrap_runtime);
    const auto wind_second_messages = drain_runtime_messages(bootstrap_runtime);
    const auto wind_second_tiles =
        collect_messages_of_type(wind_second_messages, GS1_ENGINE_MESSAGE_SITE_TILE_UPSERT);
    assert(wind_second_tiles.empty());

    set_tile_local_wind(site_run, density_coord, 18.0f);
    gs1::GameRuntimeProjectionTestAccess::mark_tile_dirty(bootstrap_runtime, density_coord);
    gs1::GameRuntimeProjectionTestAccess::flush_projection(bootstrap_runtime);
    const auto wind_third_messages = drain_runtime_messages(bootstrap_runtime);
    const auto wind_third_tiles =
        collect_messages_of_type(wind_third_messages, GS1_ENGINE_MESSAGE_SITE_TILE_UPSERT);
    assert(wind_third_tiles.size() == 1U);
    {
        const auto& payload = wind_third_tiles.front()->payload_as<Gs1EngineMessageSiteTileData>();
        assert(approx_equal(payload.local_wind, 18.0f));
    }

    set_tile_protection_visual_state(site_run, density_coord, 14.0f, 32.0f, 48.0f);
    gs1::GameRuntimeProjectionTestAccess::mark_tile_dirty(bootstrap_runtime, density_coord);
    gs1::GameRuntimeProjectionTestAccess::flush_projection(bootstrap_runtime);
    const auto protection_first_messages = drain_runtime_messages(bootstrap_runtime);
    const auto protection_first_tiles =
        collect_messages_of_type(protection_first_messages, GS1_ENGINE_MESSAGE_SITE_TILE_UPSERT);
    assert(protection_first_tiles.size() == 1U);
    {
        const auto& payload = protection_first_tiles.front()->payload_as<Gs1EngineMessageSiteTileData>();
        assert(approx_equal(payload.wind_protection, 14.0f));
        assert(approx_equal(payload.heat_protection, 32.0f));
        assert(approx_equal(payload.dust_protection, 48.0f));
    }

    set_tile_protection_visual_state(site_run, density_coord, 15.0f, 32.4f, 48.4f);
    gs1::GameRuntimeProjectionTestAccess::mark_tile_dirty(bootstrap_runtime, density_coord);
    gs1::GameRuntimeProjectionTestAccess::flush_projection(bootstrap_runtime);
    const auto protection_second_messages = drain_runtime_messages(bootstrap_runtime);
    const auto protection_second_tiles =
        collect_messages_of_type(protection_second_messages, GS1_ENGINE_MESSAGE_SITE_TILE_UPSERT);
    assert(protection_second_tiles.empty());

    set_tile_protection_visual_state(site_run, density_coord, 18.0f, 36.0f, 52.0f);
    gs1::GameRuntimeProjectionTestAccess::mark_tile_dirty(bootstrap_runtime, density_coord);
    gs1::GameRuntimeProjectionTestAccess::flush_projection(bootstrap_runtime);
    const auto protection_third_messages = drain_runtime_messages(bootstrap_runtime);
    const auto protection_third_tiles =
        collect_messages_of_type(protection_third_messages, GS1_ENGINE_MESSAGE_SITE_TILE_UPSERT);
    assert(protection_third_tiles.size() == 1U);
    {
        const auto& payload = protection_third_tiles.front()->payload_as<Gs1EngineMessageSiteTileData>();
        assert(approx_equal(payload.wind_protection, 18.0f));
        assert(approx_equal(payload.heat_protection, 36.0f));
        assert(approx_equal(payload.dust_protection, 52.0f));
    }

    set_tile_soil_visual_state(site_run, density_coord, 0.25f, 0.125f, 0.375f);
    gs1::GameRuntimeProjectionTestAccess::mark_tile_dirty(bootstrap_runtime, density_coord);
    gs1::GameRuntimeProjectionTestAccess::flush_projection(bootstrap_runtime);
    const auto soil_first_messages = drain_runtime_messages(bootstrap_runtime);
    const auto soil_first_tiles =
        collect_messages_of_type(soil_first_messages, GS1_ENGINE_MESSAGE_SITE_TILE_UPSERT);
    assert(soil_first_tiles.size() == 1U);
    {
        const auto& payload = soil_first_tiles.front()->payload_as<Gs1EngineMessageSiteTileData>();
        assert(approx_equal(payload.moisture, 25.0f));
        assert(approx_equal(payload.soil_fertility, 12.5f));
        assert(approx_equal(payload.soil_salinity, 37.5f));
    }

    set_tile_soil_visual_state(site_run, density_coord, 0.257f, 0.132f, 0.382f);
    gs1::GameRuntimeProjectionTestAccess::mark_tile_dirty(bootstrap_runtime, density_coord);
    gs1::GameRuntimeProjectionTestAccess::flush_projection(bootstrap_runtime);
    const auto soil_second_messages = drain_runtime_messages(bootstrap_runtime);
    const auto soil_second_tiles =
        collect_messages_of_type(soil_second_messages, GS1_ENGINE_MESSAGE_SITE_TILE_UPSERT);
    assert(soil_second_tiles.empty());

    set_tile_soil_visual_state(site_run, density_coord, 0.27f, 0.14f, 0.4f);
    gs1::GameRuntimeProjectionTestAccess::mark_tile_dirty(bootstrap_runtime, density_coord);
    gs1::GameRuntimeProjectionTestAccess::flush_projection(bootstrap_runtime);
    const auto soil_third_messages = drain_runtime_messages(bootstrap_runtime);
    const auto soil_third_tiles =
        collect_messages_of_type(soil_third_messages, GS1_ENGINE_MESSAGE_SITE_TILE_UPSERT);
    assert(soil_third_tiles.size() == 1U);
    {
        const auto& payload = soil_third_tiles.front()->payload_as<Gs1EngineMessageSiteTileData>();
        assert(approx_equal(payload.moisture, 27.0f));
        assert(approx_equal(payload.soil_fertility, 14.0f));
        assert(approx_equal(payload.soil_salinity, 40.0f));
    }

    set_tile_plant_state(site_run, density_coord, gs1::PlantId {}, 0.0f);
    set_tile_excavation_state(site_run, density_coord, gs1::ExcavationDepth::Rough);
    gs1::GameRuntimeProjectionTestAccess::mark_tile_dirty(bootstrap_runtime, density_coord);
    gs1::GameRuntimeProjectionTestAccess::flush_projection(bootstrap_runtime);
    const auto excavation_first_messages = drain_runtime_messages(bootstrap_runtime);
    const auto excavation_first_tiles =
        collect_messages_of_type(excavation_first_messages, GS1_ENGINE_MESSAGE_SITE_TILE_UPSERT);
    assert(excavation_first_tiles.size() == 1U);
    {
        const auto& payload = excavation_first_tiles.front()->payload_as<Gs1EngineMessageSiteTileData>();
        assert(payload.excavation_depth == static_cast<std::uint8_t>(gs1::ExcavationDepth::Rough));
        assert(payload.visible_excavation_depth == static_cast<std::uint8_t>(gs1::ExcavationDepth::Rough));
    }

    set_tile_plant_state(site_run, density_coord, gs1::PlantId {gs1::k_plant_straw_checkerboard}, 0.22f);
    gs1::GameRuntimeProjectionTestAccess::mark_tile_dirty(bootstrap_runtime, density_coord);
    gs1::GameRuntimeProjectionTestAccess::flush_projection(bootstrap_runtime);
    const auto excavation_hidden_messages = drain_runtime_messages(bootstrap_runtime);
    const auto excavation_hidden_tiles =
        collect_messages_of_type(excavation_hidden_messages, GS1_ENGINE_MESSAGE_SITE_TILE_UPSERT);
    assert(excavation_hidden_tiles.size() == 1U);
    {
        const auto& payload = excavation_hidden_tiles.front()->payload_as<Gs1EngineMessageSiteTileData>();
        assert(payload.excavation_depth == static_cast<std::uint8_t>(gs1::ExcavationDepth::Rough));
        assert(payload.visible_excavation_depth == static_cast<std::uint8_t>(gs1::ExcavationDepth::None));
    }

    set_tile_sand_burial(site_run, TileCoord {0, 0}, 0.9f);
    gs1::GameRuntimeProjectionTestAccess::mark_all_tiles_dirty(bootstrap_runtime);
    gs1::GameRuntimeProjectionTestAccess::flush_projection(bootstrap_runtime);
    const auto fallback_messages = drain_runtime_messages(bootstrap_runtime);
    const auto fallback_tiles = collect_messages_of_type(fallback_messages, GS1_ENGINE_MESSAGE_SITE_TILE_UPSERT);
    assert(!fallback_tiles.empty());
    assert(fallback_tiles.size() == gs1::site_world_access::tile_count(site_run));

    Gs1RuntimeCreateDesc phone_panel_desc {};
    phone_panel_desc.struct_size = sizeof(Gs1RuntimeCreateDesc);
    phone_panel_desc.api_version = gs1::k_api_version;
    phone_panel_desc.adapter_config_json_utf8 = nullptr;
    phone_panel_desc.fixed_step_seconds = 1.0 / 60.0;

    GameRuntime phone_panel_runtime {phone_panel_desc};
    assert(phone_panel_runtime.handle_message(make_start_campaign_message()) == GS1_STATUS_OK);
    const auto phone_panel_site_id =
        gs1::GameRuntimeProjectionTestAccess::campaign(phone_panel_runtime)->sites.front().site_id.value;
    assert(phone_panel_runtime.handle_message(make_start_site_attempt_message(phone_panel_site_id)) == GS1_STATUS_OK);
    complete_site_loading(phone_panel_runtime);
    assert(gs1::GameRuntimeProjectionTestAccess::active_site_run(phone_panel_runtime).has_value());
    auto& phone_panel_site_run =
        gs1::GameRuntimeProjectionTestAccess::active_site_run(phone_panel_runtime).value();
    drain_runtime_messages(phone_panel_runtime);

    const auto buy_listing_it = std::find_if(
        phone_panel_site_run.economy.available_phone_listings.begin(),
        phone_panel_site_run.economy.available_phone_listings.end(),
        [](const gs1::PhoneListingState& listing) {
            if (listing.kind != gs1::PhoneListingKind::BuyItem)
            {
                return false;
            }

            const auto* item_def = gs1::find_item_def(listing.item_id);
            return item_def != nullptr && gs1::item_has_capability(*item_def, gs1::ITEM_CAPABILITY_SELL);
        });
    assert(buy_listing_it != phone_panel_site_run.economy.available_phone_listings.end());
    const auto delivered_item_id = buy_listing_it->item_id;
    const auto expected_sell_listing_id = 1000U + delivered_item_id.value;
    const auto delivery_box_container =
        gs1::inventory_storage::delivery_box_container(phone_panel_site_run);
    const auto delivery_quantity_before = gs1::inventory_storage::available_item_quantity_in_container(
        phone_panel_site_run,
        delivery_box_container,
        delivered_item_id);

    GameMessage buy_sellable_listing {};
    buy_sellable_listing.type = GameMessageType::PhoneListingPurchaseRequested;
    buy_sellable_listing.set_payload(PhoneListingPurchaseRequestedMessage {
        buy_listing_it->listing_id,
        1U,
        0U});
    assert(phone_panel_runtime.handle_message(buy_sellable_listing) == GS1_STATUS_OK);
    gs1::GameRuntimeProjectionTestAccess::flush_projection(phone_panel_runtime);
    drain_runtime_messages(phone_panel_runtime);

    Gs1Phase1Result phone_panel_delivery_result {};
    run_phase1(phone_panel_runtime, k_default_delivery_real_seconds, phone_panel_delivery_result);
    const auto delivery_quantity_after = gs1::inventory_storage::available_item_quantity_in_container(
        phone_panel_site_run,
        delivery_box_container,
        delivered_item_id);
    assert(delivery_quantity_after == delivery_quantity_before + 1U);
    gs1::GameRuntimeProjectionTestAccess::flush_projection(phone_panel_runtime);
    const auto phone_panel_delivery_messages = drain_runtime_messages(phone_panel_runtime);
    assert(contains_phone_listing_message(
        phone_panel_delivery_messages,
        GS1_ENGINE_MESSAGE_SITE_PHONE_LISTING_UPSERT,
        expected_sell_listing_id));
    const auto phone_panel_delivery_state_messages =
        collect_messages_of_type(phone_panel_delivery_messages, GS1_ENGINE_MESSAGE_SITE_PHONE_PANEL_STATE);
    assert(!phone_panel_delivery_state_messages.empty());
    {
        const auto& payload =
            phone_panel_delivery_state_messages.back()->payload_as<Gs1EngineMessagePhonePanelData>();
        assert((payload.flags & GS1_PHONE_PANEL_FLAG_LAUNCHER_BADGE) != 0U);
        assert((payload.flags & GS1_PHONE_PANEL_FLAG_SELL_BADGE) != 0U);
    }

    Gs1UiAction open_tasks_action {};
    open_tasks_action.type = GS1_UI_ACTION_SET_PHONE_PANEL_SECTION;
    open_tasks_action.arg0 = GS1_PHONE_PANEL_SECTION_TASKS;
    const auto open_tasks_event = make_ui_action_event(open_tasks_action);
    Gs1Phase1Result open_tasks_result {};
    assert(phone_panel_runtime.submit_host_messages(&open_tasks_event, 1U) == GS1_STATUS_OK);
    run_phase1(phone_panel_runtime, 0.0, open_tasks_result);
    assert(open_tasks_result.processed_host_message_count == 1U);
    const auto open_tasks_messages = drain_runtime_messages(phone_panel_runtime);
    const auto open_tasks_phone_panel_messages =
        collect_messages_of_type(open_tasks_messages, GS1_ENGINE_MESSAGE_SITE_PHONE_PANEL_STATE);
    assert(open_tasks_phone_panel_messages.size() == 1U);
    {
        const auto& payload =
            open_tasks_phone_panel_messages.front()->payload_as<Gs1EngineMessagePhonePanelData>();
        assert(payload.active_section == GS1_PHONE_PANEL_SECTION_TASKS);
        assert((payload.flags & GS1_PHONE_PANEL_FLAG_LAUNCHER_BADGE) == 0U);
        assert((payload.flags & GS1_PHONE_PANEL_FLAG_TASKS_BADGE) == 0U);
    }

    Gs1RuntimeCreateDesc panel_state_desc {};
    panel_state_desc.struct_size = sizeof(Gs1RuntimeCreateDesc);
    panel_state_desc.api_version = gs1::k_api_version;
    panel_state_desc.adapter_config_json_utf8 = nullptr;
    panel_state_desc.fixed_step_seconds = 1.0 / 60.0;

    GameRuntime panel_state_runtime {panel_state_desc};
    run_phase1(panel_state_runtime, 0.0);
    drain_runtime_messages(panel_state_runtime);
    assert(panel_state_runtime.handle_message(make_start_campaign_message()) == GS1_STATUS_OK);
    const auto panel_state_site_id =
        gs1::GameRuntimeProjectionTestAccess::campaign(panel_state_runtime)->sites.front().site_id.value;
    assert(panel_state_runtime.handle_message(make_start_site_attempt_message(panel_state_site_id)) == GS1_STATUS_OK);
    complete_site_loading(panel_state_runtime);
    auto& panel_state_site_run =
        gs1::GameRuntimeProjectionTestAccess::active_site_run(panel_state_runtime).value();
    drain_runtime_messages(panel_state_runtime);

    auto open_panel_storage_event = make_storage_view_event(
        starter_storage_id(panel_state_site_run),
        GS1_INVENTORY_VIEW_EVENT_OPEN_SNAPSHOT);
    Gs1Phase1Result open_panel_storage_result {};
    assert(panel_state_runtime.submit_host_messages(&open_panel_storage_event, 1U) == GS1_STATUS_OK);
    run_phase1(panel_state_runtime, 0.0, open_panel_storage_result);
    assert(open_panel_storage_result.processed_host_message_count == 1U);
    assert(panel_state_site_run.inventory.opened_device_storage_id == starter_storage_id(panel_state_site_run));
    const auto open_panel_storage_messages = drain_runtime_messages(panel_state_runtime);
    assert(find_inventory_view_message(
               open_panel_storage_messages,
               starter_storage_id(panel_state_site_run),
               GS1_INVENTORY_VIEW_EVENT_OPEN_SNAPSHOT) != nullptr);
    assert(find_ui_surface_visibility_message(
               open_panel_storage_messages,
               GS1_UI_SURFACE_SITE_INVENTORY_PANEL,
               true) != nullptr);

    auto open_worker_pack_event = make_storage_view_event(
        panel_state_site_run.inventory.worker_pack_storage_id,
        GS1_INVENTORY_VIEW_EVENT_OPEN_SNAPSHOT);
    Gs1Phase1Result open_worker_pack_result {};
    assert(panel_state_runtime.submit_host_messages(&open_worker_pack_event, 1U) == GS1_STATUS_OK);
    run_phase1(panel_state_runtime, 0.0, open_worker_pack_result);
    assert(open_worker_pack_result.processed_host_message_count == 1U);
    assert(panel_state_site_run.inventory.worker_pack_panel_open);
    assert(panel_state_site_run.inventory.opened_device_storage_id == starter_storage_id(panel_state_site_run));
    const auto open_worker_pack_messages = drain_runtime_messages(panel_state_runtime);
    assert(find_inventory_view_message(
               open_worker_pack_messages,
               panel_state_site_run.inventory.worker_pack_storage_id,
               GS1_INVENTORY_VIEW_EVENT_OPEN_SNAPSHOT) != nullptr);
    assert(find_inventory_view_message(
               open_worker_pack_messages,
               starter_storage_id(panel_state_site_run),
               GS1_INVENTORY_VIEW_EVENT_OPEN_SNAPSHOT) == nullptr);
    assert(find_ui_surface_visibility_message(
               open_worker_pack_messages,
               GS1_UI_SURFACE_SITE_INVENTORY_PANEL,
               true) != nullptr);

    Gs1UiAction open_phone_action {};
    open_phone_action.type = GS1_UI_ACTION_SET_PHONE_PANEL_SECTION;
    open_phone_action.arg0 = GS1_PHONE_PANEL_SECTION_BUY;
    const auto open_phone_event = make_ui_action_event(open_phone_action);
    Gs1Phase1Result open_phone_result {};
    assert(panel_state_runtime.submit_host_messages(&open_phone_event, 1U) == GS1_STATUS_OK);
    run_phase1(panel_state_runtime, 0.0, open_phone_result);
    assert(open_phone_result.processed_host_message_count == 1U);
    assert(panel_state_site_run.phone_panel.open);
    assert(!panel_state_site_run.inventory.worker_pack_panel_open);
    assert(panel_state_site_run.inventory.opened_device_storage_id == 0U);
    const auto open_phone_messages = drain_runtime_messages(panel_state_runtime);
    assert(find_inventory_view_message(
               open_phone_messages,
               panel_state_site_run.inventory.worker_pack_storage_id,
               GS1_INVENTORY_VIEW_EVENT_CLOSE) != nullptr);
    assert(find_inventory_view_message(
               open_phone_messages,
               starter_storage_id(panel_state_site_run),
               GS1_INVENTORY_VIEW_EVENT_CLOSE) != nullptr);
    assert(find_ui_surface_visibility_message(
               open_phone_messages,
               GS1_UI_SURFACE_SITE_INVENTORY_PANEL,
               false) != nullptr);
    assert(find_ui_surface_visibility_message(
               open_phone_messages,
               GS1_UI_SURFACE_SITE_PHONE_PANEL,
               true) != nullptr);
    const auto open_phone_panel_messages =
        collect_messages_of_type(open_phone_messages, GS1_ENGINE_MESSAGE_SITE_PHONE_PANEL_STATE);
    assert(!open_phone_panel_messages.empty());
    {
        const auto& payload =
            open_phone_panel_messages.back()->payload_as<Gs1EngineMessagePhonePanelData>();
        assert((payload.flags & GS1_PHONE_PANEL_FLAG_OPEN) != 0U);
        assert(payload.active_section == GS1_PHONE_PANEL_SECTION_BUY);
    }

    Gs1UiAction open_site_tech_tree_action {};
    open_site_tech_tree_action.type = GS1_UI_ACTION_OPEN_REGIONAL_MAP_TECH_TREE;
    const auto open_site_tech_tree_event = make_ui_action_event(open_site_tech_tree_action);
    Gs1Phase1Result open_site_tech_tree_result {};
    assert(panel_state_runtime.submit_host_messages(&open_site_tech_tree_event, 1U) == GS1_STATUS_OK);
    run_phase1(panel_state_runtime, 0.0, open_site_tech_tree_result);
    assert(open_site_tech_tree_result.processed_host_message_count == 1U);
    const auto open_site_tech_tree_messages = drain_runtime_messages(panel_state_runtime);
    assert(gs1::GameRuntimeProjectionTestAccess::campaign(panel_state_runtime)->regional_map_state.tech_tree_open);
    assert(!panel_state_site_run.phone_panel.open);
    assert(find_ui_surface_visibility_message(
               open_site_tech_tree_messages,
               GS1_UI_SURFACE_SITE_PHONE_PANEL,
               false) != nullptr);
    assert(find_ui_surface_visibility_message(
               open_site_tech_tree_messages,
               GS1_UI_SURFACE_REGIONAL_TECH_TREE_OVERLAY,
               true) != nullptr);
    const auto tech_tree_phone_panel_messages =
        collect_messages_of_type(open_site_tech_tree_messages, GS1_ENGINE_MESSAGE_SITE_PHONE_PANEL_STATE);
    assert(!tech_tree_phone_panel_messages.empty());
    {
        const auto& payload =
            tech_tree_phone_panel_messages.back()->payload_as<Gs1EngineMessagePhonePanelData>();
        assert((payload.flags & GS1_PHONE_PANEL_FLAG_OPEN) == 0U);
    }
    assert(contains_progression_view_begin(
        open_site_tech_tree_messages,
        GS1_PROGRESSION_VIEW_REGIONAL_MAP_TECH_TREE));

    assert(panel_state_runtime.submit_host_messages(&open_worker_pack_event, 1U) == GS1_STATUS_OK);
    Gs1Phase1Result reopen_worker_pack_result {};
    run_phase1(panel_state_runtime, 0.0, reopen_worker_pack_result);
    assert(reopen_worker_pack_result.processed_host_message_count == 1U);
    assert(panel_state_site_run.inventory.worker_pack_panel_open);
    assert(!gs1::GameRuntimeProjectionTestAccess::campaign(panel_state_runtime)->regional_map_state.tech_tree_open);
    const auto reopen_worker_pack_messages = drain_runtime_messages(panel_state_runtime);
    assert(find_ui_surface_visibility_message(
               reopen_worker_pack_messages,
               GS1_UI_SURFACE_REGIONAL_TECH_TREE_OVERLAY,
               false) != nullptr);
    assert(find_ui_surface_visibility_message(
               reopen_worker_pack_messages,
               GS1_UI_SURFACE_SITE_INVENTORY_PANEL,
               true) != nullptr);
    {
        bool found_progression_close = false;
        for (const auto& message : reopen_worker_pack_messages)
        {
            if (message.type != GS1_ENGINE_MESSAGE_CLOSE_PROGRESSION_VIEW)
            {
                continue;
            }

            const auto& payload = message.payload_as<Gs1EngineMessageCloseProgressionViewData>();
            if (payload.view_id == GS1_PROGRESSION_VIEW_REGIONAL_MAP_TECH_TREE)
            {
                found_progression_close = true;
                break;
            }
        }
        assert(found_progression_close);
    }
    assert(find_inventory_view_message(
               reopen_worker_pack_messages,
               panel_state_site_run.inventory.worker_pack_storage_id,
               GS1_INVENTORY_VIEW_EVENT_OPEN_SNAPSHOT) != nullptr);

    Gs1UiAction open_protection_selector_action {};
    open_protection_selector_action.type = GS1_UI_ACTION_OPEN_SITE_PROTECTION_SELECTOR;
    const auto open_protection_selector_event = make_ui_action_event(open_protection_selector_action);
    Gs1Phase1Result open_protection_selector_result {};
    assert(panel_state_runtime.submit_host_messages(&open_protection_selector_event, 1U) == GS1_STATUS_OK);
    run_phase1(panel_state_runtime, 0.0, open_protection_selector_result);
    assert(open_protection_selector_result.processed_host_message_count == 1U);
    assert(gs1::GameRuntimeProjectionTestAccess::site_protection_selector_open(panel_state_runtime));
    assert(
        gs1::GameRuntimeProjectionTestAccess::site_protection_overlay_mode(panel_state_runtime) ==
        GS1_SITE_PROTECTION_OVERLAY_NONE);
    const auto open_protection_selector_messages = drain_runtime_messages(panel_state_runtime);
    assert(find_inventory_view_message(
               open_protection_selector_messages,
               panel_state_site_run.inventory.worker_pack_storage_id,
               GS1_INVENTORY_VIEW_EVENT_CLOSE) != nullptr);
    assert(find_ui_element_by_content(open_protection_selector_messages, 3U) != nullptr);
    assert(find_ui_element_by_content(
               open_protection_selector_messages,
               4U,
               GS1_SITE_PROTECTION_OVERLAY_WIND) != nullptr);
    assert(find_ui_element_by_content(
               open_protection_selector_messages,
               4U,
               GS1_SITE_PROTECTION_OVERLAY_HEAT) != nullptr);
    assert(find_ui_element_by_content(
               open_protection_selector_messages,
               4U,
               GS1_SITE_PROTECTION_OVERLAY_DUST) != nullptr);
    assert(find_ui_element_by_content(
               open_protection_selector_messages,
               4U,
               GS1_SITE_PROTECTION_OVERLAY_OCCUPANT_CONDITION) != nullptr);

    Gs1UiAction select_heat_overlay_action {};
    select_heat_overlay_action.type = GS1_UI_ACTION_SET_SITE_PROTECTION_OVERLAY_MODE;
    select_heat_overlay_action.arg0 = GS1_SITE_PROTECTION_OVERLAY_HEAT;
    const auto select_heat_overlay_event = make_ui_action_event(select_heat_overlay_action);
    Gs1Phase1Result select_heat_overlay_result {};
    assert(panel_state_runtime.submit_host_messages(&select_heat_overlay_event, 1U) == GS1_STATUS_OK);
    run_phase1(panel_state_runtime, 0.0, select_heat_overlay_result);
    assert(select_heat_overlay_result.processed_host_message_count == 1U);
    assert(!gs1::GameRuntimeProjectionTestAccess::site_protection_selector_open(panel_state_runtime));
    assert(
        gs1::GameRuntimeProjectionTestAccess::site_protection_overlay_mode(panel_state_runtime) ==
        GS1_SITE_PROTECTION_OVERLAY_HEAT);
    const auto select_heat_overlay_messages = drain_runtime_messages(panel_state_runtime);
    assert(find_close_ui_setup_message(
               select_heat_overlay_messages,
               GS1_UI_SETUP_SITE_PROTECTION_SELECTOR) != nullptr);
    assert(find_ui_surface_visibility_message(
               select_heat_overlay_messages,
               GS1_UI_SURFACE_SITE_OVERLAY_PANEL,
               true) != nullptr);
    const auto protection_overlay_messages =
        collect_messages_of_type(select_heat_overlay_messages, GS1_ENGINE_MESSAGE_SITE_PROTECTION_OVERLAY_STATE);
    assert(protection_overlay_messages.size() == 1U);
    {
        const auto& payload =
            protection_overlay_messages.front()->payload_as<Gs1EngineMessageSiteProtectionOverlayData>();
        assert(payload.mode == GS1_SITE_PROTECTION_OVERLAY_HEAT);
    }

    assert(panel_state_runtime.submit_host_messages(&open_site_tech_tree_event, 1U) == GS1_STATUS_OK);
    Gs1Phase1Result open_tech_tree_from_overlay_result {};
    run_phase1(panel_state_runtime, 0.0, open_tech_tree_from_overlay_result);
    assert(open_tech_tree_from_overlay_result.processed_host_message_count == 1U);
    assert(!gs1::GameRuntimeProjectionTestAccess::site_protection_selector_open(panel_state_runtime));
    assert(
        gs1::GameRuntimeProjectionTestAccess::site_protection_overlay_mode(panel_state_runtime) ==
        GS1_SITE_PROTECTION_OVERLAY_NONE);
    const auto open_tech_tree_from_overlay_messages = drain_runtime_messages(panel_state_runtime);
    assert(find_ui_surface_visibility_message(
               open_tech_tree_from_overlay_messages,
               GS1_UI_SURFACE_SITE_OVERLAY_PANEL,
               false) != nullptr);
    const auto cleared_by_tech_tree_messages =
        collect_messages_of_type(
            open_tech_tree_from_overlay_messages,
            GS1_ENGINE_MESSAGE_SITE_PROTECTION_OVERLAY_STATE);
    assert(cleared_by_tech_tree_messages.size() == 1U);
    {
        const auto& payload =
            cleared_by_tech_tree_messages.front()->payload_as<Gs1EngineMessageSiteProtectionOverlayData>();
        assert(payload.mode == GS1_SITE_PROTECTION_OVERLAY_NONE);
    }

    assert(panel_state_runtime.submit_host_messages(&open_phone_event, 1U) == GS1_STATUS_OK);
    Gs1Phase1Result reopen_phone_result {};
    run_phase1(panel_state_runtime, 0.0, reopen_phone_result);
    assert(reopen_phone_result.processed_host_message_count == 1U);
    const auto reopen_phone_messages = drain_runtime_messages(panel_state_runtime);
    assert(find_ui_surface_visibility_message(
               reopen_phone_messages,
               GS1_UI_SURFACE_SITE_OVERLAY_PANEL,
               false) != nullptr);
    assert(find_ui_surface_visibility_message(
               reopen_phone_messages,
               GS1_UI_SURFACE_SITE_PHONE_PANEL,
               true) != nullptr);
    const auto cleared_overlay_messages =
        collect_messages_of_type(reopen_phone_messages, GS1_ENGINE_MESSAGE_SITE_PROTECTION_OVERLAY_STATE);
    assert(cleared_overlay_messages.size() == 1U);
    {
        const auto& payload =
            cleared_overlay_messages.front()->payload_as<Gs1EngineMessageSiteProtectionOverlayData>();
        assert(payload.mode == GS1_SITE_PROTECTION_OVERLAY_NONE);
    }
    assert(panel_state_site_run.phone_panel.open);

    Gs1RuntimeCreateDesc ui_desc {};
    ui_desc.struct_size = sizeof(Gs1RuntimeCreateDesc);
    ui_desc.api_version = gs1::k_api_version;
    ui_desc.adapter_config_json_utf8 = nullptr;
    ui_desc.fixed_step_seconds = 1.0 / 60.0;

    GameRuntime ui_runtime {ui_desc};
    assert(ui_runtime.handle_message(make_start_campaign_message()) == GS1_STATUS_OK);
    const auto ui_site_id =
        gs1::GameRuntimeProjectionTestAccess::campaign(ui_runtime)->sites.front().site_id.value;
    assert(ui_runtime.handle_message(make_start_site_attempt_message(ui_site_id)) == GS1_STATUS_OK);
    complete_site_loading(ui_runtime);
    auto& ui_site_run = gs1::GameRuntimeProjectionTestAccess::active_site_run(ui_runtime).value();
    drain_runtime_messages(ui_runtime);

    auto open_storage_event = make_storage_view_event(
        starter_storage_id(ui_site_run),
        GS1_INVENTORY_VIEW_EVENT_OPEN_SNAPSHOT);
    Gs1Phase1Result open_storage_result {};
    assert(ui_runtime.submit_host_messages(&open_storage_event, 1U) == GS1_STATUS_OK);
    run_phase1(ui_runtime, 0.0, open_storage_result);
    assert(open_storage_result.processed_host_message_count == 1U);
    const auto open_storage_messages = drain_runtime_messages(ui_runtime);
    assert(!collect_messages_of_type(open_storage_messages, GS1_ENGINE_MESSAGE_SITE_INVENTORY_VIEW_STATE).empty());
    assert(find_inventory_slot_message(
               open_storage_messages,
               GS1_INVENTORY_CONTAINER_DEVICE_STORAGE,
               starter_storage_id(ui_site_run),
               0U) != nullptr);

    auto ui_worker = gs1::site_world_access::worker_conditions(ui_site_run);
    ui_worker.hydration = 70.0f;
    gs1::site_world_access::set_worker_conditions(ui_site_run, ui_worker);

    auto transfer_event = make_inventory_slot_tap_event(
        starter_storage_id(ui_site_run),
        GS1_INVENTORY_CONTAINER_DEVICE_STORAGE,
        0U);

    Gs1Phase1Result transfer_result {};
    assert(ui_runtime.submit_host_messages(&transfer_event, 1U) == GS1_STATUS_OK);
    run_phase1(ui_runtime, 0.0, transfer_result);
    assert(transfer_result.processed_host_message_count == 1U);
    assert(gs1::inventory_storage::available_item_quantity_in_container(
               ui_site_run,
               gs1::inventory_storage::starter_storage_container(ui_site_run),
               gs1::ItemId {gs1::k_item_water_container}) == 0U);
    assert(ui_site_run.inventory.worker_pack_slots[0].occupied);
    assert(ui_site_run.inventory.worker_pack_slots[0].item_id.value == gs1::k_item_water_container);
    assert(ui_site_run.inventory.worker_pack_slots[0].item_quantity == 1U);
    const auto transfer_messages = drain_runtime_messages(ui_runtime);
    const auto transfer_inventory_messages = collect_inventory_slot_messages(transfer_messages);
    assert(!transfer_inventory_messages.empty());
    assert(find_inventory_slot_message(
               transfer_messages,
               GS1_INVENTORY_CONTAINER_DEVICE_STORAGE,
               starter_storage_id(ui_site_run),
               0U) != nullptr);
    assert(find_inventory_slot_message(
               transfer_messages,
               GS1_INVENTORY_CONTAINER_WORKER_PACK,
               ui_site_run.inventory.worker_pack_storage_id,
               0U) != nullptr);

    auto use_event = make_inventory_slot_tap_event(
        ui_site_run.inventory.worker_pack_storage_id,
        GS1_INVENTORY_CONTAINER_WORKER_PACK,
        0U,
        ui_site_run.inventory.worker_pack_slots[0].item_instance_id);

    Gs1Phase1Result use_result {};
    assert(ui_runtime.submit_host_messages(&use_event, 1U) == GS1_STATUS_OK);
    run_phase1(ui_runtime, 0.0, use_result);
    assert(use_result.processed_host_message_count == 1U);
    assert(ui_site_run.inventory.worker_pack_slots[0].occupied);
    assert(ui_site_run.inventory.worker_pack_slots[0].item_quantity == 1U);
    assert(gs1::site_world_access::worker_conditions(ui_site_run).hydration == 70.0f);
    const auto use_messages = drain_runtime_messages(ui_runtime);
    const auto use_inventory_messages = collect_inventory_slot_messages(use_messages);
    assert(use_inventory_messages.empty());
    assert(!collect_messages_of_type(use_messages, GS1_ENGINE_MESSAGE_SITE_ACTION_UPDATE).empty());
    assert(!collect_messages_of_type(use_messages, GS1_ENGINE_MESSAGE_SITE_WORKER_UPDATE).empty());
    assert(!collect_messages_of_type(use_messages, GS1_ENGINE_MESSAGE_HUD_STATE).empty());

    Gs1RuntimeCreateDesc water_action_desc {};
    water_action_desc.struct_size = sizeof(Gs1RuntimeCreateDesc);
    water_action_desc.api_version = gs1::k_api_version;
    water_action_desc.adapter_config_json_utf8 = nullptr;
    water_action_desc.fixed_step_seconds = 60.0;

    GameRuntime water_action_runtime {water_action_desc};
    assert(water_action_runtime.handle_message(make_start_campaign_message()) == GS1_STATUS_OK);
    const auto water_action_site_id =
        gs1::GameRuntimeProjectionTestAccess::campaign(water_action_runtime)->sites.front().site_id.value;
    assert(water_action_runtime.handle_message(make_start_site_attempt_message(water_action_site_id)) == GS1_STATUS_OK);
    complete_site_loading(water_action_runtime);
    drain_runtime_messages(water_action_runtime);

    const TileCoord water_target {2, 2};
    auto& water_site_run =
        gs1::GameRuntimeProjectionTestAccess::active_site_run(water_action_runtime).value();
    water_site_run.site_action.current_action_id = gs1::RuntimeActionId {77U};
    water_site_run.site_action.action_kind = gs1::ActionKind::Water;
    water_site_run.site_action.target_tile = water_target;
    water_site_run.site_action.primary_subject_id = 17U;
    water_site_run.site_action.total_action_minutes = 0.75;
    water_site_run.site_action.remaining_action_minutes = 0.75;
    water_site_run.site_action.started_at_world_minute = water_site_run.clock.world_time_minutes;

    GameMessage water_started {};
    water_started.type = GameMessageType::SiteActionStarted;
    water_started.set_payload(gs1::SiteActionStartedMessage {
        77U,
        GS1_SITE_ACTION_WATER,
        0U,
        0U,
        water_target.x,
        water_target.y,
        17U,
        0.75f});
    assert(water_action_runtime.handle_message(water_started) == GS1_STATUS_OK);
    const auto water_start_messages = drain_runtime_messages(water_action_runtime);
    const auto water_action_updates =
        collect_messages_of_type(water_start_messages, GS1_ENGINE_MESSAGE_SITE_ACTION_UPDATE);
    assert(water_action_updates.size() == 1U);
    const auto& water_action_payload =
        water_action_updates.front()->payload_as<Gs1EngineMessageSiteActionData>();
    assert((water_action_payload.flags & GS1_SITE_ACTION_PRESENTATION_FLAG_ACTIVE) != 0U);
    assert(water_action_payload.action_kind == GS1_SITE_ACTION_WATER);
    assert(water_action_payload.target_tile_x == water_target.x);
    assert(water_action_payload.target_tile_y == water_target.y);
    assert(water_action_payload.duration_minutes == 0.75f);
    assert(water_action_payload.progress_normalized == 0.0f);

    water_site_run.site_action = {};
    GameMessage water_completed {};
    water_completed.type = GameMessageType::SiteActionCompleted;
    water_completed.set_payload(gs1::SiteActionCompletedMessage {
        77U,
        GS1_SITE_ACTION_WATER,
        0U,
        0U,
        water_target.x,
        water_target.y,
        17U,
        0U});
    assert(water_action_runtime.handle_message(water_completed) == GS1_STATUS_OK);
    const auto water_complete_messages = drain_runtime_messages(water_action_runtime);
    const auto water_clear_updates =
        collect_messages_of_type(water_complete_messages, GS1_ENGINE_MESSAGE_SITE_ACTION_UPDATE);
    assert(water_clear_updates.size() == 1U);
    {
        const auto& payload = water_clear_updates.front()->payload_as<Gs1EngineMessageSiteActionData>();
        assert((payload.flags & GS1_SITE_ACTION_PRESENTATION_FLAG_CLEAR) != 0U);
        assert(payload.action_kind == GS1_SITE_ACTION_NONE);
        assert(payload.action_id == 0U);
        assert(payload.duration_minutes == 0.0f);
    }

    Gs1RuntimeCreateDesc action_desc {};
    action_desc.struct_size = sizeof(Gs1RuntimeCreateDesc);
    action_desc.api_version = gs1::k_api_version;
    action_desc.adapter_config_json_utf8 = nullptr;
    action_desc.fixed_step_seconds = 60.0;

    GameRuntime action_runtime {action_desc};
    assert(action_runtime.handle_message(make_start_campaign_message()) == GS1_STATUS_OK);
    const auto action_site_id =
        gs1::GameRuntimeProjectionTestAccess::campaign(action_runtime)->sites.front().site_id.value;
    assert(action_runtime.handle_message(make_start_site_attempt_message(action_site_id)) == GS1_STATUS_OK);
    complete_site_loading(action_runtime);
    assert(gs1::GameRuntimeProjectionTestAccess::active_site_run(action_runtime).has_value());
    drain_runtime_messages(action_runtime);
    auto& action_site_run =
        gs1::GameRuntimeProjectionTestAccess::active_site_run(action_runtime).value();
    action_site_run.inventory.worker_pack_slots[3].occupied = true;
    action_site_run.inventory.worker_pack_slots[3].item_id = gs1::ItemId {gs1::k_item_ordos_wormwood_seed_bundle};
    action_site_run.inventory.worker_pack_slots[3].item_quantity = 2U;
    action_site_run.inventory.worker_pack_slots[3].item_condition = 1.0f;
    action_site_run.inventory.worker_pack_slots[3].item_freshness = 1.0f;

    const TileCoord action_target {5, 4};
    auto action_event = make_site_action_request_event(
        GS1_SITE_ACTION_PLANT,
        action_target,
        0U,
        gs1::k_item_ordos_wormwood_seed_bundle);
    assert(action_runtime.submit_host_messages(&action_event, 1U) == GS1_STATUS_OK);
    gs1::SiteWorld::TileEcologyData action_ecology {};
    for (int step = 0; step < 10; ++step)
    {
        run_phase1(action_runtime, 60.0);
        action_ecology = gs1::site_world_access::tile_ecology(action_site_run, action_target);
        if (action_ecology.plant_id.value == gs1::k_plant_ordos_wormwood)
        {
            break;
        }
    }

    assert(action_ecology.plant_id.value == gs1::k_plant_ordos_wormwood);
    assert(approx_equal(action_ecology.plant_density, 100.0f));
    assert(action_site_run.inventory.worker_pack_slots[3].item_quantity == 1U);

    run_phase1(action_runtime, 0.0);
    const auto action_messages = drain_runtime_messages(action_runtime);
    const auto action_tile_messages =
        collect_messages_of_type(action_messages, GS1_ENGINE_MESSAGE_SITE_TILE_UPSERT);
    const auto projected_action_tile = std::find_if(
        action_tile_messages.begin(),
        action_tile_messages.end(),
        [&](const Gs1RuntimeMessage* message) {
            const auto& payload = message->payload_as<Gs1EngineMessageSiteTileData>();
            return payload.x == static_cast<std::uint32_t>(action_target.x) &&
                payload.y == static_cast<std::uint32_t>(action_target.y) &&
                payload.plant_type_id == gs1::k_plant_ordos_wormwood &&
                approx_equal(payload.plant_density, 100.0f);
        });
    assert(projected_action_tile != action_tile_messages.end());

    const auto dirty_tile_messages = flush_tile_delta_for(action_runtime, action_target, 42.0f);
    const auto dirty_tile_upserts = collect_messages_of_type(dirty_tile_messages, GS1_ENGINE_MESSAGE_SITE_TILE_UPSERT);
    assert(dirty_tile_upserts.size() == 1U);
    {
        const auto& payload = dirty_tile_upserts.front()->payload_as<Gs1EngineMessageSiteTileData>();
        assert(payload.x == static_cast<std::uint32_t>(action_target.x));
        assert(payload.y == static_cast<std::uint32_t>(action_target.y));
        assert(approx_equal(payload.sand_burial, 42.0f));
    }

    Gs1RuntimeCreateDesc placement_preview_desc {};
    placement_preview_desc.struct_size = sizeof(Gs1RuntimeCreateDesc);
    placement_preview_desc.api_version = gs1::k_api_version;
    placement_preview_desc.adapter_config_json_utf8 = nullptr;
    placement_preview_desc.fixed_step_seconds = 1.0 / 60.0;

    GameRuntime placement_preview_runtime {placement_preview_desc};
    assert(placement_preview_runtime.handle_message(make_start_campaign_message()) == GS1_STATUS_OK);
    const auto placement_preview_site_id =
        gs1::GameRuntimeProjectionTestAccess::campaign(placement_preview_runtime)->sites.front().site_id.value;
    assert(placement_preview_runtime.handle_message(make_start_site_attempt_message(placement_preview_site_id)) == GS1_STATUS_OK);
    complete_site_loading(placement_preview_runtime);
    auto& placement_preview_site_run =
        gs1::GameRuntimeProjectionTestAccess::active_site_run(placement_preview_runtime).value();
    drain_runtime_messages(placement_preview_runtime);

    placement_preview_site_run.inventory.worker_pack_slots[2].occupied = true;
    placement_preview_site_run.inventory.worker_pack_slots[2].item_id = gs1::ItemId {gs1::k_item_ordos_wormwood_seed_bundle};
    placement_preview_site_run.inventory.worker_pack_slots[2].item_quantity = 2U;
    placement_preview_site_run.inventory.worker_pack_slots[2].item_condition = 1.0f;
    placement_preview_site_run.inventory.worker_pack_slots[2].item_freshness = 1.0f;

    auto placement_mode_event = make_site_action_request_event(
        GS1_SITE_ACTION_PLANT,
        TileCoord {4, 4},
        0U,
        gs1::k_item_ordos_wormwood_seed_bundle,
        GS1_SITE_ACTION_REQUEST_FLAG_HAS_ITEM |
            GS1_SITE_ACTION_REQUEST_FLAG_DEFERRED_TARGET_SELECTION);
    assert(placement_preview_runtime.submit_host_messages(&placement_mode_event, 1U) == GS1_STATUS_OK);
    run_phase1(placement_preview_runtime, 0.0);
    const auto placement_mode_messages = drain_runtime_messages(placement_preview_runtime);
    const auto placement_preview_messages =
        collect_messages_of_type(placement_mode_messages, GS1_ENGINE_MESSAGE_SITE_PLACEMENT_PREVIEW);
    assert(placement_preview_messages.size() == 1U);
    {
        const auto& payload =
            placement_preview_messages.front()->payload_as<Gs1EngineMessagePlacementPreviewData>();
        assert(payload.tile_x == 4);
        assert(payload.tile_y == 4);
        assert(payload.item_id == gs1::k_item_ordos_wormwood_seed_bundle);
        assert(payload.action_kind == GS1_SITE_ACTION_PLANT);
        assert(payload.footprint_width == 2U);
        assert(payload.footprint_height == 2U);
        assert((payload.flags & 1U) != 0U);
        assert((payload.flags & 2U) != 0U);
        assert(payload.blocked_mask == 0ULL);
        assert(payload.preview_tile_count > 0U);
    }
    const auto preview_tile_messages =
        collect_messages_of_type(placement_mode_messages, GS1_ENGINE_MESSAGE_SITE_PLACEMENT_PREVIEW_TILE_UPSERT);
    assert(!preview_tile_messages.empty());
    const auto preview_anchor_tile = std::find_if(
        preview_tile_messages.begin(),
        preview_tile_messages.end(),
        [](const Gs1RuntimeMessage* message) {
            const auto& payload = message->payload_as<Gs1EngineMessagePlacementPreviewTileData>();
            return payload.x == 4 && payload.y == 4 &&
                (payload.flags & GS1_PLACEMENT_PREVIEW_TILE_FLAG_OCCUPIED) != 0U &&
                (payload.flags & GS1_PLACEMENT_PREVIEW_TILE_FLAG_PLANT) != 0U &&
                approx_equal(payload.occupant_condition, 100.0f);
        });
    assert(preview_anchor_tile != preview_tile_messages.end());

    Gs1UiAction placement_open_protection_selector_action {};
    placement_open_protection_selector_action.type = GS1_UI_ACTION_OPEN_SITE_PROTECTION_SELECTOR;
    const auto placement_open_protection_selector_event =
        make_ui_action_event(placement_open_protection_selector_action);
    assert(placement_preview_runtime.submit_host_messages(&placement_open_protection_selector_event, 1U) == GS1_STATUS_OK);
    run_phase1(placement_preview_runtime, 0.0);
    assert(gs1::GameRuntimeProjectionTestAccess::site_protection_selector_open(placement_preview_runtime));
    assert(
        gs1::GameRuntimeProjectionTestAccess::site_protection_overlay_mode(placement_preview_runtime) ==
        GS1_SITE_PROTECTION_OVERLAY_NONE);
    drain_runtime_messages(placement_preview_runtime);

    Gs1UiAction placement_select_heat_overlay_action {};
    placement_select_heat_overlay_action.type = GS1_UI_ACTION_SET_SITE_PROTECTION_OVERLAY_MODE;
    placement_select_heat_overlay_action.arg0 = GS1_SITE_PROTECTION_OVERLAY_HEAT;
    const auto placement_select_heat_overlay_event = make_ui_action_event(placement_select_heat_overlay_action);
    assert(placement_preview_runtime.submit_host_messages(&placement_select_heat_overlay_event, 1U) == GS1_STATUS_OK);
    run_phase1(placement_preview_runtime, 0.0);
    assert(
        gs1::GameRuntimeProjectionTestAccess::site_protection_overlay_mode(placement_preview_runtime) ==
        GS1_SITE_PROTECTION_OVERLAY_HEAT);
    const auto placement_overlay_messages = drain_runtime_messages(placement_preview_runtime);
    const auto placement_overlay_state_messages =
        collect_messages_of_type(placement_overlay_messages, GS1_ENGINE_MESSAGE_SITE_PROTECTION_OVERLAY_STATE);
    assert(placement_overlay_state_messages.size() == 1U);
    {
        const auto& payload =
            placement_overlay_state_messages.front()->payload_as<Gs1EngineMessageSiteProtectionOverlayData>();
        assert(payload.mode == GS1_SITE_PROTECTION_OVERLAY_HEAT);
    }

    auto blocked_tile = placement_preview_site_run.site_world->tile_at(TileCoord {5, 4});
    blocked_tile.ecology.ground_cover_type_id = 9U;
    placement_preview_site_run.site_world->set_tile(TileCoord {5, 4}, blocked_tile);

    auto cursor_update_event = make_site_context_request_event(TileCoord {5, 5});
    assert(placement_preview_runtime.submit_host_messages(&cursor_update_event, 1U) == GS1_STATUS_OK);
    run_phase1(placement_preview_runtime, 0.0);
    const auto cursor_update_messages = drain_runtime_messages(placement_preview_runtime);
    const auto blocked_preview_messages =
        collect_messages_of_type(cursor_update_messages, GS1_ENGINE_MESSAGE_SITE_PLACEMENT_PREVIEW);
    assert(blocked_preview_messages.size() == 1U);
    {
        const auto& payload =
            blocked_preview_messages.front()->payload_as<Gs1EngineMessagePlacementPreviewData>();
        assert(payload.tile_x == 4);
        assert(payload.tile_y == 4);
        assert((payload.flags & 1U) != 0U);
        assert((payload.flags & 2U) == 0U);
        assert(payload.blocked_mask == (1ULL << 1U));
        assert(payload.preview_tile_count == 0U);
    }
    assert(
        gs1::GameRuntimeProjectionTestAccess::site_protection_overlay_mode(placement_preview_runtime) ==
        GS1_SITE_PROTECTION_OVERLAY_HEAT);

    auto blocked_confirm_event = make_site_action_request_event(
        GS1_SITE_ACTION_PLANT,
        TileCoord {5, 5},
        0U,
        gs1::k_item_ordos_wormwood_seed_bundle,
        GS1_SITE_ACTION_REQUEST_FLAG_HAS_ITEM);
    assert(placement_preview_runtime.submit_host_messages(&blocked_confirm_event, 1U) == GS1_STATUS_OK);
    run_phase1(placement_preview_runtime, 0.0);
    const auto blocked_confirm_messages = drain_runtime_messages(placement_preview_runtime);
    const auto placement_failure_messages =
        collect_messages_of_type(blocked_confirm_messages, GS1_ENGINE_MESSAGE_SITE_PLACEMENT_FAILURE);
    assert(placement_failure_messages.size() == 1U);
    {
        const auto& payload =
            placement_failure_messages.front()->payload_as<Gs1EngineMessagePlacementFailureData>();
        assert(payload.tile_x == 4);
        assert(payload.tile_y == 4);
        assert(payload.action_kind == GS1_SITE_ACTION_PLANT);
        assert(payload.blocked_mask == (1ULL << 1U));
        assert(payload.sequence_id != 0U);
        assert((payload.flags & 1U) != 0U);
    }

    auto cancel_placement_event = make_site_action_cancel_event(GS1_SITE_ACTION_CANCEL_FLAG_PLACEMENT_MODE);
    assert(placement_preview_runtime.submit_host_messages(&cancel_placement_event, 1U) == GS1_STATUS_OK);
    run_phase1(placement_preview_runtime, 0.0);
    const auto cancel_placement_messages = drain_runtime_messages(placement_preview_runtime);
    const auto clear_preview_messages =
        collect_messages_of_type(cancel_placement_messages, GS1_ENGINE_MESSAGE_SITE_PLACEMENT_PREVIEW);
    assert(clear_preview_messages.size() == 1U);
    {
        const auto& payload =
            clear_preview_messages.front()->payload_as<Gs1EngineMessagePlacementPreviewData>();
        assert((payload.flags & 4U) != 0U);
        assert(payload.action_kind == GS1_SITE_ACTION_NONE);
        assert(payload.item_id == 0U);
    }

    Gs1RuntimeCreateDesc storage_walk_desc {};
    storage_walk_desc.struct_size = sizeof(Gs1RuntimeCreateDesc);
    storage_walk_desc.api_version = gs1::k_api_version;
    storage_walk_desc.adapter_config_json_utf8 = nullptr;
    storage_walk_desc.fixed_step_seconds = 0.1;

    GameRuntime storage_walk_runtime {storage_walk_desc};
    assert(storage_walk_runtime.handle_message(make_start_campaign_message()) == GS1_STATUS_OK);
    const auto storage_walk_site_id =
        gs1::GameRuntimeProjectionTestAccess::campaign(storage_walk_runtime)->sites.front().site_id.value;
    assert(storage_walk_runtime.handle_message(make_start_site_attempt_message(storage_walk_site_id)) == GS1_STATUS_OK);
    complete_site_loading(storage_walk_runtime);
    auto& storage_walk_site_run =
        gs1::GameRuntimeProjectionTestAccess::active_site_run(storage_walk_runtime).value();
    drain_runtime_messages(storage_walk_runtime);

    auto worker_position = gs1::site_world_access::worker_position(storage_walk_site_run);
    worker_position.tile_coord = TileCoord {7, 7};
    worker_position.tile_x = 7.0f;
    worker_position.tile_y = 7.0f;
    gs1::site_world_access::set_worker_position(storage_walk_site_run, worker_position);

    auto distant_open_event = make_storage_view_event(
        starter_storage_id(storage_walk_site_run),
        GS1_INVENTORY_VIEW_EVENT_OPEN_SNAPSHOT);
    Gs1Phase1Result distant_open_result {};
    assert(storage_walk_runtime.submit_host_messages(&distant_open_event, 1U) == GS1_STATUS_OK);
    run_phase1(storage_walk_runtime, 0.1, distant_open_result);
    assert(distant_open_result.fixed_steps_executed == 1U);
    assert(distant_open_result.processed_host_message_count == 1U);
    assert(storage_walk_site_run.inventory.opened_device_storage_id == 0U);
    assert(storage_walk_site_run.inventory.pending_device_storage_open.active);
    const auto distant_open_messages = drain_runtime_messages(storage_walk_runtime);
    assert(collect_messages_of_type(
               distant_open_messages,
               GS1_ENGINE_MESSAGE_SITE_INVENTORY_VIEW_STATE)
               .empty());
    assert(collect_messages_of_type(
               distant_open_messages,
               GS1_ENGINE_MESSAGE_SITE_INVENTORY_SLOT_UPSERT)
               .empty());

    auto move_cancel_event = make_move_direction_event(1.0f, 0.0f, 0.0f);
    Gs1Phase1Result move_cancel_result {};
    assert(storage_walk_runtime.submit_host_messages(&move_cancel_event, 1U) == GS1_STATUS_OK);
    run_phase1(storage_walk_runtime, 0.1, move_cancel_result);
    assert(move_cancel_result.fixed_steps_executed == 1U);
    assert(move_cancel_result.processed_host_message_count == 1U);
    assert(!storage_walk_site_run.inventory.pending_device_storage_open.active);
    assert(storage_walk_site_run.inventory.opened_device_storage_id == 0U);
    drain_runtime_messages(storage_walk_runtime);

    for (int step = 0; step < 12; ++step)
    {
        run_phase1(storage_walk_runtime, 0.1);
    }

    assert(!storage_walk_site_run.inventory.pending_device_storage_open.active);
    assert(storage_walk_site_run.inventory.opened_device_storage_id == 0U);

    Gs1RuntimeCreateDesc storage_close_desc {};
    storage_close_desc.struct_size = sizeof(Gs1RuntimeCreateDesc);
    storage_close_desc.api_version = gs1::k_api_version;
    storage_close_desc.adapter_config_json_utf8 = nullptr;
    storage_close_desc.fixed_step_seconds = 1.0;

    GameRuntime storage_close_runtime {storage_close_desc};
    assert(storage_close_runtime.handle_message(make_start_campaign_message()) == GS1_STATUS_OK);
    const auto storage_close_site_id =
        gs1::GameRuntimeProjectionTestAccess::campaign(storage_close_runtime)->sites.front().site_id.value;
    assert(storage_close_runtime.handle_message(make_start_site_attempt_message(storage_close_site_id)) == GS1_STATUS_OK);
    complete_site_loading(storage_close_runtime);
    auto& storage_close_site_run =
        gs1::GameRuntimeProjectionTestAccess::active_site_run(storage_close_runtime).value();
    drain_runtime_messages(storage_close_runtime);

    auto close_worker_position = gs1::site_world_access::worker_position(storage_close_site_run);
    close_worker_position.tile_coord = TileCoord {
        storage_close_site_run.camp.starter_storage_tile.x + 1,
        storage_close_site_run.camp.starter_storage_tile.y};
    close_worker_position.tile_x = static_cast<float>(close_worker_position.tile_coord.x);
    close_worker_position.tile_y = static_cast<float>(close_worker_position.tile_coord.y);
    gs1::site_world_access::set_worker_position(storage_close_site_run, close_worker_position);

    auto close_open_event = make_storage_view_event(
        starter_storage_id(storage_close_site_run),
        GS1_INVENTORY_VIEW_EVENT_OPEN_SNAPSHOT);
    Gs1Phase1Result close_open_result {};
    assert(storage_close_runtime.submit_host_messages(&close_open_event, 1U) == GS1_STATUS_OK);
    run_phase1(storage_close_runtime, 0.0, close_open_result);
    assert(close_open_result.processed_host_message_count == 1U);
    assert(storage_close_site_run.inventory.opened_device_storage_id == starter_storage_id(storage_close_site_run));
    const auto close_open_messages = drain_runtime_messages(storage_close_runtime);
    const auto close_open_view_messages =
        collect_messages_of_type(close_open_messages, GS1_ENGINE_MESSAGE_SITE_INVENTORY_VIEW_STATE);
    assert(!close_open_view_messages.empty());

    const auto storage_tile = storage_close_site_run.camp.starter_storage_tile;
    const auto world_width = static_cast<std::int32_t>(storage_close_site_run.site_world->width());
    const auto world_height = static_cast<std::int32_t>(storage_close_site_run.site_world->height());
    TileCoord out_of_range_tile {
        std::min(close_worker_position.tile_coord.x + 2, world_width - 1),
        close_worker_position.tile_coord.y};
    if (out_of_range_tile.x >= storage_tile.x - 1 &&
        out_of_range_tile.x <= storage_tile.x + 1 &&
        out_of_range_tile.y >= storage_tile.y - 1 &&
        out_of_range_tile.y <= storage_tile.y + 1)
    {
        out_of_range_tile = TileCoord {
            close_worker_position.tile_coord.x,
            std::min(close_worker_position.tile_coord.y + 2, world_height - 1)};
    }
    assert(
        out_of_range_tile.x < storage_tile.x - 1 ||
        out_of_range_tile.x > storage_tile.x + 1 ||
        out_of_range_tile.y < storage_tile.y - 1 ||
        out_of_range_tile.y > storage_tile.y + 1);

    auto out_of_range_position = gs1::site_world_access::worker_position(storage_close_site_run);
    out_of_range_position.tile_coord = out_of_range_tile;
    out_of_range_position.tile_x = static_cast<float>(out_of_range_tile.x);
    out_of_range_position.tile_y = static_cast<float>(out_of_range_tile.y);
    gs1::site_world_access::set_worker_position(storage_close_site_run, out_of_range_position);

    Gs1Phase1Result move_away_result {};
    run_phase1(storage_close_runtime, 1.0, move_away_result);
    assert(move_away_result.fixed_steps_executed == 1U);
    assert(move_away_result.processed_host_message_count == 0U);
    assert(storage_close_site_run.inventory.opened_device_storage_id == 0U);

    const auto close_messages = drain_runtime_messages(storage_close_runtime);
    const auto close_view_messages =
        collect_messages_of_type(close_messages, GS1_ENGINE_MESSAGE_SITE_INVENTORY_VIEW_STATE);
    const auto close_view_message = std::find_if(
        close_view_messages.begin(),
        close_view_messages.end(),
        [&](const Gs1RuntimeMessage* message) {
            const auto& payload = message->payload_as<Gs1EngineMessageInventoryViewData>();
            return payload.storage_id == starter_storage_id(storage_close_site_run) &&
                payload.event_kind == GS1_INVENTORY_VIEW_EVENT_CLOSE;
        });
    assert(close_view_message != close_view_messages.end());

    return 0;
}


