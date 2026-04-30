#include <algorithm>

#include "messages/game_message.h"
#include "content/defs/gameplay_tuning_defs.h"
#include "content/defs/excavation_defs.h"
#include "content/defs/plant_defs.h"
#include "content/defs/item_defs.h"
#include "content/defs/faction_defs.h"
#include "content/defs/technology_defs.h"
#include "content/defs/structure_defs.h"
#include "content/defs/task_defs.h"
#include "content/prototype_content.h"
#include "campaign/systems/technology_system.h"
#include "site/defs/site_action_defs.h"
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
#include "support/currency.h"
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
using gs1::ModifierSystem;
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

constexpr std::int32_t reputation_for_progress_tier(std::uint32_t tier_index) noexcept
{
    return static_cast<std::int32_t>(tier_index) * 200;
}

std::uint32_t count_active_timed_buffs(const gs1::SiteRunState& site_run)
{
    return static_cast<std::uint32_t>(std::count_if(
        site_run.modifier.active_site_modifiers.begin(),
        site_run.modifier.active_site_modifiers.end(),
        [](const gs1::ActiveSiteModifierState& modifier) {
            return modifier.duration_world_minutes > 0.0;
        }));
}

const gs1::ActiveSiteModifierState* find_active_timed_buff(
    const gs1::SiteRunState& site_run,
    gs1::ModifierId modifier_id)
{
    const auto it = std::find_if(
        site_run.modifier.active_site_modifiers.begin(),
        site_run.modifier.active_site_modifiers.end(),
        [&](const gs1::ActiveSiteModifierState& modifier) {
            return modifier.duration_world_minutes > 0.0 &&
                modifier.modifier_id == modifier_id;
        });
    return it == site_run.modifier.active_site_modifiers.end() ? nullptr : &(*it);
}

gs1::ActiveSiteModifierState make_manual_timed_buff_state(
    gs1::ModifierId modifier_id)
{
    gs1::ActiveSiteModifierState modifier {};
    modifier.modifier_id = modifier_id;
    modifier.duration_world_minutes = 480.0;
    modifier.remaining_world_minutes = 480.0;
    return modifier;
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

std::uint64_t mix_seed(
    std::uint64_t base_seed,
    std::uint32_t a,
    std::uint32_t b) noexcept
{
    std::uint64_t value = base_seed ^ (static_cast<std::uint64_t>(a) << 32U) ^ static_cast<std::uint64_t>(b);
    value ^= value >> 33U;
    value *= 0xff51afd7ed558ccdULL;
    value ^= value >> 33U;
    value *= 0xc4ceb9fe1a85ec53ULL;
    value ^= value >> 33U;
    return value;
}

const gs1::PrototypePhoneBuyStockContent* find_phone_buy_stock_entry(
    std::uint32_t site_id,
    std::uint32_t listing_id)
{
    const auto* site_content = gs1::find_prototype_site_content(gs1::SiteId {site_id});
    if (site_content == nullptr)
    {
        return nullptr;
    }

    for (const auto& stock_entry : site_content->phone_buy_stock_pool)
    {
        if (stock_entry.listing_id == listing_id)
        {
            return &stock_entry;
        }
    }

    return nullptr;
}

std::uint32_t expected_generated_buy_quantity(
    const gs1::CampaignState& campaign,
    const gs1::SiteRunState& site_run,
    std::uint32_t listing_id,
    std::uint32_t generation = 1U)
{
    const auto* stock_entry =
        find_phone_buy_stock_entry(site_run.site_id.value, listing_id);
    if (stock_entry == nullptr)
    {
        return 0U;
    }

    const auto* item_def = gs1::find_item_def(stock_entry->item_id);
    if (item_def == nullptr)
    {
        return 0U;
    }

    if (item_def->linked_plant_id.value != 0U &&
        !gs1::TechnologySystem::plant_unlocked(campaign, item_def->linked_plant_id))
    {
        return 0U;
    }

    const auto unit_price = gs1::item_buy_price_cash_points(stock_entry->item_id);
    if (unit_price == 0U)
    {
        return 0U;
    }

    const auto span = stock_entry->stock_cash_points_variance * 2U + 1U;
    const auto roll = static_cast<std::uint32_t>(
        mix_seed(
            site_run.site_attempt_seed,
            generation ^ stock_entry->listing_id,
            stock_entry->item_id.value) %
        std::max<std::uint32_t>(span, 1U));
    const auto delta = static_cast<std::int64_t>(roll) -
        static_cast<std::int64_t>(stock_entry->stock_cash_points_variance);
    const auto total = static_cast<std::int64_t>(stock_entry->base_stock_cash_points) + delta;
    const auto stock_cash_points = total > 0 ? static_cast<std::uint32_t>(total) : 0U;
    return stock_cash_points == 0U
        ? 1U
        : std::max<std::uint32_t>(1U, stock_cash_points / unit_price);
}

std::uint32_t expected_available_buy_listing_count(
    const gs1::CampaignState& campaign,
    const gs1::SiteRunState& site_run)
{
    const auto* site_content = gs1::find_prototype_site_content(site_run.site_id);
    if (site_content == nullptr)
    {
        return 0U;
    }

    std::uint32_t count = 0U;
    for (const auto& stock_entry : site_content->phone_buy_stock_pool)
    {
        if (expected_generated_buy_quantity(campaign, site_run, stock_entry.listing_id) > 0U)
        {
            count += 1U;
        }
    }

    return count;
}

std::uint32_t count_phone_listings_of_kind(
    const gs1::EconomyState& economy,
    gs1::PhoneListingKind kind)
{
    return static_cast<std::uint32_t>(std::count_if(
        economy.available_phone_listings.begin(),
        economy.available_phone_listings.end(),
        [&](const gs1::PhoneListingState& listing) {
            return listing.kind == kind;
        }));
}

std::int32_t current_site_cash(const gs1::SiteRunState& site_run)
{
    return site_run.economy.current_cash;
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

void inventory_item_use_validates_and_emits_completion(gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 803U);
    GameMessageQueue queue {};
    auto site_context = make_site_context<InventorySystem>(campaign, site_run, queue);
    auto worker_context = make_site_context<WorkerConditionSystem>(campaign, site_run, queue);

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
    auto worker_conditions = site_run.site_world->worker_conditions();
    worker_conditions.health = 70.0f;
    site_run.site_world->set_worker_conditions(worker_conditions);

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
    GS1_SYSTEM_TEST_REQUIRE(context, queue.size() == 1U);
    GS1_SYSTEM_TEST_CHECK(context, queue.front().type == GameMessageType::InventoryItemUseCompleted);
    GS1_SYSTEM_TEST_CHECK(
        context,
        approx_equal(
            site_run.site_world->worker_conditions().health,
            70.0f));

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        WorkerConditionSystem::process_message(worker_context, queue.front()) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(
        context,
        approx_equal(
            site_run.site_world->worker_conditions().health,
            88.0f));
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
    GS1_SYSTEM_TEST_CHECK(context, count_messages(queue, GameMessageType::InventoryItemUseCompleted) == 0U);
    GS1_SYSTEM_TEST_CHECK(context, site_run.inventory.worker_pack_slots[0].item_quantity == 2U);
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(site_run.site_world->worker_conditions().hydration, 40.0f));

    const auto* consume_message =
        first_message(queue, GameMessageType::InventoryItemConsumeRequested);
    GS1_SYSTEM_TEST_REQUIRE(context, consume_message != nullptr);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        InventorySystem::process_message(inventory_context, *consume_message) == GS1_STATUS_OK);
    const auto* completed_message =
        first_message(queue, GameMessageType::InventoryItemUseCompleted);
    GS1_SYSTEM_TEST_REQUIRE(context, completed_message != nullptr);
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        WorkerConditionSystem::process_message(worker_context, *completed_message) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, site_run.inventory.worker_pack_slots[0].occupied);
    GS1_SYSTEM_TEST_CHECK(context, site_run.inventory.worker_pack_slots[0].item_quantity == 1U);
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(site_run.site_world->worker_conditions().hydration, 60.0f));
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
    GS1_SYSTEM_TEST_CHECK(context, count_messages(queue, GameMessageType::InventoryItemUseCompleted) == 0U);
    GS1_SYSTEM_TEST_CHECK(context, site_run.inventory.worker_pack_slots[0].item_quantity == 2U);
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(site_run.site_world->worker_conditions().nourishment, 35.0f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(site_run.site_world->worker_conditions().energy, 20.0f));

    const auto* consume_message =
        first_message(queue, GameMessageType::InventoryItemConsumeRequested);
    GS1_SYSTEM_TEST_REQUIRE(context, consume_message != nullptr);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        InventorySystem::process_message(inventory_context, *consume_message) == GS1_STATUS_OK);
    const auto* completed_message =
        first_message(queue, GameMessageType::InventoryItemUseCompleted);
    GS1_SYSTEM_TEST_REQUIRE(context, completed_message != nullptr);
    GS1_SYSTEM_TEST_CHECK(
        context,
        completed_message->payload_as<gs1::InventoryItemUseCompletedMessage>().item_id ==
            gs1::k_item_food_pack);
    GS1_SYSTEM_TEST_CHECK(
        context,
        completed_message->payload_as<gs1::InventoryItemUseCompletedMessage>().quantity == 1U);
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        WorkerConditionSystem::process_message(worker_context, *completed_message) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, site_run.inventory.worker_pack_slots[0].occupied);
    GS1_SYSTEM_TEST_CHECK(context, site_run.inventory.worker_pack_slots[0].item_quantity == 1U);
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(site_run.site_world->worker_conditions().nourishment, 50.0f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(site_run.site_world->worker_conditions().energy, 20.0f));
}

