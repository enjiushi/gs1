#include "campaign/campaign_state.h"
#include "campaign/systems/campaign_flow_system.h"
#include "campaign/systems/loadout_planner_system.h"
#include "campaign/systems/regional_support_system.h"
#include "campaign/systems/technology_system.h"
#include "messages/game_message.h"
#include "content/defs/item_defs.h"
#include "site/site_world_access.h"
#include "site/systems/failure_recovery_system.h"
#include "site/systems/site_completion_system.h"
#include "site/systems/site_flow_system.h"
#include "testing/system_test_registry.h"
#include "system_test_fixtures.h"

#include <algorithm>

namespace
{
using gs1::CampaignFixedStepContext;
using gs1::CampaignFlowMessageContext;
using gs1::CampaignFlowSystem;
using gs1::DayPhase;
using gs1::DeploymentSiteSelectionChangedMessage;
using gs1::FailureRecoverySystem;
using gs1::GameMessage;
using gs1::GameMessageQueue;
using gs1::GameMessageType;
using gs1::LoadoutPlannerSystem;
using gs1::SiteAttemptEndedMessage;
using gs1::SiteCompletionSystem;
using gs1::SiteFlowSystem;
using gs1::SiteId;
using gs1::SiteMoveDirectionInput;
using gs1::SiteRunStartedMessage;
using gs1::SiteRunStatus;
using gs1::StartNewCampaignMessage;
using gs1::StartSiteAttemptMessage;
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

const gs1::LoadoutSlot* find_loadout_slot(
    const std::vector<gs1::LoadoutSlot>& slots,
    std::uint32_t item_id) noexcept
{
    for (const auto& slot : slots)
    {
        if (slot.occupied && slot.item_id.value == item_id)
        {
            return &slot;
        }
    }

    return nullptr;
}

void campaign_flow_start_new_campaign_initializes_state(gs1::testing::SystemTestExecutionContext& context)
{
    std::optional<gs1::CampaignState> campaign {};
    std::optional<gs1::SiteRunState> active_site_run {make_test_site_run(9U, 91U)};
    Gs1AppState app_state = GS1_APP_STATE_MAIN_MENU;
    GameMessageQueue queue {};

    CampaignFlowMessageContext flow_context {
        campaign,
        active_site_run,
        app_state,
        queue};

    const auto message = make_message(
        GameMessageType::StartNewCampaign,
        StartNewCampaignMessage {77ULL, 18U});

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        CampaignFlowSystem::process_message(flow_context, message) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(context, campaign.has_value());
    GS1_SYSTEM_TEST_CHECK(context, !active_site_run.has_value());
    GS1_SYSTEM_TEST_CHECK(context, app_state == GS1_APP_STATE_REGIONAL_MAP);
    GS1_SYSTEM_TEST_CHECK(context, campaign->app_state == GS1_APP_STATE_REGIONAL_MAP);
    GS1_SYSTEM_TEST_CHECK(context, campaign->campaign_seed == 77ULL);
    GS1_SYSTEM_TEST_CHECK(context, campaign->campaign_days_total == 18U);
    GS1_SYSTEM_TEST_CHECK(context, campaign->campaign_days_remaining == 18U);
    GS1_SYSTEM_TEST_CHECK(context, campaign->regional_map_state.revealed_site_ids.size() == 1U);
    GS1_SYSTEM_TEST_CHECK(context, campaign->regional_map_state.available_site_ids.size() == 1U);
    GS1_SYSTEM_TEST_CHECK(context, campaign->regional_map_state.available_site_ids.front().value == 1U);
    GS1_SYSTEM_TEST_CHECK(context, campaign->loadout_planner_state.baseline_deployment_items.size() == 6U);
    GS1_SYSTEM_TEST_CHECK(context, campaign->loadout_planner_state.selected_loadout_slots.size() == 6U);
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        find_loadout_slot(
            campaign->loadout_planner_state.selected_loadout_slots,
            gs1::k_item_wind_reed_seed_bundle) != nullptr);
    GS1_SYSTEM_TEST_CHECK(
        context,
        find_loadout_slot(
            campaign->loadout_planner_state.selected_loadout_slots,
            gs1::k_item_wind_reed_seed_bundle)
            ->quantity == 8U);
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        find_loadout_slot(
            campaign->loadout_planner_state.selected_loadout_slots,
            gs1::k_item_wood_bundle) != nullptr);
    GS1_SYSTEM_TEST_CHECK(
        context,
        find_loadout_slot(
            campaign->loadout_planner_state.selected_loadout_slots,
            gs1::k_item_wood_bundle)
            ->quantity == 6U);
    GS1_SYSTEM_TEST_CHECK(context, queue.empty());
}

