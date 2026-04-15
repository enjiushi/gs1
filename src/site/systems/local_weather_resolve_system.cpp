#include "site/systems/local_weather_resolve_system.h"

#include "content/defs/plant_defs.h"
#include "site/site_run_state.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
namespace
{
constexpr float k_local_weather_epsilon = 0.0001f;
constexpr int k_max_supported_plant_aura = 2;
constexpr float k_own_tile_contribution_scale = 0.04f;
constexpr float k_neighbor_contribution_scale = 0.018f;
constexpr float k_bare_tile_dust_bias = 0.9f;
constexpr float k_burial_dust_bias = 1.75f;

struct WeatherSupportMeters final
{
    float heat {0.0f};
    float wind {0.0f};
    float dust {0.0f};
};

const gs1::PlantDef& resolve_occupant_def(const gs1::SiteWorld::TileData& tile) noexcept
{
    if (tile.ecology.plant_id.value != 0U)
    {
        const auto* plant_def = gs1::find_plant_def(tile.ecology.plant_id);
        if (plant_def != nullptr)
        {
            return *plant_def;
        }
        return gs1::k_generic_living_plant_def;
    }

    return gs1::k_generic_ground_cover_def;
}

WeatherSupportMeters accumulate_weather_support(
    gs1::SiteSystemContext<gs1::LocalWeatherResolveSystem>& context,
    gs1::TileCoord target_coord)
{
    WeatherSupportMeters support {};

    for (int dy = -k_max_supported_plant_aura; dy <= k_max_supported_plant_aura; ++dy)
    {
        for (int dx = -k_max_supported_plant_aura; dx <= k_max_supported_plant_aura; ++dx)
        {
            const int manhattan_distance = std::abs(dx) + std::abs(dy);
            if (manhattan_distance > k_max_supported_plant_aura)
            {
                continue;
            }

            const gs1::TileCoord source_coord {target_coord.x + dx, target_coord.y + dy};
            if (!context.world.tile_coord_in_bounds(source_coord))
            {
                continue;
            }

            const auto source_tile = context.world.read_tile(source_coord);
            if (source_tile.ecology.plant_id.value == 0U &&
                source_tile.ecology.ground_cover_type_id == 0U)
            {
                continue;
            }

            const float density =
                std::clamp(source_tile.ecology.plant_density, 0.0f, 1.0f);
            if (density <= k_local_weather_epsilon)
            {
                continue;
            }

            const auto& plant_def = resolve_occupant_def(source_tile);
            if (manhattan_distance > 0 &&
                manhattan_distance > static_cast<int>(plant_def.aura_size))
            {
                continue;
            }

            const float contribution_scale =
                manhattan_distance == 0
                    ? k_own_tile_contribution_scale
                    : (k_neighbor_contribution_scale / static_cast<float>(manhattan_distance));
            support.heat += plant_def.heat_protection_power * density * contribution_scale;
            support.wind += plant_def.wind_protection_power * density * contribution_scale;
            support.dust += plant_def.dust_protection_power * density * contribution_scale;
        }
    }

    return support;
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
    if (!context.world.has_world())
    {
        return;
    }

    // Temporary regression isolation: disable local weather resolution.
    return;

    const auto& weather = context.world.read_weather();
    const auto& modifiers = context.world.read_modifier().resolved_channel_totals;
    const float base_heat =
        std::clamp(weather.weather_heat * (1.0f + modifiers.heat), 0.0f, 100.0f);
    const float base_wind =
        std::clamp(weather.weather_wind * (1.0f + modifiers.wind), 0.0f, 100.0f);
    const float base_dust =
        std::clamp(weather.weather_dust * (1.0f + modifiers.dust), 0.0f, 100.0f);

    const auto tile_count = context.world.tile_count();
    for (std::size_t index = 0; index < tile_count; ++index)
    {
        const auto coord = context.world.tile_coord(index);
        auto tile = context.world.read_tile_at_index(index);
        const bool has_cover =
            tile.ecology.plant_id.value != 0U || tile.ecology.ground_cover_type_id != 0U;
        const float cover_density = std::clamp(tile.ecology.plant_density, 0.0f, 1.0f);
        const auto support = accumulate_weather_support(context, coord);

        const float resolved_heat =
            std::clamp(base_heat - support.heat, 0.0f, 100.0f);
        const float resolved_wind =
            std::clamp(base_wind - support.wind, 0.0f, 100.0f);
        const float resolved_dust = std::clamp(
            base_dust - support.dust +
                ((1.0f - cover_density) * k_bare_tile_dust_bias) +
                (tile.ecology.sand_burial * k_burial_dust_bias) +
                (has_cover ? 0.0f : 0.35f),
            0.0f,
            100.0f);

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
