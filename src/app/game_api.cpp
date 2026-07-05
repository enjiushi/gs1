#include "gs1/game_api.h"
#include "gs1/system_test_api.h"
#include "shared_framework/runtime/api/runtime_api.h"

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

namespace
{
[[nodiscard]] GsRuntimeHandle* to_gs_runtime_handle(Gs1RuntimeHandle* runtime) noexcept
{
    return reinterpret_cast<GsRuntimeHandle*>(runtime);
}

[[nodiscard]] Gs1RuntimeHandle* to_gs1_runtime_handle(GsRuntimeHandle* runtime) noexcept
{
    return reinterpret_cast<Gs1RuntimeHandle*>(runtime);
}

[[nodiscard]] GsStatus to_gs_status(const Gs1Status status) noexcept
{
    return static_cast<GsStatus>(status);
}
}

extern "C"
{
std::uint32_t gs_runtime_get_api_version() GS_NOEXCEPT
{
    return gs1::k_api_version;
}

const char* gs_runtime_get_build_string() GS_NOEXCEPT
{
    return "gs1_game 0.1.0";
}

GsStatus gs_runtime_create(
    const GsRuntimeCreateDesc* create_desc,
    GsRuntimeHandle** out_runtime) GS_NOEXCEPT
{
    if (create_desc == nullptr || out_runtime == nullptr)
    {
        return GS_STATUS_INVALID_ARGUMENT;
    }

    if (create_desc->struct_size != sizeof(GsRuntimeCreateDesc))
    {
        return GS_STATUS_INVALID_ARGUMENT;
    }

    if (create_desc->api_version != gs1::k_api_version)
    {
        return GS_STATUS_INVALID_ARGUMENT;
    }

    Gs1RuntimeCreateDesc gs1_create_desc {};
    gs1_create_desc.struct_size = sizeof(Gs1RuntimeCreateDesc);
    gs1_create_desc.api_version = create_desc->api_version;
    gs1_create_desc.fixed_step_seconds = create_desc->fixed_step_seconds;
    gs1_create_desc.project_config_root_utf8 = create_desc->project_config_root_utf8;
    gs1_create_desc.adapter_config_json_utf8 = create_desc->adapter_config_json_utf8;

    try
    {
        *out_runtime = to_gs_runtime_handle(new Gs1RuntimeHandle(gs1_create_desc));
        return GS_STATUS_OK;
    }
    catch (const std::bad_alloc&)
    {
        *out_runtime = nullptr;
        return GS_STATUS_OUT_OF_MEMORY;
    }
    catch (...)
    {
        *out_runtime = nullptr;
        return GS_STATUS_INTERNAL_ERROR;
    }
}

void gs_runtime_destroy(GsRuntimeHandle* runtime) GS_NOEXCEPT
{
    delete to_gs1_runtime_handle(runtime);
}

GsStatus gs_runtime_run_phase1(
    GsRuntimeHandle* runtime,
    const GsPhase1Request* request,
    GsPhase1Result* out_result) GS_NOEXCEPT
{
    if (runtime == nullptr || request == nullptr || out_result == nullptr)
    {
        return GS_STATUS_INVALID_ARGUMENT;
    }

    Gs1Phase1Request gs1_request {};
    gs1_request.struct_size = sizeof(Gs1Phase1Request);
    gs1_request.real_delta_seconds = request->real_delta_seconds;

    Gs1Phase1Result gs1_result {};
    const Gs1Status status = to_gs1_runtime_handle(runtime)->runtime.run_phase1(gs1_request, gs1_result);
    out_result->struct_size = gs1_result.struct_size;
    out_result->fixed_steps_executed = gs1_result.fixed_steps_executed;
    out_result->processed_host_message_count = gs1_result.processed_host_message_count;
    return to_gs_status(status);
}

GsStatus gs_runtime_run_phase2(
    GsRuntimeHandle* runtime,
    const GsPhase2Request* request,
    GsPhase2Result* out_result) GS_NOEXCEPT
{
    if (runtime == nullptr || request == nullptr || out_result == nullptr)
    {
        return GS_STATUS_INVALID_ARGUMENT;
    }

    Gs1Phase2Request gs1_request {};
    gs1_request.struct_size = sizeof(Gs1Phase2Request);

    Gs1Phase2Result gs1_result {};
    const Gs1Status status = to_gs1_runtime_handle(runtime)->runtime.run_phase2(gs1_request, gs1_result);
    out_result->struct_size = gs1_result.struct_size;
    out_result->processed_host_message_count = gs1_result.processed_host_message_count;
    out_result->reserved0 = gs1_result.reserved0;
    out_result->reserved1 = gs1_result.reserved1;
    out_result->reserved2 = gs1_result.reserved2;
    return to_gs_status(status);
}

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

Gs1Status gs1_submit_gameplay_action(
    Gs1RuntimeHandle* runtime,
    const Gs1GameplayAction* action) GS1_NOEXCEPT
{
    if (runtime == nullptr || action == nullptr)
    {
        return GS1_STATUS_INVALID_ARGUMENT;
    }

    return runtime->runtime.submit_gameplay_action(*action);
}

Gs1Status gs1_submit_site_move_direction(
    Gs1RuntimeHandle* runtime,
    const Gs1SiteMoveDirectionCommand* command) GS1_NOEXCEPT
{
    if (runtime == nullptr || command == nullptr)
    {
        return GS1_STATUS_INVALID_ARGUMENT;
    }

    return runtime->runtime.submit_site_move_direction(*command);
}

Gs1Status gs1_submit_site_action_request(
    Gs1RuntimeHandle* runtime,
    const Gs1SiteActionRequestCommand* command) GS1_NOEXCEPT
{
    if (runtime == nullptr || command == nullptr)
    {
        return GS1_STATUS_INVALID_ARGUMENT;
    }

    return runtime->runtime.submit_site_action_request(*command);
}

Gs1Status gs1_submit_site_action_cancel(
    Gs1RuntimeHandle* runtime,
    const Gs1SiteActionCancelCommand* command) GS1_NOEXCEPT
{
    if (runtime == nullptr || command == nullptr)
    {
        return GS1_STATUS_INVALID_ARGUMENT;
    }

    return runtime->runtime.submit_site_action_cancel(*command);
}

Gs1Status gs1_submit_site_context_request(
    Gs1RuntimeHandle* runtime,
    const Gs1SiteContextRequestCommand* command) GS1_NOEXCEPT
{
    if (runtime == nullptr || command == nullptr)
    {
        return GS1_STATUS_INVALID_ARGUMENT;
    }

    return runtime->runtime.submit_site_context_request(*command);
}

Gs1Status gs1_submit_site_inventory_slot_tap(
    Gs1RuntimeHandle* runtime,
    const Gs1SiteInventorySlotTapCommand* command) GS1_NOEXCEPT
{
    if (runtime == nullptr || command == nullptr)
    {
        return GS1_STATUS_INVALID_ARGUMENT;
    }

    return runtime->runtime.submit_site_inventory_slot_tap(*command);
}

Gs1Status gs1_submit_site_scene_ready(
    Gs1RuntimeHandle* runtime) GS1_NOEXCEPT
{
    if (runtime == nullptr)
    {
        return GS1_STATUS_INVALID_ARGUMENT;
    }

    return runtime->runtime.submit_site_scene_ready();
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

Gs1Status gs1_get_game_state_view(
    Gs1RuntimeHandle* runtime,
    Gs1GameStateView* out_view) GS1_NOEXCEPT
{
    if (runtime == nullptr || out_view == nullptr)
    {
        return GS1_STATUS_INVALID_ARGUMENT;
    }

    return runtime->runtime.get_game_state_view(*out_view);
}

Gs1Status gs1_query_site_tile_view(
    Gs1RuntimeHandle* runtime,
    std::uint32_t tile_index,
    Gs1SiteTileView* out_tile) GS1_NOEXCEPT
{
    if (runtime == nullptr || out_tile == nullptr)
    {
        return GS1_STATUS_INVALID_ARGUMENT;
    }

    return runtime->runtime.query_site_tile_view(tile_index, *out_tile);
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
