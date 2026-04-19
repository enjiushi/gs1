#include "site/systems/site_completion_system.h"

#include "site/site_run_state.h"

namespace gs1
{
namespace
{
bool has_pending_site_transition_message(
    const GameMessageQueue& message_queue,
    std::uint32_t site_id)
{
    for (const auto& message : message_queue)
    {
        if (message.type == GameMessageType::SiteAttemptEnded &&
            message.payload_as<SiteAttemptEndedMessage>().site_id == site_id)
        {
            return true;
        }
    }

    return false;
}

void enqueue_site_attempt_result(
    GameMessageQueue& message_queue,
    std::uint32_t site_id,
    Gs1SiteAttemptResult result)
{
    GameMessage message {};
    message.type = GameMessageType::SiteAttemptEnded;
    message.set_payload(SiteAttemptEndedMessage {site_id, result});
    message_queue.push_back(message);
}
}  // namespace

void SiteCompletionSystem::run(SiteSystemContext<SiteCompletionSystem>& context)
{
    const auto& counters = context.world.read_counters();
    const auto& objective = context.world.read_objective();
    const auto& clock = context.world.read_time();
    if (context.world.run_status() != SiteRunStatus::Active ||
        has_pending_site_transition_message(context.message_queue, context.world.site_id_value()))
    {
        return;
    }

    switch (objective.type)
    {
    case SiteObjectiveType::DenseRestoration:
        if (counters.site_completion_tile_threshold == 0U ||
            counters.fully_grown_tile_count < counters.site_completion_tile_threshold)
        {
            return;
        }

        enqueue_site_attempt_result(
            context.message_queue,
            context.world.site_id_value(),
            GS1_SITE_ATTEMPT_RESULT_COMPLETED);
        return;

    case SiteObjectiveType::HighwayProtection:
        if (objective.highway_max_average_sand_cover <= 0.0f ||
            objective.time_limit_minutes <= 0.0 ||
            objective.target_tile_indices.empty())
        {
            return;
        }

        if (counters.highway_average_sand_cover >= objective.highway_max_average_sand_cover)
        {
            enqueue_site_attempt_result(
                context.message_queue,
                context.world.site_id_value(),
                GS1_SITE_ATTEMPT_RESULT_FAILED);
            return;
        }

        if (clock.world_time_minutes < objective.time_limit_minutes)
        {
            return;
        }

        enqueue_site_attempt_result(
            context.message_queue,
            context.world.site_id_value(),
            GS1_SITE_ATTEMPT_RESULT_COMPLETED);
        return;

    case SiteObjectiveType::GreenWallConnection:
    case SiteObjectiveType::PureSurvival:
    default:
        return;
    }
}
}  // namespace gs1
