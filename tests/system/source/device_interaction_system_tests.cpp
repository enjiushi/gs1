#include "content/defs/item_defs.h"
#include "messages/game_message.h"
#include "site/inventory_storage.h"
#include "site/systems/action_execution_system.h"
#include "site/systems/inventory_system.h"
#include "site/systems/site_flow_system.h"
#include "testing/system_test_registry.h"
#include "system_test_fixtures.h"

namespace
{
using gs1::ActionExecutionSystem;
using gs1::GameMessage;
using gs1::GameMessageQueue;
using gs1::GameMessageType;
using gs1::InventoryItemUseRequestedMessage;
using gs1::InventoryStorageViewRequestMessage;
using gs1::InventorySystem;
using gs1::SiteMoveDirectionInput;
using gs1::SiteRunStartedMessage;
using gs1::SiteFlowSystem;
using gs1::StartSiteActionMessage;
using gs1::TileCoord;
using namespace gs1::testing::fixtures;

template <typename Payload>
GameMessage make_message(gs1::GameMessageType type, const Payload& payload)
{
    GameMessage message {};
    message.type = type;
    message.set_payload(payload);
    return message;
}

void inventory_storage_open_waits_for_worker_to_reach_device_range(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 1601U, 101U, 10U, 10U, TileCoord {2, 2}, TileCoord {7, 7});
    GameMessageQueue queue {};
    auto inventory_context = make_site_context<InventorySystem>(campaign, site_run, queue);
    auto flow_context = make_site_context<SiteFlowSystem>(campaign, site_run, queue);
    InventorySystem::run(inventory_context);

    const auto storage_container = gs1::inventory_storage::starter_storage_container(site_run);
    const auto storage_id = gs1::inventory_storage::storage_id_for_container(site_run, storage_container);
    GS1_SYSTEM_TEST_REQUIRE(context, storage_id != 0U);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        InventorySystem::process_message(
            inventory_context,
            make_message(
                GameMessageType::InventoryStorageViewRequest,
                InventoryStorageViewRequestMessage {
                    storage_id,
                    GS1_INVENTORY_VIEW_EVENT_OPEN_SNAPSHOT,
                    {0U, 0U, 0U}})) == GS1_STATUS_OK);

    GS1_SYSTEM_TEST_CHECK(context, site_run.inventory.opened_device_storage_id == 0U);
    GS1_SYSTEM_TEST_CHECK(context, site_run.inventory.pending_device_storage_open.active);
    GS1_SYSTEM_TEST_CHECK(context, site_run.inventory.pending_device_storage_open.storage_id == storage_id);

    const auto expected_approach_tile = site_run.inventory.pending_device_storage_open.approach_tile;
    for (int step = 0; step < 12; ++step)
    {
        SiteFlowSystem::run(flow_context);
        InventorySystem::run(inventory_context);
        if (site_run.inventory.opened_device_storage_id == storage_id)
        {
            break;
        }
    }

    GS1_SYSTEM_TEST_CHECK(context, site_run.inventory.opened_device_storage_id == storage_id);
    GS1_SYSTEM_TEST_CHECK(context, !site_run.inventory.pending_device_storage_open.active);
    GS1_SYSTEM_TEST_CHECK(
        context,
        site_run.site_world->worker().position.tile_coord.x == expected_approach_tile.x);
    GS1_SYSTEM_TEST_CHECK(
        context,
        site_run.site_world->worker().position.tile_coord.y == expected_approach_tile.y);
}

void opened_storage_closes_when_worker_leaves_device_range(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 1605U, 101U, 10U, 10U, TileCoord {2, 2}, TileCoord {2, 2});
    GameMessageQueue queue {};
    auto inventory_context = make_site_context<InventorySystem>(campaign, site_run, queue);
    auto flow_context = make_site_context<SiteFlowSystem>(
        campaign,
        site_run,
        queue,
        k_default_fixed_step_seconds,
        SiteMoveDirectionInput {1.0f, 0.0f, 0.0f, true});
    InventorySystem::run(inventory_context);

    const auto storage_container = gs1::inventory_storage::starter_storage_container(site_run);
    const auto storage_id = gs1::inventory_storage::storage_id_for_container(site_run, storage_container);
    GS1_SYSTEM_TEST_REQUIRE(context, storage_id != 0U);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        InventorySystem::process_message(
            inventory_context,
            make_message(
                GameMessageType::InventoryStorageViewRequest,
                InventoryStorageViewRequestMessage {
                    storage_id,
                    GS1_INVENTORY_VIEW_EVENT_OPEN_SNAPSHOT,
                    {0U, 0U, 0U}})) == GS1_STATUS_OK);

    GS1_SYSTEM_TEST_REQUIRE(context, site_run.inventory.opened_device_storage_id == storage_id);
    GS1_SYSTEM_TEST_CHECK(context, !site_run.inventory.pending_device_storage_open.active);

    SiteFlowSystem::run(flow_context);
    InventorySystem::run(inventory_context);

    GS1_SYSTEM_TEST_CHECK(context, site_run.inventory.opened_device_storage_id == 0U);
    GS1_SYSTEM_TEST_CHECK(context, site_run.pending_inventory_view_state_projection_update);
}