void campaign_flow_selection_paths_update_selection_and_queue(gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign_value = make_campaign();
    std::optional<gs1::CampaignState> campaign {campaign_value};
    std::optional<gs1::SiteRunState> active_site_run {};
    Gs1AppState app_state = GS1_APP_STATE_REGIONAL_MAP;
    GameMessageQueue queue {};

    CampaignFlowMessageContext flow_context {
        campaign,
        active_site_run,
        app_state,
        queue};

    auto select_available = make_message(
        GameMessageType::SelectDeploymentSite,
        gs1::SelectDeploymentSiteMessage {1U});
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        CampaignFlowSystem::process_message(flow_context, select_available) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(context, campaign->regional_map_state.selected_site_id.has_value());
    GS1_SYSTEM_TEST_CHECK(context, campaign->regional_map_state.selected_site_id->value == 1U);
    GS1_SYSTEM_TEST_REQUIRE(context, queue.size() == 1U);
    GS1_SYSTEM_TEST_CHECK(context, queue.front().type == GameMessageType::DeploymentSiteSelectionChanged);
    GS1_SYSTEM_TEST_CHECK(
        context,
        queue.front().payload_as<DeploymentSiteSelectionChangedMessage>().selected_site_id == 1U);

    queue.clear();
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        CampaignFlowSystem::process_message(flow_context, select_available) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, queue.empty());

    auto clear_selection = make_message(
        GameMessageType::ClearDeploymentSiteSelection,
        gs1::ClearDeploymentSiteSelectionMessage {});
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        CampaignFlowSystem::process_message(flow_context, clear_selection) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, !campaign->regional_map_state.selected_site_id.has_value());
    GS1_SYSTEM_TEST_REQUIRE(context, queue.size() == 1U);
    GS1_SYSTEM_TEST_CHECK(
        context,
        queue.front().payload_as<DeploymentSiteSelectionChangedMessage>().selected_site_id == 0U);

    queue.clear();
    auto select_missing = make_message(
        GameMessageType::SelectDeploymentSite,
        gs1::SelectDeploymentSiteMessage {999U});
    GS1_SYSTEM_TEST_CHECK(
        context,
        CampaignFlowSystem::process_message(flow_context, select_missing) == GS1_STATUS_NOT_FOUND);

    auto select_unavailable = make_message(
        GameMessageType::SelectDeploymentSite,
        gs1::SelectDeploymentSiteMessage {2U});
    GS1_SYSTEM_TEST_CHECK(
        context,
        CampaignFlowSystem::process_message(flow_context, select_unavailable) == GS1_STATUS_INVALID_STATE);
}

void campaign_flow_start_attempt_and_return_to_map(gs1::testing::SystemTestExecutionContext& context)
{
    std::optional<gs1::CampaignState> campaign {make_campaign()};
    std::optional<gs1::SiteRunState> active_site_run {};
    Gs1AppState app_state = GS1_APP_STATE_REGIONAL_MAP;
    GameMessageQueue queue {};

    CampaignFlowMessageContext flow_context {
        campaign,
        active_site_run,
        app_state,
        queue};

    const auto start_message = make_message(
        GameMessageType::StartSiteAttempt,
        StartSiteAttemptMessage {1U});

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        CampaignFlowSystem::process_message(flow_context, start_message) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(context, active_site_run.has_value());
    GS1_SYSTEM_TEST_CHECK(context, active_site_run->site_id.value == 1U);
    GS1_SYSTEM_TEST_CHECK(context, active_site_run->site_run_id.value == 1U);
    GS1_SYSTEM_TEST_CHECK(context, app_state == GS1_APP_STATE_SITE_ACTIVE);
    GS1_SYSTEM_TEST_CHECK(context, campaign->app_state == GS1_APP_STATE_SITE_ACTIVE);
    GS1_SYSTEM_TEST_CHECK(context, campaign->active_site_id.has_value());
    GS1_SYSTEM_TEST_CHECK(context, campaign->active_site_id->value == 1U);
    GS1_SYSTEM_TEST_REQUIRE(context, queue.size() == 1U);
    GS1_SYSTEM_TEST_CHECK(context, queue.front().type == GameMessageType::SiteRunStarted);
    GS1_SYSTEM_TEST_CHECK(
        context,
        queue.front().payload_as<SiteRunStartedMessage>().site_archetype_id == 101U);
    GS1_SYSTEM_TEST_CHECK(
        context,
        find_site_meta(*campaign, 1U)->attempt_count == 1U);

    queue.clear();
    const auto return_message = make_message(
        GameMessageType::ReturnToRegionalMap,
        gs1::ReturnToRegionalMapMessage {});
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        CampaignFlowSystem::process_message(flow_context, return_message) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, !active_site_run.has_value());
    GS1_SYSTEM_TEST_CHECK(context, !campaign->active_site_id.has_value());
    GS1_SYSTEM_TEST_CHECK(context, app_state == GS1_APP_STATE_REGIONAL_MAP);
    GS1_SYSTEM_TEST_CHECK(context, campaign->app_state == GS1_APP_STATE_REGIONAL_MAP);
    GS1_SYSTEM_TEST_CHECK(context, queue.empty());
}

