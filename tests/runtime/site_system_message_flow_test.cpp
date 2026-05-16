#include "app/campaign_factory.h"
#include "campaign/campaign_state.h"
#include "campaign/systems/campaign_flow_system.h"
#include "campaign/systems/campaign_time_system.h"
#include "campaign/systems/loadout_planner_system.h"
#include "messages/game_message.h"
#include "runtime/game_state.h"
#include "runtime/runtime_split_state_compat.h"
#include "runtime/state_manager.h"
#include "runtime/system_interface.h"
#include "site/site_run_state.h"
#include "site/site_world.h"
#include "site/site_world_access.h"
#include "site/systems/failure_recovery_system.h"
#include "site/systems/site_completion_system.h"
#include "site/systems/site_flow_system.h"
#include "site/systems/site_time_system.h"

#include <cassert>
#include <deque>
#include <optional>

namespace
{
using gs1::CampaignState;
using gs1::CampaignFactory;
using gs1::CampaignFlowSystem;
using gs1::CampaignTimeSystem;
using gs1::DayPhase;
using gs1::DeploymentSiteSelectionChangedMessage;
using gs1::FailureRecoverySystem;
using gs1::GameMessageQueue;
using gs1::GameMessageType;
using gs1::LoadoutPlannerSystem;
using gs1::RuntimeInvocation;
using gs1::SiteCompletionSystem;
using gs1::SiteAttemptEndedMessage;
using gs1::SiteRunStartedMessage;
using gs1::SiteFlowSystem;
using gs1::SiteId;
using gs1::SiteRunState;
using gs1::SiteRunStatus;
using gs1::SiteMoveDirectionInput;
using gs1::StartSiteAttemptMessage;
using gs1::SiteTimeSystem;
using gs1::SiteWorld;
using gs1::TileCoord;

template <typename System>
Gs1Status invoke_system_message(
    const gs1::GameMessage& message,
    Gs1AppState& app_state,
    std::optional<CampaignState>& campaign,
    std::optional<SiteRunState>& active_site_run,
    GameMessageQueue& message_queue,
    std::deque<Gs1RuntimeMessage>& runtime_messages,
    double fixed_step_seconds = 0.25,
    SiteMoveDirectionInput move_direction = {})
{
    gs1::GameState state {};
    gs1::StateManager state_manager {};
    state.app_state = app_state;
    state.fixed_step_seconds = fixed_step_seconds;
    state.runtime_messages = runtime_messages;
    state.message_queue = message_queue;
    state.move_direction = gs1::RuntimeMoveDirectionSnapshot {
        move_direction.world_move_x,
        move_direction.world_move_y,
        move_direction.world_move_z,
        move_direction.present};
    gs1::write_campaign_state_to_state_sets(campaign, state, state_manager);
    gs1::write_site_run_state_to_state_sets(active_site_run, state, state_manager);
    RuntimeInvocation invocation {
        state,
        state_manager,
        move_direction.world_move_x,
        move_direction.world_move_y,
        move_direction.world_move_z,
        move_direction.present,
        active_site_run.has_value() ? active_site_run->site_world : gs1::SiteWorldHandle {}};
    System system {};
    const auto status = system.process_game_message(invocation, message);
    app_state = state.app_state.get();
    runtime_messages = state.runtime_messages;
    message_queue = state.message_queue;
    campaign = state.campaign_core.has_value()
        ? std::optional<CampaignState> {gs1::assemble_campaign_state_from_state_sets(state, state_manager)}
        : std::nullopt;
    active_site_run = state.site_run_meta.has_value()
        ? std::optional<SiteRunState> {gs1::assemble_site_run_state_from_state_sets(
              state,
              state_manager,
              invocation.site_world_handle())}
        : std::nullopt;
    return status;
}

template <typename System>
void run_system(
    Gs1AppState& app_state,
    std::optional<CampaignState>& campaign,
    std::optional<SiteRunState>& active_site_run,
    GameMessageQueue& message_queue,
    std::deque<Gs1RuntimeMessage>& runtime_messages,
    double fixed_step_seconds = 0.25,
    SiteMoveDirectionInput move_direction = {})
{
    gs1::GameState state {};
    gs1::StateManager state_manager {};
    state.app_state = app_state;
    state.fixed_step_seconds = fixed_step_seconds;
    state.runtime_messages = runtime_messages;
    state.message_queue = message_queue;
    state.move_direction = gs1::RuntimeMoveDirectionSnapshot {
        move_direction.world_move_x,
        move_direction.world_move_y,
        move_direction.world_move_z,
        move_direction.present};
    gs1::write_campaign_state_to_state_sets(campaign, state, state_manager);
    gs1::write_site_run_state_to_state_sets(active_site_run, state, state_manager);
    RuntimeInvocation invocation {
        state,
        state_manager,
        move_direction.world_move_x,
        move_direction.world_move_y,
        move_direction.world_move_z,
        move_direction.present,
        active_site_run.has_value() ? active_site_run->site_world : gs1::SiteWorldHandle {}};
    System system {};
    system.run(invocation);
    app_state = state.app_state.get();
    runtime_messages = state.runtime_messages;
    message_queue = state.message_queue;
    campaign = state.campaign_core.has_value()
        ? std::optional<CampaignState> {gs1::assemble_campaign_state_from_state_sets(state, state_manager)}
        : std::nullopt;
    active_site_run = state.site_run_meta.has_value()
        ? std::optional<SiteRunState> {gs1::assemble_site_run_state_from_state_sets(
              state,
              state_manager,
              invocation.site_world_handle())}
        : std::nullopt;
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

template <typename System>
bool system_subscribes_to_message(GameMessageType type)
{
    System system {};
    return gs1::runtime_subscription_contains(system.subscribed_game_messages(), type);
}
}  // namespace

int main()
{
    CampaignState campaign = CampaignFactory::create_prototype_campaign(42ULL, 30U);
    std::optional<CampaignState> campaign_state {campaign};
    std::optional<SiteRunState> no_site_run {};
    Gs1AppState app_state = GS1_APP_STATE_REGIONAL_MAP;
    GameMessageQueue runtime_queue {};
    std::deque<Gs1RuntimeMessage> runtime_messages {};

    gs1::GameMessage selection_changed {};
    selection_changed.type = GameMessageType::DeploymentSiteSelectionChanged;
    selection_changed.set_payload(DeploymentSiteSelectionChangedMessage {3U});
    assert(system_subscribes_to_message<LoadoutPlannerSystem>(selection_changed.type));
    assert(
        invoke_system_message<LoadoutPlannerSystem>(
            selection_changed,
            app_state,
            campaign_state,
            no_site_run,
            runtime_queue,
            runtime_messages) == GS1_STATUS_OK);
    assert(campaign_state->loadout_planner_state.selected_target_site_id.has_value());
    assert(campaign_state->loadout_planner_state.selected_target_site_id->value == 3U);

    selection_changed.set_payload(DeploymentSiteSelectionChangedMessage {0U});
    assert(
        invoke_system_message<LoadoutPlannerSystem>(
            selection_changed,
            app_state,
            campaign_state,
            no_site_run,
            runtime_queue,
            runtime_messages) == GS1_STATUS_OK);
    assert(!campaign_state->loadout_planner_state.selected_target_site_id.has_value());

    run_system<CampaignTimeSystem>(
        app_state,
        campaign_state,
        no_site_run,
        runtime_queue,
        runtime_messages);
    assert(campaign_state->campaign_clock_minutes_elapsed > 0.0);
    assert(campaign_state->campaign_days_remaining == 30U);
    campaign_state->campaign_clock_minutes_elapsed = 0.0;

    std::optional<CampaignState> active_campaign {
        CampaignFactory::create_prototype_campaign(42ULL, 30U)};
    std::optional<SiteRunState> active_site_run {};
    GameMessageQueue site_start_queue {};
    gs1::GameMessage start_site_attempt {};
    start_site_attempt.type = GameMessageType::StartSiteAttempt;
    start_site_attempt.set_payload(StartSiteAttemptMessage {1U});
    assert(
        invoke_system_message<CampaignFlowSystem>(
            start_site_attempt,
            app_state,
            active_campaign,
            active_site_run,
            site_start_queue,
            runtime_messages) == GS1_STATUS_OK);
    assert(active_site_run.has_value());
    assert(active_site_run->site_world != nullptr);
    assert(active_site_run->site_world->initialized());
    assert(app_state == GS1_APP_STATE_SITE_LOADING);
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
    std::optional<CampaignState> site_campaign {campaign};
    std::optional<SiteRunState> runtime_site_run {std::move(site_run)};
    run_system<SiteTimeSystem>(
        app_state,
        site_campaign,
        runtime_site_run,
        message_queue,
        runtime_messages);
    run_system<SiteFlowSystem>(
        app_state,
        site_campaign,
        runtime_site_run,
        message_queue,
        runtime_messages,
        0.25,
        SiteMoveDirectionInput {1.0f, 0.0f, 0.0f, true});
    auto& site_run_ref = runtime_site_run.value();
    const auto worker_position = gs1::site_world_access::worker_position(site_run_ref);
    assert(worker_position.tile_x > 2.0f);
    assert(worker_position.tile_y == 2.0f);
    assert(worker_position.facing_degrees == 90.0f);
    assert(site_run_ref.run_status == SiteRunStatus::Active);
    assert(message_queue.empty());
    assert(site_run_ref.clock.day_phase == DayPhase::Dawn);
    assert(site_campaign->campaign_clock_minutes_elapsed == 0.0);
    assert(site_campaign->campaign_days_remaining == 30U);

    site_run_ref.counters.fully_grown_tile_count = 3U;
    run_system<SiteCompletionSystem>(
        app_state,
        site_campaign,
        runtime_site_run,
        message_queue,
        runtime_messages);
    assert(site_run_ref.run_status == SiteRunStatus::Active);
    assert(message_queue.size() == 1U);
    assert(message_queue.front().type == GameMessageType::SiteAttemptEnded);
    assert(message_queue.front().payload_as<SiteAttemptEndedMessage>().site_id == 7U);
    assert(message_queue.front().payload_as<SiteAttemptEndedMessage>().result == GS1_SITE_ATTEMPT_RESULT_COMPLETED);

    message_queue.clear();
    site_run_ref.counters.fully_grown_tile_count = 0U;
    auto worker_conditions = gs1::site_world_access::worker_conditions(site_run_ref);
    worker_conditions.health = 0.0f;
    gs1::site_world_access::set_worker_conditions(site_run_ref, worker_conditions);
    run_system<FailureRecoverySystem>(
        app_state,
        site_campaign,
        runtime_site_run,
        message_queue,
        runtime_messages);
    assert(site_run_ref.run_status == SiteRunStatus::Active);
    assert(message_queue.size() == 1U);
    assert(message_queue.front().type == GameMessageType::SiteAttemptEnded);
    assert(message_queue.front().payload_as<SiteAttemptEndedMessage>().site_id == 7U);
    assert(message_queue.front().payload_as<SiteAttemptEndedMessage>().result == GS1_SITE_ATTEMPT_RESULT_FAILED);

    return 0;
}
