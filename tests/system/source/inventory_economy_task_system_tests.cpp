#include "commands/game_command.h"
#include "content/defs/item_defs.h"
#include "site/systems/economy_phone_system.h"
#include "site/systems/inventory_system.h"
#include "site/systems/task_board_system.h"
#include "testing/system_test_registry.h"
#include "system_test_fixtures.h"

namespace
{
using gs1::EconomyPhoneSystem;
using gs1::GameCommand;
using gs1::GameCommandQueue;
using gs1::GameCommandType;
using gs1::InventorySystem;
using gs1::PhoneListingKind;
using gs1::SiteRunStartedCommand;
using gs1::TaskBoardSystem;
using gs1::TaskRuntimeListKind;
using namespace gs1::testing::fixtures;

template <typename Payload>
GameCommand make_command(gs1::GameCommandType type, const Payload& payload)
{
    GameCommand command {};
    command.type = type;
    command.set_payload(payload);
    return command;
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

const gs1::site_ecs::StorageItemStack* starter_storage_slot_stack(
    gs1::SiteRunState& site_run,
    std::uint32_t slot_index)
{
    const auto container = starter_storage_container(site_run);
    const auto slot = gs1::inventory_storage::slot_entity(site_run, container, slot_index);
    const auto item = gs1::inventory_storage::item_entity_for_slot(site_run, slot);
    return gs1::inventory_storage::stack_data(site_run, item);
}

const gs1::site_ecs::StorageItemStack* container_slot_stack(
    gs1::SiteRunState& site_run,
    flecs::entity container,
    std::uint32_t slot_index)
{
    const auto slot = gs1::inventory_storage::slot_entity(site_run, container, slot_index);
    const auto item = gs1::inventory_storage::item_entity_for_slot(site_run, slot);
    return gs1::inventory_storage::stack_data(site_run, item);
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
    GameCommandQueue inventory_queue {};
    auto inventory_context = make_site_context<InventorySystem>(campaign, site_run, inventory_queue);
    const auto status = InventorySystem::process_command(
        inventory_context,
        make_command(
            GameCommandType::SiteRunStarted,
            SiteRunStartedCommand {site_run.site_id.value, 1U, site_run.site_archetype_id, 1U, 42ULL}));
    (void)status;
}

void inventory_site_one_seed_is_applied_once(gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 801U);
    GameCommandQueue queue {};
    auto site_context = make_site_context<InventorySystem>(campaign, site_run, queue);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        InventorySystem::process_command(
            site_context,
            make_command(
                GameCommandType::SiteRunStarted,
                SiteRunStartedCommand {1U, 1U, 101U, 1U, 42ULL})) == GS1_STATUS_OK);

    GS1_SYSTEM_TEST_CHECK(context, site_run.inventory.worker_pack_slots[0].occupied);
    GS1_SYSTEM_TEST_CHECK(context, site_run.inventory.worker_pack_slots[0].item_id.value == 1U);
    GS1_SYSTEM_TEST_CHECK(context, site_run.inventory.worker_pack_slots[0].item_quantity == 2U);
    GS1_SYSTEM_TEST_CHECK(context, site_run.inventory.worker_pack_slots[1].item_id.value == 2U);
    GS1_SYSTEM_TEST_CHECK(context, site_run.inventory.worker_pack_slots[2].item_id.value == 3U);
    GS1_SYSTEM_TEST_CHECK(
        context,
        gs1::inventory_storage::available_item_quantity_in_container(
            site_run,
            starter_storage_container(site_run),
            gs1::ItemId {gs1::k_item_wind_reed_seed_bundle}) == 8U);
    GS1_SYSTEM_TEST_CHECK(
        context,
        gs1::inventory_storage::available_item_quantity_in_container(
            site_run,
            starter_storage_container(site_run),
            gs1::ItemId {gs1::k_item_wood_bundle}) == 6U);
    GS1_SYSTEM_TEST_CHECK(
        context,
        gs1::inventory_storage::available_item_quantity_in_container(
            site_run,
            starter_storage_container(site_run),
            gs1::ItemId {gs1::k_item_iron_bundle}) == 4U);

