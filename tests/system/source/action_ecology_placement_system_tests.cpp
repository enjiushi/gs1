#include "messages/game_message.h"
#include "content/defs/craft_recipe_defs.h"
#include "content/defs/item_defs.h"
#include "site/inventory_storage.h"
#include "site/site_projection_update_flags.h"
#include "site/site_world_access.h"
#include "site/systems/action_execution_system.h"
#include "site/systems/ecology_system.h"
#include "site/systems/inventory_system.h"
#include "site/systems/placement_validation_system.h"
#include "site/systems/site_flow_system.h"
#include "testing/system_test_registry.h"
#include "system_test_fixtures.h"

#include <algorithm>

namespace
{
using gs1::ActionExecutionSystem;
using gs1::ActionKind;
using gs1::EcologySystem;
using gs1::GameMessage;
using gs1::GameMessageQueue;
using gs1::GameMessageType;
using gs1::InventoryItemConsumeRequestedMessage;
using gs1::InventorySystem;
using gs1::InventoryWorkerPackInsertRequestedMessage;
using gs1::PlacementOccupancyLayer;
using gs1::PlacementReservationAcceptedMessage;
using gs1::PlacementReservationRejectedMessage;
using gs1::PlacementReservationRequestedMessage;
using gs1::PlacementReservationSubjectKind;
using gs1::PlacementModeCursorMovedMessage;
using gs1::PlacementModeCommitRejectedMessage;
using gs1::PlacementValidationSystem;
using gs1::RuntimeActionId;
using gs1::SiteActionCompletedMessage;
using gs1::SiteActionFailureReason;
using gs1::SiteActionFailedMessage;
using gs1::SiteActionStartedMessage;
using gs1::SiteGroundCoverPlacedMessage;
using gs1::SiteMoveDirectionInput;
using gs1::SiteFlowSystem;
using gs1::SiteTileBurialClearedMessage;
using gs1::SiteTileHarvestedMessage;
using gs1::SiteTilePlantingCompletedMessage;
using gs1::SiteTileWateredMessage;
using gs1::TileCoord;
using gs1::TileEcologyBatchChangedMessage;
using namespace gs1::testing::fixtures;

template <typename Payload>
GameMessage make_message(gs1::GameMessageType type, const Payload& payload)
{
    GameMessage message {};
    message.type = type;
    message.set_payload(payload);
    return message;
}

GameMessage make_start_action_message(
    Gs1SiteActionKind action_kind,
    TileCoord target_tile,
    std::uint16_t quantity = 1U,
    std::uint32_t primary_subject_id = 1U,
    std::uint32_t item_id = 0U,
    std::uint8_t flags = 0U)
{
    return make_message(
        GameMessageType::StartSiteAction,
        gs1::StartSiteActionMessage {
            action_kind,
            flags,
            quantity,
            target_tile.x,
            target_tile.y,
            primary_subject_id,
            0U,
            item_id});
}

void action_execution_rejects_invalid_or_out_of_bounds_requests(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 501U);
    GameMessageQueue queue {};
    auto site_context = make_site_context<ActionExecutionSystem>(campaign, site_run, queue);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ActionExecutionSystem::process_message(
            site_context,
            make_start_action_message(GS1_SITE_ACTION_BUILD, TileCoord {1, 1})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(context, queue.size() == 1U);
    GS1_SYSTEM_TEST_CHECK(context, queue.front().type == GameMessageType::SiteActionFailed);
    GS1_SYSTEM_TEST_CHECK(
        context,
        queue.front().payload_as<SiteActionFailedMessage>().reason == SiteActionFailureReason::InvalidState);
    GS1_SYSTEM_TEST_CHECK(context, !site_run.site_action.current_action_id.has_value());

    queue.clear();
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ActionExecutionSystem::process_message(
            site_context,
            make_start_action_message(GS1_SITE_ACTION_WATER, TileCoord {99, 99})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(context, queue.size() == 1U);
    GS1_SYSTEM_TEST_CHECK(
        context,
        queue.front().payload_as<SiteActionFailedMessage>().reason == SiteActionFailureReason::InvalidTarget);
    GS1_SYSTEM_TEST_CHECK(context, !site_run.site_action.current_action_id.has_value());
}

void action_execution_rejects_new_request_while_busy(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 502U);
    GameMessageQueue queue {};
    auto site_context = make_site_context<ActionExecutionSystem>(campaign, site_run, queue);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ActionExecutionSystem::process_message(
            site_context,
            make_start_action_message(GS1_SITE_ACTION_WATER, TileCoord {1, 1}, 1U)) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(context, site_run.site_action.current_action_id.has_value());
    GS1_SYSTEM_TEST_CHECK(context, queue.size() == 1U);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ActionExecutionSystem::process_message(
            site_context,
            make_start_action_message(GS1_SITE_ACTION_CLEAR_BURIAL, TileCoord {1, 2}, 1U)) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, queue.size() == 2U);
    GS1_SYSTEM_TEST_CHECK(
        context,
        queue.back().payload_as<SiteActionFailedMessage>().reason == SiteActionFailureReason::Busy);
}

void action_execution_water_starts_immediately_and_defers_cost_until_completion(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 503U);
    GameMessageQueue queue {};
    auto site_context = make_site_context<ActionExecutionSystem>(campaign, site_run, queue, 60.0);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ActionExecutionSystem::process_message(
            site_context,
            make_start_action_message(GS1_SITE_ACTION_WATER, TileCoord {2, 3}, 2U, 7U)) == GS1_STATUS_OK);

    GS1_SYSTEM_TEST_REQUIRE(context, site_run.site_action.current_action_id.has_value());
    GS1_SYSTEM_TEST_CHECK(context, site_run.site_action.action_kind == ActionKind::Water);
    GS1_SYSTEM_TEST_CHECK(context, !site_run.site_action.awaiting_placement_reservation);
    GS1_SYSTEM_TEST_CHECK(context, site_run.site_action.target_tile.has_value());
    GS1_SYSTEM_TEST_CHECK(context, site_run.site_action.target_tile->x == 2);
    GS1_SYSTEM_TEST_CHECK(context, site_run.site_action.target_tile->y == 3);
    GS1_SYSTEM_TEST_CHECK(context, site_run.site_action.primary_subject_id == 7U);
    GS1_SYSTEM_TEST_CHECK(context, queue.size() == 1U);
    GS1_SYSTEM_TEST_CHECK(context, queue[0].type == GameMessageType::SiteActionStarted);
    GS1_SYSTEM_TEST_CHECK(
        context,
        queue[0].payload_as<SiteActionStartedMessage>().primary_subject_id == 7U);
    GS1_SYSTEM_TEST_CHECK(
        context,
        approx_equal(
            queue[0].payload_as<SiteActionStartedMessage>().duration_minutes,
            20.0f));

    queue.clear();
    ActionExecutionSystem::run(site_context);
    GS1_SYSTEM_TEST_CHECK(context, count_messages(queue, GameMessageType::SiteActionCompleted) == 1U);
    GS1_SYSTEM_TEST_CHECK(context, count_messages(queue, GameMessageType::WorkerMeterDeltaRequested) == 1U);
    const auto* meter_message =
        first_message(queue, GameMessageType::WorkerMeterDeltaRequested);
    GS1_SYSTEM_TEST_REQUIRE(context, meter_message != nullptr);
    GS1_SYSTEM_TEST_CHECK(
        context,
        approx_equal(
            meter_message->payload_as<gs1::WorkerMeterDeltaRequestedMessage>().hydration_delta,
            -6.0f));
    GS1_SYSTEM_TEST_CHECK(
        context,
        approx_equal(
            meter_message->payload_as<gs1::WorkerMeterDeltaRequestedMessage>().nourishment_delta,
            -2.0f));
    GS1_SYSTEM_TEST_CHECK(
        context,
        approx_equal(
            meter_message->payload_as<gs1::WorkerMeterDeltaRequestedMessage>().energy_delta,
            -10.0f));
    GS1_SYSTEM_TEST_CHECK(
        context,
        approx_equal(
            meter_message->payload_as<gs1::WorkerMeterDeltaRequestedMessage>().morale_delta,
            0.0f));
}

void action_execution_weather_scales_worker_meter_costs_from_current_local_weather(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 5035U);
    GameMessageQueue queue {};
    auto site_context = make_site_context<ActionExecutionSystem>(campaign, site_run, queue, 60.0);

    const auto worker = site_run.site_world->worker();
    auto worker_tile = site_run.site_world->tile_at(worker.position.tile_coord);
    worker_tile.local_weather.heat = 100.0f;
    worker_tile.local_weather.wind = 100.0f;
    worker_tile.local_weather.dust = 100.0f;
    site_run.site_world->set_tile(worker.position.tile_coord, worker_tile);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ActionExecutionSystem::process_message(
            site_context,
            make_start_action_message(GS1_SITE_ACTION_WATER, TileCoord {2, 3}, 1U, 7U)) == GS1_STATUS_OK);

    GS1_SYSTEM_TEST_REQUIRE(context, queue.size() == 1U);
    queue.clear();
    ActionExecutionSystem::run(site_context);
    GS1_SYSTEM_TEST_REQUIRE(context, count_messages(queue, GameMessageType::WorkerMeterDeltaRequested) == 1U);
    const auto* meter_message =
        first_message(queue, GameMessageType::WorkerMeterDeltaRequested);
    GS1_SYSTEM_TEST_REQUIRE(context, meter_message != nullptr);
    GS1_SYSTEM_TEST_CHECK(
        context,
        approx_equal(
            meter_message->payload_as<gs1::WorkerMeterDeltaRequestedMessage>().hydration_delta,
            -8.1f));
    GS1_SYSTEM_TEST_CHECK(
        context,
        approx_equal(
            meter_message->payload_as<gs1::WorkerMeterDeltaRequestedMessage>().nourishment_delta,
            -1.7f));
    GS1_SYSTEM_TEST_CHECK(
        context,
        approx_equal(
            meter_message->payload_as<gs1::WorkerMeterDeltaRequestedMessage>().energy_delta,
            -13.5f));
    GS1_SYSTEM_TEST_CHECK(
        context,
        approx_equal(
            meter_message->payload_as<gs1::WorkerMeterDeltaRequestedMessage>().morale_delta,
            -10.0f));
}

void action_execution_morale_cost_reaches_full_authored_value_at_single_weather_100(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 5036U);
    GameMessageQueue queue {};
    auto site_context = make_site_context<ActionExecutionSystem>(campaign, site_run, queue, 60.0);

    const auto worker = site_run.site_world->worker();
    auto worker_tile = site_run.site_world->tile_at(worker.position.tile_coord);
    worker_tile.local_weather.heat = 100.0f;
    worker_tile.local_weather.wind = 0.0f;
    worker_tile.local_weather.dust = 0.0f;
    site_run.site_world->set_tile(worker.position.tile_coord, worker_tile);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ActionExecutionSystem::process_message(
            site_context,
            make_start_action_message(GS1_SITE_ACTION_WATER, TileCoord {2, 3}, 1U, 7U)) == GS1_STATUS_OK);

    GS1_SYSTEM_TEST_REQUIRE(context, queue.size() == 1U);
    queue.clear();
    ActionExecutionSystem::run(site_context);
    GS1_SYSTEM_TEST_REQUIRE(context, count_messages(queue, GameMessageType::WorkerMeterDeltaRequested) == 1U);
    const auto* meter_message =
        first_message(queue, GameMessageType::WorkerMeterDeltaRequested);
    GS1_SYSTEM_TEST_REQUIRE(context, meter_message != nullptr);
    GS1_SYSTEM_TEST_CHECK(
        context,
        approx_equal(
            meter_message->payload_as<gs1::WorkerMeterDeltaRequestedMessage>().morale_delta,
            -10.0f));
}

void action_execution_craft_uses_recipe_authored_duration_and_cost(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 5034U);
    GameMessageQueue queue {};
    auto site_context = make_site_context<ActionExecutionSystem>(campaign, site_run, queue, 60.0);

    (void)gs1::inventory_storage::add_item_to_container(
        site_run,
        gs1::inventory_storage::worker_pack_container(site_run),
        gs1::ItemId {gs1::k_item_wood_bundle},
        5U);
    (void)gs1::inventory_storage::add_item_to_container(
        site_run,
        gs1::inventory_storage::worker_pack_container(site_run),
        gs1::ItemId {gs1::k_item_iron_bundle},
        3U);

    const auto workbench_tile = default_starter_workbench_tile(site_run.camp.camp_anchor_tile);
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ActionExecutionSystem::process_message(
            site_context,
            make_start_action_message(
                GS1_SITE_ACTION_CRAFT,
                workbench_tile,
                1U,
                0U,
                gs1::k_item_workbench_kit)) == GS1_STATUS_OK);

    GS1_SYSTEM_TEST_REQUIRE(context, site_run.site_action.current_action_id.has_value());
    GS1_SYSTEM_TEST_CHECK(context, site_run.site_action.action_kind == ActionKind::Craft);
    GS1_SYSTEM_TEST_CHECK(context, queue.size() == 1U);
    GS1_SYSTEM_TEST_CHECK(context, queue[0].type == GameMessageType::SiteActionStarted);
    GS1_SYSTEM_TEST_CHECK(
        context,
        site_run.site_action.secondary_subject_id == gs1::k_recipe_craft_workbench);
    GS1_SYSTEM_TEST_CHECK(
        context,
        approx_equal(
            queue[0].payload_as<SiteActionStartedMessage>().duration_minutes,
            2.0f));

    queue.clear();
    ActionExecutionSystem::run(site_context);
    GS1_SYSTEM_TEST_REQUIRE(context, count_messages(queue, GameMessageType::WorkerMeterDeltaRequested) == 1U);
    const auto* meter_message =
        first_message(queue, GameMessageType::WorkerMeterDeltaRequested);
    GS1_SYSTEM_TEST_REQUIRE(context, meter_message != nullptr);
    GS1_SYSTEM_TEST_CHECK(
        context,
        approx_equal(
            meter_message->payload_as<gs1::WorkerMeterDeltaRequestedMessage>().hydration_delta,
            -0.5f));
    GS1_SYSTEM_TEST_CHECK(
        context,
        approx_equal(
            meter_message->payload_as<gs1::WorkerMeterDeltaRequestedMessage>().nourishment_delta,
            -0.2f));
    GS1_SYSTEM_TEST_CHECK(
        context,
        approx_equal(
            meter_message->payload_as<gs1::WorkerMeterDeltaRequestedMessage>().energy_delta,
            -1.5f));
}

