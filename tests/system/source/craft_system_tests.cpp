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

#include <limits>

namespace
{
using gs1::ActionExecutionSystem;
using gs1::CraftSystem;
using gs1::CraftContextRequestedMessage;
using gs1::DeviceMaintenanceSystem;
using gs1::GameMessage;
using gs1::GameMessageQueue;
using gs1::GameMessageType;
using gs1::InventoryCraftCommitRequestedMessage;
using gs1::InventoryItemConsumeRequestedMessage;
using gs1::InventorySystem;
using gs1::PlacementReservationAcceptedMessage;
using gs1::SiteActionCompletedMessage;
using gs1::SiteActionFailedMessage;
using gs1::SiteActionStartedMessage;
using gs1::SiteDeviceBrokenMessage;
using gs1::SiteDevicePlacedMessage;
using gs1::SiteDeviceRepairedMessage;
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

constexpr std::int32_t reputation_for_progress_tier(std::uint32_t tier_index) noexcept
{
    return static_cast<std::int32_t>(tier_index) * 200;
}

void action_execution_build_completion_consumes_deployable_and_emits_device_placement(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 1501U);
    GameMessageQueue queue {};
    auto inventory_context = make_site_context<InventorySystem>(campaign, site_run, queue);
    auto action_context = make_site_context<ActionExecutionSystem>(campaign, site_run, queue);

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
    const auto* started_message =
        first_message_payload<SiteActionStartedMessage>(queue, GameMessageType::SiteActionStarted);
    GS1_SYSTEM_TEST_REQUIRE(context, started_message != nullptr);
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(started_message->duration_minutes, 8.0f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(site_run.site_action.total_action_minutes, 8.0));

    queue.clear();
    for (int step = 0; step < 9; ++step)
    {
        ActionExecutionSystem::run(action_context);
    }
    GS1_SYSTEM_TEST_CHECK(context, count_messages(queue, GameMessageType::SiteActionCompleted) == 0U);
    GS1_SYSTEM_TEST_CHECK(context, site_run.site_action.remaining_action_minutes > 0.0);
    GS1_SYSTEM_TEST_CHECK(context, site_run.site_action.remaining_action_minutes <= 0.81);

    ActionExecutionSystem::run(action_context);
    if (count_messages(queue, GameMessageType::SiteActionCompleted) == 0U)
    {
        ActionExecutionSystem::run(action_context);
    }
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

void action_execution_craft_requires_hammer_for_shovel_recipe(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 1513U);
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

    const auto delivery_box = gs1::inventory_storage::delivery_box_container(site_run);
    (void)gs1::inventory_storage::add_item_to_container(
        site_run,
        delivery_box,
        gs1::ItemId {gs1::k_item_wood_bundle},
        2U);
    (void)gs1::inventory_storage::add_item_to_container(
        site_run,
        delivery_box,
        gs1::ItemId {gs1::k_item_iron_bundle},
        1U);

