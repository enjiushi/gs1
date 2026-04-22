#include "messages/game_message.h"
#include "campaign/systems/faction_reputation_system.h"
#include "content/defs/item_defs.h"
#include "content/defs/reward_defs.h"
#include "content/defs/task_defs.h"
#include "site/systems/action_execution_system.h"
#include "site/systems/economy_phone_system.h"
#include "site/systems/inventory_system.h"
#include "site/systems/modifier_system.h"
#include "site/systems/task_board_system.h"
#include "site/systems/worker_condition_system.h"
#include "testing/system_test_registry.h"
#include "system_test_fixtures.h"

namespace
{
using gs1::ActionExecutionSystem;
using gs1::EconomyPhoneSystem;
using gs1::FactionReputationSystem;
using gs1::GameMessage;
using gs1::GameMessageQueue;
using gs1::GameMessageType;
using gs1::InventorySystem;
using gs1::ModifierSystem;
using gs1::PhoneListingKind;
using gs1::SiteRunStartedMessage;
using gs1::TaskBoardSystem;
using gs1::TaskRuntimeListKind;
using gs1::WorkerConditionSystem;
using namespace gs1::testing::fixtures;

template <typename Payload>
GameMessage make_message(gs1::GameMessageType type, const Payload& payload)
{
    GameMessage message {};
    message.type = type;
    message.set_payload(payload);
    return message;
}

gs1::PhoneListingState* find_listing_by_id(gs1::EconomyState& economy, std::uint32_t listing_id)
{
    for (auto& listing : economy.available_phone_listings)
    {
        if (listing.listing_id == listing_id)
        {
            return &listing;
        }
    }

    return nullptr;
}

const gs1::site_ecs::StorageItemStack* delivery_box_slot_stack(
    gs1::SiteRunState& site_run,
    std::uint32_t slot_index)
{
    const auto container = gs1::inventory_storage::delivery_box_container(site_run);
    const auto item = gs1::inventory_storage::item_entity_for_slot(site_run, container, slot_index);
    return gs1::inventory_storage::stack_data(site_run, item);
}

const gs1::site_ecs::StorageItemStack* container_slot_stack(
    gs1::SiteRunState& site_run,
    flecs::entity container,
    std::uint32_t slot_index)
{
    const auto item = gs1::inventory_storage::item_entity_for_slot(site_run, container, slot_index);
    return gs1::inventory_storage::stack_data(site_run, item);
}

std::uint32_t find_container_slot_with_item(
    gs1::SiteRunState& site_run,
    flecs::entity container,
    gs1::ItemId item_id)
{
    const auto slot_count = gs1::inventory_storage::slot_count_in_container(site_run, container);
    for (std::uint32_t slot_index = 0U; slot_index < slot_count; ++slot_index)
    {
        const auto* stack = container_slot_stack(site_run, container, slot_index);
        if (stack != nullptr && stack->item_id == item_id && stack->quantity > 0U)
        {
            return slot_index;
        }
    }

    return slot_count;
}

std::uint32_t find_first_empty_worker_pack_slot(const gs1::SiteRunState& site_run)
{
    for (std::uint32_t slot_index = 0U;
         slot_index < site_run.inventory.worker_pack_slots.size();
         ++slot_index)
    {
        if (!site_run.inventory.worker_pack_slots[slot_index].occupied)
        {
            return slot_index;
        }
    }

    return static_cast<std::uint32_t>(site_run.inventory.worker_pack_slots.size());
}

void seed_site_one_inventory(gs1::CampaignState& campaign, gs1::SiteRunState& site_run)
{
    GameMessageQueue inventory_queue {};
    auto inventory_context = make_site_context<InventorySystem>(campaign, site_run, inventory_queue);
    const auto status = InventorySystem::process_message(
        inventory_context,
        make_message(
            GameMessageType::SiteRunStarted,
            SiteRunStartedMessage {site_run.site_id.value, 1U, site_run.site_archetype_id, 1U, 42ULL}));
    (void)status;
}

void drain_task_reward_messages(
    gs1::testing::SystemTestExecutionContext& context,
    gs1::CampaignState& campaign,
    gs1::SiteRunState& site_run,
    GameMessageQueue& queue)
{
    auto campaign_context = make_campaign_context(campaign);
    auto economy_context = make_site_context<EconomyPhoneSystem>(campaign, site_run, queue);
    auto inventory_context = make_site_context<InventorySystem>(campaign, site_run, queue);
    auto modifier_context = make_site_context<ModifierSystem>(campaign, site_run, queue);

    while (!queue.empty())
    {
        const auto message = queue.front();
        queue.pop_front();

        switch (message.type)
        {
        case GameMessageType::FactionReputationAwardRequested:
            GS1_SYSTEM_TEST_REQUIRE(
                context,
                FactionReputationSystem::process_message(campaign_context, message) == GS1_STATUS_OK);
            break;
        case GameMessageType::EconomyMoneyAwardRequested:
        case GameMessageType::SiteUnlockableRevealRequested:
            GS1_SYSTEM_TEST_REQUIRE(
                context,
                EconomyPhoneSystem::process_message(economy_context, message) == GS1_STATUS_OK);
            break;
        case GameMessageType::InventoryDeliveryRequested:
        case GameMessageType::InventoryDeliveryBatchRequested:
            GS1_SYSTEM_TEST_REQUIRE(
                context,
                InventorySystem::process_message(inventory_context, message) == GS1_STATUS_OK);
            break;
        case GameMessageType::RunModifierAwardRequested:
            GS1_SYSTEM_TEST_REQUIRE(
                context,
                ModifierSystem::process_message(modifier_context, message) == GS1_STATUS_OK);
            break;
        default:
            GS1_SYSTEM_TEST_REQUIRE(context, false);
            break;
        }
    }
}

void inventory_site_one_seed_is_applied_once(gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 801U);
    GameMessageQueue queue {};
    auto site_context = make_site_context<InventorySystem>(campaign, site_run, queue);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        InventorySystem::process_message(
            site_context,
            make_message(
                GameMessageType::SiteRunStarted,
                SiteRunStartedMessage {1U, 1U, 101U, 1U, 42ULL})) == GS1_STATUS_OK);

    GS1_SYSTEM_TEST_CHECK(context, !site_run.inventory.worker_pack_slots[0].occupied);
    GS1_SYSTEM_TEST_CHECK(
        context,
        gs1::inventory_storage::available_item_quantity_in_container(
            site_run,
            starter_storage_container(site_run),
            gs1::ItemId {gs1::k_item_water_container}) == 1U);
    GS1_SYSTEM_TEST_CHECK(
        context,
        gs1::inventory_storage::available_item_quantity_in_container(
            site_run,
            starter_storage_container(site_run),
            gs1::ItemId {gs1::k_item_basic_straw_checkerboard}) == 8U);
    GS1_SYSTEM_TEST_CHECK(
        context,
        delivery_box_slot_stack(site_run, 0U) != nullptr);
    GS1_SYSTEM_TEST_CHECK(
        context,
        delivery_box_slot_stack(site_run, 0U)->item_id.value == gs1::k_item_water_container);

    {
        auto container = gs1::inventory_storage::delivery_box_container(site_run);
        auto item = gs1::inventory_storage::item_entity_for_slot(site_run, container, 0U);
        auto& stack = item.get_mut<gs1::site_ecs::StorageItemStack>();
        stack.quantity = 99U;
        item.modified<gs1::site_ecs::StorageItemStack>();
        gs1::inventory_storage::sync_projection_views(site_run);
    }
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        InventorySystem::process_message(
            site_context,
            make_message(
                GameMessageType::SiteRunStarted,
                SiteRunStartedMessage {1U, 1U, 101U, 1U, 42ULL})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, delivery_box_slot_stack(site_run, 0U)->quantity == 99U);
}

void inventory_non_site_seed_and_run_resize_slots(gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(2U, 802U);
    GameMessageQueue queue {};
    auto site_context = make_site_context<InventorySystem>(campaign, site_run, queue);

    site_run.inventory.worker_pack_slots.resize(1U);
    InventorySystem::run(site_context);
    GS1_SYSTEM_TEST_CHECK(
        context,
        site_run.inventory.worker_pack_slots.size() == site_run.inventory.worker_pack_slot_count);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        InventorySystem::process_message(
            site_context,
            make_message(
                GameMessageType::SiteRunStarted,
                SiteRunStartedMessage {2U, 1U, 102U, 1U, 43ULL})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, !site_run.inventory.worker_pack_slots[0].occupied);
    GS1_SYSTEM_TEST_CHECK(
        context,
        gs1::inventory_storage::available_item_quantity_in_container(
            site_run,
            starter_storage_container(site_run),
            gs1::ItemId {gs1::k_item_water_container}) == 1U);
    GS1_SYSTEM_TEST_CHECK(
        context,
        gs1::inventory_storage::available_item_quantity_in_container(
            site_run,
            starter_storage_container(site_run),
            gs1::ItemId {gs1::k_item_basic_straw_checkerboard}) == 8U);
}

