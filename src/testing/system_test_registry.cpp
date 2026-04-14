#include "testing/system_test_registry.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <exception>
#include <filesystem>
#include <utility>
#include <vector>

namespace gs1::testing
{
namespace
{
std::vector<RegisteredSourceSystemTestCase>& source_system_test_registry()
{
    static std::vector<RegisteredSourceSystemTestCase> registry {};
    return registry;
}

std::vector<RegisteredAssetSystemTestRunner>& asset_system_test_runner_registry()
{
    static std::vector<RegisteredAssetSystemTestRunner> registry {};
    return registry;
}

std::string make_failure_message(const char* file, std::uint32_t line, std::string_view message)
{
    if (file == nullptr || *file == '\0')
    {
        return std::string(message);
    }

    return std::string(file) + ":" + std::to_string(line) + ": " + std::string(message);
}

void initialize_result(Gs1SystemTestRunResult& out_result)
{
    out_result = {};
    out_result.struct_size = sizeof(Gs1SystemTestRunResult);
}

void write_result_message(std::string_view message, Gs1SystemTestRunResult& out_result)
{
    std::memset(out_result.message, 0, sizeof(out_result.message));

    const std::size_t bytes_to_copy = std::min<std::size_t>(message.size(), sizeof(out_result.message) - 1U);
    if (bytes_to_copy > 0U)
    {
        std::memcpy(out_result.message, message.data(), bytes_to_copy);
    }
}

void finalize_result(
    const SystemTestExecutionContext& context,
    double elapsed_milliseconds,
    Gs1SystemTestRunResult& out_result)
{
    out_result.assertion_count = context.assertion_count();
    out_result.failure_count = context.failure_count();
    out_result.elapsed_milliseconds = elapsed_milliseconds;

    if (!context.summary().empty())
    {
        write_result_message(context.summary(), out_result);
        return;
    }

    write_result_message("passed", out_result);
}

std::filesystem::path path_from_utf8(const char* value)
{
    const auto* utf8_bytes = reinterpret_cast<const char8_t*>(value);
    return std::filesystem::path(std::u8string(utf8_bytes));
}
}  // namespace

SystemTestExecutionContext::SystemTestExecutionContext(std::string_view system_name, std::string_view test_name)
    : system_name_(system_name)
    , test_name_(test_name)
{
}

void SystemTestExecutionContext::require(
    bool condition,
    const char* expression,
    const char* file,
    std::uint32_t line)
{
    assertion_count_ += 1U;

    if (!condition)
    {
        record_failure(file, line, std::string("require failed: ") + expression, true);
    }
}

void SystemTestExecutionContext::check(
    bool condition,
    const char* expression,
    const char* file,
    std::uint32_t line)
{
    assertion_count_ += 1U;

    if (!condition)
    {
        record_failure(file, line, std::string("check failed: ") + expression, false);
    }
}

void SystemTestExecutionContext::fail(
    const char* file,
    std::uint32_t line,
    std::string_view message)
{
    record_failure(file, line, message, true);
}

void SystemTestExecutionContext::record_failure(
    const char* file,
    std::uint32_t line,
    std::string_view message,
    bool abort_after_failure)
{
    failure_count_ += 1U;
    should_abort_ = should_abort_ || abort_after_failure;

    if (summary_.empty())
    {
        summary_ = make_failure_message(file, line, message);
    }
}

SourceSystemTestRegistration::SourceSystemTestRegistration(RegisteredSourceSystemTestCase test_case)
{
    source_system_test_registry().push_back(test_case);
}

AssetSystemTestRunnerRegistration::AssetSystemTestRunnerRegistration(RegisteredAssetSystemTestRunner runner)
{
    asset_system_test_runner_registry().push_back(runner);
}

std::span<const RegisteredSourceSystemTestCase> registered_source_system_tests()
{
    const auto& registry = source_system_test_registry();
    return {registry.data(), registry.size()};
}

const RegisteredAssetSystemTestRunner* find_asset_system_test_runner(std::string_view runner_name)
{
    const auto& registry = asset_system_test_runner_registry();
    const auto it = std::find_if(
        registry.begin(),
        registry.end(),
        [&](const RegisteredAssetSystemTestRunner& runner) {
            return runner.runner_name != nullptr && runner_name == runner.runner_name;
        });
    return it == registry.end() ? nullptr : &(*it);
}

Gs1Status fill_system_test_case_info(
    std::uint32_t index,
    Gs1SystemTestCaseInfo& out_info)
{
    const auto tests = registered_source_system_tests();
    if (index >= tests.size())
    {
        return GS1_STATUS_NOT_FOUND;
    }

    const auto& test_case = tests[index];
    out_info.system_name = test_case.system_name;
    out_info.test_name = test_case.test_name;
    out_info.source_path = test_case.source_path;
    out_info.source_line = test_case.source_line;
    out_info.source_kind = GS1_SYSTEM_TEST_CASE_SOURCE_CODE;
    out_info.reserved0[0] = 0U;
    out_info.reserved0[1] = 0U;
    out_info.reserved0[2] = 0U;
    return GS1_STATUS_OK;
}

Gs1Status run_registered_source_system_test_case(
    std::uint32_t index,
    Gs1SystemTestRunResult& out_result)
{
    initialize_result(out_result);

    const auto tests = registered_source_system_tests();
    if (index >= tests.size())
    {
        write_result_message("Registered system test case index is out of range.", out_result);
        return GS1_STATUS_NOT_FOUND;
    }

    const auto& test_case = tests[index];
    SystemTestExecutionContext context {
        test_case.system_name != nullptr ? test_case.system_name : "",
        test_case.test_name != nullptr ? test_case.test_name : ""};

    const auto started_at = std::chrono::steady_clock::now();

    try
    {
        test_case.run(context);
    }
    catch (const std::exception& exception)
    {
        context.fail(test_case.source_path, test_case.source_line, std::string("Unhandled exception: ") + exception.what());
    }
    catch (...)
    {
        context.fail(test_case.source_path, test_case.source_line, "Unhandled non-standard exception.");
    }

    const auto finished_at = std::chrono::steady_clock::now();
    const std::chrono::duration<double, std::milli> elapsed = finished_at - started_at;
    finalize_result(context, elapsed.count(), out_result);
    return GS1_STATUS_OK;
}

Gs1Status run_system_test_asset_file(
    const char* asset_path_utf8,
    Gs1SystemTestRunResult& out_result)
{
    initialize_result(out_result);

    if (asset_path_utf8 == nullptr || *asset_path_utf8 == '\0')
    {
        write_result_message("System test asset path must not be empty.", out_result);
        return GS1_STATUS_INVALID_ARGUMENT;
    }

    SystemTestAssetDocument document {};
    std::string parse_error {};
    if (!load_system_test_asset_document(path_from_utf8(asset_path_utf8), document, parse_error))
    {
        write_result_message(parse_error, out_result);
        return GS1_STATUS_INVALID_ARGUMENT;
    }

    const auto* runner = find_asset_system_test_runner(document.runner_name);
    if (runner == nullptr || runner->run == nullptr)
    {
        write_result_message(
            "No asset system test runner is registered for '" + document.runner_name + "'.",
            out_result);
        return GS1_STATUS_NOT_FOUND;
    }

    SystemTestExecutionContext context {document.system_name, document.test_name};
    const auto started_at = std::chrono::steady_clock::now();

    try
    {
        runner->run(context, document);
    }
    catch (const std::exception& exception)
    {
        context.fail(nullptr, 0U, std::string("Unhandled exception: ") + exception.what());
    }
    catch (...)
    {
        context.fail(nullptr, 0U, "Unhandled non-standard exception.");
    }

    const auto finished_at = std::chrono::steady_clock::now();
    const std::chrono::duration<double, std::milli> elapsed = finished_at - started_at;
    finalize_result(context, elapsed.count(), out_result);
    return GS1_STATUS_OK;
}
}  // namespace gs1::testing