void action_execution_progress_slows_with_lower_previous_work_efficiency(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    const TileCoord target_tile {2, 2};

    auto fast_run = make_test_site_run(1U, 5031U);
    auto medium_run = make_test_site_run(1U, 5032U);
    auto slow_run = make_test_site_run(1U, 5033U);
    GameMessageQueue fast_queue {};
    GameMessageQueue medium_queue {};
    GameMessageQueue slow_queue {};
    auto fast_context = make_site_context<ActionExecutionSystem>(campaign, fast_run, fast_queue, 1.0);
    auto medium_context = make_site_context<ActionExecutionSystem>(campaign, medium_run, medium_queue, 1.0);
    auto slow_context = make_site_context<ActionExecutionSystem>(campaign, slow_run, slow_queue, 1.0);

    auto medium_worker = gs1::site_world_access::worker_conditions(medium_run);
    medium_worker.work_efficiency = 0.7f;
    gs1::site_world_access::set_worker_conditions(medium_run, medium_worker);

    auto slow_worker = gs1::site_world_access::worker_conditions(slow_run);
    slow_worker.work_efficiency = 0.4f;
    gs1::site_world_access::set_worker_conditions(slow_run, slow_worker);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ActionExecutionSystem::process_message(
            fast_context,
            make_start_action_message(GS1_SITE_ACTION_WATER, target_tile, 2U)) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ActionExecutionSystem::process_message(
            medium_context,
            make_start_action_message(GS1_SITE_ACTION_WATER, target_tile, 2U)) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ActionExecutionSystem::process_message(
            slow_context,
            make_start_action_message(GS1_SITE_ACTION_WATER, target_tile, 2U)) == GS1_STATUS_OK);

    fast_queue.clear();
    medium_queue.clear();
    slow_queue.clear();

    ActionExecutionSystem::run(fast_context);
    ActionExecutionSystem::run(medium_context);
    ActionExecutionSystem::run(slow_context);

    GS1_SYSTEM_TEST_CHECK(
        context,
        fast_run.site_action.remaining_action_minutes <
            medium_run.site_action.remaining_action_minutes);
    GS1_SYSTEM_TEST_CHECK(
        context,
        medium_run.site_action.remaining_action_minutes <
            slow_run.site_action.remaining_action_minutes);
}

void action_execution_plant_waits_for_reservation_then_starts_on_accept(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 504U);
    GameMessageQueue queue {};
    auto site_context = make_site_context<ActionExecutionSystem>(campaign, site_run, queue);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ActionExecutionSystem::process_message(
            site_context,
            make_start_action_message(GS1_SITE_ACTION_PLANT, TileCoord {3, 3}, 1U, 12U)) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(context, site_run.site_action.current_action_id.has_value());
    const auto action_id = site_run.site_action.current_action_id->value;
    GS1_SYSTEM_TEST_CHECK(context, site_run.site_action.awaiting_placement_reservation);
    GS1_SYSTEM_TEST_REQUIRE(context, queue.size() == 1U);
    GS1_SYSTEM_TEST_CHECK(context, queue.front().type == GameMessageType::PlacementReservationRequested);

    queue.clear();
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ActionExecutionSystem::process_message(
            site_context,
            make_message(
                GameMessageType::PlacementReservationAccepted,
                PlacementReservationAcceptedMessage {action_id, 3, 3, 55U})) == GS1_STATUS_OK);

    GS1_SYSTEM_TEST_CHECK(context, !site_run.site_action.awaiting_placement_reservation);
    GS1_SYSTEM_TEST_CHECK(context, site_run.site_action.placement_reservation_token == 55U);
    GS1_SYSTEM_TEST_CHECK(context, !site_run.site_action.started_at_world_minute.has_value());
    GS1_SYSTEM_TEST_CHECK(context, site_run.site_action.approach_tile.has_value());
    GS1_SYSTEM_TEST_CHECK(context, site_run.site_action.approach_tile->x == 3);
    GS1_SYSTEM_TEST_CHECK(context, site_run.site_action.approach_tile->y == 3);
    GS1_SYSTEM_TEST_CHECK(context, queue.empty());
}

void planting_auto_move_reaches_tile_before_action_starts(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 509U);
    GameMessageQueue queue {};
    auto action_context = make_site_context<ActionExecutionSystem>(campaign, site_run, queue);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ActionExecutionSystem::process_message(
            action_context,
            make_start_action_message(GS1_SITE_ACTION_PLANT, TileCoord {4, 2}, 1U, 12U)) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(context, site_run.site_action.current_action_id.has_value());
    const auto action_id = site_run.site_action.current_action_id->value;
    GS1_SYSTEM_TEST_CHECK(context, site_run.site_action.awaiting_placement_reservation);

    queue.clear();
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ActionExecutionSystem::process_message(
            action_context,
            make_message(
                GameMessageType::PlacementReservationAccepted,
                PlacementReservationAcceptedMessage {action_id, 4, 2, 56U})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, !site_run.site_action.awaiting_placement_reservation);
    GS1_SYSTEM_TEST_CHECK(context, !site_run.site_action.started_at_world_minute.has_value());
    GS1_SYSTEM_TEST_CHECK(context, queue.empty());

    auto flow_context = make_site_context<SiteFlowSystem>(campaign, site_run, queue);
    SiteFlowSystem::run(flow_context);

    GS1_SYSTEM_TEST_REQUIRE(context, site_run.site_action.approach_tile.has_value());
    const auto expected_approach_tile = site_run.site_action.approach_tile.value();
    const auto worker = site_run.site_world->worker();
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(worker.position.tile_x, static_cast<float>(expected_approach_tile.x)));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(worker.position.tile_y, static_cast<float>(expected_approach_tile.y)));
    GS1_SYSTEM_TEST_CHECK(context, worker.position.tile_coord.x == expected_approach_tile.x);
    GS1_SYSTEM_TEST_CHECK(context, worker.position.tile_coord.y == expected_approach_tile.y);

    queue.clear();
    ActionExecutionSystem::run(action_context);
    GS1_SYSTEM_TEST_REQUIRE(context, queue.size() == 1U);
    GS1_SYSTEM_TEST_CHECK(context, queue[0].type == GameMessageType::SiteActionStarted);
    GS1_SYSTEM_TEST_CHECK(context, site_run.site_action.started_at_world_minute.has_value());
}

void action_execution_deferred_plant_enters_placement_mode_and_tracks_preview(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 5091U);
    GameMessageQueue queue {};
    auto action_context = make_site_context<ActionExecutionSystem>(campaign, site_run, queue);

    site_run.inventory.worker_pack_slots[2].occupied = true;
    site_run.inventory.worker_pack_slots[2].item_id = gs1::ItemId {gs1::k_item_ordos_wormwood_seed_bundle};
    site_run.inventory.worker_pack_slots[2].item_quantity = 2U;
    site_run.inventory.worker_pack_slots[2].item_condition = 1.0f;
    site_run.inventory.worker_pack_slots[2].item_freshness = 1.0f;

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ActionExecutionSystem::process_message(
            action_context,
            make_start_action_message(
                GS1_SITE_ACTION_PLANT,
                TileCoord {4, 4},
                1U,
                0U,
                gs1::k_item_ordos_wormwood_seed_bundle,
                GS1_SITE_ACTION_REQUEST_FLAG_HAS_ITEM |
                    GS1_SITE_ACTION_REQUEST_FLAG_DEFERRED_TARGET_SELECTION)) == GS1_STATUS_OK);

    GS1_SYSTEM_TEST_CHECK(context, !site_run.site_action.current_action_id.has_value());
    GS1_SYSTEM_TEST_CHECK(context, site_run.site_action.placement_mode.active);
    GS1_SYSTEM_TEST_CHECK(context, site_run.site_action.placement_mode.action_kind == ActionKind::Plant);
    GS1_SYSTEM_TEST_CHECK(context, site_run.site_action.placement_mode.item_id == gs1::k_item_ordos_wormwood_seed_bundle);
    GS1_SYSTEM_TEST_CHECK(context, site_run.site_action.placement_mode.target_tile.has_value());
    GS1_SYSTEM_TEST_CHECK(context, site_run.site_action.placement_mode.target_tile->x == 4);
    GS1_SYSTEM_TEST_CHECK(context, site_run.site_action.placement_mode.target_tile->y == 4);
    GS1_SYSTEM_TEST_CHECK(context, site_run.site_action.placement_mode.footprint_width == 2U);
    GS1_SYSTEM_TEST_CHECK(context, site_run.site_action.placement_mode.footprint_height == 2U);
    GS1_SYSTEM_TEST_CHECK(context, site_run.site_action.placement_mode.blocked_mask == 0ULL);
    GS1_SYSTEM_TEST_CHECK(context, queue.empty());

    auto occupied_tile = site_run.site_world->tile_at(TileCoord {5, 4});
    occupied_tile.ecology.ground_cover_type_id = 5U;
    site_run.site_world->set_tile(TileCoord {5, 4}, occupied_tile);

    queue.clear();
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ActionExecutionSystem::process_message(
            action_context,
            make_message(
                GameMessageType::PlacementModeCursorMoved,
                PlacementModeCursorMovedMessage {5, 5, 0U})) == GS1_STATUS_OK);

    GS1_SYSTEM_TEST_CHECK(context, site_run.site_action.placement_mode.target_tile.has_value());
    GS1_SYSTEM_TEST_CHECK(context, site_run.site_action.placement_mode.target_tile->x == 4);
    GS1_SYSTEM_TEST_CHECK(context, site_run.site_action.placement_mode.target_tile->y == 4);
    GS1_SYSTEM_TEST_CHECK(context, site_run.site_action.placement_mode.blocked_mask == (1ULL << 1U));
    GS1_SYSTEM_TEST_CHECK(context, queue.empty());

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ActionExecutionSystem::process_message(
            action_context,
            make_start_action_message(
                GS1_SITE_ACTION_PLANT,
                TileCoord {5, 5},
                1U,
                0U,
                gs1::k_item_ordos_wormwood_seed_bundle,
                GS1_SITE_ACTION_REQUEST_FLAG_HAS_ITEM)) == GS1_STATUS_OK);

    GS1_SYSTEM_TEST_CHECK(context, site_run.site_action.placement_mode.active);
    GS1_SYSTEM_TEST_CHECK(context, !site_run.site_action.current_action_id.has_value());
    GS1_SYSTEM_TEST_REQUIRE(context, count_messages(queue, GameMessageType::PlacementModeCommitRejected) == 1U);
    const auto* placement_rejected =
        first_message_payload<PlacementModeCommitRejectedMessage>(
            queue,
            GameMessageType::PlacementModeCommitRejected);
    GS1_SYSTEM_TEST_REQUIRE(context, placement_rejected != nullptr);
    GS1_SYSTEM_TEST_CHECK(context, placement_rejected->tile_x == 4);
    GS1_SYSTEM_TEST_CHECK(context, placement_rejected->tile_y == 4);
    GS1_SYSTEM_TEST_CHECK(context, placement_rejected->blocked_mask == (1ULL << 1U));
}

