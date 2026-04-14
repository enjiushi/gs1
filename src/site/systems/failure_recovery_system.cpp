#include "site/systems/failure_recovery_system.h"

#include "site/site_run_state.h"

namespace gs1
{
namespace
{
bool has_pending_site_transition_command(
    const GameCommandQueue& command_queue,
    std::uint32_t site_id)
{
    for (const auto& command : command_queue)
    {
        if (command.type == GameCommandType::SiteAttemptEnded &&
            command.payload_as<SiteAttemptEndedCommand>().site_id == site_id)
        {
            return true;
        }
    }

    return false;
}
}  // namespace

void FailureRecoverySystem::run(SiteSystemContext<FailureRecoverySystem>& context)
{
    const auto& worker = context.world.read_worker_needs();
    if (context.world.run_status() != SiteRunStatus::Active ||
        worker.player_health > 0.0f ||
        has_pending_site_transition_command(context.command_queue, context.world.site_id_value()))
    {
        return;
    }

    GameCommand command {};
    command.type = GameCommandType::SiteAttemptEnded;
    command.set_payload(SiteAttemptEndedCommand {
        context.world.site_id_value(),
        GS1_SITE_ATTEMPT_RESULT_FAILED});
    context.command_queue.push_back(command);
}
}  // namespace gs1
