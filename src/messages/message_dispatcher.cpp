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
            runtime.state_.app_state,
            runtime.state_.campaign,
            runtime.state_.active_site_run,
            runtime.state_.site_protection_presentation,
            runtime.state_.message_queue,
            runtime.state_.runtime_messages,
            runtime.state_.fixed_step_seconds};
        runtime.presentation_.on_message_processed(context, message);
    }

    return GS1_STATUS_OK;
}
}  // namespace gs1
