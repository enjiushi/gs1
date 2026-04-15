#include "commands/game_command.h"
#include "content/defs/item_defs.h"
#include "site/site_world_access.h"
#include "site/systems/action_execution_system.h"
#include "site/systems/ecology_system.h"
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
using gs1::GameCommand;
using gs1::GameCommandQueue;
using gs1::GameCommandType;
using gs1::InventoryItemConsumeRequestedCommand;
using gs1::PlacementOccupancyLayer;
using gs1::PlacementReservationAcceptedCommand;
using gs1::PlacementReservationRejectedCommand;
using gs1::PlacementValidationSystem;
using gs1::RuntimeActionId;
using gs1::SiteActionCompletedCommand;
using gs1::SiteActionFailureReason;
using gs1::SiteActionFailedCommand;
using gs1::SiteActionStartedCommand;
using gs1::SiteGroundCoverPlacedCommand;
using gs1::SiteMoveDirectionInput;
using gs1::SiteFlowSystem;
using gs1::SiteTileBurialClearedCommand;
using gs1::SiteTilePlantingCompletedCommand;
using gs1::SiteTileWateredCommand;
using gs1::TileCoord;
using gs1::TileEcologyChangedCommand;
using namespace gs1::testing::fixtures;

template <typename Payload>
GameCommand make_command(gs1::GameCommandType type, const Payload& payload)
{
    GameCommand command {};
    command.type = type;
    command.set_payload(payload);
    return command;
}

GameCommand make_start_action_command(
    Gs1SiteActionKind action_kind,
    TileCoord target_tile,
    std::uint16_t quantity = 1U,
    std::uint32_t primary_subject_id = 1U,
    std::uint32_t item_id = 0U)
{
    return make_command(
        GameCommandType::StartSiteAction,
        gs1::StartSiteActionCommand {
            action_kind,
            0U,
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
    GameCommandQueue queue {};
    auto site_context = make_site_context<ActionExecutionSystem>(campaign, site_run, queue);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ActionExecutionSystem::process_command(
            site_context,
            make_start_action_command(GS1_SITE_ACTION_BUILD, TileCoord {1, 1})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(context, queue.size() == 1U);
    GS1_SYSTEM_TEST_CHECK(context, queue.front().type == GameCommandType::SiteActionFailed);
    GS1_SYSTEM_TEST_CHECK(
        context,
        queue.front().payload_as<SiteActionFailedCommand>().reason == SiteActionFailureReason::InvalidState);
    GS1_SYSTEM_TEST_CHECK(context, !site_run.site_action.current_action_id.has_value());

    queue.clear();
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ActionExecutionSystem::process_command(
            site_context,
            make_start_action_command(GS1_SITE_ACTION_WATER, TileCoord {99, 99})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(context, queue.size() == 1U);
    GS1_SYSTEM_TEST_CHECK(
        context,
        queue.front().payload_as<SiteActionFailedCommand>().reason == SiteActionFailureReason::InvalidTarget);
    GS1_SYSTEM_TEST_CHECK(context, !site_run.site_action.current_action_id.has_value());
}

void action_execution_rejects_new_request_while_busy(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 502U);
    GameCommandQueue queue {};
    auto site_context = make_site_context<ActionExecutionSystem>(campaign, site_run, queue);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ActionExecutionSystem::process_command(
            site_context,
            make_start_action_command(GS1_SITE_ACTION_WATER, TileCoord {1, 1}, 1U)) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(context, site_run.site_action.current_action_id.has_value());
    GS1_SYSTEM_TEST_CHECK(context, queue.size() == 2U);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ActionExecutionSystem::process_command(
            site_context,
            make_start_action_command(GS1_SITE_ACTION_CLEAR_BURIAL, TileCoord {1, 2}, 1U)) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, queue.size() == 3U);
    GS1_SYSTEM_TEST_CHECK(
        context,
        queue.back().payload_as<SiteActionFailedCommand>().reason == SiteActionFailureReason::Busy);
}

void action_execution_water_starts_immediately_and_emits_cost(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 503U);
    GameCommandQueue queue {};
    auto site_context = make_site_context<ActionExecutionSystem>(campaign, site_run, queue);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ActionExecutionSystem::process_command(
            site_context,
            make_start_action_command(GS1_SITE_ACTION_WATER, TileCoord {2, 3}, 2U, 7U)) == GS1_STATUS_OK);

    GS1_SYSTEM_TEST_REQUIRE(context, site_run.site_action.current_action_id.has_value());
    GS1_SYSTEM_TEST_CHECK(context, site_run.site_action.action_kind == ActionKind::Water);
    GS1_SYSTEM_TEST_CHECK(context, !site_run.site_action.awaiting_placement_reservation);
    GS1_SYSTEM_TEST_CHECK(context, site_run.site_action.target_tile.has_value());
    GS1_SYSTEM_TEST_CHECK(context, site_run.site_action.target_tile->x == 2);
    GS1_SYSTEM_TEST_CHECK(context, site_run.site_action.target_tile->y == 3);
    GS1_SYSTEM_TEST_CHECK(context, site_run.site_action.primary_subject_id == 7U);
    GS1_SYSTEM_TEST_CHECK(context, queue.size() == 2U);
    GS1_SYSTEM_TEST_CHECK(context, queue[0].type == GameCommandType::SiteActionStarted);
    GS1_SYSTEM_TEST_CHECK(context, queue[1].type == GameCommandType::WorkerMeterDeltaRequested);
    GS1_SYSTEM_TEST_CHECK(
        context,
        queue[0].payload_as<SiteActionStartedCommand>().primary_subject_id == 7U);
    GS1_SYSTEM_TEST_CHECK(
        context,
        approx_equal(
            queue[1].payload_as<gs1::WorkerMeterDeltaRequestedCommand>().hydration_delta,
            -0.7f));
    GS1_SYSTEM_TEST_CHECK(
        context,
        approx_equal(
            queue[1].payload_as<gs1::WorkerMeterDeltaRequestedCommand>().energy_delta,
            -2.0f));
}

