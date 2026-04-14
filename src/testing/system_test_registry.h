#pragma once

#include "gs1/status.h"
#include "gs1/system_test_api.h"
#include "testing/system_test_asset.h"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace gs1::testing
{
inline constexpr std::uint32_t k_system_test_api_version = 1U;

class SystemTestExecutionContext final
{
public:
    SystemTestExecutionContext(std::string_view system_name, std::string_view test_name);

    void require(bool condition, const char* expression, const char* file, std::uint32_t line);
    void check(bool condition, const char* expression, const char* file, std::uint32_t line);
    void fail(const char* file, std::uint32_t line, std::string_view message);

    [[nodiscard]] bool should_abort() const noexcept { return should_abort_; }
    [[nodiscard]] std::uint32_t assertion_count() const noexcept { return assertion_count_; }
    [[nodiscard]] std::uint32_t failure_count() const noexcept { return failure_count_; }
    [[nodiscard]] const std::string& summary() const noexcept { return summary_; }

private:
    void record_failure(
        const char* file,
        std::uint32_t line,
        std::string_view message,
        bool abort_after_failure);

private:
    std::string system_name_ {};
    std::string test_name_ {};
    std::uint32_t assertion_count_ {0U};
    std::uint32_t failure_count_ {0U};
    bool should_abort_ {false};
    std::string summary_ {};
};

using SourceSystemTestFunction = void (*)(SystemTestExecutionContext&);
using AssetSystemTestRunnerFunction = void (*)(SystemTestExecutionContext&, const SystemTestAssetDocument&);

struct RegisteredSourceSystemTestCase final
{
    const char* system_name;
    const char* test_name;
    const char* source_path;
    std::uint32_t source_line;
    SourceSystemTestFunction run;
};

struct RegisteredAssetSystemTestRunner final
{
    const char* runner_name;
    AssetSystemTestRunnerFunction run;
};

class SourceSystemTestRegistration final
{
public:
    explicit SourceSystemTestRegistration(RegisteredSourceSystemTestCase test_case);
};

class AssetSystemTestRunnerRegistration final
{
public:
    explicit AssetSystemTestRunnerRegistration(RegisteredAssetSystemTestRunner runner);
};

[[nodiscard]] std::span<const RegisteredSourceSystemTestCase> registered_source_system_tests();

[[nodiscard]] const RegisteredAssetSystemTestRunner* find_asset_system_test_runner(std::string_view runner_name);

[[nodiscard]] Gs1Status fill_system_test_case_info(
    std::uint32_t index,
    Gs1SystemTestCaseInfo& out_info);

[[nodiscard]] Gs1Status run_registered_source_system_test_case(
    std::uint32_t index,
    Gs1SystemTestRunResult& out_result);

[[nodiscard]] Gs1Status run_system_test_asset_file(
    const char* asset_path_utf8,
    Gs1SystemTestRunResult& out_result);
}  // namespace gs1::testing

#define GS1_SYSTEM_TEST_CONCAT_IMPL(Left, Right) Left##Right
#define GS1_SYSTEM_TEST_CONCAT(Left, Right) GS1_SYSTEM_TEST_CONCAT_IMPL(Left, Right)

#define GS1_SYSTEM_TEST_REQUIRE(Context, Expression) \
    do \
    { \
        (Context).require(static_cast<bool>(Expression), #Expression, __FILE__, static_cast<std::uint32_t>(__LINE__)); \
        if ((Context).should_abort()) \
        { \
            return; \
        } \
    } while (false)

#define GS1_SYSTEM_TEST_CHECK(Context, Expression) \
    do \
    { \
        (Context).check(static_cast<bool>(Expression), #Expression, __FILE__, static_cast<std::uint32_t>(__LINE__)); \
    } while (false)

#define GS1_SYSTEM_TEST_FAIL(Context, Message) \
    do \
    { \
        (Context).fail(__FILE__, static_cast<std::uint32_t>(__LINE__), (Message)); \
        return; \
    } while (false)

#define GS1_REGISTER_SOURCE_SYSTEM_TEST(SystemName, TestName, FunctionName) \
    namespace \
    { \
    const ::gs1::testing::SourceSystemTestRegistration \
        GS1_SYSTEM_TEST_CONCAT(g_source_system_test_registration_, __LINE__)({ \
            SystemName, \
            TestName, \
            __FILE__, \
            static_cast<std::uint32_t>(__LINE__), \
            FunctionName}); \
    }

#define GS1_REGISTER_SYSTEM_TEST_ASSET_RUNNER(RunnerName, FunctionName) \
    namespace \
    { \
    const ::gs1::testing::AssetSystemTestRunnerRegistration \
        GS1_SYSTEM_TEST_CONCAT(g_asset_system_test_runner_registration_, __LINE__)({ \
            RunnerName, \
            FunctionName}); \
    }
