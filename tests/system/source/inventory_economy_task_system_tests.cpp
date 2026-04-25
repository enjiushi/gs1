#include "messages/game_message.h"
#include "content/defs/plant_defs.h"
#include "content/defs/item_defs.h"
#include "content/defs/faction_defs.h"
#include "content/defs/structure_defs.h"
#include "content/defs/task_defs.h"
#include "content/prototype_content.h"
#include "site/systems/action_execution_system.h"
#include "site/systems/device_maintenance_system.h"
#include "site/systems/device_weather_contribution_system.h"
#include "site/systems/ecology_system.h"
#include "site/systems/economy_phone_system.h"
#include "site/systems/inventory_system.h"
#include "site/systems/local_weather_resolve_system.h"
#include "site/systems/modifier_system.h"
#include "site/systems/plant_weather_contribution_system.h"
#include "site/systems/task_board_system.h"
#include "site/systems/worker_condition_system.h"
#include "testing/system_test_registry.h"
#include "system_test_fixtures.h"

namespace
{
using gs1::ActionExecutionSystem;
using gs1::DeviceMaintenanceSystem;
using gs1::DeviceWeatherContributionSystem;
using gs1::EcologySystem;
using gs1::EconomyPhoneSystem;
using gs1::GameMessage;
using gs1::GameMessageQueue;
using gs1::GameMessageType;
using gs1::InventorySystem;
using gs1::LocalWeatherResolveSystem;
using gs1::PhoneListingKind;
using gs1::PlantWeatherContributionSystem;
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

void run_local_weather_pipeline(
    gs1::CampaignState& campaign,
    gs1::SiteRunState& site_run,
    GameMessageQueue& queue)
{
    auto plant_context =
        make_site_context<PlantWeatherContributionSystem>(campaign, site_run, queue);
    auto device_context =
        make_site_context<DeviceWeatherContributionSystem>(campaign, site_run, queue);
    auto local_weather_context =
        make_site_context<LocalWeatherResolveSystem>(campaign, site_run, queue);

    PlantWeatherContributionSystem::run(plant_context);
    DeviceWeatherContributionSystem::run(device_context);
    LocalWeatherResolveSystem::run(local_weather_context);
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

gs1::PhoneListingState* find_listing_by_item_id(
    gs1::EconomyState& economy,
    gs1::PhoneListingKind kind,
    gs1::ItemId item_id)
{
    for (auto& listing : economy.available_phone_listings)
    {
        if (listing.kind == kind && listing.item_id == item_id)
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
        gs1::ItemId {gs1::k_item_ordos_wormwood_seed_bundle},
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
        gs1::ItemId {gs1::k_item_ordos_wormwood_seed_bundle},
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

void inventory_item_use_food_restores_nourishment_without_refilling_energy(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 807U);
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
    site_run.inventory.worker_pack_slots[0].item_id = gs1::ItemId {gs1::k_item_food_pack};
    site_run.inventory.worker_pack_slots[0].item_quantity = 2U;
    site_run.inventory.worker_pack_slots[0].item_condition = 1.0f;
    site_run.inventory.worker_pack_slots[0].item_freshness = 1.0f;

    auto worker_conditions = site_run.site_world->worker_conditions();
    worker_conditions.health = 100.0f;
    worker_conditions.hydration = 100.0f;
    worker_conditions.nourishment = 35.0f;
    worker_conditions.energy = 20.0f;
    site_run.site_world->set_worker_conditions(worker_conditions);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        InventorySystem::process_message(
            inventory_context,
            make_message(
                GameMessageType::InventoryItemUseRequested,
                gs1::InventoryItemUseRequestedMessage {
                    gs1::k_item_food_pack,
                    site_run.inventory.worker_pack_storage_id,
                    1U,
                    0U})) == GS1_STATUS_OK);

    GS1_SYSTEM_TEST_REQUIRE(context, queue.size() == 1U);
    GS1_SYSTEM_TEST_CHECK(context, queue.front().type == GameMessageType::StartSiteAction);
    GS1_SYSTEM_TEST_CHECK(context, site_run.inventory.worker_pack_slots[0].item_quantity == 2U);
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(site_run.site_world->worker_conditions().nourishment, 35.0f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(site_run.site_world->worker_conditions().energy, 20.0f));

    const auto start_action_message = queue.front();
    queue.clear();
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ActionExecutionSystem::process_message(action_context, start_action_message) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(context, site_run.site_action.current_action_id.has_value());
    GS1_SYSTEM_TEST_CHECK(context, site_run.site_action.action_kind == gs1::ActionKind::Eat);
    GS1_SYSTEM_TEST_CHECK(context, site_run.site_action.started_at_world_minute.has_value());
    GS1_SYSTEM_TEST_REQUIRE(context, queue.size() == 1U);
    GS1_SYSTEM_TEST_CHECK(context, queue.front().type == GameMessageType::SiteActionStarted);

    queue.clear();
    ActionExecutionSystem::run(action_context);
    GS1_SYSTEM_TEST_CHECK(context, count_messages(queue, GameMessageType::SiteActionCompleted) == 1U);
    GS1_SYSTEM_TEST_CHECK(context, count_messages(queue, GameMessageType::InventoryItemConsumeRequested) == 1U);
    GS1_SYSTEM_TEST_CHECK(context, count_messages(queue, GameMessageType::WorkerMeterDeltaRequested) == 1U);
    GS1_SYSTEM_TEST_CHECK(context, site_run.inventory.worker_pack_slots[0].item_quantity == 2U);
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(site_run.site_world->worker_conditions().nourishment, 35.0f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(site_run.site_world->worker_conditions().energy, 20.0f));

    const auto* consume_message =
        first_message(queue, GameMessageType::InventoryItemConsumeRequested);
    const auto* meter_message =
        first_message(queue, GameMessageType::WorkerMeterDeltaRequested);
    GS1_SYSTEM_TEST_REQUIRE(context, consume_message != nullptr);
    GS1_SYSTEM_TEST_REQUIRE(context, meter_message != nullptr);
    GS1_SYSTEM_TEST_CHECK(
        context,
        meter_message->payload_as<gs1::WorkerMeterDeltaRequestedMessage>().flags ==
            gs1::WORKER_METER_CHANGED_NOURISHMENT);
    GS1_SYSTEM_TEST_CHECK(
        context,
        approx_equal(
            meter_message->payload_as<gs1::WorkerMeterDeltaRequestedMessage>().nourishment_delta,
            15.0f));
    GS1_SYSTEM_TEST_CHECK(
        context,
        approx_equal(
            meter_message->payload_as<gs1::WorkerMeterDeltaRequestedMessage>().energy_delta,
            0.0f));

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        InventorySystem::process_message(inventory_context, *consume_message) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        WorkerConditionSystem::process_message(worker_context, *meter_message) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, site_run.inventory.worker_pack_slots[0].occupied);
    GS1_SYSTEM_TEST_CHECK(context, site_run.inventory.worker_pack_slots[0].item_quantity == 1U);
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(site_run.site_world->worker_conditions().nourishment, 50.0f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(site_run.site_world->worker_conditions().energy, 20.0f));
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
                    gs1::k_item_ordos_wormwood_seed_bundle,
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
            gs1::ItemId {gs1::k_item_ordos_wormwood_seed_bundle}) == 0U);

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
        delivery_box_slot_stack(site_run, 0U)->item_id.value == gs1::k_item_ordos_wormwood_seed_bundle);
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
    GS1_SYSTEM_TEST_CHECK(context, site_one_run.economy.available_phone_listings.size() >= 9U);
    GS1_SYSTEM_TEST_CHECK(context, site_one_run.economy.direct_purchase_unlockable_ids.size() == 1U);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        EconomyPhoneSystem::process_message(other_context, started_two) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, other_site_run.economy.money == 45);
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
    GS1_SYSTEM_TEST_REQUIRE(context, queue.size() == 3U);
    GS1_SYSTEM_TEST_CHECK(context, queue.front().type == GameMessageType::CampaignCashDeltaRequested);
    GS1_SYSTEM_TEST_CHECK(
        context,
        queue[1].payload_as<gs1::InventoryDeliveryBatchRequestedMessage>().entries[0].item_id ==
            gs1::k_item_water_container);
    GS1_SYSTEM_TEST_CHECK(
        context,
        queue[1].payload_as<gs1::InventoryDeliveryBatchRequestedMessage>().entries[0].quantity == 2U);
    GS1_SYSTEM_TEST_CHECK(context, queue[1].type == GameMessageType::InventoryDeliveryBatchRequested);
    GS1_SYSTEM_TEST_CHECK(context, queue[2].type == GameMessageType::PhoneListingPurchased);

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
    GS1_SYSTEM_TEST_REQUIRE(context, queue.size() == 3U);
    GS1_SYSTEM_TEST_CHECK(context, queue.front().type == GameMessageType::CampaignCashDeltaRequested);
    GS1_SYSTEM_TEST_CHECK(
        context,
        queue[1].payload_as<gs1::InventoryGlobalItemConsumeRequestedMessage>().item_id ==
            gs1::k_item_water_container);
    GS1_SYSTEM_TEST_CHECK(context, queue[1].type == GameMessageType::InventoryGlobalItemConsumeRequested);
    GS1_SYSTEM_TEST_CHECK(context, queue[2].type == GameMessageType::PhoneListingSold);

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
    GS1_SYSTEM_TEST_REQUIRE(context, queue.size() == 4U);
    GS1_SYSTEM_TEST_CHECK(context, queue.front().type == GameMessageType::CampaignCashDeltaRequested);
    GS1_SYSTEM_TEST_CHECK(
        context,
        queue[1].payload_as<gs1::InventoryDeliveryBatchRequestedMessage>().entry_count == 2U);
    GS1_SYSTEM_TEST_CHECK(context, queue[1].type == GameMessageType::InventoryDeliveryBatchRequested);
    GS1_SYSTEM_TEST_CHECK(context, queue[2].type == GameMessageType::PhoneListingPurchased);
    GS1_SYSTEM_TEST_CHECK(context, queue[3].type == GameMessageType::PhoneListingPurchased);
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
    GS1_SYSTEM_TEST_REQUIRE(context, queue.size() == 6U);
    GS1_SYSTEM_TEST_CHECK(context, queue[0].type == GameMessageType::CampaignCashDeltaRequested);
    GS1_SYSTEM_TEST_CHECK(context, queue[1].type == GameMessageType::InventoryGlobalItemConsumeRequested);
    GS1_SYSTEM_TEST_CHECK(context, queue[2].type == GameMessageType::PhoneListingSold);
    GS1_SYSTEM_TEST_CHECK(context, queue[3].type == GameMessageType::CampaignCashDeltaRequested);
    GS1_SYSTEM_TEST_CHECK(context, queue[4].type == GameMessageType::InventoryGlobalItemConsumeRequested);
    GS1_SYSTEM_TEST_CHECK(context, queue[5].type == GameMessageType::PhoneListingSold);

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

