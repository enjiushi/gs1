#include "campaign/campaign_state.h"
#include "campaign/systems/campaign_flow_system.h"
#include "campaign/systems/campaign_time_system.h"
#include "campaign/systems/faction_reputation_system.h"
#include "campaign/systems/loadout_planner_system.h"
#include "campaign/systems/regional_support_system.h"
#include "campaign/systems/technology_system.h"
#include "content/defs/faction_defs.h"
#include "content/defs/item_defs.h"
#include "content/defs/technology_defs.h"
#include "messages/game_message.h"
#include "runtime/runtime_clock.h"
#include "site/site_world_access.h"
#include "site/systems/failure_recovery_system.h"
#include "site/systems/site_completion_system.h"
#include "site/systems/site_flow_system.h"
#include "site/systems/site_time_system.h"
#include "testing/system_test_registry.h"
#include "system_test_fixtures.h"

#include <algorithm>

namespace
{
using gs1::CampaignFixedStepContext;
using gs1::CampaignFlowMessageContext;
using gs1::CampaignFlowSystem;
using gs1::CampaignTimeSystem;
using gs1::DayPhase;
using gs1::DeploymentSiteSelectionChangedMessage;
using gs1::FailureRecoverySystem;
using gs1::FactionReputationSystem;
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
using gs1::SiteTimeSystem;
using gs1::StartNewCampaignMessage;
using gs1::StartSiteAttemptMessage;
using gs1::TileCoord;
using gs1::TechnologySystem;
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
    GS1_SYSTEM_TEST_CHECK(context, campaign->faction_progress.size() == 3U);
    GS1_SYSTEM_TEST_CHECK(context, gs1::k_faction_tech_tier_count == 4U);
    GS1_SYSTEM_TEST_CHECK(context, campaign->faction_progress[0].faction_id.value == gs1::k_faction_village_committee);
    GS1_SYSTEM_TEST_CHECK(context, campaign->faction_progress[1].faction_id.value == gs1::k_faction_forestry_bureau);
    GS1_SYSTEM_TEST_CHECK(context, campaign->faction_progress[2].faction_id.value == gs1::k_faction_agricultural_university);
    GS1_SYSTEM_TEST_CHECK(context, !campaign->regional_map_state.tech_tree_open);
    GS1_SYSTEM_TEST_CHECK(
        context,
        campaign->regional_map_state.selected_tech_tree_faction_id.value == gs1::k_faction_village_committee);
    GS1_SYSTEM_TEST_CHECK(context, campaign->faction_progress[0].occupied_reputation == 0);
    GS1_SYSTEM_TEST_CHECK(context, campaign->loadout_planner_state.baseline_deployment_items.size() == 2U);
    GS1_SYSTEM_TEST_CHECK(context, campaign->loadout_planner_state.selected_loadout_slots.size() == 2U);
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        find_loadout_slot(
            campaign->loadout_planner_state.selected_loadout_slots,
            gs1::k_item_basic_straw_checkerboard) != nullptr);
    GS1_SYSTEM_TEST_CHECK(
        context,
        find_loadout_slot(
            campaign->loadout_planner_state.selected_loadout_slots,
            gs1::k_item_basic_straw_checkerboard)
            ->quantity == 8U);
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        find_loadout_slot(
            campaign->loadout_planner_state.selected_loadout_slots,
            gs1::k_item_water_container) != nullptr);
    GS1_SYSTEM_TEST_CHECK(
        context,
        find_loadout_slot(
            campaign->loadout_planner_state.selected_loadout_slots,
            gs1::k_item_water_container)
            ->quantity == 1U);
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

    campaign->regional_map_state.tech_tree_open = true;

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
    GS1_SYSTEM_TEST_CHECK(context, !campaign->regional_map_state.tech_tree_open);
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
    GS1_SYSTEM_TEST_CHECK(
        context,
        count_messages(queue, GameMessageType::CampaignReputationAwardRequested) == 1U);
    GS1_SYSTEM_TEST_CHECK(
        context,
        count_messages(queue, GameMessageType::FactionReputationAwardRequested) == 1U);
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
    const double default_step_minutes =
        gs1::runtime_minutes_from_real_seconds(step_context.fixed_step_seconds);

    CampaignTimeSystem::run(step_context);
    GS1_SYSTEM_TEST_CHECK(
        context,
        approx_equal(campaign.campaign_clock_minutes_elapsed, default_step_minutes));
    GS1_SYSTEM_TEST_CHECK(context, campaign.campaign_days_remaining == 3U);

    campaign.campaign_clock_minutes_elapsed = gs1::k_runtime_minutes_per_day - 0.1;
    campaign.campaign_days_remaining = 3U;
    CampaignTimeSystem::run(step_context);
    GS1_SYSTEM_TEST_CHECK(context, campaign.campaign_days_remaining == 2U);

    campaign.campaign_clock_minutes_elapsed = 3.0 * gs1::k_runtime_minutes_per_day;
    campaign.campaign_days_remaining = 2U;
    CampaignTimeSystem::run(step_context);
    GS1_SYSTEM_TEST_CHECK(context, campaign.campaign_days_remaining == 0U);

    campaign.campaign_clock_minutes_elapsed = 0.0;
    campaign.campaign_days_remaining = 3U;
    CampaignFixedStepContext minute_step_context {campaign, 60.0};
    for (int step = 0; step < 30; ++step)
    {
        CampaignTimeSystem::run(minute_step_context);
    }
    GS1_SYSTEM_TEST_CHECK(
        context,
        approx_equal(campaign.campaign_clock_minutes_elapsed, gs1::k_runtime_minutes_per_day));
    GS1_SYSTEM_TEST_CHECK(context, campaign.campaign_days_remaining == 2U);
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
    GS1_SYSTEM_TEST_CHECK(context, campaign.loadout_planner_state.selected_loadout_slots.size() == 2U);

    auto clear_message = make_message(
        GameMessageType::DeploymentSiteSelectionChanged,
        DeploymentSiteSelectionChangedMessage {0U});
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        LoadoutPlannerSystem::process_message(campaign_context, clear_message) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, !campaign.loadout_planner_state.selected_target_site_id.has_value());
    GS1_SYSTEM_TEST_CHECK(context, campaign.loadout_planner_state.selected_loadout_slots.size() == 2U);
}