void focus_tonic_use_activates_timed_buff_and_adds_flat_buff_cash_points(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 808U);
    GameMessageQueue queue {};
    auto inventory_context = make_site_context<InventorySystem>(campaign, site_run, queue);
    auto worker_context = make_site_context<WorkerConditionSystem>(campaign, site_run, queue);
    auto modifier_context = make_site_context<ModifierSystem>(campaign, site_run, queue);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        InventorySystem::process_message(
            inventory_context,
            make_message(
                GameMessageType::SiteRunStarted,
                SiteRunStartedMessage {1U, 1U, 101U, 1U, 42ULL})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ModifierSystem::process_message(
            modifier_context,
            make_message(
                GameMessageType::SiteRunStarted,
                SiteRunStartedMessage {1U, 1U, 101U, 1U, 42ULL})) == GS1_STATUS_OK);
    queue.clear();

    (void)gs1::inventory_storage::add_item_to_container(
        site_run,
        gs1::inventory_storage::worker_pack_container(site_run),
        gs1::ItemId {gs1::k_item_focus_tonic},
        1U);
    auto worker_conditions = site_run.site_world->worker_conditions();
    worker_conditions.energy = 20.0f;
    worker_conditions.morale = 40.0f;
    site_run.site_world->set_worker_conditions(worker_conditions);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        InventorySystem::process_message(
            inventory_context,
            make_message(
                GameMessageType::InventoryItemUseRequested,
                gs1::InventoryItemUseRequestedMessage {
                    gs1::k_item_focus_tonic,
                    site_run.inventory.worker_pack_storage_id,
                    1U,
                    0U})) == GS1_STATUS_OK);

    const auto* completed_message =
        first_message(queue, GameMessageType::InventoryItemUseCompleted);
    GS1_SYSTEM_TEST_REQUIRE(context, completed_message != nullptr);
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        WorkerConditionSystem::process_message(worker_context, *completed_message) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ModifierSystem::process_message(modifier_context, *completed_message) == GS1_STATUS_OK);

    const auto* active_buff = find_active_timed_buff(site_run, gs1::ModifierId {3003U});
    GS1_SYSTEM_TEST_REQUIRE(context, active_buff != nullptr);
    GS1_SYSTEM_TEST_CHECK(context, count_active_timed_buffs(site_run) == 1U);
    GS1_SYSTEM_TEST_CHECK(context, active_buff->modifier_id.value == 3003U);
    GS1_SYSTEM_TEST_CHECK(
        context,
        approx_equal(site_run.site_world->worker_conditions().energy, 30.0f));
    GS1_SYSTEM_TEST_CHECK(
        context,
        approx_equal(site_run.site_world->worker_conditions().morale, 45.0f));
    GS1_SYSTEM_TEST_CHECK(
        context,
        approx_equal(
            site_run.modifier.resolved_action_cost_modifiers.energy_weight_delta,
            -0.25f));
    GS1_SYSTEM_TEST_CHECK(
        context,
        gs1::item_internal_price_cash_points(gs1::ItemId {gs1::k_item_focus_tonic}) == 385U);
    GS1_SYSTEM_TEST_CHECK(
        context,
        gs1::item_buy_price_cash_points(gs1::ItemId {gs1::k_item_focus_tonic}) == 423U);
}

void timed_modifiers_refresh_duplicate_id_and_expire_by_game_time(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 809U);
    GameMessageQueue queue {};
    auto modifier_context = make_site_context<ModifierSystem>(campaign, site_run, queue, 300.0);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ModifierSystem::process_message(
            modifier_context,
            make_message(
                GameMessageType::SiteRunStarted,
                SiteRunStartedMessage {1U, 1U, 101U, 1U, 42ULL})) == GS1_STATUS_OK);

    const auto tea_completed = make_message(
        GameMessageType::InventoryItemUseCompleted,
        gs1::InventoryItemUseCompletedMessage {
            gs1::k_item_field_tea,
            1U,
            0U});
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ModifierSystem::process_message(modifier_context, tea_completed) == GS1_STATUS_OK);
    const auto* tea_buff = find_active_timed_buff(site_run, gs1::ModifierId {3001U});
    GS1_SYSTEM_TEST_REQUIRE(context, tea_buff != nullptr);
    GS1_SYSTEM_TEST_REQUIRE(context, count_active_timed_buffs(site_run) == 1U);
    GS1_SYSTEM_TEST_CHECK(
        context,
        approx_equal(
            static_cast<float>(tea_buff->remaining_world_minutes),
            480.0f));

    ModifierSystem::run(modifier_context);
    tea_buff = find_active_timed_buff(site_run, gs1::ModifierId {3001U});
    GS1_SYSTEM_TEST_REQUIRE(context, tea_buff != nullptr);
    GS1_SYSTEM_TEST_CHECK(
        context,
        approx_equal(
            static_cast<float>(tea_buff->remaining_world_minutes),
            240.0f));

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ModifierSystem::process_message(modifier_context, tea_completed) == GS1_STATUS_OK);
    tea_buff = find_active_timed_buff(site_run, gs1::ModifierId {3001U});
    GS1_SYSTEM_TEST_REQUIRE(context, tea_buff != nullptr);
    GS1_SYSTEM_TEST_CHECK(
        context,
        approx_equal(
            static_cast<float>(tea_buff->remaining_world_minutes),
            480.0f));

    const auto tonic_completed = make_message(
        GameMessageType::InventoryItemUseCompleted,
        gs1::InventoryItemUseCompletedMessage {
            gs1::k_item_focus_tonic,
            1U,
            0U});
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ModifierSystem::process_message(modifier_context, tonic_completed) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(context, count_active_timed_buffs(site_run) == 2U);
    GS1_SYSTEM_TEST_REQUIRE(context, find_active_timed_buff(site_run, gs1::ModifierId {3001U}) != nullptr);
    GS1_SYSTEM_TEST_REQUIRE(context, find_active_timed_buff(site_run, gs1::ModifierId {3003U}) != nullptr);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ModifierSystem::process_message(modifier_context, tea_completed) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(context, count_active_timed_buffs(site_run) == 2U);
    GS1_SYSTEM_TEST_CHECK(context, find_active_timed_buff(site_run, gs1::ModifierId {3001U}) != nullptr);
    GS1_SYSTEM_TEST_CHECK(context, find_active_timed_buff(site_run, gs1::ModifierId {3003U}) != nullptr);

    auto expire_context = make_site_context<ModifierSystem>(campaign, site_run, queue, 1200.0);
    ModifierSystem::run(expire_context);
    GS1_SYSTEM_TEST_CHECK(context, count_active_timed_buffs(site_run) == 0U);
    GS1_SYSTEM_TEST_CHECK(
        context,
        approx_equal(
            site_run.modifier.resolved_action_cost_modifiers.energy_weight_delta,
            0.0f));
}

void timed_buff_action_cost_modifier_reduces_action_energy_cost(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto baseline_site_run = make_test_site_run(1U, 810U);
    auto buffed_site_run = make_test_site_run(1U, 811U);
    GameMessageQueue baseline_queue {};
    GameMessageQueue buffed_queue {};
    auto baseline_action_context =
        make_site_context<ActionExecutionSystem>(campaign, baseline_site_run, baseline_queue, 60.0);
    auto buffed_action_context =
        make_site_context<ActionExecutionSystem>(campaign, buffed_site_run, buffed_queue, 60.0);
    auto buffed_modifier_context =
        make_site_context<ModifierSystem>(campaign, buffed_site_run, buffed_queue);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ModifierSystem::process_message(
            buffed_modifier_context,
            make_message(
                GameMessageType::SiteRunStarted,
                SiteRunStartedMessage {1U, 1U, 101U, 1U, 42ULL})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ModifierSystem::process_message(
            buffed_modifier_context,
            make_message(
                GameMessageType::InventoryItemUseCompleted,
                gs1::InventoryItemUseCompletedMessage {
                    gs1::k_item_focus_tonic,
                    1U,
                    0U})) == GS1_STATUS_OK);

    const auto start_water_action = make_message(
        GameMessageType::StartSiteAction,
        gs1::StartSiteActionMessage {
            GS1_SITE_ACTION_WATER,
            0U,
            1U,
            0,
            0,
            0U,
            0U,
            0U});
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ActionExecutionSystem::process_message(baseline_action_context, start_water_action) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ActionExecutionSystem::process_message(buffed_action_context, start_water_action) == GS1_STATUS_OK);

    GS1_SYSTEM_TEST_CHECK(
        context,
        baseline_site_run.site_action.deferred_meter_delta.energy_delta <
            buffed_site_run.site_action.deferred_meter_delta.energy_delta);
}