void seed_placeholder_task_board_for_tests(gs1::SiteRunState& site_run)
{
    auto& board = site_run.task_board;
    board.visible_tasks.clear();
    board.accepted_task_ids.clear();
    board.completed_task_ids.clear();
    board.claimed_task_ids.clear();

    auto add_task = [&](gs1::TaskInstanceState task) {
        board.visible_tasks.push_back(task);
    };

    add_task(gs1::TaskInstanceState {
        gs1::TaskInstanceId {1U},
        gs1::TaskTemplateId {gs1::k_task_template_site1_restore_patch},
        gs1::FactionId {gs1::k_faction_forestry_bureau},
        1U,
        site_run.counters.site_completion_tile_threshold,
        0U,
        0U,
        {},
        {},
        {},
        {},
        {},
        {},
        gs1::ActionKind::None,
        0.0f});
    add_task(gs1::TaskInstanceState {
        gs1::TaskInstanceId {2U},
        gs1::TaskTemplateId {gs1::k_task_template_site1_buy_water},
        gs1::FactionId {gs1::k_faction_village_committee},
        1U,
        1U,
        0U,
        0U,
        gs1::ItemId {gs1::k_item_water_container},
        {},
        {},
        {},
        {},
        {},
        gs1::ActionKind::None,
        0.0f});
    add_task(gs1::TaskInstanceState {
        gs1::TaskInstanceId {3U},
        gs1::TaskTemplateId {gs1::k_task_template_site1_transfer_seeds},
        gs1::FactionId {gs1::k_faction_village_committee},
        1U,
        2U,
        0U,
        0U,
        gs1::ItemId {gs1::k_item_basic_straw_checkerboard},
        {},
        {},
        {},
        {},
        {},
        gs1::ActionKind::None,
        0.0f});
    add_task(gs1::TaskInstanceState {
        gs1::TaskInstanceId {4U},
        gs1::TaskTemplateId {gs1::k_task_template_site1_build_camp_stove},
        gs1::FactionId {gs1::k_faction_agricultural_university},
        1U,
        1U,
        0U,
        0U,
        {},
        {},
        {},
        gs1::StructureId {gs1::k_structure_camp_stove},
        {},
        {},
        gs1::ActionKind::None,
        0.0f});
    add_task(gs1::TaskInstanceState {
        gs1::TaskInstanceId {5U},
        gs1::TaskTemplateId {gs1::k_task_template_site1_keep_worker_meters_high},
        gs1::FactionId {gs1::k_faction_village_committee},
        2U,
        4U,
        0U,
        0U,
        {},
        {},
        {},
        {},
        {},
        {},
        gs1::ActionKind::None,
        85.0f});
    add_task(gs1::TaskInstanceState {
        gs1::TaskInstanceId {6U},
        gs1::TaskTemplateId {gs1::k_task_template_site1_keep_living_plants_stable},
        gs1::FactionId {gs1::k_faction_forestry_bureau},
        2U,
        4U,
        2U,
        0U,
        {},
        {},
        {},
        {},
        {},
        {},
        gs1::ActionKind::None,
        0.0f});
    add_task(gs1::TaskInstanceState {
        gs1::TaskInstanceId {7U},
        gs1::TaskTemplateId {gs1::k_task_template_site1_submit_water},
        gs1::FactionId {gs1::k_faction_village_committee},
        1U,
        2U,
        0U,
        0U,
        gs1::ItemId {gs1::k_item_water_container},
        {},
        {},
        {},
        {},
        {},
        gs1::ActionKind::None,
        0.0f});

    board.task_pool_size = static_cast<std::uint32_t>(board.visible_tasks.size());
    board.minutes_until_next_refresh = 0.0;
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

void start_task_board_with_owner_snapshots(
    gs1::testing::SystemTestExecutionContext& context,
    gs1::CampaignState& campaign,
    gs1::SiteRunState& site_run,
    GameMessageQueue& queue,
    gs1::SiteSystemContext<gs1::TaskBoardSystem>& task_context)
{
    auto ecology_context = make_site_context<EcologySystem>(campaign, site_run, queue);
    auto local_weather_context = make_site_context<LocalWeatherResolveSystem>(campaign, site_run, queue);
    auto device_context = make_site_context<DeviceMaintenanceSystem>(campaign, site_run, queue);
    auto worker_context = make_site_context<WorkerConditionSystem>(campaign, site_run, queue);

    const auto start_message =
        make_message(GameMessageType::SiteRunStarted, SiteRunStartedMessage {1U, 1U, 101U, 1U, 42ULL});
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        TaskBoardSystem::process_message(task_context, start_message) == GS1_STATUS_OK);
    seed_placeholder_task_board_for_tests(site_run);
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        EcologySystem::process_message(ecology_context, start_message) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        LocalWeatherResolveSystem::process_message(local_weather_context, start_message) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        DeviceMaintenanceSystem::process_message(device_context, start_message) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        WorkerConditionSystem::process_message(worker_context, start_message) == GS1_STATUS_OK);

    run_local_weather_pipeline(campaign, site_run, queue);

    while (!queue.empty())
    {
        const auto message = queue.front();
        queue.pop_front();
        if (TaskBoardSystem::subscribes_to(message.type))
        {
            GS1_SYSTEM_TEST_REQUIRE(
                context,
                TaskBoardSystem::process_message(task_context, message) == GS1_STATUS_OK);
        }
    }
}