    const auto workbench_tile = default_starter_workbench_tile(site_run.camp.camp_anchor_tile);
    queue.clear();
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ActionExecutionSystem::process_message(
            action_context,
            make_message(
                GameMessageType::StartSiteAction,
                gs1::StartSiteActionMessage {
                    GS1_SITE_ACTION_CRAFT,
                    4U,
                    1U,
                    workbench_tile.x,
                    workbench_tile.y,
                    0U,
                    0U,
                    gs1::k_item_shovel})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(context, count_messages(queue, GameMessageType::SiteActionFailed) == 1U);
    GS1_SYSTEM_TEST_CHECK(
        context,
        first_message_payload<SiteActionFailedMessage>(
            queue,
            GameMessageType::SiteActionFailed)->reason == gs1::SiteActionFailureReason::InsufficientResources);
    GS1_SYSTEM_TEST_CHECK(context, !site_run.site_action.current_action_id.has_value());

    (void)gs1::inventory_storage::add_item_to_container(
        site_run,
        delivery_box,
        gs1::ItemId {gs1::k_item_hammer},
        1U);
    queue.clear();
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ActionExecutionSystem::process_message(
            action_context,
            make_message(
                GameMessageType::StartSiteAction,
                gs1::StartSiteActionMessage {
                    GS1_SITE_ACTION_CRAFT,
                    4U,
                    1U,
                    workbench_tile.x,
                    workbench_tile.y,
                    0U,
                    0U,
                    gs1::k_item_shovel})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, count_messages(queue, GameMessageType::SiteActionFailed) == 0U);
    GS1_SYSTEM_TEST_CHECK(context, site_run.site_action.current_action_id.has_value());
    GS1_SYSTEM_TEST_CHECK(context, site_run.site_action.action_kind == gs1::ActionKind::Craft);
    GS1_SYSTEM_TEST_CHECK(context, site_run.site_action.secondary_subject_id == gs1::k_recipe_craft_shovel);
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

    const auto delivery_box = gs1::inventory_storage::delivery_box_container(site_run);
    (void)gs1::inventory_storage::add_item_to_container(
        site_run,
        delivery_box,
        gs1::ItemId {gs1::k_item_wood_bundle},
        5U);
    (void)gs1::inventory_storage::add_item_to_container(
        site_run,
        delivery_box,
        gs1::ItemId {gs1::k_item_iron_bundle},
        2U);
    (void)gs1::inventory_storage::add_item_to_container(
        site_run,
        delivery_box,
        gs1::ItemId {gs1::k_item_hammer},
        1U);

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
            gs1::ItemId {gs1::k_item_wood_bundle}) == 2U);
    GS1_SYSTEM_TEST_CHECK(
        context,
            gs1::inventory_storage::available_item_quantity_in_container(
            site_run,
            gs1::inventory_storage::starter_storage_container(site_run),
            gs1::ItemId {gs1::k_item_iron_bundle}) == 0U);
}

void inventory_craft_commit_requires_hammer_for_storage_crate_recipe(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 1511U);
    GameMessageQueue queue {};
    auto inventory_context = make_site_context<InventorySystem>(campaign, site_run, queue);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        InventorySystem::process_message(
            inventory_context,
            make_message(
                GameMessageType::SiteRunStarted,
                SiteRunStartedMessage {1U, 1U, 101U, 1U, 42ULL})) == GS1_STATUS_OK);

    const auto delivery_box = gs1::inventory_storage::delivery_box_container(site_run);
    (void)gs1::inventory_storage::add_item_to_container(
        site_run,
        delivery_box,
        gs1::ItemId {gs1::k_item_wood_bundle},
        5U);
    (void)gs1::inventory_storage::add_item_to_container(
        site_run,
        delivery_box,
        gs1::ItemId {gs1::k_item_iron_bundle},
        2U);

    const auto workbench_tile = default_starter_workbench_tile(site_run.camp.camp_anchor_tile);
    GS1_SYSTEM_TEST_CHECK(
        context,
        InventorySystem::process_message(
            inventory_context,
            make_message(
                GameMessageType::InventoryCraftCommitRequested,
                InventoryCraftCommitRequestedMessage {
                    gs1::k_recipe_craft_storage_crate,
                    workbench_tile.x,
                    workbench_tile.y,
                    0U})) == GS1_STATUS_INVALID_STATE);
}

void inventory_craft_commit_crafts_hammer_from_wood_and_iron(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    campaign.technology_state.total_reputation = reputation_for_progress_tier(1U);
    auto site_run = make_test_site_run(1U, 1507U);
    GameMessageQueue queue {};
    auto inventory_context = make_site_context<InventorySystem>(campaign, site_run, queue);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        InventorySystem::process_message(
            inventory_context,
            make_message(
                GameMessageType::SiteRunStarted,
                SiteRunStartedMessage {1U, 1U, 101U, 1U, 42ULL})) == GS1_STATUS_OK);

    const auto delivery_box = gs1::inventory_storage::delivery_box_container(site_run);
    (void)gs1::inventory_storage::add_item_to_container(
        site_run,
        delivery_box,
        gs1::ItemId {gs1::k_item_wood_bundle},
        5U);
    (void)gs1::inventory_storage::add_item_to_container(
        site_run,
        delivery_box,
        gs1::ItemId {gs1::k_item_iron_bundle},
        4U);

    const auto workbench_tile = default_starter_workbench_tile(site_run.camp.camp_anchor_tile);
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        InventorySystem::process_message(
            inventory_context,
            make_message(
                GameMessageType::InventoryCraftCommitRequested,
                InventoryCraftCommitRequestedMessage {
                    gs1::k_recipe_craft_hammer,
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
            gs1::ItemId {gs1::k_item_hammer}) == 1U);
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
            gs1::ItemId {gs1::k_item_iron_bundle}) == 3U);
}

