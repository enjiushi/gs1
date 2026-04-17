#include "messages/message_dispatcher.h"

#include "runtime/game_runtime.h"

namespace gs1
{
Gs1Status MessageDispatcher::dispatch_all(GameRuntime& runtime)
{
    while (!runtime.message_queue().empty())
    {
        const auto message = runtime.message_queue().front();
        runtime.message_queue().pop_front();

        const auto status = runtime.dispatch_subscribed_message(message);
        if (status != GS1_STATUS_OK)
        {
            return status;
        }

        runtime.sync_after_processed_message(message);
    }

    return GS1_STATUS_OK;
}
}  // namespace gs1