void task_board_site_run_started_leaves_board_empty_until_tasks_are_authored(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 1001U);
    GameMessageQueue queue {};
    auto site_context = make_site_context<TaskBoardSystem>(campaign, site_run, queue);

    site_run.counters.site_completion_tile_threshold = 5U;
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        TaskBoardSystem::process_message(
            site_context,
            make_message(
                GameMessageType::SiteRunStarted,
                SiteRunStartedMessage {1U, 1U, 101U, 1U, 42ULL})) == GS1_STATUS_OK);

    GS1_SYSTEM_TEST_CHECK(context, site_run.task_board.visible_tasks.empty());
    GS1_SYSTEM_TEST_CHECK(context, site_run.task_board.task_pool_size == 0U);
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(site_run.task_board.minutes_until_next_refresh, 0.0));
    GS1_SYSTEM_TEST_CHECK(
        context,
        site_run.task_board.tracked_tiles.size() ==
            static_cast<std::size_t>(site_run.task_board.tracked_tile_width) *
                static_cast<std::size_t>(site_run.task_board.tracked_tile_height));
    GS1_SYSTEM_TEST_CHECK(context, site_run.task_board.site_completion_tile_threshold == 5U);
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

    start_task_board_with_owner_snapshots(context, campaign, site_run, queue, site_context);

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