void craft_commit_crafts_chemistry_station_kit_from_workbench_when_unlocked(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    campaign.technology_state.total_reputation = reputation_for_progress_tier(5U);
    auto site_run = make_test_site_run(1U, 1510U);
    GameMessageQueue queue {};
    auto inventory_context = make_site_context<InventorySystem>(campaign, site_run, queue);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        InventorySystem::process_message(
            inventory_context,
            make_message(
                GameMessageType::SiteRunStarted,
                SiteRunStartedMessage {1U, 1U, 101U, 1U, 42ULL})) == GS1_STATUS_OK);

    const auto delivery_box = gs1::inventory_storage::delivery_box_container(site_run);
    (void)gs1::inventory_storage::add_item_to_container(
        site_run,
        delivery_box,
        gs1::ItemId {gs1::k_item_wood_bundle},
        4U);
    (void)gs1::inventory_storage::add_item_to_container(
        site_run,
        delivery_box,
        gs1::ItemId {gs1::k_item_iron_bundle},
        3U);
    (void)gs1::inventory_storage::add_item_to_container(
        site_run,
        delivery_box,
        gs1::ItemId {gs1::k_item_hammer},
        1U);

    const auto workbench_tile = default_starter_workbench_tile(site_run.camp.camp_anchor_tile);
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        InventorySystem::process_message(
            inventory_context,
            make_message(
                GameMessageType::InventoryCraftCommitRequested,
                InventoryCraftCommitRequestedMessage {
                    gs1::k_recipe_craft_chemistry_station,
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
            gs1::ItemId {gs1::k_item_chemistry_station_kit}) == 1U);
    GS1_SYSTEM_TEST_CHECK(
        context,
        gs1::inventory_storage::available_item_quantity_in_container(
            site_run,
            gs1::inventory_storage::starter_storage_container(site_run),
            gs1::ItemId {gs1::k_item_wood_bundle}) == 0U);
    GS1_SYSTEM_TEST_CHECK(
        context,
            gs1::inventory_storage::available_item_quantity_in_container(
            site_run,
            gs1::inventory_storage::starter_storage_container(site_run),
            gs1::ItemId {gs1::k_item_iron_bundle}) == 0U);
}

