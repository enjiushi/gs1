#include "app/campaign_factory.h"
#include "campaign/campaign_state.h"
#include "campaign/systems/campaign_system_context.h"
#include "campaign/systems/campaign_flow_system.h"
#include "campaign/systems/campaign_time_system.h"
#include "campaign/systems/loadout_planner_system.h"
#include "messages/game_message.h"
#include "site/site_run_state.h"
#include "site/site_world.h"
#include "site/site_world_access.h"
#include "site/systems/failure_recovery_system.h"
#include "site/systems/site_completion_system.h"
#include "site/systems/site_flow_system.h"
#include "site/systems/site_time_system.h"
#include "site/systems/site_system_context.h"

#include <cassert>

namespace
{
using gs1::CampaignState;
using gs1::CampaignFixedStepContext;
using gs1::CampaignFlowMessageContext;
using gs1::CampaignSystemContext;
using gs1::CampaignFactory;
using gs1::CampaignFlowSystem;
using gs1::CampaignTimeSystem;
using gs1::DayPhase;
using gs1::DeploymentSiteSelectionChangedMessage;
using gs1::FailureRecoverySystem;
using gs1::GameMessageQueue;
using gs1::GameMessageType;
using gs1::LoadoutPlannerSystem;
using gs1::SiteCompletionSystem;
using gs1::SiteAttemptEndedMessage;
using gs1::SiteRunStartedMessage;
using gs1::SiteFlowSystem;
using gs1::SiteId;
using gs1::SiteRunState;
using gs1::SiteRunStatus;
using gs1::SiteSystemContext;
using gs1::SiteMoveDirectionInput;
using gs1::StartSiteAttemptMessage;
using gs1::SiteTimeSystem;
using gs1::SiteWorld;
using gs1::TileCoord;

template <typename SiteSystemTag>
SiteSystemContext<SiteSystemTag> make_site_context(
    CampaignState& campaign,
    SiteRunState& site_run,
    GameMessageQueue& message_queue)
{
    return gs1::make_site_system_context<SiteSystemTag>(
        campaign,
        site_run,
        message_queue,
        0.25,
        SiteMoveDirectionInput {});
}

void initialize_site_world(
    SiteRunState& site_run,
    std::uint32_t width,
    std::uint32_t height,
    TileCoord worker_tile_coord,
    float worker_tile_x,
    float worker_tile_y,
    float worker_health)
{
    site_run.site_world = std::make_shared<SiteWorld>();
    site_run.site_world->initialize(
        SiteWorld::CreateDesc {
            width,
            height,
            worker_tile_coord,
            worker_tile_x,
            worker_tile_y,
            0.0f,
            worker_health,
            100.0f,
            100.0f,
            100.0f,
            100.0f,
            100.0f,
            1.0f,
            false});
}
}  // namespace