void loadout_planner_builds_adjacent_completed_site_support(gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto* site_one = find_site_meta(campaign, 1U);
    auto* site_two = find_site_meta(campaign, 2U);
    GS1_SYSTEM_TEST_REQUIRE(context, site_one != nullptr);
    GS1_SYSTEM_TEST_REQUIRE(context, site_two != nullptr);
    site_one->site_state = GS1_SITE_STATE_COMPLETED;
    site_two->site_state = GS1_SITE_STATE_AVAILABLE;

    auto campaign_context = make_campaign_context(campaign);
    auto select_message = make_message(
        GameMessageType::DeploymentSiteSelectionChanged,
        DeploymentSiteSelectionChangedMessage {2U});
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        LoadoutPlannerSystem::process_message(campaign_context, select_message) == GS1_STATUS_OK);

    GS1_SYSTEM_TEST_REQUIRE(context, campaign.loadout_planner_state.selected_target_site_id.has_value());
    GS1_SYSTEM_TEST_CHECK(context, campaign.loadout_planner_state.selected_target_site_id->value == 2U);
    GS1_SYSTEM_TEST_CHECK(context, campaign.loadout_planner_state.support_quota == 1U);
    GS1_SYSTEM_TEST_CHECK(context, campaign.loadout_planner_state.available_exported_support_items.size() == 2U);
    GS1_SYSTEM_TEST_CHECK(context, campaign.loadout_planner_state.active_nearby_aura_modifier_ids.size() == 1U);
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        find_loadout_slot(
            campaign.loadout_planner_state.selected_loadout_slots,
            gs1::k_item_saltbush_seed_bundle) != nullptr);
    GS1_SYSTEM_TEST_CHECK(
        context,
        find_loadout_slot(
            campaign.loadout_planner_state.selected_loadout_slots,
            gs1::k_item_saltbush_seed_bundle)
            ->quantity == 4U);
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        find_loadout_slot(
            campaign.loadout_planner_state.selected_loadout_slots,
            gs1::k_item_wood_bundle) != nullptr);
    GS1_SYSTEM_TEST_CHECK(
        context,
        find_loadout_slot(
            campaign.loadout_planner_state.selected_loadout_slots,
            gs1::k_item_wood_bundle)
            ->quantity == 2U);
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

