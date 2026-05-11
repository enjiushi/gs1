#include "gs1/game_api.h"
#include "gs1/system_test_api.h"

#include "runtime/game_runtime.h"
#include "testing/system_test_registry.h"

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

Gs1Status gs1_submit_host_messages(
    Gs1RuntimeHandle* runtime,
    const Gs1HostMessage* messages,
    std::uint32_t message_count) GS1_NOEXCEPT
{
    if (runtime == nullptr)
    {
        return GS1_STATUS_INVALID_ARGUMENT;
    }

    return runtime->runtime.submit_host_messages(messages, message_count);
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

Gs1Status gs1_pop_runtime_message(
    Gs1RuntimeHandle* runtime,
    Gs1RuntimeMessage* out_message) GS1_NOEXCEPT
{
    if (runtime == nullptr || out_message == nullptr)
    {
        return GS1_STATUS_INVALID_ARGUMENT;
    }

    return runtime->runtime.pop_runtime_message(*out_message);
}

Gs1Status gs1_get_runtime_profiling_snapshot(
    Gs1RuntimeHandle* runtime,
    Gs1RuntimeProfilingSnapshot* out_snapshot) GS1_NOEXCEPT
{
    if (runtime == nullptr || out_snapshot == nullptr)
    {
        return GS1_STATUS_INVALID_ARGUMENT;
    }

    return runtime->runtime.get_profiling_snapshot(*out_snapshot);
}

Gs1Status gs1_reset_runtime_profiling(
    Gs1RuntimeHandle* runtime) GS1_NOEXCEPT
{
    if (runtime == nullptr)
    {
        return GS1_STATUS_INVALID_ARGUMENT;
    }

    runtime->runtime.reset_profiling();
    return GS1_STATUS_OK;
}

Gs1Status gs1_set_runtime_profile_system_enabled(
    Gs1RuntimeHandle* runtime,
    Gs1RuntimeProfileSystemId system_id,
    std::uint8_t enabled) GS1_NOEXCEPT
{
    if (runtime == nullptr)
    {
        return GS1_STATUS_INVALID_ARGUMENT;
    }

    return runtime->runtime.set_profiled_system_enabled(system_id, enabled != 0U);
}

std::uint32_t gs1_get_system_test_api_version() GS1_NOEXCEPT
{
    return gs1::testing::k_system_test_api_version;
}

std::uint32_t gs1_get_system_test_case_count() GS1_NOEXCEPT
{
    return static_cast<std::uint32_t>(gs1::testing::registered_source_system_tests().size());
}

Gs1Status gs1_get_system_test_case_info(
    std::uint32_t index,
    Gs1SystemTestCaseInfo* out_info) GS1_NOEXCEPT
{
    if (out_info == nullptr || out_info->struct_size != sizeof(Gs1SystemTestCaseInfo))
    {
        return GS1_STATUS_INVALID_ARGUMENT;
    }

    return gs1::testing::fill_system_test_case_info(index, *out_info);
}

Gs1Status gs1_run_system_test_case(
    std::uint32_t index,
    Gs1SystemTestRunResult* out_result) GS1_NOEXCEPT
{
    if (out_result == nullptr || out_result->struct_size != sizeof(Gs1SystemTestRunResult))
    {
        return GS1_STATUS_INVALID_ARGUMENT;
    }

    return gs1::testing::run_registered_source_system_test_case(index, *out_result);
}

Gs1Status gs1_run_system_test_asset_file(
    const char* asset_path_utf8,
    Gs1SystemTestRunResult* out_result) GS1_NOEXCEPT
{
    if (out_result == nullptr || out_result->struct_size != sizeof(Gs1SystemTestRunResult))
    {
        return GS1_STATUS_INVALID_ARGUMENT;
    }

    return gs1::testing::run_system_test_asset_file(asset_path_utf8, *out_result);
}
}