void timed_modifier_can_be_manually_ended_without_touching_permanent_modifier(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 812U);
    GameMessageQueue queue {};
    auto modifier_context = make_site_context<ModifierSystem>(campaign, site_run, queue);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ModifierSystem::process_message(
            modifier_context,
            make_message(
                GameMessageType::SiteRunStarted,
                SiteRunStartedMessage {1U, 1U, 101U, 1U, 42ULL})) == GS1_STATUS_OK);

    const auto baseline_totals = site_run.modifier.resolved_channel_totals;
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ModifierSystem::process_message(
            modifier_context,
            make_message(
                GameMessageType::RunModifierAwardRequested,
                gs1::RunModifierAwardRequestedMessage {4U})) == GS1_STATUS_OK);
    const auto run_modifier_totals = site_run.modifier.resolved_channel_totals;
    GS1_SYSTEM_TEST_CHECK(
        context,
        !approx_equal(run_modifier_totals.energy_cap, baseline_totals.energy_cap) ||
            !approx_equal(run_modifier_totals.morale, baseline_totals.morale) ||
            !approx_equal(run_modifier_totals.work_efficiency, baseline_totals.work_efficiency));

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ModifierSystem::process_message(
            modifier_context,
            make_message(
                GameMessageType::InventoryItemUseCompleted,
                gs1::InventoryItemUseCompletedMessage {
                    gs1::k_item_focus_tonic,
                    1U,
                    0U})) == GS1_STATUS_OK);

    GS1_SYSTEM_TEST_CHECK(context, site_run.modifier.active_site_modifiers.size() == 2U);
    GS1_SYSTEM_TEST_REQUIRE(context, count_active_timed_buffs(site_run) == 1U);
    GS1_SYSTEM_TEST_CHECK(
        context,
        approx_equal(site_run.modifier.resolved_action_cost_modifiers.energy_weight_delta, -0.25f));
    GS1_SYSTEM_TEST_CHECK(
        context,
        site_run.modifier.resolved_channel_totals.morale > run_modifier_totals.morale);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ModifierSystem::process_message(
            modifier_context,
            make_message(
                GameMessageType::SiteModifierEndRequested,
                gs1::SiteModifierEndRequestedMessage {3003U})) == GS1_STATUS_OK);

    GS1_SYSTEM_TEST_CHECK(context, count_active_timed_buffs(site_run) == 0U);
    GS1_SYSTEM_TEST_CHECK(context, site_run.modifier.active_site_modifiers.size() == 1U);
    GS1_SYSTEM_TEST_CHECK(
        context,
        approx_equal(site_run.modifier.resolved_action_cost_modifiers.energy_weight_delta, 0.0f));
    GS1_SYSTEM_TEST_CHECK(
        context,
        approx_equal(site_run.modifier.resolved_channel_totals.energy_cap, run_modifier_totals.energy_cap));
    GS1_SYSTEM_TEST_CHECK(
        context,
        approx_equal(site_run.modifier.resolved_channel_totals.morale, run_modifier_totals.morale));

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ModifierSystem::process_message(
            modifier_context,
            make_message(
                GameMessageType::SiteModifierEndRequested,
                gs1::SiteModifierEndRequestedMessage {4U})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, site_run.modifier.active_site_modifiers.size() == 1U);
}

void timed_buff_cap_blocks_distinct_buffs_and_bias_can_expand_limit(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 813U);
    GameMessageQueue queue {};
    auto modifier_context = make_site_context<ModifierSystem>(campaign, site_run, queue);

    GS1_SYSTEM_TEST_CHECK(
        context,
        gs1::gameplay_tuning_def().modifier_system.active_timed_buff_cap == 3U);
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ModifierSystem::process_message(
            modifier_context,
            make_message(
                GameMessageType::SiteRunStarted,
                SiteRunStartedMessage {1U, 1U, 101U, 1U, 42ULL})) == GS1_STATUS_OK);

    site_run.modifier.active_site_modifiers.push_back(
        make_manual_timed_buff_state(gs1::ModifierId {3002U}));
    site_run.modifier.active_site_modifiers.push_back(
        make_manual_timed_buff_state(gs1::ModifierId {3011U}));
    site_run.modifier.active_site_modifiers.push_back(
        make_manual_timed_buff_state(gs1::ModifierId {3012U}));

    const auto stew_completed = make_message(
        GameMessageType::InventoryItemUseCompleted,
        gs1::InventoryItemUseCompletedMessage {
            gs1::k_item_spiced_stew,
            1U,
            0U});
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ModifierSystem::process_message(modifier_context, stew_completed) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, count_active_timed_buffs(site_run) == 3U);
    GS1_SYSTEM_TEST_REQUIRE(context, find_active_timed_buff(site_run, gs1::ModifierId {3002U}) != nullptr);

    const auto tea_completed = make_message(
        GameMessageType::InventoryItemUseCompleted,
        gs1::InventoryItemUseCompletedMessage {
            gs1::k_item_field_tea,
            1U,
            0U});
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ModifierSystem::process_message(modifier_context, tea_completed) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, count_active_timed_buffs(site_run) == 3U);
    GS1_SYSTEM_TEST_CHECK(context, find_active_timed_buff(site_run, gs1::ModifierId {3001U}) == nullptr);

    site_run.modifier.resolved_channel_totals.timed_buff_cap_bias = 1.0f;
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ModifierSystem::process_message(modifier_context, tea_completed) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, count_active_timed_buffs(site_run) == 4U);
    GS1_SYSTEM_TEST_REQUIRE(context, find_active_timed_buff(site_run, gs1::ModifierId {3001U}) != nullptr);
}

void village_tier_seven_tech_expands_timed_buff_cap(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    campaign.faction_progress[0].faction_reputation = reputation_for_progress_tier(27U);
    auto site_run = make_test_site_run(1U, 814U);
    GameMessageQueue queue {};
    auto modifier_context = make_site_context<ModifierSystem>(campaign, site_run, queue);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ModifierSystem::process_message(
            modifier_context,
            make_message(
                GameMessageType::SiteRunStarted,
                SiteRunStartedMessage {1U, 1U, 101U, 1U, 42ULL})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(
        context,
        approx_equal(site_run.modifier.resolved_channel_totals.timed_buff_cap_bias, 2.0f));

    site_run.modifier.active_site_modifiers.push_back(
        make_manual_timed_buff_state(gs1::ModifierId {3002U}));
    site_run.modifier.active_site_modifiers.push_back(
        make_manual_timed_buff_state(gs1::ModifierId {3011U}));
    site_run.modifier.active_site_modifiers.push_back(
        make_manual_timed_buff_state(gs1::ModifierId {3012U}));
    site_run.modifier.active_site_modifiers.push_back(
        make_manual_timed_buff_state(gs1::ModifierId {3013U}));

    const auto tea_completed = make_message(
        GameMessageType::InventoryItemUseCompleted,
        gs1::InventoryItemUseCompletedMessage {
            gs1::k_item_field_tea,
            1U,
            0U});
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ModifierSystem::process_message(modifier_context, tea_completed) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, count_active_timed_buffs(site_run) == 5U);
    GS1_SYSTEM_TEST_REQUIRE(context, find_active_timed_buff(site_run, gs1::ModifierId {3001U}) != nullptr);
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
    GS1_SYSTEM_TEST_CHECK(
        context,
        current_site_cash(site_one_run) == gs1::cash_points_from_cash(20));
    GS1_SYSTEM_TEST_CHECK(
        context,
        count_phone_listings_of_kind(site_one_run.economy, gs1::PhoneListingKind::BuyItem) ==
            expected_available_buy_listing_count(campaign, site_one_run));
    GS1_SYSTEM_TEST_CHECK(
        context,
        count_phone_listings_of_kind(site_one_run.economy, gs1::PhoneListingKind::HireContractor) == 1U);
    GS1_SYSTEM_TEST_CHECK(
        context,
        find_listing_by_item_id(
            site_one_run.economy,
            gs1::PhoneListingKind::BuyItem,
            gs1::ItemId {gs1::k_item_food_pack}) != nullptr);
    GS1_SYSTEM_TEST_CHECK(context, site_one_run.economy.direct_purchase_unlockable_ids.empty());

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        EconomyPhoneSystem::process_message(other_context, started_two) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(
        context,
        current_site_cash(other_site_run) == gs1::cash_points_from_cash(20));
    GS1_SYSTEM_TEST_CHECK(context, other_site_run.economy.available_phone_listings.empty());
}

