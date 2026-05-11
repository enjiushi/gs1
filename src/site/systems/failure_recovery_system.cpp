#include "site/systems/failure_recovery_system.h"

#include "runtime/game_runtime.h"
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

void FailureRecoverySystem::run(SiteSystemContext<FailureRecoverySystem>& context)
{
    if (!context.world.has_world())
    {
        return;
    }

    const auto worker = context.world.read_worker();
    const float worker_health = worker.conditions.health;
    const auto& site_run = context.site_run;
    if (site_run.run_status != SiteRunStatus::Active ||
        worker_health > 0.0f ||
        has_pending_site_transition_message(context.message_queue, site_run.site_id.value))
    {
        return;
    }

    GameMessage message {};
    message.type = GameMessageType::SiteAttemptEnded;
    message.set_payload(SiteAttemptEndedMessage {
        context.world.site_id_value(),
        GS1_SITE_ATTEMPT_RESULT_FAILED});
    context.message_queue.push_back(message);
}
GS1_IMPLEMENT_RUNTIME_SITE_RUN_ONLY_SYSTEM(
    FailureRecoverySystem,
    GS1_RUNTIME_PROFILE_SYSTEM_FAILURE_RECOVERY,
    19U)
}  // namespace gs1
