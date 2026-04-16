#include "commands/game_command.h"
#include "content/defs/craft_recipe_defs.h"
#include "content/defs/item_defs.h"
#include "site/craft_logic.h"
#include "site/inventory_storage.h"
#include "site/systems/action_execution_system.h"
#include "site/systems/craft_system.h"
#include "site/systems/inventory_system.h"
#include "testing/system_test_registry.h"
#include "system_test_fixtures.h"

namespace
{
using gs1::ActionExecutionSystem;
using gs1::CraftSystem;
using gs1::GameCommand;
using gs1::GameCommandQueue;
using gs1::GameCommandType;
using gs1::InventoryCraftCommitRequestedCommand;
using gs1::InventoryItemConsumeRequestedCommand;
using gs1::InventorySystem;
using gs1::PlacementReservationAcceptedCommand;
using gs1::SiteActionCompletedCommand;
using gs1::SiteActionStartedCommand;
using gs1::SiteDevicePlacedCommand;
using gs1::SiteRunStartedCommand;
using gs1::TileCoord;
using namespace gs1::testing::fixtures;

template <typename Payload>
GameCommand make_command(gs1::GameCommandType type, const Payload& payload)
{
    GameCommand command {};
    command.type = type;
    command.set_payload(payload);
    return command;
}

void action_execution_build_completion_consumes_deployable_and_emits_device_placement(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 1501U);
    GameCommandQueue queue {};
    auto inventory_context = make_site_context<InventorySystem>(campaign, site_run, queue);
    auto action_context = make_site_context<ActionExecutionSystem>(campaign, site_run, queue, 60.0);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        InventorySystem::process_command(
            inventory_context,
            make_command(
                GameCommandType::SiteRunStarted,
                SiteRunStartedCommand {1U, 1U, 101U, 1U, 42ULL})) == GS1_STATUS_OK);
    (void)gs1::inventory_storage::add_item_to_container(
        site_run,
        gs1::inventory_storage::worker_pack_container(site_run),
        gs1::ItemId {gs1::k_item_storage_crate_kit},
        1U);

    queue.clear();
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ActionExecutionSystem::process_command(
            action_context,
            make_command(
                GameCommandType::StartSiteAction,
                gs1::StartSiteActionCommand {
                    GS1_SITE_ACTION_BUILD,
                    0U,
                    1U,
                    site_run.camp.camp_anchor_tile.x,
                    site_run.camp.camp_anchor_tile.y,
                    0U,
                    0U,
                    gs1::k_item_storage_crate_kit})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(context, count_commands(queue, GameCommandType::PlacementReservationRequested) == 1U);

    queue.clear();
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ActionExecutionSystem::process_command(
            action_context,
            make_command(
                GameCommandType::PlacementReservationAccepted,
                PlacementReservationAcceptedCommand {
                    site_run.site_action.current_action_id->value,
                    site_run.camp.camp_anchor_tile.x,
                    site_run.camp.camp_anchor_tile.y,
                    55U})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(context, count_commands(queue, GameCommandType::SiteActionStarted) == 1U);

    queue.clear();
    ActionExecutionSystem::run(action_context);
    GS1_SYSTEM_TEST_REQUIRE(context, count_commands(queue, GameCommandType::SiteActionCompleted) == 1U);
    GS1_SYSTEM_TEST_REQUIRE(context, count_commands(queue, GameCommandType::InventoryItemConsumeRequested) == 1U);
    GS1_SYSTEM_TEST_REQUIRE(context, count_commands(queue, GameCommandType::SiteDevicePlaced) == 1U);
    GS1_SYSTEM_TEST_CHECK(
        context,
        first_command_payload<InventoryItemConsumeRequestedCommand>(
            queue,
            GameCommandType::InventoryItemConsumeRequested)->item_id == gs1::k_item_storage_crate_kit);
    GS1_SYSTEM_TEST_CHECK(
        context,
        first_command_payload<SiteDevicePlacedCommand>(
            queue,
            GameCommandType::SiteDevicePlaced)->structure_id == gs1::k_structure_storage_crate);
}

void inventory_craft_commit_consumes_nearby_ingredients_and_outputs_to_device_storage(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 1502U);
    GameCommandQueue queue {};
    auto inventory_context = make_site_context<InventorySystem>(campaign, site_run, queue);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        InventorySystem::process_command(
            inventory_context,
            make_command(
                GameCommandType::SiteRunStarted,
                SiteRunStartedCommand {1U, 1U, 101U, 1U, 42ULL})) == GS1_STATUS_OK);

    const auto workbench_tile = default_starter_workbench_tile(site_run.camp.camp_anchor_tile);
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        InventorySystem::process_command(
            inventory_context,
            make_command(
                GameCommandType::InventoryCraftCommitRequested,
                InventoryCraftCommitRequestedCommand {
                    gs1::k_recipe_craft_storage_crate,
                    workbench_tile.x,
                    workbench_tile.y,
                    0U})) == GS1_STATUS_OK);

    const auto workbench_entity_id = site_run.site_world->device_entity_id(workbench_tile);
    const auto workbench_storage =
        gs1::inventory_storage::find_device_storage_container(site_run, workbench_entity_id);
    GS1_SYSTEM_TEST_CHECK(
        context,
        gs1::inventory_storage::available_item_quantity_in_container(
            site_run,
            workbench_storage,
            gs1::ItemId {gs1::k_item_storage_crate_kit}) == 1U);
    GS1_SYSTEM_TEST_CHECK(
        context,
        gs1::inventory_storage::available_item_quantity_in_container(
            site_run,
            gs1::inventory_storage::camp_storage_container(site_run),
            gs1::ItemId {gs1::k_item_wood_bundle}) == 3U);
    GS1_SYSTEM_TEST_CHECK(
        context,
        gs1::inventory_storage::available_item_quantity_in_container(
            site_run,
            gs1::inventory_storage::camp_storage_container(site_run),
            gs1::ItemId {gs1::k_item_iron_bundle}) == 2U);
}