    {
        auto container = gs1::inventory_storage::worker_pack_container(site_run);
        auto slot = gs1::inventory_storage::slot_entity(site_run, container, 0U);
        auto item = gs1::inventory_storage::item_entity_for_slot(site_run, slot);
        auto& stack = item.get_mut<gs1::site_ecs::StorageItemStack>();
        stack.quantity = 99U;
        item.modified<gs1::site_ecs::StorageItemStack>();
        gs1::inventory_storage::sync_projection_views(site_run);
    }
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        InventorySystem::process_command(
            site_context,
            make_command(
                GameCommandType::SiteRunStarted,
                SiteRunStartedCommand {1U, 1U, 101U, 1U, 42ULL})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, site_run.inventory.worker_pack_slots[0].item_quantity == 99U);
}

void inventory_non_site_seed_and_run_resize_slots(gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(2U, 802U);
    GameCommandQueue queue {};
    auto site_context = make_site_context<InventorySystem>(campaign, site_run, queue);

    site_run.inventory.worker_pack_slots.resize(1U);
    InventorySystem::run(site_context);
    GS1_SYSTEM_TEST_CHECK(
        context,
        site_run.inventory.worker_pack_slots.size() == site_run.inventory.worker_pack_slot_count);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        InventorySystem::process_command(
            site_context,
            make_command(
                GameCommandType::SiteRunStarted,
                SiteRunStartedCommand {2U, 1U, 102U, 1U, 43ULL})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, site_run.inventory.worker_pack_slots[0].occupied);
    GS1_SYSTEM_TEST_CHECK(context, site_run.inventory.worker_pack_slots[0].item_id.value == 1U);
    GS1_SYSTEM_TEST_CHECK(
        context,
        gs1::inventory_storage::available_item_quantity_in_container(
            site_run,
            starter_storage_container(site_run),
            gs1::ItemId {gs1::k_item_wind_reed_seed_bundle}) == 8U);
}

void inventory_item_use_validates_and_emits_meter_delta(gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 803U);
    GameCommandQueue queue {};
    auto site_context = make_site_context<InventorySystem>(campaign, site_run, queue);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        InventorySystem::process_command(
            site_context,
            make_command(
                GameCommandType::SiteRunStarted,
                SiteRunStartedCommand {1U, 1U, 101U, 1U, 42ULL})) == GS1_STATUS_OK);

    queue.clear();
    GS1_SYSTEM_TEST_CHECK(
        context,
        InventorySystem::process_command(
            site_context,
            make_command(
                GameCommandType::InventoryItemUseRequested,
                gs1::InventoryItemUseRequestedCommand {
                    999U,
                    1U,
                    GS1_INVENTORY_CONTAINER_WORKER_PACK,
                    0U})) == GS1_STATUS_INVALID_ARGUMENT);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        InventorySystem::process_command(
            site_context,
            make_command(
                GameCommandType::InventoryItemUseRequested,
                gs1::InventoryItemUseRequestedCommand {
                    1U,
                    1U,
                    GS1_INVENTORY_CONTAINER_WORKER_PACK,
                    0U})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, site_run.inventory.worker_pack_slots[0].item_quantity == 1U);
    GS1_SYSTEM_TEST_REQUIRE(context, queue.size() == 1U);
    GS1_SYSTEM_TEST_CHECK(context, queue.front().type == GameCommandType::WorkerMeterDeltaRequested);
    GS1_SYSTEM_TEST_CHECK(
        context,
        approx_equal(
            queue.front().payload_as<gs1::WorkerMeterDeltaRequestedCommand>().hydration_delta,
            12.0f));
}

