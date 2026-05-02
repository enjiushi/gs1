#include "messages/game_message.h"
#include "content/defs/item_defs.h"
#include "site/inventory_storage.h"
#include "site/systems/economy_phone_system.h"
#include "site/systems/inventory_system.h"
#include "site/systems/phone_panel_system.h"
#include "site/systems/task_board_system.h"
#include "testing/system_test_registry.h"
#include "system_test_fixtures.h"

#include <algorithm>

namespace
{
using gs1::EconomyPhoneSystem;
using gs1::GameMessage;
using gs1::GameMessageQueue;
using gs1::GameMessageType;
using gs1::InventorySystem;
using gs1::PhonePanelSection;
using gs1::PhonePanelSystem;
using gs1::SiteRunStartedMessage;
using gs1::TaskBoardSystem;
using namespace gs1::testing::fixtures;

template <typename Payload>
GameMessage make_message(gs1::GameMessageType type, const Payload& payload)
{
    GameMessage message {};
    message.type = type;
    message.set_payload(payload);
    return message;
}

const gs1::PhoneListingState* find_phone_panel_listing(
    const gs1::PhonePanelState& phone_panel,
    std::uint32_t listing_id) noexcept
{
    for (const auto& listing : phone_panel.listings)
    {
        if (listing.listing_id == listing_id)
        {
            return &listing;
        }
    }

    return nullptr;
}

void seed_site_one_inventory(gs1::CampaignState& campaign, gs1::SiteRunState& site_run)
{
    GameMessageQueue queue {};
    auto inventory_context = make_site_context<InventorySystem>(campaign, site_run, queue);
    const auto status = InventorySystem::process_message(
        inventory_context,
        make_message(
            GameMessageType::SiteRunStarted,
            SiteRunStartedMessage {site_run.site_id.value, 1U, site_run.site_archetype_id, 1U, 42ULL}));
    (void)status;
}

void phone_panel_site_run_started_seeds_home_snapshot(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 1201U);
    GameMessageQueue queue {};
    auto inventory_context = make_site_context<InventorySystem>(campaign, site_run, queue);
    auto task_context = make_site_context<TaskBoardSystem>(campaign, site_run, queue);
    auto economy_context = make_site_context<EconomyPhoneSystem>(campaign, site_run, queue);
    auto phone_context = make_site_context<PhonePanelSystem>(campaign, site_run, queue);

    const auto start_message =
        make_message(GameMessageType::SiteRunStarted, SiteRunStartedMessage {1U, 1U, 101U, 1U, 42ULL});
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        InventorySystem::process_message(inventory_context, start_message) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        TaskBoardSystem::process_message(task_context, start_message) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        EconomyPhoneSystem::process_message(economy_context, start_message) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        PhonePanelSystem::process_message(phone_context, start_message) == GS1_STATUS_OK);

    const auto& phone_panel = site_run.phone_panel;
    GS1_SYSTEM_TEST_CHECK(context, phone_panel.active_section == PhonePanelSection::Home);
    GS1_SYSTEM_TEST_CHECK(context, phone_panel.visible_task_count == 0U);
    GS1_SYSTEM_TEST_CHECK(context, phone_panel.accepted_task_count == 1U);
    GS1_SYSTEM_TEST_CHECK(context, phone_panel.completed_task_count == 0U);
    GS1_SYSTEM_TEST_CHECK(context, phone_panel.claimed_task_count == 0U);
    GS1_SYSTEM_TEST_CHECK(context, phone_panel.buy_listing_count >= 3U);
    GS1_SYSTEM_TEST_CHECK(context, phone_panel.sell_listing_count >= 1U);
    GS1_SYSTEM_TEST_CHECK(context, phone_panel.service_listing_count == 0U);
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        find_phone_panel_listing(
            phone_panel,
            1000U + static_cast<std::uint32_t>(gs1::k_item_water_container)) != nullptr);
    GS1_SYSTEM_TEST_CHECK(context, find_phone_panel_listing(phone_panel, 10U) == nullptr);
    GS1_SYSTEM_TEST_REQUIRE(context, find_phone_panel_listing(phone_panel, 16U) != nullptr);
}

