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
    static std::optional<CampaignState>& campaign(GameRuntime& runtime)
    {
        return runtime.campaign_;
    }

    static std::optional<SiteRunState>& active_site_run(GameRuntime& runtime)
    {
        return runtime.active_site_run_;
    }

    static void mark_tile_dirty(GameRuntime& runtime, TileCoord coord)
    {
        runtime.mark_site_tile_projection_dirty(coord);
    }

    static void mark_all_tiles_dirty(GameRuntime& runtime)
    {
        runtime.mark_site_projection_update_dirty(1ULL << 0);
    }

    static void flush_projection(GameRuntime& runtime)
    {
        runtime.flush_site_presentation_if_dirty();
    }

    static void mark_projection_dirty(GameRuntime& runtime, std::uint64_t dirty_flags)
    {
        runtime.mark_site_projection_update_dirty(dirty_flags);
    }

    static bool site_protection_selector_open(const GameRuntime& runtime)
    {
        return runtime.site_protection_selector_open_;
    }

    static Gs1SiteProtectionOverlayMode site_protection_overlay_mode(const GameRuntime& runtime)
    {
        return runtime.site_protection_overlay_mode_;
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

std::vector<Gs1EngineMessage> drain_engine_messages(GameRuntime& runtime)
{
    std::vector<Gs1EngineMessage> messages {};
    Gs1EngineMessage message {};
    while (runtime.pop_engine_message(message) == GS1_STATUS_OK)
    {
        messages.push_back(message);
    }
    return messages;
}

Gs1HostEvent make_site_action_request_event(
    Gs1SiteActionKind action_kind,
    TileCoord target_tile,
    std::uint32_t primary_subject_id,
    std::uint32_t item_id = 0U,
    std::uint8_t flags = 0U)
{
    Gs1HostEvent event {};
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

Gs1HostEvent make_site_context_request_event(TileCoord target_tile)
{
    Gs1HostEvent event {};
    event.type = GS1_HOST_EVENT_SITE_CONTEXT_REQUEST;
    event.payload.site_context_request.tile_x = target_tile.x;
    event.payload.site_context_request.tile_y = target_tile.y;
    event.payload.site_context_request.flags = 0U;
    return event;
}

Gs1HostEvent make_site_action_cancel_event(std::uint32_t flags)
{
    Gs1HostEvent event {};
    event.type = GS1_HOST_EVENT_SITE_ACTION_CANCEL;
    event.payload.site_action_cancel.action_id = 0U;
    event.payload.site_action_cancel.flags = flags;
    return event;
}

std::uint64_t pack_inventory_use_arg(
    Gs1InventoryContainerKind container_kind,
    std::uint32_t slot_index,
    std::uint32_t quantity)
{
    return static_cast<std::uint64_t>(container_kind) |
        (static_cast<std::uint64_t>(slot_index & 0xffU) << 8U) |
        (static_cast<std::uint64_t>(quantity & 0xffffU) << 16U);
}

std::uint64_t pack_inventory_transfer_arg(
    Gs1InventoryContainerKind source_container_kind,
    std::uint32_t source_slot_index,
    Gs1InventoryContainerKind destination_container_kind,
    std::uint32_t quantity)
{
    return static_cast<std::uint64_t>(source_container_kind) |
        (static_cast<std::uint64_t>(source_slot_index & 0xffU) << 8U) |
        (static_cast<std::uint64_t>(destination_container_kind) << 16U) |
        (static_cast<std::uint64_t>(quantity & 0xffffU) << 24U);
}

std::uint64_t pack_inventory_transfer_owner_arg(
    std::uint32_t source_owner_id,
    std::uint32_t destination_owner_id)
{
    return static_cast<std::uint64_t>(source_owner_id) |
        (static_cast<std::uint64_t>(destination_owner_id) << 32U);
}

Gs1HostEvent make_ui_action_event(const Gs1UiAction& action)
{
    Gs1HostEvent event {};
    event.type = GS1_HOST_EVENT_UI_ACTION;
    event.payload.ui_action.action = action;
    return event;
}

Gs1HostEvent make_storage_view_event(
    std::uint32_t storage_id,
    Gs1InventoryViewEventKind event_kind)
{
    Gs1HostEvent event {};
    event.type = GS1_HOST_EVENT_SITE_STORAGE_VIEW;
    event.payload.site_storage_view.storage_id = storage_id;
    event.payload.site_storage_view.event_kind = event_kind;
    return event;
}

Gs1HostEvent make_move_direction_event(float world_move_x, float world_move_y, float world_move_z)
{
    Gs1HostEvent event {};
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

std::vector<Gs1EngineMessage> flush_tile_delta_for(
    GameRuntime& runtime,
    TileCoord coord,
    float sand_burial)
{
    auto& site_run = gs1::GameRuntimeProjectionTestAccess::active_site_run(runtime).value();
    set_tile_sand_burial(site_run, coord, sand_burial);
    gs1::GameRuntimeProjectionTestAccess::mark_tile_dirty(runtime, coord);
    gs1::GameRuntimeProjectionTestAccess::flush_projection(runtime);
    return drain_engine_messages(runtime);
}

std::vector<const Gs1EngineMessage*> collect_messages_of_type(
    const std::vector<Gs1EngineMessage>& messages,
    Gs1EngineMessageType type)
{
    std::vector<const Gs1EngineMessage*> matches {};
    for (const auto& message : messages)
    {
        if (message.type == type)
        {
            matches.push_back(&message);
        }
    }
    return matches;
}

std::vector<const Gs1EngineMessage*> collect_inventory_slot_messages(
    const std::vector<Gs1EngineMessage>& messages)
{
    return collect_messages_of_type(messages, GS1_ENGINE_MESSAGE_SITE_INVENTORY_SLOT_UPSERT);
}

const Gs1EngineMessage* find_inventory_storage_message(
    const std::vector<Gs1EngineMessage>& messages,
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

const Gs1EngineMessage* find_inventory_slot_message(
    const std::vector<Gs1EngineMessage>& messages,
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

const Gs1EngineMessage* find_task_message(
    const std::vector<Gs1EngineMessage>& messages,
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

const Gs1EngineMessage* find_inventory_view_message(
    const std::vector<Gs1EngineMessage>& messages,
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

const Gs1EngineMessage* find_close_ui_setup_message(
    const std::vector<Gs1EngineMessage>& messages,
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

bool contains_phone_listing_message(
    const std::vector<Gs1EngineMessage>& messages,
    Gs1EngineMessageType message_type,
    std::uint32_t listing_id)
{
    return std::any_of(messages.begin(), messages.end(), [&](const Gs1EngineMessage& message) {
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

bool contains_ui_element_text(
    const std::vector<Gs1EngineMessage>& messages,
    const char* expected_text)
{
    for (const auto& message : messages)
    {
        if (message.type != GS1_ENGINE_MESSAGE_UI_ELEMENT_UPSERT)
        {
            continue;
        }

        const auto& payload = message.payload_as<Gs1EngineMessageUiElementData>();
        if (std::strcmp(payload.text, expected_text) == 0)
        {
            return true;
        }
    }

    return false;
}

bool contains_ui_element_text_fragment(
    const std::vector<Gs1EngineMessage>& messages,
    const char* expected_fragment)
{
    for (const auto& message : messages)
    {
        if (message.type != GS1_ENGINE_MESSAGE_UI_ELEMENT_UPSERT)
        {
            continue;
        }

        const auto& payload = message.payload_as<Gs1EngineMessageUiElementData>();
        if (std::strstr(payload.text, expected_fragment) != nullptr)
        {
            return true;
        }
    }

    return false;
}

const Gs1EngineMessage* find_regional_map_site_message(
    const std::vector<Gs1EngineMessage>& messages,
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
    const std::vector<Gs1EngineMessage>& messages,
    std::uint32_t from_site_id,
    std::uint32_t to_site_id)
{
    return std::any_of(messages.begin(), messages.end(), [&](const Gs1EngineMessage& message) {
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

    GameRuntime runtime {create_desc};

    assert(runtime.handle_message(make_start_campaign_message()) == GS1_STATUS_OK);
    assert(gs1::GameRuntimeProjectionTestAccess::campaign(runtime).has_value());
    const auto initial_map_messages = drain_engine_messages(runtime);
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
        assert(site_two_message->payload_as<Gs1EngineMessageRegionalMapSiteData>().map_x == 160);
        assert(site_two_message->payload_as<Gs1EngineMessageRegionalMapSiteData>().map_y == 0);
        assert(site_three_message->payload_as<Gs1EngineMessageRegionalMapSiteData>().map_x == 320);
        assert(site_three_message->payload_as<Gs1EngineMessageRegionalMapSiteData>().map_y == 160);
        assert(site_four_message->payload_as<Gs1EngineMessageRegionalMapSiteData>().map_x == 480);
        assert(site_four_message->payload_as<Gs1EngineMessageRegionalMapSiteData>().map_y == 160);
        assert(contains_regional_map_link(initial_map_messages, 1U, 2U));
        assert(contains_regional_map_link(initial_map_messages, 2U, 3U));
        assert(contains_regional_map_link(initial_map_messages, 3U, 4U));
    }

    const auto campaign_site_id = gs1::GameRuntimeProjectionTestAccess::campaign(runtime)->sites.front().site_id.value;
    assert(runtime.handle_message(make_select_site_message(campaign_site_id)) == GS1_STATUS_OK);
    const auto loadout_ui_messages = drain_engine_messages(runtime);
    assert(contains_ui_element_text(loadout_ui_messages, "Water x1"));
    assert(contains_ui_element_text_fragment(loadout_ui_messages, "Basic Straw Checkerboard"));
    assert(contains_ui_element_text(loadout_ui_messages, "Open Tech Tree"));

    GameMessage open_tech_tree_message {};
    open_tech_tree_message.type = GameMessageType::OpenRegionalMapTechTree;
    open_tech_tree_message.set_payload(gs1::OpenRegionalMapTechTreeMessage {});
    assert(runtime.handle_message(open_tech_tree_message) == GS1_STATUS_OK);
    const auto tech_tree_messages = drain_engine_messages(runtime);
    assert(contains_ui_element_text(tech_tree_messages, "Prototype Tech Tree"));
    assert(contains_ui_element_text(tech_tree_messages, "Starter Plants"));
    assert(contains_ui_element_text(tech_tree_messages, "Tier Tech Tree"));
    assert(contains_ui_element_text(tech_tree_messages, "Checkerboard | Wormwood"));
    assert(contains_ui_element_text(tech_tree_messages, "Tab: Village"));
    assert(contains_ui_element_text(tech_tree_messages, "Tab: University"));

    GameMessage select_forestry_tab_message {};
    select_forestry_tab_message.type = GameMessageType::SelectRegionalMapTechTreeFaction;
    select_forestry_tab_message.set_payload(gs1::SelectRegionalMapTechTreeFactionMessage {
        gs1::k_faction_forestry_bureau});
    assert(runtime.handle_message(select_forestry_tab_message) == GS1_STATUS_OK);
    const auto forestry_messages = drain_engine_messages(runtime);
    assert(contains_ui_element_text(forestry_messages, "Tab: Forestry"));

    GameMessage select_university_tab_message {};
    select_university_tab_message.type = GameMessageType::SelectRegionalMapTechTreeFaction;
    select_university_tab_message.set_payload(gs1::SelectRegionalMapTechTreeFactionMessage {
        gs1::k_faction_agricultural_university});
    assert(runtime.handle_message(select_university_tab_message) == GS1_STATUS_OK);
    const auto university_messages = drain_engine_messages(runtime);
    assert(contains_ui_element_text(university_messages, "Tab: University"));

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

    GameMessage select_village_tab_message {};
    select_village_tab_message.type = GameMessageType::SelectRegionalMapTechTreeFaction;
    select_village_tab_message.set_payload(gs1::SelectRegionalMapTechTreeFactionMessage {
        gs1::k_faction_village_committee});
    assert(runtime.handle_message(select_village_tab_message) == GS1_STATUS_OK);
    const auto awarded_messages = drain_engine_messages(runtime);
    assert(contains_ui_element_text(awarded_messages, "Tab: Village"));

    GameMessage claim_tech_node_message {};
    claim_tech_node_message.type = GameMessageType::TechnologyNodeClaimRequested;
    claim_tech_node_message.set_payload(gs1::TechnologyNodeClaimRequestedMessage {
        gs1::base_technology_node_id(gs1::FactionId {gs1::k_faction_village_committee}, 1U),
        gs1::k_faction_village_committee});
    assert(runtime.handle_message(claim_tech_node_message) == GS1_STATUS_OK);
    const auto claimed_messages = drain_engine_messages(runtime);
    assert(contains_ui_element_text(claimed_messages, "Prototype Tech Tree"));

    GameRuntime support_runtime {create_desc};
    assert(support_runtime.handle_message(make_start_campaign_message()) == GS1_STATUS_OK);
    assert(gs1::GameRuntimeProjectionTestAccess::campaign(support_runtime).has_value());
    auto& support_campaign = gs1::GameRuntimeProjectionTestAccess::campaign(support_runtime).value();
    support_campaign.sites[0].site_state = GS1_SITE_STATE_COMPLETED;
    support_campaign.sites[1].site_state = GS1_SITE_STATE_AVAILABLE;
    support_campaign.regional_map_state.available_site_ids = {gs1::SiteId {2U}};
    support_campaign.regional_map_state.completed_site_ids = {gs1::SiteId {1U}};
    drain_engine_messages(support_runtime);
    assert(support_runtime.handle_message(make_select_site_message(2U)) == GS1_STATUS_OK);
    const auto support_loadout_messages = drain_engine_messages(support_runtime);
    assert(contains_ui_element_text(support_loadout_messages, "White Thorn Seeds x4"));
    assert(contains_ui_element_text(support_loadout_messages, "Wood x2"));
    assert(contains_ui_element_text(support_loadout_messages, "Adj Support x1"));
    assert(contains_ui_element_text(support_loadout_messages, "Aura Ready x1"));
    {
        const auto* contributor_message = find_regional_map_site_message(support_loadout_messages, 1U);
        assert(contributor_message != nullptr);
        const auto& payload = contributor_message->payload_as<Gs1EngineMessageRegionalMapSiteData>();
        assert(payload.support_package_id == 1001U);
        assert(payload.support_preview_mask != 0U);
    }
    {
        const auto* target_message = find_regional_map_site_message(support_loadout_messages, 2U);
        assert(target_message != nullptr);
        const auto& payload = target_message->payload_as<Gs1EngineMessageRegionalMapSiteData>();
        assert(payload.support_preview_mask == 0U);
    }
    assert(support_runtime.handle_message(make_start_site_attempt_message(2U)) == GS1_STATUS_OK);
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
    drain_engine_messages(support_runtime);

    assert(support_runtime.handle_message(open_tech_tree_message) == GS1_STATUS_OK);
    const auto site_tech_tree_messages = drain_engine_messages(support_runtime);
    assert(contains_ui_element_text(site_tech_tree_messages, "Prototype Tech Tree"));
    assert(contains_ui_element_text(site_tech_tree_messages, "Tab: Village"));
    assert(contains_ui_element_text(site_tech_tree_messages, "Tier Tech Tree"));

    assert(support_runtime.handle_message(select_forestry_tab_message) == GS1_STATUS_OK);
    const auto site_forestry_messages = drain_engine_messages(support_runtime);
    assert(contains_ui_element_text(site_forestry_messages, "Tab: Forestry"));

    const auto first_site_id = campaign_site_id;
    const auto* first_site_content = gs1::find_prototype_site_content(gs1::SiteId {first_site_id});
    assert(first_site_content != nullptr);
    assert(runtime.handle_message(make_start_site_attempt_message(first_site_id)) == GS1_STATUS_OK);
    assert(gs1::GameRuntimeProjectionTestAccess::active_site_run(runtime).has_value());

    auto& bootstrap_site_run = gs1::GameRuntimeProjectionTestAccess::active_site_run(runtime).value();
    assert(gs1::site_world_access::width(bootstrap_site_run) == 32U);
    assert(gs1::site_world_access::height(bootstrap_site_run) == 32U);
    assert(bootstrap_site_run.inventory.worker_pack_slots.size() == 12U);
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

    const auto bootstrap_messages = drain_engine_messages(runtime);
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

    seed_runtime_test_task(runtime, bootstrap_site_run.counters.site_completion_tile_threshold);
    const auto seeded_task_messages = drain_engine_messages(runtime);
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
    assert(runtime.handle_message(accept_task) == GS1_STATUS_OK);
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
            runtime,
            gs1::SITE_PROJECTION_UPDATE_TASKS | gs1::SITE_PROJECTION_UPDATE_PHONE);
        gs1::GameRuntimeProjectionTestAccess::flush_projection(runtime);
        const auto mixed_task_messages = drain_engine_messages(runtime);
        const auto mixed_phone_panel_messages =
            collect_messages_of_type(mixed_task_messages, GS1_ENGINE_MESSAGE_SITE_PHONE_PANEL_STATE);
        assert(!mixed_phone_panel_messages.empty());
        {
            const auto& phone_panel_payload =
                mixed_phone_panel_messages.front()->payload_as<Gs1EngineMessagePhonePanelData>();
            assert(phone_panel_payload.visible_task_count == 0U);
            assert(phone_panel_payload.accepted_task_count == 1U);
            assert(phone_panel_payload.completed_task_count == 1U);
            assert(phone_panel_payload.claimed_task_count == 0U);
        }
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
    assert(runtime.handle_message(use_item) == GS1_STATUS_OK);
    assert(!bootstrap_site_run.inventory.worker_pack_slots[4].occupied);
    assert(gs1::site_world_access::worker_conditions(bootstrap_site_run).health > 70.0f);

    GameMessage buy_listing {};
    buy_listing.type = GameMessageType::PhoneListingPurchaseRequested;
    buy_listing.set_payload(PhoneListingPurchaseRequestedMessage {1U, 1U, 0U});
    assert(runtime.handle_message(buy_listing) == GS1_STATUS_OK);
    assert(bootstrap_site_run.economy.current_cash == 1280);
    assert(bootstrap_site_run.economy.available_phone_listings[0].quantity == 5U);
    gs1::GameRuntimeProjectionTestAccess::flush_projection(runtime);
    drain_engine_messages(runtime);

    GameMessage sell_listing {};
    sell_listing.type = GameMessageType::PhoneListingSaleRequested;
    sell_listing.set_payload(gs1::PhoneListingSaleRequestedMessage {1001U, 1U, 0U});
    assert(runtime.handle_message(sell_listing) == GS1_STATUS_OK);
    assert(runtime.handle_message(sell_listing) == GS1_STATUS_OK);
    Gs1Phase1Result sell_result {};
    run_phase1(runtime, 1.0 / 60.0, sell_result);
    gs1::GameRuntimeProjectionTestAccess::flush_projection(runtime);
    const auto sell_projection_messages = drain_engine_messages(runtime);
    assert(contains_phone_listing_message(
        sell_projection_messages,
        GS1_ENGINE_MESSAGE_SITE_PHONE_LISTING_UPSERT,
        1001U));

    Gs1Phase1Result delivery_result {};
    run_phase1(runtime, k_default_delivery_real_seconds, delivery_result);
    const auto delivered_slot_index = find_delivery_box_slot_index(
        bootstrap_site_run,
        gs1::ItemId {gs1::k_item_water_container},
        1U);
    assert(delivered_slot_index != std::numeric_limits<std::uint16_t>::max());
    gs1::GameRuntimeProjectionTestAccess::flush_projection(runtime);
    const auto delivery_messages = drain_engine_messages(runtime);
    const auto delivery_inventory_messages = collect_inventory_slot_messages(delivery_messages);
    assert(delivery_inventory_messages.empty());

    const auto first_messages = flush_tile_delta_for(runtime, TileCoord {3, 2}, 0.25f);
    const auto first_tiles = collect_messages_of_type(first_messages, GS1_ENGINE_MESSAGE_SITE_TILE_UPSERT);
    assert(first_messages.size() == 3U);
    assert(first_messages.front().type == GS1_ENGINE_MESSAGE_BEGIN_SITE_SNAPSHOT);
    assert(first_messages.back().type == GS1_ENGINE_MESSAGE_END_SITE_SNAPSHOT);
    assert(first_tiles.size() == 1U);
    {
        const auto& payload = first_tiles.front()->payload_as<Gs1EngineMessageSiteTileData>();
        assert(payload.x == 3U);
        assert(payload.y == 2U);
        assert(approx_equal(payload.sand_burial, 25.0f));
    }

    auto& site_run = gs1::GameRuntimeProjectionTestAccess::active_site_run(runtime).value();
    const auto repeated_coord = TileCoord {5, 4};
    set_tile_sand_burial(site_run, repeated_coord, 0.5f);
    gs1::GameRuntimeProjectionTestAccess::mark_tile_dirty(runtime, repeated_coord);
    gs1::GameRuntimeProjectionTestAccess::mark_tile_dirty(runtime, repeated_coord);
    set_tile_sand_burial(site_run, TileCoord {1, 1}, 0.75f);
    gs1::GameRuntimeProjectionTestAccess::mark_tile_dirty(runtime, TileCoord {1, 1});
    gs1::GameRuntimeProjectionTestAccess::flush_projection(runtime);

    const auto second_messages = drain_engine_messages(runtime);
    const auto second_tiles = collect_messages_of_type(second_messages, GS1_ENGINE_MESSAGE_SITE_TILE_UPSERT);
    assert(second_messages.size() == 4U);
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
    gs1::GameRuntimeProjectionTestAccess::mark_tile_dirty(runtime, density_coord);
    gs1::GameRuntimeProjectionTestAccess::flush_projection(runtime);
    const auto density_first_messages = drain_engine_messages(runtime);
    const auto density_first_tiles =
        collect_messages_of_type(density_first_messages, GS1_ENGINE_MESSAGE_SITE_TILE_UPSERT);
    assert(density_first_tiles.size() == 1U);

    set_tile_plant_state(site_run, density_coord, gs1::PlantId {gs1::k_plant_straw_checkerboard}, 0.19f);
    gs1::GameRuntimeProjectionTestAccess::mark_tile_dirty(runtime, density_coord);
    gs1::GameRuntimeProjectionTestAccess::flush_projection(runtime);
    const auto density_second_messages = drain_engine_messages(runtime);
    const auto density_second_tiles =
        collect_messages_of_type(density_second_messages, GS1_ENGINE_MESSAGE_SITE_TILE_UPSERT);
    assert(density_second_tiles.empty());

    set_tile_plant_state(site_run, density_coord, gs1::PlantId {gs1::k_plant_straw_checkerboard}, 0.22f);
    gs1::GameRuntimeProjectionTestAccess::mark_tile_dirty(runtime, density_coord);
    gs1::GameRuntimeProjectionTestAccess::flush_projection(runtime);
    const auto density_third_messages = drain_engine_messages(runtime);
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

    set_tile_local_wind(site_run, density_coord, 14.0f);
    gs1::GameRuntimeProjectionTestAccess::mark_tile_dirty(runtime, density_coord);
    gs1::GameRuntimeProjectionTestAccess::flush_projection(runtime);
    const auto wind_first_messages = drain_engine_messages(runtime);
    const auto wind_first_tiles =
        collect_messages_of_type(wind_first_messages, GS1_ENGINE_MESSAGE_SITE_TILE_UPSERT);
    assert(wind_first_tiles.size() == 1U);
    {
        const auto& payload = wind_first_tiles.front()->payload_as<Gs1EngineMessageSiteTileData>();
        assert(approx_equal(payload.local_wind, 14.0f));
    }

    set_tile_local_wind(site_run, density_coord, 15.0f);
    gs1::GameRuntimeProjectionTestAccess::mark_tile_dirty(runtime, density_coord);
    gs1::GameRuntimeProjectionTestAccess::flush_projection(runtime);
    const auto wind_second_messages = drain_engine_messages(runtime);
    const auto wind_second_tiles =
        collect_messages_of_type(wind_second_messages, GS1_ENGINE_MESSAGE_SITE_TILE_UPSERT);
    assert(wind_second_tiles.empty());

    set_tile_local_wind(site_run, density_coord, 18.0f);
    gs1::GameRuntimeProjectionTestAccess::mark_tile_dirty(runtime, density_coord);
    gs1::GameRuntimeProjectionTestAccess::flush_projection(runtime);
    const auto wind_third_messages = drain_engine_messages(runtime);
    const auto wind_third_tiles =
        collect_messages_of_type(wind_third_messages, GS1_ENGINE_MESSAGE_SITE_TILE_UPSERT);
    assert(wind_third_tiles.size() == 1U);
    {
        const auto& payload = wind_third_tiles.front()->payload_as<Gs1EngineMessageSiteTileData>();
        assert(approx_equal(payload.local_wind, 18.0f));
    }

    set_tile_protection_visual_state(site_run, density_coord, 14.0f, 32.0f, 48.0f);
    gs1::GameRuntimeProjectionTestAccess::mark_tile_dirty(runtime, density_coord);
    gs1::GameRuntimeProjectionTestAccess::flush_projection(runtime);
    const auto protection_first_messages = drain_engine_messages(runtime);
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
    gs1::GameRuntimeProjectionTestAccess::mark_tile_dirty(runtime, density_coord);
    gs1::GameRuntimeProjectionTestAccess::flush_projection(runtime);
    const auto protection_second_messages = drain_engine_messages(runtime);
    const auto protection_second_tiles =
        collect_messages_of_type(protection_second_messages, GS1_ENGINE_MESSAGE_SITE_TILE_UPSERT);
    assert(protection_second_tiles.empty());

    set_tile_protection_visual_state(site_run, density_coord, 18.0f, 36.0f, 52.0f);
    gs1::GameRuntimeProjectionTestAccess::mark_tile_dirty(runtime, density_coord);
    gs1::GameRuntimeProjectionTestAccess::flush_projection(runtime);
    const auto protection_third_messages = drain_engine_messages(runtime);
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
    gs1::GameRuntimeProjectionTestAccess::mark_tile_dirty(runtime, density_coord);
    gs1::GameRuntimeProjectionTestAccess::flush_projection(runtime);
    const auto soil_first_messages = drain_engine_messages(runtime);
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
    gs1::GameRuntimeProjectionTestAccess::mark_tile_dirty(runtime, density_coord);
    gs1::GameRuntimeProjectionTestAccess::flush_projection(runtime);
    const auto soil_second_messages = drain_engine_messages(runtime);
    const auto soil_second_tiles =
        collect_messages_of_type(soil_second_messages, GS1_ENGINE_MESSAGE_SITE_TILE_UPSERT);
    assert(soil_second_tiles.empty());

    set_tile_soil_visual_state(site_run, density_coord, 0.27f, 0.14f, 0.4f);
    gs1::GameRuntimeProjectionTestAccess::mark_tile_dirty(runtime, density_coord);
    gs1::GameRuntimeProjectionTestAccess::flush_projection(runtime);
    const auto soil_third_messages = drain_engine_messages(runtime);
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
    gs1::GameRuntimeProjectionTestAccess::mark_tile_dirty(runtime, density_coord);
    gs1::GameRuntimeProjectionTestAccess::flush_projection(runtime);
    const auto excavation_first_messages = drain_engine_messages(runtime);
    const auto excavation_first_tiles =
        collect_messages_of_type(excavation_first_messages, GS1_ENGINE_MESSAGE_SITE_TILE_UPSERT);
    assert(excavation_first_tiles.size() == 1U);
    {
        const auto& payload = excavation_first_tiles.front()->payload_as<Gs1EngineMessageSiteTileData>();
        assert(payload.excavation_depth == static_cast<std::uint8_t>(gs1::ExcavationDepth::Rough));
        assert(payload.visible_excavation_depth == static_cast<std::uint8_t>(gs1::ExcavationDepth::Rough));
    }

    set_tile_plant_state(site_run, density_coord, gs1::PlantId {gs1::k_plant_straw_checkerboard}, 0.22f);
    gs1::GameRuntimeProjectionTestAccess::mark_tile_dirty(runtime, density_coord);
    gs1::GameRuntimeProjectionTestAccess::flush_projection(runtime);
    const auto excavation_hidden_messages = drain_engine_messages(runtime);
    const auto excavation_hidden_tiles =
        collect_messages_of_type(excavation_hidden_messages, GS1_ENGINE_MESSAGE_SITE_TILE_UPSERT);
    assert(excavation_hidden_tiles.size() == 1U);
    {
        const auto& payload = excavation_hidden_tiles.front()->payload_as<Gs1EngineMessageSiteTileData>();
        assert(payload.excavation_depth == static_cast<std::uint8_t>(gs1::ExcavationDepth::Rough));
        assert(payload.visible_excavation_depth == static_cast<std::uint8_t>(gs1::ExcavationDepth::None));
    }

    set_tile_sand_burial(site_run, TileCoord {0, 0}, 0.9f);
    gs1::GameRuntimeProjectionTestAccess::mark_all_tiles_dirty(runtime);
    gs1::GameRuntimeProjectionTestAccess::flush_projection(runtime);
    const auto fallback_messages = drain_engine_messages(runtime);
    const auto fallback_tiles = collect_messages_of_type(fallback_messages, GS1_ENGINE_MESSAGE_SITE_TILE_UPSERT);
    assert(!fallback_tiles.empty());
    assert(fallback_tiles.size() == gs1::site_world_access::tile_count(site_run));

    Gs1RuntimeCreateDesc phone_panel_desc {};
    phone_panel_desc.struct_size = sizeof(Gs1RuntimeCreateDesc);
    phone_panel_desc.api_version = gs1::k_api_version;
    phone_panel_desc.fixed_step_seconds = 1.0 / 60.0;

    GameRuntime phone_panel_runtime {phone_panel_desc};
    assert(phone_panel_runtime.handle_message(make_start_campaign_message()) == GS1_STATUS_OK);
    const auto phone_panel_site_id =
        gs1::GameRuntimeProjectionTestAccess::campaign(phone_panel_runtime)->sites.front().site_id.value;
    assert(phone_panel_runtime.handle_message(make_start_site_attempt_message(phone_panel_site_id)) == GS1_STATUS_OK);
    assert(gs1::GameRuntimeProjectionTestAccess::active_site_run(phone_panel_runtime).has_value());
    auto& phone_panel_site_run =
        gs1::GameRuntimeProjectionTestAccess::active_site_run(phone_panel_runtime).value();
    drain_engine_messages(phone_panel_runtime);

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
    drain_engine_messages(phone_panel_runtime);

    Gs1Phase1Result phone_panel_delivery_result {};
    run_phase1(phone_panel_runtime, k_default_delivery_real_seconds, phone_panel_delivery_result);
    const auto delivery_quantity_after = gs1::inventory_storage::available_item_quantity_in_container(
        phone_panel_site_run,
        delivery_box_container,
        delivered_item_id);
    assert(delivery_quantity_after == delivery_quantity_before + 1U);
    gs1::GameRuntimeProjectionTestAccess::flush_projection(phone_panel_runtime);
    const auto phone_panel_delivery_messages = drain_engine_messages(phone_panel_runtime);
    assert(contains_phone_listing_message(
        phone_panel_delivery_messages,
        GS1_ENGINE_MESSAGE_SITE_PHONE_LISTING_UPSERT,
        expected_sell_listing_id));
    assert(!collect_messages_of_type(
        phone_panel_delivery_messages,
        GS1_ENGINE_MESSAGE_SITE_PHONE_PANEL_STATE).empty());

    Gs1UiAction open_tasks_action {};
    open_tasks_action.type = GS1_UI_ACTION_SET_PHONE_PANEL_SECTION;
    open_tasks_action.arg0 = GS1_PHONE_PANEL_SECTION_TASKS;
    const auto open_tasks_event = make_ui_action_event(open_tasks_action);
    Gs1Phase1Result open_tasks_result {};
    assert(phone_panel_runtime.submit_host_events(&open_tasks_event, 1U) == GS1_STATUS_OK);
    run_phase1(phone_panel_runtime, 0.0, open_tasks_result);
    assert(open_tasks_result.processed_host_event_count == 1U);
    const auto open_tasks_messages = drain_engine_messages(phone_panel_runtime);
    const auto open_tasks_phone_panel_messages =
        collect_messages_of_type(open_tasks_messages, GS1_ENGINE_MESSAGE_SITE_PHONE_PANEL_STATE);
    assert(open_tasks_phone_panel_messages.size() == 1U);
    {
        const auto& payload =
            open_tasks_phone_panel_messages.front()->payload_as<Gs1EngineMessagePhonePanelData>();
        assert(payload.active_section == GS1_PHONE_PANEL_SECTION_TASKS);
    }

    Gs1RuntimeCreateDesc panel_state_desc {};
    panel_state_desc.struct_size = sizeof(Gs1RuntimeCreateDesc);
    panel_state_desc.api_version = gs1::k_api_version;
    panel_state_desc.fixed_step_seconds = 1.0 / 60.0;

    GameRuntime panel_state_runtime {panel_state_desc};
    run_phase1(panel_state_runtime, 0.0);
    drain_engine_messages(panel_state_runtime);
    assert(panel_state_runtime.handle_message(make_start_campaign_message()) == GS1_STATUS_OK);
    const auto panel_state_site_id =
        gs1::GameRuntimeProjectionTestAccess::campaign(panel_state_runtime)->sites.front().site_id.value;
    assert(panel_state_runtime.handle_message(make_start_site_attempt_message(panel_state_site_id)) == GS1_STATUS_OK);
    auto& panel_state_site_run =
        gs1::GameRuntimeProjectionTestAccess::active_site_run(panel_state_runtime).value();
    drain_engine_messages(panel_state_runtime);

    auto open_panel_storage_event = make_storage_view_event(
        starter_storage_id(panel_state_site_run),
        GS1_INVENTORY_VIEW_EVENT_OPEN_SNAPSHOT);
    Gs1Phase1Result open_panel_storage_result {};
    assert(panel_state_runtime.submit_host_events(&open_panel_storage_event, 1U) == GS1_STATUS_OK);
    run_phase1(panel_state_runtime, 0.0, open_panel_storage_result);
    assert(open_panel_storage_result.processed_host_event_count == 1U);
    assert(panel_state_site_run.inventory.opened_device_storage_id == starter_storage_id(panel_state_site_run));
    const auto open_panel_storage_messages = drain_engine_messages(panel_state_runtime);
    assert(find_inventory_view_message(
               open_panel_storage_messages,
               starter_storage_id(panel_state_site_run),
               GS1_INVENTORY_VIEW_EVENT_OPEN_SNAPSHOT) != nullptr);

    auto open_worker_pack_event = make_storage_view_event(
        panel_state_site_run.inventory.worker_pack_storage_id,
        GS1_INVENTORY_VIEW_EVENT_OPEN_SNAPSHOT);
    Gs1Phase1Result open_worker_pack_result {};
    assert(panel_state_runtime.submit_host_events(&open_worker_pack_event, 1U) == GS1_STATUS_OK);
    run_phase1(panel_state_runtime, 0.0, open_worker_pack_result);
    assert(open_worker_pack_result.processed_host_event_count == 1U);
    assert(panel_state_site_run.inventory.worker_pack_panel_open);
    assert(panel_state_site_run.inventory.opened_device_storage_id == starter_storage_id(panel_state_site_run));
    const auto open_worker_pack_messages = drain_engine_messages(panel_state_runtime);
    assert(find_inventory_view_message(
               open_worker_pack_messages,
               panel_state_site_run.inventory.worker_pack_storage_id,
               GS1_INVENTORY_VIEW_EVENT_OPEN_SNAPSHOT) != nullptr);
    assert(find_inventory_view_message(
               open_worker_pack_messages,
               starter_storage_id(panel_state_site_run),
               GS1_INVENTORY_VIEW_EVENT_OPEN_SNAPSHOT) == nullptr);

    Gs1UiAction open_phone_action {};
    open_phone_action.type = GS1_UI_ACTION_SET_PHONE_PANEL_SECTION;
    open_phone_action.arg0 = GS1_PHONE_PANEL_SECTION_BUY;
    const auto open_phone_event = make_ui_action_event(open_phone_action);
    Gs1Phase1Result open_phone_result {};
    assert(panel_state_runtime.submit_host_events(&open_phone_event, 1U) == GS1_STATUS_OK);
    run_phase1(panel_state_runtime, 0.0, open_phone_result);
    assert(open_phone_result.processed_host_event_count == 1U);
    assert(panel_state_site_run.phone_panel.open);
    assert(!panel_state_site_run.inventory.worker_pack_panel_open);
    assert(panel_state_site_run.inventory.opened_device_storage_id == 0U);
    const auto open_phone_messages = drain_engine_messages(panel_state_runtime);
    assert(find_inventory_view_message(
               open_phone_messages,
               panel_state_site_run.inventory.worker_pack_storage_id,
               GS1_INVENTORY_VIEW_EVENT_CLOSE) != nullptr);
    assert(find_inventory_view_message(
               open_phone_messages,
               starter_storage_id(panel_state_site_run),
               GS1_INVENTORY_VIEW_EVENT_CLOSE) != nullptr);
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
    assert(panel_state_runtime.submit_host_events(&open_site_tech_tree_event, 1U) == GS1_STATUS_OK);
    run_phase1(panel_state_runtime, 0.0, open_site_tech_tree_result);
    assert(open_site_tech_tree_result.processed_host_event_count == 1U);
    const auto open_site_tech_tree_messages = drain_engine_messages(panel_state_runtime);
    assert(gs1::GameRuntimeProjectionTestAccess::campaign(panel_state_runtime)->regional_map_state.tech_tree_open);
    assert(!panel_state_site_run.phone_panel.open);
    const auto tech_tree_phone_panel_messages =
        collect_messages_of_type(open_site_tech_tree_messages, GS1_ENGINE_MESSAGE_SITE_PHONE_PANEL_STATE);
    assert(!tech_tree_phone_panel_messages.empty());
    {
        const auto& payload =
            tech_tree_phone_panel_messages.back()->payload_as<Gs1EngineMessagePhonePanelData>();
        assert((payload.flags & GS1_PHONE_PANEL_FLAG_OPEN) == 0U);
    }
    assert(contains_ui_element_text(open_site_tech_tree_messages, "Prototype Tech Tree"));

    assert(panel_state_runtime.submit_host_events(&open_worker_pack_event, 1U) == GS1_STATUS_OK);
    Gs1Phase1Result reopen_worker_pack_result {};
    run_phase1(panel_state_runtime, 0.0, reopen_worker_pack_result);
    assert(reopen_worker_pack_result.processed_host_event_count == 1U);
    assert(panel_state_site_run.inventory.worker_pack_panel_open);
    assert(!gs1::GameRuntimeProjectionTestAccess::campaign(panel_state_runtime)->regional_map_state.tech_tree_open);
    const auto reopen_worker_pack_messages = drain_engine_messages(panel_state_runtime);
    assert(find_close_ui_setup_message(
               reopen_worker_pack_messages,
               GS1_UI_SETUP_REGIONAL_MAP_TECH_TREE) != nullptr);
    assert(find_inventory_view_message(
               reopen_worker_pack_messages,
               panel_state_site_run.inventory.worker_pack_storage_id,
               GS1_INVENTORY_VIEW_EVENT_OPEN_SNAPSHOT) != nullptr);

    Gs1UiAction open_protection_selector_action {};
    open_protection_selector_action.type = GS1_UI_ACTION_OPEN_SITE_PROTECTION_SELECTOR;
    const auto open_protection_selector_event = make_ui_action_event(open_protection_selector_action);
    Gs1Phase1Result open_protection_selector_result {};
    assert(panel_state_runtime.submit_host_events(&open_protection_selector_event, 1U) == GS1_STATUS_OK);
    run_phase1(panel_state_runtime, 0.0, open_protection_selector_result);
    assert(open_protection_selector_result.processed_host_event_count == 1U);
    assert(gs1::GameRuntimeProjectionTestAccess::site_protection_selector_open(panel_state_runtime));
    assert(
        gs1::GameRuntimeProjectionTestAccess::site_protection_overlay_mode(panel_state_runtime) ==
        GS1_SITE_PROTECTION_OVERLAY_NONE);
    const auto open_protection_selector_messages = drain_engine_messages(panel_state_runtime);
    assert(find_inventory_view_message(
               open_protection_selector_messages,
               panel_state_site_run.inventory.worker_pack_storage_id,
               GS1_INVENTORY_VIEW_EVENT_CLOSE) != nullptr);
    assert(contains_ui_element_text(open_protection_selector_messages, "Protection Overlay"));
    assert(contains_ui_element_text(open_protection_selector_messages, "Wind Protection"));
    assert(contains_ui_element_text(open_protection_selector_messages, "Heat Protection"));
    assert(contains_ui_element_text(open_protection_selector_messages, "Dust Protection"));

    Gs1UiAction select_heat_overlay_action {};
    select_heat_overlay_action.type = GS1_UI_ACTION_SET_SITE_PROTECTION_OVERLAY_MODE;
    select_heat_overlay_action.arg0 = GS1_SITE_PROTECTION_OVERLAY_HEAT;
    const auto select_heat_overlay_event = make_ui_action_event(select_heat_overlay_action);
    Gs1Phase1Result select_heat_overlay_result {};
    assert(panel_state_runtime.submit_host_events(&select_heat_overlay_event, 1U) == GS1_STATUS_OK);
    run_phase1(panel_state_runtime, 0.0, select_heat_overlay_result);
    assert(select_heat_overlay_result.processed_host_event_count == 1U);
    assert(!gs1::GameRuntimeProjectionTestAccess::site_protection_selector_open(panel_state_runtime));
    assert(
        gs1::GameRuntimeProjectionTestAccess::site_protection_overlay_mode(panel_state_runtime) ==
        GS1_SITE_PROTECTION_OVERLAY_HEAT);
    const auto select_heat_overlay_messages = drain_engine_messages(panel_state_runtime);
    assert(find_close_ui_setup_message(
               select_heat_overlay_messages,
               GS1_UI_SETUP_SITE_PROTECTION_SELECTOR) != nullptr);
    const auto protection_overlay_messages =
        collect_messages_of_type(select_heat_overlay_messages, GS1_ENGINE_MESSAGE_SITE_PROTECTION_OVERLAY_STATE);
    assert(protection_overlay_messages.size() == 1U);
    {
        const auto& payload =
            protection_overlay_messages.front()->payload_as<Gs1EngineMessageSiteProtectionOverlayData>();
        assert(payload.mode == GS1_SITE_PROTECTION_OVERLAY_HEAT);
    }

    assert(panel_state_runtime.submit_host_events(&open_phone_event, 1U) == GS1_STATUS_OK);
    Gs1Phase1Result reopen_phone_result {};
    run_phase1(panel_state_runtime, 0.0, reopen_phone_result);
    assert(reopen_phone_result.processed_host_event_count == 1U);
    const auto reopen_phone_messages = drain_engine_messages(panel_state_runtime);
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
    ui_desc.fixed_step_seconds = 1.0 / 60.0;

    GameRuntime ui_runtime {ui_desc};
    assert(ui_runtime.handle_message(make_start_campaign_message()) == GS1_STATUS_OK);
    const auto ui_site_id =
        gs1::GameRuntimeProjectionTestAccess::campaign(ui_runtime)->sites.front().site_id.value;
    assert(ui_runtime.handle_message(make_start_site_attempt_message(ui_site_id)) == GS1_STATUS_OK);
    auto& ui_site_run = gs1::GameRuntimeProjectionTestAccess::active_site_run(ui_runtime).value();
    drain_engine_messages(ui_runtime);

    auto open_storage_event = make_storage_view_event(
        starter_storage_id(ui_site_run),
        GS1_INVENTORY_VIEW_EVENT_OPEN_SNAPSHOT);
    Gs1Phase1Result open_storage_result {};
    assert(ui_runtime.submit_host_events(&open_storage_event, 1U) == GS1_STATUS_OK);
    run_phase1(ui_runtime, 0.0, open_storage_result);
    assert(open_storage_result.processed_host_event_count == 1U);
    const auto open_storage_messages = drain_engine_messages(ui_runtime);
    assert(!collect_messages_of_type(open_storage_messages, GS1_ENGINE_MESSAGE_SITE_INVENTORY_VIEW_STATE).empty());
    assert(find_inventory_slot_message(
               open_storage_messages,
               GS1_INVENTORY_CONTAINER_DEVICE_STORAGE,
               starter_storage_id(ui_site_run),
               0U) != nullptr);

    auto ui_worker = gs1::site_world_access::worker_conditions(ui_site_run);
    ui_worker.hydration = 70.0f;
    gs1::site_world_access::set_worker_conditions(ui_site_run, ui_worker);

    Gs1UiAction transfer_action {};
    transfer_action.type = GS1_UI_ACTION_TRANSFER_INVENTORY_ITEM;
    transfer_action.arg0 = pack_inventory_transfer_arg(
        GS1_INVENTORY_CONTAINER_DEVICE_STORAGE,
        0U,
        GS1_INVENTORY_CONTAINER_WORKER_PACK,
        0U);
    transfer_action.arg1 = pack_inventory_transfer_owner_arg(
        starter_storage_id(ui_site_run),
        ui_site_run.inventory.worker_pack_storage_id);
    auto transfer_event = make_ui_action_event(transfer_action);

    Gs1Phase1Result transfer_result {};
    assert(ui_runtime.submit_host_events(&transfer_event, 1U) == GS1_STATUS_OK);
    run_phase1(ui_runtime, 0.0, transfer_result);
    assert(transfer_result.processed_host_event_count == 1U);
    assert(gs1::inventory_storage::available_item_quantity_in_container(
               ui_site_run,
               gs1::inventory_storage::starter_storage_container(ui_site_run),
               gs1::ItemId {gs1::k_item_water_container}) == 0U);
    assert(ui_site_run.inventory.worker_pack_slots[0].occupied);
    assert(ui_site_run.inventory.worker_pack_slots[0].item_id.value == gs1::k_item_water_container);
    assert(ui_site_run.inventory.worker_pack_slots[0].item_quantity == 1U);
    const auto transfer_messages = drain_engine_messages(ui_runtime);
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

    Gs1UiAction use_action {};
    use_action.type = GS1_UI_ACTION_USE_INVENTORY_ITEM;
    use_action.target_id = ui_site_run.inventory.worker_pack_slots[0].item_instance_id;
    use_action.arg0 = pack_inventory_use_arg(
        GS1_INVENTORY_CONTAINER_WORKER_PACK,
        0U,
        1U);
    use_action.arg1 = ui_site_run.inventory.worker_pack_storage_id;
    auto use_event = make_ui_action_event(use_action);

    Gs1Phase1Result use_result {};
    assert(ui_runtime.submit_host_events(&use_event, 1U) == GS1_STATUS_OK);
    run_phase1(ui_runtime, 0.0, use_result);
    assert(use_result.processed_host_event_count == 1U);
    assert(ui_site_run.inventory.worker_pack_slots[0].occupied);
    assert(ui_site_run.inventory.worker_pack_slots[0].item_quantity == 1U);
    assert(gs1::site_world_access::worker_conditions(ui_site_run).hydration == 70.0f);
    const auto use_messages = drain_engine_messages(ui_runtime);
    const auto use_inventory_messages = collect_inventory_slot_messages(use_messages);
    assert(use_inventory_messages.empty());
    assert(!collect_messages_of_type(use_messages, GS1_ENGINE_MESSAGE_SITE_ACTION_UPDATE).empty());
    assert(!collect_messages_of_type(use_messages, GS1_ENGINE_MESSAGE_SITE_WORKER_UPDATE).empty());
    assert(!collect_messages_of_type(use_messages, GS1_ENGINE_MESSAGE_HUD_STATE).empty());

    Gs1RuntimeCreateDesc water_action_desc {};
    water_action_desc.struct_size = sizeof(Gs1RuntimeCreateDesc);
    water_action_desc.api_version = gs1::k_api_version;
    water_action_desc.fixed_step_seconds = 60.0;

    GameRuntime water_action_runtime {water_action_desc};
    assert(water_action_runtime.handle_message(make_start_campaign_message()) == GS1_STATUS_OK);
    const auto water_action_site_id =
        gs1::GameRuntimeProjectionTestAccess::campaign(water_action_runtime)->sites.front().site_id.value;
    assert(water_action_runtime.handle_message(make_start_site_attempt_message(water_action_site_id)) == GS1_STATUS_OK);
    drain_engine_messages(water_action_runtime);

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
    const auto water_start_messages = drain_engine_messages(water_action_runtime);
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
    const auto water_complete_messages = drain_engine_messages(water_action_runtime);
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
    action_desc.fixed_step_seconds = 60.0;

    GameRuntime action_runtime {action_desc};
    assert(action_runtime.handle_message(make_start_campaign_message()) == GS1_STATUS_OK);
    const auto action_site_id =
        gs1::GameRuntimeProjectionTestAccess::campaign(action_runtime)->sites.front().site_id.value;
    assert(action_runtime.handle_message(make_start_site_attempt_message(action_site_id)) == GS1_STATUS_OK);
    assert(gs1::GameRuntimeProjectionTestAccess::active_site_run(action_runtime).has_value());
    drain_engine_messages(action_runtime);
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
    assert(action_runtime.submit_host_events(&action_event, 1U) == GS1_STATUS_OK);
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
    const auto action_messages = drain_engine_messages(action_runtime);
    const auto action_tile_messages =
        collect_messages_of_type(action_messages, GS1_ENGINE_MESSAGE_SITE_TILE_UPSERT);
    const auto projected_action_tile = std::find_if(
        action_tile_messages.begin(),
        action_tile_messages.end(),
        [&](const Gs1EngineMessage* message) {
            const auto& payload = message->payload_as<Gs1EngineMessageSiteTileData>();
            return payload.x == static_cast<std::uint32_t>(action_target.x) &&
                payload.y == static_cast<std::uint32_t>(action_target.y) &&
                payload.plant_type_id == gs1::k_plant_ordos_wormwood &&
                approx_equal(payload.plant_density, 100.0f);
        });
    assert(projected_action_tile != action_tile_messages.end());

    Gs1RuntimeCreateDesc placement_preview_desc {};
    placement_preview_desc.struct_size = sizeof(Gs1RuntimeCreateDesc);
    placement_preview_desc.api_version = gs1::k_api_version;
    placement_preview_desc.fixed_step_seconds = 1.0 / 60.0;

    GameRuntime placement_preview_runtime {placement_preview_desc};
    assert(placement_preview_runtime.handle_message(make_start_campaign_message()) == GS1_STATUS_OK);
    const auto placement_preview_site_id =
        gs1::GameRuntimeProjectionTestAccess::campaign(placement_preview_runtime)->sites.front().site_id.value;
    assert(placement_preview_runtime.handle_message(make_start_site_attempt_message(placement_preview_site_id)) == GS1_STATUS_OK);
    auto& placement_preview_site_run =
        gs1::GameRuntimeProjectionTestAccess::active_site_run(placement_preview_runtime).value();
    drain_engine_messages(placement_preview_runtime);

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
    assert(placement_preview_runtime.submit_host_events(&placement_mode_event, 1U) == GS1_STATUS_OK);
    run_phase1(placement_preview_runtime, 0.0);
    const auto placement_mode_messages = drain_engine_messages(placement_preview_runtime);
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
    }

    auto blocked_tile = placement_preview_site_run.site_world->tile_at(TileCoord {5, 4});
    blocked_tile.ecology.ground_cover_type_id = 9U;
    placement_preview_site_run.site_world->set_tile(TileCoord {5, 4}, blocked_tile);

    auto cursor_update_event = make_site_context_request_event(TileCoord {5, 5});
    assert(placement_preview_runtime.submit_host_events(&cursor_update_event, 1U) == GS1_STATUS_OK);
    run_phase1(placement_preview_runtime, 0.0);
    const auto cursor_update_messages = drain_engine_messages(placement_preview_runtime);
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
    }

    auto blocked_confirm_event = make_site_action_request_event(
        GS1_SITE_ACTION_PLANT,
        TileCoord {5, 5},
        0U,
        gs1::k_item_ordos_wormwood_seed_bundle,
        GS1_SITE_ACTION_REQUEST_FLAG_HAS_ITEM);
    assert(placement_preview_runtime.submit_host_events(&blocked_confirm_event, 1U) == GS1_STATUS_OK);
    run_phase1(placement_preview_runtime, 0.0);
    const auto blocked_confirm_messages = drain_engine_messages(placement_preview_runtime);
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
    assert(placement_preview_runtime.submit_host_events(&cancel_placement_event, 1U) == GS1_STATUS_OK);
    run_phase1(placement_preview_runtime, 0.0);
    const auto cancel_placement_messages = drain_engine_messages(placement_preview_runtime);
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
    storage_walk_desc.fixed_step_seconds = 0.1;

    GameRuntime storage_walk_runtime {storage_walk_desc};
    assert(storage_walk_runtime.handle_message(make_start_campaign_message()) == GS1_STATUS_OK);
    const auto storage_walk_site_id =
        gs1::GameRuntimeProjectionTestAccess::campaign(storage_walk_runtime)->sites.front().site_id.value;
    assert(storage_walk_runtime.handle_message(make_start_site_attempt_message(storage_walk_site_id)) == GS1_STATUS_OK);
    auto& storage_walk_site_run =
        gs1::GameRuntimeProjectionTestAccess::active_site_run(storage_walk_runtime).value();
    drain_engine_messages(storage_walk_runtime);

    auto worker_position = gs1::site_world_access::worker_position(storage_walk_site_run);
    worker_position.tile_coord = TileCoord {7, 7};
    worker_position.tile_x = 7.0f;
    worker_position.tile_y = 7.0f;
    gs1::site_world_access::set_worker_position(storage_walk_site_run, worker_position);

    auto distant_open_event = make_storage_view_event(
        starter_storage_id(storage_walk_site_run),
        GS1_INVENTORY_VIEW_EVENT_OPEN_SNAPSHOT);
    Gs1Phase1Result distant_open_result {};
    assert(storage_walk_runtime.submit_host_events(&distant_open_event, 1U) == GS1_STATUS_OK);
    run_phase1(storage_walk_runtime, 0.1, distant_open_result);
    assert(distant_open_result.fixed_steps_executed == 1U);
    assert(distant_open_result.processed_host_event_count == 1U);
    assert(storage_walk_site_run.inventory.opened_device_storage_id == 0U);
    assert(storage_walk_site_run.inventory.pending_device_storage_open.active);
    const auto distant_open_messages = drain_engine_messages(storage_walk_runtime);
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
    assert(storage_walk_runtime.submit_host_events(&move_cancel_event, 1U) == GS1_STATUS_OK);
    run_phase1(storage_walk_runtime, 0.1, move_cancel_result);
    assert(move_cancel_result.fixed_steps_executed == 1U);
    assert(move_cancel_result.processed_host_event_count == 1U);
    assert(!storage_walk_site_run.inventory.pending_device_storage_open.active);
    assert(storage_walk_site_run.inventory.opened_device_storage_id == 0U);
    drain_engine_messages(storage_walk_runtime);

    for (int step = 0; step < 12; ++step)
    {
        run_phase1(storage_walk_runtime, 0.1);
    }

    assert(!storage_walk_site_run.inventory.pending_device_storage_open.active);
    assert(storage_walk_site_run.inventory.opened_device_storage_id == 0U);

    Gs1RuntimeCreateDesc storage_close_desc {};
    storage_close_desc.struct_size = sizeof(Gs1RuntimeCreateDesc);
    storage_close_desc.api_version = gs1::k_api_version;
    storage_close_desc.fixed_step_seconds = 1.0;

    GameRuntime storage_close_runtime {storage_close_desc};
    assert(storage_close_runtime.handle_message(make_start_campaign_message()) == GS1_STATUS_OK);
    const auto storage_close_site_id =
        gs1::GameRuntimeProjectionTestAccess::campaign(storage_close_runtime)->sites.front().site_id.value;
    assert(storage_close_runtime.handle_message(make_start_site_attempt_message(storage_close_site_id)) == GS1_STATUS_OK);
    auto& storage_close_site_run =
        gs1::GameRuntimeProjectionTestAccess::active_site_run(storage_close_runtime).value();
    drain_engine_messages(storage_close_runtime);

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
    assert(storage_close_runtime.submit_host_events(&close_open_event, 1U) == GS1_STATUS_OK);
    run_phase1(storage_close_runtime, 0.0, close_open_result);
    assert(close_open_result.processed_host_event_count == 1U);
    assert(storage_close_site_run.inventory.opened_device_storage_id == starter_storage_id(storage_close_site_run));
    const auto close_open_messages = drain_engine_messages(storage_close_runtime);
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
    assert(move_away_result.processed_host_event_count == 0U);
    assert(storage_close_site_run.inventory.opened_device_storage_id == 0U);

    const auto close_messages = drain_engine_messages(storage_close_runtime);
    const auto close_view_messages =
        collect_messages_of_type(close_messages, GS1_ENGINE_MESSAGE_SITE_INVENTORY_VIEW_STATE);
    const auto close_view_message = std::find_if(
        close_view_messages.begin(),
        close_view_messages.end(),
        [&](const Gs1EngineMessage* message) {
            const auto& payload = message->payload_as<Gs1EngineMessageInventoryViewData>();
            return payload.storage_id == starter_storage_id(storage_close_site_run) &&
                payload.event_kind == GS1_INVENTORY_VIEW_EVENT_CLOSE;
        });
    assert(close_view_message != close_view_messages.end());

    return 0;
}