void economy_purchase_sell_and_hire_paths_update_money(
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
    GS1_SYSTEM_TEST_CHECK(context, current_site_cash(site_run) == 1060);
    GS1_SYSTEM_TEST_REQUIRE(context, find_listing_by_id(site_run.economy, 1U) != nullptr);
    GS1_SYSTEM_TEST_CHECK(
        context,
        find_listing_by_id(site_run.economy, 1U)->quantity ==
            expected_generated_buy_quantity(campaign, site_run, 1U) - 2U);
    GS1_SYSTEM_TEST_REQUIRE(context, queue.size() == 2U);
    GS1_SYSTEM_TEST_CHECK(
        context,
        queue[0].payload_as<gs1::InventoryDeliveryBatchRequestedMessage>().entries[0].item_id ==
            gs1::k_item_water_container);
    GS1_SYSTEM_TEST_CHECK(
        context,
        queue[0].payload_as<gs1::InventoryDeliveryBatchRequestedMessage>().entries[0].quantity == 2U);
    GS1_SYSTEM_TEST_CHECK(context, queue[0].type == GameMessageType::InventoryDeliveryBatchRequested);
    GS1_SYSTEM_TEST_CHECK(context, queue[1].type == GameMessageType::PhoneListingPurchased);

    queue.clear();
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        EconomyPhoneSystem::process_message(
            site_context,
            make_message(
                GameMessageType::PhoneListingSaleRequested,
                gs1::PhoneListingSaleRequestedMessage {1001U, 2U, 0U})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, current_site_cash(site_run) == 1420);
    GS1_SYSTEM_TEST_REQUIRE(context, find_listing_by_id(site_run.economy, 1001U) != nullptr);
    GS1_SYSTEM_TEST_CHECK(context, find_listing_by_id(site_run.economy, 1001U)->quantity == 0U);
    GS1_SYSTEM_TEST_REQUIRE(context, queue.size() == 2U);
    GS1_SYSTEM_TEST_CHECK(
        context,
        queue[0].payload_as<gs1::InventoryGlobalItemConsumeRequestedMessage>().item_id ==
            gs1::k_item_water_container);
    GS1_SYSTEM_TEST_CHECK(context, queue[0].type == GameMessageType::InventoryGlobalItemConsumeRequested);
    GS1_SYSTEM_TEST_CHECK(context, queue[1].type == GameMessageType::PhoneListingSold);

    queue.clear();
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        EconomyPhoneSystem::process_message(
            site_context,
            make_message(
                GameMessageType::ContractorHireRequested,
                gs1::ContractorHireRequestedMessage {10U, 1U})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, current_site_cash(site_run) == 620);
    GS1_SYSTEM_TEST_REQUIRE(context, find_listing_by_id(site_run.economy, 10U) != nullptr);
    GS1_SYSTEM_TEST_CHECK(context, find_listing_by_id(site_run.economy, 10U)->quantity == 2U);
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
                gs1::PhoneListingCartAddRequestedMessage {16U, 1U, 0U})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, find_listing_by_id(site_run.economy, 1U)->cart_quantity == 2U);
    GS1_SYSTEM_TEST_CHECK(context, find_listing_by_id(site_run.economy, 16U)->cart_quantity == 1U);

    queue.clear();
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        EconomyPhoneSystem::process_message(
            site_context,
            make_message(
                GameMessageType::PhoneCartCheckoutRequested,
                gs1::PhoneCartCheckoutRequestedMessage {})) == GS1_STATUS_OK);

    GS1_SYSTEM_TEST_CHECK(
        context,
        current_site_cash(site_run) ==
            gs1::cash_points_from_cash(20) -
                static_cast<std::int32_t>(gs1::item_buy_price_cash_points(
                    gs1::ItemId {gs1::k_item_water_container}) * 2U) -
                static_cast<std::int32_t>(gs1::item_buy_price_cash_points(
                    gs1::ItemId {gs1::k_item_basic_straw_checkerboard})) -
                site_run.economy.phone_delivery_fee);
    GS1_SYSTEM_TEST_CHECK(
        context,
        find_listing_by_id(site_run.economy, 1U)->quantity ==
            expected_generated_buy_quantity(campaign, site_run, 1U) - 2U);
    GS1_SYSTEM_TEST_CHECK(
        context,
        find_listing_by_id(site_run.economy, 16U)->quantity ==
            expected_generated_buy_quantity(campaign, site_run, 16U) - 1U);
    GS1_SYSTEM_TEST_CHECK(context, find_listing_by_id(site_run.economy, 1U)->cart_quantity == 0U);
    GS1_SYSTEM_TEST_CHECK(context, find_listing_by_id(site_run.economy, 16U)->cart_quantity == 0U);
    GS1_SYSTEM_TEST_REQUIRE(context, queue.size() == 3U);
    GS1_SYSTEM_TEST_CHECK(
        context,
        queue[0].payload_as<gs1::InventoryDeliveryBatchRequestedMessage>().entry_count == 2U);
    GS1_SYSTEM_TEST_CHECK(context, queue[0].type == GameMessageType::InventoryDeliveryBatchRequested);
    GS1_SYSTEM_TEST_CHECK(context, queue[1].type == GameMessageType::PhoneListingPurchased);
    GS1_SYSTEM_TEST_CHECK(context, queue[2].type == GameMessageType::PhoneListingPurchased);
}

void economy_site_unlockable_purchase_path_is_absent_without_seeded_progression_listings(
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
                GameMessageType::SiteUnlockablePurchaseRequested,
                gs1::SiteUnlockablePurchaseRequestedMessage {101U})) == GS1_STATUS_NOT_FOUND);
    GS1_SYSTEM_TEST_CHECK(context, current_site_cash(site_run) == gs1::cash_points_from_cash(20));
    GS1_SYSTEM_TEST_CHECK(context, site_run.economy.direct_purchase_unlockable_ids.empty());
    GS1_SYSTEM_TEST_CHECK(context, site_run.economy.revealed_site_unlockable_ids.empty());
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

    site_run.economy.current_cash = 0;
    GS1_SYSTEM_TEST_CHECK(
        context,
        EconomyPhoneSystem::process_message(
            site_context,
            make_message(
                GameMessageType::ContractorHireRequested,
                gs1::ContractorHireRequestedMessage {10U, 1U})) == GS1_STATUS_INVALID_STATE);
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
    GS1_SYSTEM_TEST_CHECK(context, current_site_cash(site_run) == 2360);
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
    GS1_SYSTEM_TEST_CHECK(context, current_site_cash(site_run) == 2360);
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
    GS1_SYSTEM_TEST_CHECK(
        context,
        listing->quantity == expected_generated_buy_quantity(campaign, site_run, 1U));

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
    GS1_SYSTEM_TEST_CHECK(
        context,
        listing->cart_quantity == expected_generated_buy_quantity(campaign, site_run, 1U));
    GS1_SYSTEM_TEST_CHECK(
        context,
        listing->quantity == expected_generated_buy_quantity(campaign, site_run, 1U));
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

void task_board_site_run_started_seeds_first_onboarding_step(
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

    GS1_SYSTEM_TEST_CHECK(context, site_run.task_board.visible_tasks.size() == 1U);
    GS1_SYSTEM_TEST_CHECK(context, site_run.task_board.task_pool_size == 11U);
    GS1_SYSTEM_TEST_CHECK(context, site_run.task_board.accepted_task_ids.size() == 1U);
    GS1_SYSTEM_TEST_CHECK(
        context,
        site_run.task_board.visible_tasks.front().task_template_id.value ==
            gs1::k_task_template_site1_onboarding_plant_checkerboard);
    GS1_SYSTEM_TEST_CHECK(
        context,
        site_run.task_board.visible_tasks.front().runtime_list_kind == TaskRuntimeListKind::Accepted);
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(site_run.task_board.minutes_until_next_refresh, 0.0));
    GS1_SYSTEM_TEST_CHECK(
        context,
        site_run.task_board.tracked_tiles.size() ==
            static_cast<std::size_t>(site_run.task_board.tracked_tile_width) *
                static_cast<std::size_t>(site_run.task_board.tracked_tile_height));
    GS1_SYSTEM_TEST_CHECK(context, site_run.task_board.site_completion_tile_threshold == 5U);
}

