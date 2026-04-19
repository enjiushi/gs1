#include "site/systems/local_weather_resolve_system.h"

#include "content/defs/plant_defs.h"
#include "content/defs/structure_defs.h"
#include "site/site_run_state.h"
#include "site/tile_footprint.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace
{
constexpr float k_local_weather_epsilon = 0.0001f;
constexpr int k_max_supported_support_range = 2;
constexpr float k_own_tile_contribution_scale = 0.04f;
constexpr float k_neighbor_contribution_scale = 0.018f;
constexpr float k_bare_tile_dust_bias = 0.9f;
constexpr float k_burial_dust_bias = 1.75f;
constexpr std::uint32_t k_local_weather_full_refresh_tile_budget = 32U;
constexpr float k_wind_shadow_alignment_exponent = 2.0f;
constexpr float k_wind_shadow_falloff_exponent = 3.0f;
constexpr float k_wind_shadow_falloff_strength = 2.0f;

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
    float wind_direction_degrees {0.0f};
};

struct UnitVector final
{
    float x {1.0f};
    float y {0.0f};
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

[[nodiscard]] float resolve_contribution_scale(int manhattan_distance) noexcept
{
    return manhattan_distance == 0
        ? k_own_tile_contribution_scale
        : (k_neighbor_contribution_scale / static_cast<float>(manhattan_distance));
}

[[nodiscard]] float normalize_wind_direction_degrees(float value) noexcept
{
    const float wrapped = std::fmod(value, 360.0f);
    return wrapped < 0.0f ? wrapped + 360.0f : wrapped;
}

[[nodiscard]] UnitVector resolve_wind_direction_unit_vector(float wind_direction_degrees) noexcept
{
    const float radians =
        normalize_wind_direction_degrees(wind_direction_degrees) *
        (3.14159265358979323846f / 180.0f);
    return UnitVector {std::cos(radians), std::sin(radians)};
}

[[nodiscard]] float compute_directional_wind_shadow_scale(
    int source_x,
    int source_y,
    int target_x,
    int target_y,
    std::uint8_t protection_range,
    UnitVector wind_direction) noexcept
{
    if (protection_range == 0U)
    {
        return 0.0f;
    }

    const float offset_x = static_cast<float>(target_x - source_x);
    const float offset_y = static_cast<float>(target_y - source_y);
    const float distance =
        std::sqrt(offset_x * offset_x + offset_y * offset_y);
    if (distance <= k_local_weather_epsilon ||
        distance > static_cast<float>(protection_range) + k_local_weather_epsilon)
    {
        return 0.0f;
    }

    const float alignment =
        ((offset_x / distance) * wind_direction.x) +
        ((offset_y / distance) * wind_direction.y);
    if (alignment <= k_local_weather_epsilon)
    {
        return 0.0f;
    }

    const float alignment_scale =
        std::pow(std::clamp(alignment, 0.0f, 1.0f), k_wind_shadow_alignment_exponent);
    const float normalized_progress =
        protection_range <= 1U
        ? 0.0f
        : std::clamp(
              (distance - 1.0f) /
                  static_cast<float>(protection_range - 1U),
              0.0f,
              1.0f);
    const float distance_scale =
        std::exp(
            -k_wind_shadow_falloff_strength *
            std::pow(normalized_progress, k_wind_shadow_falloff_exponent));
    return alignment_scale * distance_scale;
}

[[nodiscard]] bool device_tile_is_support_anchor(
    gs1::TileCoord coord,
    gs1::StructureId structure_id) noexcept
{
    const auto anchor =
        align_tile_anchor_to_footprint(coord, resolve_structure_tile_footprint(structure_id));
    return anchor.x == coord.x && anchor.y == coord.y;
}

[[nodiscard]] bool structure_has_wind_protection(gs1::StructureId structure_id) noexcept
{
    const auto* structure_def = gs1::find_structure_def(structure_id);
    return structure_def != nullptr && structure_def->wind_protection_value > k_local_weather_epsilon;
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

void queue_dirty_structure_footprint_neighborhood(
    gs1::SiteSystemContext<gs1::LocalWeatherResolveSystem>& context,
    gs1::TileCoord anchor,
    gs1::StructureId structure_id)
{
    for_each_tile_in_footprint(
        anchor,
        resolve_structure_tile_footprint(structure_id),
        [&](gs1::TileCoord footprint_coord) {
            if (!context.world.tile_coord_in_bounds(footprint_coord))
            {
                return;
            }

            queue_dirty_neighborhood(context, footprint_coord);
        });
}

BaseLocalWeather compute_base_local_weather(
    gs1::SiteSystemContext<gs1::LocalWeatherResolveSystem>& context)
{
    const auto& weather = context.world.read_weather();
    const auto& modifiers = context.world.read_modifier().resolved_channel_totals;
    return BaseLocalWeather {
        std::clamp(weather.weather_heat * (1.0f + modifiers.heat), 0.0f, 100.0f),
        std::clamp(weather.weather_wind * (1.0f + modifiers.wind), 0.0f, 100.0f),
        std::clamp(weather.weather_dust * (1.0f + modifiers.dust), 0.0f, 100.0f),
        normalize_wind_direction_degrees(weather.weather_wind_direction_degrees)};
}

WeatherSupportMeters accumulate_weather_support(
    gs1::SiteSystemContext<gs1::LocalWeatherResolveSystem>& context,
    gs1::TileCoord target_coord,
    const BaseLocalWeather& base_weather)
{
    WeatherSupportMeters support {};
    const UnitVector wind_direction =
        resolve_wind_direction_unit_vector(base_weather.wind_direction_degrees);

    for (const auto sample : k_weather_support_samples)
    {
        if (sample.manhattan_distance > k_max_supported_support_range)
        {
            continue;
        }

        const gs1::TileCoord source_coord {target_coord.x + sample.dx, target_coord.y + sample.dy};
        if (!context.world.tile_coord_in_bounds(source_coord))
        {
            continue;
        }

        const auto source_tile = context.world.read_tile(source_coord);
        const auto& source_ecology = source_tile.ecology;
        const float contribution_scale = resolve_contribution_scale(sample.manhattan_distance);

        if (ecology_has_weather_support_source(source_ecology))
        {
            const float density = std::clamp(source_ecology.plant_density, 0.0f, 1.0f);
            if (density > k_local_weather_epsilon)
            {
                const auto& plant_def = resolve_occupant_def(source_ecology);
                const bool within_shared_aura =
                    sample.manhattan_distance == 0 ||
                    sample.manhattan_distance <= static_cast<int>(plant_def.aura_size);
                if (within_shared_aura)
                {
                    support.heat += plant_def.heat_protection_power * density * contribution_scale;
                    support.dust += plant_def.dust_protection_power * density * contribution_scale;
                }

                if (sample.manhattan_distance == 0)
                {
                    support.wind += plant_def.wind_protection_power * density * contribution_scale;
                }
                else if (sample.manhattan_distance <= static_cast<int>(plant_def.wind_protection_range))
                {
                    const float shadow_scale = compute_directional_wind_shadow_scale(
                        source_coord.x,
                        source_coord.y,
                        target_coord.x,
                        target_coord.y,
                        plant_def.wind_protection_range,
                        wind_direction);
                    if (shadow_scale > k_local_weather_epsilon)
                    {
                        support.wind +=
                            plant_def.wind_protection_power *
                            density *
                            contribution_scale *
                            shadow_scale;
                    }
                }
            }
        }

        const auto structure_id = source_tile.device.structure_id;
        if (structure_id.value == 0U ||
            !device_tile_is_support_anchor(source_coord, structure_id))
        {
            continue;
        }

        const auto* structure_def = gs1::find_structure_def(structure_id);
        if (structure_def == nullptr || structure_def->wind_protection_value <= k_local_weather_epsilon)
        {
            continue;
        }

        const float efficiency = std::clamp(source_tile.device.device_efficiency, 0.0f, 1.0f);
        if (efficiency <= k_local_weather_epsilon)
        {
            continue;
        }

        if (sample.manhattan_distance == 0)
        {
            support.wind += structure_def->wind_protection_value * efficiency * contribution_scale;
            continue;
        }

        if (sample.manhattan_distance > static_cast<int>(structure_def->wind_protection_range))
        {
            continue;
        }

        const float shadow_scale = compute_directional_wind_shadow_scale(
            source_coord.x,
            source_coord.y,
            target_coord.x,
            target_coord.y,
            structure_def->wind_protection_range,
            wind_direction);
        if (shadow_scale <= k_local_weather_epsilon)
        {
            continue;
        }

        support.wind +=
            structure_def->wind_protection_value *
            efficiency *
            contribution_scale *
            shadow_scale;
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
    const auto support = accumulate_weather_support(context, coord, base_weather);

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
    return type == GameMessageType::TileEcologyChanged ||
        type == GameMessageType::SiteDevicePlaced ||
        type == GameMessageType::SiteDeviceBroken ||
        type == GameMessageType::SiteDeviceRepaired;
}

Gs1Status LocalWeatherResolveSystem::process_message(
    SiteSystemContext<LocalWeatherResolveSystem>& context,
    const GameMessage& message)
{
    if (!context.world.has_world())
    {
        return GS1_STATUS_OK;
    }

    auto& runtime = context.world.own_local_weather_runtime();
    ensure_local_weather_runtime_buffers(runtime, context.world.tile_count());

    if (message.type == GameMessageType::TileEcologyChanged)
    {
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

    if (message.type == GameMessageType::SiteDevicePlaced)
    {
        const auto& payload = message.payload_as<SiteDevicePlacedMessage>();
        if (!structure_has_wind_protection(StructureId {payload.structure_id}))
        {
            return GS1_STATUS_OK;
        }

        const TileCoord anchor {payload.target_tile_x, payload.target_tile_y};
        if (!context.world.tile_coord_in_bounds(anchor))
        {
            return GS1_STATUS_OK;
        }

        queue_dirty_structure_footprint_neighborhood(context, anchor, StructureId {payload.structure_id});
        return GS1_STATUS_OK;
    }

    if (message.type == GameMessageType::SiteDeviceBroken)
    {
        const auto& payload = message.payload_as<SiteDeviceBrokenMessage>();
        if (!structure_has_wind_protection(StructureId {payload.structure_id}))
        {
            return GS1_STATUS_OK;
        }

        const TileCoord anchor {payload.target_tile_x, payload.target_tile_y};
        if (!context.world.tile_coord_in_bounds(anchor))
        {
            return GS1_STATUS_OK;
        }

        queue_dirty_structure_footprint_neighborhood(context, anchor, StructureId {payload.structure_id});
        return GS1_STATUS_OK;
    }

    if (message.type == GameMessageType::SiteDeviceRepaired)
    {
        const auto& payload = message.payload_as<SiteDeviceRepairedMessage>();
        const TileCoord anchor {payload.target_tile_x, payload.target_tile_y};
        if (!context.world.tile_coord_in_bounds(anchor))
        {
            return GS1_STATUS_OK;
        }

        const auto structure_id = context.world.read_tile(anchor).device.structure_id;
        if (!structure_has_wind_protection(structure_id))
        {
            return GS1_STATUS_OK;
        }

        queue_dirty_structure_footprint_neighborhood(context, anchor, structure_id);
    }

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
        std::fabs(runtime.last_base_dust - base_weather.dust) > k_local_weather_epsilon ||
        std::fabs(
            runtime.last_base_wind_direction_degrees - base_weather.wind_direction_degrees) >
            k_local_weather_epsilon;
    if (base_changed)
    {
        runtime.last_base_heat = base_weather.heat;
        runtime.last_base_wind = base_weather.wind;
        runtime.last_base_dust = base_weather.dust;
        runtime.last_base_wind_direction_degrees = base_weather.wind_direction_degrees;
        runtime.base_inputs_cached = true;
        schedule_full_refresh(runtime, tile_count);
    }

    process_dirty_tiles(context, base_weather);
    process_full_refresh_chunk(context, base_weather);
}
}  // namespace gs1