void action_execution_plant_waits_for_reservation_then_starts_on_accept(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 504U);
    GameCommandQueue queue {};
    auto site_context = make_site_context<ActionExecutionSystem>(campaign, site_run, queue);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ActionExecutionSystem::process_command(
            site_context,
            make_start_action_command(GS1_SITE_ACTION_PLANT, TileCoord {3, 3}, 1U, 12U)) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(context, site_run.site_action.current_action_id.has_value());
    const auto action_id = site_run.site_action.current_action_id->value;
    GS1_SYSTEM_TEST_CHECK(context, site_run.site_action.awaiting_placement_reservation);
    GS1_SYSTEM_TEST_REQUIRE(context, queue.size() == 1U);
    GS1_SYSTEM_TEST_CHECK(context, queue.front().type == GameCommandType::PlacementReservationRequested);

    queue.clear();
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ActionExecutionSystem::process_command(
            site_context,
            make_command(
                GameCommandType::PlacementReservationAccepted,
                PlacementReservationAcceptedCommand {action_id, 3, 3, 55U})) == GS1_STATUS_OK);

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
    GameCommandQueue queue {};
    auto action_context = make_site_context<ActionExecutionSystem>(campaign, site_run, queue);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ActionExecutionSystem::process_command(
            action_context,
            make_start_action_command(GS1_SITE_ACTION_PLANT, TileCoord {4, 2}, 1U, 12U)) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(context, site_run.site_action.current_action_id.has_value());
    const auto action_id = site_run.site_action.current_action_id->value;
    GS1_SYSTEM_TEST_CHECK(context, site_run.site_action.awaiting_placement_reservation);

    queue.clear();
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ActionExecutionSystem::process_command(
            action_context,
            make_command(
                GameCommandType::PlacementReservationAccepted,
                PlacementReservationAcceptedCommand {action_id, 4, 2, 56U})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, !site_run.site_action.awaiting_placement_reservation);
    GS1_SYSTEM_TEST_CHECK(context, !site_run.site_action.started_at_world_minute.has_value());
    GS1_SYSTEM_TEST_CHECK(context, queue.empty());

    auto flow_context = make_site_context<SiteFlowSystem>(campaign, site_run, queue);
    SiteFlowSystem::run(flow_context);

    const auto worker = site_run.site_world->worker();
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(worker.position.tile_x, 4.0f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(worker.position.tile_y, 2.0f));
    GS1_SYSTEM_TEST_CHECK(context, worker.position.tile_coord.x == 4);
    GS1_SYSTEM_TEST_CHECK(context, worker.position.tile_coord.y == 2);

    queue.clear();
    ActionExecutionSystem::run(action_context);
    GS1_SYSTEM_TEST_REQUIRE(context, queue.size() == 2U);
    GS1_SYSTEM_TEST_CHECK(context, queue[0].type == GameCommandType::SiteActionStarted);
    GS1_SYSTEM_TEST_CHECK(context, queue[1].type == GameCommandType::WorkerMeterDeltaRequested);
    GS1_SYSTEM_TEST_CHECK(context, site_run.site_action.started_at_world_minute.has_value());
}