void action_execution_confirming_placement_mode_starts_real_action(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 5092U);
    GameMessageQueue queue {};
    auto action_context = make_site_context<ActionExecutionSystem>(campaign, site_run, queue);

    site_run.inventory.worker_pack_slots[3].occupied = true;
    site_run.inventory.worker_pack_slots[3].item_id = gs1::ItemId {gs1::k_item_ordos_wormwood_seed_bundle};
    site_run.inventory.worker_pack_slots[3].item_quantity = 2U;
    site_run.inventory.worker_pack_slots[3].item_condition = 1.0f;
    site_run.inventory.worker_pack_slots[3].item_freshness = 1.0f;

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ActionExecutionSystem::process_message(
            action_context,
            make_start_action_message(
                GS1_SITE_ACTION_PLANT,
                TileCoord {4, 4},
                1U,
                0U,
                gs1::k_item_ordos_wormwood_seed_bundle,
                GS1_SITE_ACTION_REQUEST_FLAG_HAS_ITEM |
                    GS1_SITE_ACTION_REQUEST_FLAG_DEFERRED_TARGET_SELECTION)) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, site_run.site_action.placement_mode.active);

    queue.clear();
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ActionExecutionSystem::process_message(
            action_context,
            make_start_action_message(
                GS1_SITE_ACTION_PLANT,
                TileCoord {5, 4},
                1U,
                0U,
                gs1::k_item_ordos_wormwood_seed_bundle,
                GS1_SITE_ACTION_REQUEST_FLAG_HAS_ITEM)) == GS1_STATUS_OK);

    GS1_SYSTEM_TEST_CHECK(context, !site_run.site_action.placement_mode.active);
    GS1_SYSTEM_TEST_REQUIRE(context, site_run.site_action.current_action_id.has_value());
    GS1_SYSTEM_TEST_CHECK(context, site_run.site_action.awaiting_placement_reservation);
    GS1_SYSTEM_TEST_CHECK(context, site_run.site_action.target_tile.has_value());
    GS1_SYSTEM_TEST_CHECK(context, site_run.site_action.target_tile->x == 4);
    GS1_SYSTEM_TEST_CHECK(context, site_run.site_action.target_tile->y == 4);
    GS1_SYSTEM_TEST_REQUIRE(context, queue.size() == 1U);
    GS1_SYSTEM_TEST_CHECK(context, queue.front().type == GameMessageType::PlacementReservationRequested);
    GS1_SYSTEM_TEST_CHECK(
        context,
        queue.front().payload_as<PlacementReservationRequestedMessage>().target_tile_x == 4);
    GS1_SYSTEM_TEST_CHECK(
        context,
        queue.front().payload_as<PlacementReservationRequestedMessage>().target_tile_y == 4);
}

void action_execution_cancel_placement_mode_clears_preview_without_failure(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 5093U);
    GameMessageQueue queue {};
    auto action_context = make_site_context<ActionExecutionSystem>(campaign, site_run, queue);

    site_run.inventory.worker_pack_slots[4].occupied = true;
    site_run.inventory.worker_pack_slots[4].item_id = gs1::ItemId {gs1::k_item_workbench_kit};
    site_run.inventory.worker_pack_slots[4].item_quantity = 1U;
    site_run.inventory.worker_pack_slots[4].item_condition = 1.0f;
    site_run.inventory.worker_pack_slots[4].item_freshness = 1.0f;

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ActionExecutionSystem::process_message(
            action_context,
            make_start_action_message(
                GS1_SITE_ACTION_BUILD,
                TileCoord {3, 3},
                1U,
                0U,
                gs1::k_item_workbench_kit,
                GS1_SITE_ACTION_REQUEST_FLAG_HAS_ITEM |
                    GS1_SITE_ACTION_REQUEST_FLAG_DEFERRED_TARGET_SELECTION)) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, site_run.site_action.placement_mode.active);

    queue.clear();
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ActionExecutionSystem::process_message(
            action_context,
            make_message(
                GameMessageType::CancelSiteAction,
                gs1::CancelSiteActionMessage {
                    0U,
                    GS1_SITE_ACTION_CANCEL_FLAG_PLACEMENT_MODE})) == GS1_STATUS_OK);

    GS1_SYSTEM_TEST_CHECK(context, !site_run.site_action.placement_mode.active);
    GS1_SYSTEM_TEST_CHECK(context, !site_run.site_action.current_action_id.has_value());
    GS1_SYSTEM_TEST_CHECK(context, queue.empty());
}

void action_execution_rejection_and_cancel_clear_action_state(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 505U);
    GameMessageQueue queue {};
    auto site_context = make_site_context<ActionExecutionSystem>(campaign, site_run, queue);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ActionExecutionSystem::process_message(
            site_context,
            make_start_action_message(GS1_SITE_ACTION_PLANT, TileCoord {4, 4})) == GS1_STATUS_OK);
    const auto action_id = site_run.site_action.current_action_id->value;

    queue.clear();
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ActionExecutionSystem::process_message(
            site_context,
            make_message(
                GameMessageType::PlacementReservationRejected,
                PlacementReservationRejectedMessage {
                    action_id,
                    4,
                    4,
                    gs1::PlacementReservationRejectionReason::Occupied,
                    {0U, 0U, 0U}})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, !site_run.site_action.current_action_id.has_value());
    GS1_SYSTEM_TEST_REQUIRE(context, queue.size() == 1U);
    GS1_SYSTEM_TEST_CHECK(context, queue.front().type == GameMessageType::SiteActionFailed);

    queue.clear();
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ActionExecutionSystem::process_message(
            site_context,
            make_start_action_message(GS1_SITE_ACTION_WATER, TileCoord {1, 1})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(context, site_run.site_action.current_action_id.has_value());
    const auto cancel_id = site_run.site_action.current_action_id->value;

    queue.clear();
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ActionExecutionSystem::process_message(
            site_context,
            make_message(
                GameMessageType::CancelSiteAction,
                gs1::CancelSiteActionMessage {cancel_id, GS1_SITE_ACTION_CANCEL_FLAG_CURRENT_ACTION})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, !site_run.site_action.current_action_id.has_value());
    GS1_SYSTEM_TEST_REQUIRE(context, queue.size() == 2U);
    GS1_SYSTEM_TEST_CHECK(context, queue[0].type == GameMessageType::PlacementReservationReleased);
    GS1_SYSTEM_TEST_CHECK(context, queue[1].type == GameMessageType::SiteActionFailed);
    GS1_SYSTEM_TEST_CHECK(context, count_messages(queue, GameMessageType::WorkerMeterDeltaRequested) == 0U);
    GS1_SYSTEM_TEST_CHECK(
        context,
        queue[1].payload_as<SiteActionFailedMessage>().reason == SiteActionFailureReason::Cancelled);
}

void action_execution_run_completes_actions_and_emits_fact_messages(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 506U);
    GameMessageQueue queue {};
    auto site_context = make_site_context<ActionExecutionSystem>(campaign, site_run, queue, 60.0);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ActionExecutionSystem::process_message(
            site_context,
            make_start_action_message(GS1_SITE_ACTION_WATER, TileCoord {2, 2}, 3U)) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, site_run.site_action.started_at_world_minute.has_value());

    queue.clear();
    for (std::uint32_t step = 0U;
         step < 16U && site_run.site_action.current_action_id.has_value();
         ++step)
    {
        ActionExecutionSystem::run(site_context);
    }
    GS1_SYSTEM_TEST_CHECK(context, count_messages(queue, GameMessageType::SiteActionCompleted) == 1U);
    GS1_SYSTEM_TEST_CHECK(context, count_messages(queue, GameMessageType::WorkerMeterDeltaRequested) == 1U);
    GS1_SYSTEM_TEST_CHECK(context, count_messages(queue, GameMessageType::SiteTileWatered) == 1U);
    GS1_SYSTEM_TEST_CHECK(context, count_messages(queue, GameMessageType::PlacementReservationReleased) == 1U);
    const auto* watered_meter =
        first_message(queue, GameMessageType::WorkerMeterDeltaRequested);
    GS1_SYSTEM_TEST_REQUIRE(context, watered_meter != nullptr);
    GS1_SYSTEM_TEST_CHECK(
        context,
        approx_equal(
            watered_meter->payload_as<gs1::WorkerMeterDeltaRequestedMessage>().hydration_delta,
            -9.0f));
    const auto* watered =
        first_message_payload<SiteTileWateredMessage>(
            queue,
            GameMessageType::SiteTileWatered);
    GS1_SYSTEM_TEST_REQUIRE(context, watered != nullptr);
    GS1_SYSTEM_TEST_CHECK(context, watered->water_amount == 3.0f);
    GS1_SYSTEM_TEST_CHECK(context, !site_run.site_action.current_action_id.has_value());

    queue.clear();
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ActionExecutionSystem::process_message(
            site_context,
            make_start_action_message(GS1_SITE_ACTION_CLEAR_BURIAL, TileCoord {1, 1}, 2U)) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, site_run.site_action.started_at_world_minute.has_value());
    queue.clear();
    for (std::uint32_t step = 0U;
         step < 16U && site_run.site_action.current_action_id.has_value();
         ++step)
    {
        ActionExecutionSystem::run(site_context);
    }
    GS1_SYSTEM_TEST_CHECK(context, count_messages(queue, GameMessageType::SiteActionCompleted) == 1U);
    GS1_SYSTEM_TEST_CHECK(context, count_messages(queue, GameMessageType::WorkerMeterDeltaRequested) == 1U);
    GS1_SYSTEM_TEST_CHECK(context, count_messages(queue, GameMessageType::SiteTileBurialCleared) == 1U);
    GS1_SYSTEM_TEST_CHECK(context, count_messages(queue, GameMessageType::PlacementReservationReleased) == 1U);
    const auto* burial_cleared =
        first_message_payload<SiteTileBurialClearedMessage>(
            queue,
            GameMessageType::SiteTileBurialCleared);
    GS1_SYSTEM_TEST_REQUIRE(context, burial_cleared != nullptr);
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(burial_cleared->cleared_amount, 70.0f));
}

void action_execution_plant_completion_emits_ground_cover_after_reservation(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 507U);
    GameMessageQueue queue {};
    auto site_context = make_site_context<ActionExecutionSystem>(campaign, site_run, queue, 60.0);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ActionExecutionSystem::process_message(
            site_context,
            make_start_action_message(GS1_SITE_ACTION_PLANT, TileCoord {2, 2}, 2U, 9U)) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(context, site_run.site_action.current_action_id.has_value());
    const auto action_id = site_run.site_action.current_action_id->value;
    GS1_SYSTEM_TEST_CHECK(context, site_run.site_action.awaiting_placement_reservation);

    queue.clear();
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ActionExecutionSystem::process_message(
            site_context,
            make_message(
                GameMessageType::PlacementReservationAccepted,
                PlacementReservationAcceptedMessage {action_id, 2, 2, 77U})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, !site_run.site_action.awaiting_placement_reservation);

    queue.clear();
    for (std::uint32_t step = 0U;
         step < 16U && site_run.site_action.current_action_id.has_value();
         ++step)
    {
        ActionExecutionSystem::run(site_context);
    }
    GS1_SYSTEM_TEST_CHECK(context, count_messages(queue, GameMessageType::SiteActionCompleted) == 1U);
    GS1_SYSTEM_TEST_CHECK(context, count_messages(queue, GameMessageType::SiteGroundCoverPlaced) == 1U);
    GS1_SYSTEM_TEST_CHECK(context, count_messages(queue, GameMessageType::PlacementReservationReleased) == 1U);
    const auto* ground_cover =
        first_message_payload<SiteGroundCoverPlacedMessage>(
            queue,
            GameMessageType::SiteGroundCoverPlaced);
    GS1_SYSTEM_TEST_REQUIRE(context, ground_cover != nullptr);
    GS1_SYSTEM_TEST_CHECK(context, ground_cover->ground_cover_type_id == 9U);
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(ground_cover->initial_density, 50.0f));
    GS1_SYSTEM_TEST_CHECK(context, !site_run.site_action.current_action_id.has_value());
}

void action_execution_harvest_completion_emits_worker_pack_insert_and_tile_harvested(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 5071U);
    GameMessageQueue queue {};
    auto site_context = make_site_context<ActionExecutionSystem>(campaign, site_run, queue, 60.0);

    for (const auto coord : {TileCoord {2, 2}, TileCoord {3, 2}, TileCoord {2, 3}, TileCoord {3, 3}})
    {
        auto tile = site_run.site_world->tile_at(coord);
        tile.ecology.plant_id = gs1::PlantId {gs1::k_plant_white_thorn};
        tile.ecology.plant_density = 85.0f;
        site_run.site_world->set_tile(coord, tile);
    }

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ActionExecutionSystem::process_message(
            site_context,
            make_start_action_message(GS1_SITE_ACTION_HARVEST, TileCoord {2, 2}, 1U, 0U, 0U)) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(context, site_run.site_action.current_action_id.has_value());
    GS1_SYSTEM_TEST_CHECK(context, site_run.site_action.action_kind == ActionKind::Harvest);
    GS1_SYSTEM_TEST_CHECK(context, site_run.site_action.primary_subject_id == gs1::k_plant_white_thorn);
    GS1_SYSTEM_TEST_CHECK(context, site_run.site_action.item_id == gs1::k_item_white_thorn_berries);
    GS1_SYSTEM_TEST_CHECK(context, site_run.site_action.started_at_world_minute.has_value());

    queue.clear();
    ActionExecutionSystem::run(site_context);
    GS1_SYSTEM_TEST_CHECK(context, count_messages(queue, GameMessageType::SiteActionCompleted) == 1U);
    GS1_SYSTEM_TEST_CHECK(context, count_messages(queue, GameMessageType::InventoryWorkerPackInsertRequested) == 1U);
    GS1_SYSTEM_TEST_CHECK(context, count_messages(queue, GameMessageType::SiteTileHarvested) == 1U);
    GS1_SYSTEM_TEST_CHECK(context, count_messages(queue, GameMessageType::PlacementReservationReleased) == 1U);

    const auto* insert =
        first_message_payload<InventoryWorkerPackInsertRequestedMessage>(
            queue,
            GameMessageType::InventoryWorkerPackInsertRequested);
    GS1_SYSTEM_TEST_REQUIRE(context, insert != nullptr);
    GS1_SYSTEM_TEST_CHECK(context, insert->item_id == gs1::k_item_white_thorn_berries);
    GS1_SYSTEM_TEST_CHECK(context, insert->quantity == 1U);

    const auto* harvested =
        first_message_payload<SiteTileHarvestedMessage>(
            queue,
            GameMessageType::SiteTileHarvested);
    GS1_SYSTEM_TEST_REQUIRE(context, harvested != nullptr);
    GS1_SYSTEM_TEST_CHECK(context, harvested->plant_type_id == gs1::k_plant_white_thorn);
    GS1_SYSTEM_TEST_CHECK(context, harvested->item_id == gs1::k_item_white_thorn_berries);
    GS1_SYSTEM_TEST_CHECK(context, harvested->item_quantity == 1U);
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(harvested->density_removed, 25.0f));
}