void inventory_item_use_validates_and_emits_meter_delta(gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 803U);
    GameMessageQueue queue {};
    auto site_context = make_site_context<InventorySystem>(campaign, site_run, queue);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        InventorySystem::process_message(
            site_context,
            make_message(
                GameMessageType::SiteRunStarted,
                SiteRunStartedMessage {1U, 1U, 101U, 1U, 42ULL})) == GS1_STATUS_OK);

    queue.clear();
    GS1_SYSTEM_TEST_CHECK(
        context,
        InventorySystem::process_message(
            site_context,
            make_message(
                GameMessageType::InventoryItemUseRequested,
                gs1::InventoryItemUseRequestedMessage {
                    999U,
                    site_run.inventory.worker_pack_storage_id,
                    1U,
                    0U})) == GS1_STATUS_INVALID_ARGUMENT);

    (void)gs1::inventory_storage::add_item_to_container(
        site_run,
        gs1::inventory_storage::worker_pack_container(site_run),
        gs1::ItemId {gs1::k_item_medicine_pack},
        1U);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        InventorySystem::process_message(
            site_context,
            make_message(
                GameMessageType::InventoryItemUseRequested,
                gs1::InventoryItemUseRequestedMessage {
                    gs1::k_item_medicine_pack,
                    site_run.inventory.worker_pack_storage_id,
                    1U,
                    0U})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, !site_run.inventory.worker_pack_slots[0].occupied);
    GS1_SYSTEM_TEST_REQUIRE(context, queue.size() == 2U);
    GS1_SYSTEM_TEST_CHECK(context, queue.front().type == GameMessageType::WorkerMeterDeltaRequested);
    GS1_SYSTEM_TEST_CHECK(context, queue[1].type == GameMessageType::InventoryItemUseCompleted);
    GS1_SYSTEM_TEST_CHECK(
        context,
        approx_equal(
            queue.front().payload_as<gs1::WorkerMeterDeltaRequestedMessage>().health_delta,
            18.0f));
}

void inventory_transfer_moves_and_merges_stacks(gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 804U);
    GameMessageQueue queue {};
    auto site_context = make_site_context<InventorySystem>(campaign, site_run, queue);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        InventorySystem::process_message(
            site_context,
            make_message(
                GameMessageType::SiteRunStarted,
                SiteRunStartedMessage {1U, 1U, 101U, 1U, 42ULL})) == GS1_STATUS_OK);

    site_run.inventory.worker_pack_slots[0].occupied = true;
    site_run.inventory.worker_pack_slots[0].item_id = gs1::ItemId {gs1::k_item_water_container};
    site_run.inventory.worker_pack_slots[0].item_quantity = 2U;
    site_run.inventory.worker_pack_slots[0].item_condition = 1.0f;
    site_run.inventory.worker_pack_slots[0].item_freshness = 1.0f;

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        InventorySystem::process_message(
            site_context,
            make_message(
                GameMessageType::InventoryTransferRequested,
                gs1::InventoryTransferRequestedMessage {
                    site_run.inventory.worker_pack_storage_id,
                    site_run.inventory.worker_pack_storage_id,
                    0U,
                    3U,
                    1U,
                    0U,
                    0U})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, site_run.inventory.worker_pack_slots[0].item_quantity == 1U);
    GS1_SYSTEM_TEST_CHECK(context, site_run.inventory.worker_pack_slots[3].item_id.value == 1U);
    GS1_SYSTEM_TEST_CHECK(context, site_run.inventory.worker_pack_slots[3].item_quantity == 1U);

    site_run.inventory.worker_pack_slots[4] = site_run.inventory.worker_pack_slots[3];
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        InventorySystem::process_message(
            site_context,
            make_message(
                GameMessageType::InventoryTransferRequested,
                gs1::InventoryTransferRequestedMessage {
                    site_run.inventory.worker_pack_storage_id,
                    site_run.inventory.worker_pack_storage_id,
                    3U,
                    4U,
                    1U,
                    0U,
                    0U})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, !site_run.inventory.worker_pack_slots[3].occupied);
    GS1_SYSTEM_TEST_CHECK(context, site_run.inventory.worker_pack_slots[4].item_quantity == 2U);
}

void inventory_device_storage_items_must_route_through_worker_pack(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 806U);
    GameMessageQueue queue {};
    auto site_context = make_site_context<InventorySystem>(campaign, site_run, queue);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        InventorySystem::process_message(
            site_context,
            make_message(
                GameMessageType::SiteRunStarted,
                SiteRunStartedMessage {1U, 1U, 101U, 1U, 42ULL})) == GS1_STATUS_OK);

    const auto workbench_tile = default_starter_workbench_tile(site_run.camp.camp_anchor_tile);
    const auto workbench_owner_id =
        static_cast<std::uint32_t>(site_run.site_world->device_entity_id(workbench_tile));
    const auto workbench_storage =
        gs1::inventory_storage::find_device_storage_container(site_run, workbench_owner_id);
    const auto starter_storage = starter_storage_container(site_run);
    const auto worker_pack = gs1::inventory_storage::worker_pack_container(site_run);
    const auto workbench_storage_id = gs1::inventory_storage::storage_id_for_container(site_run, workbench_storage);
    GS1_SYSTEM_TEST_REQUIRE(context, workbench_storage.is_valid());
    GS1_SYSTEM_TEST_REQUIRE(context, starter_storage.is_valid());
    GS1_SYSTEM_TEST_REQUIRE(context, worker_pack.is_valid());

    (void)gs1::inventory_storage::add_item_to_container(
        site_run,
        workbench_storage,
        gs1::ItemId {gs1::k_item_storage_crate_kit},
        1U);

    const auto starter_storage_kit_before = gs1::inventory_storage::available_item_quantity_in_container(
        site_run,
        starter_storage,
        gs1::ItemId {gs1::k_item_storage_crate_kit});
    GS1_SYSTEM_TEST_CHECK(
        context,
        InventorySystem::process_message(
            site_context,
            make_message(
                GameMessageType::InventoryTransferRequested,
                gs1::InventoryTransferRequestedMessage {
                    site_run.inventory.worker_pack_storage_id,
                    site_run.inventory.worker_pack_storage_id,
                    0U,
                    0U,
                    0U,
                    gs1::k_inventory_transfer_flag_resolve_destination_in_dll,
                    0U})) == GS1_STATUS_INVALID_STATE);
    GS1_SYSTEM_TEST_CHECK(context, !site_run.inventory.worker_pack_slots[0].occupied);

    (void)gs1::inventory_storage::add_item_to_container(
        site_run,
        worker_pack,
        gs1::ItemId {gs1::k_item_water_container},
        1U);
    (void)gs1::inventory_storage::add_item_to_container(
        site_run,
        worker_pack,
        gs1::ItemId {gs1::k_item_food_pack},
        1U);
    (void)gs1::inventory_storage::add_item_to_container(
        site_run,
        worker_pack,
        gs1::ItemId {gs1::k_item_medicine_pack},
        1U);
    (void)gs1::inventory_storage::add_item_to_container(
        site_run,
        worker_pack,
        gs1::ItemId {gs1::k_item_wind_reed_seed_bundle},
        1U);
    (void)gs1::inventory_storage::add_item_to_container(
        site_run,
        worker_pack,
        gs1::ItemId {gs1::k_item_wood_bundle},
        1U);
    (void)gs1::inventory_storage::add_item_to_container(
        site_run,
        worker_pack,
        gs1::ItemId {gs1::k_item_iron_bundle},
        1U);
    (void)gs1::inventory_storage::add_item_to_container(
        site_run,
        worker_pack,
        gs1::ItemId {gs1::k_item_basic_straw_checkerboard},
        1U);
    (void)gs1::inventory_storage::add_item_to_container(
        site_run,
        worker_pack,
        gs1::ItemId {gs1::k_item_storage_crate_kit},
        1U);
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        find_first_empty_worker_pack_slot(site_run) == site_run.inventory.worker_pack_slots.size());

    GS1_SYSTEM_TEST_CHECK(
        context,
        InventorySystem::process_message(
            site_context,
            make_message(
                GameMessageType::InventoryTransferRequested,
                gs1::InventoryTransferRequestedMessage {
                    workbench_storage_id,
                    site_run.inventory.worker_pack_storage_id,
                    0U,
                    0U,
                    0U,
                    gs1::k_inventory_transfer_flag_resolve_destination_in_dll,
                    0U})) == GS1_STATUS_INVALID_STATE);
    GS1_SYSTEM_TEST_CHECK(
        context,
        gs1::inventory_storage::available_item_quantity_in_container(
            site_run,
            workbench_storage,
            gs1::ItemId {gs1::k_item_storage_crate_kit}) == 1U);

    const auto freed_quantity = gs1::inventory_storage::consume_item_type_from_container(
        site_run,
        worker_pack,
        gs1::ItemId {gs1::k_item_wind_reed_seed_bundle},
        1U);
    GS1_SYSTEM_TEST_REQUIRE(context, freed_quantity == 0U);
    GS1_SYSTEM_TEST_REQUIRE(context, find_first_empty_worker_pack_slot(site_run) == 3U);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        InventorySystem::process_message(
            site_context,
            make_message(
                GameMessageType::InventoryTransferRequested,
                gs1::InventoryTransferRequestedMessage {
                    workbench_storage_id,
                    site_run.inventory.worker_pack_storage_id,
                    0U,
                    0U,
                    0U,
                    gs1::k_inventory_transfer_flag_resolve_destination_in_dll,
                    0U})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(context, container_slot_stack(site_run, workbench_storage, 0U) == nullptr);
    GS1_SYSTEM_TEST_CHECK(
        context,
        site_run.inventory.worker_pack_slots[3].item_id.value == gs1::k_item_storage_crate_kit);
    GS1_SYSTEM_TEST_CHECK(context, site_run.inventory.worker_pack_slots[3].item_quantity == 1U);

    GS1_SYSTEM_TEST_CHECK(
        context,
        InventorySystem::process_message(
            site_context,
            make_message(
                GameMessageType::InventoryTransferRequested,
                gs1::InventoryTransferRequestedMessage {
                    site_run.inventory.worker_pack_storage_id,
                    site_run.inventory.worker_pack_storage_id,
                    3U,
                    0U,
                    0U,
                    gs1::k_inventory_transfer_flag_resolve_destination_in_dll,
                    0U})) == GS1_STATUS_INVALID_STATE);
    GS1_SYSTEM_TEST_CHECK(context, site_run.inventory.worker_pack_slots[3].occupied);
    GS1_SYSTEM_TEST_CHECK(
        context,
        gs1::inventory_storage::available_item_quantity_in_container(
            site_run,
            starter_storage,
            gs1::ItemId {gs1::k_item_storage_crate_kit}) == starter_storage_kit_before);

    GS1_SYSTEM_TEST_CHECK(
        context,
        InventorySystem::process_message(
            site_context,
            make_message(
                GameMessageType::InventoryTransferRequested,
                gs1::InventoryTransferRequestedMessage {
                    site_run.inventory.worker_pack_storage_id,
                    workbench_storage_id,
                    3U,
                    0U,
                    0U,
                    gs1::k_inventory_transfer_flag_resolve_destination_in_dll,
                    0U})) == GS1_STATUS_OK);

    GS1_SYSTEM_TEST_CHECK(
        context,
        !site_run.inventory.worker_pack_slots[3].occupied);
    GS1_SYSTEM_TEST_CHECK(
        context,
        gs1::inventory_storage::available_item_quantity_in_container(
            site_run,
            workbench_storage,
            gs1::ItemId {gs1::k_item_storage_crate_kit}) == starter_storage_kit_before + 1U);
}

