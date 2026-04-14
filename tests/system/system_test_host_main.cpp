#include "runtime_dll_loader.h"
#include "testing/system_test_asset.h"

#include <filesystem>
#include <iostream>
#include <set>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace
{
enum class DiscoveredSystemTestKind : std::uint8_t
{
    SourceCode = 0,
    AssetFile = 1
};

struct DiscoveredSystemTestCase final
{
    DiscoveredSystemTestKind kind {DiscoveredSystemTestKind::SourceCode};
    std::string system_name {};
    std::string test_name {};
    std::string origin_text {};
    std::string runner_name {};
    std::uint32_t source_index {0U};
    std::filesystem::path asset_path {};
};

struct HostOptions final
{
    std::filesystem::path dll_path {};
    std::filesystem::path asset_dir {};
    std::vector<std::string> system_filters {};
    std::vector<std::filesystem::path> explicit_assets {};
    bool list_only {false};
    bool help_requested {false};
};

std::string lowercase_ascii_copy(std::string_view value)
{
    std::string result(value);
    for (char& character : result)
    {
        if (character >= 'A' && character <= 'Z')
        {
            character = static_cast<char>(character - 'A' + 'a');
        }
    }

    return result;
}

std::string utf8_from_path(const std::filesystem::path& path)
{
    const auto utf8_path = path.lexically_normal().u8string();
    return {reinterpret_cast<const char*>(utf8_path.c_str()), utf8_path.size()};
}

std::string system_test_label(const DiscoveredSystemTestCase& test_case)
{
    return test_case.system_name + "." + test_case.test_name;
}

const char* kind_text(DiscoveredSystemTestKind kind)
{
    switch (kind)
    {
    case DiscoveredSystemTestKind::SourceCode:
        return "source";
    case DiscoveredSystemTestKind::AssetFile:
        return "asset";
    }

    return "unknown";
}

const char* status_text(Gs1Status status)
{
    switch (status)
    {
    case GS1_STATUS_OK:
        return "OK";
    case GS1_STATUS_INVALID_ARGUMENT:
        return "INVALID_ARGUMENT";
    case GS1_STATUS_OUT_OF_MEMORY:
        return "OUT_OF_MEMORY";
    case GS1_STATUS_INVALID_STATE:
        return "INVALID_STATE";
    case GS1_STATUS_NOT_FOUND:
        return "NOT_FOUND";
    case GS1_STATUS_BUFFER_EMPTY:
        return "BUFFER_EMPTY";
    case GS1_STATUS_NOT_IMPLEMENTED:
        return "NOT_IMPLEMENTED";
    case GS1_STATUS_INTERNAL_ERROR:
        return "INTERNAL_ERROR";
    }

    return "UNKNOWN_STATUS";
}

void print_usage()
{
    std::cout
        << "Usage: gs1_system_test_host [dll_path] [options]\n"
        << "Options:\n"
        << "  --list                 List discovered system tests without running them.\n"
        << "  --all                  Run all discovered tests (default).\n"
        << "  --system <name>        Limit execution to one system. Repeat to include more systems.\n"
        << "  --asset-dir <path>     Override the default asset discovery directory.\n"
        << "  --asset <path>         Include a specific asset test file. Repeat as needed.\n"
        << "  --help                 Print this message.\n";
}

bool parse_arguments(int argc, char** argv, HostOptions& out_options, std::string& out_error)
{
    out_options = {};
    out_error.clear();

    const std::filesystem::path repo_root = std::filesystem::current_path();
    out_options.dll_path = repo_root / "build" / "Debug" / "gs1_game.dll";
    out_options.asset_dir = repo_root / "tests" / "system" / "assets";

    bool saw_dll_path = false;

    for (int index = 1; index < argc; ++index)
    {
        const std::string argument = argv[index];

        if (argument == "--help")
        {
            out_options.help_requested = true;
            return true;
        }

        if (argument == "--list")
        {
            out_options.list_only = true;
            continue;
        }

        if (argument == "--all")
        {
            continue;
        }

        if (argument == "--system" || argument == "--asset-dir" || argument == "--asset")
        {
            if (index + 1 >= argc)
            {
                out_error = "Missing value for " + argument + ".";
                return false;
            }

            const std::string value = argv[++index];
            if (argument == "--system")
            {
                out_options.system_filters.push_back(lowercase_ascii_copy(value));
            }
            else if (argument == "--asset-dir")
            {
                out_options.asset_dir = std::filesystem::path(value);
            }
            else
            {
                out_options.explicit_assets.push_back(std::filesystem::path(value));
            }

            continue;
        }

        if (!argument.empty() && argument.front() == '-')
        {
            out_error = "Unknown option: " + argument;
            return false;
        }

        if (saw_dll_path)
        {
            out_error = "Only one positional DLL path is supported.";
            return false;
        }

        out_options.dll_path = std::filesystem::path(argument);
        saw_dll_path = true;
    }

    return true;
}

bool matches_filters(const HostOptions& options, const DiscoveredSystemTestCase& test_case)
{
    if (options.system_filters.empty())
    {
        return true;
    }

    const std::string normalized_system_name = lowercase_ascii_copy(test_case.system_name);
    for (const std::string& filter : options.system_filters)
    {
        if (filter == normalized_system_name)
        {
            return true;
        }
    }

    return false;
}

std::vector<DiscoveredSystemTestCase> discover_source_system_tests(
    const Gs1RuntimeApi& api,
    std::vector<std::string>& out_errors)
{
    std::vector<DiscoveredSystemTestCase> discovered {};
    out_errors.clear();

    const std::uint32_t test_count = api.get_system_test_case_count();
    for (std::uint32_t index = 0U; index < test_count; ++index)
    {
        Gs1SystemTestCaseInfo info {};
        info.struct_size = sizeof(Gs1SystemTestCaseInfo);

        const Gs1Status status = api.get_system_test_case_info(index, &info);
        if (status != GS1_STATUS_OK)
        {
            out_errors.push_back(
                "Failed to query source system test case index " + std::to_string(index) +
                ": " + status_text(status));
            continue;
        }

        DiscoveredSystemTestCase test_case {};
        test_case.kind = DiscoveredSystemTestKind::SourceCode;
        test_case.system_name = info.system_name != nullptr ? info.system_name : "";
        test_case.test_name = info.test_name != nullptr ? info.test_name : "";
        test_case.source_index = index;
        test_case.origin_text =
            std::string(info.source_path != nullptr ? info.source_path : "") + ":" + std::to_string(info.source_line);
        discovered.push_back(std::move(test_case));
    }

    return discovered;
}

void append_asset_document(
    gs1::testing::SystemTestAssetDocument&& document,
    std::set<std::string>& seen_paths,
    std::vector<DiscoveredSystemTestCase>& out_cases)
{
    const std::string normalized_path = document.path.lexically_normal().string();
    if (!seen_paths.insert(normalized_path).second)
    {
        return;
    }

    DiscoveredSystemTestCase test_case {};
    test_case.kind = DiscoveredSystemTestKind::AssetFile;
    test_case.system_name = document.system_name;
    test_case.test_name = document.test_name;
    test_case.origin_text = normalized_path;
    test_case.runner_name = document.runner_name;
    test_case.asset_path = std::move(document.path);
    out_cases.push_back(std::move(test_case));
}

std::vector<DiscoveredSystemTestCase> discover_asset_system_tests(
    const HostOptions& options,
    std::vector<std::string>& out_errors)
{
    std::vector<DiscoveredSystemTestCase> discovered {};
    out_errors.clear();

    std::set<std::string> seen_paths {};

    std::vector<gs1::testing::SystemTestAssetDocument> asset_documents {};
    std::vector<std::string> discovery_errors {};
    const bool discovery_succeeded =
        gs1::testing::discover_system_test_asset_documents(options.asset_dir, asset_documents, discovery_errors);

    if (!discovery_succeeded && discovery_errors.empty())
    {
        out_errors.push_back("System test asset discovery failed.");
    }

    for (std::string& error : discovery_errors)
    {
        out_errors.push_back(std::move(error));
    }

    for (auto& document : asset_documents)
    {
        append_asset_document(std::move(document), seen_paths, discovered);
    }

    for (const auto& asset_path : options.explicit_assets)
    {
        gs1::testing::SystemTestAssetDocument document {};
        std::string error {};
        if (!gs1::testing::load_system_test_asset_document(asset_path, document, error))
        {
            out_errors.push_back(std::move(error));
            continue;
        }

        append_asset_document(std::move(document), seen_paths, discovered);
    }

    return discovered;
}

void print_discovered_tests(const std::vector<DiscoveredSystemTestCase>& tests)
{
    if (tests.empty())
    {
        std::cout << "No system tests discovered.\n";
        return;
    }

    for (const auto& test_case : tests)
    {
        std::cout << "[" << kind_text(test_case.kind) << "] " << system_test_label(test_case)
                  << "  " << test_case.origin_text;
        if (test_case.kind == DiscoveredSystemTestKind::AssetFile)
        {
            std::cout << "  runner=" << test_case.runner_name;
        }
        std::cout << "\n";
    }
}
}  // namespace