void craft_cache_tracks_worker_pack_membership_by_distance(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 1503U, 101U, 16U, 16U);
    GameCommandQueue queue {};
    auto inventory_context = make_site_context<InventorySystem>(campaign, site_run, queue);
    auto craft_context = make_site_context<CraftSystem>(campaign, site_run, queue);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        InventorySystem::process_command(
            inventory_context,
            make_command(
                GameCommandType::SiteRunStarted,
                SiteRunStartedCommand {1U, 1U, 101U, 1U, 42ULL})) == GS1_STATUS_OK);

    CraftSystem::run(craft_context);
    const auto workbench_tile = default_starter_workbench_tile(site_run.camp.camp_anchor_tile);
    const auto workbench_entity_id = site_run.site_world->device_entity_id(workbench_tile);
    const auto* initial_cache = gs1::craft_logic::find_device_cache(site_run.craft, workbench_entity_id);
    GS1_SYSTEM_TEST_REQUIRE(context, initial_cache != nullptr);
    GS1_SYSTEM_TEST_CHECK(context, initial_cache->worker_pack_included);
    GS1_SYSTEM_TEST_CHECK(
        context,
        gs1::craft_logic::available_cached_item_quantity(
            site_run,
            initial_cache->nearby_item_instance_ids,
            gs1::ItemId {gs1::k_item_water_container}) == 2U);

    auto worker = gs1::site_world_access::worker_position(site_run);
    worker.tile_coord = TileCoord {15, 15};
    worker.tile_x = 15.0f;
    worker.tile_y = 15.0f;
    gs1::site_world_access::set_worker_position(site_run, worker);

    CraftSystem::run(craft_context);
    const auto* far_cache = gs1::craft_logic::find_device_cache(site_run.craft, workbench_entity_id);
    GS1_SYSTEM_TEST_REQUIRE(context, far_cache != nullptr);
    GS1_SYSTEM_TEST_CHECK(context, !far_cache->worker_pack_included);
    GS1_SYSTEM_TEST_CHECK(
        context,
        gs1::craft_logic::available_cached_item_quantity(
            site_run,
            far_cache->nearby_item_instance_ids,
            gs1::ItemId {gs1::k_item_water_container}) == 0U);
}
}  // namespace

GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "action_execution",
    "build_completion_consumes_deployable_and_emits_device_placement",
    action_execution_build_completion_consumes_deployable_and_emits_device_placement);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "inventory",
    "craft_commit_consumes_nearby_ingredients_and_outputs_to_device_storage",
    inventory_craft_commit_consumes_nearby_ingredients_and_outputs_to_device_storage);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "craft",
    "cache_tracks_worker_pack_membership_by_distance",
    craft_cache_tracks_worker_pack_membership_by_distance);