void task_board_completion_does_not_queue_faction_reputation(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 1003U);
    GameMessageQueue queue {};
    auto site_context = make_site_context<TaskBoardSystem>(campaign, site_run, queue);

    site_run.counters.site_completion_tile_threshold = 2U;
    start_task_board_with_owner_snapshots(context, campaign, site_run, queue, site_context);

    const auto task_id = site_run.task_board.visible_tasks.front().task_instance_id.value;
    complete_seeded_task(context, site_context, site_run, queue);
    GS1_SYSTEM_TEST_CHECK(
        context,
        site_run.task_board.visible_tasks.front().runtime_list_kind == TaskRuntimeListKind::Completed);
    GS1_SYSTEM_TEST_CHECK(context, site_run.task_board.completed_task_ids.size() == 1U);
    GS1_SYSTEM_TEST_CHECK(context, site_run.task_board.accepted_task_ids.empty());
    GS1_SYSTEM_TEST_CHECK(context, queue.empty());

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        TaskBoardSystem::process_message(
            site_context,
            make_message(
                GameMessageType::RestorationProgressChanged,
                gs1::RestorationProgressChangedMessage {2U, 2U, 1.0f})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, count_queued_messages(queue, GameMessageType::FactionReputationAwardRequested) == 0U);
    GS1_SYSTEM_TEST_CHECK(context, campaign.faction_progress[0].faction_reputation == 0);
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
    start_task_board_with_owner_snapshots(context, campaign, site_run, queue, task_context);

    auto* buy_task =
        find_task_by_template_id(site_run.task_board, gs1::k_task_template_site1_buy_water);
    GS1_SYSTEM_TEST_REQUIRE(context, buy_task != nullptr);
    GS1_SYSTEM_TEST_REQUIRE(context, buy_task->item_id.value != 0U);
    auto* buy_listing =
        find_listing_by_item_id(site_run.economy, PhoneListingKind::BuyItem, buy_task->item_id);
    GS1_SYSTEM_TEST_REQUIRE(context, buy_listing != nullptr);
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
                gs1::PhoneListingPurchaseRequestedMessage {
                    buy_listing->listing_id,
                    static_cast<std::uint16_t>(buy_task->target_amount),
                    0U})) == GS1_STATUS_OK);
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
    start_task_board_with_owner_snapshots(context, campaign, site_run, queue, task_context);

    auto* transfer_task =
        find_task_by_template_id(site_run.task_board, gs1::k_task_template_site1_transfer_seeds);
    GS1_SYSTEM_TEST_REQUIRE(context, transfer_task != nullptr);
    GS1_SYSTEM_TEST_REQUIRE(context, transfer_task->item_id.value != 0U);
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
        transfer_task->item_id,
        transfer_task->target_amount);
    const auto starter_storage_id =
        gs1::inventory_storage::storage_id_for_container(site_run, starter_storage);
    queue.clear();
    for (std::uint32_t transferred = 0U; transferred < transfer_task->target_amount; ++transferred)
    {
        const auto seed_slot_index = find_container_slot_with_item(
            site_run,
            starter_storage,
            transfer_task->item_id);
        GS1_SYSTEM_TEST_REQUIRE(
            context,
            seed_slot_index < gs1::inventory_storage::slot_count_in_container(site_run, starter_storage));

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
                        0U,
                        1U,
                        gs1::k_inventory_transfer_flag_resolve_destination_in_dll,
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
    }

    transfer_task =
        find_task_by_template_id(site_run.task_board, gs1::k_task_template_site1_transfer_seeds);
    GS1_SYSTEM_TEST_REQUIRE(context, transfer_task != nullptr);
    GS1_SYSTEM_TEST_CHECK(context, transfer_task->runtime_list_kind == TaskRuntimeListKind::Completed);
}

