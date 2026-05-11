#include "content/defs/plant_defs.h"
#include "runtime/game_runtime.h"
#include "site/tile_footprint.h"

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
using gs1::GameMessage;
using gs1::GameMessageType;
using gs1::GameRuntime;
using gs1::SiteRunState;
using gs1::StartNewCampaignMessage;
using gs1::StartSiteAttemptMessage;
using gs1::TileCoord;

constexpr double kFixedStepSeconds = 1.0 / 60.0;
constexpr int kDefaultWarmupFrames = 120;
constexpr int kDefaultMeasuredFrames = 600;

[[noreturn]] void fail_test(const char* message)
{
    std::cerr << "Test failure: " << message << "\n";
    std::abort();
}

void require(bool condition, const char* message)
{
    if (!condition)
    {
        fail_test(message);
    }
}

void require_ok(Gs1Status status, const char* message)
{
    if (status != GS1_STATUS_OK)
    {
        std::cerr << "Test failure: " << message
                  << " status=" << static_cast<int>(status) << "\n";
        std::abort();
    }
}
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
    {"campaign_time", "CampaignTimeSystem", GS1_RUNTIME_PROFILE_SYSTEM_CAMPAIGN_TIME},
    {"loadout_planner", "LoadoutPlannerSystem", GS1_RUNTIME_PROFILE_SYSTEM_LOADOUT_PLANNER},
    {"site_flow", "SiteFlowSystem", GS1_RUNTIME_PROFILE_SYSTEM_SITE_FLOW},
    {"site_time", "SiteTimeSystem", GS1_RUNTIME_PROFILE_SYSTEM_SITE_TIME},
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

struct SeededPlant final
{
    gs1::PlantId plant_id {};
    TileCoord anchor {};
    float density {0.0f};
};

constexpr SeededPlant kPerfSeededPlants[] {
    {gs1::PlantId {gs1::k_plant_straw_checkerboard}, TileCoord {2, 2}, 58.0f},
    {gs1::PlantId {gs1::k_plant_ordos_wormwood}, TileCoord {6, 2}, 64.0f},
    {gs1::PlantId {gs1::k_plant_white_thorn}, TileCoord {10, 2}, 62.0f},
    {gs1::PlantId {gs1::k_plant_red_tamarisk}, TileCoord {20, 2}, 66.0f},
    {gs1::PlantId {gs1::k_plant_ningxia_wolfberry}, TileCoord {24, 2}, 60.0f},
    {gs1::PlantId {gs1::k_plant_korshinsk_peashrub}, TileCoord {2, 6}, 63.0f},
    {gs1::PlantId {gs1::k_plant_jiji_grass}, TileCoord {6, 6}, 57.0f},
    {gs1::PlantId {gs1::k_plant_sea_buckthorn}, TileCoord {10, 6}, 68.0f},
    {gs1::PlantId {gs1::k_plant_desert_ephedra}, TileCoord {20, 6}, 59.0f},
    {gs1::PlantId {gs1::k_plant_saxaul}, TileCoord {24, 6}, 72.0f},
    {gs1::PlantId {gs1::k_plant_ordos_wormwood}, TileCoord {2, 22}, 61.0f},
    {gs1::PlantId {gs1::k_plant_white_thorn}, TileCoord {6, 22}, 65.0f},
    {gs1::PlantId {gs1::k_plant_red_tamarisk}, TileCoord {10, 22}, 67.0f},
    {gs1::PlantId {gs1::k_plant_ningxia_wolfberry}, TileCoord {20, 22}, 56.0f},
    {gs1::PlantId {gs1::k_plant_straw_checkerboard}, TileCoord {24, 22}, 55.0f},
    {gs1::PlantId {gs1::k_plant_korshinsk_peashrub}, TileCoord {2, 26}, 64.0f},
    {gs1::PlantId {gs1::k_plant_jiji_grass}, TileCoord {6, 26}, 58.0f},
    {gs1::PlantId {gs1::k_plant_sea_buckthorn}, TileCoord {10, 26}, 69.0f},
    {gs1::PlantId {gs1::k_plant_desert_ephedra}, TileCoord {20, 26}, 60.0f},
    {gs1::PlantId {gs1::k_plant_saxaul}, TileCoord {24, 26}, 74.0f},
};

GameMessage make_start_campaign_message()
{
    GameMessage message {};
    message.type = GameMessageType::StartNewCampaign;
    message.set_payload(StartNewCampaignMessage {42ULL, 30U});
    return message;
}

GameMessage make_start_site_attempt_message(std::uint32_t site_id)
{
    GameMessage message {};
    message.type = GameMessageType::StartSiteAttempt;
    message.set_payload(StartSiteAttemptMessage {site_id});
    return message;
}

Gs1HostMessage make_site_scene_ready_event()
{
    Gs1HostMessage event {};
    event.type = GS1_HOST_EVENT_SITE_SCENE_READY;
    return event;
}

void drain_runtime_messages(GameRuntime& runtime)
{
    Gs1RuntimeMessage message {};
    while (runtime.pop_runtime_message(message) == GS1_STATUS_OK)
    {
    }
}