void site_flow_respects_traversability_and_time_system_updates_phase(
    gs1::testing::SystemTestExecutionContext& context)
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

    auto time_context = make_site_context<SiteTimeSystem>(
        campaign,
        site_run,
        queue,
        0.25,
        SiteMoveDirectionInput {1.0f, 0.0f, 0.0f, true});
    auto site_context = make_site_context<SiteFlowSystem>(
        campaign,
        site_run,
        queue,
        0.25,
        SiteMoveDirectionInput {1.0f, 0.0f, 0.0f, true});

    SiteTimeSystem::run(time_context);
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

void site_completion_highway_protection_fails_when_average_cover_reaches_target(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(5U, 403U);
    GameMessageQueue queue {};

    configure_highway_protection_objective(
        site_run,
        gs1::SiteObjectiveTargetEdge::East,
        12.0,
        0.5f);
    mark_objective_target_tile(site_run, TileCoord {7, 2});
    site_run.counters.highway_average_sand_cover = 0.5f;
    site_run.counters.objective_progress_normalized = 0.0f;

    auto site_context = make_site_context<SiteCompletionSystem>(campaign, site_run, queue);
    SiteCompletionSystem::run(site_context);

    GS1_SYSTEM_TEST_REQUIRE(context, queue.size() == 1U);
    GS1_SYSTEM_TEST_CHECK(context, queue.front().type == GameMessageType::SiteAttemptEnded);
    GS1_SYSTEM_TEST_CHECK(
        context,
        queue.front().payload_as<SiteAttemptEndedMessage>().result == GS1_SITE_ATTEMPT_RESULT_FAILED);
}

void site_completion_highway_protection_completes_at_time_limit_when_cover_stays_safe(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(5U, 404U);
    GameMessageQueue queue {};

    configure_highway_protection_objective(
        site_run,
        gs1::SiteObjectiveTargetEdge::East,
        12.0,
        0.5f);
    mark_objective_target_tile(site_run, TileCoord {7, 2});
    site_run.clock.world_time_minutes = 12.0;
    site_run.counters.highway_average_sand_cover = 0.35f;
    site_run.counters.objective_progress_normalized = 0.3f;

    auto site_context = make_site_context<SiteCompletionSystem>(campaign, site_run, queue);
    SiteCompletionSystem::run(site_context);

    GS1_SYSTEM_TEST_REQUIRE(context, queue.size() == 1U);
    GS1_SYSTEM_TEST_CHECK(context, queue.front().type == GameMessageType::SiteAttemptEnded);
    GS1_SYSTEM_TEST_CHECK(
        context,
        queue.front().payload_as<SiteAttemptEndedMessage>().result == GS1_SITE_ATTEMPT_RESULT_COMPLETED);
}