void harvest_flow_inserts_output_and_reduces_multitile_density(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 5072U);
    GameMessageQueue queue {};
    auto action_context = make_site_context<ActionExecutionSystem>(campaign, site_run, queue, 60.0);
    auto inventory_context = make_site_context<InventorySystem>(campaign, site_run, queue, 60.0);
    auto ecology_context = make_site_context<EcologySystem>(campaign, site_run, queue, 60.0);

    for (const auto coord : {TileCoord {2, 2}, TileCoord {3, 2}, TileCoord {2, 3}, TileCoord {3, 3}})
    {
        auto tile = site_run.site_world->tile_at(coord);
        tile.ecology.plant_id = gs1::PlantId {gs1::k_plant_white_thorn};
        tile.ecology.plant_density = 85.0f;
        site_run.site_world->set_tile(coord, tile);
    }

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ActionExecutionSystem::process_message(
            action_context,
            make_start_action_message(GS1_SITE_ACTION_HARVEST, TileCoord {2, 2}, 1U, 0U, 0U)) == GS1_STATUS_OK);

    queue.clear();
    ActionExecutionSystem::run(action_context);

    while (!queue.empty())
    {
        const auto message = queue.front();
        queue.pop_front();
        GS1_SYSTEM_TEST_CHECK(
            context,
            InventorySystem::process_message(inventory_context, message) == GS1_STATUS_OK);
        GS1_SYSTEM_TEST_CHECK(
            context,
            EcologySystem::process_message(ecology_context, message) == GS1_STATUS_OK);
    }

    GS1_SYSTEM_TEST_CHECK(
        context,
        gs1::inventory_storage::available_item_quantity_in_container(
            site_run,
            gs1::inventory_storage::worker_pack_container(site_run),
            gs1::ItemId {gs1::k_item_white_thorn_berries}) == 1U);

    for (const auto coord : {TileCoord {2, 2}, TileCoord {3, 2}, TileCoord {2, 3}, TileCoord {3, 3}})
    {
        const auto tile = site_run.site_world->tile_at(coord);
        GS1_SYSTEM_TEST_CHECK(context, tile.ecology.plant_id.value == gs1::k_plant_white_thorn);
        GS1_SYSTEM_TEST_CHECK(context, approx_equal(tile.ecology.plant_density, 60.0f));
    }
}

void harvest_last_cut_kills_low_density_plant_without_loot(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 5073U);
    GameMessageQueue queue {};
    auto action_context = make_site_context<ActionExecutionSystem>(campaign, site_run, queue, 60.0);
    auto inventory_context = make_site_context<InventorySystem>(campaign, site_run, queue, 60.0);
    auto ecology_context = make_site_context<EcologySystem>(campaign, site_run, queue, 60.0);

    for (const auto coord : {TileCoord {2, 2}, TileCoord {3, 2}, TileCoord {2, 3}, TileCoord {3, 3}})
    {
        auto tile = site_run.site_world->tile_at(coord);
        tile.ecology.plant_id = gs1::PlantId {gs1::k_plant_ordos_wormwood};
        tile.ecology.plant_density = 25.0f;
        site_run.site_world->set_tile(coord, tile);
    }

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ActionExecutionSystem::process_message(
            action_context,
            make_start_action_message(GS1_SITE_ACTION_HARVEST, TileCoord {2, 2}, 1U, 0U, 0U)) == GS1_STATUS_OK);

    queue.clear();
    ActionExecutionSystem::run(action_context);
    GS1_SYSTEM_TEST_CHECK(context, count_messages(queue, GameMessageType::InventoryWorkerPackInsertRequested) == 0U);
    GS1_SYSTEM_TEST_CHECK(context, count_messages(queue, GameMessageType::SiteTileHarvested) == 1U);

    const auto* harvested =
        first_message_payload<SiteTileHarvestedMessage>(
            queue,
            GameMessageType::SiteTileHarvested);
    GS1_SYSTEM_TEST_REQUIRE(context, harvested != nullptr);
    GS1_SYSTEM_TEST_CHECK(context, harvested->item_quantity == 0U);

    while (!queue.empty())
    {
        const auto message = queue.front();
        queue.pop_front();
        GS1_SYSTEM_TEST_CHECK(
            context,
            InventorySystem::process_message(inventory_context, message) == GS1_STATUS_OK);
        GS1_SYSTEM_TEST_CHECK(
            context,
            EcologySystem::process_message(ecology_context, message) == GS1_STATUS_OK);
    }

    GS1_SYSTEM_TEST_CHECK(
        context,
        gs1::inventory_storage::available_item_quantity_in_container(
            site_run,
            gs1::inventory_storage::worker_pack_container(site_run),
            gs1::ItemId {gs1::k_item_wormwood_bundle}) == 0U);

    for (const auto coord : {TileCoord {2, 2}, TileCoord {3, 2}, TileCoord {2, 3}, TileCoord {3, 3}})
    {
        const auto tile = site_run.site_world->tile_at(coord);
        GS1_SYSTEM_TEST_CHECK(context, tile.ecology.plant_id.value == 0U);
        GS1_SYSTEM_TEST_CHECK(context, approx_equal(tile.ecology.plant_density, 0.0f));
    }
}

void action_execution_excavate_starts_immediately_and_emits_cost(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(
        1U,
        5074U,
        101U,
        8U,
        8U,
        TileCoord {2, 2},
        TileCoord {2, 2},
        100.0f,
        16U);
    GameMessageQueue queue {};
    auto site_context = make_site_context<ActionExecutionSystem>(campaign, site_run, queue);
    auto worker = site_run.site_world->worker();
    worker.position.tile_coord = TileCoord {3, 3};
    worker.position.tile_x = 3.0f;
    worker.position.tile_y = 3.0f;
    site_run.site_world->set_worker(worker);
    constexpr TileCoord k_excavate_tile {3, 3};

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ActionExecutionSystem::process_message(
            site_context,
            make_start_action_message(GS1_SITE_ACTION_EXCAVATE, k_excavate_tile, 1U, 0U, 0U)) == GS1_STATUS_OK);

    GS1_SYSTEM_TEST_REQUIRE(context, site_run.site_action.current_action_id.has_value());
    GS1_SYSTEM_TEST_CHECK(context, site_run.site_action.action_kind == ActionKind::Excavate);
    GS1_SYSTEM_TEST_CHECK(
        context,
        site_run.site_action.primary_subject_id == static_cast<std::uint32_t>(gs1::ExcavationDepth::Rough));
    GS1_SYSTEM_TEST_REQUIRE(context, queue.size() == 1U);
    GS1_SYSTEM_TEST_CHECK(context, queue[0].type == GameMessageType::SiteActionStarted);
    GS1_SYSTEM_TEST_CHECK(
        context,
        approx_equal(
            queue[0].payload_as<SiteActionStartedMessage>().duration_minutes,
            20.0f));
    GS1_SYSTEM_TEST_CHECK(context, count_messages(queue, GameMessageType::WorkerMeterDeltaRequested) == 0U);
    GS1_SYSTEM_TEST_CHECK(
        context,
        approx_equal(
            site_run.site_action.deferred_meter_delta.energy_delta,
            -30.0f));
    GS1_SYSTEM_TEST_CHECK(context, site_run.site_action.deferred_meter_delta.flags != 0U);
}

void action_execution_excavate_rejects_occupied_tiles(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(
        1U,
        5075U,
        101U,
        8U,
        8U,
        TileCoord {2, 2},
        TileCoord {2, 2},
        100.0f,
        16U);
    GameMessageQueue queue {};
    auto site_context = make_site_context<ActionExecutionSystem>(campaign, site_run, queue);
    auto worker = site_run.site_world->worker();
    worker.position.tile_coord = TileCoord {3, 3};
    worker.position.tile_x = 3.0f;
    worker.position.tile_y = 3.0f;
    site_run.site_world->set_worker(worker);

    auto tile = site_run.site_world->tile_at(TileCoord {3, 3});
    tile.device.structure_id = gs1::StructureId {gs1::k_structure_workbench};
    site_run.site_world->set_tile(TileCoord {3, 3}, tile);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ActionExecutionSystem::process_message(
            site_context,
            make_start_action_message(GS1_SITE_ACTION_EXCAVATE, TileCoord {3, 3}, 1U, 0U, 0U)) == GS1_STATUS_OK);

    GS1_SYSTEM_TEST_REQUIRE(context, queue.size() == 1U);
    GS1_SYSTEM_TEST_CHECK(context, queue.front().type == GameMessageType::SiteActionFailed);
    GS1_SYSTEM_TEST_CHECK(
        context,
        queue.front().payload_as<SiteActionFailedMessage>().reason == SiteActionFailureReason::InvalidState);
}

void action_execution_excavate_rejects_when_worker_pack_has_no_reward_space(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 5076U);
    GameMessageQueue queue {};
    auto site_context = make_site_context<ActionExecutionSystem>(campaign, site_run, queue);
    auto worker = site_run.site_world->worker();
    worker.position.tile_coord = TileCoord {3, 3};
    worker.position.tile_x = 3.0f;
    worker.position.tile_y = 3.0f;
    site_run.site_world->set_worker(worker);

    const auto worker_pack = gs1::inventory_storage::worker_pack_container(site_run);
    GS1_SYSTEM_TEST_REQUIRE(context, worker_pack.is_valid());
    for (const auto item_id : {
             gs1::k_item_water_container,
             gs1::k_item_medicine_pack,
             gs1::k_item_ordos_wormwood_seed_bundle,
             gs1::k_item_wood_bundle,
             gs1::k_item_iron_bundle,
             gs1::k_item_basic_straw_checkerboard,
             gs1::k_item_storage_crate_kit,
             gs1::k_item_workbench_kit})
    {
        GS1_SYSTEM_TEST_REQUIRE(
            context,
            gs1::inventory_storage::add_item_to_container(site_run, worker_pack, gs1::ItemId {item_id}, 1U) == 0U);
    }

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        !gs1::inventory_storage::can_fit_item_in_container(
            site_run,
            worker_pack,
            gs1::ItemId {gs1::k_item_gobi_agate},
            1U));
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ActionExecutionSystem::process_message(
            site_context,
            make_start_action_message(GS1_SITE_ACTION_EXCAVATE, TileCoord {3, 3}, 1U, 0U, 0U)) == GS1_STATUS_OK);

    GS1_SYSTEM_TEST_REQUIRE(context, queue.size() == 1U);
    GS1_SYSTEM_TEST_CHECK(context, queue.front().type == GameMessageType::SiteActionFailed);
    GS1_SYSTEM_TEST_CHECK(
        context,
        queue.front().payload_as<SiteActionFailedMessage>().reason == SiteActionFailureReason::InvalidState);
    GS1_SYSTEM_TEST_CHECK(context, !site_run.site_action.current_action_id.has_value());
}

void action_execution_excavate_completion_misses_or_finds_deterministic_loot(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(
        1U,
        1U,
        101U,
        8U,
        8U,
        TileCoord {2, 2},
        TileCoord {2, 2},
        100.0f,
        16U);
    GameMessageQueue queue {};
    auto action_context = make_site_context<ActionExecutionSystem>(campaign, site_run, queue, 60.0);
    auto inventory_context = make_site_context<InventorySystem>(campaign, site_run, queue, 60.0);
    auto worker = site_run.site_world->worker();
    worker.position.tile_coord = TileCoord {1, 1};
    worker.position.tile_x = 1.0f;
    worker.position.tile_y = 1.0f;
    site_run.site_world->set_worker(worker);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ActionExecutionSystem::process_message(
            action_context,
            make_start_action_message(GS1_SITE_ACTION_EXCAVATE, TileCoord {1, 1}, 1U, 0U, 0U)) == GS1_STATUS_OK);
    {
        auto worker = gs1::site_world_access::worker_position(site_run);
        worker.tile_coord = TileCoord {1, 1};
        worker.tile_x = 1.0f;
        worker.tile_y = 1.0f;
        gs1::site_world_access::set_worker_position(site_run, worker);
    }
    queue.clear();
    ActionExecutionSystem::run(action_context);
    GS1_SYSTEM_TEST_CHECK(context, count_messages(queue, GameMessageType::InventoryWorkerPackInsertRequested) == 0U);
    GS1_SYSTEM_TEST_CHECK(
        context,
        site_run.site_world->tile_excavation(TileCoord {1, 1}).depth == gs1::ExcavationDepth::Rough);

    while (!queue.empty())
    {
        const auto message = queue.front();
        queue.pop_front();
        GS1_SYSTEM_TEST_CHECK(
            context,
            InventorySystem::process_message(inventory_context, message) == GS1_STATUS_OK);
    }

    worker = site_run.site_world->worker();
    worker.position.tile_coord = TileCoord {3, 3};
    worker.position.tile_x = 3.0f;
    worker.position.tile_y = 3.0f;
    site_run.site_world->set_worker(worker);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ActionExecutionSystem::process_message(
            action_context,
            make_start_action_message(GS1_SITE_ACTION_EXCAVATE, TileCoord {3, 3}, 1U, 0U, 0U)) == GS1_STATUS_OK);
    {
        auto worker = gs1::site_world_access::worker_position(site_run);
        worker.tile_coord = TileCoord {3, 3};
        worker.tile_x = 3.0f;
        worker.tile_y = 3.0f;
        gs1::site_world_access::set_worker_position(site_run, worker);
    }
    queue.clear();
    ActionExecutionSystem::run(action_context);
    GS1_SYSTEM_TEST_CHECK(context, count_messages(queue, GameMessageType::InventoryWorkerPackInsertRequested) == 1U);
    const auto* insert =
        first_message_payload<InventoryWorkerPackInsertRequestedMessage>(
            queue,
            GameMessageType::InventoryWorkerPackInsertRequested);
    GS1_SYSTEM_TEST_REQUIRE(context, insert != nullptr);
    GS1_SYSTEM_TEST_CHECK(context, insert->item_id == gs1::k_item_alxa_agate);
    GS1_SYSTEM_TEST_CHECK(context, insert->quantity == 1U);
    GS1_SYSTEM_TEST_CHECK(
        context,
        site_run.site_world->tile_excavation(TileCoord {3, 3}).depth == gs1::ExcavationDepth::Rough);

    while (!queue.empty())
    {
        const auto message = queue.front();
        queue.pop_front();
        GS1_SYSTEM_TEST_CHECK(
            context,
            InventorySystem::process_message(inventory_context, message) == GS1_STATUS_OK);
    }

    GS1_SYSTEM_TEST_CHECK(
        context,
        gs1::inventory_storage::available_item_quantity_in_container(
            site_run,
            gs1::inventory_storage::worker_pack_container(site_run),
            gs1::ItemId {gs1::k_item_alxa_agate}) == 1U);
}