void campaign_flow_completed_attempt_reveals_adjacent_sites(gs1::testing::SystemTestExecutionContext& context)
{
    std::optional<gs1::CampaignState> campaign {make_campaign()};
    std::optional<gs1::SiteRunState> active_site_run {};
    Gs1AppState app_state = GS1_APP_STATE_REGIONAL_MAP;
    GameMessageQueue queue {};

    CampaignFlowMessageContext flow_context {
        campaign,
        active_site_run,
        app_state,
        queue};

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        CampaignFlowSystem::process_message(
            flow_context,
            make_message(GameMessageType::StartSiteAttempt, StartSiteAttemptMessage {1U})) == GS1_STATUS_OK);

    queue.clear();
    GS1_SYSTEM_TEST_REQUIRE(context, active_site_run.has_value());

    const auto end_message = make_message(
        GameMessageType::SiteAttemptEnded,
        SiteAttemptEndedMessage {1U, GS1_SITE_ATTEMPT_RESULT_COMPLETED});
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        CampaignFlowSystem::process_message(flow_context, end_message) == GS1_STATUS_OK);

    GS1_SYSTEM_TEST_CHECK(context, active_site_run->run_status == SiteRunStatus::Completed);
    GS1_SYSTEM_TEST_CHECK(context, active_site_run->result_newly_revealed_site_count == 1U);
    GS1_SYSTEM_TEST_CHECK(context, app_state == GS1_APP_STATE_SITE_RESULT);
    GS1_SYSTEM_TEST_CHECK(context, campaign->app_state == GS1_APP_STATE_SITE_RESULT);
    GS1_SYSTEM_TEST_CHECK(context, find_site_meta(*campaign, 1U)->site_state == GS1_SITE_STATE_COMPLETED);
    GS1_SYSTEM_TEST_CHECK(context, find_site_meta(*campaign, 2U)->site_state == GS1_SITE_STATE_AVAILABLE);
    GS1_SYSTEM_TEST_CHECK(
        context,
        std::find(
            campaign->regional_map_state.completed_site_ids.begin(),
            campaign->regional_map_state.completed_site_ids.end(),
            SiteId {1U}) != campaign->regional_map_state.completed_site_ids.end());
    GS1_SYSTEM_TEST_CHECK(
        context,
        std::find(
            campaign->regional_map_state.available_site_ids.begin(),
            campaign->regional_map_state.available_site_ids.end(),
            SiteId {2U}) != campaign->regional_map_state.available_site_ids.end());
}

void campaign_flow_failed_attempt_preserves_locked_neighbors(gs1::testing::SystemTestExecutionContext& context)
{
    std::optional<gs1::CampaignState> campaign {make_campaign()};
    std::optional<gs1::SiteRunState> active_site_run {};
    Gs1AppState app_state = GS1_APP_STATE_REGIONAL_MAP;
    GameMessageQueue queue {};

    CampaignFlowMessageContext flow_context {
        campaign,
        active_site_run,
        app_state,
        queue};

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        CampaignFlowSystem::process_message(
            flow_context,
            make_message(GameMessageType::StartSiteAttempt, StartSiteAttemptMessage {1U})) == GS1_STATUS_OK);

    queue.clear();
    GS1_SYSTEM_TEST_REQUIRE(context, active_site_run.has_value());

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        CampaignFlowSystem::process_message(
            flow_context,
            make_message(
                GameMessageType::SiteAttemptEnded,
                SiteAttemptEndedMessage {1U, GS1_SITE_ATTEMPT_RESULT_FAILED})) == GS1_STATUS_OK);

    GS1_SYSTEM_TEST_CHECK(context, active_site_run->run_status == SiteRunStatus::Failed);
    GS1_SYSTEM_TEST_CHECK(context, active_site_run->result_newly_revealed_site_count == 0U);
    GS1_SYSTEM_TEST_CHECK(context, find_site_meta(*campaign, 1U)->site_state == GS1_SITE_STATE_AVAILABLE);
    GS1_SYSTEM_TEST_CHECK(context, find_site_meta(*campaign, 2U)->site_state == GS1_SITE_STATE_LOCKED);
}