void action_execution_rejection_and_cancel_clear_action_state(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 505U);
    GameCommandQueue queue {};
    auto site_context = make_site_context<ActionExecutionSystem>(campaign, site_run, queue);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ActionExecutionSystem::process_command(
            site_context,
            make_start_action_command(GS1_SITE_ACTION_PLANT, TileCoord {4, 4})) == GS1_STATUS_OK);
    const auto action_id = site_run.site_action.current_action_id->value;

    queue.clear();
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ActionExecutionSystem::process_command(
            site_context,
            make_command(
                GameCommandType::PlacementReservationRejected,
                PlacementReservationRejectedCommand {
                    action_id,
                    4,
                    4,
                    gs1::PlacementReservationRejectionReason::Occupied,
                    {0U, 0U, 0U}})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, !site_run.site_action.current_action_id.has_value());
    GS1_SYSTEM_TEST_REQUIRE(context, queue.size() == 1U);
    GS1_SYSTEM_TEST_CHECK(context, queue.front().type == GameCommandType::SiteActionFailed);

    queue.clear();
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ActionExecutionSystem::process_command(
            site_context,
            make_start_action_command(GS1_SITE_ACTION_WATER, TileCoord {1, 1})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(context, site_run.site_action.current_action_id.has_value());
    const auto cancel_id = site_run.site_action.current_action_id->value;

    queue.clear();
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ActionExecutionSystem::process_command(
            site_context,
            make_command(
                GameCommandType::CancelSiteAction,
                gs1::CancelSiteActionCommand {cancel_id, GS1_SITE_ACTION_CANCEL_FLAG_CURRENT_ACTION})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, !site_run.site_action.current_action_id.has_value());
    GS1_SYSTEM_TEST_REQUIRE(context, queue.size() == 2U);
    GS1_SYSTEM_TEST_CHECK(context, queue[0].type == GameCommandType::PlacementReservationReleased);
    GS1_SYSTEM_TEST_CHECK(context, queue[1].type == GameCommandType::SiteActionFailed);
    GS1_SYSTEM_TEST_CHECK(
        context,
        queue[1].payload_as<SiteActionFailedCommand>().reason == SiteActionFailureReason::Cancelled);
}

void action_execution_run_completes_actions_and_emits_fact_commands(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 506U);
    GameCommandQueue queue {};
    auto site_context = make_site_context<ActionExecutionSystem>(campaign, site_run, queue);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ActionExecutionSystem::process_command(
            site_context,
            make_start_action_command(GS1_SITE_ACTION_WATER, TileCoord {2, 2}, 3U)) == GS1_STATUS_OK);

    queue.clear();
    site_run.clock.world_time_minutes += 3.0;
    ActionExecutionSystem::run(site_context);
    GS1_SYSTEM_TEST_REQUIRE(context, queue.size() == 3U);
    GS1_SYSTEM_TEST_CHECK(context, queue[0].type == GameCommandType::SiteActionCompleted);
    GS1_SYSTEM_TEST_CHECK(context, queue[1].type == GameCommandType::SiteTileWatered);
    GS1_SYSTEM_TEST_CHECK(context, queue[2].type == GameCommandType::PlacementReservationReleased);
    GS1_SYSTEM_TEST_CHECK(
        context,
        queue[1].payload_as<SiteTileWateredCommand>().water_amount == 3.0f);
    GS1_SYSTEM_TEST_CHECK(context, !site_run.site_action.current_action_id.has_value());

    queue.clear();
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ActionExecutionSystem::process_command(
            site_context,
            make_start_action_command(GS1_SITE_ACTION_CLEAR_BURIAL, TileCoord {1, 1}, 2U)) == GS1_STATUS_OK);
    queue.clear();
    site_run.clock.world_time_minutes += 3.0;
    ActionExecutionSystem::run(site_context);
    GS1_SYSTEM_TEST_REQUIRE(context, queue.size() == 3U);
    GS1_SYSTEM_TEST_CHECK(context, queue[1].type == GameCommandType::SiteTileBurialCleared);
    GS1_SYSTEM_TEST_CHECK(
        context,
        approx_equal(queue[1].payload_as<SiteTileBurialClearedCommand>().cleared_amount, 0.7f));
}

