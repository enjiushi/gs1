#include "site/systems/local_weather_resolve_system.h"

#include "site/site_run_state.h"
#include "site/weather_contribution_logic.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace
{
constexpr std::uint32_t k_site_tile_state_changed_local_weather = 1U << 0U;
constexpr std::uint32_t k_site_tile_state_changed_support = 1U << 1U;

struct BaseLocalWeather final
{
    float heat {0.0f};
    float wind {0.0f};
    float dust {0.0f};
};

void ensure_local_weather_runtime_buffers(
    gs1::LocalWeatherResolveState& state,
    std::size_t tile_count)
{
    if (state.last_total_weather_contributions.size() == tile_count)
    {
        return;
    }

    state.last_total_weather_contributions.assign(tile_count, gs1::zero_weather_contribution());
}

bool ecology_has_projected_plant_visual(const gs1::SiteWorld::TileEcologyData& ecology) noexcept
{
    return ecology.plant_id.value != 0U ||
        ecology.plant_density > gs1::k_weather_contribution_density_epsilon_raw;
}

void emit_site_tile_state_changed(
    gs1::SiteSystemContext<gs1::LocalWeatherResolveSystem>& context,
    gs1::TileCoord coord,
    const gs1::SiteWorld::TileData& tile,
    const gs1::SiteWorld::TileWeatherContributionData& total_contribution,
    bool local_weather_changed,
    bool support_changed)
{
    std::uint32_t changed_mask = 0U;
    if (local_weather_changed)
    {
        changed_mask |= k_site_tile_state_changed_local_weather;
    }
    if (support_changed)
    {
        changed_mask |= k_site_tile_state_changed_support;
    }
    if (changed_mask == 0U)
    {
        return;
    }

    gs1::GameMessage message {};
    message.type = gs1::GameMessageType::SiteTileStateChanged;
    message.set_payload(gs1::SiteTileStateChangedMessage {
        coord.x,
        coord.y,
        changed_mask,
        tile.ecology.plant_id.value,
        tile.ecology.plant_density,
        tile.ecology.moisture,
        tile.local_weather.heat,
        tile.local_weather.dust,
        total_contribution.wind_protection,
        total_contribution.heat_protection,
        total_contribution.dust_protection});
    context.message_queue.push_back(message);
}

gs1::SiteWorld::TileLocalWeatherData resolve_local_weather(
    const BaseLocalWeather& base_weather,
    const gs1::SiteWorld::TileWeatherContributionData& total_contribution) noexcept
{
    return gs1::SiteWorld::TileLocalWeatherData {
        std::clamp(base_weather.heat - total_contribution.heat_protection, 0.0f, 100.0f),
        std::clamp(base_weather.wind - total_contribution.wind_protection, 0.0f, 100.0f),
        std::clamp(base_weather.dust - total_contribution.dust_protection, 0.0f, 100.0f)};
}

BaseLocalWeather compute_base_local_weather(
    gs1::SiteSystemContext<gs1::LocalWeatherResolveSystem>& context)
{
    const auto& weather = context.world.read_weather();
    const auto& modifiers = context.world.read_modifier().resolved_channel_totals;
    return BaseLocalWeather {
        std::clamp(weather.weather_heat * (1.0f + modifiers.heat), 0.0f, 100.0f),
        std::clamp(weather.weather_wind * (1.0f + modifiers.wind), 0.0f, 100.0f),
        std::clamp(weather.weather_dust * (1.0f + modifiers.dust), 0.0f, 100.0f)};
}
}  // namespace

namespace gs1
{
bool LocalWeatherResolveSystem::subscribes_to(GameMessageType type) noexcept
{
    return type == GameMessageType::SiteRunStarted;
}

Gs1Status LocalWeatherResolveSystem::process_message(
    SiteSystemContext<LocalWeatherResolveSystem>& context,
    const GameMessage& message)
{
    if (!context.world.has_world())
    {
        return GS1_STATUS_OK;
    }

    if (message.type == GameMessageType::SiteRunStarted)
    {
        context.world.own_local_weather_runtime().emit_full_snapshot_on_next_run = true;
    }

    return GS1_STATUS_OK;
}

void LocalWeatherResolveSystem::run(SiteSystemContext<LocalWeatherResolveSystem>& context)
{
    if (!context.world.has_world())
    {
        return;
    }

    auto& runtime = context.world.own_local_weather_runtime();
    ensure_local_weather_runtime_buffers(runtime, context.world.tile_count());

    const bool emit_full_snapshot = runtime.emit_full_snapshot_on_next_run;
    runtime.emit_full_snapshot_on_next_run = false;

    const auto base_weather = compute_base_local_weather(context);
    const auto tile_count = context.world.tile_count();
    for (std::size_t index = 0U; index < tile_count; ++index)
    {
        auto tile = context.world.read_tile_at_index(index);
        const TileCoord coord = context.world.tile_coord(index);

        const auto total_contribution = clamp_weather_contribution(
            sum_weather_contributions(
                tile.plant_weather_contribution,
                tile.device_weather_contribution));
        const auto resolved_weather = resolve_local_weather(base_weather, total_contribution);
        const auto previous_total_contribution = runtime.last_total_weather_contributions[index];

        const bool heat_changed =
            std::fabs(tile.local_weather.heat - resolved_weather.heat) > k_weather_contribution_epsilon;
        const bool wind_changed =
            std::fabs(tile.local_weather.wind - resolved_weather.wind) > k_weather_contribution_epsilon;
        const bool dust_changed =
            std::fabs(tile.local_weather.dust - resolved_weather.dust) > k_weather_contribution_epsilon;
        const bool local_weather_changed = heat_changed || wind_changed || dust_changed;
        const bool support_changed =
            weather_contribution_changed(previous_total_contribution, total_contribution);

        if (local_weather_changed)
        {
            tile.local_weather = resolved_weather;
            context.world.write_tile_local_weather_at_index(index, tile.local_weather);
        }

        if ((emit_full_snapshot || local_weather_changed || support_changed))
        {
            if (local_weather_changed)
            {
                tile.local_weather = resolved_weather;
            }

            emit_site_tile_state_changed(
                context,
                coord,
                tile,
                total_contribution,
                emit_full_snapshot || local_weather_changed,
                emit_full_snapshot || support_changed);
        }

        if (wind_changed && ecology_has_projected_plant_visual(tile.ecology))
        {
            context.world.mark_tile_projection_dirty(coord);
        }

        runtime.last_total_weather_contributions[index] = total_contribution;
    }
}
}  // namespace gs1