void task_board_site_one_onboarding_inserts_hammer_before_storage_crate(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 1013U);
    GameMessageQueue queue {};
    auto task_context = make_site_context<TaskBoardSystem>(campaign, site_run, queue);

    const auto start_message =
        make_message(GameMessageType::SiteRunStarted, SiteRunStartedMessage {1U, 1U, 101U, 1U, 42ULL});
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        TaskBoardSystem::process_message(task_context, start_message) == GS1_STATUS_OK);

    GS1_SYSTEM_TEST_REQUIRE(context, site_run.task_board.visible_tasks.size() == 1U);
    GS1_SYSTEM_TEST_CHECK(
        context,
        site_run.task_board.visible_tasks.front().task_template_id.value ==
            gs1::k_task_template_site1_onboarding_plant_checkerboard);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        TaskBoardSystem::process_message(
            task_context,
            make_message(
                GameMessageType::SiteTilePlantingCompleted,
                gs1::SiteTilePlantingCompletedMessage {1U, 4, 4, 5U, 100.0f, 0U})) ==
            GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        site_run.task_board.visible_tasks.front().runtime_list_kind == TaskRuntimeListKind::PendingClaim);

    queue.clear();
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        TaskBoardSystem::process_message(
            task_context,
            make_message(
                GameMessageType::TaskRewardClaimRequested,
                gs1::TaskRewardClaimRequestedMessage {
                    site_run.task_board.visible_tasks.front().task_instance_id.value,
                    0U})) == GS1_STATUS_OK);

    auto* hammer_task =
        find_task_by_template_id(site_run.task_board, gs1::k_task_template_site1_onboarding_craft_hammer);
    GS1_SYSTEM_TEST_REQUIRE(context, hammer_task != nullptr);
    GS1_SYSTEM_TEST_CHECK(context, hammer_task->runtime_list_kind == TaskRuntimeListKind::Accepted);
    GS1_SYSTEM_TEST_CHECK(context, hammer_task->recipe_id.value == gs1::k_recipe_craft_hammer);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        TaskBoardSystem::process_message(
            task_context,
            make_message(
                GameMessageType::InventoryCraftCompleted,
                gs1::InventoryCraftCompletedMessage {
                    gs1::k_recipe_craft_hammer,
                    gs1::k_item_hammer,
                    1U,
                    0U})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(context, hammer_task->runtime_list_kind == TaskRuntimeListKind::PendingClaim);

    queue.clear();
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        TaskBoardSystem::process_message(
            task_context,
            make_message(
                GameMessageType::TaskRewardClaimRequested,
                gs1::TaskRewardClaimRequestedMessage {
                    hammer_task->task_instance_id.value,
                    0U})) == GS1_STATUS_OK);

    auto* storage_crate_task =
        find_task_by_template_id(site_run.task_board, gs1::k_task_template_site1_onboarding_craft_storage_crate);
    GS1_SYSTEM_TEST_REQUIRE(context, storage_crate_task != nullptr);
    GS1_SYSTEM_TEST_CHECK(context, storage_crate_task->runtime_list_kind == TaskRuntimeListKind::Accepted);
    GS1_SYSTEM_TEST_CHECK(context, storage_crate_task->recipe_id.value == gs1::k_recipe_craft_storage_crate);
}

void task_board_site_one_onboarding_buy_food_progresses_from_food_purchase(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 1014U);
    GameMessageQueue queue {};
    auto task_context = make_site_context<TaskBoardSystem>(campaign, site_run, queue);

    const auto start_message =
        make_message(GameMessageType::SiteRunStarted, SiteRunStartedMessage {1U, 1U, 101U, 1U, 42ULL});
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        TaskBoardSystem::process_message(task_context, start_message) == GS1_STATUS_OK);

    const auto claim_task = [&](gs1::TaskInstanceId task_instance_id) {
        queue.clear();
        GS1_SYSTEM_TEST_REQUIRE(
            context,
            TaskBoardSystem::process_message(
                task_context,
                make_message(
                    GameMessageType::TaskRewardClaimRequested,
                    gs1::TaskRewardClaimRequestedMessage {task_instance_id.value, 0U})) == GS1_STATUS_OK);
    };

    GS1_SYSTEM_TEST_REQUIRE(context, site_run.task_board.visible_tasks.size() == 1U);
    GS1_SYSTEM_TEST_CHECK(
        context,
        site_run.task_board.visible_tasks.front().task_template_id.value ==
            gs1::k_task_template_site1_onboarding_plant_checkerboard);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        TaskBoardSystem::process_message(
            task_context,
            make_message(
                GameMessageType::SiteTilePlantingCompleted,
                gs1::SiteTilePlantingCompletedMessage {1U, 4, 4, 5U, 100.0f, 0U})) ==
            GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(
        context,
        site_run.task_board.visible_tasks.front().runtime_list_kind == TaskRuntimeListKind::PendingClaim);
    claim_task(site_run.task_board.visible_tasks.front().task_instance_id);

    auto* hammer_task =
        find_task_by_template_id(site_run.task_board, gs1::k_task_template_site1_onboarding_craft_hammer);
    GS1_SYSTEM_TEST_REQUIRE(context, hammer_task != nullptr);
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        TaskBoardSystem::process_message(
            task_context,
            make_message(
                GameMessageType::InventoryCraftCompleted,
                gs1::InventoryCraftCompletedMessage {
                    gs1::k_recipe_craft_hammer,
                    gs1::k_item_hammer,
                    1U,
                    0U})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, hammer_task->runtime_list_kind == TaskRuntimeListKind::PendingClaim);
    claim_task(hammer_task->task_instance_id);

    auto* storage_crate_task =
        find_task_by_template_id(site_run.task_board, gs1::k_task_template_site1_onboarding_craft_storage_crate);
    GS1_SYSTEM_TEST_REQUIRE(context, storage_crate_task != nullptr);
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        TaskBoardSystem::process_message(
            task_context,
            make_message(
                GameMessageType::InventoryCraftCompleted,
                gs1::InventoryCraftCompletedMessage {
                    gs1::k_recipe_craft_storage_crate,
                    gs1::k_item_storage_crate_kit,
                    1U,
                    0U})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(
        context,
        storage_crate_task->runtime_list_kind == TaskRuntimeListKind::PendingClaim);
    claim_task(storage_crate_task->task_instance_id);

    auto* buy_water_task =
        find_task_by_template_id(site_run.task_board, gs1::k_task_template_site1_onboarding_buy_water);
    GS1_SYSTEM_TEST_REQUIRE(context, buy_water_task != nullptr);
    GS1_SYSTEM_TEST_CHECK(context, buy_water_task->runtime_list_kind == TaskRuntimeListKind::Accepted);
    GS1_SYSTEM_TEST_CHECK(context, buy_water_task->item_id.value == gs1::k_item_water_container);
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        TaskBoardSystem::process_message(
            task_context,
            make_message(
                GameMessageType::PhoneListingPurchased,
                gs1::PhoneListingPurchasedMessage {1U, gs1::k_item_water_container, 1U, 0U})) ==
            GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, buy_water_task->runtime_list_kind == TaskRuntimeListKind::PendingClaim);
    claim_task(buy_water_task->task_instance_id);

    auto* buy_food_task =
        find_task_by_template_id(site_run.task_board, gs1::k_task_template_site1_onboarding_buy_food);
    GS1_SYSTEM_TEST_REQUIRE(context, buy_food_task != nullptr);
    GS1_SYSTEM_TEST_CHECK(context, buy_food_task->runtime_list_kind == TaskRuntimeListKind::Accepted);
    GS1_SYSTEM_TEST_CHECK(context, buy_food_task->item_id.value == gs1::k_item_food_pack);
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        TaskBoardSystem::process_message(
            task_context,
            make_message(
                GameMessageType::PhoneListingPurchased,
                gs1::PhoneListingPurchasedMessage {2U, gs1::k_item_food_pack, 1U, 0U})) ==
            GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, buy_food_task->runtime_list_kind == TaskRuntimeListKind::PendingClaim);
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

    GS1_SYSTEM_TEST_CHECK(context, site_run.task_board.accepted_task_ids.empty());
    GS1_SYSTEM_TEST_CHECK(context, site_run.task_board.completed_task_ids.empty());
    GS1_SYSTEM_TEST_CHECK(context, !site_run.task_board.active_chain_state.has_value());
    GS1_SYSTEM_TEST_CHECK(
        context,
        site_run.task_board.visible_tasks.size() ==
            gs1::gameplay_tuning_def().task_board.normal_task_pool_size);
    GS1_SYSTEM_TEST_CHECK(
        context,
        site_run.task_board.task_pool_size ==
            gs1::gameplay_tuning_def().task_board.normal_task_pool_size);
    GS1_SYSTEM_TEST_CHECK(
        context,
        approx_equal(
            site_run.task_board.minutes_until_next_refresh,
            static_cast<double>(gs1::gameplay_tuning_def().task_board.refresh_interval_minutes)));
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
        site_run.task_board.visible_tasks.front().runtime_list_kind == TaskRuntimeListKind::PendingClaim);
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
    GS1_SYSTEM_TEST_CHECK(context, buy_task->runtime_list_kind == TaskRuntimeListKind::PendingClaim);
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
    GS1_SYSTEM_TEST_CHECK(context, transfer_task->runtime_list_kind == TaskRuntimeListKind::PendingClaim);
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
    GS1_SYSTEM_TEST_CHECK(context, build_task->runtime_list_kind == TaskRuntimeListKind::PendingClaim);
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
    GS1_SYSTEM_TEST_CHECK(context, meter_task->runtime_list_kind == TaskRuntimeListKind::PendingClaim);
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
    GS1_SYSTEM_TEST_CHECK(context, plant_task->runtime_list_kind == TaskRuntimeListKind::PendingClaim);
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
        site_run.task_board.visible_tasks.front().runtime_list_kind == TaskRuntimeListKind::PendingClaim);
    GS1_SYSTEM_TEST_CHECK(context, site_run.task_board.claimed_task_ids.empty());
}