void action_execution_plant_completion_emits_ground_cover_after_reservation(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 507U);
    GameCommandQueue queue {};
    auto site_context = make_site_context<ActionExecutionSystem>(campaign, site_run, queue);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ActionExecutionSystem::process_command(
            site_context,
            make_start_action_command(GS1_SITE_ACTION_PLANT, TileCoord {2, 2}, 2U, 9U)) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(context, site_run.site_action.current_action_id.has_value());
    const auto action_id = site_run.site_action.current_action_id->value;
    GS1_SYSTEM_TEST_CHECK(context, site_run.site_action.awaiting_placement_reservation);

    queue.clear();
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ActionExecutionSystem::process_command(
            site_context,
            make_command(
                GameCommandType::PlacementReservationAccepted,
                PlacementReservationAcceptedCommand {action_id, 2, 2, 77U})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, !site_run.site_action.awaiting_placement_reservation);

    queue.clear();
    site_run.clock.world_time_minutes += 3.0;
    ActionExecutionSystem::run(site_context);
    GS1_SYSTEM_TEST_REQUIRE(context, queue.size() == 3U);
    GS1_SYSTEM_TEST_CHECK(context, queue[0].type == GameCommandType::SiteActionCompleted);
    GS1_SYSTEM_TEST_CHECK(context, queue[1].type == GameCommandType::SiteGroundCoverPlaced);
    GS1_SYSTEM_TEST_CHECK(context, queue[2].type == GameCommandType::PlacementReservationReleased);
    GS1_SYSTEM_TEST_CHECK(
        context,
        queue[1].payload_as<SiteGroundCoverPlacedCommand>().ground_cover_type_id == 9U);
    GS1_SYSTEM_TEST_CHECK(
        context,
        approx_equal(queue[1].payload_as<SiteGroundCoverPlacedCommand>().initial_density, 0.5f));
    GS1_SYSTEM_TEST_CHECK(context, !site_run.site_action.current_action_id.has_value());
}

void action_execution_item_based_plant_completion_consumes_seed_and_emits_planting(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 508U);
    GameCommandQueue queue {};
    auto site_context = make_site_context<ActionExecutionSystem>(campaign, site_run, queue);

    site_run.inventory.worker_pack_slots[3].occupied = true;
    site_run.inventory.worker_pack_slots[3].item_id = gs1::ItemId {gs1::k_item_wind_reed_seed_bundle};
    site_run.inventory.worker_pack_slots[3].item_quantity = 2U;
    site_run.inventory.worker_pack_slots[3].item_condition = 1.0f;
    site_run.inventory.worker_pack_slots[3].item_freshness = 1.0f;

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ActionExecutionSystem::process_command(
            site_context,
            make_start_action_command(
                GS1_SITE_ACTION_PLANT,
                TileCoord {2, 2},
                2U,
                0U,
                gs1::k_item_wind_reed_seed_bundle)) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(context, site_run.site_action.current_action_id.has_value());
    const auto action_id = site_run.site_action.current_action_id->value;
    GS1_SYSTEM_TEST_CHECK(context, site_run.site_action.awaiting_placement_reservation);
    GS1_SYSTEM_TEST_REQUIRE(context, queue.size() == 1U);
    GS1_SYSTEM_TEST_CHECK(context, queue.front().type == GameCommandType::PlacementReservationRequested);

    queue.clear();
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ActionExecutionSystem::process_command(
            site_context,
            make_command(
                GameCommandType::PlacementReservationAccepted,
                PlacementReservationAcceptedCommand {action_id, 2, 2, 88U})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, !site_run.site_action.awaiting_placement_reservation);

    queue.clear();
    site_run.clock.world_time_minutes += 3.0;
    ActionExecutionSystem::run(site_context);
    GS1_SYSTEM_TEST_REQUIRE(context, queue.size() == 4U);
    GS1_SYSTEM_TEST_CHECK(context, queue[0].type == GameCommandType::SiteActionCompleted);
    GS1_SYSTEM_TEST_CHECK(context, queue[1].type == GameCommandType::InventoryItemConsumeRequested);
    GS1_SYSTEM_TEST_CHECK(context, queue[2].type == GameCommandType::SiteTilePlantingCompleted);
    GS1_SYSTEM_TEST_CHECK(context, queue[3].type == GameCommandType::PlacementReservationReleased);
    GS1_SYSTEM_TEST_CHECK(
        context,
        queue[1].payload_as<InventoryItemConsumeRequestedCommand>().item_id ==
            gs1::k_item_wind_reed_seed_bundle);
    GS1_SYSTEM_TEST_CHECK(
        context,
        queue[1].payload_as<InventoryItemConsumeRequestedCommand>().quantity == 2U);
    GS1_SYSTEM_TEST_CHECK(
        context,
        queue[1].payload_as<InventoryItemConsumeRequestedCommand>().container_kind ==
            GS1_INVENTORY_CONTAINER_WORKER_PACK);
    GS1_SYSTEM_TEST_CHECK(
        context,
        queue[2].payload_as<SiteTilePlantingCompletedCommand>().plant_type_id == gs1::k_plant_wind_reed);
    GS1_SYSTEM_TEST_CHECK(
        context,
        approx_equal(queue[2].payload_as<SiteTilePlantingCompletedCommand>().initial_density, 0.4f));
    GS1_SYSTEM_TEST_CHECK(context, !site_run.site_action.current_action_id.has_value());
}