void action_execution_excavate_completion_marks_tile_projection_dirty(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(
        1U,
        5077U,
        101U,
        8U,
        8U,
        TileCoord {2, 2},
        TileCoord {2, 2},
        100.0f,
        16U);
    GameMessageQueue queue {};
    auto action_context = make_site_context<ActionExecutionSystem>(campaign, site_run, queue, 60.0);
    auto flow_context = make_site_context<SiteFlowSystem>(campaign, site_run, queue, 60.0);
    auto worker = site_run.site_world->worker();
    worker.position.tile_coord = TileCoord {3, 3};
    worker.position.tile_x = 3.0f;
    worker.position.tile_y = 3.0f;
    site_run.site_world->set_worker(worker);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ActionExecutionSystem::process_message(
            action_context,
            make_start_action_message(GS1_SITE_ACTION_EXCAVATE, TileCoord {3, 3}, 1U, 0U, 0U)) == GS1_STATUS_OK);
    queue.clear();

    site_run.pending_projection_update_flags = 0U;
    site_run.pending_full_tile_projection_update = false;
    std::fill(
        site_run.pending_tile_projection_update_mask.begin(),
        site_run.pending_tile_projection_update_mask.end(),
        static_cast<std::uint8_t>(0U));
    site_run.pending_tile_projection_updates.clear();

    for (int step = 0; step < 4 && site_run.site_action.current_action_id.has_value(); ++step)
    {
        SiteFlowSystem::run(flow_context);
        ActionExecutionSystem::run(action_context);
    }

    GS1_SYSTEM_TEST_CHECK(context, !site_run.site_action.current_action_id.has_value());
    GS1_SYSTEM_TEST_CHECK(
        context,
        site_run.site_world->tile_excavation(TileCoord {3, 3}).depth == gs1::ExcavationDepth::Rough);
    GS1_SYSTEM_TEST_CHECK(
        context,
        (site_run.pending_projection_update_flags & gs1::SITE_PROJECTION_UPDATE_TILES) != 0U);
    GS1_SYSTEM_TEST_CHECK(context, !site_run.pending_full_tile_projection_update);
    GS1_SYSTEM_TEST_REQUIRE(context, site_run.pending_tile_projection_updates.size() == 1U);
    GS1_SYSTEM_TEST_CHECK(
        context,
        site_run.pending_tile_projection_updates[0].x == 3 &&
            site_run.pending_tile_projection_updates[0].y == 3);
}

void action_execution_excavate_cannot_repeat_rough_depth_by_default(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(
        1U,
        2U,
        101U,
        8U,
        8U,
        TileCoord {2, 2},
        TileCoord {2, 2},
        100.0f,
        16U);
    GameMessageQueue queue {};
    auto action_context = make_site_context<ActionExecutionSystem>(campaign, site_run, queue, 60.0);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ActionExecutionSystem::process_message(
            action_context,
            make_start_action_message(GS1_SITE_ACTION_EXCAVATE, TileCoord {2, 2}, 1U, 0U, 0U)) == GS1_STATUS_OK);
    queue.clear();
    ActionExecutionSystem::run(action_context);
    queue.clear();

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ActionExecutionSystem::process_message(
            action_context,
            make_start_action_message(GS1_SITE_ACTION_EXCAVATE, TileCoord {2, 2}, 1U, 0U, 0U)) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(context, queue.size() == 1U);
    GS1_SYSTEM_TEST_CHECK(context, queue.front().type == GameMessageType::SiteActionFailed);
    GS1_SYSTEM_TEST_CHECK(
        context,
        queue.front().payload_as<SiteActionFailedMessage>().reason == SiteActionFailureReason::InvalidState);
}

void action_execution_item_based_plant_completion_consumes_seed_and_emits_planting(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 508U);
    GameMessageQueue queue {};
    auto site_context = make_site_context<ActionExecutionSystem>(campaign, site_run, queue, 60.0);

    site_run.inventory.worker_pack_slots[3].occupied = true;
    site_run.inventory.worker_pack_slots[3].item_id = gs1::ItemId {gs1::k_item_ordos_wormwood_seed_bundle};
    site_run.inventory.worker_pack_slots[3].item_quantity = 2U;
    site_run.inventory.worker_pack_slots[3].item_condition = 1.0f;
    site_run.inventory.worker_pack_slots[3].item_freshness = 1.0f;

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ActionExecutionSystem::process_message(
            site_context,
            make_start_action_message(
                GS1_SITE_ACTION_PLANT,
                TileCoord {2, 2},
                2U,
                0U,
                gs1::k_item_ordos_wormwood_seed_bundle)) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(context, site_run.site_action.current_action_id.has_value());
    const auto action_id = site_run.site_action.current_action_id->value;
    GS1_SYSTEM_TEST_CHECK(context, site_run.site_action.awaiting_placement_reservation);
    GS1_SYSTEM_TEST_REQUIRE(context, queue.size() == 1U);
    GS1_SYSTEM_TEST_CHECK(context, queue.front().type == GameMessageType::PlacementReservationRequested);
    GS1_SYSTEM_TEST_CHECK(
        context,
        queue.front().payload_as<PlacementReservationRequestedMessage>().subject_kind ==
            PlacementReservationSubjectKind::PlantType);
    GS1_SYSTEM_TEST_CHECK(
        context,
        queue.front().payload_as<PlacementReservationRequestedMessage>().subject_id ==
            gs1::k_plant_ordos_wormwood);

    queue.clear();
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ActionExecutionSystem::process_message(
            site_context,
            make_message(
                GameMessageType::PlacementReservationAccepted,
                PlacementReservationAcceptedMessage {action_id, 2, 2, 88U})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, !site_run.site_action.awaiting_placement_reservation);

    queue.clear();
    ActionExecutionSystem::run(site_context);
    GS1_SYSTEM_TEST_CHECK(context, count_messages(queue, GameMessageType::SiteActionCompleted) == 1U);
    GS1_SYSTEM_TEST_CHECK(context, count_messages(queue, GameMessageType::InventoryItemConsumeRequested) == 1U);
    GS1_SYSTEM_TEST_CHECK(context, count_messages(queue, GameMessageType::SiteTilePlantingCompleted) == 1U);
    GS1_SYSTEM_TEST_CHECK(context, count_messages(queue, GameMessageType::PlacementReservationReleased) == 1U);
    const auto* consume =
        first_message_payload<InventoryItemConsumeRequestedMessage>(
            queue,
            GameMessageType::InventoryItemConsumeRequested);
    GS1_SYSTEM_TEST_REQUIRE(context, consume != nullptr);
    GS1_SYSTEM_TEST_CHECK(context, consume->item_id == gs1::k_item_ordos_wormwood_seed_bundle);
    GS1_SYSTEM_TEST_CHECK(context, consume->quantity == 2U);
    GS1_SYSTEM_TEST_CHECK(context, consume->container_kind == GS1_INVENTORY_CONTAINER_WORKER_PACK);
    const auto* planting =
        first_message_payload<SiteTilePlantingCompletedMessage>(
            queue,
            GameMessageType::SiteTilePlantingCompleted);
    GS1_SYSTEM_TEST_REQUIRE(context, planting != nullptr);
    GS1_SYSTEM_TEST_CHECK(context, planting->plant_type_id == gs1::k_plant_ordos_wormwood);
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(planting->initial_density, 40.0f));
    GS1_SYSTEM_TEST_CHECK(context, !site_run.site_action.current_action_id.has_value());
}

void action_execution_item_based_plant_completion_rearms_placement_mode_when_seed_remains(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 50811U);
    GameMessageQueue queue {};
    auto site_context = make_site_context<ActionExecutionSystem>(campaign, site_run, queue, 60.0);

    site_run.inventory.worker_pack_slots[3].occupied = true;
    site_run.inventory.worker_pack_slots[3].item_id = gs1::ItemId {gs1::k_item_ordos_wormwood_seed_bundle};
    site_run.inventory.worker_pack_slots[3].item_quantity = 2U;
    site_run.inventory.worker_pack_slots[3].item_condition = 1.0f;
    site_run.inventory.worker_pack_slots[3].item_freshness = 1.0f;

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ActionExecutionSystem::process_message(
            site_context,
            make_start_action_message(
                GS1_SITE_ACTION_PLANT,
                TileCoord {4, 4},
                1U,
                0U,
                gs1::k_item_ordos_wormwood_seed_bundle,
                GS1_SITE_ACTION_REQUEST_FLAG_HAS_ITEM |
                    GS1_SITE_ACTION_REQUEST_FLAG_DEFERRED_TARGET_SELECTION)) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, site_run.site_action.placement_mode.active);

    queue.clear();
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ActionExecutionSystem::process_message(
            site_context,
            make_start_action_message(
                GS1_SITE_ACTION_PLANT,
                TileCoord {5, 4},
                1U,
                0U,
                gs1::k_item_ordos_wormwood_seed_bundle,
                GS1_SITE_ACTION_REQUEST_FLAG_HAS_ITEM)) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(context, site_run.site_action.current_action_id.has_value());
    const auto action_id = site_run.site_action.current_action_id->value;

    queue.clear();
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ActionExecutionSystem::process_message(
            site_context,
            make_message(
                GameMessageType::PlacementReservationAccepted,
                PlacementReservationAcceptedMessage {action_id, 4, 4, 92U})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, !site_run.site_action.awaiting_placement_reservation);

    auto flow_context = make_site_context<SiteFlowSystem>(campaign, site_run, queue, 60.0);
    SiteFlowSystem::run(flow_context);

    queue.clear();
    ActionExecutionSystem::run(site_context);
    GS1_SYSTEM_TEST_CHECK(context, count_messages(queue, GameMessageType::SiteActionCompleted) == 1U);
    GS1_SYSTEM_TEST_CHECK(context, count_messages(queue, GameMessageType::InventoryItemConsumeRequested) == 1U);
    GS1_SYSTEM_TEST_CHECK(context, !site_run.site_action.current_action_id.has_value());
    GS1_SYSTEM_TEST_CHECK(context, site_run.site_action.placement_mode.active);
    GS1_SYSTEM_TEST_CHECK(context, site_run.site_action.placement_mode.action_kind == ActionKind::Plant);
    GS1_SYSTEM_TEST_CHECK(
        context,
        site_run.site_action.placement_mode.item_id == gs1::k_item_ordos_wormwood_seed_bundle);
    GS1_SYSTEM_TEST_REQUIRE(context, site_run.site_action.placement_mode.target_tile.has_value());
    GS1_SYSTEM_TEST_CHECK(context, site_run.site_action.placement_mode.target_tile->x == 4);
    GS1_SYSTEM_TEST_CHECK(context, site_run.site_action.placement_mode.target_tile->y == 4);
    GS1_SYSTEM_TEST_CHECK(context, site_run.site_action.placement_mode.footprint_width == 2U);
    GS1_SYSTEM_TEST_CHECK(context, site_run.site_action.placement_mode.footprint_height == 2U);
    GS1_SYSTEM_TEST_CHECK(context, site_run.site_action.placement_mode.blocked_mask == 0xFULL);
}