void task_board_reward_claim_queues_resolved_message_after_reward_effects(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 1010U);
    GameMessageQueue queue {};
    auto task_context = make_site_context<TaskBoardSystem>(campaign, site_run, queue);

    auto& task = site_run.task_board.visible_tasks.emplace_back();
    task.task_instance_id = gs1::TaskInstanceId {1U};
    task.task_template_id = gs1::TaskTemplateId {gs1::k_task_template_site1_buy_water};
    task.publisher_faction_id = gs1::FactionId {gs1::k_faction_village_committee};
    task.task_tier_id = 1U;
    task.target_amount = 1U;
    task.current_progress_amount = 1U;
    task.runtime_list_kind = TaskRuntimeListKind::PendingClaim;
    task.reward_draft_options.push_back(gs1::TaskRewardDraftOption {gs1::RewardCandidateId {1U}, false});
    task.reward_draft_options.push_back(gs1::TaskRewardDraftOption {gs1::RewardCandidateId {2U}, false});
    site_run.task_board.completed_task_ids.push_back(task.task_instance_id);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        TaskBoardSystem::process_message(
            task_context,
            make_message(
                GameMessageType::TaskRewardClaimRequested,
                gs1::TaskRewardClaimRequestedMessage {
                    task.task_instance_id.value,
                    0U})) == GS1_STATUS_OK);

    GS1_SYSTEM_TEST_REQUIRE(context, queue.size() == 3U);
    GS1_SYSTEM_TEST_CHECK(context, queue[0].type == GameMessageType::EconomyMoneyAwardRequested);
    GS1_SYSTEM_TEST_CHECK(context, queue[1].type == GameMessageType::InventoryDeliveryRequested);
    GS1_SYSTEM_TEST_CHECK(context, queue[2].type == GameMessageType::TaskRewardClaimResolved);
    {
        const auto& payload = queue[2].payload_as<gs1::TaskRewardClaimResolvedMessage>();
        GS1_SYSTEM_TEST_CHECK(context, payload.task_instance_id == 1U);
        GS1_SYSTEM_TEST_CHECK(context, payload.task_template_id == gs1::k_task_template_site1_buy_water);
        GS1_SYSTEM_TEST_CHECK(context, payload.reward_candidate_count == 2U);
    }
}

void task_board_reward_claim_uses_first_draft_option_when_candidate_is_unspecified(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 1009U);
    GameMessageQueue queue {};
    auto task_context = make_site_context<TaskBoardSystem>(campaign, site_run, queue);

    auto& task = site_run.task_board.visible_tasks.emplace_back();
    task.task_instance_id = gs1::TaskInstanceId {1U};
    task.task_template_id = gs1::TaskTemplateId {gs1::k_task_template_site1_buy_water};
    task.publisher_faction_id = gs1::FactionId {gs1::k_faction_village_committee};
    task.task_tier_id = 1U;
    task.target_amount = 1U;
    task.current_progress_amount = 1U;
    task.runtime_list_kind = TaskRuntimeListKind::PendingClaim;
    task.reward_draft_options.push_back(gs1::TaskRewardDraftOption {gs1::RewardCandidateId {1U}, false});
    task.reward_draft_options.push_back(gs1::TaskRewardDraftOption {gs1::RewardCandidateId {2U}, false});
    site_run.task_board.completed_task_ids.push_back(task.task_instance_id);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        TaskBoardSystem::process_message(
            task_context,
            make_message(
                GameMessageType::TaskRewardClaimRequested,
                gs1::TaskRewardClaimRequestedMessage {
                    task.task_instance_id.value,
                    0U})) == GS1_STATUS_OK);

    GS1_SYSTEM_TEST_CHECK(
        context,
        site_run.task_board.visible_tasks.front().runtime_list_kind == TaskRuntimeListKind::Claimed);
    GS1_SYSTEM_TEST_CHECK(context, site_run.task_board.completed_task_ids.empty());
    GS1_SYSTEM_TEST_CHECK(context, site_run.task_board.claimed_task_ids.size() == 1U);
    GS1_SYSTEM_TEST_CHECK(
        context,
        count_queued_messages(queue, GameMessageType::EconomyMoneyAwardRequested) == 1U);
    GS1_SYSTEM_TEST_CHECK(
        context,
        count_queued_messages(queue, GameMessageType::InventoryDeliveryRequested) == 1U);
    GS1_SYSTEM_TEST_CHECK(
        context,
        count_queued_messages(queue, GameMessageType::TaskRewardClaimResolved) == 1U);
    GS1_SYSTEM_TEST_CHECK(context, site_run.task_board.visible_tasks.front().reward_draft_options[0].selected);
    GS1_SYSTEM_TEST_CHECK(context, site_run.task_board.visible_tasks.front().reward_draft_options[1].selected);
}