void inventory_transfer_moves_and_merges_stacks(gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 804U);
    GameCommandQueue queue {};
    auto site_context = make_site_context<InventorySystem>(campaign, site_run, queue);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        InventorySystem::process_command(
            site_context,
            make_command(
                GameCommandType::SiteRunStarted,
                SiteRunStartedCommand {1U, 1U, 101U, 1U, 42ULL})) == GS1_STATUS_OK);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        InventorySystem::process_command(
            site_context,
            make_command(
                GameCommandType::InventoryTransferRequested,
                gs1::InventoryTransferRequestedCommand {
                    0U,
                    3U,
                    1U,
                    GS1_INVENTORY_CONTAINER_WORKER_PACK,
                    GS1_INVENTORY_CONTAINER_WORKER_PACK,
                    0U,
                    0U,
                    0U,
                    0U})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, site_run.inventory.worker_pack_slots[0].item_quantity == 1U);
    GS1_SYSTEM_TEST_CHECK(context, site_run.inventory.worker_pack_slots[3].item_id.value == 1U);
    GS1_SYSTEM_TEST_CHECK(context, site_run.inventory.worker_pack_slots[3].item_quantity == 1U);

    site_run.inventory.worker_pack_slots[4] = site_run.inventory.worker_pack_slots[3];
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        InventorySystem::process_command(
            site_context,
            make_command(
                GameCommandType::InventoryTransferRequested,
                gs1::InventoryTransferRequestedCommand {
                    3U,
                    4U,
                    1U,
                    GS1_INVENTORY_CONTAINER_WORKER_PACK,
                    GS1_INVENTORY_CONTAINER_WORKER_PACK,
                    0U,
                    0U,
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
    GameCommandQueue queue {};
    auto site_context = make_site_context<InventorySystem>(campaign, site_run, queue);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        InventorySystem::process_command(
            site_context,
            make_command(
                GameCommandType::SiteRunStarted,
                SiteRunStartedCommand {1U, 1U, 101U, 1U, 42ULL})) == GS1_STATUS_OK);

    const auto workbench_tile = default_starter_workbench_tile(site_run.camp.camp_anchor_tile);
    const auto workbench_owner_id =
        static_cast<std::uint32_t>(site_run.site_world->device_entity_id(workbench_tile));
    const auto starter_owner_id =
        static_cast<std::uint32_t>(site_run.site_world->device_entity_id(site_run.camp.starter_storage_tile));
    const auto workbench_storage =
        gs1::inventory_storage::find_device_storage_container(site_run, workbench_owner_id);
    const auto starter_storage = starter_storage_container(site_run);
    const auto worker_pack = gs1::inventory_storage::worker_pack_container(site_run);
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
        InventorySystem::process_command(
            site_context,
            make_command(
                GameCommandType::InventoryTransferRequested,
                gs1::InventoryTransferRequestedCommand {
                    0U,
                    0U,
                    0U,
                    GS1_INVENTORY_CONTAINER_WORKER_PACK,
                    GS1_INVENTORY_CONTAINER_DEVICE_STORAGE,
                    gs1::k_inventory_transfer_flag_resolve_destination_in_dll,
                    0U,
                    0U,
                    0U})) == GS1_STATUS_INVALID_STATE);
    GS1_SYSTEM_TEST_CHECK(context, site_run.inventory.worker_pack_slots[0].occupied);
    GS1_SYSTEM_TEST_CHECK(context, site_run.inventory.worker_pack_slots[0].item_quantity == 2U);

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
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        find_first_empty_worker_pack_slot(site_run) == site_run.inventory.worker_pack_slots.size());

    GS1_SYSTEM_TEST_CHECK(
        context,
        InventorySystem::process_command(
            site_context,
            make_command(
                GameCommandType::InventoryTransferRequested,
                gs1::InventoryTransferRequestedCommand {
                    0U,
                    0U,
                    0U,
                    GS1_INVENTORY_CONTAINER_DEVICE_STORAGE,
                    GS1_INVENTORY_CONTAINER_WORKER_PACK,
                    gs1::k_inventory_transfer_flag_resolve_destination_in_dll,
                    0U,
                    workbench_owner_id,
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
        InventorySystem::process_command(
            site_context,
            make_command(
                GameCommandType::InventoryTransferRequested,
                gs1::InventoryTransferRequestedCommand {
                    0U,
                    0U,
                    0U,
                    GS1_INVENTORY_CONTAINER_DEVICE_STORAGE,
                    GS1_INVENTORY_CONTAINER_WORKER_PACK,
                    gs1::k_inventory_transfer_flag_resolve_destination_in_dll,
                    0U,
                    workbench_owner_id,
                    0U})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(context, container_slot_stack(site_run, workbench_storage, 0U) == nullptr);
    GS1_SYSTEM_TEST_CHECK(
        context,
        site_run.inventory.worker_pack_slots[3].item_id.value == gs1::k_item_storage_crate_kit);
    GS1_SYSTEM_TEST_CHECK(context, site_run.inventory.worker_pack_slots[3].item_quantity == 1U);

    GS1_SYSTEM_TEST_CHECK(
        context,
        InventorySystem::process_command(
            site_context,
            make_command(
                GameCommandType::InventoryTransferRequested,
                gs1::InventoryTransferRequestedCommand {
                    3U,
                    0U,
                    0U,
                    GS1_INVENTORY_CONTAINER_WORKER_PACK,
                    GS1_INVENTORY_CONTAINER_DEVICE_STORAGE,
                    gs1::k_inventory_transfer_flag_resolve_destination_in_dll,
                    0U,
                    0U,
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
        InventorySystem::process_command(
            site_context,
            make_command(
                GameCommandType::InventoryTransferRequested,
                gs1::InventoryTransferRequestedCommand {
                    3U,
                    0U,
                    0U,
                    GS1_INVENTORY_CONTAINER_WORKER_PACK,
                    GS1_INVENTORY_CONTAINER_DEVICE_STORAGE,
                    gs1::k_inventory_transfer_flag_resolve_destination_in_dll,
                    0U,
                    0U,
                    starter_owner_id})) == GS1_STATUS_OK);

    GS1_SYSTEM_TEST_CHECK(
        context,
        !site_run.inventory.worker_pack_slots[3].occupied);
    GS1_SYSTEM_TEST_CHECK(
        context,
        gs1::inventory_storage::available_item_quantity_in_container(
            site_run,
            starter_storage,
            gs1::ItemId {gs1::k_item_storage_crate_kit}) == starter_storage_kit_before + 1U);
}

void inventory_item_consume_removes_quantity_across_matching_stacks(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 805U);
    GameCommandQueue queue {};
    auto site_context = make_site_context<InventorySystem>(campaign, site_run, queue);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        InventorySystem::process_command(
            site_context,
            make_command(
                GameCommandType::SiteRunStarted,
                SiteRunStartedCommand {1U, 1U, 101U, 1U, 42ULL})) == GS1_STATUS_OK);

    site_run.inventory.worker_pack_slots[3].occupied = true;
    site_run.inventory.worker_pack_slots[3].item_id = gs1::ItemId {gs1::k_item_water_container};
    site_run.inventory.worker_pack_slots[3].item_quantity = 2U;
    site_run.inventory.worker_pack_slots[3].item_condition = 1.0f;
    site_run.inventory.worker_pack_slots[3].item_freshness = 1.0f;

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        InventorySystem::process_command(
            site_context,
            make_command(
                GameCommandType::InventoryItemConsumeRequested,
                gs1::InventoryItemConsumeRequestedCommand {
                    gs1::k_item_water_container,
                    3U,
                    GS1_INVENTORY_CONTAINER_WORKER_PACK,
                    0U})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, !site_run.inventory.worker_pack_slots[0].occupied);
    GS1_SYSTEM_TEST_CHECK(context, site_run.inventory.worker_pack_slots[3].occupied);
    GS1_SYSTEM_TEST_CHECK(context, site_run.inventory.worker_pack_slots[3].item_quantity == 1U);
}

void inventory_delivery_request_arrives_in_starter_device_storage(gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 805U);
    GameCommandQueue queue {};
    auto site_context = make_site_context<InventorySystem>(campaign, site_run, queue);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        InventorySystem::process_command(
            site_context,
            make_command(
                GameCommandType::InventoryDeliveryRequested,
                gs1::InventoryDeliveryRequestedCommand {
                    gs1::k_item_wind_reed_seed_bundle,
                    3U,
                    1U})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, site_run.inventory.pending_delivery_queue.size() == 1U);
    GS1_SYSTEM_TEST_CHECK(context, starter_storage_slot_stack(site_run, 0U) == nullptr);

    for (int step = 0; step < 4; ++step)
    {
        InventorySystem::run(site_context);
    }
    GS1_SYSTEM_TEST_CHECK(context, starter_storage_slot_stack(site_run, 0U) == nullptr);

    for (int step = 0; step < 2 && !site_run.inventory.pending_delivery_queue.empty(); ++step)
    {
        InventorySystem::run(site_context);
    }
    GS1_SYSTEM_TEST_CHECK(context, site_run.inventory.pending_delivery_queue.empty());
    GS1_SYSTEM_TEST_REQUIRE(context, starter_storage_slot_stack(site_run, 0U) != nullptr);
    GS1_SYSTEM_TEST_CHECK(
        context,
        starter_storage_slot_stack(site_run, 0U)->item_id.value == gs1::k_item_wind_reed_seed_bundle);
    GS1_SYSTEM_TEST_CHECK(context, starter_storage_slot_stack(site_run, 0U)->quantity == 3U);
}

void economy_site_run_started_seeds_site_one_and_resets_other_sites(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_one_run = make_test_site_run(1U, 901U);
    auto other_site_run = make_test_site_run(2U, 902U);
    GameCommandQueue site_one_queue {};
    GameCommandQueue other_queue {};

    auto site_one_context = make_site_context<EconomyPhoneSystem>(campaign, site_one_run, site_one_queue);
    auto other_context = make_site_context<EconomyPhoneSystem>(campaign, other_site_run, other_queue);
    const auto started_one = make_command(
        GameCommandType::SiteRunStarted,
        SiteRunStartedCommand {1U, 1U, 101U, 1U, 42ULL});
    const auto started_two = make_command(
        GameCommandType::SiteRunStarted,
        SiteRunStartedCommand {2U, 1U, 102U, 1U, 42ULL});

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        EconomyPhoneSystem::process_command(site_one_context, started_one) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, site_one_run.economy.money == 45);
    GS1_SYSTEM_TEST_CHECK(context, site_one_run.economy.available_phone_listings.size() >= 11U);
    GS1_SYSTEM_TEST_CHECK(context, site_one_run.economy.direct_purchase_unlockable_ids.size() == 1U);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        EconomyPhoneSystem::process_command(other_context, started_two) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, other_site_run.economy.money == 0);
    GS1_SYSTEM_TEST_CHECK(context, other_site_run.economy.available_phone_listings.empty());
}

void economy_purchase_sell_hire_and_unlockable_paths_update_money(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 903U);
    GameCommandQueue queue {};
    auto site_context = make_site_context<EconomyPhoneSystem>(campaign, site_run, queue);
    seed_site_one_inventory(campaign, site_run);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        EconomyPhoneSystem::process_command(
            site_context,
            make_command(
                GameCommandType::SiteRunStarted,
                SiteRunStartedCommand {1U, 1U, 101U, 1U, 42ULL})) == GS1_STATUS_OK);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        EconomyPhoneSystem::process_command(
            site_context,
            make_command(
                GameCommandType::PhoneListingPurchaseRequested,
                gs1::PhoneListingPurchaseRequestedCommand {1U, 2U, 0U})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, site_run.economy.money == 35);
    GS1_SYSTEM_TEST_REQUIRE(context, find_listing_by_id(site_run.economy, 1U) != nullptr);
    GS1_SYSTEM_TEST_CHECK(context, find_listing_by_id(site_run.economy, 1U)->quantity == 4U);
    GS1_SYSTEM_TEST_REQUIRE(context, queue.size() == 1U);
    GS1_SYSTEM_TEST_CHECK(context, queue.front().type == GameCommandType::InventoryDeliveryRequested);
    GS1_SYSTEM_TEST_CHECK(
        context,
        queue.front().payload_as<gs1::InventoryDeliveryRequestedCommand>().item_id ==
            gs1::k_item_water_container);

    queue.clear();
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        EconomyPhoneSystem::process_command(
            site_context,
            make_command(
                GameCommandType::PhoneListingSaleRequested,
                gs1::PhoneListingSaleRequestedCommand {1001U, 2U, 0U})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, site_run.economy.money == 41);
    GS1_SYSTEM_TEST_REQUIRE(context, find_listing_by_id(site_run.economy, 1001U) != nullptr);
    GS1_SYSTEM_TEST_CHECK(context, find_listing_by_id(site_run.economy, 1001U)->quantity == 0U);
    GS1_SYSTEM_TEST_REQUIRE(context, queue.size() == 1U);
    GS1_SYSTEM_TEST_CHECK(context, queue.front().type == GameCommandType::InventoryGlobalItemConsumeRequested);
    GS1_SYSTEM_TEST_CHECK(
        context,
        queue.front().payload_as<gs1::InventoryGlobalItemConsumeRequestedCommand>().item_id ==
            gs1::k_item_water_container);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        EconomyPhoneSystem::process_command(
            site_context,
            make_command(
                GameCommandType::ContractorHireRequested,
                gs1::ContractorHireRequestedCommand {10U, 1U})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, site_run.economy.money == 33);
    GS1_SYSTEM_TEST_REQUIRE(context, find_listing_by_id(site_run.economy, 10U) != nullptr);
    GS1_SYSTEM_TEST_CHECK(context, find_listing_by_id(site_run.economy, 10U)->quantity == 2U);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        EconomyPhoneSystem::process_command(
            site_context,
            make_command(
                GameCommandType::SiteUnlockablePurchaseRequested,
                gs1::SiteUnlockablePurchaseRequestedCommand {101U})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, site_run.economy.money == 13);
    GS1_SYSTEM_TEST_CHECK(context, site_run.economy.direct_purchase_unlockable_ids.empty());
    GS1_SYSTEM_TEST_CHECK(context, site_run.economy.revealed_site_unlockable_ids.size() == 1U);
}