void bootstrap_site_one(GameRuntime& runtime)
{
    require_ok(runtime.handle_message(make_start_campaign_message()), "starting prototype campaign");
    auto& campaign = gs1::GameRuntimeProjectionTestAccess::campaign(runtime);
    require(campaign.has_value(), "campaign state was not created");
    require(!campaign->sites.empty(), "prototype campaign has no sites");

    const auto site_id = campaign->sites.front().site_id.value;
    require_ok(runtime.handle_message(make_start_site_attempt_message(site_id)), "starting first site attempt");
    require(
        gs1::GameRuntimeProjectionTestAccess::active_site_run(runtime).has_value(),
        "active site run was not created");

    const auto ready_event = make_site_scene_ready_event();
    require_ok(runtime.submit_host_messages(&ready_event, 1U), "submitting site scene ready message");
    Gs1Phase2Request phase2_request {};
    phase2_request.struct_size = sizeof(Gs1Phase2Request);
    Gs1Phase2Result phase2_result {};
    require_ok(runtime.run_phase2(phase2_request, phase2_result), "running site ready phase2");
}

void run_boot_phase(GameRuntime& runtime)
{
    Gs1Phase1Request phase1_request {};
    phase1_request.struct_size = sizeof(Gs1Phase1Request);
    phase1_request.real_delta_seconds = 0.0;

    Gs1Phase2Request phase2_request {};
    phase2_request.struct_size = sizeof(Gs1Phase2Request);

    Gs1Phase1Result phase1_result {};
    Gs1Phase2Result phase2_result {};
    require_ok(runtime.run_phase1(phase1_request, phase1_result), "running boot phase1");
    require_ok(runtime.run_phase2(phase2_request, phase2_result), "running boot phase2");
    drain_runtime_messages(runtime);
}

void seed_dense_cover(SiteRunState& site_run)
{
    require(site_run.site_world != nullptr, "site run has no site world");
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

void seed_perf_plants(SiteRunState& site_run)
{
    require(site_run.site_world != nullptr, "site run has no site world");
    for (const auto& seeded_plant : kPerfSeededPlants)
    {
        const auto footprint = gs1::resolve_plant_tile_footprint(seeded_plant.plant_id);
        gs1::for_each_tile_in_footprint(
            seeded_plant.anchor,
            footprint,
            [&](TileCoord coord) {
                if (!site_run.site_world->contains(coord))
                {
                    return;
                }

                auto tile = site_run.site_world->tile_at(coord);
                tile.ecology.plant_id = seeded_plant.plant_id;
                tile.ecology.ground_cover_type_id = 0U;
                tile.ecology.plant_density = seeded_plant.density;
                tile.ecology.growth_pressure = 0.0f;
                site_run.site_world->set_tile(coord, tile);
            });
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
    create_desc.adapter_config_json_utf8 = nullptr;

    GameRuntime runtime {create_desc};
    run_boot_phase(runtime);
    bootstrap_site_one(runtime);
    auto& active_site_run = gs1::GameRuntimeProjectionTestAccess::active_site_run(runtime);
    require(active_site_run.has_value(), "active site run missing after bootstrap");
    auto& site_run = active_site_run.value();
    seed_dense_cover(site_run);
    seed_perf_plants(site_run);
    drain_runtime_messages(runtime);

    for (const auto system_id : config.disabled_systems)
    {
        require_ok(runtime.set_profiled_system_enabled(system_id, false), "disabling profiled system");
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
        require_ok(runtime.run_phase1(phase1_request, phase1_result), "running warmup phase1");
        require_ok(runtime.run_phase2(phase2_request, phase2_result), "running warmup phase2");
        drain_runtime_messages(runtime);
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
        require_ok(runtime.run_phase1(phase1_request, phase1_result), "running measured phase1");
        require_ok(runtime.run_phase2(phase2_request, phase2_result), "running measured phase2");
        const double frame_wall_ms = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - started_at)
                                         .count();

        result.total_wall_ms += frame_wall_ms;
        result.max_wall_ms = std::max(result.max_wall_ms, frame_wall_ms);
        result.fixed_steps_executed += phase1_result.fixed_steps_executed;
        drain_runtime_messages(runtime);
    }

    result.avg_wall_ms =
        config.measured_frames > 0
            ? (result.total_wall_ms / static_cast<double>(config.measured_frames))
            : 0.0;
    require_ok(runtime.get_profiling_snapshot(result.profiling), "capturing profiling snapshot");
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
            const double lhs_total = lhs->run_timing.total_elapsed_ms + lhs->message_timing.total_elapsed_ms;
            const double rhs_total = rhs->run_timing.total_elapsed_ms + rhs->message_timing.total_elapsed_ms;
            return lhs_total > rhs_total;
        });

    std::cout << "  per_system_ms:\n";
    for (const auto* system : sorted_systems)
    {
        const double total_ms = system->run_timing.total_elapsed_ms + system->message_timing.total_elapsed_ms;
        const double avg_run_ms = system->run_timing.sample_count > 0
            ? (system->run_timing.total_elapsed_ms / static_cast<double>(system->run_timing.sample_count))
            : 0.0;
        const double avg_message_ms = system->message_timing.sample_count > 0
            ? (system->message_timing.total_elapsed_ms /
                static_cast<double>(system->message_timing.sample_count))
            : 0.0;

        std::cout << "    " << system_name(system->system_id)
                  << " enabled=" << static_cast<unsigned>(system->enabled)
                  << " total=" << total_ms
                  << " run_avg=" << avg_run_ms
                  << " run_max=" << system->run_timing.max_elapsed_ms
                  << " message_avg=" << avg_message_ms
                  << " message_max=" << system->message_timing.max_elapsed_ms
                  << " run_calls=" << system->run_timing.sample_count
                  << " message_calls=" << system->message_timing.sample_count
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
    std::cout << "  note=local weather resolution is enabled in normal builds\n";
    std::cout << "  seeded_plants=" << std::size(kPerfSeededPlants) << " mixed authored placements\n";

    for (const auto& scenario : scenarios)
    {
        const auto result = run_scenario(scenario);
        print_scenario_result(result);
    }

    return 0;
}