void inventory_item_consume_removes_quantity_across_matching_stacks(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 805U);
    GameMessageQueue queue {};
    auto site_context = make_site_context<InventorySystem>(campaign, site_run, queue);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        InventorySystem::process_message(
            site_context,
            make_message(
                GameMessageType::SiteRunStarted,
                SiteRunStartedMessage {1U, 1U, 101U, 1U, 42ULL})) == GS1_STATUS_OK);

    site_run.inventory.worker_pack_slots[0].occupied = true;
    site_run.inventory.worker_pack_slots[0].item_id = gs1::ItemId {gs1::k_item_water_container};
    site_run.inventory.worker_pack_slots[0].item_quantity = 2U;
    site_run.inventory.worker_pack_slots[0].item_condition = 1.0f;
    site_run.inventory.worker_pack_slots[0].item_freshness = 1.0f;

    site_run.inventory.worker_pack_slots[3].occupied = true;
    site_run.inventory.worker_pack_slots[3].item_id = gs1::ItemId {gs1::k_item_water_container};
    site_run.inventory.worker_pack_slots[3].item_quantity = 2U;
    site_run.inventory.worker_pack_slots[3].item_condition = 1.0f;
    site_run.inventory.worker_pack_slots[3].item_freshness = 1.0f;

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        InventorySystem::process_message(
            site_context,
            make_message(
                GameMessageType::InventoryItemConsumeRequested,
                gs1::InventoryItemConsumeRequestedMessage {
                    gs1::k_item_water_container,
                    3U,
                    GS1_INVENTORY_CONTAINER_WORKER_PACK,
                    0U})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, !site_run.inventory.worker_pack_slots[0].occupied);
    GS1_SYSTEM_TEST_CHECK(context, site_run.inventory.worker_pack_slots[3].occupied);
    GS1_SYSTEM_TEST_CHECK(context, site_run.inventory.worker_pack_slots[3].item_quantity == 1U);
}

void inventory_item_use_drink_defers_item_and_meter_changes_until_action_completion(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 806U);
    GameMessageQueue queue {};
    auto inventory_context = make_site_context<InventorySystem>(campaign, site_run, queue, 60.0);
    auto action_context = make_site_context<ActionExecutionSystem>(campaign, site_run, queue, 60.0);
    auto worker_context = make_site_context<WorkerConditionSystem>(campaign, site_run, queue, 60.0);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        InventorySystem::process_message(
            inventory_context,
            make_message(
                GameMessageType::SiteRunStarted,
                SiteRunStartedMessage {1U, 1U, 101U, 1U, 42ULL})) == GS1_STATUS_OK);

    site_run.inventory.worker_pack_slots[0].occupied = true;
    site_run.inventory.worker_pack_slots[0].item_id = gs1::ItemId {gs1::k_item_water_container};
    site_run.inventory.worker_pack_slots[0].item_quantity = 2U;
    site_run.inventory.worker_pack_slots[0].item_condition = 1.0f;
    site_run.inventory.worker_pack_slots[0].item_freshness = 1.0f;

    auto worker_conditions = site_run.site_world->worker_conditions();
    worker_conditions.hydration = 40.0f;
    site_run.site_world->set_worker_conditions(worker_conditions);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        InventorySystem::process_message(
            inventory_context,
            make_message(
                GameMessageType::InventoryItemUseRequested,
                gs1::InventoryItemUseRequestedMessage {
                    gs1::k_item_water_container,
                    site_run.inventory.worker_pack_storage_id,
                    1U,
                    0U})) == GS1_STATUS_OK);

    GS1_SYSTEM_TEST_REQUIRE(context, queue.size() == 1U);
    GS1_SYSTEM_TEST_CHECK(context, queue.front().type == GameMessageType::StartSiteAction);
    GS1_SYSTEM_TEST_CHECK(context, site_run.inventory.worker_pack_slots[0].item_quantity == 2U);
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(site_run.site_world->worker_conditions().hydration, 40.0f));

    const auto start_action_message = queue.front();
    queue.clear();
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ActionExecutionSystem::process_message(action_context, start_action_message) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(context, site_run.site_action.current_action_id.has_value());
    GS1_SYSTEM_TEST_CHECK(context, site_run.site_action.action_kind == gs1::ActionKind::Drink);
    GS1_SYSTEM_TEST_CHECK(context, site_run.site_action.started_at_world_minute.has_value());
    GS1_SYSTEM_TEST_REQUIRE(context, queue.size() == 1U);
    GS1_SYSTEM_TEST_CHECK(context, queue.front().type == GameMessageType::SiteActionStarted);

    queue.clear();
    ActionExecutionSystem::run(action_context);
    GS1_SYSTEM_TEST_CHECK(context, count_messages(queue, GameMessageType::SiteActionCompleted) == 1U);
    GS1_SYSTEM_TEST_CHECK(context, count_messages(queue, GameMessageType::InventoryItemConsumeRequested) == 1U);
    GS1_SYSTEM_TEST_CHECK(context, count_messages(queue, GameMessageType::WorkerMeterDeltaRequested) == 1U);
    GS1_SYSTEM_TEST_CHECK(context, site_run.inventory.worker_pack_slots[0].item_quantity == 2U);
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(site_run.site_world->worker_conditions().hydration, 40.0f));

    const auto* consume_message =
        first_message(queue, GameMessageType::InventoryItemConsumeRequested);
    const auto* meter_message =
        first_message(queue, GameMessageType::WorkerMeterDeltaRequested);
    GS1_SYSTEM_TEST_REQUIRE(context, consume_message != nullptr);
    GS1_SYSTEM_TEST_REQUIRE(context, meter_message != nullptr);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        InventorySystem::process_message(inventory_context, *consume_message) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        WorkerConditionSystem::process_message(worker_context, *meter_message) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, site_run.inventory.worker_pack_slots[0].occupied);
    GS1_SYSTEM_TEST_CHECK(context, site_run.inventory.worker_pack_slots[0].item_quantity == 1U);
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(site_run.site_world->worker_conditions().hydration, 52.0f));
}