void phone_panel_section_request_switches_authoritative_section(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 1202U);
    GameMessageQueue queue {};
    auto phone_context = make_site_context<PhonePanelSystem>(campaign, site_run, queue);

    struct SectionCase final
    {
        Gs1PhonePanelSection requested_section;
        PhonePanelSection expected_section;
    };

    const SectionCase cases[] = {
        {GS1_PHONE_PANEL_SECTION_TASKS, PhonePanelSection::Tasks},
        {GS1_PHONE_PANEL_SECTION_BUY, PhonePanelSection::Buy},
        {GS1_PHONE_PANEL_SECTION_SELL, PhonePanelSection::Sell},
        {GS1_PHONE_PANEL_SECTION_HIRE, PhonePanelSection::Hire},
        {GS1_PHONE_PANEL_SECTION_CART, PhonePanelSection::Cart},
        {GS1_PHONE_PANEL_SECTION_HOME, PhonePanelSection::Home}};

    for (const auto& section_case : cases)
    {
        GS1_SYSTEM_TEST_REQUIRE(
            context,
            PhonePanelSystem::process_message(
                phone_context,
                make_message(
                    GameMessageType::PhonePanelSectionRequested,
                    gs1::PhonePanelSectionRequestedMessage {
                        section_case.requested_section,
                        {0U, 0U, 0U}})) == GS1_STATUS_OK);
        GS1_SYSTEM_TEST_CHECK(context, site_run.phone_panel.active_section == section_case.expected_section);
    }
}

void phone_panel_rejects_unknown_section_request(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 1204U);
    GameMessageQueue queue {};
    auto phone_context = make_site_context<PhonePanelSystem>(campaign, site_run, queue);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        PhonePanelSystem::process_message(
            phone_context,
            make_message(
                GameMessageType::PhonePanelSectionRequested,
                gs1::PhonePanelSectionRequestedMessage {
                    static_cast<Gs1PhonePanelSection>(99U),
                    {0U, 0U, 0U}})) == GS1_STATUS_INVALID_ARGUMENT);
    GS1_SYSTEM_TEST_CHECK(context, site_run.phone_panel.active_section == PhonePanelSection::Home);
}

void phone_panel_sell_list_refreshes_when_purchase_delivery_arrives(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 1203U);
    GameMessageQueue queue {};
    auto inventory_context = make_site_context<InventorySystem>(campaign, site_run, queue);
    auto economy_context = make_site_context<EconomyPhoneSystem>(campaign, site_run, queue);
    auto phone_context = make_site_context<PhonePanelSystem>(campaign, site_run, queue);

    seed_site_one_inventory(campaign, site_run);
    const auto start_message =
        make_message(GameMessageType::SiteRunStarted, SiteRunStartedMessage {1U, 1U, 101U, 1U, 42ULL});
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        EconomyPhoneSystem::process_message(economy_context, start_message) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        PhonePanelSystem::process_message(phone_context, start_message) == GS1_STATUS_OK);

    const auto buy_listing_it = std::find_if(
        site_run.economy.available_phone_listings.begin(),
        site_run.economy.available_phone_listings.end(),
        [&site_run](const gs1::PhoneListingState& listing) {
            if (listing.kind != gs1::PhoneListingKind::BuyItem)
            {
                return false;
            }

            const auto* item_def = gs1::find_item_def(listing.item_id);
            return item_def != nullptr &&
                gs1::item_has_capability(*item_def, gs1::ITEM_CAPABILITY_SELL) &&
                gs1::inventory_storage::available_item_quantity_in_container(
                    site_run,
                    gs1::inventory_storage::delivery_box_container(site_run),
                    listing.item_id) == 0U;
        });
    GS1_SYSTEM_TEST_REQUIRE(context, buy_listing_it != site_run.economy.available_phone_listings.end());
    const auto delivered_item_id = buy_listing_it->item_id;
    const auto dynamic_sell_listing_id = 1000U + delivered_item_id.value;
    GS1_SYSTEM_TEST_CHECK(
        context,
        find_phone_panel_listing(site_run.phone_panel, dynamic_sell_listing_id) == nullptr);
    const auto quantity_before = gs1::inventory_storage::available_item_quantity_in_container(
        site_run,
        gs1::inventory_storage::delivery_box_container(site_run),
        delivered_item_id);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        EconomyPhoneSystem::process_message(
            economy_context,
            make_message(
                GameMessageType::PhoneListingPurchaseRequested,
                gs1::PhoneListingPurchaseRequestedMessage {buy_listing_it->listing_id, 1U, 0U})) == GS1_STATUS_OK);
    bool processed_delivery = false;
    while (!queue.empty())
    {
        const auto message = queue.front();
        queue.pop_front();
        if (message.type == GameMessageType::InventoryDeliveryBatchRequested)
        {
            GS1_SYSTEM_TEST_REQUIRE(
                context,
                InventorySystem::process_message(inventory_context, message) == GS1_STATUS_OK);
            processed_delivery = true;
        }
    }
    GS1_SYSTEM_TEST_REQUIRE(context, processed_delivery);
    queue.clear();

    GS1_SYSTEM_TEST_CHECK(context, site_run.inventory.pending_delivery_queue.empty());
    PhonePanelSystem::run(phone_context);

    const auto quantity_after = gs1::inventory_storage::available_item_quantity_in_container(
        site_run,
        gs1::inventory_storage::delivery_box_container(site_run),
        delivered_item_id);
    const auto* sell_listing = find_phone_panel_listing(site_run.phone_panel, dynamic_sell_listing_id);
    GS1_SYSTEM_TEST_REQUIRE(context, sell_listing != nullptr);
    GS1_SYSTEM_TEST_CHECK(context, sell_listing->quantity == 1U);
    GS1_SYSTEM_TEST_CHECK(context, site_run.phone_panel.sell_listing_count >= 1U);
    GS1_SYSTEM_TEST_CHECK(context, quantity_after == quantity_before + 1U);
}

