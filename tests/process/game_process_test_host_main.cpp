#include "game_process_registry.h"

#include "host/adapter_metadata_catalog.h"
#include "host/runtime_dll_loader.h"
#include "smoke_log.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace
{
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
        smoke_log::errorf(
            "Failed to create gameplay runtime for process tests (status=%d).\n",
            static_cast<int>(status));
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
    const bool logging_ready = smoke_log::initialize_file_sink(log_path);
    if (!logging_ready)
    {
        smoke_log::errorf("Failed to open log file: %s\n", log_path.string().c_str());
    }
    else
    {
        smoke_log::infof("Writing game-process test log to %s\n", log_path.string().c_str());
    }

    RuntimeDllLoader loader {};
    if (!loader.load(options.dll_path.c_str()))
    {
        smoke_log::errorf("%s\n", loader.last_error().c_str());
        smoke_log::shutdown_file_sink();
        return 1;
    }

    const auto& api = loader.api();
    const auto log_mode = options.verbose_logging
        ? SmokeEngineHost::LogMode::Verbose
        : SmokeEngineHost::LogMode::ActivityOnly;

    bool succeeded = true;
    for (const auto& scenario : selected_scenarios)
    {
        smoke_log::infof("[PROCESS] starting scenario=%s\n", scenario.name);

        Gs1RuntimeHandle* runtime = nullptr;
        if (!create_runtime(api, repo_root, runtime))
        {
            succeeded = false;
            break;
        }

        const int exit_code = scenario.run(api, runtime, log_mode, options.run_options);
        api.destroy_runtime(runtime);

        if (exit_code != 0)
        {
            smoke_log::errorf("[PROCESS] scenario=%s failed with exit code %d\n", scenario.name, exit_code);
            succeeded = false;
            break;
        }

        smoke_log::infof("[PROCESS] scenario=%s passed\n", scenario.name);
    }

    smoke_log::shutdown_file_sink();
    return succeeded ? 0 : 1;
}