void craft_context_omits_hammer_gated_device_recipe_without_hammer(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 1512U);
    GameMessageQueue queue {};
    auto inventory_context = make_site_context<InventorySystem>(campaign, site_run, queue);
    auto craft_context = make_site_context<CraftSystem>(campaign, site_run, queue);

    const auto start_message =
        make_message(GameMessageType::SiteRunStarted, SiteRunStartedMessage {1U, 1U, 101U, 1U, 42ULL});
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        InventorySystem::process_message(inventory_context, start_message) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        CraftSystem::process_message(craft_context, start_message) == GS1_STATUS_OK);

    const auto delivery_box = gs1::inventory_storage::delivery_box_container(site_run);
    (void)gs1::inventory_storage::add_item_to_container(
        site_run,
        delivery_box,
        gs1::ItemId {gs1::k_item_wood_bundle},
        3U);
    (void)gs1::inventory_storage::add_item_to_container(
        site_run,
        delivery_box,
        gs1::ItemId {gs1::k_item_iron_bundle},
        2U);

    const auto workbench_tile = default_starter_workbench_tile(site_run.camp.camp_anchor_tile);
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        CraftSystem::process_message(
            craft_context,
            make_message(
                GameMessageType::InventoryCraftContextRequested,
                CraftContextRequestedMessage {
                    workbench_tile.x,
                    workbench_tile.y,
                    0U})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, site_run.craft.context_presentation.occupied);
    GS1_SYSTEM_TEST_CHECK(
        context,
        std::any_of(
            site_run.craft.context_presentation.options.begin(),
            site_run.craft.context_presentation.options.end(),
            [](const gs1::CraftContextOptionState& option) {
                return option.recipe_id == gs1::k_recipe_craft_hammer;
            }));
    GS1_SYSTEM_TEST_CHECK(
        context,
        !std::any_of(
            site_run.craft.context_presentation.options.begin(),
            site_run.craft.context_presentation.options.end(),
            [](const gs1::CraftContextOptionState& option) {
                return option.recipe_id == gs1::k_recipe_craft_storage_crate;
            }));

    (void)gs1::inventory_storage::add_item_to_container(
        site_run,
        delivery_box,
        gs1::ItemId {gs1::k_item_hammer},
        1U);
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        CraftSystem::process_message(
            craft_context,
            make_message(
                GameMessageType::InventoryCraftContextRequested,
                CraftContextRequestedMessage {
                    workbench_tile.x,
                    workbench_tile.y,
                    0U})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(
        context,
        std::any_of(
            site_run.craft.context_presentation.options.begin(),
            site_run.craft.context_presentation.options.end(),
            [](const gs1::CraftContextOptionState& option) {
                return option.recipe_id == gs1::k_recipe_craft_storage_crate;
            }));
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

    const auto item_defs = gs1::all_item_defs();
    GS1_SYSTEM_TEST_REQUIRE(context, !item_defs.empty());
    const auto test_item_id = item_defs.front().item_id;
    const auto worker_pack = gs1::inventory_storage::worker_pack_container(site_run);
    const auto worker_pack_ids_before_add =
        gs1::inventory_storage::collect_item_instance_ids_in_container(site_run, worker_pack);
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        gs1::inventory_storage::add_item_to_container(
            site_run,
            worker_pack,
            test_item_id,
            1U) == 0U);
    const auto worker_pack_ids_after_add =
        gs1::inventory_storage::collect_item_instance_ids_in_container(site_run, worker_pack);
    std::uint64_t worker_pack_item_instance_id = 0U;
    for (const auto item_instance_id : worker_pack_ids_after_add)
    {
        if (std::find(
                worker_pack_ids_before_add.begin(),
                worker_pack_ids_before_add.end(),
                item_instance_id) == worker_pack_ids_before_add.end())
        {
            worker_pack_item_instance_id = item_instance_id;
            break;
        }
    }
    GS1_SYSTEM_TEST_REQUIRE(context, worker_pack_item_instance_id != 0U);

    CraftSystem::run(craft_context);
    const auto workbench_tile = default_starter_workbench_tile(site_run.camp.camp_anchor_tile);
    const auto workbench_entity_id = site_run.site_world->device_entity_id(workbench_tile);
    const auto* initial_cache = gs1::craft_logic::find_device_cache(site_run.craft, workbench_entity_id);
    GS1_SYSTEM_TEST_REQUIRE(context, initial_cache != nullptr);
    GS1_SYSTEM_TEST_CHECK(context, initial_cache->worker_pack_included);
    GS1_SYSTEM_TEST_CHECK(
        context,
        std::find(
            initial_cache->nearby_item_instance_ids.begin(),
            initial_cache->nearby_item_instance_ids.end(),
            worker_pack_item_instance_id) != initial_cache->nearby_item_instance_ids.end());

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
        std::find(
            far_cache->nearby_item_instance_ids.begin(),
            far_cache->nearby_item_instance_ids.end(),
            worker_pack_item_instance_id) == far_cache->nearby_item_instance_ids.end());
}