int main(int argc, char** argv)
{
    HostOptions options {};
    std::string argument_error {};
    if (!parse_arguments(argc, argv, options, argument_error))
    {
        std::cerr << argument_error << "\n\n";
        print_usage();
        return 1;
    }

    if (options.help_requested)
    {
        print_usage();
        return 0;
    }

    RuntimeDllLoader loader {};
    if (!loader.load(options.dll_path.c_str()))
    {
        std::cerr << loader.last_error() << "\n";
        return 1;
    }

    const auto& api = loader.api();
    if (api.get_system_test_api_version() != 1U)
    {
        std::cerr << "Unsupported system test API version: " << api.get_system_test_api_version() << "\n";
        return 1;
    }

    std::vector<std::string> source_errors {};
    std::vector<std::string> asset_errors {};
    std::vector<DiscoveredSystemTestCase> discovered = discover_source_system_tests(api, source_errors);

    std::vector<DiscoveredSystemTestCase> asset_tests = discover_asset_system_tests(options, asset_errors);
    discovered.insert(discovered.end(), asset_tests.begin(), asset_tests.end());

    for (const std::string& error : source_errors)
    {
        std::cerr << error << "\n";
    }

    for (const std::string& error : asset_errors)
    {
        std::cerr << error << "\n";
    }

    if (!source_errors.empty() || !asset_errors.empty())
    {
        return 1;
    }

    std::vector<DiscoveredSystemTestCase> selected {};
    for (const auto& test_case : discovered)
    {
        if (matches_filters(options, test_case))
        {
            selected.push_back(test_case);
        }
    }

    if (options.list_only)
    {
        print_discovered_tests(selected);
        return 0;
    }

    if (selected.empty())
    {
        if (!options.system_filters.empty() || !options.explicit_assets.empty())
        {
            std::cerr << "No system tests matched the requested filters.\n";
            return 1;
        }

        std::cout << "No system tests discovered.\n";
        return 0;
    }

    std::uint32_t passed_count = 0U;
    std::uint32_t failed_count = 0U;
    std::uint32_t error_count = 0U;

    for (const auto& test_case : selected)
    {
        std::cout << "RUN  " << system_test_label(test_case) << " [" << kind_text(test_case.kind) << "]\n";

        Gs1SystemTestRunResult result {};
        result.struct_size = sizeof(Gs1SystemTestRunResult);

        Gs1Status status = GS1_STATUS_INTERNAL_ERROR;
        if (test_case.kind == DiscoveredSystemTestKind::SourceCode)
        {
            status = api.run_system_test_case(test_case.source_index, &result);
        }
        else
        {
            const std::string asset_path_utf8 = utf8_from_path(test_case.asset_path);
            status = api.run_system_test_asset_file(asset_path_utf8.c_str(), &result);
        }

        if (status != GS1_STATUS_OK)
        {
            error_count += 1U;
            std::cout << "ERR  " << system_test_label(test_case) << " status=" << status_text(status);
            if (result.message[0] != '\0')
            {
                std::cout << "  " << result.message;
            }
            std::cout << "\n";
            continue;
        }

        if (result.failure_count > 0U)
        {
            failed_count += 1U;
            std::cout << "FAIL " << system_test_label(test_case)
                      << " assertions=" << result.assertion_count
                      << " elapsed_ms=" << result.elapsed_milliseconds;
            if (result.message[0] != '\0')
            {
                std::cout << "  " << result.message;
            }
            std::cout << "\n";
            continue;
        }

        passed_count += 1U;
        std::cout << "PASS " << system_test_label(test_case)
                  << " assertions=" << result.assertion_count
                  << " elapsed_ms=" << result.elapsed_milliseconds << "\n";
    }

    std::cout << "\nExecuted " << selected.size()
              << " system test(s): "
              << passed_count << " passed, "
              << failed_count << " failed, "
              << error_count << " errors.\n";

    return (failed_count == 0U && error_count == 0U) ? 0 : 1;
}
