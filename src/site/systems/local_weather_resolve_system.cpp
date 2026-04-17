#include "site/systems/local_weather_resolve_system.h"

#include "content/defs/plant_defs.h"
#include "site/site_run_state.h"

#include <algorithm>
#include <array>
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
constexpr std::uint32_t k_local_weather_full_refresh_tile_budget = 32U;

struct WeatherSupportMeters final
{
    float heat {0.0f};
    float wind {0.0f};
    float dust {0.0f};
};

struct NeighborSample final
{
    int dx;
    int dy;
    int manhattan_distance;
};

struct BaseLocalWeather final
{
    float heat {0.0f};
    float wind {0.0f};
    float dust {0.0f};
};

constexpr std::array<NeighborSample, 13> k_weather_support_samples {{
    {0, 0, 0},
    {0, -1, 1},
    {-1, 0, 1},
    {1, 0, 1},
    {0, 1, 1},
    {0, -2, 2},
    {-1, -1, 2},
    {1, -1, 2},
    {-2, 0, 2},
    {2, 0, 2},
    {-1, 1, 2},
    {1, 1, 2},
    {0, 2, 2},
}};

const gs1::PlantDef& resolve_occupant_def(const gs1::SiteWorld::TileEcologyData& ecology) noexcept
{
    if (ecology.plant_id.value != 0U)
    {
        const auto* plant_def = gs1::find_plant_def(ecology.plant_id);
        if (plant_def != nullptr)
        {
            return *plant_def;
        }
        return gs1::k_generic_living_plant_def;
    }

    return gs1::k_generic_ground_cover_def;
}

bool ecology_has_weather_support_source(const gs1::SiteWorld::TileEcologyData& ecology) noexcept
{
    return ecology.plant_id.value != 0U || ecology.ground_cover_type_id != 0U;
}

bool ecology_change_affects_local_weather(std::uint32_t changed_mask) noexcept
{
    return (changed_mask &
        (gs1::TILE_ECOLOGY_CHANGED_OCCUPANCY |
            gs1::TILE_ECOLOGY_CHANGED_DENSITY |
            gs1::TILE_ECOLOGY_CHANGED_SAND_BURIAL)) != 0U;
}

void ensure_local_weather_runtime_buffers(
    gs1::LocalWeatherResolveState& state,
    std::size_t tile_count)
{
    if (state.pending_tile_mask.size() == tile_count)
    {
        return;
    }

    state.pending_tile_mask.assign(tile_count, 0U);
    state.pending_tile_indices.clear();
    state.next_full_refresh_tile_index = 0U;
    state.full_refresh_remaining_tiles = static_cast<std::uint32_t>(tile_count);
    state.base_inputs_cached = false;
}

void schedule_full_refresh(
    gs1::LocalWeatherResolveState& state,
    std::size_t tile_count)
{
    const auto clamped_tile_count = static_cast<std::uint32_t>(tile_count);
    state.full_refresh_remaining_tiles = clamped_tile_count;
    if (clamped_tile_count == 0U || state.next_full_refresh_tile_index >= clamped_tile_count)
    {
        state.next_full_refresh_tile_index = 0U;
    }
}

void queue_dirty_tile(gs1::LocalWeatherResolveState& state, std::uint32_t tile_index)
{
    if (tile_index >= state.pending_tile_mask.size() || state.pending_tile_mask[tile_index] != 0U)
    {
        return;
    }

    state.pending_tile_mask[tile_index] = 1U;
    state.pending_tile_indices.push_back(tile_index);
}