void task_board_build_task_completes_from_device_placement(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 1006U);
    GameMessageQueue queue {};
    auto task_context = make_site_context<TaskBoardSystem>(campaign, site_run, queue);

    start_task_board_with_owner_snapshots(context, campaign, site_run, queue, task_context);

    auto* build_task =
        find_task_by_template_id(site_run.task_board, gs1::k_task_template_site1_build_camp_stove);
    GS1_SYSTEM_TEST_REQUIRE(context, build_task != nullptr);
    GS1_SYSTEM_TEST_REQUIRE(context, build_task->structure_id.value != 0U);
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        TaskBoardSystem::process_message(
            task_context,
            make_message(
                GameMessageType::TaskAcceptRequested,
                gs1::TaskAcceptRequestedMessage {build_task->task_instance_id.value})) == GS1_STATUS_OK);

    for (std::uint32_t count = 0U; count < build_task->target_amount; ++count)
    {
        GS1_SYSTEM_TEST_REQUIRE(
            context,
            TaskBoardSystem::process_message(
                task_context,
                make_message(
                    GameMessageType::SiteDevicePlaced,
                    gs1::SiteDevicePlacedMessage {
                        9U + count,
                        4 + static_cast<int>(count),
                        4,
                        build_task->structure_id.value,
                        0U})) == GS1_STATUS_OK);
    }

    build_task =
        find_task_by_template_id(site_run.task_board, gs1::k_task_template_site1_build_camp_stove);
    GS1_SYSTEM_TEST_REQUIRE(context, build_task != nullptr);
    GS1_SYSTEM_TEST_CHECK(context, build_task->runtime_list_kind == TaskRuntimeListKind::Completed);
}

