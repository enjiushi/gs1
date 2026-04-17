#include "campaign/campaign_state.h"
#include "messages/game_message.h"
#include "site/site_run_state.h"
#include "site/site_world.h"
#include "site/systems/local_weather_resolve_system.h"
#include "site/systems/site_system_context.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>

namespace
{
constexpr std::uint32_t kSiteWidth = 32U;
constexpr std::uint32_t kSiteHeight = 32U;
constexpr std::size_t kTileCount =
    static_cast<std::size_t>(kSiteWidth) * static_cast<std::size_t>(kSiteHeight);
constexpr std::size_t kManhattanRadiusTwoCellCount = 13U;
constexpr std::size_t kTileReadsPerTile = 1U + kManhattanRadiusTwoCellCount;
constexpr std::size_t kComponentGetsPerTileRead = 15U;
constexpr std::size_t kComponentSetsPerTileWrite = 15U;
constexpr double kFixedStepSeconds = 1.0 / 60.0;
constexpr int kInitialPassIterations = 200;
constexpr int kSteadyStateIterations = 1000;

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
    site_run.pending_tile_projection_update_mask.assign(site_run.site_world->tile_count(), 0U);
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

double measure_initial_pass_ms()
{
    gs1::CampaignState campaign {};
    gs1::GameMessageQueue message_queue {};
    double total_ms = 0.0;

    for (int iteration = 0; iteration < kInitialPassIterations; ++iteration)
    {
        auto site_run = make_site_run();
        seed_dense_cover(site_run);
        auto context = gs1::make_site_system_context<gs1::LocalWeatherResolveSystem>(
            campaign,
            site_run,
            message_queue,
            kFixedStepSeconds,
            {});
        message_queue.clear();

        const auto started = std::chrono::steady_clock::now();
        gs1::LocalWeatherResolveSystem::run(context);
        const auto ended = std::chrono::steady_clock::now();
        total_ms +=
            std::chrono::duration<double, std::milli>(ended - started).count();
    }

    return total_ms / static_cast<double>(kInitialPassIterations);
}

double measure_steady_state_pass_ms()
{
    gs1::CampaignState campaign {};
    gs1::GameMessageQueue message_queue {};
    auto site_run = make_site_run();
    seed_dense_cover(site_run);
    auto context = gs1::make_site_system_context<gs1::LocalWeatherResolveSystem>(
        campaign,
        site_run,
        message_queue,
        kFixedStepSeconds,
        {});

    gs1::LocalWeatherResolveSystem::run(context);

    const auto started = std::chrono::steady_clock::now();
    for (int iteration = 0; iteration < kSteadyStateIterations; ++iteration)
    {
        gs1::LocalWeatherResolveSystem::run(context);
    }
    const auto ended = std::chrono::steady_clock::now();

    return std::chrono::duration<double, std::milli>(ended - started).count() /
        static_cast<double>(kSteadyStateIterations);
}
}  // namespace

int main()
{
    const auto component_gets_per_pass =
        kTileCount * kTileReadsPerTile * kComponentGetsPerTileRead;
    const auto component_sets_per_full_write_pass =
        kTileCount * kComponentSetsPerTileWrite;
    const double initial_pass_ms = measure_initial_pass_ms();
    const double steady_state_pass_ms = measure_steady_state_pass_ms();
    constexpr double kFrameBudgetMs = 1000.0 / 60.0;

    std::cout << std::fixed << std::setprecision(3);
    std::cout << "Local weather perf probe\n";
    std::cout << "grid: " << kSiteWidth << "x" << kSiteHeight << " (" << kTileCount << " tiles)\n";
    std::cout << "estimated full-tile reads per pass: " << (kTileCount * kTileReadsPerTile) << "\n";
    std::cout << "estimated ECS component gets per pass: " << component_gets_per_pass << "\n";
    std::cout << "estimated ECS component sets on full rewrite pass: "
              << component_sets_per_full_write_pass << "\n";
    std::cout << "initial pass avg ms (" << kInitialPassIterations << " runs): "
              << initial_pass_ms << "\n";
    std::cout << "steady-state avg ms (" << kSteadyStateIterations << " runs): "
              << steady_state_pass_ms << "\n";
    std::cout << "steady-state share of 60 FPS frame budget: "
              << ((steady_state_pass_ms / kFrameBudgetMs) * 100.0) << "%\n";
    return 0;
}