void inventory_delivery_queues_only_overflow_until_delivery_crate_space_opens(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 805U);
    GameMessageQueue queue {};
    auto site_context = make_site_context<InventorySystem>(campaign, site_run, queue);
    const auto delivery_box_tile = site_run.camp.delivery_box_tile;
    const auto delivery_box_device_entity_id = site_run.site_world->device_entity_id(delivery_box_tile);
    GS1_SYSTEM_TEST_REQUIRE(context, delivery_box_device_entity_id != 0U);
    const auto delivery_box = gs1::inventory_storage::ensure_device_storage_container(
        site_run,
        delivery_box_device_entity_id,
        delivery_box_tile,
        10U,
        gs1::inventory_storage::k_inventory_storage_flag_delivery_box |
            gs1::inventory_storage::k_inventory_storage_flag_retrieval_only);
    GS1_SYSTEM_TEST_REQUIRE(context, delivery_box.is_valid());

    for (std::uint32_t slot_index = 0U;
         slot_index < gs1::inventory_storage::slot_count_in_container(site_run, delivery_box);
         ++slot_index)
    {
        (void)gs1::inventory_storage::add_item_to_container(
            site_run,
            delivery_box,
            gs1::ItemId {gs1::k_item_wood_bundle},
            20U);
    }

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        InventorySystem::process_message(
            site_context,
            make_message(
                GameMessageType::InventoryDeliveryRequested,
                gs1::InventoryDeliveryRequestedMessage {
                    gs1::k_item_wind_reed_seed_bundle,
                    3U,
                    1U})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, site_run.inventory.pending_delivery_queue.size() == 1U);
    GS1_SYSTEM_TEST_CHECK(
        context,
        site_run.inventory.pending_delivery_queue.front().state == gs1::PendingDeliveryState::Pending);
    GS1_SYSTEM_TEST_CHECK(context, delivery_box_slot_stack(site_run, 0U) != nullptr);

    for (int step = 0; step < 8; ++step)
    {
        InventorySystem::run(site_context);
    }
    GS1_SYSTEM_TEST_CHECK(
        context,
        site_run.inventory.pending_delivery_queue.front().state == gs1::PendingDeliveryState::Pending);
    GS1_SYSTEM_TEST_CHECK(
        context,
        gs1::inventory_storage::available_item_quantity_in_container(
            site_run,
            delivery_box,
            gs1::ItemId {gs1::k_item_wind_reed_seed_bundle}) == 0U);

    const auto cleared_quantity = gs1::inventory_storage::consume_item_type_from_container(
        site_run,
        delivery_box,
        gs1::ItemId {gs1::k_item_wood_bundle},
        200U);
    GS1_SYSTEM_TEST_CHECK(context, cleared_quantity == 0U);

    InventorySystem::run(site_context);
    GS1_SYSTEM_TEST_CHECK(context, site_run.inventory.pending_delivery_queue.empty());
    GS1_SYSTEM_TEST_REQUIRE(context, delivery_box_slot_stack(site_run, 0U) != nullptr);
    GS1_SYSTEM_TEST_CHECK(
        context,
        delivery_box_slot_stack(site_run, 0U)->item_id.value == gs1::k_item_wind_reed_seed_bundle);
    GS1_SYSTEM_TEST_CHECK(context, delivery_box_slot_stack(site_run, 0U)->quantity == 3U);
}

void economy_site_run_started_seeds_site_one_and_resets_other_sites(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_one_run = make_test_site_run(1U, 901U);
    auto other_site_run = make_test_site_run(2U, 902U);
    GameMessageQueue site_one_queue {};
    GameMessageQueue other_queue {};

    auto site_one_context = make_site_context<EconomyPhoneSystem>(campaign, site_one_run, site_one_queue);
    auto other_context = make_site_context<EconomyPhoneSystem>(campaign, other_site_run, other_queue);
    const auto started_one = make_message(
        GameMessageType::SiteRunStarted,
        SiteRunStartedMessage {1U, 1U, 101U, 1U, 42ULL});
    const auto started_two = make_message(
        GameMessageType::SiteRunStarted,
        SiteRunStartedMessage {2U, 1U, 102U, 1U, 42ULL});

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        EconomyPhoneSystem::process_message(site_one_context, started_one) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, site_one_run.economy.money == 45);
    GS1_SYSTEM_TEST_CHECK(context, site_one_run.economy.available_phone_listings.size() >= 11U);
    GS1_SYSTEM_TEST_CHECK(context, site_one_run.economy.direct_purchase_unlockable_ids.size() == 1U);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        EconomyPhoneSystem::process_message(other_context, started_two) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, other_site_run.economy.money == 0);
    GS1_SYSTEM_TEST_CHECK(context, other_site_run.economy.available_phone_listings.empty());
}

void economy_purchase_sell_hire_and_unlockable_paths_update_money(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 903U);
    GameMessageQueue queue {};
    auto site_context = make_site_context<EconomyPhoneSystem>(campaign, site_run, queue);
    seed_site_one_inventory(campaign, site_run);
    (void)gs1::inventory_storage::add_item_to_container(
        site_run,
        gs1::inventory_storage::delivery_box_container(site_run),
        gs1::ItemId {gs1::k_item_water_container},
        1U);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        EconomyPhoneSystem::process_message(
            site_context,
            make_message(
                GameMessageType::SiteRunStarted,
                SiteRunStartedMessage {1U, 1U, 101U, 1U, 42ULL})) == GS1_STATUS_OK);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        EconomyPhoneSystem::process_message(
            site_context,
            make_message(
                GameMessageType::PhoneListingPurchaseRequested,
                gs1::PhoneListingPurchaseRequestedMessage {1U, 2U, 0U})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, site_run.economy.money == 30);
    GS1_SYSTEM_TEST_REQUIRE(context, find_listing_by_id(site_run.economy, 1U) != nullptr);
    GS1_SYSTEM_TEST_CHECK(context, find_listing_by_id(site_run.economy, 1U)->quantity == 4U);
    GS1_SYSTEM_TEST_REQUIRE(context, queue.size() == 2U);
    GS1_SYSTEM_TEST_CHECK(context, queue.front().type == GameMessageType::InventoryDeliveryBatchRequested);
    GS1_SYSTEM_TEST_CHECK(
        context,
        queue.front().payload_as<gs1::InventoryDeliveryBatchRequestedMessage>().entries[0].item_id ==
            gs1::k_item_water_container);
    GS1_SYSTEM_TEST_CHECK(
        context,
        queue.front().payload_as<gs1::InventoryDeliveryBatchRequestedMessage>().entries[0].quantity == 2U);
    GS1_SYSTEM_TEST_CHECK(context, queue[1].type == GameMessageType::PhoneListingPurchased);

    queue.clear();
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        EconomyPhoneSystem::process_message(
            site_context,
            make_message(
                GameMessageType::PhoneListingSaleRequested,
                gs1::PhoneListingSaleRequestedMessage {1001U, 2U, 0U})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, site_run.economy.money == 36);
    GS1_SYSTEM_TEST_REQUIRE(context, find_listing_by_id(site_run.economy, 1001U) != nullptr);
    GS1_SYSTEM_TEST_CHECK(context, find_listing_by_id(site_run.economy, 1001U)->quantity == 0U);
    GS1_SYSTEM_TEST_REQUIRE(context, queue.size() == 2U);
    GS1_SYSTEM_TEST_CHECK(context, queue.front().type == GameMessageType::InventoryGlobalItemConsumeRequested);
    GS1_SYSTEM_TEST_CHECK(
        context,
        queue.front().payload_as<gs1::InventoryGlobalItemConsumeRequestedMessage>().item_id ==
            gs1::k_item_water_container);
    GS1_SYSTEM_TEST_CHECK(context, queue[1].type == GameMessageType::PhoneListingSold);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        EconomyPhoneSystem::process_message(
            site_context,
            make_message(
                GameMessageType::ContractorHireRequested,
                gs1::ContractorHireRequestedMessage {10U, 1U})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, site_run.economy.money == 28);
    GS1_SYSTEM_TEST_REQUIRE(context, find_listing_by_id(site_run.economy, 10U) != nullptr);
    GS1_SYSTEM_TEST_CHECK(context, find_listing_by_id(site_run.economy, 10U)->quantity == 2U);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        EconomyPhoneSystem::process_message(
            site_context,
            make_message(
                GameMessageType::SiteUnlockablePurchaseRequested,
                gs1::SiteUnlockablePurchaseRequestedMessage {101U})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, site_run.economy.money == 8);
    GS1_SYSTEM_TEST_CHECK(context, site_run.economy.direct_purchase_unlockable_ids.empty());
    GS1_SYSTEM_TEST_CHECK(context, site_run.economy.revealed_site_unlockable_ids.size() == 1U);
}

