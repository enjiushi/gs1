#include "messages/game_message.h"
#include "content/defs/craft_recipe_defs.h"
#include "content/defs/item_defs.h"
#include "site/craft_logic.h"
#include "site/inventory_storage.h"
#include "site/systems/action_execution_system.h"
#include "site/systems/device_maintenance_system.h"
#include "site/systems/craft_system.h"
#include "site/systems/inventory_system.h"
#include "testing/system_test_registry.h"
#include "system_test_fixtures.h"

namespace
{
using gs1::ActionExecutionSystem;
using gs1::CraftSystem;
using gs1::DeviceMaintenanceSystem;
using gs1::GameMessage;
using gs1::GameMessageQueue;
using gs1::GameMessageType;
using gs1::InventoryCraftCommitRequestedMessage;
using gs1::InventoryItemConsumeRequestedMessage;
using gs1::InventorySystem;
using gs1::PlacementReservationAcceptedMessage;
using gs1::SiteActionCompletedMessage;
using gs1::SiteActionStartedMessage;
using gs1::SiteDevicePlacedMessage;
using gs1::SiteRunStartedMessage;
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

void action_execution_build_completion_consumes_deployable_and_emits_device_placement(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 1501U);
    GameMessageQueue queue {};
    auto inventory_context = make_site_context<InventorySystem>(campaign, site_run, queue);
    auto action_context = make_site_context<ActionExecutionSystem>(campaign, site_run, queue, 60.0);

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
        gs1::ItemId {gs1::k_item_storage_crate_kit},
        1U);

    queue.clear();
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ActionExecutionSystem::process_message(
            action_context,
            make_message(
                GameMessageType::StartSiteAction,
                gs1::StartSiteActionMessage {
                    GS1_SITE_ACTION_BUILD,
                    0U,
                    1U,
                    site_run.camp.camp_anchor_tile.x,
                    site_run.camp.camp_anchor_tile.y,
                    0U,
                    0U,
                    gs1::k_item_storage_crate_kit})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(context, count_messages(queue, GameMessageType::PlacementReservationRequested) == 1U);

    queue.clear();
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ActionExecutionSystem::process_message(
            action_context,
            make_message(
                GameMessageType::PlacementReservationAccepted,
                PlacementReservationAcceptedMessage {
                    site_run.site_action.current_action_id->value,
                    site_run.camp.camp_anchor_tile.x,
                    site_run.camp.camp_anchor_tile.y,
                    55U})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(context, count_messages(queue, GameMessageType::SiteActionStarted) == 1U);

    queue.clear();
    ActionExecutionSystem::run(action_context);
    GS1_SYSTEM_TEST_REQUIRE(context, count_messages(queue, GameMessageType::SiteActionCompleted) == 1U);
    GS1_SYSTEM_TEST_REQUIRE(context, count_messages(queue, GameMessageType::InventoryItemConsumeRequested) == 1U);
    GS1_SYSTEM_TEST_REQUIRE(context, count_messages(queue, GameMessageType::SiteDevicePlaced) == 1U);
    GS1_SYSTEM_TEST_CHECK(
        context,
        first_message_payload<InventoryItemConsumeRequestedMessage>(
            queue,
            GameMessageType::InventoryItemConsumeRequested)->item_id == gs1::k_item_storage_crate_kit);
    GS1_SYSTEM_TEST_CHECK(
        context,
        first_message_payload<SiteDevicePlacedMessage>(
            queue,
            GameMessageType::SiteDevicePlaced)->structure_id == gs1::k_structure_storage_crate);
}

void inventory_craft_commit_consumes_nearby_ingredients_and_outputs_to_device_storage(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 1502U);
    GameMessageQueue queue {};
    auto inventory_context = make_site_context<InventorySystem>(campaign, site_run, queue);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        InventorySystem::process_message(
            inventory_context,
            make_message(
                GameMessageType::SiteRunStarted,
                SiteRunStartedMessage {1U, 1U, 101U, 1U, 42ULL})) == GS1_STATUS_OK);

    const auto workbench_tile = default_starter_workbench_tile(site_run.camp.camp_anchor_tile);
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        InventorySystem::process_message(
            inventory_context,
            make_message(
                GameMessageType::InventoryCraftCommitRequested,
                InventoryCraftCommitRequestedMessage {
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
            gs1::inventory_storage::starter_storage_container(site_run),
            gs1::ItemId {gs1::k_item_wood_bundle}) == 3U);
    GS1_SYSTEM_TEST_CHECK(
        context,
        gs1::inventory_storage::available_item_quantity_in_container(
            site_run,
            gs1::inventory_storage::starter_storage_container(site_run),
            gs1::ItemId {gs1::k_item_iron_bundle}) == 2U);
}