void economy_purchase_listing_unlockable_path_spends_money_and_removes_direct_unlockable(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 905U);
    GameCommandQueue queue {};
    auto site_context = make_site_context<EconomyPhoneSystem>(campaign, site_run, queue);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        EconomyPhoneSystem::process_command(
            site_context,
            make_command(
                GameCommandType::SiteRunStarted,
                SiteRunStartedCommand {1U, 1U, 101U, 1U, 42ULL})) == GS1_STATUS_OK);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        EconomyPhoneSystem::process_command(
            site_context,
            make_command(
                GameCommandType::PhoneListingPurchaseRequested,
                gs1::PhoneListingPurchaseRequestedCommand {11U, 1U, 0U})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, site_run.economy.money == 25);
    GS1_SYSTEM_TEST_CHECK(context, site_run.economy.direct_purchase_unlockable_ids.empty());
    GS1_SYSTEM_TEST_CHECK(context, site_run.economy.revealed_site_unlockable_ids.size() == 1U);
}

void economy_invalid_listing_or_money_returns_failures(gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 904U);
    GameCommandQueue queue {};
    auto site_context = make_site_context<EconomyPhoneSystem>(campaign, site_run, queue);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        EconomyPhoneSystem::process_command(
            site_context,
            make_command(
                GameCommandType::SiteRunStarted,
                SiteRunStartedCommand {1U, 1U, 101U, 1U, 42ULL})) == GS1_STATUS_OK);

    GS1_SYSTEM_TEST_CHECK(
        context,
        EconomyPhoneSystem::process_command(
            site_context,
            make_command(
                GameCommandType::PhoneListingPurchaseRequested,
                gs1::PhoneListingPurchaseRequestedCommand {999U, 1U, 0U})) == GS1_STATUS_NOT_FOUND);

    GS1_SYSTEM_TEST_CHECK(
        context,
        EconomyPhoneSystem::process_command(
            site_context,
            make_command(
                GameCommandType::PhoneListingPurchaseRequested,
                gs1::PhoneListingPurchaseRequestedCommand {1U, 50U, 0U})) == GS1_STATUS_INVALID_STATE);

    site_run.economy.money = 0;
    GS1_SYSTEM_TEST_CHECK(
        context,
        EconomyPhoneSystem::process_command(
            site_context,
            make_command(
                GameCommandType::ContractorHireRequested,
                gs1::ContractorHireRequestedCommand {9U, 1U})) == GS1_STATUS_INVALID_STATE);
}