void task_board_content_tuning_exposes_internal_prices_and_task_scoring_inputs(
    gs1::testing::SystemTestExecutionContext& context)
{
    const auto* water = gs1::find_item_def(gs1::ItemId {gs1::k_item_water_container});
    GS1_SYSTEM_TEST_REQUIRE(context, water != nullptr);
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(water->hydration_delta, 20.0f));
    GS1_SYSTEM_TEST_CHECK(context, water->internal_price_cash_points == 0U);
    GS1_SYSTEM_TEST_CHECK(
        context,
        gs1::item_internal_price_cash_points(gs1::ItemId {gs1::k_item_water_container}) == 200U);

    const auto* food = gs1::find_item_def(gs1::ItemId {gs1::k_item_food_pack});
    GS1_SYSTEM_TEST_REQUIRE(context, food != nullptr);
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(food->nourishment_delta, 15.0f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(food->energy_delta, 0.0f));
    GS1_SYSTEM_TEST_CHECK(context, food->internal_price_cash_points == 0U);
    GS1_SYSTEM_TEST_CHECK(
        context,
        gs1::item_internal_price_cash_points(gs1::ItemId {gs1::k_item_food_pack}) == 375U);

    const auto* medicine = gs1::find_item_def(gs1::ItemId {gs1::k_item_medicine_pack});
    GS1_SYSTEM_TEST_REQUIRE(context, medicine != nullptr);
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(medicine->health_delta, 18.0f));
    GS1_SYSTEM_TEST_CHECK(context, medicine->internal_price_cash_points == 0U);
    GS1_SYSTEM_TEST_CHECK(
        context,
        gs1::item_internal_price_cash_points(gs1::ItemId {gs1::k_item_medicine_pack}) == 900U);

    const auto& meter_cash_points = gs1::gameplay_tuning_def().player_meter_cash_points;
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(meter_cash_points.health_per_point, 50.0f, 0.001f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(meter_cash_points.hydration_per_point, 10.0f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(meter_cash_points.nourishment_per_point, 25.0f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(meter_cash_points.energy_per_point, 1.0f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(meter_cash_points.morale_per_point, 35.0f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(meter_cash_points.buy_price_multiplier, 1.1f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(meter_cash_points.sell_price_multiplier, 0.9f));
    const auto& plant_harvest_tuning = gs1::gameplay_tuning_def().plant_harvest;
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(plant_harvest_tuning.plant_cash_points_per_pool_point, 10.0f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(plant_harvest_tuning.seed_cash_points_per_pool_point, 2.0f));
    GS1_SYSTEM_TEST_CHECK(
        context,
        gs1::item_buy_price_cash_points(gs1::ItemId {gs1::k_item_water_container}) == 220U);
    GS1_SYSTEM_TEST_CHECK(
        context,
        gs1::item_sell_price_cash_points(gs1::ItemId {gs1::k_item_water_container}) == 180U);

    const auto* white_thorn_berries = gs1::find_item_def(gs1::ItemId {gs1::k_item_white_thorn_berries});
    GS1_SYSTEM_TEST_REQUIRE(context, white_thorn_berries != nullptr);
    GS1_SYSTEM_TEST_CHECK(context, white_thorn_berries->internal_price_cash_points == 0U);
    GS1_SYSTEM_TEST_CHECK(
        context,
        gs1::item_internal_price_cash_points(gs1::ItemId {gs1::k_item_white_thorn_berries}) == 60U);
    GS1_SYSTEM_TEST_CHECK(
        context,
        gs1::item_sell_price_cash_points(gs1::ItemId {gs1::k_item_white_thorn_berries}) == 54U);

    const auto* white_thorn_seed = gs1::find_item_def(gs1::ItemId {gs1::k_item_white_thorn_seed_bundle});
    GS1_SYSTEM_TEST_REQUIRE(context, white_thorn_seed != nullptr);
    GS1_SYSTEM_TEST_CHECK(context, white_thorn_seed->internal_price_cash_points == 0U);
    GS1_SYSTEM_TEST_CHECK(
        context,
        gs1::item_internal_price_cash_points(gs1::ItemId {gs1::k_item_white_thorn_seed_bundle}) == 220U);
    GS1_SYSTEM_TEST_CHECK(
        context,
        gs1::item_buy_price_cash_points(gs1::ItemId {gs1::k_item_white_thorn_seed_bundle}) == 242U);
    GS1_SYSTEM_TEST_CHECK(
        context,
        gs1::item_sell_price_cash_points(gs1::ItemId {gs1::k_item_white_thorn_seed_bundle}) == 198U);

    const auto* site_content = gs1::find_prototype_site_content(gs1::SiteId {1U});
    GS1_SYSTEM_TEST_REQUIRE(context, site_content != nullptr);
    bool found_seed_listing = false;
    for (const auto& listing : site_content->phone_buy_stock_pool)
    {
        if (listing.listing_id != 11U)
        {
            continue;
        }

        found_seed_listing = true;
        GS1_SYSTEM_TEST_CHECK(context, listing.item_id.value == 16U);
        GS1_SYSTEM_TEST_CHECK(context, listing.base_stock_cash_points == 4400U);
        GS1_SYSTEM_TEST_CHECK(context, listing.stock_cash_points_variance == 550U);
    }
    GS1_SYSTEM_TEST_CHECK(context, found_seed_listing);

    const auto* submit_task =
        gs1::find_task_template_def(gs1::TaskTemplateId {gs1::k_task_template_site1_submit_water});
    GS1_SYSTEM_TEST_REQUIRE(context, submit_task != nullptr);
    GS1_SYSTEM_TEST_CHECK(context, submit_task->progress_kind == gs1::TaskProgressKind::SubmitItem);
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(submit_task->expected_task_hours_in_game, 0.5f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(submit_task->risk_multiplier, 0.05f));

    std::vector<std::uint32_t> task_template_ids {};
    for (const auto& task_template : gs1::all_task_template_defs())
    {
        task_template_ids.push_back(task_template.task_template_id.value);
    }
    std::sort(task_template_ids.begin(), task_template_ids.end());
    GS1_SYSTEM_TEST_CHECK(
        context,
        std::adjacent_find(task_template_ids.begin(), task_template_ids.end()) == task_template_ids.end());

    const auto* hammer_onboarding_task =
        gs1::find_task_template_def(gs1::TaskTemplateId {gs1::k_task_template_site1_onboarding_craft_hammer});
    GS1_SYSTEM_TEST_REQUIRE(context, hammer_onboarding_task != nullptr);
    GS1_SYSTEM_TEST_CHECK(context, hammer_onboarding_task->progress_kind == gs1::TaskProgressKind::CraftRecipe);
    GS1_SYSTEM_TEST_CHECK(context, hammer_onboarding_task->recipe_id.value == gs1::k_recipe_craft_hammer);
}

void excavation_content_tuning_exposes_depth_specific_sell_balance(
    gs1::testing::SystemTestExecutionContext& context)
{
    struct ExpectedStoneValue final
    {
        std::uint32_t item_id;
        std::uint32_t internal_cash_points;
    };

    constexpr ExpectedStoneValue expected_values[] {
        {gs1::k_item_wind_polished_desert_pebble, 110U},
        {gs1::k_item_desert_jasper, 385U},
        {gs1::k_item_black_gobi_stone, 605U},
        {gs1::k_item_gobi_agate, 935U},
        {gs1::k_item_alxa_agate, 1430U},
        {gs1::k_item_turquoise_vein_fragment, 2420U},
        {gs1::k_item_golden_silk_jade, 3080U},
        {gs1::k_item_hetian_jade_pebble, 11000U},
    };

    for (const auto& entry : expected_values)
    {
        GS1_SYSTEM_TEST_CHECK(
            context,
            gs1::item_internal_price_cash_points(gs1::ItemId {entry.item_id}) == entry.internal_cash_points);
    }

    const auto* excavate_action = gs1::find_site_action_def(gs1::ActionKind::Excavate);
    GS1_SYSTEM_TEST_REQUIRE(context, excavate_action != nullptr);
    const double sell_multiplier =
        static_cast<double>(gs1::gameplay_tuning_def().player_meter_cash_points.sell_price_multiplier);

    const auto expected_sell_ev_for_depth = [](
                                           gs1::ExcavationDepth depth) -> double
    {
        const auto* depth_def = gs1::find_excavation_depth_def(depth);
        if (depth_def == nullptr)
        {
            return 0.0;
        }

        const auto tier_percent = [depth_def](gs1::ExcavationLootTier tier) -> double
        {
            switch (tier)
            {
            case gs1::ExcavationLootTier::Common:
                return static_cast<double>(depth_def->common_tier_percent);
            case gs1::ExcavationLootTier::Uncommon:
                return static_cast<double>(depth_def->uncommon_tier_percent);
            case gs1::ExcavationLootTier::Rare:
                return static_cast<double>(depth_def->rare_tier_percent);
            case gs1::ExcavationLootTier::VeryRare:
                return static_cast<double>(depth_def->very_rare_tier_percent);
            case gs1::ExcavationLootTier::Jackpot:
                return static_cast<double>(depth_def->jackpot_tier_percent);
            case gs1::ExcavationLootTier::None:
            default:
                return 0.0;
            }
        };

        double sell_ev = 0.0;
        for (const auto& entry : gs1::all_excavation_loot_entry_defs())
        {
            if (entry.depth != depth)
            {
                continue;
            }

            sell_ev +=
                static_cast<double>(gs1::item_internal_price_cash_points(entry.item_id)) *
                (static_cast<double>(depth_def->find_chance_percent) * 0.01) *
                (tier_percent(entry.tier) * 0.01) *
                (static_cast<double>(entry.percent_within_tier) * 0.01);
        }
        return sell_ev;
    };

    const auto calm_excavation_cost_for_depth = [excavate_action](
                                                     gs1::ExcavationDepth depth) -> std::uint32_t
    {
        const auto* depth_def = gs1::find_excavation_depth_def(depth);
        return depth_def == nullptr
            ? 0U
            : gs1::player_meter_cost_internal_cash_points(
                0.0f,
                excavate_action->hydration_cost_per_unit,
                excavate_action->nourishment_cost_per_unit,
                excavate_action->energy_cost_per_unit * depth_def->energy_cost_multiplier,
                0.0f);
    };

    const auto* rough_depth = gs1::find_excavation_depth_def(gs1::ExcavationDepth::Rough);
    const auto* careful_depth = gs1::find_excavation_depth_def(gs1::ExcavationDepth::Careful);
    const auto* thorough_depth = gs1::find_excavation_depth_def(gs1::ExcavationDepth::Thorough);
    GS1_SYSTEM_TEST_REQUIRE(context, rough_depth != nullptr);
    GS1_SYSTEM_TEST_REQUIRE(context, careful_depth != nullptr);
    GS1_SYSTEM_TEST_REQUIRE(context, thorough_depth != nullptr);
    GS1_SYSTEM_TEST_CHECK(context, rough_depth->duration_minutes == 20.0f);
    GS1_SYSTEM_TEST_CHECK(context, careful_depth->duration_minutes == 30.0f);
    GS1_SYSTEM_TEST_CHECK(context, thorough_depth->duration_minutes == 40.0f);
    GS1_SYSTEM_TEST_CHECK(context, rough_depth->find_chance_percent == 50.0f);
    GS1_SYSTEM_TEST_CHECK(context, careful_depth->find_chance_percent == 50.0f);
    GS1_SYSTEM_TEST_CHECK(context, thorough_depth->find_chance_percent == 50.0f);

    const double rough_sell_ev = expected_sell_ev_for_depth(gs1::ExcavationDepth::Rough);
    const double careful_sell_ev = expected_sell_ev_for_depth(gs1::ExcavationDepth::Careful);
    const double thorough_sell_ev = expected_sell_ev_for_depth(gs1::ExcavationDepth::Thorough);

    GS1_SYSTEM_TEST_CHECK(
        context,
        approx_equal(static_cast<float>(rough_sell_ev * sell_multiplier), 366.30f, 0.01f));
    GS1_SYSTEM_TEST_CHECK(
        context,
        approx_equal(static_cast<float>(careful_sell_ev * sell_multiplier), 540.42f, 0.01f));
    GS1_SYSTEM_TEST_CHECK(
        context,
        approx_equal(static_cast<float>(thorough_sell_ev * sell_multiplier), 682.36f, 0.01f));

    GS1_SYSTEM_TEST_CHECK(
        context,
        approx_equal(
            static_cast<float>(
                rough_sell_ev * sell_multiplier -
                static_cast<double>(calm_excavation_cost_for_depth(gs1::ExcavationDepth::Rough))),
            156.30f,
            0.01f));
    GS1_SYSTEM_TEST_CHECK(
        context,
        approx_equal(
            static_cast<float>(careful_sell_ev * sell_multiplier -
                static_cast<double>(calm_excavation_cost_for_depth(gs1::ExcavationDepth::Careful))),
            319.42f,
            0.01f));
    GS1_SYSTEM_TEST_CHECK(
        context,
        approx_equal(
            static_cast<float>(thorough_sell_ev * sell_multiplier -
                static_cast<double>(calm_excavation_cost_for_depth(gs1::ExcavationDepth::Thorough))),
            449.36f,
            0.01f));
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
    GS1_SYSTEM_TEST_CHECK(context, submit_task->runtime_list_kind == TaskRuntimeListKind::PendingClaim);
}

void economy_phone_refresh_tick_ignores_onboarding_then_rerolls_generated_stock(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 1011U);
    GameMessageQueue queue {};
    auto economy_context = make_site_context<EconomyPhoneSystem>(campaign, site_run, queue);
    auto task_context = make_site_context<TaskBoardSystem>(campaign, site_run, queue);

    const auto start_message =
        make_message(GameMessageType::SiteRunStarted, SiteRunStartedMessage {1U, 1U, 101U, 1U, 42ULL});
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        TaskBoardSystem::process_message(task_context, start_message) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        EconomyPhoneSystem::process_message(economy_context, start_message) == GS1_STATUS_OK);

    auto* listing = find_listing_by_id(site_run.economy, 1U);
    GS1_SYSTEM_TEST_REQUIRE(context, listing != nullptr);
    const auto initial_quantity = listing->quantity;
    GS1_SYSTEM_TEST_CHECK(
        context,
        initial_quantity == expected_generated_buy_quantity(campaign, site_run, 1U, 1U));
    GS1_SYSTEM_TEST_CHECK(context, site_run.economy.phone_buy_stock_refresh_generation == 1U);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        EconomyPhoneSystem::process_message(
            economy_context,
            make_message(
                GameMessageType::SiteRefreshTick,
                gs1::SiteRefreshTickMessage {gs1::SITE_REFRESH_TICK_PHONE_BUY_STOCK})) == GS1_STATUS_OK);
    listing = find_listing_by_id(site_run.economy, 1U);
    GS1_SYSTEM_TEST_REQUIRE(context, listing != nullptr);
    GS1_SYSTEM_TEST_CHECK(context, listing->quantity == initial_quantity);
    GS1_SYSTEM_TEST_CHECK(context, site_run.economy.phone_buy_stock_refresh_generation == 1U);

    site_run.task_board.visible_tasks.clear();
    site_run.task_board.accepted_task_ids.clear();

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        EconomyPhoneSystem::process_message(
            economy_context,
            make_message(
                GameMessageType::SiteRefreshTick,
                gs1::SiteRefreshTickMessage {gs1::SITE_REFRESH_TICK_PHONE_BUY_STOCK})) == GS1_STATUS_OK);
    listing = find_listing_by_id(site_run.economy, 1U);
    GS1_SYSTEM_TEST_REQUIRE(context, listing != nullptr);
    GS1_SYSTEM_TEST_CHECK(
        context,
        listing->quantity == expected_generated_buy_quantity(campaign, site_run, 1U, 2U));
    GS1_SYSTEM_TEST_CHECK(context, site_run.economy.phone_buy_stock_refresh_generation == 2U);
}