void economy_cart_checkout_charges_delivery_fee_and_clears_cart(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 906U);
    GameMessageQueue queue {};
    auto site_context = make_site_context<EconomyPhoneSystem>(campaign, site_run, queue);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        EconomyPhoneSystem::process_message(
            site_context,
            make_message(
                GameMessageType::SiteRunStarted,
                SiteRunStartedMessage {1U, 1U, 101U, 1U, 42ULL})) == GS1_STATUS_OK);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        EconomyPhoneSystem::process_message(
            site_context,
            make_message(
                GameMessageType::PhoneListingCartAddRequested,
                gs1::PhoneListingCartAddRequestedMessage {1U, 2U, 0U})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        EconomyPhoneSystem::process_message(
            site_context,
            make_message(
                GameMessageType::PhoneListingCartAddRequested,
                gs1::PhoneListingCartAddRequestedMessage {2U, 1U, 0U})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, find_listing_by_id(site_run.economy, 1U)->cart_quantity == 2U);
    GS1_SYSTEM_TEST_CHECK(context, find_listing_by_id(site_run.economy, 2U)->cart_quantity == 1U);

    queue.clear();
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        EconomyPhoneSystem::process_message(
            site_context,
            make_message(
                GameMessageType::PhoneCartCheckoutRequested,
                gs1::PhoneCartCheckoutRequestedMessage {})) == GS1_STATUS_OK);

    GS1_SYSTEM_TEST_CHECK(context, site_run.economy.money == 26);
    GS1_SYSTEM_TEST_CHECK(context, find_listing_by_id(site_run.economy, 1U)->quantity == 4U);
    GS1_SYSTEM_TEST_CHECK(context, find_listing_by_id(site_run.economy, 2U)->quantity == 4U);
    GS1_SYSTEM_TEST_CHECK(context, find_listing_by_id(site_run.economy, 1U)->cart_quantity == 0U);
    GS1_SYSTEM_TEST_CHECK(context, find_listing_by_id(site_run.economy, 2U)->cart_quantity == 0U);
    GS1_SYSTEM_TEST_REQUIRE(context, queue.size() == 3U);
    GS1_SYSTEM_TEST_CHECK(context, queue.front().type == GameMessageType::InventoryDeliveryBatchRequested);
    GS1_SYSTEM_TEST_CHECK(
        context,
        queue.front().payload_as<gs1::InventoryDeliveryBatchRequestedMessage>().entry_count == 2U);
    GS1_SYSTEM_TEST_CHECK(context, queue[1].type == GameMessageType::PhoneListingPurchased);
    GS1_SYSTEM_TEST_CHECK(context, queue[2].type == GameMessageType::PhoneListingPurchased);
}

void economy_purchase_listing_unlockable_path_spends_money_and_removes_direct_unlockable(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 905U);
    GameMessageQueue queue {};
    auto site_context = make_site_context<EconomyPhoneSystem>(campaign, site_run, queue);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        EconomyPhoneSystem::process_message(
            site_context,
            make_message(
                GameMessageType::SiteRunStarted,
                SiteRunStartedMessage {1U, 1U, 101U, 1U, 42ULL})) == GS1_STATUS_OK);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        EconomyPhoneSystem::process_message(
            site_context,
            make_message(
                GameMessageType::PhoneListingPurchaseRequested,
                gs1::PhoneListingPurchaseRequestedMessage {11U, 1U, 0U})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, site_run.economy.money == 25);
    GS1_SYSTEM_TEST_CHECK(context, site_run.economy.direct_purchase_unlockable_ids.empty());
    GS1_SYSTEM_TEST_CHECK(context, site_run.economy.revealed_site_unlockable_ids.size() == 1U);
}

void economy_invalid_listing_or_money_returns_failures(gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 904U);
    GameMessageQueue queue {};
    auto site_context = make_site_context<EconomyPhoneSystem>(campaign, site_run, queue);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        EconomyPhoneSystem::process_message(
            site_context,
            make_message(
                GameMessageType::SiteRunStarted,
                SiteRunStartedMessage {1U, 1U, 101U, 1U, 42ULL})) == GS1_STATUS_OK);

    GS1_SYSTEM_TEST_CHECK(
        context,
        EconomyPhoneSystem::process_message(
            site_context,
            make_message(
                GameMessageType::PhoneListingPurchaseRequested,
                gs1::PhoneListingPurchaseRequestedMessage {999U, 1U, 0U})) == GS1_STATUS_NOT_FOUND);

    GS1_SYSTEM_TEST_CHECK(
        context,
        EconomyPhoneSystem::process_message(
            site_context,
            make_message(
                GameMessageType::PhoneListingPurchaseRequested,
                gs1::PhoneListingPurchaseRequestedMessage {1U, 50U, 0U})) == GS1_STATUS_INVALID_STATE);

    site_run.economy.money = 0;
    GS1_SYSTEM_TEST_CHECK(
        context,
        EconomyPhoneSystem::process_message(
            site_context,
            make_message(
                GameMessageType::ContractorHireRequested,
                gs1::ContractorHireRequestedMessage {9U, 1U})) == GS1_STATUS_INVALID_STATE);
}

void economy_repeated_sell_requests_do_not_oversubscribe_inventory(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 907U);
    GameMessageQueue queue {};
    auto economy_context = make_site_context<EconomyPhoneSystem>(campaign, site_run, queue);
    auto inventory_context = make_site_context<InventorySystem>(campaign, site_run, queue);
    seed_site_one_inventory(campaign, site_run);
    (void)gs1::inventory_storage::add_item_to_container(
        site_run,
        gs1::inventory_storage::delivery_box_container(site_run),
        gs1::ItemId {gs1::k_item_water_container},
        1U);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        EconomyPhoneSystem::process_message(
            economy_context,
            make_message(
                GameMessageType::SiteRunStarted,
                SiteRunStartedMessage {1U, 1U, 101U, 1U, 42ULL})) == GS1_STATUS_OK);

    auto* sell_listing = find_listing_by_id(site_run.economy, 1001U);
    GS1_SYSTEM_TEST_REQUIRE(context, sell_listing != nullptr);
    GS1_SYSTEM_TEST_CHECK(context, sell_listing->quantity == 2U);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        EconomyPhoneSystem::process_message(
            economy_context,
            make_message(
                GameMessageType::PhoneListingSaleRequested,
                gs1::PhoneListingSaleRequestedMessage {1001U, 1U, 0U})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        EconomyPhoneSystem::process_message(
            economy_context,
            make_message(
                GameMessageType::PhoneListingSaleRequested,
                gs1::PhoneListingSaleRequestedMessage {1001U, 1U, 0U})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, site_run.economy.money == 51);
    GS1_SYSTEM_TEST_REQUIRE(context, queue.size() == 4U);
    GS1_SYSTEM_TEST_CHECK(context, queue[0].type == GameMessageType::InventoryGlobalItemConsumeRequested);
    GS1_SYSTEM_TEST_CHECK(context, queue[1].type == GameMessageType::PhoneListingSold);
    GS1_SYSTEM_TEST_CHECK(context, queue[2].type == GameMessageType::InventoryGlobalItemConsumeRequested);
    GS1_SYSTEM_TEST_CHECK(context, queue[3].type == GameMessageType::PhoneListingSold);

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
            gs1::ItemId {gs1::k_item_water_container}) == 0U);

    EconomyPhoneSystem::run(economy_context);
    sell_listing = find_listing_by_id(site_run.economy, 1001U);
    GS1_SYSTEM_TEST_CHECK(context, sell_listing == nullptr || sell_listing->quantity == 0U);
    GS1_SYSTEM_TEST_CHECK(
        context,
        EconomyPhoneSystem::process_message(
            economy_context,
            make_message(
                GameMessageType::PhoneListingSaleRequested,
                gs1::PhoneListingSaleRequestedMessage {1001U, 1U, 0U})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, site_run.economy.money == 51);
}

void economy_repeated_cart_add_requests_clamp_without_failure(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 908U);
    GameMessageQueue queue {};
    auto site_context = make_site_context<EconomyPhoneSystem>(campaign, site_run, queue);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        EconomyPhoneSystem::process_message(
            site_context,
            make_message(
                GameMessageType::SiteRunStarted,
                SiteRunStartedMessage {1U, 1U, 101U, 1U, 42ULL})) == GS1_STATUS_OK);

    auto* listing = find_listing_by_id(site_run.economy, 1U);
    GS1_SYSTEM_TEST_REQUIRE(context, listing != nullptr);
    GS1_SYSTEM_TEST_CHECK(context, listing->quantity == 6U);

    for (std::uint32_t index = 0U; index < 8U; ++index)
    {
        GS1_SYSTEM_TEST_CHECK(
            context,
            EconomyPhoneSystem::process_message(
                site_context,
                make_message(
                    GameMessageType::PhoneListingCartAddRequested,
                    gs1::PhoneListingCartAddRequestedMessage {1U, 1U, 0U})) == GS1_STATUS_OK);
    }

    listing = find_listing_by_id(site_run.economy, 1U);
    GS1_SYSTEM_TEST_REQUIRE(context, listing != nullptr);
    GS1_SYSTEM_TEST_CHECK(context, listing->cart_quantity == 6U);
    GS1_SYSTEM_TEST_CHECK(context, listing->quantity == 6U);
}