void craft_cache_skips_refresh_while_idle(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 1508U);
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
    auto* cache = gs1::craft_logic::find_device_cache(site_run.craft, workbench_entity_id);
    GS1_SYSTEM_TEST_REQUIRE(context, cache != nullptr);

    constexpr std::uint64_t k_sentinel_item_instance_id = 0xFFFFFFFFULL;
    cache->nearby_item_instance_ids.push_back(k_sentinel_item_instance_id);
    const auto expected_membership_revision =
        site_run.craft.device_cache_source_membership_revision;
    const auto expected_worker_tile = site_run.craft.device_cache_worker_tile;

    CraftSystem::run(craft_context);

    const auto* idle_cache =
        gs1::craft_logic::find_device_cache(site_run.craft, workbench_entity_id);
    GS1_SYSTEM_TEST_REQUIRE(context, idle_cache != nullptr);
    GS1_SYSTEM_TEST_CHECK(
        context,
        std::find(
            idle_cache->nearby_item_instance_ids.begin(),
            idle_cache->nearby_item_instance_ids.end(),
            k_sentinel_item_instance_id) != idle_cache->nearby_item_instance_ids.end());
    GS1_SYSTEM_TEST_CHECK(
        context,
        site_run.craft.device_cache_source_membership_revision ==
            expected_membership_revision);
    GS1_SYSTEM_TEST_CHECK(
        context,
        site_run.craft.device_cache_worker_tile.x == expected_worker_tile.x &&
            site_run.craft.device_cache_worker_tile.y == expected_worker_tile.y);
}

void craft_context_recognizes_nearby_inputs_with_64bit_item_entity_ids(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 1514U);
    GameMessageQueue queue {};
    auto inventory_context = make_site_context<InventorySystem>(campaign, site_run, queue);
    auto craft_context = make_site_context<CraftSystem>(campaign, site_run, queue);

    const auto start_message =
        make_message(GameMessageType::SiteRunStarted, SiteRunStartedMessage {1U, 1U, 101U, 1U, 42ULL});
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        InventorySystem::process_message(inventory_context, start_message) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        CraftSystem::process_message(craft_context, start_message) == GS1_STATUS_OK);

    const auto delivery_box = gs1::inventory_storage::delivery_box_container(site_run);
    GS1_SYSTEM_TEST_REQUIRE(context, delivery_box.is_valid());
    std::uint64_t recycled_wood_item_entity_id = 0U;
    for (int attempt = 0; attempt < 4; ++attempt)
    {
        GS1_SYSTEM_TEST_REQUIRE(
            context,
            gs1::inventory_storage::add_item_to_container(
                site_run,
                delivery_box,
                gs1::ItemId {gs1::k_item_wood_bundle},
                3U) == 0U);
        const auto delivery_item_ids =
            gs1::inventory_storage::collect_item_instance_ids_in_container(site_run, delivery_box);
        for (const auto item_entity_id : delivery_item_ids)
        {
            const auto item = gs1::inventory_storage::entity_from_id(site_run, item_entity_id);
            const auto* stack = gs1::inventory_storage::stack_data(site_run, item);
            if (stack != nullptr && stack->item_id.value == gs1::k_item_wood_bundle)
            {
                recycled_wood_item_entity_id = item_entity_id;
                break;
            }
        }

        if (recycled_wood_item_entity_id >
            static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max()))
        {
            break;
        }

        GS1_SYSTEM_TEST_REQUIRE(
            context,
            gs1::inventory_storage::consume_item_type_from_container(
                site_run,
                delivery_box,
                gs1::ItemId {gs1::k_item_wood_bundle},
                3U) == 0U);
        recycled_wood_item_entity_id = 0U;
    }

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        gs1::inventory_storage::add_item_to_container(
            site_run,
            delivery_box,
            gs1::ItemId {gs1::k_item_iron_bundle},
            1U) == 0U);
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        recycled_wood_item_entity_id >
            static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max()));

    const auto delivery_item_ids =
        gs1::inventory_storage::collect_item_instance_ids_in_container(site_run, delivery_box);
    GS1_SYSTEM_TEST_CHECK(
        context,
        std::find(
            delivery_item_ids.begin(),
            delivery_item_ids.end(),
            recycled_wood_item_entity_id) != delivery_item_ids.end());

    const auto workbench_tile = default_starter_workbench_tile(site_run.camp.camp_anchor_tile);
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        CraftSystem::process_message(
            craft_context,
            make_message(
                GameMessageType::InventoryCraftContextRequested,
                CraftContextRequestedMessage {
                    workbench_tile.x,
                    workbench_tile.y,
                    0U})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, site_run.craft.context_presentation.occupied);
    GS1_SYSTEM_TEST_CHECK(
        context,
        std::any_of(
            site_run.craft.context_presentation.options.begin(),
            site_run.craft.context_presentation.options.end(),
            [](const gs1::CraftContextOptionState& option) {
                return option.recipe_id == gs1::k_recipe_craft_hammer;
            }));
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