void medicine_item_use_keeps_pending_storage_open_chain(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 1603U, 101U, 10U, 10U, TileCoord {2, 2}, TileCoord {7, 7});
    GameMessageQueue queue {};
    auto inventory_context = make_site_context<InventorySystem>(campaign, site_run, queue);
    auto flow_context = make_site_context<SiteFlowSystem>(campaign, site_run, queue);
    InventorySystem::run(inventory_context);

    site_run.inventory.worker_pack_slots[4].occupied = true;
    site_run.inventory.worker_pack_slots[4].item_id = gs1::ItemId {gs1::k_item_medicine_pack};
    site_run.inventory.worker_pack_slots[4].item_quantity = 1U;
    site_run.inventory.worker_pack_slots[4].item_condition = 1.0f;
    site_run.inventory.worker_pack_slots[4].item_freshness = 1.0f;

    const auto storage_container = gs1::inventory_storage::starter_storage_container(site_run);
    const auto storage_id = gs1::inventory_storage::storage_id_for_container(site_run, storage_container);
    GS1_SYSTEM_TEST_REQUIRE(context, storage_id != 0U);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        InventorySystem::process_message(
            inventory_context,
            make_message(
                GameMessageType::InventoryStorageViewRequest,
                InventoryStorageViewRequestMessage {
                    storage_id,
                    GS1_INVENTORY_VIEW_EVENT_OPEN_SNAPSHOT,
                    {0U, 0U, 0U}})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(context, site_run.inventory.pending_device_storage_open.active);

    queue.clear();
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        InventorySystem::process_message(
            inventory_context,
            make_message(
                GameMessageType::InventoryItemUseRequested,
                InventoryItemUseRequestedMessage {
                    gs1::k_item_medicine_pack,
                    site_run.inventory.worker_pack_storage_id,
                    1U,
                    4U})) == GS1_STATUS_OK);

    GS1_SYSTEM_TEST_CHECK(context, site_run.inventory.pending_device_storage_open.active);
    GS1_SYSTEM_TEST_CHECK(context, count_messages(queue, GameMessageType::StartSiteAction) == 0U);

    for (int step = 0; step < 12; ++step)
    {
        SiteFlowSystem::run(flow_context);
        InventorySystem::run(inventory_context);
        if (site_run.inventory.opened_device_storage_id == storage_id)
        {
            break;
        }
    }

    GS1_SYSTEM_TEST_CHECK(context, site_run.inventory.opened_device_storage_id == storage_id);
}

void drink_action_breaks_pending_storage_open_chain(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 1604U, 101U, 10U, 10U, TileCoord {2, 2}, TileCoord {7, 7});
    GameMessageQueue queue {};
    auto inventory_context = make_site_context<InventorySystem>(campaign, site_run, queue);
    auto action_context = make_site_context<ActionExecutionSystem>(campaign, site_run, queue);
    auto flow_context = make_site_context<SiteFlowSystem>(campaign, site_run, queue);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        InventorySystem::process_message(
            inventory_context,
            make_message(
                GameMessageType::SiteRunStarted,
                SiteRunStartedMessage {1U, 1U, 101U, 1U, 42ULL})) == GS1_STATUS_OK);
    (void)gs1::inventory_storage::add_item_to_container(
        site_run,
        gs1::inventory_storage::worker_pack_container(site_run),
        gs1::ItemId {gs1::k_item_water_container},
        1U);

    const auto storage_container = gs1::inventory_storage::starter_storage_container(site_run);
    const auto storage_id = gs1::inventory_storage::storage_id_for_container(site_run, storage_container);
    GS1_SYSTEM_TEST_REQUIRE(context, storage_id != 0U);

    queue.clear();
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        InventorySystem::process_message(
            inventory_context,
            make_message(
                GameMessageType::InventoryStorageViewRequest,
                InventoryStorageViewRequestMessage {
                    storage_id,
                    GS1_INVENTORY_VIEW_EVENT_OPEN_SNAPSHOT,
                    {0U, 0U, 0U}})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(context, site_run.inventory.pending_device_storage_open.active);

    queue.clear();
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        InventorySystem::process_message(
            inventory_context,
            make_message(
                GameMessageType::InventoryItemUseRequested,
                InventoryItemUseRequestedMessage {
                    gs1::k_item_water_container,
                    site_run.inventory.worker_pack_storage_id,
                    1U,
                    0U})) == GS1_STATUS_OK);

    GS1_SYSTEM_TEST_REQUIRE(context, count_messages(queue, GameMessageType::StartSiteAction) == 1U);
    const auto start_action_message = *first_message(queue, GameMessageType::StartSiteAction);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ActionExecutionSystem::process_message(action_context, start_action_message) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        InventorySystem::process_message(inventory_context, start_action_message) == GS1_STATUS_OK);

    GS1_SYSTEM_TEST_CHECK(context, !site_run.inventory.pending_device_storage_open.active);
    GS1_SYSTEM_TEST_CHECK(context, site_run.inventory.opened_device_storage_id == 0U);

    for (int step = 0; step < 12; ++step)
    {
        SiteFlowSystem::run(flow_context);
        InventorySystem::run(inventory_context);
    }

    GS1_SYSTEM_TEST_CHECK(context, site_run.inventory.opened_device_storage_id == 0U);
}