const gs1::TaskRewardDraftOption* find_task_reward_option(
    const gs1::TaskInstanceState& task,
    std::uint32_t reward_candidate_id)
{
    for (const auto& reward_option : task.reward_draft_options)
    {
        if (reward_option.reward_candidate_id.value == reward_candidate_id)
        {
            return &reward_option;
        }
    }

    return nullptr;
}

gs1::TaskInstanceState* find_task_by_template_id(
    gs1::TaskBoardState& task_board,
    std::uint32_t task_template_id)
{
    for (auto& task : task_board.visible_tasks)
    {
        if (task.task_template_id.value == task_template_id)
        {
            return &task;
        }
    }

    return nullptr;
}

std::size_t count_queued_messages(
    const GameMessageQueue& queue,
    GameMessageType type)
{
    std::size_t count = 0U;
    for (const auto& message : queue)
    {
        if (message.type == type)
        {
            count += 1U;
        }
    }

    return count;
}

void complete_seeded_task(
    gs1::testing::SystemTestExecutionContext& context,
    gs1::SiteSystemContext<gs1::TaskBoardSystem>& site_context,
    gs1::SiteRunState& site_run,
    GameMessageQueue& queue)
{
    const auto task_id = site_run.task_board.visible_tasks.front().task_instance_id.value;
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        TaskBoardSystem::process_message(
            site_context,
            make_message(
                GameMessageType::TaskAcceptRequested,
                gs1::TaskAcceptRequestedMessage {task_id})) == GS1_STATUS_OK);

    queue.clear();
    const auto target_amount = site_run.task_board.visible_tasks.front().target_amount;
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        TaskBoardSystem::process_message(
            site_context,
            make_message(
                GameMessageType::RestorationProgressChanged,
                gs1::RestorationProgressChangedMessage {
                    target_amount,
                    target_amount,
                    1.0f})) == GS1_STATUS_OK);
}

void task_board_site_run_started_seeds_site_one_board(gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 1001U);
    GameMessageQueue queue {};
    auto site_context = make_site_context<TaskBoardSystem>(campaign, site_run, queue);

    site_run.counters.site_completion_tile_threshold = 5U;
    site_run.counters.fully_grown_tile_count = 2U;
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        TaskBoardSystem::process_message(
            site_context,
            make_message(
                GameMessageType::SiteRunStarted,
                SiteRunStartedMessage {1U, 1U, 101U, 1U, 42ULL})) == GS1_STATUS_OK);

    GS1_SYSTEM_TEST_REQUIRE(context, site_run.task_board.visible_tasks.size() == 7U);
    const auto& task = site_run.task_board.visible_tasks.front();
    GS1_SYSTEM_TEST_CHECK(context, task.target_amount == 5U);
    GS1_SYSTEM_TEST_CHECK(context, task.current_progress_amount == 2U);
    GS1_SYSTEM_TEST_CHECK(context, task.runtime_list_kind == TaskRuntimeListKind::Visible);
    GS1_SYSTEM_TEST_REQUIRE(context, task.reward_draft_options.size() == 2U);
    GS1_SYSTEM_TEST_CHECK(context, !task.reward_draft_options[0].selected);
    GS1_SYSTEM_TEST_CHECK(context, !task.reward_draft_options[1].selected);
}

void task_board_non_site_run_started_clears_existing_board_state(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(2U, 1004U);
    GameMessageQueue queue {};
    auto site_context = make_site_context<TaskBoardSystem>(campaign, site_run, queue);

    gs1::TaskInstanceState task {};
    task.task_instance_id = gs1::TaskInstanceId {99U};
    task.runtime_list_kind = TaskRuntimeListKind::Accepted;
    site_run.task_board.visible_tasks.push_back(task);
    site_run.task_board.accepted_task_ids.push_back(task.task_instance_id);
    site_run.task_board.completed_task_ids.push_back(gs1::TaskInstanceId {100U});
    site_run.task_board.active_chain_state = gs1::TaskChainState {};
    site_run.task_board.task_pool_size = 5U;
    site_run.task_board.minutes_until_next_refresh = 12.0;

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        TaskBoardSystem::process_message(
            site_context,
            make_message(
                GameMessageType::SiteRunStarted,
                SiteRunStartedMessage {2U, 1U, 102U, 1U, 42ULL})) == GS1_STATUS_OK);

    GS1_SYSTEM_TEST_CHECK(context, site_run.task_board.visible_tasks.empty());
    GS1_SYSTEM_TEST_CHECK(context, site_run.task_board.accepted_task_ids.empty());
    GS1_SYSTEM_TEST_CHECK(context, site_run.task_board.completed_task_ids.empty());
    GS1_SYSTEM_TEST_CHECK(context, !site_run.task_board.active_chain_state.has_value());
    GS1_SYSTEM_TEST_CHECK(context, site_run.task_board.task_pool_size == 0U);
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(site_run.task_board.minutes_until_next_refresh, 0.0));
}

void task_board_accept_respects_cap_and_list_kind(gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 1002U);
    GameMessageQueue queue {};
    auto site_context = make_site_context<TaskBoardSystem>(campaign, site_run, queue);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        TaskBoardSystem::process_message(
            site_context,
            make_message(
                GameMessageType::SiteRunStarted,
                SiteRunStartedMessage {1U, 1U, 101U, 1U, 42ULL})) == GS1_STATUS_OK);

    const auto task_id = site_run.task_board.visible_tasks.front().task_instance_id.value;
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        TaskBoardSystem::process_message(
            site_context,
            make_message(
                GameMessageType::TaskAcceptRequested,
                gs1::TaskAcceptRequestedMessage {task_id})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, site_run.task_board.accepted_task_ids.size() == 1U);
    GS1_SYSTEM_TEST_CHECK(
        context,
        site_run.task_board.visible_tasks.front().runtime_list_kind == TaskRuntimeListKind::Accepted);

    site_run.task_board.accepted_task_cap = 0U;
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        TaskBoardSystem::process_message(
            site_context,
            make_message(
                GameMessageType::TaskAcceptRequested,
                gs1::TaskAcceptRequestedMessage {task_id})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, site_run.task_board.accepted_task_ids.size() == 1U);
}

void task_board_completion_queues_faction_reputation_once(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 1003U);
    GameMessageQueue queue {};
    auto site_context = make_site_context<TaskBoardSystem>(campaign, site_run, queue);

    site_run.counters.site_completion_tile_threshold = 2U;
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        TaskBoardSystem::process_message(
            site_context,
            make_message(
                GameMessageType::SiteRunStarted,
                SiteRunStartedMessage {1U, 1U, 101U, 1U, 42ULL})) == GS1_STATUS_OK);

    const auto task_id = site_run.task_board.visible_tasks.front().task_instance_id.value;
    complete_seeded_task(context, site_context, site_run, queue);
    GS1_SYSTEM_TEST_CHECK(
        context,
        site_run.task_board.visible_tasks.front().runtime_list_kind == TaskRuntimeListKind::Completed);
    GS1_SYSTEM_TEST_CHECK(context, site_run.task_board.completed_task_ids.size() == 1U);
    GS1_SYSTEM_TEST_CHECK(context, site_run.task_board.accepted_task_ids.empty());
    GS1_SYSTEM_TEST_REQUIRE(context, queue.size() == 1U);
    GS1_SYSTEM_TEST_CHECK(context, queue.front().type == GameMessageType::FactionReputationAwardRequested);
    GS1_SYSTEM_TEST_CHECK(
        context,
        queue.front().payload_as<gs1::FactionReputationAwardRequestedMessage>().faction_id == 1U);
    GS1_SYSTEM_TEST_CHECK(
        context,
        queue.front().payload_as<gs1::FactionReputationAwardRequestedMessage>().delta == 5);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        TaskBoardSystem::process_message(
            site_context,
            make_message(
                GameMessageType::RestorationProgressChanged,
                gs1::RestorationProgressChangedMessage {2U, 2U, 1.0f})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, count_queued_messages(queue, GameMessageType::FactionReputationAwardRequested) == 1U);

    drain_task_reward_messages(context, campaign, site_run, queue);
    GS1_SYSTEM_TEST_CHECK(context, campaign.faction_progress[0].faction_reputation == 5);
    GS1_SYSTEM_TEST_CHECK(context, site_run.task_board.completed_task_ids.front().value == task_id);
}