void action_execution_item_based_plant_completion_stops_rearming_when_last_seed_is_consumed(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 50812U);
    GameMessageQueue queue {};
    auto site_context = make_site_context<ActionExecutionSystem>(campaign, site_run, queue, 60.0);

    site_run.inventory.worker_pack_slots[3].occupied = true;
    site_run.inventory.worker_pack_slots[3].item_id = gs1::ItemId {gs1::k_item_ordos_wormwood_seed_bundle};
    site_run.inventory.worker_pack_slots[3].item_quantity = 1U;
    site_run.inventory.worker_pack_slots[3].item_condition = 1.0f;
    site_run.inventory.worker_pack_slots[3].item_freshness = 1.0f;

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ActionExecutionSystem::process_message(
            site_context,
            make_start_action_message(
                GS1_SITE_ACTION_PLANT,
                TileCoord {4, 4},
                1U,
                0U,
                gs1::k_item_ordos_wormwood_seed_bundle,
                GS1_SITE_ACTION_REQUEST_FLAG_HAS_ITEM |
                    GS1_SITE_ACTION_REQUEST_FLAG_DEFERRED_TARGET_SELECTION)) == GS1_STATUS_OK);

    queue.clear();
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ActionExecutionSystem::process_message(
            site_context,
            make_start_action_message(
                GS1_SITE_ACTION_PLANT,
                TileCoord {5, 4},
                1U,
                0U,
                gs1::k_item_ordos_wormwood_seed_bundle,
                GS1_SITE_ACTION_REQUEST_FLAG_HAS_ITEM)) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(context, site_run.site_action.current_action_id.has_value());
    const auto action_id = site_run.site_action.current_action_id->value;

    queue.clear();
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ActionExecutionSystem::process_message(
            site_context,
            make_message(
                GameMessageType::PlacementReservationAccepted,
                PlacementReservationAcceptedMessage {action_id, 4, 4, 93U})) == GS1_STATUS_OK);

    auto flow_context = make_site_context<SiteFlowSystem>(campaign, site_run, queue, 60.0);
    SiteFlowSystem::run(flow_context);

    queue.clear();
    ActionExecutionSystem::run(site_context);
    GS1_SYSTEM_TEST_CHECK(context, count_messages(queue, GameMessageType::SiteActionCompleted) == 1U);
    GS1_SYSTEM_TEST_CHECK(context, !site_run.site_action.current_action_id.has_value());
    GS1_SYSTEM_TEST_CHECK(context, !site_run.site_action.placement_mode.active);
}

void action_execution_checkerboard_item_uses_2x2_placement_and_rejects_occupied_tiles(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 5081U);
    GameMessageQueue queue {};
    auto action_context = make_site_context<ActionExecutionSystem>(campaign, site_run, queue);

    site_run.inventory.worker_pack_slots[4].occupied = true;
    site_run.inventory.worker_pack_slots[4].item_id = gs1::ItemId {gs1::k_item_basic_straw_checkerboard};
    site_run.inventory.worker_pack_slots[4].item_quantity = 2U;
    site_run.inventory.worker_pack_slots[4].item_condition = 1.0f;
    site_run.inventory.worker_pack_slots[4].item_freshness = 1.0f;

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ActionExecutionSystem::process_message(
            action_context,
            make_start_action_message(
                GS1_SITE_ACTION_PLANT,
                TileCoord {4, 4},
                1U,
                0U,
                gs1::k_item_basic_straw_checkerboard,
                GS1_SITE_ACTION_REQUEST_FLAG_HAS_ITEM |
                    GS1_SITE_ACTION_REQUEST_FLAG_DEFERRED_TARGET_SELECTION)) == GS1_STATUS_OK);

    GS1_SYSTEM_TEST_CHECK(context, !site_run.site_action.current_action_id.has_value());
    GS1_SYSTEM_TEST_CHECK(context, site_run.site_action.placement_mode.active);
    GS1_SYSTEM_TEST_CHECK(context, site_run.site_action.placement_mode.action_kind == ActionKind::Plant);
    GS1_SYSTEM_TEST_CHECK(
        context,
        site_run.site_action.placement_mode.item_id == gs1::k_item_basic_straw_checkerboard);
    GS1_SYSTEM_TEST_CHECK(context, site_run.site_action.placement_mode.target_tile.has_value());
    GS1_SYSTEM_TEST_CHECK(context, site_run.site_action.placement_mode.target_tile->x == 4);
    GS1_SYSTEM_TEST_CHECK(context, site_run.site_action.placement_mode.target_tile->y == 4);
    GS1_SYSTEM_TEST_CHECK(context, site_run.site_action.placement_mode.footprint_width == 2U);
    GS1_SYSTEM_TEST_CHECK(context, site_run.site_action.placement_mode.footprint_height == 2U);
    GS1_SYSTEM_TEST_CHECK(context, site_run.site_action.placement_mode.blocked_mask == 0ULL);

    auto occupied_tile = site_run.site_world->tile_at(TileCoord {5, 5});
    occupied_tile.ecology.plant_id = gs1::PlantId {gs1::k_plant_ordos_wormwood};
    occupied_tile.ecology.plant_density = 0.6f;
    site_run.site_world->set_tile(TileCoord {5, 5}, occupied_tile);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ActionExecutionSystem::process_message(
            action_context,
            make_message(
                GameMessageType::PlacementModeCursorMoved,
                PlacementModeCursorMovedMessage {5, 5, 0U})) == GS1_STATUS_OK);

    GS1_SYSTEM_TEST_CHECK(context, site_run.site_action.placement_mode.target_tile.has_value());
    GS1_SYSTEM_TEST_CHECK(context, site_run.site_action.placement_mode.target_tile->x == 4);
    GS1_SYSTEM_TEST_CHECK(context, site_run.site_action.placement_mode.target_tile->y == 4);
    GS1_SYSTEM_TEST_CHECK(context, site_run.site_action.placement_mode.blocked_mask == (1ULL << 3U));

    queue.clear();
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ActionExecutionSystem::process_message(
            action_context,
            make_start_action_message(
                GS1_SITE_ACTION_PLANT,
                TileCoord {5, 5},
                1U,
                0U,
                gs1::k_item_basic_straw_checkerboard,
                GS1_SITE_ACTION_REQUEST_FLAG_HAS_ITEM)) == GS1_STATUS_OK);

    GS1_SYSTEM_TEST_CHECK(context, site_run.site_action.placement_mode.active);
    GS1_SYSTEM_TEST_CHECK(context, !site_run.site_action.current_action_id.has_value());
    GS1_SYSTEM_TEST_REQUIRE(context, count_messages(queue, GameMessageType::PlacementModeCommitRejected) == 1U);
    const auto* placement_rejected =
        first_message_payload<PlacementModeCommitRejectedMessage>(
            queue,
            GameMessageType::PlacementModeCommitRejected);
    GS1_SYSTEM_TEST_REQUIRE(context, placement_rejected != nullptr);
    GS1_SYSTEM_TEST_CHECK(context, placement_rejected->tile_x == 4);
    GS1_SYSTEM_TEST_CHECK(context, placement_rejected->tile_y == 4);
    GS1_SYSTEM_TEST_CHECK(context, placement_rejected->blocked_mask == (1ULL << 3U));
}

void action_execution_plant_progress_tracks_fixed_step_duration(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 510U);
    GameMessageQueue queue {};
    auto action_context = make_site_context<ActionExecutionSystem>(campaign, site_run, queue, 0.25);
    auto flow_context = make_site_context<SiteFlowSystem>(campaign, site_run, queue, 0.25);

    site_run.inventory.worker_pack_slots[3].occupied = true;
    site_run.inventory.worker_pack_slots[3].item_id = gs1::ItemId {gs1::k_item_ordos_wormwood_seed_bundle};
    site_run.inventory.worker_pack_slots[3].item_quantity = 1U;
    site_run.inventory.worker_pack_slots[3].item_condition = 1.0f;
    site_run.inventory.worker_pack_slots[3].item_freshness = 1.0f;

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ActionExecutionSystem::process_message(
            action_context,
            make_start_action_message(
                GS1_SITE_ACTION_PLANT,
                TileCoord {2, 2},
                1U,
                0U,
                gs1::k_item_ordos_wormwood_seed_bundle)) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(context, site_run.site_action.current_action_id.has_value());
    const auto action_id = site_run.site_action.current_action_id->value;

    queue.clear();
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ActionExecutionSystem::process_message(
            action_context,
            make_message(
                GameMessageType::PlacementReservationAccepted,
                PlacementReservationAcceptedMessage {action_id, 2, 2, 90U})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(context, queue.size() == 1U);
    GS1_SYSTEM_TEST_CHECK(context, queue[0].type == GameMessageType::SiteActionStarted);
    GS1_SYSTEM_TEST_CHECK(
        context,
        approx_equal(
            queue[0].payload_as<SiteActionStartedMessage>().duration_minutes,
            0.75f));
    GS1_SYSTEM_TEST_CHECK(
        context,
        approx_equal(site_run.site_action.total_action_minutes, 0.75));
    GS1_SYSTEM_TEST_CHECK(context, site_run.site_action.started_at_world_minute.has_value());

    queue.clear();
    for (int step = 0; step < 2; ++step)
    {
        SiteFlowSystem::run(flow_context);
        ActionExecutionSystem::run(action_context);
        GS1_SYSTEM_TEST_CHECK(context, site_run.site_action.current_action_id.has_value());
        GS1_SYSTEM_TEST_CHECK(context, count_messages(queue, GameMessageType::SiteActionCompleted) == 0U);
    }

    queue.clear();
    bool completed = false;
    for (int step = 0; step < 2 && !completed; ++step)
    {
        SiteFlowSystem::run(flow_context);
        ActionExecutionSystem::run(action_context);
        completed = count_messages(queue, GameMessageType::SiteActionCompleted) == 1U;
        if (!completed)
        {
            queue.clear();
        }
    }
    GS1_SYSTEM_TEST_CHECK(context, completed);
    GS1_SYSTEM_TEST_CHECK(context, !site_run.site_action.current_action_id.has_value());
    GS1_SYSTEM_TEST_CHECK(context, count_messages(queue, GameMessageType::SiteTilePlantingCompleted) == 1U);
}

void action_execution_plant_duration_respects_small_fixed_steps(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 511U);
    GameMessageQueue queue {};
    auto action_context = make_site_context<ActionExecutionSystem>(campaign, site_run, queue, 1.0 / 60.0);
    auto flow_context = make_site_context<SiteFlowSystem>(campaign, site_run, queue, 1.0 / 60.0);

    site_run.inventory.worker_pack_slots[3].occupied = true;
    site_run.inventory.worker_pack_slots[3].item_id = gs1::ItemId {gs1::k_item_ordos_wormwood_seed_bundle};
    site_run.inventory.worker_pack_slots[3].item_quantity = 1U;
    site_run.inventory.worker_pack_slots[3].item_condition = 1.0f;
    site_run.inventory.worker_pack_slots[3].item_freshness = 1.0f;

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ActionExecutionSystem::process_message(
            action_context,
            make_start_action_message(
                GS1_SITE_ACTION_PLANT,
                TileCoord {2, 2},
                1U,
                0U,
                gs1::k_item_ordos_wormwood_seed_bundle)) == GS1_STATUS_OK);
    const auto action_id = site_run.site_action.current_action_id->value;

    queue.clear();
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ActionExecutionSystem::process_message(
            action_context,
            make_message(
                GameMessageType::PlacementReservationAccepted,
                PlacementReservationAcceptedMessage {action_id, 2, 2, 91U})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(context, queue.size() == 1U);
    queue.clear();

    for (int step = 0; step < 4; ++step)
    {
        SiteFlowSystem::run(flow_context);
        ActionExecutionSystem::run(action_context);
    }

    GS1_SYSTEM_TEST_CHECK(context, site_run.site_action.current_action_id.has_value());
    GS1_SYSTEM_TEST_CHECK(context, count_messages(queue, GameMessageType::SiteActionCompleted) == 0U);
    GS1_SYSTEM_TEST_CHECK(context, count_messages(queue, GameMessageType::SiteTilePlantingCompleted) == 0U);

    bool completed = false;
    for (int step = 0; step < 80 && !completed; ++step)
    {
        SiteFlowSystem::run(flow_context);
        ActionExecutionSystem::run(action_context);
        completed = count_messages(queue, GameMessageType::SiteActionCompleted) == 1U;
    }

    GS1_SYSTEM_TEST_CHECK(context, completed);
    GS1_SYSTEM_TEST_CHECK(context, !site_run.site_action.current_action_id.has_value());
    GS1_SYSTEM_TEST_CHECK(context, count_messages(queue, GameMessageType::SiteTilePlantingCompleted) == 1U);
}

void placement_validation_accepts_releases_and_rejects_reserved_tiles(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 601U);
    GameMessageQueue queue {};
    auto site_context = make_site_context<PlacementValidationSystem>(campaign, site_run, queue);

    const auto request = make_message(
        GameMessageType::PlacementReservationRequested,
        gs1::PlacementReservationRequestedMessage {
            11U,
            2,
            2,
            PlacementOccupancyLayer::GroundCover,
            PlacementReservationSubjectKind::GroundCoverType,
            {0U, 0U},
            0U});

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        PlacementValidationSystem::process_message(site_context, request) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(context, queue.size() == 1U);
    GS1_SYSTEM_TEST_CHECK(context, queue.front().type == GameMessageType::PlacementReservationAccepted);
    const auto token = queue.front().payload_as<PlacementReservationAcceptedMessage>().reservation_token;

    queue.clear();
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        PlacementValidationSystem::process_message(
            site_context,
            make_message(
                GameMessageType::PlacementReservationRequested,
                gs1::PlacementReservationRequestedMessage {
                    12U,
                    2,
                    2,
                    PlacementOccupancyLayer::GroundCover,
                    PlacementReservationSubjectKind::GroundCoverType,
                    {0U, 0U},
                    0U})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(context, queue.size() == 1U);
    GS1_SYSTEM_TEST_CHECK(context, queue.front().type == GameMessageType::PlacementReservationRejected);
    GS1_SYSTEM_TEST_CHECK(
        context,
        queue.front().payload_as<PlacementReservationRejectedMessage>().reason_code ==
            gs1::PlacementReservationRejectionReason::Reserved);

    queue.clear();
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        PlacementValidationSystem::process_message(
            site_context,
            make_message(
                GameMessageType::PlacementReservationReleased,
                gs1::PlacementReservationReleasedMessage {0U, token})) == GS1_STATUS_OK);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        PlacementValidationSystem::process_message(
            site_context,
            make_message(
                GameMessageType::PlacementReservationRequested,
                gs1::PlacementReservationRequestedMessage {
                    13U,
                    2,
                    2,
                    PlacementOccupancyLayer::GroundCover,
                    PlacementReservationSubjectKind::GroundCoverType,
                    {0U, 0U},
                    0U})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, queue.back().type == GameMessageType::PlacementReservationAccepted);
}