void action_execution_plant_progress_tracks_world_time(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 510U);
    GameCommandQueue queue {};
    auto action_context = make_site_context<ActionExecutionSystem>(campaign, site_run, queue, 0.25);
    auto flow_context = make_site_context<SiteFlowSystem>(campaign, site_run, queue, 0.25);

    site_run.inventory.worker_pack_slots[3].occupied = true;
    site_run.inventory.worker_pack_slots[3].item_id = gs1::ItemId {gs1::k_item_wind_reed_seed_bundle};
    site_run.inventory.worker_pack_slots[3].item_quantity = 1U;
    site_run.inventory.worker_pack_slots[3].item_condition = 1.0f;
    site_run.inventory.worker_pack_slots[3].item_freshness = 1.0f;

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ActionExecutionSystem::process_command(
            action_context,
            make_start_action_command(
                GS1_SITE_ACTION_PLANT,
                TileCoord {2, 2},
                1U,
                0U,
                gs1::k_item_wind_reed_seed_bundle)) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(context, site_run.site_action.current_action_id.has_value());
    const auto action_id = site_run.site_action.current_action_id->value;

    queue.clear();
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ActionExecutionSystem::process_command(
            action_context,
            make_command(
                GameCommandType::PlacementReservationAccepted,
                PlacementReservationAcceptedCommand {action_id, 2, 2, 90U})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, site_run.site_action.started_at_world_minute.has_value());

    queue.clear();
    for (int step = 0; step < 4; ++step)
    {
        SiteFlowSystem::run(flow_context);
        ActionExecutionSystem::run(action_context);
        GS1_SYSTEM_TEST_CHECK(context, site_run.site_action.current_action_id.has_value());
        GS1_SYSTEM_TEST_CHECK(context, count_commands(queue, GameCommandType::SiteActionCompleted) == 0U);
    }

    queue.clear();
    SiteFlowSystem::run(flow_context);
    ActionExecutionSystem::run(action_context);
    GS1_SYSTEM_TEST_CHECK(context, !site_run.site_action.current_action_id.has_value());
    GS1_SYSTEM_TEST_CHECK(context, count_commands(queue, GameCommandType::SiteActionCompleted) == 1U);
    GS1_SYSTEM_TEST_CHECK(context, count_commands(queue, GameCommandType::SiteTilePlantingCompleted) == 1U);
}

