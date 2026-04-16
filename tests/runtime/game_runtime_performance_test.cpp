#include "runtime/game_runtime.h"
#include "site/site_world_access.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace
{
using gs1::CampaignState;
using gs1::GameCommand;
using gs1::GameCommandType;
using gs1::GameRuntime;
using gs1::SiteRunState;
using gs1::StartNewCampaignCommand;
using gs1::StartSiteAttemptCommand;
using gs1::TileCoord;

constexpr double kFixedStepSeconds = 1.0 / 60.0;
constexpr int kDefaultWarmupFrames = 120;
constexpr int kDefaultMeasuredFrames = 600;
}  // namespace

namespace gs1
{

struct GameRuntimeProjectionTestAccess
{
    static std::optional<CampaignState>& campaign(GameRuntime& runtime)
    {
        return runtime.campaign_;
    }

    static std::optional<SiteRunState>& active_site_run(GameRuntime& runtime)
    {
        return runtime.active_site_run_;
    }
};
}  // namespace gs1

namespace
{

struct NamedSystem final
{
    std::string_view cli_name;
    std::string_view display_name;
    Gs1RuntimeProfileSystemId system_id;
};

constexpr NamedSystem kNamedSystems[] {
    {"campaign_flow", "CampaignFlowSystem", GS1_RUNTIME_PROFILE_SYSTEM_CAMPAIGN_FLOW},
    {"loadout_planner", "LoadoutPlannerSystem", GS1_RUNTIME_PROFILE_SYSTEM_LOADOUT_PLANNER},
    {"site_flow", "SiteFlowSystem", GS1_RUNTIME_PROFILE_SYSTEM_SITE_FLOW},
    {"modifier", "ModifierSystem", GS1_RUNTIME_PROFILE_SYSTEM_MODIFIER},
    {"weather_event", "WeatherEventSystem", GS1_RUNTIME_PROFILE_SYSTEM_WEATHER_EVENT},
    {"action_execution", "ActionExecutionSystem", GS1_RUNTIME_PROFILE_SYSTEM_ACTION_EXECUTION},
    {"local_weather_resolve", "LocalWeatherResolveSystem", GS1_RUNTIME_PROFILE_SYSTEM_LOCAL_WEATHER_RESOLVE},
    {"worker_condition", "WorkerConditionSystem", GS1_RUNTIME_PROFILE_SYSTEM_WORKER_CONDITION},
    {"camp_durability", "CampDurabilitySystem", GS1_RUNTIME_PROFILE_SYSTEM_CAMP_DURABILITY},
    {"device_maintenance", "DeviceMaintenanceSystem", GS1_RUNTIME_PROFILE_SYSTEM_DEVICE_MAINTENANCE},
    {"device_support", "DeviceSupportSystem", GS1_RUNTIME_PROFILE_SYSTEM_DEVICE_SUPPORT},
    {"ecology", "EcologySystem", GS1_RUNTIME_PROFILE_SYSTEM_ECOLOGY},
    {"inventory", "InventorySystem", GS1_RUNTIME_PROFILE_SYSTEM_INVENTORY},
    {"craft", "CraftSystem", GS1_RUNTIME_PROFILE_SYSTEM_CRAFT},
    {"task_board", "TaskBoardSystem", GS1_RUNTIME_PROFILE_SYSTEM_TASK_BOARD},
    {"economy_phone", "EconomyPhoneSystem", GS1_RUNTIME_PROFILE_SYSTEM_ECONOMY_PHONE},
    {"placement_validation", "PlacementValidationSystem", GS1_RUNTIME_PROFILE_SYSTEM_PLACEMENT_VALIDATION},
    {"failure_recovery", "FailureRecoverySystem", GS1_RUNTIME_PROFILE_SYSTEM_FAILURE_RECOVERY},
    {"site_completion", "SiteCompletionSystem", GS1_RUNTIME_PROFILE_SYSTEM_SITE_COMPLETION},
};

struct ScenarioConfig final
{
    std::string name;
    int warmup_frames {kDefaultWarmupFrames};
    int measured_frames {kDefaultMeasuredFrames};
    std::vector<Gs1RuntimeProfileSystemId> disabled_systems {};
};

struct ScenarioResult final
{
    std::string name;
    int warmup_frames {0};
    int measured_frames {0};
    std::uint64_t fixed_steps_executed {0U};
    double total_wall_ms {0.0};
    double avg_wall_ms {0.0};
    double max_wall_ms {0.0};
    Gs1RuntimeProfilingSnapshot profiling {};
};

GameCommand make_start_campaign_command()
{
    GameCommand command {};
    command.type = GameCommandType::StartNewCampaign;
    command.set_payload(StartNewCampaignCommand {42ULL, 30U});
    return command;
}

GameCommand make_start_site_attempt_command(std::uint32_t site_id)
{
    GameCommand command {};
    command.type = GameCommandType::StartSiteAttempt;
    command.set_payload(StartSiteAttemptCommand {site_id});
    return command;
}

void drain_engine_commands(GameRuntime& runtime)
{
    Gs1EngineCommand command {};
    while (runtime.pop_engine_command(command) == GS1_STATUS_OK)
    {
    }
}

void bootstrap_site_one(GameRuntime& runtime)
{
    assert(runtime.handle_command(make_start_campaign_command()) == GS1_STATUS_OK);
    auto& campaign = gs1::GameRuntimeProjectionTestAccess::campaign(runtime);
    assert(campaign.has_value());
    assert(!campaign->sites.empty());

    const auto site_id = campaign->sites.front().site_id.value;
    assert(runtime.handle_command(make_start_site_attempt_command(site_id)) == GS1_STATUS_OK);
    assert(gs1::GameRuntimeProjectionTestAccess::active_site_run(runtime).has_value());
}

void seed_dense_cover(SiteRunState& site_run)
{
    assert(site_run.site_world != nullptr);
    const auto tile_count = site_run.site_world->tile_count();
    for (std::size_t index = 0; index < tile_count; ++index)
    {
        auto tile = site_run.site_world->tile_at_index(index);
        tile.ecology.ground_cover_type_id = 4U;
        tile.ecology.plant_density = 0.35f + (static_cast<float>(index % 5U) * 0.1f);
        tile.ecology.sand_burial = static_cast<float>(index % 4U) * 0.15f;
        site_run.site_world->set_tile_at_index(index, tile);
    }
}

const NamedSystem* find_named_system(std::string_view name)
{
    const auto it = std::find_if(
        std::begin(kNamedSystems),
        std::end(kNamedSystems),
        [&](const NamedSystem& entry) { return entry.cli_name == name; });
    return it == std::end(kNamedSystems) ? nullptr : &(*it);
}

std::string system_name(Gs1RuntimeProfileSystemId system_id)
{
    const auto it = std::find_if(
        std::begin(kNamedSystems),
        std::end(kNamedSystems),
        [&](const NamedSystem& entry) { return entry.system_id == system_id; });
    return it == std::end(kNamedSystems)
        ? std::string {"unknown"}
        : std::string {it->display_name};
}

void print_system_list()
{
    std::cout << "Available systems:\n";
    for (const auto& system : kNamedSystems)
    {
        std::cout << "  " << system.cli_name << " (" << system.display_name << ")\n";
    }
}

ScenarioResult run_scenario(const ScenarioConfig& config)
{
    Gs1RuntimeCreateDesc create_desc {};
    create_desc.struct_size = sizeof(Gs1RuntimeCreateDesc);
    create_desc.api_version = gs1::k_api_version;
    create_desc.fixed_step_seconds = kFixedStepSeconds;

    GameRuntime runtime {create_desc};
    bootstrap_site_one(runtime);
    auto& site_run = gs1::GameRuntimeProjectionTestAccess::active_site_run(runtime).value();
    seed_dense_cover(site_run);
    drain_engine_commands(runtime);

    for (const auto system_id : config.disabled_systems)
    {
        assert(runtime.set_profiled_system_enabled(system_id, false) == GS1_STATUS_OK);
    }

    Gs1Phase1Request phase1_request {};
    phase1_request.struct_size = sizeof(Gs1Phase1Request);
    phase1_request.real_delta_seconds = kFixedStepSeconds;

    Gs1Phase2Request phase2_request {};
    phase2_request.struct_size = sizeof(Gs1Phase2Request);

    runtime.reset_profiling();

    for (int frame = 0; frame < config.warmup_frames; ++frame)
    {
        Gs1Phase1Result phase1_result {};
        Gs1Phase2Result phase2_result {};
        assert(runtime.run_phase1(phase1_request, phase1_result) == GS1_STATUS_OK);
        assert(runtime.run_phase2(phase2_request, phase2_result) == GS1_STATUS_OK);
        drain_engine_commands(runtime);
    }

    runtime.reset_profiling();

    ScenarioResult result {};
    result.name = config.name;
    result.warmup_frames = config.warmup_frames;
    result.measured_frames = config.measured_frames;
    result.profiling.struct_size = sizeof(Gs1RuntimeProfilingSnapshot);

    for (int frame = 0; frame < config.measured_frames; ++frame)
    {
        Gs1Phase1Result phase1_result {};
        Gs1Phase2Result phase2_result {};

        const auto started_at = std::chrono::steady_clock::now();
        assert(runtime.run_phase1(phase1_request, phase1_result) == GS1_STATUS_OK);
        assert(runtime.run_phase2(phase2_request, phase2_result) == GS1_STATUS_OK);
        const double frame_wall_ms = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - started_at)
                                         .count();

        result.total_wall_ms += frame_wall_ms;
        result.max_wall_ms = std::max(result.max_wall_ms, frame_wall_ms);
        result.fixed_steps_executed += phase1_result.fixed_steps_executed;
        drain_engine_commands(runtime);
    }