void repair_action_requires_hammer_and_restores_device_integrity(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 1505U);
    GameMessageQueue queue {};
    auto action_context = make_site_context<ActionExecutionSystem>(campaign, site_run, queue, 60.0);
    auto maintenance_context = make_site_context<DeviceMaintenanceSystem>(campaign, site_run, queue);

    const auto repair_tile = site_run.camp.camp_anchor_tile;
    auto tile = site_run.site_world->tile_at(repair_tile);
    tile.device.structure_id = gs1::StructureId {gs1::k_structure_storage_crate};
    tile.device.device_integrity = 0.4f;
    site_run.site_world->set_tile(repair_tile, tile);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ActionExecutionSystem::process_message(
            action_context,
            make_message(
                GameMessageType::StartSiteAction,
                gs1::StartSiteActionMessage {
                    GS1_SITE_ACTION_REPAIR,
                    4U,
                    1U,
                    repair_tile.x,
                    repair_tile.y,
                    0U,
                    0U,
                    gs1::k_item_hammer})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(context, count_messages(queue, GameMessageType::SiteActionFailed) == 1U);
    GS1_SYSTEM_TEST_CHECK(
        context,
        first_message_payload<SiteActionFailedMessage>(
            queue,
            GameMessageType::SiteActionFailed)->reason == gs1::SiteActionFailureReason::InsufficientResources);

    queue.clear();
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        gs1::inventory_storage::add_item_to_container(
            site_run,
            gs1::inventory_storage::worker_pack_container(site_run),
            gs1::ItemId {gs1::k_item_hammer},
            1U) == 0U);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ActionExecutionSystem::process_message(
            action_context,
            make_message(
                GameMessageType::StartSiteAction,
                gs1::StartSiteActionMessage {
                    GS1_SITE_ACTION_REPAIR,
                    4U,
                    1U,
                    repair_tile.x,
                    repair_tile.y,
                    0U,
                    0U,
                    gs1::k_item_hammer})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(context, count_messages(queue, GameMessageType::SiteActionStarted) == 1U);

    queue.clear();
    ActionExecutionSystem::run(action_context);
    GS1_SYSTEM_TEST_REQUIRE(context, count_messages(queue, GameMessageType::SiteActionCompleted) == 1U);
    GS1_SYSTEM_TEST_REQUIRE(context, count_messages(queue, GameMessageType::SiteDeviceRepaired) == 1U);
    GS1_SYSTEM_TEST_CHECK(context, count_messages(queue, GameMessageType::InventoryItemConsumeRequested) == 0U);

    const auto* repaired_message =
        first_message(queue, GameMessageType::SiteDeviceRepaired);
    GS1_SYSTEM_TEST_REQUIRE(context, repaired_message != nullptr);
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        DeviceMaintenanceSystem::process_message(maintenance_context, *repaired_message) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(
        context,
        approx_equal(site_run.site_world->tile_device(repair_tile).device_integrity, 1.0f));
}