void task_board_site_run_started_seeds_site_one_board(gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 1001U);
    GameCommandQueue queue {};
    auto site_context = make_site_context<TaskBoardSystem>(campaign, site_run, queue);

    site_run.counters.site_completion_tile_threshold = 5U;
    site_run.counters.fully_grown_tile_count = 2U;
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        TaskBoardSystem::process_command(
            site_context,
            make_command(
                GameCommandType::SiteRunStarted,
                SiteRunStartedCommand {1U, 1U, 101U, 1U, 42ULL})) == GS1_STATUS_OK);

    GS1_SYSTEM_TEST_REQUIRE(context, site_run.task_board.visible_tasks.size() == 1U);
    const auto& task = site_run.task_board.visible_tasks.front();
    GS1_SYSTEM_TEST_CHECK(context, task.target_amount == 5U);
    GS1_SYSTEM_TEST_CHECK(context, task.current_progress_amount == 2U);
    GS1_SYSTEM_TEST_CHECK(context, task.runtime_list_kind == TaskRuntimeListKind::Visible);
}

void task_board_non_site_run_started_clears_existing_board_state(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(2U, 1004U);
    GameCommandQueue queue {};
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
        TaskBoardSystem::process_command(
            site_context,
            make_command(
                GameCommandType::SiteRunStarted,
                SiteRunStartedCommand {2U, 1U, 102U, 1U, 42ULL})) == GS1_STATUS_OK);

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
    GameCommandQueue queue {};
    auto site_context = make_site_context<TaskBoardSystem>(campaign, site_run, queue);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        TaskBoardSystem::process_command(
            site_context,
            make_command(
                GameCommandType::SiteRunStarted,
                SiteRunStartedCommand {1U, 1U, 101U, 1U, 42ULL})) == GS1_STATUS_OK);

    const auto task_id = site_run.task_board.visible_tasks.front().task_instance_id.value;
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        TaskBoardSystem::process_command(
            site_context,
            make_command(
                GameCommandType::TaskAcceptRequested,
                gs1::TaskAcceptRequestedCommand {task_id})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, site_run.task_board.accepted_task_ids.size() == 1U);
    GS1_SYSTEM_TEST_CHECK(
        context,
        site_run.task_board.visible_tasks.front().runtime_list_kind == TaskRuntimeListKind::Accepted);

    site_run.task_board.accepted_task_cap = 0U;
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        TaskBoardSystem::process_command(
            site_context,
            make_command(
                GameCommandType::TaskAcceptRequested,
                gs1::TaskAcceptRequestedCommand {task_id})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, site_run.task_board.accepted_task_ids.size() == 1U);
}

