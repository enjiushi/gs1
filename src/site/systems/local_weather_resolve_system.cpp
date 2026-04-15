#include "site/systems/local_weather_resolve_system.h"

#include "site/site_run_state.h"

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
    if (!context.world.has_world())
    {
        return;
    }

    const auto& weather = context.world.read_weather();
    const float base_heat = weather.weather_heat;
    const float base_wind = weather.weather_wind;
    const float base_dust = weather.weather_dust;

    const auto tile_count = context.world.tile_count();
    for (std::size_t index = 0; index < tile_count; ++index)
    {
        auto tile = context.world.read_tile_at_index(index);
        const float occupant_density = std::clamp(tile.ecology.plant_density, 0.0f, 1.0f);
        const bool has_cover =
            tile.ecology.plant_id.value != 0U || tile.ecology.ground_cover_type_id != 0U;
        const float cover_factor = has_cover ? 1.0f : 0.0f;

        const float resolved_heat = base_heat + (occupant_density * k_cover_heat_modifier);
        const float resolved_wind = base_wind + (cover_factor * k_cover_wind_modifier);
        const float resolved_dust =
            base_dust +
            (occupant_density * k_cover_dust_modifier) +
            ((1.0f - cover_factor) * k_exposed_dust_bias);

        const bool heat_changed =
            std::fabs(tile.local_weather.heat - resolved_heat) > k_local_weather_epsilon;
        const bool wind_changed =
            std::fabs(tile.local_weather.wind - resolved_wind) > k_local_weather_epsilon;
        const bool dust_changed =
            std::fabs(tile.local_weather.dust - resolved_dust) > k_local_weather_epsilon;
        if (!heat_changed && !wind_changed && !dust_changed)
        {
            continue;
        }

        tile.local_weather.heat = resolved_heat;
        tile.local_weather.wind = resolved_wind;
        tile.local_weather.dust = resolved_dust;
        context.world.write_tile_at_index(index, tile);
    }
}
}  // namespace gs1