void site_completion_pure_survival_completes_at_time_limit(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(6U, 405U);
    GameMessageQueue queue {};

    configure_pure_survival_objective(site_run, 8.0);
    site_run.clock.world_time_minutes = 8.0;

    auto site_context = make_site_context<SiteCompletionSystem>(campaign, site_run, queue);
    SiteCompletionSystem::run(site_context);

    GS1_SYSTEM_TEST_REQUIRE(context, queue.size() == 1U);
    GS1_SYSTEM_TEST_CHECK(context, queue.front().type == GameMessageType::SiteAttemptEnded);
    GS1_SYSTEM_TEST_CHECK(
        context,
        queue.front().payload_as<SiteAttemptEndedMessage>().result == GS1_SITE_ATTEMPT_RESULT_COMPLETED);
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(site_run.counters.objective_progress_normalized, 1.0f));
}

void site_completion_green_wall_completes_after_stable_hold_and_pauses_main_timer(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(7U, 406U);
    GameMessageQueue queue {};

    configure_green_wall_connection_objective(
        site_run,
        gs1::SiteObjectiveTargetEdge::East,
        0.2,
        0.4);
    mark_objective_target_tile(site_run, TileCoord {5, 2});
    mark_green_wall_connection_start_tile(site_run, TileCoord {1, 2});
    mark_green_wall_connection_goal_tile(site_run, TileCoord {3, 2});
    for (const TileCoord coord : {TileCoord {1, 2}, TileCoord {2, 2}, TileCoord {3, 2}})
    {
        auto tile = site_run.site_world->tile_at(coord);
        tile.ecology.ground_cover_type_id = 1U;
        tile.ecology.plant_density = 1.0f;
        site_run.site_world->set_tile(coord, tile);
    }

    auto target_tile = site_run.site_world->tile_at(TileCoord {5, 2});
    target_tile.ecology.sand_burial = 0.2f;
    site_run.site_world->set_tile(TileCoord {5, 2}, target_tile);

    auto site_context = make_site_context<SiteCompletionSystem>(campaign, site_run, queue);
    site_run.clock.world_time_minutes = 0.2;
    SiteCompletionSystem::run(site_context);
    GS1_SYSTEM_TEST_CHECK(context, queue.empty());
    GS1_SYSTEM_TEST_CHECK(context, site_run.objective.completion_hold_progress_minutes >= 0.19);
    GS1_SYSTEM_TEST_CHECK(context, site_run.objective.paused_main_timer_minutes >= 0.19);
    GS1_SYSTEM_TEST_CHECK(context, site_run.counters.objective_progress_normalized >= 0.74f);

    site_run.clock.world_time_minutes = 0.4;
    SiteCompletionSystem::run(site_context);
    GS1_SYSTEM_TEST_REQUIRE(context, queue.size() == 1U);
    GS1_SYSTEM_TEST_CHECK(
        context,
        queue.front().payload_as<SiteAttemptEndedMessage>().result == GS1_SITE_ATTEMPT_RESULT_COMPLETED);
    GS1_SYSTEM_TEST_CHECK(context, site_run.objective.paused_main_timer_minutes >= 0.39);
}

