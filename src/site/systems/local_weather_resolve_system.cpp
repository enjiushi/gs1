#include "site/systems/local_weather_resolve_system.h"

#include "site/site_run_state.h"
#include "site/tile_grid_state.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace
{
constexpr float k_local_weather_epsilon = 0.0001f;
constexpr float k_cover_heat_modifier = -0.25f;
constexpr float k_cover_wind_modifier = -0.15f;
constexpr float k_cover_dust_modifier = -0.35f;
constexpr float k_exposed_dust_bias = 0.1f;

bool has_valid_tile_grid(const gs1::TileGridState& tile_grid) noexcept
{
    const auto tile_count = tile_grid.tile_count();
    return tile_count > 0U && tile_grid.width > 0U && tile_grid.height > 0U &&
        tile_grid.tile_heat.size() >= tile_count &&
        tile_grid.tile_wind.size() >= tile_count &&
        tile_grid.tile_dust.size() >= tile_count &&
        tile_grid.tile_plant_density.size() >= tile_count &&
        tile_grid.plant_type_ids.size() >= tile_count &&
        tile_grid.ground_cover_type_ids.size() >= tile_count;
}

gs1::TileCoord tile_coord_from_index(const gs1::TileGridState& tile_grid, std::size_t index) noexcept
{
    return gs1::TileCoord {
        static_cast<std::int32_t>(index % tile_grid.width),
        static_cast<std::int32_t>(index / tile_grid.width)};
}

}  // namespace

namespace gs1
{
bool LocalWeatherResolveSystem::subscribes_to(GameCommandType type) noexcept
{
    (void)type;
    return false;
}

Gs1Status LocalWeatherResolveSystem::process_command(
    SiteSystemContext<LocalWeatherResolveSystem>& context,
    const GameCommand& command)
{
    (void)context;
    (void)command;
    return GS1_STATUS_OK;
}

void LocalWeatherResolveSystem::run(SiteSystemContext<LocalWeatherResolveSystem>& context)
{
    auto& tile_grid = context.world.own_tile_grid();
    if (!has_valid_tile_grid(tile_grid))
    {
        return;
    }

    const auto tile_count = tile_grid.tile_count();
    if (tile_count == 0U)
    {
        return;
    }

    const auto& weather = context.world.read_weather();
    const float base_heat = weather.weather_heat;
    const float base_wind = weather.weather_wind;
    const float base_dust = weather.weather_dust;

    for (std::size_t index = 0; index < tile_count; ++index)
    {
        const float occupant_density = std::clamp(tile_grid.tile_plant_density[index], 0.0f, 1.0f);
        const bool has_cover = tile_grid.plant_type_ids[index].value != 0U ||
            tile_grid.ground_cover_type_ids[index] != 0U;
        const float cover_factor = has_cover ? 1.0f : 0.0f;

        const float resolved_heat = base_heat + (occupant_density * k_cover_heat_modifier);
        const float resolved_wind = base_wind + (cover_factor * k_cover_wind_modifier);
        const float resolved_dust =
            base_dust +
            (occupant_density * k_cover_dust_modifier) +
            ((1.0f - cover_factor) * k_exposed_dust_bias);

        const float previous_heat = tile_grid.tile_heat[index];
        const float previous_wind = tile_grid.tile_wind[index];
        const float previous_dust = tile_grid.tile_dust[index];

        const bool heat_changed = std::fabs(previous_heat - resolved_heat) > k_local_weather_epsilon;
        const bool wind_changed = std::fabs(previous_wind - resolved_wind) > k_local_weather_epsilon;
        const bool dust_changed = std::fabs(previous_dust - resolved_dust) > k_local_weather_epsilon;

        if (heat_changed)
        {
            tile_grid.tile_heat[index] = resolved_heat;
        }

        if (wind_changed)
        {
            tile_grid.tile_wind[index] = resolved_wind;
        }

        if (dust_changed)
        {
            tile_grid.tile_dust[index] = resolved_dust;
        }

        if (heat_changed || wind_changed || dust_changed)
        {
            (void)tile_coord_from_index(tile_grid, index);
        }
    }
}
}  // namespace gs1
