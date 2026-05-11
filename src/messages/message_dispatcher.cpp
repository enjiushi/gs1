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

        GamePresentationRuntimeContext context {
            runtime.app_state_,
            runtime.campaign_,
            runtime.active_site_run_,
            runtime.message_queue_,
            runtime.runtime_messages_,
            runtime.fixed_step_seconds_};
        runtime.presentation_.on_message_processed(context, message);
    }

    return GS1_STATUS_OK;
}
}  // namespace gs1