void phone_panel_task_counts_follow_pending_claim_until_reward_claim(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 1205U);
    GameMessageQueue queue {};
    auto phone_context = make_site_context<PhonePanelSystem>(campaign, site_run, queue);

    auto& task = site_run.task_board.visible_tasks.emplace_back();
    task.task_instance_id = gs1::TaskInstanceId {1U};
    task.runtime_list_kind = gs1::TaskRuntimeListKind::Accepted;
    site_run.task_board.accepted_task_ids.push_back(task.task_instance_id);

    const auto start_message =
        make_message(GameMessageType::SiteRunStarted, SiteRunStartedMessage {1U, 1U, 101U, 1U, 42ULL});
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        PhonePanelSystem::process_message(phone_context, start_message) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, site_run.phone_panel.accepted_task_count == 1U);
    GS1_SYSTEM_TEST_CHECK(context, site_run.phone_panel.completed_task_count == 0U);
    GS1_SYSTEM_TEST_CHECK(context, site_run.phone_panel.claimed_task_count == 0U);

    task.runtime_list_kind = gs1::TaskRuntimeListKind::PendingClaim;
    site_run.task_board.accepted_task_ids.clear();
    site_run.task_board.completed_task_ids.push_back(task.task_instance_id);
    PhonePanelSystem::run(phone_context);
    GS1_SYSTEM_TEST_CHECK(context, site_run.phone_panel.accepted_task_count == 0U);
    GS1_SYSTEM_TEST_CHECK(context, site_run.phone_panel.completed_task_count == 1U);
    GS1_SYSTEM_TEST_CHECK(context, site_run.phone_panel.claimed_task_count == 0U);

    task.runtime_list_kind = gs1::TaskRuntimeListKind::Claimed;
    site_run.task_board.completed_task_ids.clear();
    site_run.task_board.claimed_task_ids.push_back(task.task_instance_id);
    PhonePanelSystem::run(phone_context);
    GS1_SYSTEM_TEST_CHECK(context, site_run.phone_panel.accepted_task_count == 0U);
    GS1_SYSTEM_TEST_CHECK(context, site_run.phone_panel.completed_task_count == 0U);
    GS1_SYSTEM_TEST_CHECK(context, site_run.phone_panel.claimed_task_count == 1U);
}

