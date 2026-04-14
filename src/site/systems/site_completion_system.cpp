#include "site/systems/site_completion_system.h"

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

void SiteCompletionSystem::run(SiteSystemContext<SiteCompletionSystem>& context)
{
    const auto& counters = context.world.read_counters();
    if (context.world.run_status() != SiteRunStatus::Active ||
        counters.site_completion_tile_threshold == 0U ||
        counters.fully_grown_tile_count < counters.site_completion_tile_threshold ||
        has_pending_site_transition_command(context.command_queue, context.world.site_id_value()))
    {
        return;
    }

    GameCommand command {};
    command.type = GameCommandType::SiteAttemptEnded;
    command.set_payload(SiteAttemptEndedCommand {
        context.world.site_id_value(),
        GS1_SITE_ATTEMPT_RESULT_COMPLETED});
    context.command_queue.push_back(command);
}
}  // namespace gs1