int main()
{
    CampaignState campaign = CampaignFactory::create_prototype_campaign(42ULL, 30U);

    CampaignSystemContext campaign_context {campaign};
    gs1::GameMessage selection_changed {};
    selection_changed.type = GameMessageType::DeploymentSiteSelectionChanged;
    selection_changed.set_payload(DeploymentSiteSelectionChangedMessage {3U});
    assert(LoadoutPlannerSystem::subscribes_to(selection_changed.type));
    assert(LoadoutPlannerSystem::process_message(campaign_context, selection_changed) == GS1_STATUS_OK);
    assert(campaign.loadout_planner_state.selected_target_site_id.has_value());
    assert(campaign.loadout_planner_state.selected_target_site_id->value == 3U);

    selection_changed.set_payload(DeploymentSiteSelectionChangedMessage {0U});
    assert(LoadoutPlannerSystem::process_message(campaign_context, selection_changed) == GS1_STATUS_OK);
    assert(!campaign.loadout_planner_state.selected_target_site_id.has_value());

    CampaignFixedStepContext campaign_step_context {campaign};
    CampaignTimeSystem::run(campaign_step_context);
    assert(campaign.campaign_clock_minutes_elapsed > 0.0);
    assert(campaign.campaign_days_remaining == 30U);
    campaign.campaign_clock_minutes_elapsed = 0.0;

    std::optional<CampaignState> active_campaign {
        CampaignFactory::create_prototype_campaign(42ULL, 30U)};
    std::optional<SiteRunState> active_site_run {};
    Gs1AppState app_state = GS1_APP_STATE_REGIONAL_MAP;
    GameMessageQueue site_start_queue {};
    CampaignFlowMessageContext flow_context {
        active_campaign,
        active_site_run,
        app_state,
        site_start_queue};
    gs1::GameMessage start_site_attempt {};
    start_site_attempt.type = GameMessageType::StartSiteAttempt;
    start_site_attempt.set_payload(StartSiteAttemptMessage {1U});
    assert(CampaignFlowSystem::process_message(flow_context, start_site_attempt) == GS1_STATUS_OK);
    assert(active_site_run.has_value());
    assert(app_state == GS1_APP_STATE_SITE_ACTIVE);
    assert(site_start_queue.size() == 1U);
    assert(site_start_queue.front().type == GameMessageType::SiteRunStarted);
    assert(site_start_queue.front().payload_as<SiteRunStartedMessage>().site_id == 1U);
    assert(site_start_queue.front().payload_as<SiteRunStartedMessage>().site_run_id == 1U);
    assert(site_start_queue.front().payload_as<SiteRunStartedMessage>().site_archetype_id == 101U);
    assert(site_start_queue.front().payload_as<SiteRunStartedMessage>().attempt_index == 1U);

    SiteRunState site_run {};
    site_run.site_id = SiteId {7U};
    site_run.run_status = SiteRunStatus::Active;
    initialize_site_world(site_run, 8U, 8U, {2, 2}, 2.0f, 2.0f, 100.0f);
    site_run.counters.site_completion_tile_threshold = 3U;
    site_run.counters.fully_grown_tile_count = 0U;

    GameMessageQueue message_queue {};
    auto site_time_context = make_site_context<SiteTimeSystem>(campaign, site_run, message_queue);
    auto site_flow_context = make_site_context<SiteFlowSystem>(campaign, site_run, message_queue);
    site_flow_context.move_direction = SiteMoveDirectionInput {1.0f, 0.0f, 0.0f, true};

    SiteTimeSystem::run(site_time_context);
    SiteFlowSystem::run(site_flow_context);
    const auto worker_position = gs1::site_world_access::worker_position(site_run);
    assert(worker_position.tile_x > 2.0f);
    assert(worker_position.tile_y == 2.0f);
    assert(worker_position.facing_degrees == 90.0f);
    assert(site_run.run_status == SiteRunStatus::Active);
    assert(message_queue.empty());
    assert(site_run.clock.day_phase == DayPhase::Dawn);
    assert(campaign.campaign_clock_minutes_elapsed == 0.0);
    assert(campaign.campaign_days_remaining == 30U);

    site_run.counters.fully_grown_tile_count = 3U;
    auto completion_context = make_site_context<SiteCompletionSystem>(campaign, site_run, message_queue);
    SiteCompletionSystem::run(completion_context);
    assert(site_run.run_status == SiteRunStatus::Active);
    assert(message_queue.size() == 1U);
    assert(message_queue.front().type == GameMessageType::SiteAttemptEnded);
    assert(message_queue.front().payload_as<SiteAttemptEndedMessage>().site_id == 7U);
    assert(message_queue.front().payload_as<SiteAttemptEndedMessage>().result == GS1_SITE_ATTEMPT_RESULT_COMPLETED);

    message_queue.clear();
    site_run.counters.fully_grown_tile_count = 0U;
    auto failure_context = make_site_context<FailureRecoverySystem>(campaign, site_run, message_queue);
    auto worker_conditions = gs1::site_world_access::worker_conditions(site_run);
    worker_conditions.health = 0.0f;
    gs1::site_world_access::set_worker_conditions(site_run, worker_conditions);
    FailureRecoverySystem::run(failure_context);
    assert(site_run.run_status == SiteRunStatus::Active);
    assert(message_queue.size() == 1U);
    assert(message_queue.front().type == GameMessageType::SiteAttemptEnded);
    assert(message_queue.front().payload_as<SiteAttemptEndedMessage>().site_id == 7U);
    assert(message_queue.front().payload_as<SiteAttemptEndedMessage>().result == GS1_SITE_ATTEMPT_RESULT_FAILED);

    return 0;
}