void site_completion_green_wall_resets_hold_when_target_band_worsens(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(7U, 407U);
    GameMessageQueue queue {};

    configure_green_wall_connection_objective(
        site_run,
        gs1::SiteObjectiveTargetEdge::East,
        4.0,
        0.6);
    mark_objective_target_tile(site_run, TileCoord {5, 2});
    mark_green_wall_connection_start_tile(site_run, TileCoord {1, 2});
    mark_green_wall_connection_goal_tile(site_run, TileCoord {3, 2});
    for (const TileCoord coord : {TileCoord {1, 2}, TileCoord {2, 2}, TileCoord {3, 2}})
    {
        auto tile = site_run.site_world->tile_at(coord);
        tile.ecology.ground_cover_type_id = 1U;
        tile.ecology.plant_density = 1.0f;
        site_run.site_world->set_tile(coord, tile);
    }

    auto target_tile = site_run.site_world->tile_at(TileCoord {5, 2});
    target_tile.ecology.sand_burial = 0.1f;
    site_run.site_world->set_tile(TileCoord {5, 2}, target_tile);

    auto site_context = make_site_context<SiteCompletionSystem>(campaign, site_run, queue);
    site_run.clock.world_time_minutes = 0.2;
    SiteCompletionSystem::run(site_context);
    GS1_SYSTEM_TEST_CHECK(context, site_run.objective.completion_hold_progress_minutes >= 0.19);

    target_tile = site_run.site_world->tile_at(TileCoord {5, 2});
    target_tile.ecology.sand_burial = 0.35f;
    site_run.site_world->set_tile(TileCoord {5, 2}, target_tile);
    site_run.clock.world_time_minutes = 0.4;
    SiteCompletionSystem::run(site_context);

    GS1_SYSTEM_TEST_CHECK(context, queue.empty());
    GS1_SYSTEM_TEST_CHECK(context, site_run.objective.completion_hold_progress_minutes <= 0.001);
    GS1_SYSTEM_TEST_CHECK(context, site_run.counters.objective_progress_normalized <= 0.51f);
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

void campaign_flow_tech_tree_open_and_close_toggles_regional_map_state(
    gs1::testing::SystemTestExecutionContext& context)
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
            make_message(
                GameMessageType::OpenRegionalMapTechTree,
                gs1::OpenRegionalMapTechTreeMessage {})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, campaign->regional_map_state.tech_tree_open);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        CampaignFlowSystem::process_message(
            flow_context,
            make_message(
                GameMessageType::CloseRegionalMapTechTree,
                gs1::CloseRegionalMapTechTreeMessage {})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, !campaign->regional_map_state.tech_tree_open);
}

void campaign_flow_selects_tech_tree_faction_tab(gs1::testing::SystemTestExecutionContext& context)
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
            make_message(
                GameMessageType::SelectRegionalMapTechTreeFaction,
                gs1::SelectRegionalMapTechTreeFactionMessage {
                    gs1::k_faction_forestry_bureau})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(
        context,
        campaign->regional_map_state.selected_tech_tree_faction_id.value == gs1::k_faction_forestry_bureau);
}

void faction_reputation_awards_unlock_assistant_at_threshold(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto campaign_context = make_campaign_context(campaign);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        FactionReputationSystem::process_message(
            campaign_context,
            make_message(
                GameMessageType::FactionReputationAwardRequested,
                gs1::FactionReputationAwardRequestedMessage {
                    gs1::k_faction_village_committee,
                    10})) == GS1_STATUS_OK);

    GS1_SYSTEM_TEST_CHECK(context, campaign.faction_progress[0].faction_reputation == 10);
    GS1_SYSTEM_TEST_CHECK(
        context,
        TechnologySystem::available_faction_reputation(campaign, gs1::FactionId {gs1::k_faction_village_committee}) ==
            10);
    GS1_SYSTEM_TEST_CHECK(context, campaign.faction_progress[0].has_unlocked_assistant_package);
    GS1_SYSTEM_TEST_CHECK(context, campaign.faction_progress[0].unlocked_assistant_package_id == 1001U);
}

void technology_claim_occupies_faction_reputation_and_records_purchase(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto campaign_context = make_campaign_context(campaign);
    campaign.faction_progress[0].faction_reputation = 40;

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        TechnologySystem::process_message(
            campaign_context,
            make_message(
                GameMessageType::TechnologyNodeClaimRequested,
                gs1::TechnologyNodeClaimRequestedMessage {
                    gs1::k_tech_node_village_t1_field_rations})) == GS1_STATUS_OK);

    GS1_SYSTEM_TEST_CHECK(context, campaign.faction_progress[0].occupied_reputation == 10);
    GS1_SYSTEM_TEST_CHECK(
        context,
        TechnologySystem::available_faction_reputation(campaign, gs1::FactionId {gs1::k_faction_village_committee}) ==
            30);
    GS1_SYSTEM_TEST_CHECK(
        context,
        TechnologySystem::node_purchased(
            campaign,
            gs1::TechNodeId {gs1::k_tech_node_village_t1_field_rations}));
}