void placement_validation_rejects_blocked_and_occupied_tiles(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 602U);
    GameMessageQueue queue {};
    auto site_context = make_site_context<PlacementValidationSystem>(campaign, site_run, queue);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        with_tile_component_mut<gs1::site_ecs::TilePlantable>(
            site_run,
            TileCoord {1, 1},
            [](auto& plantable) { plantable.value = false; }));
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        PlacementValidationSystem::process_message(
            site_context,
            make_message(
                GameMessageType::PlacementReservationRequested,
                gs1::PlacementReservationRequestedMessage {
                    21U,
                    1,
                    1,
                    PlacementOccupancyLayer::GroundCover,
                    PlacementReservationSubjectKind::GroundCoverType,
                    {0U, 0U},
                    0U})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(context, queue.size() == 1U);
    GS1_SYSTEM_TEST_CHECK(
        context,
        queue.front().payload_as<PlacementReservationRejectedMessage>().reason_code ==
            gs1::PlacementReservationRejectionReason::TerrainBlocked);

    queue.clear();
    auto occupied_tile = site_run.site_world->tile_at(TileCoord {3, 3});
    occupied_tile.ecology.ground_cover_type_id = 9U;
    site_run.site_world->set_tile(TileCoord {3, 3}, occupied_tile);
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        PlacementValidationSystem::process_message(
            site_context,
            make_message(
                GameMessageType::PlacementReservationRequested,
                gs1::PlacementReservationRequestedMessage {
                    22U,
                    3,
                    3,
                    PlacementOccupancyLayer::GroundCover,
                    PlacementReservationSubjectKind::GroundCoverType,
                    {0U, 0U},
                    0U})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(context, queue.size() == 1U);
    GS1_SYSTEM_TEST_CHECK(
        context,
        queue.front().payload_as<PlacementReservationRejectedMessage>().reason_code ==
            gs1::PlacementReservationRejectionReason::Occupied);
}

void placement_validation_structure_layer_handles_blocked_occupied_and_valid_tiles(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 603U);
    GameMessageQueue queue {};
    auto site_context = make_site_context<PlacementValidationSystem>(campaign, site_run, queue);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        with_tile_component_mut<gs1::site_ecs::TileTraversable>(
            site_run,
            TileCoord {5, 5},
            [](auto& traversable) { traversable.value = false; }));
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        PlacementValidationSystem::process_message(
            site_context,
            make_message(
                GameMessageType::PlacementReservationRequested,
                gs1::PlacementReservationRequestedMessage {
                    31U,
                    5,
                    5,
                    PlacementOccupancyLayer::Structure,
                    PlacementReservationSubjectKind::StructureType,
                    {0U, 0U},
                    gs1::k_structure_storage_crate})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(context, queue.size() == 1U);
    GS1_SYSTEM_TEST_CHECK(
        context,
        queue.front().payload_as<PlacementReservationRejectedMessage>().reason_code ==
            gs1::PlacementReservationRejectionReason::TerrainBlocked);

    queue.clear();
    auto occupied = site_run.site_world->tile_at(TileCoord {6, 5});
    occupied.device.structure_id = gs1::StructureId {33U};
    site_run.site_world->set_tile(TileCoord {6, 5}, occupied);
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        PlacementValidationSystem::process_message(
            site_context,
            make_message(
                GameMessageType::PlacementReservationRequested,
                gs1::PlacementReservationRequestedMessage {
                    32U,
                    6,
                    5,
                    PlacementOccupancyLayer::Structure,
                    PlacementReservationSubjectKind::StructureType,
                    {0U, 0U},
                    gs1::k_structure_storage_crate})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(context, queue.size() == 1U);
    GS1_SYSTEM_TEST_CHECK(
        context,
        queue.front().payload_as<PlacementReservationRejectedMessage>().reason_code ==
            gs1::PlacementReservationRejectionReason::Occupied);

    queue.clear();
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        PlacementValidationSystem::process_message(
            site_context,
            make_message(
                GameMessageType::PlacementReservationRequested,
                gs1::PlacementReservationRequestedMessage {
                    33U,
                    1,
                    5,
                    PlacementOccupancyLayer::Structure,
                    PlacementReservationSubjectKind::StructureType,
                    {0U, 0U},
                    gs1::k_structure_storage_crate})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(context, queue.size() == 1U);
    GS1_SYSTEM_TEST_CHECK(context, queue.front().type == GameMessageType::PlacementReservationAccepted);
}

void placement_validation_living_plants_use_2x2_footprint(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 604U);
    GameMessageQueue queue {};
    auto site_context = make_site_context<PlacementValidationSystem>(campaign, site_run, queue);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        PlacementValidationSystem::process_message(
            site_context,
            make_message(
                GameMessageType::PlacementReservationRequested,
                PlacementReservationRequestedMessage {
                    41U,
                    4,
                    4,
                    PlacementOccupancyLayer::GroundCover,
                    PlacementReservationSubjectKind::PlantType,
                    {0U, 0U},
                    gs1::k_plant_ordos_wormwood})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(context, queue.size() == 1U);
    GS1_SYSTEM_TEST_CHECK(context, queue.front().type == GameMessageType::PlacementReservationAccepted);
    const auto token = queue.front().payload_as<PlacementReservationAcceptedMessage>().reservation_token;

    queue.clear();
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        PlacementValidationSystem::process_message(
            site_context,
            make_message(
                GameMessageType::PlacementReservationRequested,
                PlacementReservationRequestedMessage {
                    42U,
                    5,
                    5,
                    PlacementOccupancyLayer::GroundCover,
                    PlacementReservationSubjectKind::GroundCoverType,
                    {0U, 0U},
                    0U})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(context, queue.size() == 1U);
    GS1_SYSTEM_TEST_CHECK(
        context,
        queue.front().payload_as<PlacementReservationRejectedMessage>().reason_code ==
            gs1::PlacementReservationRejectionReason::Reserved);

    queue.clear();
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        PlacementValidationSystem::process_message(
            site_context,
            make_message(
                GameMessageType::PlacementReservationRequested,
                PlacementReservationRequestedMessage {
                    43U,
                    8,
                    8,
                    PlacementOccupancyLayer::GroundCover,
                    PlacementReservationSubjectKind::PlantType,
                    {0U, 0U},
                    gs1::k_plant_ordos_wormwood})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(context, queue.size() == 1U);
    GS1_SYSTEM_TEST_CHECK(
        context,
        queue.front().payload_as<PlacementReservationRejectedMessage>().reason_code ==
            gs1::PlacementReservationRejectionReason::OutOfBounds);

    queue.clear();
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        PlacementValidationSystem::process_message(
            site_context,
            make_message(
                GameMessageType::PlacementReservationReleased,
                gs1::PlacementReservationReleasedMessage {0U, token})) == GS1_STATUS_OK);

    auto occupied_tile = site_run.site_world->tile_at(TileCoord {5, 4});
    occupied_tile.ecology.ground_cover_type_id = 9U;
    site_run.site_world->set_tile(TileCoord {5, 4}, occupied_tile);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        PlacementValidationSystem::process_message(
            site_context,
            make_message(
                GameMessageType::PlacementReservationRequested,
                PlacementReservationRequestedMessage {
                    44U,
                    4,
                    4,
                    PlacementOccupancyLayer::GroundCover,
                    PlacementReservationSubjectKind::PlantType,
                    {0U, 0U},
                    gs1::k_plant_ordos_wormwood})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(context, queue.size() == 1U);
    GS1_SYSTEM_TEST_CHECK(
        context,
        queue.front().payload_as<PlacementReservationRejectedMessage>().reason_code ==
            gs1::PlacementReservationRejectionReason::Occupied);
}

void placement_validation_rejects_misaligned_multitile_requests(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 605U);
    GameMessageQueue queue {};
    auto site_context = make_site_context<PlacementValidationSystem>(campaign, site_run, queue);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        PlacementValidationSystem::process_message(
            site_context,
            make_message(
                GameMessageType::PlacementReservationRequested,
                PlacementReservationRequestedMessage {
                    45U,
                    5,
                    5,
                    PlacementOccupancyLayer::GroundCover,
                    PlacementReservationSubjectKind::PlantType,
                    {0U, 0U},
                    gs1::k_plant_ordos_wormwood})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(context, queue.size() == 1U);
    GS1_SYSTEM_TEST_CHECK(context, queue.front().type == GameMessageType::PlacementReservationRejected);
    GS1_SYSTEM_TEST_CHECK(
        context,
        queue.front().payload_as<PlacementReservationRejectedMessage>().reason_code ==
            gs1::PlacementReservationRejectionReason::Misaligned);
}

void ecology_ground_cover_and_planting_update_tile_state(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 701U);
    GameMessageQueue queue {};
    auto site_context = make_site_context<EcologySystem>(campaign, site_run, queue);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        EcologySystem::process_message(
            site_context,
            make_message(
                GameMessageType::SiteGroundCoverPlaced,
                SiteGroundCoverPlacedMessage {1U, 2, 2, 5U, 0.4f, 0U})) == GS1_STATUS_OK);
    auto tile = site_run.site_world->tile_at(TileCoord {2, 2});
    GS1_SYSTEM_TEST_CHECK(context, tile.ecology.ground_cover_type_id == 5U);
    GS1_SYSTEM_TEST_CHECK(context, tile.ecology.plant_id.value == 0U);
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(tile.ecology.plant_density, 40.0f));
    GS1_SYSTEM_TEST_REQUIRE(context, queue.size() == 1U);
    GS1_SYSTEM_TEST_CHECK(context, queue.front().type == GameMessageType::TileEcologyBatchChanged);
    GS1_SYSTEM_TEST_CHECK(
        context,
        (queue.front().payload_as<TileEcologyBatchChangedMessage>().entries[0].changed_mask &
            gs1::TILE_ECOLOGY_CHANGED_OCCUPANCY) != 0U);

    queue.clear();
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        EcologySystem::process_message(
            site_context,
            make_message(
                GameMessageType::SiteTilePlantingCompleted,
                SiteTilePlantingCompletedMessage {
                    2U,
                    2,
                    2,
                    gs1::k_plant_ordos_wormwood,
                    0.6f,
                    0U})) == GS1_STATUS_OK);
    for (const auto coord : {TileCoord {2, 2}, TileCoord {3, 2}, TileCoord {2, 3}, TileCoord {3, 3}})
    {
        tile = site_run.site_world->tile_at(coord);
        GS1_SYSTEM_TEST_CHECK(context, tile.ecology.plant_id.value == gs1::k_plant_ordos_wormwood);
        GS1_SYSTEM_TEST_CHECK(context, tile.ecology.ground_cover_type_id == 0U);
        GS1_SYSTEM_TEST_CHECK(context, approx_equal(tile.ecology.plant_density, 60.0f));
    }
    GS1_SYSTEM_TEST_CHECK(context, count_tile_ecology_entries(queue) == 4U);
}

void ecology_watering_and_burial_clear_require_valid_targets(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 702U);
    GameMessageQueue queue {};
    auto site_context = make_site_context<EcologySystem>(campaign, site_run, queue);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        EcologySystem::process_message(
            site_context,
            make_message(
                GameMessageType::SiteTileWatered,
                SiteTileWateredMessage {1U, 1, 1, 1.0f, 0U})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, queue.empty());

    auto tile = site_run.site_world->tile_at(TileCoord {1, 1});
    tile.ecology.ground_cover_type_id = 3U;
    tile.ecology.plant_density = 20.0f;
    tile.ecology.moisture = 20.0f;
    tile.ecology.sand_burial = 80.0f;
    site_run.site_world->set_tile(TileCoord {1, 1}, tile);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        EcologySystem::process_message(
            site_context,
            make_message(
                GameMessageType::SiteTileWatered,
                SiteTileWateredMessage {2U, 1, 1, 1.5f, 0U})) == GS1_STATUS_OK);
    tile = site_run.site_world->tile_at(TileCoord {1, 1});
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(tile.ecology.moisture, 53.0f));
    GS1_SYSTEM_TEST_REQUIRE(context, queue.size() == 1U);
    GS1_SYSTEM_TEST_CHECK(
        context,
        queue.front().payload_as<TileEcologyBatchChangedMessage>().entries[0].changed_mask ==
            gs1::TILE_ECOLOGY_CHANGED_MOISTURE);

    queue.clear();
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        EcologySystem::process_message(
            site_context,
            make_message(
                GameMessageType::SiteTileBurialCleared,
                SiteTileBurialClearedMessage {3U, 1, 1, 0.3f, 0U})) == GS1_STATUS_OK);
    tile = site_run.site_world->tile_at(TileCoord {1, 1});
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(tile.ecology.sand_burial, 50.0f));
    GS1_SYSTEM_TEST_CHECK(
        context,
        queue.front().payload_as<TileEcologyBatchChangedMessage>().entries[0].changed_mask ==
            gs1::TILE_ECOLOGY_CHANGED_SAND_BURIAL);
}

