#pragma once

#include "gs1/export.h"
#include "gs1/status.h"

#include <cstddef>
#include <cstdint>
#include <type_traits>

enum Gs1SystemTestCaseSourceKind : std::uint8_t
{
    GS1_SYSTEM_TEST_CASE_SOURCE_NONE = 0,
    GS1_SYSTEM_TEST_CASE_SOURCE_CODE = 1,
    GS1_SYSTEM_TEST_CASE_SOURCE_ASSET_FILE = 2
};

inline constexpr std::size_t GS1_SYSTEM_TEST_MESSAGE_BYTE_COUNT = 512U;

struct Gs1SystemTestCaseInfo
{
    std::uint32_t struct_size;
    const char* system_name;
    const char* test_name;
    const char* source_path;
    std::uint32_t source_line;
    Gs1SystemTestCaseSourceKind source_kind;
    std::uint8_t reserved0[3];
};

struct Gs1SystemTestRunResult
{
    std::uint32_t struct_size;
    std::uint32_t assertion_count;
    std::uint32_t failure_count;
    double elapsed_milliseconds;
    char message[GS1_SYSTEM_TEST_MESSAGE_BYTE_COUNT];
};

static_assert(std::is_standard_layout_v<Gs1SystemTestCaseInfo>);
static_assert(std::is_trivially_copyable_v<Gs1SystemTestCaseInfo>);
static_assert(std::is_standard_layout_v<Gs1SystemTestRunResult>);
static_assert(std::is_trivially_copyable_v<Gs1SystemTestRunResult>);

extern "C"
{
GS1_API std::uint32_t gs1_get_system_test_api_version() GS1_NOEXCEPT;

GS1_API std::uint32_t gs1_get_system_test_case_count() GS1_NOEXCEPT;

GS1_API Gs1Status gs1_get_system_test_case_info(
    std::uint32_t index,
    Gs1SystemTestCaseInfo* out_info) GS1_NOEXCEPT;

GS1_API Gs1Status gs1_run_system_test_case(
    std::uint32_t index,
    Gs1SystemTestRunResult* out_result) GS1_NOEXCEPT;

GS1_API Gs1Status gs1_run_system_test_asset_file(
    const char* asset_path_utf8,
    Gs1SystemTestRunResult* out_result) GS1_NOEXCEPT;
}
