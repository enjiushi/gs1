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

void FailureRecoverySystem::run(SiteSystemContext& context)
{
    const auto& site_run = context.site_run;
    if (site_run.run_status != SiteRunStatus::Active ||
        site_run.worker.player_health > 0.0f ||
        has_pending_site_transition_command(context.command_queue, site_run.site_id.value))
    {
        return;
    }

    GameCommand command {};
    command.type = GameCommandType::SiteAttemptEnded;
    command.set_payload(SiteAttemptEndedCommand {
        site_run.site_id.value,
        GS1_SITE_ATTEMPT_RESULT_FAILED});
    context.command_queue.push_back(command);
}
}  // namespace gs1