void campaign_flow_run_advances_campaign_days(gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign(42ULL, 3U);
    CampaignFixedStepContext step_context {campaign};

    CampaignFlowSystem::run(step_context);
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(campaign.campaign_clock_minutes_elapsed, 0.2));
    GS1_SYSTEM_TEST_CHECK(context, campaign.campaign_days_remaining == 3U);

    campaign.campaign_clock_minutes_elapsed = 1439.9;
    campaign.campaign_days_remaining = 3U;
    CampaignFlowSystem::run(step_context);
    GS1_SYSTEM_TEST_CHECK(context, campaign.campaign_days_remaining == 2U);

    campaign.campaign_clock_minutes_elapsed = 3.0 * 1440.0;
    campaign.campaign_days_remaining = 2U;
    CampaignFlowSystem::run(step_context);
    GS1_SYSTEM_TEST_CHECK(context, campaign.campaign_days_remaining == 0U);
}

void loadout_planner_tracks_selected_target_site(gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto campaign_context = make_campaign_context(campaign);

    auto select_message = make_message(
        GameMessageType::DeploymentSiteSelectionChanged,
        DeploymentSiteSelectionChangedMessage {3U});
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        LoadoutPlannerSystem::process_message(campaign_context, select_message) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(context, campaign.loadout_planner_state.selected_target_site_id.has_value());
    GS1_SYSTEM_TEST_CHECK(context, campaign.loadout_planner_state.selected_target_site_id->value == 3U);
    GS1_SYSTEM_TEST_CHECK(context, campaign.loadout_planner_state.selected_loadout_slots.size() == 6U);

    auto clear_message = make_message(
        GameMessageType::DeploymentSiteSelectionChanged,
        DeploymentSiteSelectionChangedMessage {0U});
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        LoadoutPlannerSystem::process_message(campaign_context, clear_message) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, !campaign.loadout_planner_state.selected_target_site_id.has_value());
    GS1_SYSTEM_TEST_CHECK(context, campaign.loadout_planner_state.selected_loadout_slots.size() == 6U);
}

void site_flow_moves_worker_and_marks_projection_dirty(gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 301U, 101U, 8U, 8U, {2, 2}, {2, 2});
    GameMessageQueue queue {};

    auto site_context = make_site_context<SiteFlowSystem>(
        campaign,
        site_run,
        queue,
        1.0,
        SiteMoveDirectionInput {1.0f, 0.0f, 0.0f, true});

    SiteFlowSystem::run(site_context);

    const auto worker = gs1::site_world_access::worker_position(site_run);
    GS1_SYSTEM_TEST_CHECK(context, worker.tile_x > 2.0f);
    GS1_SYSTEM_TEST_CHECK(context, worker.tile_coord.x > 2);
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(worker.facing_degrees, 90.0f));
    GS1_SYSTEM_TEST_CHECK(
        context,
        (site_run.pending_projection_update_flags & gs1::SITE_PROJECTION_UPDATE_WORKER) != 0U);
}

void site_flow_respects_traversability_and_updates_phase(gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 302U, 101U, 8U, 8U, {2, 2}, {2, 2});
    GameMessageQueue queue {};

    site_run.clock.world_time_minutes = 359.9;
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        with_tile_component_mut<gs1::site_ecs::TileTraversable>(
            site_run,
            TileCoord {3, 2},
            [](auto& traversable) { traversable.value = false; }));

    auto site_context = make_site_context<SiteFlowSystem>(
        campaign,
        site_run,
        queue,
        0.25,
        SiteMoveDirectionInput {1.0f, 0.0f, 0.0f, true});

    SiteFlowSystem::run(site_context);

    const auto worker = gs1::site_world_access::worker_position(site_run);
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(worker.tile_x, 2.0f));
    GS1_SYSTEM_TEST_CHECK(context, worker.tile_coord.x == 2);
    GS1_SYSTEM_TEST_CHECK(context, site_run.clock.day_phase == DayPhase::Day);
    GS1_SYSTEM_TEST_CHECK(
        context,
        (site_run.pending_projection_update_flags & gs1::SITE_PROJECTION_UPDATE_WORKER) == 0U);
}