    result.avg_wall_ms =
        config.measured_frames > 0
            ? (result.total_wall_ms / static_cast<double>(config.measured_frames))
            : 0.0;
    assert(runtime.get_profiling_snapshot(result.profiling) == GS1_STATUS_OK);
    return result;
}

void print_scenario_result(const ScenarioResult& result)
{
    std::cout << "\nScenario: " << result.name << "\n";
    std::cout << "  warmup_frames=" << result.warmup_frames
              << " measured_frames=" << result.measured_frames
              << " fixed_steps=" << result.fixed_steps_executed << "\n";
    std::cout << std::fixed << std::setprecision(3);
    std::cout << "  frame_wall_ms avg=" << result.avg_wall_ms
              << " max=" << result.max_wall_ms
              << " total=" << result.total_wall_ms << "\n";
    std::cout << "  runtime phase1_ms avg="
              << (result.profiling.phase1_timing.sample_count > 0
                      ? (result.profiling.phase1_timing.total_elapsed_ms /
                          static_cast<double>(result.profiling.phase1_timing.sample_count))
                      : 0.0)
              << " max=" << result.profiling.phase1_timing.max_elapsed_ms << "\n";
    std::cout << "  runtime phase2_ms avg="
              << (result.profiling.phase2_timing.sample_count > 0
                      ? (result.profiling.phase2_timing.total_elapsed_ms /
                          static_cast<double>(result.profiling.phase2_timing.sample_count))
                      : 0.0)
              << " max=" << result.profiling.phase2_timing.max_elapsed_ms << "\n";
    std::cout << "  fixed_step_ms avg="
              << (result.profiling.fixed_step_timing.sample_count > 0
                      ? (result.profiling.fixed_step_timing.total_elapsed_ms /
                          static_cast<double>(result.profiling.fixed_step_timing.sample_count))
                      : 0.0)
              << " max=" << result.profiling.fixed_step_timing.max_elapsed_ms << "\n";

    std::vector<const Gs1RuntimeProfileSystemStats*> sorted_systems {};
    sorted_systems.reserve(result.profiling.system_count);
    for (std::uint32_t index = 0; index < result.profiling.system_count; ++index)
    {
        sorted_systems.push_back(&result.profiling.systems[index]);
    }

    std::sort(
        sorted_systems.begin(),
        sorted_systems.end(),
        [](const Gs1RuntimeProfileSystemStats* lhs, const Gs1RuntimeProfileSystemStats* rhs) {
            const double lhs_total = lhs->run_timing.total_elapsed_ms + lhs->command_timing.total_elapsed_ms;
            const double rhs_total = rhs->run_timing.total_elapsed_ms + rhs->command_timing.total_elapsed_ms;
            return lhs_total > rhs_total;
        });

    std::cout << "  per_system_ms:\n";
    for (const auto* system : sorted_systems)
    {
        const double total_ms = system->run_timing.total_elapsed_ms + system->command_timing.total_elapsed_ms;
        const double avg_run_ms = system->run_timing.sample_count > 0
            ? (system->run_timing.total_elapsed_ms / static_cast<double>(system->run_timing.sample_count))
            : 0.0;
        const double avg_command_ms = system->command_timing.sample_count > 0
            ? (system->command_timing.total_elapsed_ms /
                static_cast<double>(system->command_timing.sample_count))
            : 0.0;

        std::cout << "    " << system_name(system->system_id)
                  << " enabled=" << static_cast<unsigned>(system->enabled)
                  << " total=" << total_ms
                  << " run_avg=" << avg_run_ms
                  << " run_max=" << system->run_timing.max_elapsed_ms
                  << " command_avg=" << avg_command_ms
                  << " command_max=" << system->command_timing.max_elapsed_ms
                  << " run_calls=" << system->run_timing.sample_count
                  << " command_calls=" << system->command_timing.sample_count
                  << "\n";
    }
}

}  // namespace

