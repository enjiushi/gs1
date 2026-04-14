#include "site/systems/local_weather_resolve_system.h"

#include "site/site_run_state.h"
#include "site/site_world_access.h"

#include <algorithm>
#include <cmath>
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
    auto& site_run = context.site_run;
    if (!site_world_access::has_world(site_run))
    {
        return;
    }

    const auto& weather = context.world.read_weather();
    const float base_heat = weather.weather_heat;
    const float base_wind = weather.weather_wind;
    const float base_dust = weather.weather_dust;

    site_world_access::local_weather::for_each_tile_mut(
        site_run,
        [&](TileCoord,
            const site_ecs::TilePlantSlot& plant,
            const site_ecs::TileGroundCoverSlot& ground_cover,
            const site_ecs::TilePlantDensity& density,
            site_ecs::TileHeat& heat,
            site_ecs::TileWind& wind,
            site_ecs::TileDust& dust) {
            const float occupant_density = std::clamp(density.value, 0.0f, 1.0f);
            const bool has_cover =
                plant.plant_id.value != 0U || ground_cover.ground_cover_type_id != 0U;
            const float cover_factor = has_cover ? 1.0f : 0.0f;

            const float resolved_heat = base_heat + (occupant_density * k_cover_heat_modifier);
            const float resolved_wind = base_wind + (cover_factor * k_cover_wind_modifier);
            const float resolved_dust =
                base_dust +
                (occupant_density * k_cover_dust_modifier) +
                ((1.0f - cover_factor) * k_exposed_dust_bias);

            const bool heat_changed = std::fabs(heat.value - resolved_heat) > k_local_weather_epsilon;
            const bool wind_changed = std::fabs(wind.value - resolved_wind) > k_local_weather_epsilon;
            const bool dust_changed = std::fabs(dust.value - resolved_dust) > k_local_weather_epsilon;

            if (heat_changed)
            {
                heat.value = resolved_heat;
            }

            if (wind_changed)
            {
                wind.value = resolved_wind;
            }

            if (dust_changed)
            {
                dust.value = resolved_dust;
            }

            return heat_changed || wind_changed || dust_changed;
        });
}
}  // namespace gs1
