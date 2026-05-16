#include "campaign/campaign_state.h"
#include "content/defs/plant_defs.h"
#include "messages/game_message.h"
#include "runtime/game_state.h"
#include "runtime/runtime_split_state_compat.h"
#include "runtime/state_manager.h"
#include "runtime/system_interface.h"
#include "site/site_run_state.h"
#include "site/tile_footprint.h"
#include "site/site_world.h"
#include "site/systems/device_weather_contribution_system.h"
#include "site/systems/local_weather_resolve_system.h"
#include "site/systems/plant_weather_contribution_system.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <iomanip>
#include <iostream>
#include <optional>

namespace
{
constexpr std::uint32_t kSiteWidth = 32U;
constexpr std::uint32_t kSiteHeight = 32U;
constexpr std::size_t kTileCount =
    static_cast<std::size_t>(kSiteWidth) * static_cast<std::size_t>(kSiteHeight);
constexpr double kFixedStepSeconds = 1.0 / 60.0;
constexpr int kInitialPassIterations = 200;
constexpr int kSteadyStateIterations = 1000;

struct PipelineTimings final
{
    double plant_ms {0.0};
    double device_ms {0.0};
    double resolve_ms {0.0};
    double total_ms {0.0};
};

struct SeededPlant final
{
    gs1::PlantId plant_id {};
    gs1::TileCoord anchor {};
    float density {0.0f};
};

constexpr SeededPlant kPerfSeededPlants[] {
    {gs1::PlantId {gs1::k_plant_straw_checkerboard}, gs1::TileCoord {2, 2}, 58.0f},
    {gs1::PlantId {gs1::k_plant_ordos_wormwood}, gs1::TileCoord {6, 2}, 64.0f},
    {gs1::PlantId {gs1::k_plant_white_thorn}, gs1::TileCoord {10, 2}, 62.0f},
    {gs1::PlantId {gs1::k_plant_red_tamarisk}, gs1::TileCoord {20, 2}, 66.0f},
    {gs1::PlantId {gs1::k_plant_ningxia_wolfberry}, gs1::TileCoord {24, 2}, 60.0f},
    {gs1::PlantId {gs1::k_plant_korshinsk_peashrub}, gs1::TileCoord {2, 6}, 63.0f},
    {gs1::PlantId {gs1::k_plant_jiji_grass}, gs1::TileCoord {6, 6}, 57.0f},
    {gs1::PlantId {gs1::k_plant_sea_buckthorn}, gs1::TileCoord {10, 6}, 68.0f},
    {gs1::PlantId {gs1::k_plant_desert_ephedra}, gs1::TileCoord {20, 6}, 59.0f},
    {gs1::PlantId {gs1::k_plant_saxaul}, gs1::TileCoord {24, 6}, 72.0f},
    {gs1::PlantId {gs1::k_plant_ordos_wormwood}, gs1::TileCoord {2, 22}, 61.0f},
    {gs1::PlantId {gs1::k_plant_white_thorn}, gs1::TileCoord {6, 22}, 65.0f},
    {gs1::PlantId {gs1::k_plant_red_tamarisk}, gs1::TileCoord {10, 22}, 67.0f},
    {gs1::PlantId {gs1::k_plant_ningxia_wolfberry}, gs1::TileCoord {20, 22}, 56.0f},
    {gs1::PlantId {gs1::k_plant_straw_checkerboard}, gs1::TileCoord {24, 22}, 55.0f},
    {gs1::PlantId {gs1::k_plant_korshinsk_peashrub}, gs1::TileCoord {2, 26}, 64.0f},
    {gs1::PlantId {gs1::k_plant_jiji_grass}, gs1::TileCoord {6, 26}, 58.0f},
    {gs1::PlantId {gs1::k_plant_sea_buckthorn}, gs1::TileCoord {10, 26}, 69.0f},
    {gs1::PlantId {gs1::k_plant_desert_ephedra}, gs1::TileCoord {20, 26}, 60.0f},
    {gs1::PlantId {gs1::k_plant_saxaul}, gs1::TileCoord {24, 26}, 74.0f},
};

gs1::SiteRunState make_site_run()
{
    gs1::SiteRunState site_run {};
    site_run.site_id = gs1::SiteId {1U};
    site_run.site_run_id = gs1::SiteRunId {1U};
    site_run.site_archetype_id = 101U;
    site_run.attempt_index = 1U;
    site_run.run_status = gs1::SiteRunStatus::Active;
    site_run.camp.camp_anchor_tile = gs1::TileCoord {2, 2};
    site_run.site_world = std::make_shared<gs1::SiteWorld>();
    site_run.site_world->initialize(
        gs1::SiteWorld::CreateDesc {
            kSiteWidth,
            kSiteHeight,
            gs1::TileCoord {2, 2},
            2.0f,
            2.0f,
            0.0f,
            100.0f,
            100.0f,
            100.0f,
            100.0f,
            100.0f,
            100.0f,
            1.0f,
            false});
    site_run.weather.weather_heat = 20.0f;
    site_run.weather.weather_wind = 12.0f;
    site_run.weather.weather_dust = 8.0f;
    return site_run;
}

void seed_dense_cover(gs1::SiteRunState& site_run)
{
    for (std::size_t index = 0; index < site_run.site_world->tile_count(); ++index)
    {
        auto tile = site_run.site_world->tile_at_index(index);
        tile.ecology.ground_cover_type_id = 4U;
        tile.ecology.plant_density = 0.35f + (static_cast<float>(index % 5U) * 0.1f);
        tile.ecology.sand_burial = static_cast<float>(index % 4U) * 0.15f;
        site_run.site_world->set_tile_at_index(index, tile);
    }
}

void seed_perf_plants(gs1::SiteRunState& site_run)
{
    for (const auto& seeded_plant : kPerfSeededPlants)
    {
        const auto footprint = gs1::resolve_plant_tile_footprint(seeded_plant.plant_id);
        gs1::for_each_tile_in_footprint(
            seeded_plant.anchor,
            footprint,
            [&](gs1::TileCoord coord) {
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

PipelineTimings run_local_weather_pipeline(
    gs1::CampaignState& campaign,
    gs1::SiteRunState& site_run,
    gs1::GameMessageQueue& message_queue)
{
    gs1::GameState state {};
    gs1::StateManager state_manager {};
    state.app_state = GS1_APP_STATE_SITE_ACTIVE;
    state.fixed_step_seconds = kFixedStepSeconds;
    gs1::write_campaign_state_to_state_sets(std::optional<gs1::CampaignState> {campaign}, state, state_manager);
    gs1::write_site_run_state_to_state_sets(std::optional<gs1::SiteRunState> {site_run}, state, state_manager);

    auto make_invocation = [&]() -> gs1::RuntimeInvocation
    {
        return gs1::RuntimeInvocation {
            state,
            state_manager,
            site_run.site_world};
    };

    gs1::PlantWeatherContributionSystem plant_system {};
    gs1::DeviceWeatherContributionSystem device_system {};
    gs1::LocalWeatherResolveSystem resolve_system {};

    const auto total_started = std::chrono::steady_clock::now();

    const auto plant_started = std::chrono::steady_clock::now();
    auto plant_invocation = make_invocation();
    plant_system.run(plant_invocation);
    const auto device_started = std::chrono::steady_clock::now();
    auto device_invocation = make_invocation();
    device_system.run(device_invocation);
    const auto resolve_started = std::chrono::steady_clock::now();
    auto resolve_invocation = make_invocation();
    resolve_system.run(resolve_invocation);
    const auto total_ended = std::chrono::steady_clock::now();

    site_run = gs1::assemble_site_run_state_from_state_sets(state, state_manager, site_run.site_world);

    return PipelineTimings {
        std::chrono::duration<double, std::milli>(device_started - plant_started).count(),
        std::chrono::duration<double, std::milli>(resolve_started - device_started).count(),
        std::chrono::duration<double, std::milli>(total_ended - resolve_started).count(),
        std::chrono::duration<double, std::milli>(total_ended - total_started).count()};
}

PipelineTimings average_timings(const PipelineTimings& totals, int iterations)
{
    const double divisor = iterations > 0 ? static_cast<double>(iterations) : 1.0;
    return PipelineTimings {
        totals.plant_ms / divisor,
        totals.device_ms / divisor,
        totals.resolve_ms / divisor,
        totals.total_ms / divisor};
}

void accumulate_timings(PipelineTimings& totals, const PipelineTimings& delta)
{
    totals.plant_ms += delta.plant_ms;
    totals.device_ms += delta.device_ms;
    totals.resolve_ms += delta.resolve_ms;
    totals.total_ms += delta.total_ms;
}

PipelineTimings measure_initial_pass_ms()
{
    gs1::CampaignState campaign {};
    gs1::GameMessageQueue message_queue {};
    PipelineTimings totals {};

    for (int iteration = 0; iteration < kInitialPassIterations; ++iteration)
    {
        auto site_run = make_site_run();
        seed_dense_cover(site_run);
        seed_perf_plants(site_run);
        message_queue.clear();

        accumulate_timings(totals, run_local_weather_pipeline(campaign, site_run, message_queue));
    }

    return average_timings(totals, kInitialPassIterations);
}

PipelineTimings measure_steady_state_pass_ms()
{
    gs1::CampaignState campaign {};
    gs1::GameMessageQueue message_queue {};
    auto site_run = make_site_run();
    seed_dense_cover(site_run);
    seed_perf_plants(site_run);

    run_local_weather_pipeline(campaign, site_run, message_queue);

    PipelineTimings totals {};
    for (int iteration = 0; iteration < kSteadyStateIterations; ++iteration)
    {
        accumulate_timings(totals, run_local_weather_pipeline(campaign, site_run, message_queue));
    }

    return average_timings(totals, kSteadyStateIterations);
}

void print_stage_timings(const char* label, int iterations, const PipelineTimings& timings)
{
    std::cout << label << " avg ms (" << iterations << " runs): " << timings.total_ms << "\n";
    std::cout << "  plant contribution: " << timings.plant_ms << "\n";
    std::cout << "  device contribution: " << timings.device_ms << "\n";
    std::cout << "  local weather resolve: " << timings.resolve_ms << "\n";
}
}  // namespace

int main()
{
    const PipelineTimings initial_pass = measure_initial_pass_ms();
    const PipelineTimings steady_state_pass = measure_steady_state_pass_ms();
    constexpr double kFrameBudgetMs = 1000.0 / 60.0;

    std::cout << std::fixed << std::setprecision(3);
    std::cout << "Local weather perf probe\n";
    std::cout << "grid: " << kSiteWidth << "x" << kSiteHeight << " (" << kTileCount << " tiles)\n";
    std::cout << "pipeline: plant contribution -> device contribution -> local weather resolve\n";
    std::cout << "seeded_plants: " << std::size(kPerfSeededPlants) << " mixed authored placements\n";
    print_stage_timings("initial pass", kInitialPassIterations, initial_pass);
    print_stage_timings("steady-state", kSteadyStateIterations, steady_state_pass);
    std::cout << "steady-state share of 60 FPS frame budget: "
              << ((steady_state_pass.total_ms / kFrameBudgetMs) * 100.0) << "%\n";
    return 0;
}