void storage_device_breakage_destroys_owned_storage_and_items(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 1506U);
    GameMessageQueue queue {};
    auto inventory_context = make_site_context<InventorySystem>(campaign, site_run, queue);
    auto maintenance_context = make_site_context<DeviceMaintenanceSystem>(campaign, site_run, queue, 10.0);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        InventorySystem::process_message(
            inventory_context,
            make_message(
                GameMessageType::SiteRunStarted,
                SiteRunStartedMessage {1U, 1U, 101U, 1U, 42ULL})) == GS1_STATUS_OK);

    const auto storage_tile = default_starter_workbench_tile(site_run.camp.camp_anchor_tile);
    const auto broken_device_entity_id = site_run.site_world->device_entity_id(storage_tile);
    const auto storage_container =
        gs1::inventory_storage::find_device_storage_container(site_run, broken_device_entity_id);
    GS1_SYSTEM_TEST_REQUIRE(context, storage_container.is_valid());
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        gs1::inventory_storage::add_item_to_container(
            site_run,
            storage_container,
            gs1::ItemId {gs1::k_item_hammer},
            1U) == 0U);
    const auto storage_id =
        gs1::inventory_storage::storage_id_for_container(site_run, storage_container);
    site_run.inventory.opened_device_storage_id = storage_id;

    auto tile = site_run.site_world->tile_at(storage_tile);
    tile.ecology.sand_burial = 100.0f;
    tile.device.device_integrity = 0.01f;
    site_run.site_world->set_tile(storage_tile, tile);
    site_run.weather.weather_heat = 100.0f;

    queue.clear();
    DeviceMaintenanceSystem::run(maintenance_context);
    GS1_SYSTEM_TEST_REQUIRE(context, count_messages(queue, GameMessageType::SiteDeviceBroken) == 1U);
    GS1_SYSTEM_TEST_CHECK(context, site_run.site_world->tile_device(storage_tile).structure_id.value == 0U);

    const auto* broken_message =
        first_message(queue, GameMessageType::SiteDeviceBroken);
    GS1_SYSTEM_TEST_REQUIRE(context, broken_message != nullptr);
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        InventorySystem::process_message(inventory_context, *broken_message) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(
        context,
        !gs1::inventory_storage::find_device_storage_container(site_run, broken_device_entity_id).is_valid());
    GS1_SYSTEM_TEST_CHECK(context, site_run.inventory.opened_device_storage_id == 0U);
    GS1_SYSTEM_TEST_CHECK(
        context,
        gs1::inventory_storage::storage_container_state_for_storage_id(site_run, storage_id) == nullptr);
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
    "inventory",
    "craft_commit_requires_hammer_for_storage_crate_recipe",
    inventory_craft_commit_requires_hammer_for_storage_crate_recipe);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "inventory",
    "craft_commit_crafts_hammer_from_wood_and_iron",
    inventory_craft_commit_crafts_hammer_from_wood_and_iron);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "craft",
    "craft_commit_crafts_chemistry_station_kit_from_workbench_when_unlocked",
    craft_commit_crafts_chemistry_station_kit_from_workbench_when_unlocked);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "craft",
    "context_omits_hammer_gated_device_recipe_without_hammer",
    craft_context_omits_hammer_gated_device_recipe_without_hammer);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "craft",
    "cache_tracks_worker_pack_membership_by_distance",
    craft_cache_tracks_worker_pack_membership_by_distance);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "craft",
    "cache_skips_refresh_while_idle",
    craft_cache_skips_refresh_while_idle);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "craft",
    "context_recognizes_nearby_inputs_with_64bit_item_entity_ids",
    craft_context_recognizes_nearby_inputs_with_64bit_item_entity_ids);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "inventory",
    "dynamically_placed_storage_device_reuses_single_inventory_container",
    dynamically_placed_storage_device_reuses_single_inventory_container);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "action_execution",
    "craft_requires_hammer_for_shovel_recipe",
    action_execution_craft_requires_hammer_for_shovel_recipe);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "action_execution",
    "repair_requires_hammer_and_restores_device_integrity",
    repair_action_requires_hammer_and_restores_device_integrity);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "inventory",
    "storage_device_breakage_destroys_owned_storage_and_items",
    storage_device_breakage_destroys_owned_storage_and_items);