void technology_amplification_requires_base_and_locks_other_choice(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto campaign_context = make_campaign_context(campaign);
    campaign.faction_progress[0].faction_reputation = 80;

    GS1_SYSTEM_TEST_CHECK(
        context,
        TechnologySystem::process_message(
            campaign_context,
            make_message(
                GameMessageType::TechnologyNodeClaimRequested,
                gs1::TechnologyNodeClaimRequestedMessage {
                    gs1::k_tech_node_village_t1_field_rations_amp_recovery})) == GS1_STATUS_INVALID_STATE);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        TechnologySystem::process_message(
            campaign_context,
            make_message(
                GameMessageType::TechnologyNodeClaimRequested,
                gs1::TechnologyNodeClaimRequestedMessage {
                    gs1::k_tech_node_village_t1_field_rations})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        TechnologySystem::process_message(
            campaign_context,
            make_message(
                GameMessageType::TechnologyNodeClaimRequested,
                gs1::TechnologyNodeClaimRequestedMessage {
                    gs1::k_tech_node_village_t1_field_rations_amp_recovery})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, campaign.faction_progress[0].occupied_reputation == 22);
    GS1_SYSTEM_TEST_CHECK(
        context,
        TechnologySystem::process_message(
            campaign_context,
            make_message(
                GameMessageType::TechnologyNodeClaimRequested,
                gs1::TechnologyNodeClaimRequestedMessage {
                    gs1::k_tech_node_village_t1_field_rations_amp_comfort})) == GS1_STATUS_INVALID_STATE);
}

void technology_next_tier_requires_base_and_amplification_in_previous_tier(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto campaign_context = make_campaign_context(campaign);
    campaign.faction_progress[0].faction_reputation = 80;

    GS1_SYSTEM_TEST_CHECK(
        context,
        TechnologySystem::process_message(
            campaign_context,
            make_message(
                GameMessageType::TechnologyNodeClaimRequested,
                gs1::TechnologyNodeClaimRequestedMessage {
                    gs1::k_tech_node_village_t2_stove_network})) == GS1_STATUS_INVALID_STATE);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        TechnologySystem::process_message(
            campaign_context,
            make_message(
                GameMessageType::TechnologyNodeClaimRequested,
                gs1::TechnologyNodeClaimRequestedMessage {
                    gs1::k_tech_node_village_t1_field_rations})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(
        context,
        TechnologySystem::process_message(
            campaign_context,
            make_message(
                GameMessageType::TechnologyNodeClaimRequested,
                gs1::TechnologyNodeClaimRequestedMessage {
                    gs1::k_tech_node_village_t2_stove_network})) == GS1_STATUS_INVALID_STATE);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        TechnologySystem::process_message(
            campaign_context,
            make_message(
                GameMessageType::TechnologyNodeClaimRequested,
                gs1::TechnologyNodeClaimRequestedMessage {
                    gs1::k_tech_node_village_t1_field_rations_amp_recovery})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        TechnologySystem::process_message(
            campaign_context,
            make_message(
                GameMessageType::TechnologyNodeClaimRequested,
                gs1::TechnologyNodeClaimRequestedMessage {
                    gs1::k_tech_node_village_t2_stove_network})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, campaign.faction_progress[0].occupied_reputation == 40);
}

void technology_additional_same_tier_purchase_increases_cost(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto campaign_context = make_campaign_context(campaign);
    campaign.faction_progress[0].faction_reputation = 100;

    GS1_SYSTEM_TEST_CHECK(
        context,
        TechnologySystem::current_reputation_cost(
            campaign,
            *gs1::find_technology_node_def(gs1::TechNodeId {gs1::k_tech_node_village_t1_field_rations})) == 10);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        TechnologySystem::process_message(
            campaign_context,
            make_message(
                GameMessageType::TechnologyNodeClaimRequested,
                gs1::TechnologyNodeClaimRequestedMessage {
                    gs1::k_tech_node_village_t1_field_rations})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(
        context,
        TechnologySystem::current_reputation_cost(
            campaign,
            *gs1::find_technology_node_def(gs1::TechNodeId {gs1::k_tech_node_village_t1_shift_meals})) == 12);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        TechnologySystem::process_message(
            campaign_context,
            make_message(
                GameMessageType::TechnologyNodeClaimRequested,
                gs1::TechnologyNodeClaimRequestedMessage {
                    gs1::k_tech_node_village_t1_shift_meals})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(
        context,
        TechnologySystem::current_reputation_cost(
            campaign,
            *gs1::find_technology_node_def(gs1::TechNodeId {gs1::k_tech_node_village_t1_supply_runners})) == 15);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        TechnologySystem::process_message(
            campaign_context,
            make_message(
                GameMessageType::TechnologyNodeClaimRequested,
                gs1::TechnologyNodeClaimRequestedMessage {
                    gs1::k_tech_node_village_t1_supply_runners})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(
        context,
        TechnologySystem::current_reputation_cost(
            campaign,
            *gs1::find_technology_node_def(gs1::TechNodeId {
                gs1::k_tech_node_village_t1_field_rations_amp_recovery})) == 18);
    GS1_SYSTEM_TEST_CHECK(context, campaign.faction_progress[0].occupied_reputation == 37);
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
    "loadout_planner",
    "builds_adjacent_completed_site_support",
    loadout_planner_builds_adjacent_completed_site_support);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "site_flow",
    "moves_worker_and_marks_projection_dirty",
    site_flow_moves_worker_and_marks_projection_dirty);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "site_flow",
    "respects_traversability_and_time_system_updates_phase",
    site_flow_respects_traversability_and_time_system_updates_phase);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "site_completion",
    "only_emits_when_threshold_is_met",
    site_completion_only_emits_when_threshold_is_met);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "site_completion",
    "highway_protection_fails_when_average_cover_reaches_target",
    site_completion_highway_protection_fails_when_average_cover_reaches_target);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "site_completion",
    "highway_protection_completes_at_time_limit_when_cover_stays_safe",
    site_completion_highway_protection_completes_at_time_limit_when_cover_stays_safe);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "site_completion",
    "pure_survival_completes_at_time_limit",
    site_completion_pure_survival_completes_at_time_limit);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "site_completion",
    "green_wall_completes_after_stable_hold_and_pauses_main_timer",
    site_completion_green_wall_completes_after_stable_hold_and_pauses_main_timer);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "site_completion",
    "green_wall_resets_hold_when_target_band_worsens",
    site_completion_green_wall_resets_hold_when_target_band_worsens);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "failure_recovery",
    "only_emits_when_worker_health_is_zero",
    failure_recovery_only_emits_when_worker_health_is_zero);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "campaign_flow",
    "tech_tree_open_and_close_toggles_regional_map_state",
    campaign_flow_tech_tree_open_and_close_toggles_regional_map_state);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "campaign_flow",
    "selects_tech_tree_faction_tab",
    campaign_flow_selects_tech_tree_faction_tab);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "faction_reputation",
    "awards_unlock_assistant_at_threshold",
    faction_reputation_awards_unlock_assistant_at_threshold);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "technology",
    "claim_occupies_faction_reputation_and_records_purchase",
    technology_claim_occupies_faction_reputation_and_records_purchase);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "technology",
    "amplification_requires_base_and_locks_other_choice",
    technology_amplification_requires_base_and_locks_other_choice);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "technology",
    "next_tier_requires_base_and_amplification_in_previous_tier",
    technology_next_tier_requires_base_and_amplification_in_previous_tier);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "technology",
    "additional_same_tier_purchase_increases_cost",
    technology_additional_same_tier_purchase_increases_cost);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "regional_support",
    "placeholder_test_plan_is_recorded",
    regional_support_placeholder_test_plan_is_recorded);