void placement_validation_accepts_releases_and_rejects_reserved_tiles(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 601U);
    GameCommandQueue queue {};
    auto site_context = make_site_context<PlacementValidationSystem>(campaign, site_run, queue);

    const auto request = make_command(
        GameCommandType::PlacementReservationRequested,
        gs1::PlacementReservationRequestedCommand {
            11U,
            2,
            2,
            PlacementOccupancyLayer::GroundCover,
            {0U, 0U, 0U},
            1U});

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        PlacementValidationSystem::process_command(site_context, request) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(context, queue.size() == 1U);
    GS1_SYSTEM_TEST_CHECK(context, queue.front().type == GameCommandType::PlacementReservationAccepted);
    const auto token = queue.front().payload_as<PlacementReservationAcceptedCommand>().reservation_token;

    queue.clear();
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        PlacementValidationSystem::process_command(
            site_context,
            make_command(
                GameCommandType::PlacementReservationRequested,
                gs1::PlacementReservationRequestedCommand {
                    12U,
                    2,
                    2,
                    PlacementOccupancyLayer::GroundCover,
                    {0U, 0U, 0U},
                    1U})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(context, queue.size() == 1U);
    GS1_SYSTEM_TEST_CHECK(context, queue.front().type == GameCommandType::PlacementReservationRejected);
    GS1_SYSTEM_TEST_CHECK(
        context,
        queue.front().payload_as<PlacementReservationRejectedCommand>().reason_code ==
            gs1::PlacementReservationRejectionReason::Reserved);

    queue.clear();
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        PlacementValidationSystem::process_command(
            site_context,
            make_command(
                GameCommandType::PlacementReservationReleased,
                gs1::PlacementReservationReleasedCommand {0U, token})) == GS1_STATUS_OK);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        PlacementValidationSystem::process_command(
            site_context,
            make_command(
                GameCommandType::PlacementReservationRequested,
                gs1::PlacementReservationRequestedCommand {
                    13U,
                    2,
                    2,
                    PlacementOccupancyLayer::GroundCover,
                    {0U, 0U, 0U},
                    1U})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, queue.back().type == GameCommandType::PlacementReservationAccepted);
}