void task_board_worker_meter_duration_task_tracks_time_and_resets(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 1007U);
    GameMessageQueue queue {};
    auto task_context = make_site_context<TaskBoardSystem>(campaign, site_run, queue);

    start_task_board_with_owner_snapshots(context, campaign, site_run, queue, task_context);

    auto* meter_task =
        find_task_by_template_id(site_run.task_board, gs1::k_task_template_site1_keep_worker_meters_high);
    GS1_SYSTEM_TEST_REQUIRE(context, meter_task != nullptr);
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        TaskBoardSystem::process_message(
            task_context,
            make_message(
                GameMessageType::TaskAcceptRequested,
                gs1::TaskAcceptRequestedMessage {meter_task->task_instance_id.value})) == GS1_STATUS_OK);

    for (std::uint32_t index = 0U; index < 3U; ++index)
    {
        TaskBoardSystem::run(task_context);
    }

    meter_task =
        find_task_by_template_id(site_run.task_board, gs1::k_task_template_site1_keep_worker_meters_high);
    GS1_SYSTEM_TEST_REQUIRE(context, meter_task != nullptr);
    GS1_SYSTEM_TEST_CHECK(context, meter_task->current_progress_amount >= 2U);
    GS1_SYSTEM_TEST_CHECK(context, meter_task->runtime_list_kind == TaskRuntimeListKind::Accepted);

    auto worker = site_run.site_world->worker();
    worker.conditions.hydration = 40.0f;
    site_run.site_world->set_worker(worker);
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        TaskBoardSystem::process_message(
            task_context,
            make_message(
                GameMessageType::WorkerMetersChanged,
                gs1::WorkerMetersChangedMessage {
                    gs1::WORKER_METER_CHANGED_HYDRATION,
                    worker.conditions.health,
                    worker.conditions.hydration,
                    worker.conditions.nourishment,
                    worker.conditions.energy_cap,
                    worker.conditions.energy,
                    worker.conditions.morale,
                    worker.conditions.work_efficiency})) == GS1_STATUS_OK);
    TaskBoardSystem::run(task_context);

    meter_task =
        find_task_by_template_id(site_run.task_board, gs1::k_task_template_site1_keep_worker_meters_high);
    GS1_SYSTEM_TEST_REQUIRE(context, meter_task != nullptr);
    GS1_SYSTEM_TEST_CHECK(context, meter_task->current_progress_amount == 0U);

    worker.conditions.hydration = 100.0f;
    site_run.site_world->set_worker(worker);
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        TaskBoardSystem::process_message(
            task_context,
            make_message(
                GameMessageType::WorkerMetersChanged,
                gs1::WorkerMetersChangedMessage {
                    gs1::WORKER_METER_CHANGED_HYDRATION,
                    worker.conditions.health,
                    worker.conditions.hydration,
                    worker.conditions.nourishment,
                    worker.conditions.energy_cap,
                    worker.conditions.energy,
                    worker.conditions.morale,
                    worker.conditions.work_efficiency})) == GS1_STATUS_OK);
    for (std::uint32_t index = 0U; index < 7U; ++index)
    {
        TaskBoardSystem::run(task_context);
    }

    meter_task =
        find_task_by_template_id(site_run.task_board, gs1::k_task_template_site1_keep_worker_meters_high);
    GS1_SYSTEM_TEST_REQUIRE(context, meter_task != nullptr);
    GS1_SYSTEM_TEST_CHECK(context, meter_task->runtime_list_kind == TaskRuntimeListKind::Completed);
}

void task_board_living_plant_duration_task_ignores_straw_and_resets_on_density_drop(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 1009U);
    GameMessageQueue queue {};
    auto task_context = make_site_context<TaskBoardSystem>(campaign, site_run, queue);
    auto ecology_context = make_site_context<EcologySystem>(campaign, site_run, queue);

    start_task_board_with_owner_snapshots(context, campaign, site_run, queue, task_context);

    auto* plant_task =
        find_task_by_template_id(site_run.task_board, gs1::k_task_template_site1_keep_living_plants_stable);
    GS1_SYSTEM_TEST_REQUIRE(context, plant_task != nullptr);
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        TaskBoardSystem::process_message(
            task_context,
            make_message(
                GameMessageType::TaskAcceptRequested,
                gs1::TaskAcceptRequestedMessage {plant_task->task_instance_id.value})) == GS1_STATUS_OK);

    auto first_tile = site_run.site_world->tile_at({2, 2});
    first_tile.ecology.plant_id = gs1::PlantId {gs1::k_plant_ordos_wormwood};
    first_tile.ecology.plant_density = 36.0f;
    first_tile.ecology.moisture = 100.0f;
    first_tile.ecology.soil_fertility = 100.0f;
    first_tile.ecology.soil_salinity = 0.0f;
    site_run.site_world->set_tile({2, 2}, first_tile);

    auto second_tile = site_run.site_world->tile_at({3, 2});
    second_tile.ecology.plant_id = gs1::PlantId {gs1::k_plant_white_thorn};
    second_tile.ecology.plant_density = 34.0f;
    second_tile.ecology.moisture = 100.0f;
    second_tile.ecology.soil_fertility = 100.0f;
    second_tile.ecology.soil_salinity = 0.0f;
    site_run.site_world->set_tile({3, 2}, second_tile);

    auto straw_tile = site_run.site_world->tile_at({4, 2});
    straw_tile.ecology.plant_id = gs1::PlantId {gs1::k_plant_straw_checkerboard};
    straw_tile.ecology.plant_density = 100.0f;
    straw_tile.ecology.moisture = 100.0f;
    straw_tile.ecology.soil_fertility = 100.0f;
    site_run.site_world->set_tile({4, 2}, straw_tile);

    auto drain_task_messages = [&]() {
        while (!queue.empty())
        {
            const auto message = queue.front();
            queue.pop_front();
            if (TaskBoardSystem::subscribes_to(message.type))
            {
                GS1_SYSTEM_TEST_REQUIRE(
                    context,
                    TaskBoardSystem::process_message(task_context, message) == GS1_STATUS_OK);
            }
        }
    };

    for (std::uint32_t index = 0U; index < 3U; ++index)
    {
        EcologySystem::run(ecology_context);
        drain_task_messages();
        TaskBoardSystem::run(task_context);
    }

    plant_task =
        find_task_by_template_id(site_run.task_board, gs1::k_task_template_site1_keep_living_plants_stable);
    GS1_SYSTEM_TEST_REQUIRE(context, plant_task != nullptr);
    GS1_SYSTEM_TEST_CHECK(context, plant_task->current_progress_amount >= 1U);
    GS1_SYSTEM_TEST_CHECK(context, plant_task->runtime_list_kind == TaskRuntimeListKind::Accepted);

    first_tile = site_run.site_world->tile_at({2, 2});
    first_tile.ecology.moisture = 0.0f;
    first_tile.ecology.soil_fertility = 0.0f;
    first_tile.local_weather.heat = 100.0f;
    first_tile.local_weather.wind = 100.0f;
    first_tile.local_weather.dust = 100.0f;
    site_run.site_world->set_tile({2, 2}, first_tile);
    EcologySystem::run(ecology_context);
    drain_task_messages();
    TaskBoardSystem::run(task_context);

    plant_task =
        find_task_by_template_id(site_run.task_board, gs1::k_task_template_site1_keep_living_plants_stable);
    GS1_SYSTEM_TEST_REQUIRE(context, plant_task != nullptr);
    GS1_SYSTEM_TEST_CHECK(context, plant_task->current_progress_amount == 0U);
    GS1_SYSTEM_TEST_CHECK(context, plant_task->runtime_list_kind == TaskRuntimeListKind::Accepted);

    first_tile = site_run.site_world->tile_at({2, 2});
    first_tile.ecology.moisture = 100.0f;
    first_tile.ecology.soil_fertility = 100.0f;
    first_tile.local_weather.heat = 0.0f;
    first_tile.local_weather.wind = 0.0f;
    first_tile.local_weather.dust = 0.0f;
    site_run.site_world->set_tile({2, 2}, first_tile);

    straw_tile = site_run.site_world->tile_at({4, 2});
    straw_tile.ecology.plant_density = 92.0f;
    site_run.site_world->set_tile({4, 2}, straw_tile);

    for (std::uint32_t index = 0U; index < plant_task->target_amount + 1U; ++index)
    {
        EcologySystem::run(ecology_context);
        drain_task_messages();
        TaskBoardSystem::run(task_context);
    }

    plant_task =
        find_task_by_template_id(site_run.task_board, gs1::k_task_template_site1_keep_living_plants_stable);
    GS1_SYSTEM_TEST_REQUIRE(context, plant_task != nullptr);
    GS1_SYSTEM_TEST_CHECK(context, plant_task->runtime_list_kind == TaskRuntimeListKind::Completed);
}

