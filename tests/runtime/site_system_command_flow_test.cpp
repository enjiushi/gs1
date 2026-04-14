#include "app/campaign_factory.h"
#include "campaign/campaign_state.h"
#include "campaign/systems/campaign_system_context.h"
#include "campaign/systems/campaign_flow_system.h"
#include "campaign/systems/loadout_planner_system.h"
#include "commands/game_command.h"
#include "site/site_run_state.h"
#include "site/systems/failure_recovery_system.h"
#include "site/systems/site_completion_system.h"
#include "site/systems/site_flow_system.h"
#include "site/systems/site_system_context.h"

#include <cassert>

namespace
{
using gs1::CampaignState;
using gs1::CampaignFixedStepContext;
using gs1::CampaignFlowCommandContext;
using gs1::CampaignSystemContext;
using gs1::CampaignFactory;
using gs1::CampaignFlowSystem;
using gs1::DayPhase;
using gs1::DeploymentSiteSelectionChangedCommand;
using gs1::FailureRecoverySystem;
using gs1::GameCommandQueue;
using gs1::GameCommandType;
using gs1::LoadoutPlannerSystem;
using gs1::SiteCompletionSystem;
using gs1::SiteAttemptEndedCommand;
using gs1::SiteRunStartedCommand;
using gs1::SiteFlowSystem;
using gs1::SiteId;
using gs1::SiteRunState;
using gs1::SiteRunStatus;
using gs1::SiteSystemContext;
using gs1::SiteMoveDirectionInput;
using gs1::StartSiteAttemptCommand;

template <typename SiteSystemTag>
SiteSystemContext<SiteSystemTag> make_site_context(
    CampaignState& campaign,
    SiteRunState& site_run,
    GameCommandQueue& command_queue)
{
    return gs1::make_site_system_context<SiteSystemTag>(
        campaign,
        site_run,
        command_queue,
        0.25,
        SiteMoveDirectionInput {});
}
}  // namespace

int main()
{
    CampaignState campaign {};
    campaign.campaign_days_total = 30U;
    campaign.campaign_days_remaining = 30U;

    CampaignSystemContext campaign_context {campaign};
    gs1::GameCommand selection_changed {};
    selection_changed.type = GameCommandType::DeploymentSiteSelectionChanged;
    selection_changed.set_payload(DeploymentSiteSelectionChangedCommand {3U});
    assert(LoadoutPlannerSystem::subscribes_to(selection_changed.type));
    assert(LoadoutPlannerSystem::process_command(campaign_context, selection_changed) == GS1_STATUS_OK);
    assert(campaign.loadout_planner_state.selected_target_site_id.has_value());
    assert(campaign.loadout_planner_state.selected_target_site_id->value == 3U);

    selection_changed.set_payload(DeploymentSiteSelectionChangedCommand {0U});
    assert(LoadoutPlannerSystem::process_command(campaign_context, selection_changed) == GS1_STATUS_OK);
    assert(!campaign.loadout_planner_state.selected_target_site_id.has_value());

    CampaignFixedStepContext campaign_step_context {campaign};
    CampaignFlowSystem::run(campaign_step_context);
    assert(campaign.campaign_clock_minutes_elapsed > 0.0);
    assert(campaign.campaign_days_remaining == 30U);
    campaign.campaign_clock_minutes_elapsed = 0.0;

    std::optional<CampaignState> active_campaign {
        CampaignFactory::create_prototype_campaign(42ULL, 30U)};
    std::optional<SiteRunState> active_site_run {};
    Gs1AppState app_state = GS1_APP_STATE_REGIONAL_MAP;
    GameCommandQueue site_start_queue {};
    CampaignFlowCommandContext flow_context {
        active_campaign,
        active_site_run,
        app_state,
        site_start_queue};
    gs1::GameCommand start_site_attempt {};
    start_site_attempt.type = GameCommandType::StartSiteAttempt;
    start_site_attempt.set_payload(StartSiteAttemptCommand {1U});
    assert(CampaignFlowSystem::process_command(flow_context, start_site_attempt) == GS1_STATUS_OK);
    assert(active_site_run.has_value());
    assert(app_state == GS1_APP_STATE_SITE_ACTIVE);
    assert(site_start_queue.size() == 1U);
    assert(site_start_queue.front().type == GameCommandType::SiteRunStarted);
    assert(site_start_queue.front().payload_as<SiteRunStartedCommand>().site_id == 1U);
    assert(site_start_queue.front().payload_as<SiteRunStartedCommand>().site_run_id == 1U);
    assert(site_start_queue.front().payload_as<SiteRunStartedCommand>().site_archetype_id == 101U);
    assert(site_start_queue.front().payload_as<SiteRunStartedCommand>().attempt_index == 1U);

    SiteRunState site_run {};
    site_run.site_id = SiteId {7U};
    site_run.run_status = SiteRunStatus::Active;
    site_run.tile_grid.width = 8U;
    site_run.tile_grid.height = 8U;
    site_run.tile_grid.tile_traversable.assign(site_run.tile_grid.tile_count(), 1U);
    site_run.worker.tile_coord = {2, 2};
    site_run.worker.tile_position_x = 2.0f;
    site_run.worker.tile_position_y = 2.0f;
    site_run.worker.player_health = 100.0f;
    site_run.counters.site_completion_tile_threshold = 3U;
    site_run.counters.fully_grown_tile_count = 0U;

    GameCommandQueue command_queue {};
    auto site_flow_context = make_site_context<SiteFlowSystem>(campaign, site_run, command_queue);
    site_flow_context.move_direction = SiteMoveDirectionInput {1.0f, 0.0f, 0.0f, true};

    SiteFlowSystem::run(site_flow_context);
    assert(site_run.worker.tile_position_x > 2.0f);
    assert(site_run.worker.tile_position_y == 2.0f);
    assert(site_run.worker.facing_degrees == 90.0f);
    assert(site_run.run_status == SiteRunStatus::Active);
    assert(command_queue.empty());
    assert(site_run.clock.day_phase == DayPhase::Dawn);
    assert(campaign.campaign_clock_minutes_elapsed == 0.0);
    assert(campaign.campaign_days_remaining == 30U);

    site_run.counters.fully_grown_tile_count = 3U;
    auto completion_context = make_site_context<SiteCompletionSystem>(campaign, site_run, command_queue);
    SiteCompletionSystem::run(completion_context);
    assert(site_run.run_status == SiteRunStatus::Active);
    assert(command_queue.size() == 1U);
    assert(command_queue.front().type == GameCommandType::SiteAttemptEnded);
    assert(command_queue.front().payload_as<SiteAttemptEndedCommand>().site_id == 7U);
    assert(command_queue.front().payload_as<SiteAttemptEndedCommand>().result == GS1_SITE_ATTEMPT_RESULT_COMPLETED);

    command_queue.clear();
    site_run.counters.fully_grown_tile_count = 0U;
    site_run.worker.player_health = 0.0f;
    auto failure_context = make_site_context<FailureRecoverySystem>(campaign, site_run, command_queue);
    FailureRecoverySystem::run(failure_context);
    assert(site_run.run_status == SiteRunStatus::Active);
    assert(command_queue.size() == 1U);
    assert(command_queue.front().type == GameCommandType::SiteAttemptEnded);
    assert(command_queue.front().payload_as<SiteAttemptEndedCommand>().site_id == 7U);
    assert(command_queue.front().payload_as<SiteAttemptEndedCommand>().result == GS1_SITE_ATTEMPT_RESULT_FAILED);

    return 0;
}