void placement_validation_rejects_blocked_and_occupied_tiles(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 602U);
    GameCommandQueue queue {};
    auto site_context = make_site_context<PlacementValidationSystem>(campaign, site_run, queue);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        with_tile_component_mut<gs1::site_ecs::TilePlantable>(
            site_run,
            TileCoord {1, 1},
            [](auto& plantable) { plantable.value = false; }));
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        PlacementValidationSystem::process_command(
            site_context,
            make_command(
                GameCommandType::PlacementReservationRequested,
                gs1::PlacementReservationRequestedCommand {
                    21U,
                    1,
                    1,
                    PlacementOccupancyLayer::GroundCover,
                    {0U, 0U, 0U},
                    1U})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(context, queue.size() == 1U);
    GS1_SYSTEM_TEST_CHECK(
        context,
        queue.front().payload_as<PlacementReservationRejectedCommand>().reason_code ==
            gs1::PlacementReservationRejectionReason::TerrainBlocked);

    queue.clear();
    auto occupied_tile = site_run.site_world->tile_at(TileCoord {3, 3});
    occupied_tile.ecology.ground_cover_type_id = 9U;
    site_run.site_world->set_tile(TileCoord {3, 3}, occupied_tile);
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        PlacementValidationSystem::process_command(
            site_context,
            make_command(
                GameCommandType::PlacementReservationRequested,
                gs1::PlacementReservationRequestedCommand {
                    22U,
                    3,
                    3,
                    PlacementOccupancyLayer::GroundCover,
                    {0U, 0U, 0U},
                    1U})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(context, queue.size() == 1U);
    GS1_SYSTEM_TEST_CHECK(
        context,
        queue.front().payload_as<PlacementReservationRejectedCommand>().reason_code ==
            gs1::PlacementReservationRejectionReason::Occupied);
}

void placement_validation_structure_layer_handles_blocked_occupied_and_valid_tiles(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 603U);
    GameCommandQueue queue {};
    auto site_context = make_site_context<PlacementValidationSystem>(campaign, site_run, queue);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        with_tile_component_mut<gs1::site_ecs::TileTraversable>(
            site_run,
            TileCoord {5, 5},
            [](auto& traversable) { traversable.value = false; }));
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        PlacementValidationSystem::process_command(
            site_context,
            make_command(
                GameCommandType::PlacementReservationRequested,
                gs1::PlacementReservationRequestedCommand {
                    31U,
                    5,
                    5,
                    PlacementOccupancyLayer::Structure,
                    {0U, 0U, 0U},
                    1U})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(context, queue.size() == 1U);
    GS1_SYSTEM_TEST_CHECK(
        context,
        queue.front().payload_as<PlacementReservationRejectedCommand>().reason_code ==
            gs1::PlacementReservationRejectionReason::TerrainBlocked);

    queue.clear();
    auto occupied = site_run.site_world->tile_at(TileCoord {6, 5});
    occupied.device.structure_id = gs1::StructureId {33U};
    site_run.site_world->set_tile(TileCoord {6, 5}, occupied);
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        PlacementValidationSystem::process_command(
            site_context,
            make_command(
                GameCommandType::PlacementReservationRequested,
                gs1::PlacementReservationRequestedCommand {
                    32U,
                    6,
                    5,
                    PlacementOccupancyLayer::Structure,
                    {0U, 0U, 0U},
                    1U})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(context, queue.size() == 1U);
    GS1_SYSTEM_TEST_CHECK(
        context,
        queue.front().payload_as<PlacementReservationRejectedCommand>().reason_code ==
            gs1::PlacementReservationRejectionReason::Occupied);

    queue.clear();
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        PlacementValidationSystem::process_command(
            site_context,
            make_command(
                GameCommandType::PlacementReservationRequested,
                gs1::PlacementReservationRequestedCommand {
                    33U,
                    1,
                    5,
                    PlacementOccupancyLayer::Structure,
                    {0U, 0U, 0U},
                    1U})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(context, queue.size() == 1U);
    GS1_SYSTEM_TEST_CHECK(context, queue.front().type == GameCommandType::PlacementReservationAccepted);
}

void ecology_ground_cover_and_planting_update_tile_state(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 701U);
    GameCommandQueue queue {};
    auto site_context = make_site_context<EcologySystem>(campaign, site_run, queue);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        EcologySystem::process_command(
            site_context,
            make_command(
                GameCommandType::SiteGroundCoverPlaced,
                SiteGroundCoverPlacedCommand {1U, 2, 2, 5U, 0.4f, 0U})) == GS1_STATUS_OK);
    auto tile = site_run.site_world->tile_at(TileCoord {2, 2});
    GS1_SYSTEM_TEST_CHECK(context, tile.ecology.ground_cover_type_id == 5U);
    GS1_SYSTEM_TEST_CHECK(context, tile.ecology.plant_id.value == 0U);
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(tile.ecology.plant_density, 0.4f));
    GS1_SYSTEM_TEST_REQUIRE(context, queue.size() == 1U);
    GS1_SYSTEM_TEST_CHECK(context, queue.front().type == GameCommandType::TileEcologyChanged);
    GS1_SYSTEM_TEST_CHECK(
        context,
        (queue.front().payload_as<TileEcologyChangedCommand>().changed_mask &
            gs1::TILE_ECOLOGY_CHANGED_OCCUPANCY) != 0U);

    queue.clear();
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        EcologySystem::process_command(
            site_context,
            make_command(
                GameCommandType::SiteTilePlantingCompleted,
                SiteTilePlantingCompletedCommand {2U, 2, 2, 8U, 0.6f, 0U})) == GS1_STATUS_OK);
    tile = site_run.site_world->tile_at(TileCoord {2, 2});
    GS1_SYSTEM_TEST_CHECK(context, tile.ecology.plant_id.value == 8U);
    GS1_SYSTEM_TEST_CHECK(context, tile.ecology.ground_cover_type_id == 0U);
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(tile.ecology.plant_density, 0.6f));
}