void task_board_reward_claim_is_ignored_without_draft_options(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 1008U);
    GameMessageQueue queue {};
    auto task_context = make_site_context<TaskBoardSystem>(campaign, site_run, queue);

    start_task_board_with_owner_snapshots(context, campaign, site_run, queue, task_context);

    const auto task_id = site_run.task_board.visible_tasks.front().task_instance_id.value;
    complete_seeded_task(context, task_context, site_run, queue);
    queue.clear();

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        TaskBoardSystem::process_message(
            task_context,
            make_message(
                GameMessageType::TaskRewardClaimRequested,
                gs1::TaskRewardClaimRequestedMessage {
                    task_id,
                    1U})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, queue.empty());
    GS1_SYSTEM_TEST_CHECK(
        context,
        site_run.task_board.visible_tasks.front().runtime_list_kind == TaskRuntimeListKind::Completed);
    GS1_SYSTEM_TEST_CHECK(context, site_run.task_board.claimed_task_ids.empty());
}

void task_board_content_tuning_exposes_internal_prices_and_task_scoring_inputs(
    gs1::testing::SystemTestExecutionContext& context)
{
    const auto* water = gs1::find_item_def(gs1::ItemId {gs1::k_item_water_container});
    GS1_SYSTEM_TEST_REQUIRE(context, water != nullptr);
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(water->hydration_delta, 20.0f));
    GS1_SYSTEM_TEST_CHECK(context, water->internal_price_cash_points == 200U);
    GS1_SYSTEM_TEST_CHECK(
        context,
        gs1::item_internal_price_cash_points(gs1::ItemId {gs1::k_item_water_container}) == 200U);

    const auto* food = gs1::find_item_def(gs1::ItemId {gs1::k_item_food_pack});
    GS1_SYSTEM_TEST_REQUIRE(context, food != nullptr);
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(food->nourishment_delta, 15.0f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(food->energy_delta, 0.0f));
    GS1_SYSTEM_TEST_CHECK(context, food->internal_price_cash_points == 420U);

    const auto* site_content = gs1::find_prototype_site_content(gs1::SiteId {1U});
    GS1_SYSTEM_TEST_REQUIRE(context, site_content != nullptr);
    bool found_unlockable_listing = false;
    for (const auto& listing : site_content->seeded_phone_listings)
    {
        if (listing.listing_id != 11U)
        {
            continue;
        }

        found_unlockable_listing = true;
        GS1_SYSTEM_TEST_CHECK(context, listing.kind == gs1::PhoneListingKind::PurchaseUnlockable);
        GS1_SYSTEM_TEST_CHECK(context, listing.internal_price_cash_points == 2000U);
        GS1_SYSTEM_TEST_CHECK(context, listing.price == 20);
    }
    GS1_SYSTEM_TEST_CHECK(context, found_unlockable_listing);

    const auto* submit_task =
        gs1::find_task_template_def(gs1::TaskTemplateId {gs1::k_task_template_site1_submit_water});
    GS1_SYSTEM_TEST_REQUIRE(context, submit_task != nullptr);
    GS1_SYSTEM_TEST_CHECK(context, submit_task->progress_kind == gs1::TaskProgressKind::SubmitItem);
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(submit_task->expected_task_hours_in_game, 0.5f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(submit_task->risk_multiplier, 0.05f));
}