void task_board_buy_task_completes_from_successful_purchase(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 1004U);
    GameMessageQueue queue {};
    auto inventory_context = make_site_context<InventorySystem>(campaign, site_run, queue);
    auto economy_context = make_site_context<EconomyPhoneSystem>(campaign, site_run, queue);
    auto task_context = make_site_context<TaskBoardSystem>(campaign, site_run, queue);

    const auto start_message =
        make_message(GameMessageType::SiteRunStarted, SiteRunStartedMessage {1U, 1U, 101U, 1U, 42ULL});
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        InventorySystem::process_message(inventory_context, start_message) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        EconomyPhoneSystem::process_message(economy_context, start_message) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        TaskBoardSystem::process_message(task_context, start_message) == GS1_STATUS_OK);

    auto* buy_task =
        find_task_by_template_id(site_run.task_board, gs1::k_task_template_site1_buy_water);
    GS1_SYSTEM_TEST_REQUIRE(context, buy_task != nullptr);
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        TaskBoardSystem::process_message(
            task_context,
            make_message(
                GameMessageType::TaskAcceptRequested,
                gs1::TaskAcceptRequestedMessage {buy_task->task_instance_id.value})) == GS1_STATUS_OK);

    queue.clear();
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        EconomyPhoneSystem::process_message(
            economy_context,
            make_message(
                GameMessageType::PhoneListingPurchaseRequested,
                gs1::PhoneListingPurchaseRequestedMessage {1U, 1U, 0U})) == GS1_STATUS_OK);
    while (!queue.empty())
    {
        const auto message = queue.front();
        queue.pop_front();
        if (message.type == GameMessageType::InventoryDeliveryBatchRequested)
        {
            GS1_SYSTEM_TEST_REQUIRE(
                context,
                InventorySystem::process_message(inventory_context, message) == GS1_STATUS_OK);
        }
        else if (message.type == GameMessageType::PhoneListingPurchased)
        {
            GS1_SYSTEM_TEST_REQUIRE(
                context,
                TaskBoardSystem::process_message(task_context, message) == GS1_STATUS_OK);
        }
    }

    buy_task = find_task_by_template_id(site_run.task_board, gs1::k_task_template_site1_buy_water);
    GS1_SYSTEM_TEST_REQUIRE(context, buy_task != nullptr);
    GS1_SYSTEM_TEST_CHECK(context, buy_task->runtime_list_kind == TaskRuntimeListKind::Completed);
}

void task_board_transfer_task_completes_from_successful_transfer(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 1005U);
    GameMessageQueue queue {};
    auto inventory_context = make_site_context<InventorySystem>(campaign, site_run, queue);
    auto task_context = make_site_context<TaskBoardSystem>(campaign, site_run, queue);

    const auto start_message =
        make_message(GameMessageType::SiteRunStarted, SiteRunStartedMessage {1U, 1U, 101U, 1U, 42ULL});
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        InventorySystem::process_message(inventory_context, start_message) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        TaskBoardSystem::process_message(task_context, start_message) == GS1_STATUS_OK);

    auto* transfer_task =
        find_task_by_template_id(site_run.task_board, gs1::k_task_template_site1_transfer_seeds);
    GS1_SYSTEM_TEST_REQUIRE(context, transfer_task != nullptr);
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        TaskBoardSystem::process_message(
            task_context,
            make_message(
                GameMessageType::TaskAcceptRequested,
                gs1::TaskAcceptRequestedMessage {transfer_task->task_instance_id.value})) == GS1_STATUS_OK);

    const auto starter_storage =
        gs1::inventory_storage::starter_storage_container(site_run);
    (void)gs1::inventory_storage::add_item_to_container(
        site_run,
        starter_storage,
        gs1::ItemId {gs1::k_item_wind_reed_seed_bundle},
        1U);
    const auto starter_storage_id =
        gs1::inventory_storage::storage_id_for_container(site_run, starter_storage);
    const auto seed_slot_index = find_container_slot_with_item(
        site_run,
        starter_storage,
        gs1::ItemId {gs1::k_item_wind_reed_seed_bundle});
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        seed_slot_index < gs1::inventory_storage::slot_count_in_container(site_run, starter_storage));
    queue.clear();
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        InventorySystem::process_message(
            inventory_context,
            make_message(
                GameMessageType::InventoryTransferRequested,
                gs1::InventoryTransferRequestedMessage {
                    starter_storage_id,
                    site_run.inventory.worker_pack_storage_id,
                    static_cast<std::uint16_t>(seed_slot_index),
                    3U,
                    1U,
                    0U,
                    0U})) == GS1_STATUS_OK);

    while (!queue.empty())
    {
        const auto message = queue.front();
        queue.pop_front();
        if (message.type == GameMessageType::InventoryTransferCompleted)
        {
            GS1_SYSTEM_TEST_REQUIRE(
                context,
                TaskBoardSystem::process_message(task_context, message) == GS1_STATUS_OK);
        }
    }

    transfer_task =
        find_task_by_template_id(site_run.task_board, gs1::k_task_template_site1_transfer_seeds);
    GS1_SYSTEM_TEST_REQUIRE(context, transfer_task != nullptr);
    GS1_SYSTEM_TEST_CHECK(context, transfer_task->runtime_list_kind == TaskRuntimeListKind::Completed);
}

void task_board_claim_reward_marks_selected_and_applies_money_reward(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 1005U);
    GameMessageQueue queue {};
    auto task_context = make_site_context<TaskBoardSystem>(campaign, site_run, queue);
    auto economy_context = make_site_context<EconomyPhoneSystem>(campaign, site_run, queue);

    const auto start_message =
        make_message(GameMessageType::SiteRunStarted, SiteRunStartedMessage {1U, 1U, 101U, 1U, 42ULL});
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        EconomyPhoneSystem::process_message(economy_context, start_message) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        TaskBoardSystem::process_message(task_context, start_message) == GS1_STATUS_OK);

    const auto task_id = site_run.task_board.visible_tasks.front().task_instance_id.value;
    complete_seeded_task(context, task_context, site_run, queue);
    queue.clear();
    site_run.task_board.visible_tasks.front().reward_draft_options[0].reward_candidate_id =
        gs1::RewardCandidateId {gs1::k_reward_candidate_site1_money_stipend};

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        TaskBoardSystem::process_message(
            task_context,
            make_message(
                GameMessageType::TaskRewardClaimRequested,
                gs1::TaskRewardClaimRequestedMessage {
                    task_id,
                    gs1::k_reward_candidate_site1_money_stipend})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(context, queue.size() == 1U);
    GS1_SYSTEM_TEST_CHECK(context, queue.front().type == GameMessageType::EconomyMoneyAwardRequested);
    GS1_SYSTEM_TEST_CHECK(
        context,
        queue.front().payload_as<gs1::EconomyMoneyAwardRequestedMessage>().delta == 12);

    drain_task_reward_messages(context, campaign, site_run, queue);
    const auto* reward_option =
        find_task_reward_option(site_run.task_board.visible_tasks.front(), gs1::k_reward_candidate_site1_money_stipend);
    GS1_SYSTEM_TEST_REQUIRE(context, reward_option != nullptr);
    GS1_SYSTEM_TEST_CHECK(context, reward_option->selected);
    GS1_SYSTEM_TEST_CHECK(context, site_run.economy.money == 57);
}

void task_board_claim_reward_delivers_to_crate_immediately_and_marks_selected(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 1006U);
    GameMessageQueue queue {};
    auto task_context = make_site_context<TaskBoardSystem>(campaign, site_run, queue);
    auto inventory_context = make_site_context<InventorySystem>(campaign, site_run, queue);

    const auto start_message =
        make_message(GameMessageType::SiteRunStarted, SiteRunStartedMessage {1U, 1U, 101U, 1U, 42ULL});
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        InventorySystem::process_message(inventory_context, start_message) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        TaskBoardSystem::process_message(task_context, start_message) == GS1_STATUS_OK);

    const auto task_id = site_run.task_board.visible_tasks.front().task_instance_id.value;
    complete_seeded_task(context, task_context, site_run, queue);
    queue.clear();
    site_run.task_board.visible_tasks.front().reward_draft_options[1].reward_candidate_id =
        gs1::RewardCandidateId {gs1::k_reward_candidate_site1_food_delivery};

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        TaskBoardSystem::process_message(
            task_context,
            make_message(
                GameMessageType::TaskRewardClaimRequested,
                gs1::TaskRewardClaimRequestedMessage {
                    task_id,
                    gs1::k_reward_candidate_site1_food_delivery})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(context, queue.size() == 1U);
    GS1_SYSTEM_TEST_CHECK(context, queue.front().type == GameMessageType::InventoryDeliveryRequested);
    const auto& delivery_payload =
        queue.front().payload_as<gs1::InventoryDeliveryRequestedMessage>();
    GS1_SYSTEM_TEST_CHECK(context, delivery_payload.minutes_until_arrival == 0U);
    GS1_SYSTEM_TEST_CHECK(context, delivery_payload.item_id == gs1::k_item_food_pack);
    GS1_SYSTEM_TEST_CHECK(context, delivery_payload.quantity == 1U);

    drain_task_reward_messages(context, campaign, site_run, queue);
    const auto* reward_option =
        find_task_reward_option(site_run.task_board.visible_tasks.front(), gs1::k_reward_candidate_site1_food_delivery);
    GS1_SYSTEM_TEST_REQUIRE(context, reward_option != nullptr);
    GS1_SYSTEM_TEST_CHECK(context, reward_option->selected);
    GS1_SYSTEM_TEST_CHECK(
        context,
        site_run.inventory.pending_delivery_queue.empty());
    GS1_SYSTEM_TEST_CHECK(
        context,
        gs1::inventory_storage::available_item_quantity_in_container(
            site_run,
            gs1::inventory_storage::delivery_box_container(site_run),
            gs1::ItemId {gs1::k_item_food_pack}) == 1U);
}

