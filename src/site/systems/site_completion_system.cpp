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
}  // namespace

void SiteCompletionSystem::run(SiteSystemContext<SiteCompletionSystem>& context)
{
    const auto& counters = context.world.read_counters();
    if (context.world.run_status() != SiteRunStatus::Active ||
        counters.site_completion_tile_threshold == 0U ||
        counters.fully_grown_tile_count < counters.site_completion_tile_threshold ||
        has_pending_site_transition_message(context.message_queue, context.world.site_id_value()))
    {
        return;
    }

    GameMessage message {};
    message.type = GameMessageType::SiteAttemptEnded;
    message.set_payload(SiteAttemptEndedMessage {
        context.world.site_id_value(),
        GS1_SITE_ATTEMPT_RESULT_COMPLETED});
    context.message_queue.push_back(message);
}
}  // namespace gs1
