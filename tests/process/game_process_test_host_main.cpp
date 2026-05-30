#include "game_process_registry.h"

#include "host/adapter_metadata_catalog.h"
#include "host/runtime_dll_loader.h"

#include <cstdlib>
#include <filesystem>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace
{
std::ofstream g_log_stream {};

void write_process_log(FILE* stream, const std::string& text)
{
    if (stream != nullptr)
    {
        std::fputs(text.c_str(), stream);
        std::fflush(stream);
    }
    if (g_log_stream.is_open())
    {
        g_log_stream << text;
        g_log_stream.flush();
    }
}

void process_info(const std::string& text)
{
    write_process_log(stdout, text);
}

void process_error(const std::string& text)
{
    write_process_log(stderr, text);
}

struct HostOptions final
{
    std::filesystem::path dll_path {};
    std::vector<std::string> scenarios {};
    GameProcessTestRunOptions run_options {};
    bool list_only {false};
    bool help_requested {false};
    bool verbose_logging {false};
};

void print_usage()
{
    std::cout
        << "Usage: gs1_game_process_test_host [dll_path] [options]\n"
        << "Options:\n"
        << "  --list                         List registered game-process scenarios without running them.\n"
        << "  --all                          Run all registered scenarios (default).\n"
        << "  --scenario <name>             Run one named scenario. Repeat to include more.\n"
        << "  --max-frames <count>          Override the per-scenario frame budget.\n"
        << "  --frame-delta-seconds <secs>  Override the fixed headless frame delta.\n"
        << "  --verbose                     Keep verbose gameplay logging enabled.\n"
        << "  --help                        Print this message.\n";
}

bool parse_arguments(int argc, char** argv, const std::filesystem::path& repo_root, HostOptions& out_options)
{
    out_options = {};
    out_options.dll_path = repo_root / "build" / "Debug" / "gs1_game.dll";

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

        if (argument == "--verbose")
        {
            out_options.verbose_logging = true;
            continue;
        }

        if ((argument == "--scenario" ||
                argument == "--max-frames" ||
                argument == "--frame-delta-seconds") &&
            (index + 1) >= argc)
        {
            std::cerr << "Missing value for " << argument << ".\n";
            return false;
        }

        if (argument == "--scenario")
        {
            out_options.scenarios.emplace_back(argv[++index]);
            continue;
        }

        if (argument == "--max-frames")
        {
            out_options.run_options.max_frames = static_cast<std::uint32_t>(
                std::strtoul(argv[++index], nullptr, 10));
            continue;
        }

        if (argument == "--frame-delta-seconds")
        {
            out_options.run_options.frame_delta_seconds = std::strtod(argv[++index], nullptr);
            continue;
        }

        if (!argument.empty() && argument.front() == '-')
        {
            std::cerr << "Unknown option: " << argument << "\n";
            return false;
        }

        if (saw_dll_path)
        {
            std::cerr << "Only one positional DLL path is supported.\n";
            return false;
        }

        out_options.dll_path = std::filesystem::path(argument);
        saw_dll_path = true;
    }

    return true;
}

std::vector<GameProcessScenarioDescriptor> collect_selected_scenarios(const HostOptions& options)
{
    const auto all_scenarios = list_game_process_scenarios();
    if (options.scenarios.empty())
    {
        return all_scenarios;
    }

    std::vector<GameProcessScenarioDescriptor> selected {};
    selected.reserve(options.scenarios.size());
    for (const std::string& name : options.scenarios)
    {
        const auto* scenario = find_game_process_scenario(name);
        if (scenario == nullptr)
        {
            std::cerr << "Unknown game-process scenario: " << name << "\n";
            return {};
        }

        selected.push_back(*scenario);
    }

    return selected;
}

bool create_runtime(
    const Gs1RuntimeApi& api,
    const std::filesystem::path& repo_root,
    Gs1RuntimeHandle*& out_runtime)
{
    Gs1RuntimeCreateDesc create_desc {};
    create_desc.struct_size = sizeof(Gs1RuntimeCreateDesc);
    create_desc.api_version = api.get_api_version();
    create_desc.fixed_step_seconds = 1.0 / 60.0;

    const std::string project_config_root = (repo_root / "project").string();
    load_adapter_metadata_catalog_from_project_root(project_config_root);
    create_desc.project_config_root_utf8 = project_config_root.c_str();
    create_desc.adapter_config_json_utf8 = nullptr;

    out_runtime = nullptr;
    const Gs1Status status = api.create_runtime(&create_desc, &out_runtime);
    if (status != GS1_STATUS_OK || out_runtime == nullptr)
    {
        process_error(
            "Failed to create gameplay runtime for process tests (status=" +
            std::to_string(static_cast<int>(status)) +
            ").\n");
        return false;
    }

    return true;
}
}  // namespace

int main(int argc, char** argv)
{
    const std::filesystem::path repo_root = std::filesystem::current_path();

    HostOptions options {};
    if (!parse_arguments(argc, argv, repo_root, options))
    {
        print_usage();
        return 1;
    }

    if (options.help_requested)
    {
        print_usage();
        return 0;
    }

    const auto selected_scenarios = collect_selected_scenarios(options);
    if (!options.scenarios.empty() && selected_scenarios.empty())
    {
        return 1;
    }

    if (options.list_only)
    {
        if (selected_scenarios.empty())
        {
            std::cout << "No game-process scenarios registered.\n";
            return 0;
        }

        for (const auto& scenario : selected_scenarios)
        {
            std::cout << scenario.name << " - " << scenario.description << "\n";
        }

        return 0;
    }

    if (selected_scenarios.empty())
    {
        std::cerr << "No game-process scenarios registered.\n";
        return 1;
    }

    const auto log_path = repo_root / "out" / "logs" / "game_process_test_host_latest.log";
    g_log_stream.open(log_path, std::ios::out | std::ios::trunc);
    if (!g_log_stream.is_open())
    {
        process_error("Failed to open log file: " + log_path.string() + "\n");
    }
    else
    {
        process_info("Writing game-process test log to " + log_path.string() + "\n");
    }

    RuntimeDllLoader loader {};
    if (!loader.load(options.dll_path.c_str()))
    {
        process_error(loader.last_error() + "\n");
        g_log_stream.close();
        return 1;
    }

    const auto& api = loader.api();

    bool succeeded = true;
    for (const auto& scenario : selected_scenarios)
    {
        process_info(std::string("[PROCESS] starting scenario=") + scenario.name + "\n");

        Gs1RuntimeHandle* runtime = nullptr;
        if (!create_runtime(api, repo_root, runtime))
        {
            succeeded = false;
            break;
        }

        const int exit_code = scenario.run(api, runtime, options.verbose_logging, options.run_options);
        api.destroy_runtime(runtime);

        if (exit_code != 0)
        {
            process_error(
                std::string("[PROCESS] scenario=") + scenario.name +
                " failed with exit code " + std::to_string(exit_code) + "\n");
            succeeded = false;
            break;
        }

        process_info(std::string("[PROCESS] scenario=") + scenario.name + " passed\n");
    }

    g_log_stream.close();
    return succeeded ? 0 : 1;
}