void task_board_restoration_progress_completes_task(gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 1003U);
    GameCommandQueue queue {};
    auto site_context = make_site_context<TaskBoardSystem>(campaign, site_run, queue);

    site_run.counters.site_completion_tile_threshold = 2U;
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        TaskBoardSystem::process_command(
            site_context,
            make_command(
                GameCommandType::SiteRunStarted,
                SiteRunStartedCommand {1U, 1U, 101U, 1U, 42ULL})) == GS1_STATUS_OK);

    const auto task_id = site_run.task_board.visible_tasks.front().task_instance_id.value;
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        TaskBoardSystem::process_command(
            site_context,
            make_command(
                GameCommandType::TaskAcceptRequested,
                gs1::TaskAcceptRequestedCommand {task_id})) == GS1_STATUS_OK);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        TaskBoardSystem::process_command(
            site_context,
            make_command(
                GameCommandType::RestorationProgressChanged,
                gs1::RestorationProgressChangedCommand {2U, 2U, 1.0f})) == GS1_STATUS_OK);

    GS1_SYSTEM_TEST_CHECK(
        context,
        site_run.task_board.visible_tasks.front().runtime_list_kind == TaskRuntimeListKind::Completed);
    GS1_SYSTEM_TEST_CHECK(context, site_run.task_board.completed_task_ids.size() == 1U);
    GS1_SYSTEM_TEST_CHECK(context, site_run.task_board.accepted_task_ids.empty());
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
    "delivery_request_arrives_in_starter_device_storage",
    inventory_delivery_request_arrives_in_starter_device_storage);
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
    "purchase_listing_unlockable_path_spends_money_and_removes_direct_unlockable",
    economy_purchase_listing_unlockable_path_spends_money_and_removes_direct_unlockable);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "economy_phone",
    "invalid_listing_or_money_returns_failures",
    economy_invalid_listing_or_money_returns_failures);
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
    "restoration_progress_completes_task",
    task_board_restoration_progress_completes_task);