void craft_action_waits_for_worker_to_reach_device_range_before_starting(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 1602U, 101U, 12U, 12U, TileCoord {2, 2}, TileCoord {10, 10});
    GameMessageQueue queue {};
    auto inventory_context = make_site_context<InventorySystem>(campaign, site_run, queue);
    auto action_context = make_site_context<ActionExecutionSystem>(campaign, site_run, queue);
    auto flow_context = make_site_context<SiteFlowSystem>(campaign, site_run, queue);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        InventorySystem::process_message(
            inventory_context,
            make_message(
                GameMessageType::SiteRunStarted,
                SiteRunStartedMessage {1U, 1U, 101U, 1U, 42ULL})) == GS1_STATUS_OK);

    const auto workbench_tile = default_starter_workbench_tile(site_run.camp.camp_anchor_tile);
    const auto workbench_entity_id = site_run.site_world->device_entity_id(workbench_tile);
    const auto workbench_storage =
        gs1::inventory_storage::find_device_storage_container(site_run, workbench_entity_id);
    GS1_SYSTEM_TEST_REQUIRE(context, workbench_storage.is_valid());
    (void)gs1::inventory_storage::add_item_to_container(
        site_run,
        workbench_storage,
        gs1::ItemId {gs1::k_item_wood_bundle},
        5U);
    (void)gs1::inventory_storage::add_item_to_container(
        site_run,
        workbench_storage,
        gs1::ItemId {gs1::k_item_iron_bundle},
        3U);
    (void)gs1::inventory_storage::add_item_to_container(
        site_run,
        workbench_storage,
        gs1::ItemId {gs1::k_item_hammer},
        1U);

    queue.clear();
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ActionExecutionSystem::process_message(
            action_context,
            make_message(
                GameMessageType::StartSiteAction,
                StartSiteActionMessage {
                    GS1_SITE_ACTION_CRAFT,
                    0U,
                    1U,
                    workbench_tile.x,
                    workbench_tile.y,
                    0U,
                    0U,
                    gs1::k_item_workbench_kit})) == GS1_STATUS_OK);

    GS1_SYSTEM_TEST_REQUIRE(context, site_run.site_action.current_action_id.has_value());
    GS1_SYSTEM_TEST_CHECK(context, !site_run.site_action.started_at_world_minute.has_value());
    GS1_SYSTEM_TEST_CHECK(context, site_run.site_action.approach_tile.has_value());
    GS1_SYSTEM_TEST_CHECK(context, queue.empty());

    const auto expected_approach_tile = *site_run.site_action.approach_tile;
    for (int step = 0; step < 12; ++step)
    {
        SiteFlowSystem::run(flow_context);
        queue.clear();
        ActionExecutionSystem::run(action_context);
        if (site_run.site_action.started_at_world_minute.has_value())
        {
            break;
        }
    }

    GS1_SYSTEM_TEST_CHECK(context, site_run.site_action.started_at_world_minute.has_value());
    GS1_SYSTEM_TEST_REQUIRE(context, count_messages(queue, GameMessageType::SiteActionStarted) == 1U);
    GS1_SYSTEM_TEST_REQUIRE(context, count_messages(queue, GameMessageType::WorkerMeterDeltaRequested) == 0U);
    GS1_SYSTEM_TEST_CHECK(
        context,
        site_run.site_world->worker().position.tile_coord.x == expected_approach_tile.x);
    GS1_SYSTEM_TEST_CHECK(
        context,
        site_run.site_world->worker().position.tile_coord.y == expected_approach_tile.y);
}
}  // namespace

GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "device_interaction",
    "storage_open_waits_for_worker_to_reach_device_range",
    inventory_storage_open_waits_for_worker_to_reach_device_range);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "device_interaction",
    "opened_storage_closes_when_worker_leaves_device_range",
    opened_storage_closes_when_worker_leaves_device_range);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "device_interaction",
    "medicine_item_use_keeps_pending_storage_open_chain",
    medicine_item_use_keeps_pending_storage_open_chain);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "device_interaction",
    "drink_action_breaks_pending_storage_open_chain",
    drink_action_breaks_pending_storage_open_chain);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "device_interaction",
    "craft_action_waits_for_worker_to_reach_device_range_before_starting",
    craft_action_waits_for_worker_to_reach_device_range_before_starting);