void site_completion_only_emits_when_threshold_is_met(gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 401U);
    GameMessageQueue queue {};

    site_run.counters.site_completion_tile_threshold = 2U;
    site_run.counters.fully_grown_tile_count = 2U;

    auto site_context = make_site_context<SiteCompletionSystem>(campaign, site_run, queue);
    SiteCompletionSystem::run(site_context);

    GS1_SYSTEM_TEST_REQUIRE(context, queue.size() == 1U);
    GS1_SYSTEM_TEST_CHECK(context, queue.front().type == GameMessageType::SiteAttemptEnded);
    GS1_SYSTEM_TEST_CHECK(
        context,
        queue.front().payload_as<SiteAttemptEndedMessage>().result == GS1_SITE_ATTEMPT_RESULT_COMPLETED);

    SiteCompletionSystem::run(site_context);
    GS1_SYSTEM_TEST_CHECK(context, queue.size() == 1U);

    queue.clear();
    site_run.counters.site_completion_tile_threshold = 0U;
    SiteCompletionSystem::run(site_context);
    GS1_SYSTEM_TEST_CHECK(context, queue.empty());
}

void failure_recovery_only_emits_when_worker_health_is_zero(gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 402U);
    GameMessageQueue queue {};

    auto site_context = make_site_context<FailureRecoverySystem>(campaign, site_run, queue);

    FailureRecoverySystem::run(site_context);
    GS1_SYSTEM_TEST_CHECK(context, queue.empty());

    auto worker = gs1::site_world_access::worker_conditions(site_run);
    worker.health = 0.0f;
    gs1::site_world_access::set_worker_conditions(site_run, worker);
    FailureRecoverySystem::run(site_context);

    GS1_SYSTEM_TEST_REQUIRE(context, queue.size() == 1U);
    GS1_SYSTEM_TEST_CHECK(context, queue.front().type == GameMessageType::SiteAttemptEnded);
    GS1_SYSTEM_TEST_CHECK(
        context,
        queue.front().payload_as<SiteAttemptEndedMessage>().result == GS1_SITE_ATTEMPT_RESULT_FAILED);

    FailureRecoverySystem::run(site_context);
    GS1_SYSTEM_TEST_CHECK(context, queue.size() == 1U);
}

void technology_placeholder_test_plan_is_recorded(gs1::testing::SystemTestExecutionContext& context)
{
    (void)sizeof(gs1::TechnologySystem);
    GS1_SYSTEM_TEST_CHECK(context, true);
}

void regional_support_placeholder_test_plan_is_recorded(gs1::testing::SystemTestExecutionContext& context)
{
    (void)sizeof(gs1::RegionalSupportSystem);
    GS1_SYSTEM_TEST_CHECK(context, true);
}
}  // namespace

GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "campaign_flow",
    "start_new_campaign_initializes_state",
    campaign_flow_start_new_campaign_initializes_state);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "campaign_flow",
    "selection_paths_update_selection_and_queue",
    campaign_flow_selection_paths_update_selection_and_queue);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "campaign_flow",
    "start_attempt_and_return_to_map",
    campaign_flow_start_attempt_and_return_to_map);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "campaign_flow",
    "completed_attempt_reveals_adjacent_sites",
    campaign_flow_completed_attempt_reveals_adjacent_sites);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "campaign_flow",
    "failed_attempt_preserves_locked_neighbors",
    campaign_flow_failed_attempt_preserves_locked_neighbors);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "campaign_flow",
    "run_advances_campaign_days",
    campaign_flow_run_advances_campaign_days);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "loadout_planner",
    "tracks_selected_target_site",
    loadout_planner_tracks_selected_target_site);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "site_flow",
    "moves_worker_and_marks_projection_dirty",
    site_flow_moves_worker_and_marks_projection_dirty);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "site_flow",
    "respects_traversability_and_updates_phase",
    site_flow_respects_traversability_and_updates_phase);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "site_completion",
    "only_emits_when_threshold_is_met",
    site_completion_only_emits_when_threshold_is_met);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "failure_recovery",
    "only_emits_when_worker_health_is_zero",
    failure_recovery_only_emits_when_worker_health_is_zero);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "technology",
    "placeholder_test_plan_is_recorded",
    technology_placeholder_test_plan_is_recorded);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "regional_support",
    "placeholder_test_plan_is_recorded",
    regional_support_placeholder_test_plan_is_recorded);