void task_board_claim_reward_reveals_unlockable_and_blocks_duplicate_claim(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 1007U);
    GameMessageQueue queue {};
    auto task_context = make_site_context<TaskBoardSystem>(campaign, site_run, queue);
    auto economy_context = make_site_context<EconomyPhoneSystem>(campaign, site_run, queue);

    const auto start_message =
        make_message(GameMessageType::SiteRunStarted, SiteRunStartedMessage {1U, 1U, 101U, 1U, 42ULL});
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        EconomyPhoneSystem::process_message(economy_context, start_message) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        TaskBoardSystem::process_message(task_context, start_message) == GS1_STATUS_OK);

    const auto task_id = site_run.task_board.visible_tasks.front().task_instance_id.value;
    complete_seeded_task(context, task_context, site_run, queue);
    queue.clear();
    site_run.task_board.visible_tasks.front().reward_draft_options[1].reward_candidate_id =
        gs1::RewardCandidateId {gs1::k_reward_candidate_site1_unlockable_reveal};

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        TaskBoardSystem::process_message(
            task_context,
            make_message(
                GameMessageType::TaskRewardClaimRequested,
                gs1::TaskRewardClaimRequestedMessage {
                    task_id,
                    gs1::k_reward_candidate_site1_unlockable_reveal})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(context, queue.size() == 1U);
    GS1_SYSTEM_TEST_CHECK(context, queue.front().type == GameMessageType::SiteUnlockableRevealRequested);
    GS1_SYSTEM_TEST_CHECK(
        context,
        queue.front().payload_as<gs1::SiteUnlockableRevealRequestedMessage>().unlockable_id == 102U);

    drain_task_reward_messages(context, campaign, site_run, queue);
    const auto* reward_option = find_task_reward_option(
        site_run.task_board.visible_tasks.front(),
        gs1::k_reward_candidate_site1_unlockable_reveal);
    GS1_SYSTEM_TEST_REQUIRE(context, reward_option != nullptr);
    GS1_SYSTEM_TEST_CHECK(context, reward_option->selected);
    GS1_SYSTEM_TEST_CHECK(context, site_run.economy.revealed_site_unlockable_ids.size() == 2U);
    GS1_SYSTEM_TEST_CHECK(context, site_run.economy.revealed_site_unlockable_ids[1] == 102U);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        TaskBoardSystem::process_message(
            task_context,
            make_message(
                GameMessageType::TaskRewardClaimRequested,
                gs1::TaskRewardClaimRequestedMessage {
                    task_id,
                    gs1::k_reward_candidate_site1_money_stipend})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, queue.empty());
    std::size_t selected_reward_count = 0U;
    for (const auto& reward_draft_option : site_run.task_board.visible_tasks.front().reward_draft_options)
    {
        if (reward_draft_option.selected)
        {
            selected_reward_count += 1U;
        }
    }
    GS1_SYSTEM_TEST_CHECK(context, selected_reward_count == 1U);
}

void task_board_claim_reward_can_activate_run_modifier(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 1008U);
    GameMessageQueue queue {};
    auto task_context = make_site_context<TaskBoardSystem>(campaign, site_run, queue);
    auto modifier_context = make_site_context<ModifierSystem>(campaign, site_run, queue);

    const auto start_message =
        make_message(GameMessageType::SiteRunStarted, SiteRunStartedMessage {1U, 1U, 101U, 1U, 42ULL});
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ModifierSystem::process_message(modifier_context, start_message) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        TaskBoardSystem::process_message(task_context, start_message) == GS1_STATUS_OK);

    const auto task_id = site_run.task_board.visible_tasks.front().task_instance_id.value;
    complete_seeded_task(context, task_context, site_run, queue);
    queue.clear();
    site_run.task_board.visible_tasks.front().reward_draft_options[0].reward_candidate_id =
        gs1::RewardCandidateId {gs1::k_reward_candidate_site1_run_modifier};

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        TaskBoardSystem::process_message(
            task_context,
            make_message(
                GameMessageType::TaskRewardClaimRequested,
                gs1::TaskRewardClaimRequestedMessage {
                    task_id,
                    gs1::k_reward_candidate_site1_run_modifier})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(context, queue.size() == 1U);
    GS1_SYSTEM_TEST_CHECK(context, queue.front().type == GameMessageType::RunModifierAwardRequested);
    GS1_SYSTEM_TEST_CHECK(
        context,
        queue.front().payload_as<gs1::RunModifierAwardRequestedMessage>().modifier_id == 4U);

    drain_task_reward_messages(context, campaign, site_run, queue);
    const auto* reward_option =
        find_task_reward_option(site_run.task_board.visible_tasks.front(), gs1::k_reward_candidate_site1_run_modifier);
    GS1_SYSTEM_TEST_REQUIRE(context, reward_option != nullptr);
    GS1_SYSTEM_TEST_CHECK(context, reward_option->selected);
    GS1_SYSTEM_TEST_CHECK(context, site_run.modifier.active_run_modifier_ids.size() == 1U);
    GS1_SYSTEM_TEST_CHECK(context, site_run.modifier.active_run_modifier_ids.front().value == 4U);
}
}  // namespace

GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "inventory",
    "site_one_seed_is_applied_once",
    inventory_site_one_seed_is_applied_once);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "inventory",
    "non_site_seed_and_run_resize_slots",
    inventory_non_site_seed_and_run_resize_slots);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "inventory",
    "item_use_validates_and_emits_meter_delta",
    inventory_item_use_validates_and_emits_meter_delta);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "inventory",
    "transfer_moves_and_merges_stacks",
    inventory_transfer_moves_and_merges_stacks);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "inventory",
    "device_storage_items_must_route_through_worker_pack",
    inventory_device_storage_items_must_route_through_worker_pack);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "inventory",
    "item_consume_removes_quantity_across_matching_stacks",
    inventory_item_consume_removes_quantity_across_matching_stacks);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "inventory",
    "item_use_drink_defers_item_and_meter_changes_until_action_completion",
    inventory_item_use_drink_defers_item_and_meter_changes_until_action_completion);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "inventory",
    "delivery_queues_only_overflow_until_delivery_crate_space_opens",
    inventory_delivery_queues_only_overflow_until_delivery_crate_space_opens);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "economy_phone",
    "site_run_started_seeds_site_one_and_resets_other_sites",
    economy_site_run_started_seeds_site_one_and_resets_other_sites);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "economy_phone",
    "purchase_sell_hire_and_unlockable_paths_update_money",
    economy_purchase_sell_hire_and_unlockable_paths_update_money);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "economy_phone",
    "cart_checkout_charges_delivery_fee_and_clears_cart",
    economy_cart_checkout_charges_delivery_fee_and_clears_cart);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "economy_phone",
    "purchase_listing_unlockable_path_spends_money_and_removes_direct_unlockable",
    economy_purchase_listing_unlockable_path_spends_money_and_removes_direct_unlockable);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "economy_phone",
    "invalid_listing_or_money_returns_failures",
    economy_invalid_listing_or_money_returns_failures);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "economy_phone",
    "repeated_sell_requests_do_not_oversubscribe_inventory",
    economy_repeated_sell_requests_do_not_oversubscribe_inventory);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "economy_phone",
    "repeated_cart_add_requests_clamp_without_failure",
    economy_repeated_cart_add_requests_clamp_without_failure);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "task_board",
    "site_run_started_seeds_site_one_board",
    task_board_site_run_started_seeds_site_one_board);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "task_board",
    "non_site_run_started_clears_existing_board_state",
    task_board_non_site_run_started_clears_existing_board_state);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "task_board",
    "accept_respects_cap_and_list_kind",
    task_board_accept_respects_cap_and_list_kind);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "task_board",
    "completion_queues_faction_reputation_once",
    task_board_completion_queues_faction_reputation_once);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "task_board",
    "buy_task_completes_from_successful_purchase",
    task_board_buy_task_completes_from_successful_purchase);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "task_board",
    "transfer_task_completes_from_successful_transfer",
    task_board_transfer_task_completes_from_successful_transfer);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "task_board",
    "claim_reward_marks_selected_and_applies_money_reward",
    task_board_claim_reward_marks_selected_and_applies_money_reward);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "task_board",
    "claim_reward_delivers_to_crate_immediately_and_marks_selected",
    task_board_claim_reward_delivers_to_crate_immediately_and_marks_selected);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "task_board",
    "claim_reward_reveals_unlockable_and_blocks_duplicate_claim",
    task_board_claim_reward_reveals_unlockable_and_blocks_duplicate_claim);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "task_board",
    "claim_reward_can_activate_run_modifier",
    task_board_claim_reward_can_activate_run_modifier);