void task_board_refresh_tick_ignores_onboarding_then_generates_normal_pool(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 1012U);
    GameMessageQueue queue {};
    auto task_context = make_site_context<TaskBoardSystem>(campaign, site_run, queue);

    const auto start_message =
        make_message(GameMessageType::SiteRunStarted, SiteRunStartedMessage {1U, 1U, 101U, 1U, 42ULL});
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        TaskBoardSystem::process_message(task_context, start_message) == GS1_STATUS_OK);

    GS1_SYSTEM_TEST_REQUIRE(context, site_run.task_board.visible_tasks.size() == 1U);
    GS1_SYSTEM_TEST_CHECK(context, site_run.task_board.task_pool_size == 11U);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        TaskBoardSystem::process_message(
            task_context,
            make_message(
                GameMessageType::SiteRefreshTick,
                gs1::SiteRefreshTickMessage {gs1::SITE_REFRESH_TICK_TASK_BOARD})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, site_run.task_board.visible_tasks.size() == 1U);
    GS1_SYSTEM_TEST_CHECK(context, site_run.task_board.task_pool_size == 11U);

    site_run.task_board.visible_tasks.clear();
    site_run.task_board.accepted_task_ids.clear();
    site_run.task_board.completed_task_ids.clear();
    site_run.task_board.claimed_task_ids.clear();

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        TaskBoardSystem::process_message(
            task_context,
            make_message(
                GameMessageType::SiteRefreshTick,
                gs1::SiteRefreshTickMessage {gs1::SITE_REFRESH_TICK_TASK_BOARD})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(
        context,
        site_run.task_board.visible_tasks.size() ==
            gs1::gameplay_tuning_def().task_board.normal_task_pool_size);
    GS1_SYSTEM_TEST_CHECK(
        context,
        site_run.task_board.task_pool_size ==
            gs1::gameplay_tuning_def().task_board.normal_task_pool_size);
    GS1_SYSTEM_TEST_CHECK(
        context,
        site_run.task_board.refresh_generation == 1U);
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
    "item_use_validates_and_emits_completion",
    inventory_item_use_validates_and_emits_completion);
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
    "focus_tonic_use_activates_timed_buff_and_adds_flat_buff_cash_points",
    focus_tonic_use_activates_timed_buff_and_adds_flat_buff_cash_points);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "inventory",
    "timed_modifiers_refresh_duplicate_id_and_expire_by_game_time",
    timed_modifiers_refresh_duplicate_id_and_expire_by_game_time);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "inventory",
    "timed_buff_action_cost_modifier_reduces_action_energy_cost",
    timed_buff_action_cost_modifier_reduces_action_energy_cost);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "inventory",
    "timed_modifier_can_be_manually_ended_without_touching_permanent_modifier",
    timed_modifier_can_be_manually_ended_without_touching_permanent_modifier);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "inventory",
    "timed_buff_cap_blocks_distinct_buffs_and_bias_can_expand_limit",
    timed_buff_cap_blocks_distinct_buffs_and_bias_can_expand_limit);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "inventory",
    "village_tier_seven_tech_expands_timed_buff_cap",
    village_tier_seven_tech_expands_timed_buff_cap);
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
    "purchase_sell_and_hire_paths_update_money",
    economy_purchase_sell_and_hire_paths_update_money);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "economy_phone",
    "cart_checkout_charges_delivery_fee_and_clears_cart",
    economy_cart_checkout_charges_delivery_fee_and_clears_cart);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "economy_phone",
    "site_unlockable_purchase_path_is_absent_without_seeded_progression_listings",
    economy_site_unlockable_purchase_path_is_absent_without_seeded_progression_listings);
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
    "economy_phone",
    "refresh_tick_ignores_onboarding_then_rerolls_generated_stock",
    economy_phone_refresh_tick_ignores_onboarding_then_rerolls_generated_stock);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "task_board",
    "site_run_started_seeds_first_onboarding_step",
    task_board_site_run_started_seeds_first_onboarding_step);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "task_board",
    "site_one_onboarding_inserts_hammer_before_storage_crate",
    task_board_site_one_onboarding_inserts_hammer_before_storage_crate);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "task_board",
    "site_one_onboarding_buy_food_progresses_from_food_purchase",
    task_board_site_one_onboarding_buy_food_progresses_from_food_purchase);
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
    "reward_claim_queues_resolved_message_after_reward_effects",
    task_board_reward_claim_queues_resolved_message_after_reward_effects);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "task_board",
    "reward_claim_uses_first_draft_option_when_candidate_is_unspecified",
    task_board_reward_claim_uses_first_draft_option_when_candidate_is_unspecified);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "task_board",
    "content_tuning_exposes_internal_prices_and_task_scoring_inputs",
    task_board_content_tuning_exposes_internal_prices_and_task_scoring_inputs);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "inventory",
    "excavation_content_tuning_exposes_depth_specific_sell_balance",
    excavation_content_tuning_exposes_depth_specific_sell_balance);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "task_board",
    "submit_task_completes_from_successful_submission",
    task_board_submit_task_completes_from_successful_submission);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "task_board",
    "refresh_tick_ignores_onboarding_then_generates_normal_pool",
    task_board_refresh_tick_ignores_onboarding_then_generates_normal_pool);