void craft_cache_tracks_worker_pack_membership_by_distance(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 1503U, 101U, 16U, 16U);
    GameMessageQueue queue {};
    auto inventory_context = make_site_context<InventorySystem>(campaign, site_run, queue);
    auto craft_context = make_site_context<CraftSystem>(campaign, site_run, queue);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        InventorySystem::process_message(
            inventory_context,
            make_message(
                GameMessageType::SiteRunStarted,
                SiteRunStartedMessage {1U, 1U, 101U, 1U, 42ULL})) == GS1_STATUS_OK);

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

void dynamically_placed_storage_device_reuses_single_inventory_container(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 1504U);
    GameMessageQueue queue {};
    auto inventory_context = make_site_context<InventorySystem>(campaign, site_run, queue);
    auto maintenance_context = make_site_context<DeviceMaintenanceSystem>(campaign, site_run, queue);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        InventorySystem::process_message(
            inventory_context,
            make_message(
                GameMessageType::SiteRunStarted,
                SiteRunStartedMessage {1U, 1U, 101U, 1U, 42ULL})) == GS1_STATUS_OK);

    const TileCoord target_tile {site_run.camp.camp_anchor_tile.x, site_run.camp.camp_anchor_tile.y - 1};
    GS1_SYSTEM_TEST_REQUIRE(context, site_run.site_world->contains(target_tile));
    GS1_SYSTEM_TEST_CHECK(context, site_run.site_world->tile_device(target_tile).structure_id.value == 0U);

    const auto worker_pack = gs1::inventory_storage::worker_pack_container(site_run);
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        gs1::inventory_storage::add_item_to_container(
            site_run,
            worker_pack,
            gs1::ItemId {gs1::k_item_storage_crate_kit},
            1U) == 0U);

    std::uint64_t consumed_item_entity_id = 0U;
    const auto slot_count = gs1::inventory_storage::slot_count_in_container(site_run, worker_pack);
    for (std::uint32_t slot_index = 0U; slot_index < slot_count; ++slot_index)
    {
        const auto item = gs1::inventory_storage::item_entity_for_slot(site_run, worker_pack, slot_index);
        const auto* stack = gs1::inventory_storage::stack_data(site_run, item);
        if (stack != nullptr && stack->item_id.value == gs1::k_item_storage_crate_kit)
        {
            consumed_item_entity_id = item.id();
            break;
        }
    }
    GS1_SYSTEM_TEST_REQUIRE(context, consumed_item_entity_id != 0U);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        InventorySystem::process_message(
            inventory_context,
            make_message(
                GameMessageType::InventoryItemConsumeRequested,
                InventoryItemConsumeRequestedMessage {
                    gs1::k_item_storage_crate_kit,
                    1U,
                    GS1_INVENTORY_CONTAINER_WORKER_PACK,
                    0U})) == GS1_STATUS_OK);

    const auto placed_message = make_message(
        GameMessageType::SiteDevicePlaced,
        SiteDevicePlacedMessage {
            1U,
            target_tile.x,
            target_tile.y,
            gs1::k_structure_storage_crate,
            0U});

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        DeviceMaintenanceSystem::process_message(maintenance_context, placed_message) == GS1_STATUS_OK);
    const auto placed_device_entity_id = site_run.site_world->device_entity_id(target_tile);
    GS1_SYSTEM_TEST_REQUIRE(context, placed_device_entity_id != 0U);
    GS1_SYSTEM_TEST_CHECK(
        context,
        static_cast<std::uint32_t>(placed_device_entity_id) ==
            static_cast<std::uint32_t>(consumed_item_entity_id));

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        InventorySystem::process_message(inventory_context, placed_message) == GS1_STATUS_OK);

    InventorySystem::run(inventory_context);
    InventorySystem::run(inventory_context);

    std::size_t matching_storage_count = 0U;
    for (const auto& storage : site_run.inventory.storage_containers)
    {
        if (storage.container_kind != GS1_INVENTORY_CONTAINER_DEVICE_STORAGE)
        {
            continue;
        }
        if (storage.tile_coord.x == target_tile.x && storage.tile_coord.y == target_tile.y)
        {
            matching_storage_count += 1U;
        }
    }

    GS1_SYSTEM_TEST_CHECK(context, matching_storage_count == 1U);
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
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "inventory",
    "dynamically_placed_storage_device_reuses_single_inventory_container",
    dynamically_placed_storage_device_reuses_single_inventory_container);