void ecology_watering_and_burial_clear_require_valid_targets(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 702U);
    GameCommandQueue queue {};
    auto site_context = make_site_context<EcologySystem>(campaign, site_run, queue);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        EcologySystem::process_command(
            site_context,
            make_command(
                GameCommandType::SiteTileWatered,
                SiteTileWateredCommand {1U, 1, 1, 1.0f, 0U})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, queue.empty());

    auto tile = site_run.site_world->tile_at(TileCoord {1, 1});
    tile.ecology.ground_cover_type_id = 3U;
    tile.ecology.plant_density = 0.2f;
    tile.ecology.moisture = 0.2f;
    tile.ecology.sand_burial = 0.8f;
    site_run.site_world->set_tile(TileCoord {1, 1}, tile);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        EcologySystem::process_command(
            site_context,
            make_command(
                GameCommandType::SiteTileWatered,
                SiteTileWateredCommand {2U, 1, 1, 1.5f, 0U})) == GS1_STATUS_OK);
    tile = site_run.site_world->tile_at(TileCoord {1, 1});
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(tile.ecology.moisture, 0.53f));
    GS1_SYSTEM_TEST_REQUIRE(context, queue.size() == 1U);
    GS1_SYSTEM_TEST_CHECK(
        context,
        queue.front().payload_as<TileEcologyChangedCommand>().changed_mask ==
            gs1::TILE_ECOLOGY_CHANGED_MOISTURE);

    queue.clear();
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        EcologySystem::process_command(
            site_context,
            make_command(
                GameCommandType::SiteTileBurialCleared,
                SiteTileBurialClearedCommand {3U, 1, 1, 0.3f, 0U})) == GS1_STATUS_OK);
    tile = site_run.site_world->tile_at(TileCoord {1, 1});
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(tile.ecology.sand_burial, 0.5f));
    GS1_SYSTEM_TEST_CHECK(
        context,
        queue.front().payload_as<TileEcologyChangedCommand>().changed_mask ==
            gs1::TILE_ECOLOGY_CHANGED_SAND_BURIAL);
}

void ecology_run_grows_occupied_tiles_and_updates_progress(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 703U);
    GameCommandQueue queue {};
    auto site_context = make_site_context<EcologySystem>(campaign, site_run, queue, 60.0);

    site_run.counters.site_completion_tile_threshold = 2U;
    auto occupied = site_run.site_world->tile_at(TileCoord {2, 2});
    occupied.ecology.plant_id = gs1::PlantId {7U};
    occupied.ecology.plant_density = 0.9f;
    site_run.site_world->set_tile(TileCoord {2, 2}, occupied);

    auto second = site_run.site_world->tile_at(TileCoord {3, 2});
    second.ecology.ground_cover_type_id = 5U;
    second.ecology.plant_density = 1.0f;
    site_run.site_world->set_tile(TileCoord {3, 2}, second);

    EcologySystem::run(site_context);

    occupied = site_run.site_world->tile_at(TileCoord {2, 2});
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(occupied.ecology.plant_density, 1.0f));
    GS1_SYSTEM_TEST_CHECK(context, site_run.counters.fully_grown_tile_count == 2U);
    GS1_SYSTEM_TEST_CHECK(context, count_commands(queue, GameCommandType::TileEcologyChanged) >= 1U);
    GS1_SYSTEM_TEST_CHECK(context, count_commands(queue, GameCommandType::RestorationProgressChanged) == 1U);
    const auto* progress =
        first_command_payload<gs1::RestorationProgressChangedCommand>(
            queue,
            GameCommandType::RestorationProgressChanged);
    GS1_SYSTEM_TEST_REQUIRE(context, progress != nullptr);
    GS1_SYSTEM_TEST_CHECK(context, progress->fully_grown_tile_count == 2U);
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(progress->normalized_progress, 1.0f));
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
    "water_starts_immediately_and_emits_cost",
    action_execution_water_starts_immediately_and_emits_cost);
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
    "rejection_and_cancel_clear_action_state",
    action_execution_rejection_and_cancel_clear_action_state);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "action_execution",
    "run_completes_actions_and_emits_fact_commands",
    action_execution_run_completes_actions_and_emits_fact_commands);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "action_execution",
    "plant_completion_emits_ground_cover_after_reservation",
    action_execution_plant_completion_emits_ground_cover_after_reservation);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "action_execution",
    "item_based_plant_completion_consumes_seed_and_emits_planting",
    action_execution_item_based_plant_completion_consumes_seed_and_emits_planting);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "action_execution",
    "plant_progress_tracks_world_time",
    action_execution_plant_progress_tracks_world_time);
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
    "ecology",
    "ground_cover_and_planting_update_tile_state",
    ecology_ground_cover_and_planting_update_tile_state);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "ecology",
    "watering_and_burial_clear_require_valid_targets",
    ecology_watering_and_burial_clear_require_valid_targets);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "ecology",
    "run_grows_occupied_tiles_and_updates_progress",
    ecology_run_grows_occupied_tiles_and_updates_progress);
