#include "commands/command_dispatcher.h"

#include "runtime/game_runtime.h"

namespace gs1
{
Gs1Status CommandDispatcher::dispatch_all(GameRuntime& runtime)
{
    while (!runtime.command_queue().empty())
    {
        const auto command = runtime.command_queue().front();
        runtime.command_queue().pop_front();

        const auto status = runtime.dispatch_subscribed_command(command);
        if (status != GS1_STATUS_OK)
        {
            return status;
        }

        runtime.sync_after_processed_command(command);
    }

    return GS1_STATUS_OK;
}
}  // namespace gs1
