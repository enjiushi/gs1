#pragma once

#include <cstdint>

#if defined(_WIN32) || defined(__CYGWIN__)
#  if defined(GS_BUILD_DLL)
#    define GS_API __declspec(dllexport)
#  elif defined(GS_USE_DLL)
#    define GS_API __declspec(dllimport)
#  else
#    define GS_API
#  endif
#else
#  define GS_API __attribute__((visibility("default")))
#endif

#define GS_NOEXCEPT noexcept

struct GsRuntimeHandle;

enum GsStatus : std::uint32_t
{
    GS_STATUS_OK = 0,
    GS_STATUS_INVALID_ARGUMENT = 1,
    GS_STATUS_OUT_OF_MEMORY = 2,
    GS_STATUS_INVALID_STATE = 3,
    GS_STATUS_NOT_FOUND = 4,
    GS_STATUS_BUFFER_EMPTY = 5,
    GS_STATUS_NOT_IMPLEMENTED = 6,
    GS_STATUS_INTERNAL_ERROR = 7
};

struct GsRuntimeCreateDesc
{
    std::uint32_t struct_size;
    std::uint32_t api_version;
    double fixed_step_seconds;
    const char* project_config_root_utf8;
    const char* adapter_config_json_utf8;
};

struct GsPhase1Request
{
    std::uint32_t struct_size;
    double real_delta_seconds;
};

struct GsPhase1Result
{
    std::uint32_t struct_size;
    std::uint32_t fixed_steps_executed;
    std::uint32_t processed_host_message_count;
};

struct GsPhase2Request
{
    std::uint32_t struct_size;
};

struct GsPhase2Result
{
    std::uint32_t struct_size;
    std::uint32_t processed_host_message_count;
    std::uint32_t reserved0;
    std::uint32_t reserved1;
    std::uint32_t reserved2;
};

extern "C"
{
GS_API std::uint32_t gs_runtime_get_api_version() GS_NOEXCEPT;
GS_API const char* gs_runtime_get_build_string() GS_NOEXCEPT;

GS_API GsStatus gs_runtime_create(
    const GsRuntimeCreateDesc* create_desc,
    GsRuntimeHandle** out_runtime) GS_NOEXCEPT;

GS_API void gs_runtime_destroy(
    GsRuntimeHandle* runtime) GS_NOEXCEPT;

GS_API GsStatus gs_runtime_run_phase1(
    GsRuntimeHandle* runtime,
    const GsPhase1Request* request,
    GsPhase1Result* out_result) GS_NOEXCEPT;

GS_API GsStatus gs_runtime_run_phase2(
    GsRuntimeHandle* runtime,
    const GsPhase2Request* request,
    GsPhase2Result* out_result) GS_NOEXCEPT;
}