void ecology_uses_average_wind_across_multitile_plant_footprint(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 7021U);
    GameMessageQueue queue {};
    auto site_context = make_site_context<EcologySystem>(campaign, site_run, queue, 1.0);

    for (const auto coord : {TileCoord {2, 2}, TileCoord {3, 2}, TileCoord {2, 3}, TileCoord {3, 3}})
    {
        auto tile = site_run.site_world->tile_at(coord);
        tile.ecology.plant_id = gs1::PlantId {gs1::k_plant_ordos_wormwood};
        tile.ecology.plant_density = 60.0f;
        tile.ecology.moisture = 0.0f;
        tile.ecology.soil_fertility = 0.0f;
        tile.ecology.soil_salinity = 0.0f;
        tile.ecology.sand_burial = 0.0f;
        tile.local_weather.heat = 0.0f;
        tile.local_weather.dust = 0.0f;
        tile.local_weather.wind =
            (coord.y == 2) ? 0.0f : 40.0f;
        site_run.site_world->set_tile(coord, tile);
    }

    EcologySystem::run(site_context);

    const auto top_left = site_run.site_world->tile_at(TileCoord {2, 2});
    const auto top_right = site_run.site_world->tile_at(TileCoord {3, 2});
    const auto bottom_left = site_run.site_world->tile_at(TileCoord {2, 3});
    const auto bottom_right = site_run.site_world->tile_at(TileCoord {3, 3});

    GS1_SYSTEM_TEST_CHECK(context, top_left.ecology.growth_pressure > 0.0f);
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(top_left.ecology.growth_pressure, top_right.ecology.growth_pressure));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(top_left.ecology.growth_pressure, bottom_left.ecology.growth_pressure));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(top_left.ecology.growth_pressure, bottom_right.ecology.growth_pressure));
}

void ecology_uses_owner_specific_weather_contributions_for_terrain_updates(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 7022U);
    GameMessageQueue queue {};
    auto site_context = make_site_context<EcologySystem>(campaign, site_run, queue, 1.0);

    auto tile = site_run.site_world->tile_at(TileCoord {2, 2});
    tile.ecology.plant_id = gs1::PlantId {gs1::k_plant_white_thorn};
    tile.ecology.plant_density = 50.0f;
    tile.ecology.moisture = 35.0f;
    tile.ecology.soil_fertility = 40.0f;
    tile.ecology.soil_salinity = 20.0f;
    tile.local_weather = gs1::SiteWorld::TileLocalWeatherData {0.0f, 0.0f, 0.0f};
    tile.plant_weather_contribution = gs1::SiteWorld::TileWeatherContributionData {
        0.0f,
        0.0f,
        0.0f,
        2.5f,
        2.0f,
        1.5f};
    tile.device_weather_contribution = gs1::SiteWorld::TileWeatherContributionData {
        0.0f,
        0.0f,
        0.0f,
        1.5f,
        2.0f,
        2.5f};
    site_run.site_world->set_tile(TileCoord {2, 2}, tile);

    EcologySystem::run(site_context);

    const auto updated = site_run.site_world->tile_at(TileCoord {2, 2});
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(updated.ecology.moisture, 35.064f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(updated.ecology.soil_fertility, 40.048f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(updated.ecology.soil_salinity, 16.8f));
}

void ecology_run_grows_occupied_tiles_and_updates_progress(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 703U);
    GameMessageQueue queue {};
    auto site_context = make_site_context<EcologySystem>(campaign, site_run, queue, 60.0);

    site_run.counters.site_completion_tile_threshold = 2U;
    auto occupied = site_run.site_world->tile_at(TileCoord {2, 2});
    occupied.ecology.plant_id = gs1::PlantId {7U};
    occupied.ecology.plant_density = 99.0f;
    occupied.ecology.moisture = 100.0f;
    occupied.ecology.soil_fertility = 100.0f;
    site_run.site_world->set_tile(TileCoord {2, 2}, occupied);

    auto second = site_run.site_world->tile_at(TileCoord {3, 2});
    second.ecology.ground_cover_type_id = 5U;
    second.ecology.plant_density = 100.0f;
    site_run.site_world->set_tile(TileCoord {3, 2}, second);

    EcologySystem::run(site_context);

    occupied = site_run.site_world->tile_at(TileCoord {2, 2});
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(occupied.ecology.plant_density, 100.0f));
    GS1_SYSTEM_TEST_CHECK(context, site_run.counters.fully_grown_tile_count == 2U);
    GS1_SYSTEM_TEST_CHECK(context, count_tile_ecology_entries(queue) >= 1U);
    GS1_SYSTEM_TEST_CHECK(context, count_messages(queue, GameMessageType::RestorationProgressChanged) == 1U);
    const auto* progress =
        first_message_payload<gs1::RestorationProgressChangedMessage>(
            queue,
            GameMessageType::RestorationProgressChanged);
    GS1_SYSTEM_TEST_REQUIRE(context, progress != nullptr);
    GS1_SYSTEM_TEST_CHECK(context, progress->fully_grown_tile_count == 2U);
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(progress->normalized_progress, 1.0f));
}

void ecology_highway_target_tiles_accumulate_cover_and_clear_via_burial_action(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(5U, 704U);
    GameMessageQueue queue {};
    auto site_context = make_site_context<EcologySystem>(campaign, site_run, queue, 60.0);

    configure_highway_protection_objective(
        site_run,
        gs1::SiteObjectiveTargetEdge::East,
        12.0,
        0.5f);
    const TileCoord road_coord {7, 2};
    mark_objective_target_tile(site_run, road_coord);

    auto road_tile = site_run.site_world->tile_at(road_coord);
    road_tile.local_weather.wind = 50.0f;
    road_tile.local_weather.dust = 35.0f;
    site_run.site_world->set_tile(road_coord, road_tile);

    EcologySystem::run(site_context);

    road_tile = site_run.site_world->tile_at(road_coord);
    GS1_SYSTEM_TEST_CHECK(context, !road_tile.static_data.plantable);
    GS1_SYSTEM_TEST_CHECK(context, road_tile.ecology.soil_fertility > 0.0f);
    GS1_SYSTEM_TEST_CHECK(
        context,
        approx_equal(
            site_run.counters.highway_average_sand_cover,
            road_tile.ecology.soil_fertility));
    GS1_SYSTEM_TEST_CHECK(context, site_run.counters.objective_progress_normalized < 1.0f);
    GS1_SYSTEM_TEST_CHECK(context, count_messages(queue, GameMessageType::RestorationProgressChanged) == 0U);

    const float cover_after_accumulation = road_tile.ecology.soil_fertility;
    queue.clear();
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        EcologySystem::process_message(
            site_context,
            make_message(
                GameMessageType::SiteTileBurialCleared,
                SiteTileBurialClearedMessage {
                    1U,
                    road_coord.x,
                    road_coord.y,
                    0.1f,
                    0U})) == GS1_STATUS_OK);
    road_tile = site_run.site_world->tile_at(road_coord);
    GS1_SYSTEM_TEST_CHECK(context, road_tile.ecology.soil_fertility < cover_after_accumulation);
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(road_tile.ecology.sand_burial, 0.0f));
}
}  // namespace

GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "action_execution",
    "rejects_invalid_or_out_of_bounds_requests",
    action_execution_rejects_invalid_or_out_of_bounds_requests);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "action_execution",
    "rejects_new_request_while_busy",
    action_execution_rejects_new_request_while_busy);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "action_execution",
    "water_starts_immediately_and_defers_cost_until_completion",
    action_execution_water_starts_immediately_and_defers_cost_until_completion);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "action_execution",
    "weather_scales_worker_meter_costs_from_current_local_weather",
    action_execution_weather_scales_worker_meter_costs_from_current_local_weather);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "action_execution",
    "morale_cost_reaches_full_authored_value_at_single_weather_100",
    action_execution_morale_cost_reaches_full_authored_value_at_single_weather_100);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "action_execution",
    "craft_uses_recipe_authored_duration_and_cost",
    action_execution_craft_uses_recipe_authored_duration_and_cost);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "action_execution",
    "progress_slows_with_lower_previous_work_efficiency",
    action_execution_progress_slows_with_lower_previous_work_efficiency);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "action_execution",
    "plant_waits_for_reservation_then_starts_on_accept",
    action_execution_plant_waits_for_reservation_then_starts_on_accept);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "action_execution",
    "planting_auto_move_reaches_tile_before_action_starts",
    planting_auto_move_reaches_tile_before_action_starts);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "action_execution",
    "deferred_plant_enters_placement_mode_and_tracks_preview",
    action_execution_deferred_plant_enters_placement_mode_and_tracks_preview);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "action_execution",
    "confirming_placement_mode_starts_real_action",
    action_execution_confirming_placement_mode_starts_real_action);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "action_execution",
    "cancel_placement_mode_clears_preview_without_failure",
    action_execution_cancel_placement_mode_clears_preview_without_failure);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "action_execution",
    "rejection_and_cancel_clear_action_state",
    action_execution_rejection_and_cancel_clear_action_state);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "action_execution",
    "run_completes_actions_and_emits_fact_messages",
    action_execution_run_completes_actions_and_emits_fact_messages);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "action_execution",
    "plant_completion_emits_ground_cover_after_reservation",
    action_execution_plant_completion_emits_ground_cover_after_reservation);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "action_execution",
    "harvest_completion_emits_worker_pack_insert_and_tile_harvested",
    action_execution_harvest_completion_emits_worker_pack_insert_and_tile_harvested);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "action_execution",
    "excavate_starts_immediately_and_emits_cost",
    action_execution_excavate_starts_immediately_and_emits_cost);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "action_execution",
    "excavate_rejects_occupied_tiles",
    action_execution_excavate_rejects_occupied_tiles);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "action_execution",
    "excavate_rejects_when_worker_pack_has_no_reward_space",
    action_execution_excavate_rejects_when_worker_pack_has_no_reward_space);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "action_execution",
    "excavate_completion_misses_or_finds_deterministic_loot",
    action_execution_excavate_completion_misses_or_finds_deterministic_loot);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "action_execution",
    "excavate_completion_marks_tile_projection_dirty",
    action_execution_excavate_completion_marks_tile_projection_dirty);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "action_execution",
    "excavate_cannot_repeat_rough_depth_by_default",
    action_execution_excavate_cannot_repeat_rough_depth_by_default);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "action_execution",
    "item_based_plant_completion_consumes_seed_and_emits_planting",
    action_execution_item_based_plant_completion_consumes_seed_and_emits_planting);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "action_execution",
    "item_based_plant_completion_rearms_placement_mode_when_seed_remains",
    action_execution_item_based_plant_completion_rearms_placement_mode_when_seed_remains);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "action_execution",
    "item_based_plant_completion_stops_rearming_when_last_seed_is_consumed",
    action_execution_item_based_plant_completion_stops_rearming_when_last_seed_is_consumed);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "action_execution",
    "checkerboard_item_uses_2x2_placement_and_rejects_occupied_tiles",
    action_execution_checkerboard_item_uses_2x2_placement_and_rejects_occupied_tiles);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "action_execution",
    "plant_progress_tracks_fixed_step_duration",
    action_execution_plant_progress_tracks_fixed_step_duration);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "action_execution",
    "plant_duration_respects_small_fixed_steps",
    action_execution_plant_duration_respects_small_fixed_steps);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "placement_validation",
    "accepts_releases_and_rejects_reserved_tiles",
    placement_validation_accepts_releases_and_rejects_reserved_tiles);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "placement_validation",
    "rejects_blocked_and_occupied_tiles",
    placement_validation_rejects_blocked_and_occupied_tiles);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "placement_validation",
    "structure_layer_handles_blocked_occupied_and_valid_tiles",
    placement_validation_structure_layer_handles_blocked_occupied_and_valid_tiles);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "placement_validation",
    "living_plants_use_2x2_footprint",
    placement_validation_living_plants_use_2x2_footprint);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "placement_validation",
    "rejects_misaligned_multitile_requests",
    placement_validation_rejects_misaligned_multitile_requests);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "ecology",
    "ground_cover_and_planting_update_tile_state",
    ecology_ground_cover_and_planting_update_tile_state);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "ecology",
    "watering_and_burial_clear_require_valid_targets",
    ecology_watering_and_burial_clear_require_valid_targets);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "ecology",
    "uses_average_wind_across_multitile_plant_footprint",
    ecology_uses_average_wind_across_multitile_plant_footprint);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "ecology",
    "uses_owner_specific_weather_contributions_for_terrain_updates",
    ecology_uses_owner_specific_weather_contributions_for_terrain_updates);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "ecology",
    "run_grows_occupied_tiles_and_updates_progress",
    ecology_run_grows_occupied_tiles_and_updates_progress);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "ecology",
    "highway_target_tiles_accumulate_cover_and_clear_via_burial_action",
    ecology_highway_target_tiles_accumulate_cover_and_clear_via_burial_action);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "ecology",
    "harvest_flow_inserts_output_and_reduces_multitile_density",
    harvest_flow_inserts_output_and_reduces_multitile_density);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "ecology",
    "harvest_last_cut_kills_low_density_plant_without_loot",
    harvest_last_cut_kills_low_density_plant_without_loot);
