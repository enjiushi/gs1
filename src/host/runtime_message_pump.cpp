#include "host/runtime_message_pump.h"

bool Gs1RuntimeMessagePump::drain_runtime_messages(
    const Gs1RuntimeSession& session,
    Gs1RuntimeProjectionCache& projection_cache,
    std::vector<Gs1EngineMessage>* drained_messages,
    std::uint32_t& out_message_count,
    std::string& out_error)
{
    out_message_count = 0;
    out_error.clear();
    if (drained_messages != nullptr)
    {
        drained_messages->clear();
    }

    const Gs1RuntimeApi* api = session.api();
    if (api == nullptr || api->pop_engine_message == nullptr || session.runtime() == nullptr)
    {
        out_error = "Runtime session is not ready to drain engine messages.";
        return false;
    }

    Gs1EngineMessage message {};
    while (true)
    {
        const Gs1Status status = api->pop_engine_message(session.runtime(), &message);
        if (status == GS1_STATUS_BUFFER_EMPTY)
        {
            return true;
        }

        if (status != GS1_STATUS_OK)
        {
            out_error = "gs1_pop_engine_message failed with status " + std::to_string(static_cast<unsigned>(status));
            return false;
        }

        projection_cache.apply_engine_message(message);
        if (drained_messages != nullptr)
        {
            drained_messages->push_back(message);
        }
        out_message_count += 1U;
    }
}
