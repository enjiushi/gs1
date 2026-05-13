#pragma once

#include "gs1/export.h"
#include "gs1/state_view.h"
#include "gs1/status.h"
#include "gs1/types.h"

#include <cstdint>

extern "C"
{
GS1_API std::uint32_t gs1_get_api_version() GS1_NOEXCEPT;
GS1_API const char* gs1_get_build_string() GS1_NOEXCEPT;

GS1_API Gs1Status gs1_create_runtime(
    const Gs1RuntimeCreateDesc* create_desc,
    Gs1RuntimeHandle** out_runtime) GS1_NOEXCEPT;

GS1_API void gs1_destroy_runtime(
    Gs1RuntimeHandle* runtime) GS1_NOEXCEPT;

GS1_API Gs1Status gs1_submit_host_messages(
    Gs1RuntimeHandle* runtime,
    const Gs1HostMessage* messages,
    std::uint32_t message_count) GS1_NOEXCEPT;

GS1_API Gs1Status gs1_run_phase1(
    Gs1RuntimeHandle* runtime,
    const Gs1Phase1Request* request,
    Gs1Phase1Result* out_result) GS1_NOEXCEPT;

GS1_API Gs1Status gs1_run_phase2(
    Gs1RuntimeHandle* runtime,
    const Gs1Phase2Request* request,
    Gs1Phase2Result* out_result) GS1_NOEXCEPT;

GS1_API Gs1Status gs1_pop_runtime_message(
    Gs1RuntimeHandle* runtime,
    Gs1RuntimeMessage* out_message) GS1_NOEXCEPT;

GS1_API Gs1Status gs1_get_game_state_view(
    Gs1RuntimeHandle* runtime,
    Gs1GameStateView* out_view) GS1_NOEXCEPT;

GS1_API Gs1Status gs1_query_site_tile_view(
    Gs1RuntimeHandle* runtime,
    std::uint32_t tile_index,
    Gs1SiteTileView* out_tile) GS1_NOEXCEPT;

GS1_API Gs1Status gs1_get_runtime_profiling_snapshot(
    Gs1RuntimeHandle* runtime,
    Gs1RuntimeProfilingSnapshot* out_snapshot) GS1_NOEXCEPT;

GS1_API Gs1Status gs1_reset_runtime_profiling(
    Gs1RuntimeHandle* runtime) GS1_NOEXCEPT;

GS1_API Gs1Status gs1_set_runtime_profile_system_enabled(
    Gs1RuntimeHandle* runtime,
    Gs1RuntimeProfileSystemId system_id,
    std::uint8_t enabled) GS1_NOEXCEPT;
}