int main(int argc, char** argv)
{
    int warmup_frames = kDefaultWarmupFrames;
    int measured_frames = kDefaultMeasuredFrames;
    bool list_systems = false;
    bool custom_toggle_mode = false;
    std::vector<Gs1RuntimeProfileSystemId> custom_disabled_systems {};

    for (int index = 1; index < argc; ++index)
    {
        const std::string_view arg {argv[index]};
        if (arg == "--list-systems")
        {
            list_systems = true;
            continue;
        }

        if (arg == "--frames" && index + 1 < argc)
        {
            measured_frames = std::max(1, std::atoi(argv[++index]));
            continue;
        }

        if (arg == "--warmup" && index + 1 < argc)
        {
            warmup_frames = std::max(0, std::atoi(argv[++index]));
            continue;
        }

        if (arg == "--disable-system" && index + 1 < argc)
        {
            custom_toggle_mode = true;
            const auto* system = find_named_system(argv[++index]);
            if (system == nullptr)
            {
                std::cerr << "Unknown system name.\n";
                print_system_list();
                return 1;
            }

            custom_disabled_systems.push_back(system->system_id);
            continue;
        }

        std::cerr << "Unsupported argument: " << arg << "\n";
        std::cerr << "Usage: gs1_runtime_performance_test [--list-systems] [--frames N] [--warmup N]"
                  << " [--disable-system name]\n";
        return 1;
    }

    if (list_systems)
    {
        print_system_list();
        return 0;
    }

    std::vector<ScenarioConfig> scenarios {};
    if (custom_toggle_mode)
    {
        scenarios.push_back(ScenarioConfig {
            "custom",
            warmup_frames,
            measured_frames,
            custom_disabled_systems});
    }
    else
    {
        scenarios.push_back(ScenarioConfig {
            "baseline_no_weather",
            warmup_frames,
            measured_frames,
            {
                GS1_RUNTIME_PROFILE_SYSTEM_WEATHER_EVENT,
                GS1_RUNTIME_PROFILE_SYSTEM_LOCAL_WEATHER_RESOLVE,
            }});
        scenarios.push_back(ScenarioConfig {
            "all_enabled",
            warmup_frames,
            measured_frames,
            {}});
    }

    std::cout << "GS1 runtime performance test\n";
    std::cout << "  fixed_step_seconds=" << std::fixed << std::setprecision(6) << kFixedStepSeconds << "\n";
    std::cout << "  note=local weather resolution is enabled in this perf target only\n";

    for (const auto& scenario : scenarios)
    {
        const auto result = run_scenario(scenario);
        print_scenario_result(result);
    }

    return 0;
}
