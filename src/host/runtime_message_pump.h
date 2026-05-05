#pragma once

#include "gs1/types.h"
#include "host/runtime_projection_state.h"
#include "host/runtime_session.h"

#include <cstdint>
#include <string>
#include <vector>

class Gs1RuntimeMessagePump final
{
public:
    [[nodiscard]] bool drain_runtime_messages(
        const Gs1RuntimeSession& session,
        Gs1RuntimeProjectionCache& projection_cache,
        std::vector<Gs1EngineMessage>* drained_messages,
        std::uint32_t& out_message_count,
        std::string& out_error);
};