void phone_panel_unread_badges_follow_launcher_and_app_visibility(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 1206U);
    GameMessageQueue queue {};
    auto inventory_context = make_site_context<InventorySystem>(campaign, site_run, queue);
    auto economy_context = make_site_context<EconomyPhoneSystem>(campaign, site_run, queue);
    auto phone_context = make_site_context<PhonePanelSystem>(campaign, site_run, queue);

    seed_site_one_inventory(campaign, site_run);
    const auto start_message =
        make_message(GameMessageType::SiteRunStarted, SiteRunStartedMessage {1U, 1U, 101U, 1U, 42ULL});
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        EconomyPhoneSystem::process_message(economy_context, start_message) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        PhonePanelSystem::process_message(phone_context, start_message) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, site_run.phone_panel.badge_flags == 0U);

    auto& task = site_run.task_board.visible_tasks.emplace_back();
    task.task_instance_id = gs1::TaskInstanceId {9U};
    task.runtime_list_kind = gs1::TaskRuntimeListKind::PendingClaim;
    site_run.task_board.completed_task_ids.push_back(task.task_instance_id);
    PhonePanelSystem::run(phone_context);
    GS1_SYSTEM_TEST_CHECK(
        context,
        (site_run.phone_panel.badge_flags & GS1_PHONE_PANEL_FLAG_LAUNCHER_BADGE) != 0U);
    GS1_SYSTEM_TEST_CHECK(
        context,
        (site_run.phone_panel.badge_flags & GS1_PHONE_PANEL_FLAG_TASKS_BADGE) != 0U);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        PhonePanelSystem::process_message(
            phone_context,
            make_message(
                GameMessageType::PhonePanelSectionRequested,
                gs1::PhonePanelSectionRequestedMessage {
                    GS1_PHONE_PANEL_SECTION_HOME,
                    {0U, 0U, 0U}})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(
        context,
        (site_run.phone_panel.badge_flags & GS1_PHONE_PANEL_FLAG_LAUNCHER_BADGE) == 0U);
    GS1_SYSTEM_TEST_CHECK(
        context,
        (site_run.phone_panel.badge_flags & GS1_PHONE_PANEL_FLAG_TASKS_BADGE) != 0U);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        PhonePanelSystem::process_message(
            phone_context,
            make_message(
                GameMessageType::PhonePanelSectionRequested,
                gs1::PhonePanelSectionRequestedMessage {
                    GS1_PHONE_PANEL_SECTION_TASKS,
                    {0U, 0U, 0U}})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(
        context,
        (site_run.phone_panel.badge_flags & GS1_PHONE_PANEL_FLAG_TASKS_BADGE) == 0U);

    const auto buy_listing_it = std::find_if(
        site_run.economy.available_phone_listings.begin(),
        site_run.economy.available_phone_listings.end(),
        [&site_run](const gs1::PhoneListingState& listing) {
            if (listing.kind != gs1::PhoneListingKind::BuyItem)
            {
                return false;
            }

            const auto* item_def = gs1::find_item_def(listing.item_id);
            return item_def != nullptr &&
                gs1::item_has_capability(*item_def, gs1::ITEM_CAPABILITY_SELL) &&
                gs1::inventory_storage::available_item_quantity_in_container(
                    site_run,
                    gs1::inventory_storage::delivery_box_container(site_run),
                    listing.item_id) == 0U;
        });
    GS1_SYSTEM_TEST_REQUIRE(context, buy_listing_it != site_run.economy.available_phone_listings.end());

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        EconomyPhoneSystem::process_message(
            economy_context,
            make_message(
                GameMessageType::PhoneListingPurchaseRequested,
                gs1::PhoneListingPurchaseRequestedMessage {buy_listing_it->listing_id, 1U, 0U})) == GS1_STATUS_OK);
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
    }

    site_run.phone_panel.open = false;
    site_run.phone_panel.active_section = PhonePanelSection::Home;
    PhonePanelSystem::run(phone_context);
    GS1_SYSTEM_TEST_CHECK(
        context,
        (site_run.phone_panel.badge_flags & GS1_PHONE_PANEL_FLAG_LAUNCHER_BADGE) != 0U);
    GS1_SYSTEM_TEST_CHECK(
        context,
        (site_run.phone_panel.badge_flags & GS1_PHONE_PANEL_FLAG_SELL_BADGE) != 0U);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        PhonePanelSystem::process_message(
            phone_context,
            make_message(
                GameMessageType::PhonePanelSectionRequested,
                gs1::PhonePanelSectionRequestedMessage {
                    GS1_PHONE_PANEL_SECTION_SELL,
                    {0U, 0U, 0U}})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(
        context,
        (site_run.phone_panel.badge_flags & GS1_PHONE_PANEL_FLAG_LAUNCHER_BADGE) == 0U);
    GS1_SYSTEM_TEST_CHECK(
        context,
        (site_run.phone_panel.badge_flags & GS1_PHONE_PANEL_FLAG_SELL_BADGE) == 0U);
}

GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "phone_panel",
    "site_run_started_seeds_home_snapshot",
    phone_panel_site_run_started_seeds_home_snapshot);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "phone_panel",
    "section_request_switches_authoritative_section",
    phone_panel_section_request_switches_authoritative_section);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "phone_panel",
    "rejects_unknown_section_request",
    phone_panel_rejects_unknown_section_request);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "phone_panel",
    "sell_list_refreshes_when_purchase_delivery_arrives",
    phone_panel_sell_list_refreshes_when_purchase_delivery_arrives);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "phone_panel",
    "task_counts_follow_pending_claim_until_reward_claim",
    phone_panel_task_counts_follow_pending_claim_until_reward_claim);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "phone_panel",
    "unread_badges_follow_launcher_and_app_visibility",
    phone_panel_unread_badges_follow_launcher_and_app_visibility);
}  // namespace
