#include "gs1/game_api.h"

#include "runtime/game_runtime.h"

#include <new>

struct Gs1RuntimeHandle
{
    gs1::GameRuntime runtime;

    explicit Gs1RuntimeHandle(Gs1RuntimeCreateDesc create_desc)
        : runtime(create_desc)
    {
    }
};

extern "C"
{
std::uint32_t gs1_get_api_version() GS1_NOEXCEPT
{
    return gs1::k_api_version;
}

const char* gs1_get_build_string() GS1_NOEXCEPT
{
    return "gs1_game 0.1.0";
}

Gs1Status gs1_create_runtime(
    const Gs1RuntimeCreateDesc* create_desc,
    Gs1RuntimeHandle** out_runtime) GS1_NOEXCEPT
{
    if (create_desc == nullptr || out_runtime == nullptr)
    {
        return GS1_STATUS_INVALID_ARGUMENT;
    }

    if (create_desc->struct_size != sizeof(Gs1RuntimeCreateDesc))
    {
        return GS1_STATUS_INVALID_ARGUMENT;
    }

    if (create_desc->api_version != gs1::k_api_version)
    {
        return GS1_STATUS_INVALID_ARGUMENT;
    }

    try
    {
        *out_runtime = new Gs1RuntimeHandle(*create_desc);
        return GS1_STATUS_OK;
    }
    catch (const std::bad_alloc&)
    {
        *out_runtime = nullptr;
        return GS1_STATUS_OUT_OF_MEMORY;
    }
    catch (...)
    {
        *out_runtime = nullptr;
        return GS1_STATUS_INTERNAL_ERROR;
    }
}

void gs1_destroy_runtime(Gs1RuntimeHandle* runtime) GS1_NOEXCEPT
{
    delete runtime;
}

Gs1Status gs1_submit_host_events(
    Gs1RuntimeHandle* runtime,
    const Gs1HostEvent* events,
    std::uint32_t event_count) GS1_NOEXCEPT
{
    if (runtime == nullptr)
    {
        return GS1_STATUS_INVALID_ARGUMENT;
    }

    return runtime->runtime.submit_host_events(events, event_count);
}

Gs1Status gs1_submit_feedback_events(
    Gs1RuntimeHandle* runtime,
    const Gs1FeedbackEvent* events,
    std::uint32_t event_count) GS1_NOEXCEPT
{
    if (runtime == nullptr)
    {
        return GS1_STATUS_INVALID_ARGUMENT;
    }

    return runtime->runtime.submit_feedback_events(events, event_count);
}

Gs1Status gs1_run_phase1(
    Gs1RuntimeHandle* runtime,
    const Gs1Phase1Request* request,
    Gs1Phase1Result* out_result) GS1_NOEXCEPT
{
    if (runtime == nullptr || request == nullptr || out_result == nullptr)
    {
        return GS1_STATUS_INVALID_ARGUMENT;
    }

    return runtime->runtime.run_phase1(*request, *out_result);
}

Gs1Status gs1_run_phase2(
    Gs1RuntimeHandle* runtime,
    const Gs1Phase2Request* request,
    Gs1Phase2Result* out_result) GS1_NOEXCEPT
{
    if (runtime == nullptr || request == nullptr || out_result == nullptr)
    {
        return GS1_STATUS_INVALID_ARGUMENT;
    }

    return runtime->runtime.run_phase2(*request, *out_result);
}

Gs1Status gs1_pop_engine_command(
    Gs1RuntimeHandle* runtime,
    Gs1EngineCommand* out_command) GS1_NOEXCEPT
{
    if (runtime == nullptr || out_command == nullptr)
    {
        return GS1_STATUS_INVALID_ARGUMENT;
    }

    return runtime->runtime.pop_engine_command(*out_command);
}
}