void task_board_submit_task_completes_from_successful_submission(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 1010U);
    GameMessageQueue queue {};
    auto inventory_context = make_site_context<InventorySystem>(campaign, site_run, queue);
    auto task_context = make_site_context<TaskBoardSystem>(campaign, site_run, queue);

    const auto start_message =
        make_message(GameMessageType::SiteRunStarted, SiteRunStartedMessage {1U, 1U, 101U, 1U, 42ULL});
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        InventorySystem::process_message(inventory_context, start_message) == GS1_STATUS_OK);
    start_task_board_with_owner_snapshots(context, campaign, site_run, queue, task_context);

    auto* submit_task =
        find_task_by_template_id(site_run.task_board, gs1::k_task_template_site1_submit_water);
    GS1_SYSTEM_TEST_REQUIRE(context, submit_task != nullptr);
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        TaskBoardSystem::process_message(
            task_context,
            make_message(
                GameMessageType::TaskAcceptRequested,
                gs1::TaskAcceptRequestedMessage {submit_task->task_instance_id.value})) == GS1_STATUS_OK);

    const auto worker_pack = gs1::inventory_storage::worker_pack_container(site_run);
    (void)gs1::inventory_storage::add_item_to_container(
        site_run,
        worker_pack,
        submit_task->item_id,
        submit_task->target_amount);
    const auto worker_slot_index =
        find_container_slot_with_item(site_run, worker_pack, submit_task->item_id);
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        worker_slot_index < gs1::inventory_storage::slot_count_in_container(site_run, worker_pack));

    queue.clear();
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        InventorySystem::process_message(
            inventory_context,
            make_message(
                GameMessageType::InventoryItemSubmitRequested,
                gs1::InventoryItemSubmitRequestedMessage {
                    site_run.inventory.worker_pack_storage_id,
                    static_cast<std::uint16_t>(worker_slot_index),
                    static_cast<std::uint16_t>(submit_task->target_amount)})) == GS1_STATUS_OK);

    while (!queue.empty())
    {
        const auto message = queue.front();
        queue.pop_front();
        if (message.type == GameMessageType::InventoryItemSubmitted)
        {
            GS1_SYSTEM_TEST_REQUIRE(
                context,
                TaskBoardSystem::process_message(task_context, message) == GS1_STATUS_OK);
        }
    }

    submit_task =
        find_task_by_template_id(site_run.task_board, gs1::k_task_template_site1_submit_water);
    GS1_SYSTEM_TEST_REQUIRE(context, submit_task != nullptr);
    GS1_SYSTEM_TEST_CHECK(context, submit_task->runtime_list_kind == TaskRuntimeListKind::Completed);
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
    "item_use_food_restores_nourishment_without_refilling_energy",
    inventory_item_use_food_restores_nourishment_without_refilling_energy);
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
    "site_run_started_leaves_board_empty_until_tasks_are_authored",
    task_board_site_run_started_leaves_board_empty_until_tasks_are_authored);
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
    "completion_does_not_queue_faction_reputation",
    task_board_completion_does_not_queue_faction_reputation);
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
    "build_task_completes_from_device_placement",
    task_board_build_task_completes_from_device_placement);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "task_board",
    "worker_meter_duration_task_tracks_time_and_resets",
    task_board_worker_meter_duration_task_tracks_time_and_resets);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "task_board",
    "living_plant_duration_task_ignores_straw_and_resets_on_density_drop",
    task_board_living_plant_duration_task_ignores_straw_and_resets_on_density_drop);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "task_board",
    "reward_claim_is_ignored_without_draft_options",
    task_board_reward_claim_is_ignored_without_draft_options);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "task_board",
    "content_tuning_exposes_internal_prices_and_task_scoring_inputs",
    task_board_content_tuning_exposes_internal_prices_and_task_scoring_inputs);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "task_board",
    "submit_task_completes_from_successful_submission",
    task_board_submit_task_completes_from_successful_submission);