void queue_dirty_neighborhood(
    gs1::SiteSystemContext<gs1::LocalWeatherResolveSystem>& context,
    gs1::TileCoord center)
{
    auto& runtime = context.world.own_local_weather_runtime();
    for (const auto sample : k_weather_support_samples)
    {
        const gs1::TileCoord coord {center.x + sample.dx, center.y + sample.dy};
        if (!context.world.tile_coord_in_bounds(coord))
        {
            continue;
        }

        queue_dirty_tile(runtime, context.world.tile_index(coord));
    }
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

WeatherSupportMeters accumulate_weather_support(
    gs1::SiteSystemContext<gs1::LocalWeatherResolveSystem>& context,
    gs1::TileCoord target_coord)
{
    WeatherSupportMeters support {};

    for (const auto sample : k_weather_support_samples)
    {
        if (sample.manhattan_distance > k_max_supported_plant_aura)
        {
            continue;
        }

        const gs1::TileCoord source_coord {target_coord.x + sample.dx, target_coord.y + sample.dy};
        if (!context.world.tile_coord_in_bounds(source_coord))
        {
            continue;
        }

        const auto source_ecology = context.world.read_tile_ecology(source_coord);
        if (!ecology_has_weather_support_source(source_ecology))
        {
            continue;
        }

        const float density = std::clamp(source_ecology.plant_density, 0.0f, 1.0f);
        if (density <= k_local_weather_epsilon)
        {
            continue;
        }

        const auto& plant_def = resolve_occupant_def(source_ecology);
        if (sample.manhattan_distance > 0 &&
            sample.manhattan_distance > static_cast<int>(plant_def.aura_size))
        {
            continue;
        }

        const float contribution_scale =
            sample.manhattan_distance == 0
                ? k_own_tile_contribution_scale
                : (k_neighbor_contribution_scale / static_cast<float>(sample.manhattan_distance));
        support.heat += plant_def.heat_protection_power * density * contribution_scale;
        support.wind += plant_def.wind_protection_power * density * contribution_scale;
        support.dust += plant_def.dust_protection_power * density * contribution_scale;
    }

    return support;
}

bool resolve_tile_local_weather(
    gs1::SiteSystemContext<gs1::LocalWeatherResolveSystem>& context,
    std::size_t index,
    const BaseLocalWeather& base_weather)
{
    const auto coord = context.world.tile_coord(index);
    const auto ecology = context.world.read_tile_ecology_at_index(index);
    const auto current_weather = context.world.read_tile_local_weather_at_index(index);
    const bool has_cover = ecology_has_weather_support_source(ecology);
    const float cover_density = std::clamp(ecology.plant_density, 0.0f, 1.0f);
    const auto support = accumulate_weather_support(context, coord);

    const gs1::SiteWorld::TileLocalWeatherData resolved_weather {
        std::clamp(base_weather.heat - support.heat, 0.0f, 100.0f),
        std::clamp(base_weather.wind - support.wind, 0.0f, 100.0f),
        std::clamp(
            base_weather.dust - support.dust +
                ((1.0f - cover_density) * k_bare_tile_dust_bias) +
                (ecology.sand_burial * k_burial_dust_bias) +
                (has_cover ? 0.0f : 0.35f),
            0.0f,
            100.0f)};

    const bool heat_changed =
        std::fabs(current_weather.heat - resolved_weather.heat) > k_local_weather_epsilon;
    const bool wind_changed =
        std::fabs(current_weather.wind - resolved_weather.wind) > k_local_weather_epsilon;
    const bool dust_changed =
        std::fabs(current_weather.dust - resolved_weather.dust) > k_local_weather_epsilon;
    if (!heat_changed && !wind_changed && !dust_changed)
    {
        return false;
    }

    context.world.write_tile_local_weather_at_index(index, resolved_weather);
    return true;
}

void process_dirty_tiles(
    gs1::SiteSystemContext<gs1::LocalWeatherResolveSystem>& context,
    const BaseLocalWeather& base_weather)
{
    auto& runtime = context.world.own_local_weather_runtime();
    std::vector<std::uint32_t> pending_indices {};
    pending_indices.swap(runtime.pending_tile_indices);

    const auto tile_count = context.world.tile_count();
    for (const auto tile_index : pending_indices)
    {
        if (tile_index < runtime.pending_tile_mask.size())
        {
            runtime.pending_tile_mask[tile_index] = 0U;
        }

        if (tile_index >= tile_count)
        {
            continue;
        }

        (void)resolve_tile_local_weather(context, tile_index, base_weather);
    }
}

void process_full_refresh_chunk(
    gs1::SiteSystemContext<gs1::LocalWeatherResolveSystem>& context,
    const BaseLocalWeather& base_weather)
{
    auto& runtime = context.world.own_local_weather_runtime();
    const auto tile_count = static_cast<std::uint32_t>(context.world.tile_count());
    if (tile_count == 0U || runtime.full_refresh_remaining_tiles == 0U)
    {
        return;
    }

    const auto tiles_this_frame = std::min(
        k_local_weather_full_refresh_tile_budget,
        runtime.full_refresh_remaining_tiles);
    for (std::uint32_t processed = 0U; processed < tiles_this_frame; ++processed)
    {
        if (runtime.next_full_refresh_tile_index >= tile_count)
        {
            runtime.next_full_refresh_tile_index = 0U;
        }

        (void)resolve_tile_local_weather(context, runtime.next_full_refresh_tile_index, base_weather);
        runtime.next_full_refresh_tile_index =
            (runtime.next_full_refresh_tile_index + 1U) % tile_count;
        runtime.full_refresh_remaining_tiles -= 1U;
    }
}

}  // namespace

namespace gs1
{
bool LocalWeatherResolveSystem::subscribes_to(GameMessageType type) noexcept
{
    return type == GameMessageType::TileEcologyChanged;
}

Gs1Status LocalWeatherResolveSystem::process_message(
    SiteSystemContext<LocalWeatherResolveSystem>& context,
    const GameMessage& message)
{
    if (!context.world.has_world() || message.type != GameMessageType::TileEcologyChanged)
    {
        return GS1_STATUS_OK;
    }

    auto& runtime = context.world.own_local_weather_runtime();
    ensure_local_weather_runtime_buffers(runtime, context.world.tile_count());

    const auto& payload = message.payload_as<TileEcologyChangedMessage>();
    if (!ecology_change_affects_local_weather(payload.changed_mask))
    {
        return GS1_STATUS_OK;
    }

    const TileCoord center {payload.target_tile_x, payload.target_tile_y};
    if (!context.world.tile_coord_in_bounds(center))
    {
        return GS1_STATUS_OK;
    }

    queue_dirty_neighborhood(context, center);
    return GS1_STATUS_OK;
}

void LocalWeatherResolveSystem::run(SiteSystemContext<LocalWeatherResolveSystem>& context)
{
    if (!context.world.has_world())
    {
        return;
    }

    const auto tile_count = context.world.tile_count();
    auto& runtime = context.world.own_local_weather_runtime();
    ensure_local_weather_runtime_buffers(runtime, tile_count);

    const auto base_weather = compute_base_local_weather(context);
    const bool base_changed =
        !runtime.base_inputs_cached ||
        std::fabs(runtime.last_base_heat - base_weather.heat) > k_local_weather_epsilon ||
        std::fabs(runtime.last_base_wind - base_weather.wind) > k_local_weather_epsilon ||
        std::fabs(runtime.last_base_dust - base_weather.dust) > k_local_weather_epsilon;
    if (base_changed)
    {
        runtime.last_base_heat = base_weather.heat;
        runtime.last_base_wind = base_weather.wind;
        runtime.last_base_dust = base_weather.dust;
        runtime.base_inputs_cached = true;
        schedule_full_refresh(runtime, tile_count);
    }

    process_dirty_tiles(context, base_weather);
    process_full_refresh_chunk(context, base_weather);
}
}  // namespace gs1
