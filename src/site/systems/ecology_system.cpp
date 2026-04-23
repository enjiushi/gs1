#include "site/systems/ecology_system.h"

#include "content/defs/plant_defs.h"
#include "runtime/runtime_clock.h"
#include "site/site_projection_update_flags.h"
#include "site/site_run_state.h"
#include "site/tile_footprint.h"
#include "site/weather_contribution_logic.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace gs1
{
namespace
{
constexpr float k_density_epsilon = 0.0001f;
constexpr float k_density_epsilon_raw = 0.01f;
constexpr float k_meter_scale = 100.0f;
constexpr float k_inverse_meter_scale = 0.01f;
constexpr float k_moisture_gain_per_water_unit = 22.0f;
constexpr float k_fertility_to_moisture_cap_factor = 1.0f;
constexpr float k_fertility_to_moisture_cap_weight = 1.0f;
constexpr float k_fertility_to_moisture_cap_bias = 0.0f;
constexpr float k_salinity_to_fertility_cap_factor = 1.0f;
constexpr float k_salinity_to_fertility_cap_weight = 1.0f;
constexpr float k_salinity_to_fertility_cap_bias = 0.0f;
constexpr float k_moisture_factor = 0.02f;
constexpr float k_moisture_weight = 1.0f;
constexpr float k_moisture_bias = 0.0f;
constexpr float k_heat_to_moisture_factor = 0.00025f;
constexpr float k_heat_to_moisture_weight = 1.0f;
constexpr float k_heat_to_moisture_bias = 0.0f;
constexpr float k_wind_to_moisture_factor = 0.00018f;
constexpr float k_wind_to_moisture_weight = 1.0f;
constexpr float k_wind_to_moisture_bias = 0.0f;
constexpr float k_fertility_factor = 0.015f;
constexpr float k_fertility_weight = 1.0f;
constexpr float k_fertility_bias = 0.0f;
constexpr float k_wind_to_fertility_factor = 0.00008f;
constexpr float k_wind_to_fertility_weight = 1.0f;
constexpr float k_wind_to_fertility_bias = 0.0f;
constexpr float k_dust_to_fertility_factor = 0.00006f;
constexpr float k_dust_to_fertility_weight = 1.0f;
constexpr float k_dust_to_fertility_bias = 0.0f;
constexpr float k_salinity_factor = 0.012f;
constexpr float k_salinity_weight = 1.0f;
constexpr float k_salinity_bias = 0.0f;
constexpr float k_salinity_source = 0.0f;
constexpr float k_growth_relief_from_moisture = 0.55f;
constexpr float k_growth_relief_from_fertility = 0.35f;
constexpr float k_growth_pressure_heat_scale = 0.04f;
constexpr float k_growth_pressure_wind_scale = 0.04f;
constexpr float k_growth_pressure_dust_scale = 0.04f;
constexpr float k_growth_gain_scale = 0.0012f;
constexpr float k_growth_loss_scale = 0.0024f;
constexpr float k_salinity_cap_softening = 0.75f;
constexpr float k_resistance_density_influence = 0.35f;
constexpr float k_highway_cover_gain_wind_scale = 0.00028f;
constexpr float k_highway_cover_gain_dust_scale = 0.00045f;

bool has_tile_occupant(PlantId plant_id, std::uint32_t ground_cover_type_id) noexcept
{
    return plant_id.value != 0U || ground_cover_type_id != 0U;
}

bool is_tracked_living_plant(PlantId plant_id) noexcept
{
    return plant_id.value != 0U && plant_id.value != k_plant_straw_checkerboard;
}

float unit_from_raw_meter(float value) noexcept
{
    return std::clamp(value * k_inverse_meter_scale, 0.0f, 1.0f);
}

float raw_from_unit_meter(float value) noexcept
{
    return std::clamp(value, 0.0f, 1.0f) * k_meter_scale;
}

float raw_meter_clamp(float value) noexcept
{
    return std::clamp(value, 0.0f, k_meter_scale);
}

float raw_meter_from_legacy_input(float value) noexcept
{
    if (value > 0.0f && value <= 1.0f)
    {
        return raw_meter_clamp(value * k_meter_scale);
    }

    return raw_meter_clamp(value);
}

bool ecology_change_affects_visible_projection(std::uint32_t changed_mask) noexcept
{
    return (changed_mask &
        (TILE_ECOLOGY_CHANGED_OCCUPANCY |
            TILE_ECOLOGY_CHANGED_DENSITY |
            TILE_ECOLOGY_CHANGED_SAND_BURIAL)) != 0U;
}

const PlantDef& resolve_occupant_def(const SiteWorld::TileData& tile) noexcept
{
    if (tile.ecology.plant_id.value != 0U)
    {
        const auto* plant_def = find_plant_def(tile.ecology.plant_id);
        if (plant_def != nullptr)
        {
            return *plant_def;
        }
        return k_generic_living_plant_def;
    }

    return k_generic_ground_cover_def;
}

float resolve_density_scaled_resistance(float max_value, float density) noexcept
{
    const float clamped_density = unit_from_raw_meter(density);
    const float floor_scale = 1.0f - std::clamp(k_resistance_density_influence, 0.0f, 1.0f);
    const float min_value = max_value * floor_scale;
    return std::lerp(min_value, max_value, clamped_density);
}

float resolve_tunable_factor(float factor, float weight, float bias) noexcept
{
    return factor * weight + bias;
}

SiteWorld::TileWeatherContributionData total_weather_contribution(
    const SiteWorld::TileData& tile) noexcept
{
    return clamp_weather_contribution(
        sum_weather_contributions(
            tile.plant_weather_contribution,
            tile.device_weather_contribution));
}

float compute_salinity_density_cap(
    const SiteWorld::TileEcologyData& ecology,
    const PlantDef& plant_def,
    const ModifierChannelTotals& modifiers) noexcept
{
    const float density = ecology.plant_density;
    const float effective_salt_tolerance =
        resolve_density_scaled_resistance(plant_def.salt_tolerance, density);
    const float salinity_penalty =
        unit_from_raw_meter(ecology.soil_salinity) *
        std::max(0.0f, 1.0f - (effective_salt_tolerance * 0.01f));
    float salinity_cap = 1.0f - (salinity_penalty * k_salinity_cap_softening);
    salinity_cap *= 1.0f + (modifiers.salinity_density_cap * 0.35f);
    return raw_from_unit_meter(std::clamp(salinity_cap, 0.15f, 1.0f));
}

float compute_growth_pressure(
    const SiteWorld::TileData& tile,
    const PlantDef& plant_def,
    const ModifierChannelTotals& modifiers,
    float effective_wind) noexcept
{
    if (!has_tile_occupant(tile.ecology.plant_id, tile.ecology.ground_cover_type_id))
    {
        return 0.0f;
    }

    const float density = tile.ecology.plant_density;
    const float effective_heat_tolerance =
        resolve_density_scaled_resistance(plant_def.heat_tolerance, density);
    const float effective_wind_resistance =
        resolve_density_scaled_resistance(plant_def.wind_resistance, density);
    const float effective_dust_tolerance =
        resolve_density_scaled_resistance(plant_def.dust_tolerance, density);
    const float effective_salt_tolerance =
        resolve_density_scaled_resistance(plant_def.salt_tolerance, density);
    const float heat_term = std::clamp(
        (tile.local_weather.heat * k_growth_pressure_heat_scale) -
            (effective_heat_tolerance * 0.01f),
        0.0f,
        1.0f);
    const float wind_term = std::clamp(
        (effective_wind * k_growth_pressure_wind_scale) -
            (effective_wind_resistance * 0.01f),
        0.0f,
        1.0f);
    const float dust_term = std::clamp(
        ((tile.local_weather.dust + unit_from_raw_meter(tile.ecology.sand_burial) * 12.0f) * k_growth_pressure_dust_scale) -
            (effective_dust_tolerance * 0.01f),
        0.0f,
        1.0f);
    const float salinity_term =
        unit_from_raw_meter(tile.ecology.soil_salinity) *
        std::max(0.0f, 1.0f - (effective_salt_tolerance * 0.01f)) *
        0.45f;
    const float moisture_relief =
        unit_from_raw_meter(tile.ecology.moisture) * k_growth_relief_from_moisture;
    const float fertility_relief =
        unit_from_raw_meter(tile.ecology.soil_fertility) * k_growth_relief_from_fertility;

    float pressure =
        0.2f +
        heat_term * 0.9f +
        wind_term * 0.75f +
        dust_term * 0.65f +
        unit_from_raw_meter(tile.ecology.sand_burial) * 0.55f +
        salinity_term -
        moisture_relief -
        fertility_relief;
    pressure += modifiers.growth_pressure * 0.35f;
    return raw_from_unit_meter(std::clamp(pressure, 0.0f, 1.0f));
}

float compute_next_moisture(
    const SiteWorld::TileData& tile,
    const TerrainFactorModifierState& factor_modifiers,
    float simulation_dt_minutes) noexcept
{
    const auto contribution = total_weather_contribution(tile);
    const float fertility_to_moisture_cap = resolve_tunable_factor(
        k_fertility_to_moisture_cap_factor,
        factor_modifiers.fertility_to_moisture_cap_weight,
        factor_modifiers.fertility_to_moisture_cap_bias);
    const float moisture_factor = resolve_tunable_factor(
        k_moisture_factor,
        factor_modifiers.moisture_weight,
        factor_modifiers.moisture_bias);
    const float heat_to_moisture_factor = resolve_tunable_factor(
        k_heat_to_moisture_factor,
        factor_modifiers.heat_to_moisture_weight,
        factor_modifiers.heat_to_moisture_bias);
    const float wind_to_moisture_factor = resolve_tunable_factor(
        k_wind_to_moisture_factor,
        factor_modifiers.wind_to_moisture_weight,
        factor_modifiers.wind_to_moisture_bias);
    const float moisture_top = std::clamp(
        tile.ecology.soil_fertility * fertility_to_moisture_cap,
        0.0f,
        k_meter_scale);
    const float moisture_rate =
        moisture_factor *
        (contribution.irrigation -
            tile.local_weather.heat * heat_to_moisture_factor -
            tile.local_weather.wind * wind_to_moisture_factor);
    return raw_meter_clamp(std::clamp(
        tile.ecology.moisture + (moisture_rate * simulation_dt_minutes),
        0.0f,
        moisture_top));
}

float compute_next_fertility(
    const SiteWorld::TileData& tile,
    const TerrainFactorModifierState& factor_modifiers,
    float simulation_dt_minutes) noexcept
{
    const auto contribution = total_weather_contribution(tile);
    const float salinity_to_fertility_cap = resolve_tunable_factor(
        k_salinity_to_fertility_cap_factor,
        factor_modifiers.salinity_to_fertility_cap_weight,
        factor_modifiers.salinity_to_fertility_cap_bias);
    const float fertility_factor = resolve_tunable_factor(
        k_fertility_factor,
        factor_modifiers.fertility_weight,
        factor_modifiers.fertility_bias);
    const float wind_to_fertility_factor = resolve_tunable_factor(
        k_wind_to_fertility_factor,
        factor_modifiers.wind_to_fertility_weight,
        factor_modifiers.wind_to_fertility_bias);
    const float dust_to_fertility_factor = resolve_tunable_factor(
        k_dust_to_fertility_factor,
        factor_modifiers.dust_to_fertility_weight,
        factor_modifiers.dust_to_fertility_bias);
    const float fertility_top = std::clamp(
        k_meter_scale - (tile.ecology.soil_salinity * salinity_to_fertility_cap),
        0.0f,
        k_meter_scale);
    const float fertility_rate =
        fertility_factor *
        (contribution.fertility_improve -
            tile.local_weather.wind * wind_to_fertility_factor -
            tile.local_weather.dust * dust_to_fertility_factor);
    return raw_meter_clamp(std::clamp(
        tile.ecology.soil_fertility + (fertility_rate * simulation_dt_minutes),
        0.0f,
        fertility_top));
}

float compute_next_salinity(
    const SiteWorld::TileData& tile,
    const TerrainFactorModifierState& factor_modifiers,
    float simulation_dt_minutes) noexcept
{
    const auto contribution = total_weather_contribution(tile);
    const float salinity_source_factor = resolve_tunable_factor(
        k_salinity_factor,
        factor_modifiers.salinity_source_weight,
        factor_modifiers.salinity_source_bias);
    const float salinity_reduction_factor = resolve_tunable_factor(
        1.0f,
        factor_modifiers.salinity_reduction_weight,
        factor_modifiers.salinity_reduction_bias);
    const float salinity_rate =
        (k_salinity_source * salinity_source_factor) -
        (contribution.salinity_reduction * salinity_reduction_factor);
    return raw_meter_clamp(std::clamp(
        tile.ecology.soil_salinity + (salinity_rate * simulation_dt_minutes),
        0.0f,
        k_meter_scale));
}

float compute_density_delta(
    const SiteWorld::TileData& tile,
    const PlantDef& plant_def,
    const ModifierChannelTotals& modifiers,
    float growth_pressure,
    float salinity_cap,
    float simulation_dt_minutes) noexcept
{
    const float density = unit_from_raw_meter(tile.ecology.plant_density);
    const float growth_pressure_unit = unit_from_raw_meter(growth_pressure);
    const float salinity_cap_unit = unit_from_raw_meter(salinity_cap);
    const float delta_minutes = std::max(simulation_dt_minutes, 0.0f);

    float density_gain = 0.0f;
    if (plant_def.growable && growth_pressure_unit < 0.45f && density < 1.0f - k_density_epsilon)
    {
        const float growth_headroom = (0.45f - growth_pressure_unit) / 0.45f;
        const float establishment_bonus = density < 0.3f ? 1.15f : 1.0f;
        density_gain =
            growth_headroom *
            (k_growth_gain_scale +
                unit_from_raw_meter(tile.ecology.moisture) * 0.0008f +
                unit_from_raw_meter(tile.ecology.soil_fertility) * 0.0006f) *
            establishment_bonus *
            delta_minutes;
    }

    const float fragility = 1.25f - density * 0.5f;
    float density_loss = 0.0f;
    if (growth_pressure_unit > 0.55f)
    {
        density_loss +=
            ((growth_pressure_unit - 0.55f) / 0.45f) *
            k_growth_loss_scale *
            fragility *
            delta_minutes;
    }

    if (density > salinity_cap_unit + k_density_epsilon)
    {
        density_loss += (density - salinity_cap_unit) * 1.8f * delta_minutes;
    }

    if (plant_def.constant_wither_rate > 0.0f)
    {
        density_loss +=
            (plant_def.constant_wither_rate / 100.0f) * 0.08f * delta_minutes;
    }

    float net_delta = density_gain - density_loss;
    net_delta *= 1.0f + (modifiers.plant_density * 0.35f);
    return net_delta * k_meter_scale;
}

float compute_effective_wind_exposure(
    SiteWorldAccess<EcologySystem>& world,
    TileCoord coord,
    const SiteWorld::TileData& tile) noexcept
{
    if (tile.ecology.plant_id.value == 0U)
    {
        return tile.local_weather.wind;
    }

    const TileFootprint footprint = resolve_plant_tile_footprint(tile.ecology.plant_id);
    const TileCoord anchor = align_tile_anchor_to_footprint(coord, footprint);

    float total_wind = 0.0f;
    std::uint32_t sample_count = 0U;
    for_each_tile_in_footprint(
        anchor,
        footprint,
        [&](TileCoord footprint_coord) {
            if (!world.tile_coord_in_bounds(footprint_coord))
            {
                return;
            }

            const auto footprint_tile = world.read_tile(footprint_coord);
            if (footprint_tile.ecology.plant_id != tile.ecology.plant_id)
            {
                return;
            }

            total_wind += footprint_tile.local_weather.wind;
            ++sample_count;
        });

    return sample_count == 0U
        ? tile.local_weather.wind
        : (total_wind / static_cast<float>(sample_count));
}

void emit_tile_ecology_changed(
    SiteSystemContext<EcologySystem>& context,
    TileCoord coord,
    std::uint32_t changed_mask)
{
    if (changed_mask == TILE_ECOLOGY_CHANGED_NONE)
    {
        return;
    }

    if (!context.world.tile_coord_in_bounds(coord))
    {
        return;
    }

    const auto tile = context.world.read_tile(coord);
    GameMessage message {};
    message.type = GameMessageType::TileEcologyChanged;
    message.set_payload(TileEcologyChangedMessage {
        coord.x,
        coord.y,
        changed_mask,
        tile.ecology.plant_id.value,
        tile.ecology.ground_cover_type_id,
        tile.ecology.plant_density,
        tile.ecology.sand_burial});
    context.message_queue.push_back(message);

    if (ecology_change_affects_visible_projection(changed_mask))
    {
        context.world.mark_tile_projection_dirty(coord);
    }
}

std::uint32_t apply_ground_cover(
    SiteWorldAccess<EcologySystem>& world,
    TileCoord coord,
    const SiteGroundCoverPlacedMessage& payload)
{
    if (!world.tile_coord_in_bounds(coord))
    {
        return TILE_ECOLOGY_CHANGED_NONE;
    }

    auto tile = world.read_tile(coord);
    std::uint32_t changed_mask = TILE_ECOLOGY_CHANGED_NONE;
    bool modified = false;
    bool occupancy_changed = false;

    if (tile.ecology.ground_cover_type_id != payload.ground_cover_type_id)
    {
        tile.ecology.ground_cover_type_id = payload.ground_cover_type_id;
        occupancy_changed = true;
        modified = true;
    }

    if (tile.ecology.plant_id.value != 0U)
    {
        tile.ecology.plant_id = PlantId {};
        occupancy_changed = true;
        modified = true;
    }

    const float target_density = raw_meter_from_legacy_input(payload.initial_density);
    if (std::fabs(tile.ecology.plant_density - target_density) > k_density_epsilon_raw)
    {
        tile.ecology.plant_density = target_density;
        changed_mask |= TILE_ECOLOGY_CHANGED_DENSITY;
        modified = true;
    }

    if (std::fabs(tile.ecology.growth_pressure) > k_density_epsilon_raw)
    {
        tile.ecology.growth_pressure = 0.0f;
        changed_mask |= TILE_ECOLOGY_CHANGED_GROWTH_PRESSURE;
        modified = true;
    }

    if (occupancy_changed)
    {
        changed_mask |= TILE_ECOLOGY_CHANGED_OCCUPANCY;
    }

    if (modified)
    {
        world.write_tile(coord, tile);
    }

    return changed_mask;
}

std::uint32_t apply_planting(
    SiteWorldAccess<EcologySystem>& world,
    TileCoord coord,
    const SiteTilePlantingCompletedMessage& payload)
{
    if (!world.tile_coord_in_bounds(coord))
    {
        return TILE_ECOLOGY_CHANGED_NONE;
    }

    auto tile = world.read_tile(coord);
    std::uint32_t changed_mask = TILE_ECOLOGY_CHANGED_NONE;
    bool modified = false;
    bool occupancy_changed = false;

    const PlantId requested_plant {payload.plant_type_id};
    if (tile.ecology.plant_id.value != requested_plant.value)
    {
        tile.ecology.plant_id = requested_plant;
        occupancy_changed = true;
        modified = true;
    }

    if (tile.ecology.ground_cover_type_id != 0U)
    {
        tile.ecology.ground_cover_type_id = 0U;
        occupancy_changed = true;
        modified = true;
    }

    const float target_density = raw_meter_from_legacy_input(payload.initial_density);
    if (std::fabs(tile.ecology.plant_density - target_density) > k_density_epsilon_raw)
    {
        tile.ecology.plant_density = target_density;
        changed_mask |= TILE_ECOLOGY_CHANGED_DENSITY;
        modified = true;
    }

    if (std::fabs(tile.ecology.growth_pressure) > k_density_epsilon_raw)
    {
        tile.ecology.growth_pressure = 0.0f;
        changed_mask |= TILE_ECOLOGY_CHANGED_GROWTH_PRESSURE;
        modified = true;
    }

    if (occupancy_changed)
    {
        changed_mask |= TILE_ECOLOGY_CHANGED_OCCUPANCY;
    }

    if (modified)
    {
        world.write_tile(coord, tile);
    }

    return changed_mask;
}

std::uint32_t apply_watering(
    SiteWorldAccess<EcologySystem>& world,
    TileCoord coord,
    const SiteTileWateredMessage& payload)
{
    if (!world.tile_coord_in_bounds(coord))
    {
        return TILE_ECOLOGY_CHANGED_NONE;
    }

    const float amount = std::max(0.0f, payload.water_amount);
    if (amount <= 0.0f)
    {
        return TILE_ECOLOGY_CHANGED_NONE;
    }

    auto tile = world.read_tile(coord);
    if (!has_tile_occupant(tile.ecology.plant_id, tile.ecology.ground_cover_type_id))
    {
        return TILE_ECOLOGY_CHANGED_NONE;
    }

    const float next_moisture = std::clamp(
        tile.ecology.moisture + amount * k_moisture_gain_per_water_unit,
        0.0f,
        k_meter_scale);
    if (std::fabs(next_moisture - tile.ecology.moisture) <= k_density_epsilon_raw)
    {
        return TILE_ECOLOGY_CHANGED_NONE;
    }

    tile.ecology.moisture = next_moisture;
    world.write_tile(coord, tile);
    return TILE_ECOLOGY_CHANGED_MOISTURE;
}

std::uint32_t apply_burial_cleared(
    SiteSystemContext<EcologySystem>& context,
    TileCoord coord,
    const SiteTileBurialClearedMessage& payload)
{
    auto& world = context.world;
    if (!world.tile_coord_in_bounds(coord))
    {
        return TILE_ECOLOGY_CHANGED_NONE;
    }

    auto tile = world.read_tile(coord);
    const float reduction = raw_meter_from_legacy_input(payload.cleared_amount);
    const auto tile_index = world.tile_index(coord);
    const auto& objective = world.read_objective();
    const bool is_highway_target =
        objective.type == SiteObjectiveType::HighwayProtection &&
        tile_index < objective.target_tile_mask.size() &&
        objective.target_tile_mask[tile_index] != 0U;
    if (is_highway_target)
    {
        const float next_cover = std::max(0.0f, tile.ecology.soil_fertility - reduction);
        if (std::fabs(tile.ecology.soil_fertility - next_cover) <= k_density_epsilon_raw)
        {
            return TILE_ECOLOGY_CHANGED_NONE;
        }

        tile.ecology.soil_fertility = next_cover;
        world.write_tile(coord, tile);
        return TILE_ECOLOGY_CHANGED_FERTILITY;
    }

    const float next_burial = std::max(0.0f, tile.ecology.sand_burial - reduction);
    if (std::fabs(tile.ecology.sand_burial - next_burial) <= k_density_epsilon_raw)
    {
        return TILE_ECOLOGY_CHANGED_NONE;
    }

    tile.ecology.sand_burial = next_burial;
    world.write_tile(coord, tile);
    return TILE_ECOLOGY_CHANGED_SAND_BURIAL;
}

std::uint32_t apply_harvest(
    SiteWorldAccess<EcologySystem>& world,
    TileCoord coord,
    const SiteTileHarvestedMessage& payload)
{
    if (!world.tile_coord_in_bounds(coord))
    {
        return TILE_ECOLOGY_CHANGED_NONE;
    }

    auto tile = world.read_tile(coord);
    if (tile.ecology.plant_id.value != payload.plant_type_id)
    {
        return TILE_ECOLOGY_CHANGED_NONE;
    }

    const float density_removed = raw_meter_from_legacy_input(payload.density_removed);
    if (density_removed <= k_density_epsilon_raw)
    {
        return TILE_ECOLOGY_CHANGED_NONE;
    }

    const float next_density = std::max(0.0f, tile.ecology.plant_density - density_removed);
    if (std::fabs(next_density - tile.ecology.plant_density) <= k_density_epsilon_raw)
    {
        return TILE_ECOLOGY_CHANGED_NONE;
    }

    tile.ecology.plant_density = next_density;
    std::uint32_t changed_mask = TILE_ECOLOGY_CHANGED_DENSITY;
    if (tile.ecology.plant_density <= k_density_epsilon_raw)
    {
        tile.ecology.plant_density = 0.0f;
        tile.ecology.plant_id = PlantId {};
        tile.ecology.ground_cover_type_id = 0U;
        tile.ecology.growth_pressure = 0.0f;
        changed_mask |= TILE_ECOLOGY_CHANGED_OCCUPANCY |
            TILE_ECOLOGY_CHANGED_GROWTH_PRESSURE;
    }

    world.write_tile(coord, tile);
    return changed_mask;
}

void update_restoration_progress(
    SiteSystemContext<EcologySystem>& context,
    std::uint32_t new_count)
{
    auto& counters = context.world.own_counters();
    const auto previous_count = counters.fully_grown_tile_count;
    if (previous_count == new_count)
    {
        return;
    }

    counters.fully_grown_tile_count = new_count;
    float normalized_progress = 0.0f;
    if (counters.site_completion_tile_threshold > 0U)
    {
        normalized_progress = static_cast<float>(new_count) /
            static_cast<float>(counters.site_completion_tile_threshold);
        normalized_progress = std::clamp(normalized_progress, 0.0f, 1.0f);
    }
    counters.objective_progress_normalized = normalized_progress;

    GameMessage progress_message {};
    progress_message.type = GameMessageType::RestorationProgressChanged;
    progress_message.set_payload(RestorationProgressChangedMessage {
        new_count,
        counters.site_completion_tile_threshold,
        normalized_progress});
    context.message_queue.push_back(progress_message);
    context.world.mark_projection_dirty(SITE_PROJECTION_UPDATE_HUD);
}

float compute_highway_sand_cover_delta(
    const SiteWorld::TileData& tile,
    float simulation_dt_minutes) noexcept
{
    return raw_from_unit_meter(
        (std::max(tile.local_weather.wind, 0.0f) * k_highway_cover_gain_wind_scale +
            std::max(tile.local_weather.dust, 0.0f) * k_highway_cover_gain_dust_scale) *
        std::max(simulation_dt_minutes, 0.0f));
}

void update_highway_protection_progress(
    SiteSystemContext<EcologySystem>& context,
    float average_sand_cover)
{
    auto& counters = context.world.own_counters();
    const auto& objective = context.world.read_objective();
    const float clamped_average = std::clamp(average_sand_cover, 0.0f, k_meter_scale);
    const float previous_average = counters.highway_average_sand_cover;
    float normalized_progress = 1.0f;
    const float cover_threshold =
        objective.highway_max_average_sand_cover > 0.0f &&
            objective.highway_max_average_sand_cover <= 1.0f
        ? objective.highway_max_average_sand_cover * k_meter_scale
        : objective.highway_max_average_sand_cover;
    if (cover_threshold > k_density_epsilon_raw)
    {
        normalized_progress =
            1.0f - (clamped_average / cover_threshold);
        normalized_progress = std::clamp(normalized_progress, 0.0f, 1.0f);
    }

    if (std::fabs(previous_average - clamped_average) <= k_density_epsilon &&
        std::fabs(counters.objective_progress_normalized - normalized_progress) <= k_density_epsilon)
    {
        return;
    }

    counters.highway_average_sand_cover = clamped_average;
    counters.objective_progress_normalized = normalized_progress;
    context.world.mark_projection_dirty(SITE_PROJECTION_UPDATE_HUD);
}

void update_living_plant_stability(
    SiteSystemContext<EcologySystem>& context,
    std::uint32_t tracked_living_plant_count,
    bool any_tracked_plant_withering)
{
    auto& counters = context.world.own_counters();
    const bool all_tracked_plants_stable =
        tracked_living_plant_count > 0U && !any_tracked_plant_withering;
    if (counters.tracked_living_plant_count == tracked_living_plant_count &&
        counters.all_tracked_living_plants_stable == all_tracked_plants_stable)
    {
        return;
    }

    counters.tracked_living_plant_count = tracked_living_plant_count;
    counters.all_tracked_living_plants_stable = all_tracked_plants_stable;

    std::uint32_t status_flags = LIVING_PLANT_STABILITY_NONE;
    if (all_tracked_plants_stable)
    {
        status_flags |= LIVING_PLANT_STABILITY_ALL_TRACKED_PLANTS_STABLE;
    }
    else if (tracked_living_plant_count > 0U && any_tracked_plant_withering)
    {
        status_flags |= LIVING_PLANT_STABILITY_ANY_TRACKED_PLANT_WITHERING;
    }

    GameMessage message {};
    message.type = GameMessageType::LivingPlantStabilityChanged;
    message.set_payload(LivingPlantStabilityChangedMessage {
        tracked_living_plant_count,
        status_flags});
    context.message_queue.push_back(message);
}

void emit_startup_ecology_snapshots(
    SiteSystemContext<EcologySystem>& context) noexcept
{
    if (!context.world.has_world())
    {
        return;
    }

    std::uint32_t fully_grown_count = 0U;
    std::uint32_t tracked_living_plant_count = 0U;
    const auto tile_count = context.world.tile_count();
    for (std::size_t index = 0U; index < tile_count; ++index)
    {
        const auto coord = context.world.tile_coord(index);
        const auto tile = context.world.read_tile_at_index(index);
        emit_tile_ecology_changed(
            context,
            coord,
            TILE_ECOLOGY_CHANGED_OCCUPANCY |
                TILE_ECOLOGY_CHANGED_DENSITY |
                TILE_ECOLOGY_CHANGED_SAND_BURIAL);
        if (has_tile_occupant(tile.ecology.plant_id, tile.ecology.ground_cover_type_id) &&
            tile.ecology.plant_density >= k_meter_scale - k_density_epsilon_raw)
        {
            ++fully_grown_count;
        }
        if (is_tracked_living_plant(tile.ecology.plant_id))
        {
            ++tracked_living_plant_count;
        }
    }

    update_living_plant_stability(context, tracked_living_plant_count, false);
    update_restoration_progress(context, fully_grown_count);
}
}  // namespace

bool EcologySystem::subscribes_to(GameMessageType type) noexcept
{
    switch (type)
    {
    case GameMessageType::SiteRunStarted:
    case GameMessageType::SiteGroundCoverPlaced:
    case GameMessageType::SiteTilePlantingCompleted:
    case GameMessageType::SiteTileWatered:
    case GameMessageType::SiteTileBurialCleared:
    case GameMessageType::SiteTileHarvested:
        return true;
    default:
        return false;
    }
}

Gs1Status EcologySystem::process_message(
    SiteSystemContext<EcologySystem>& context,
    const GameMessage& message)
{
    switch (message.type)
    {
    case GameMessageType::SiteRunStarted:
        emit_startup_ecology_snapshots(context);
        break;
    case GameMessageType::SiteGroundCoverPlaced:
    {
        const auto& payload = message.payload_as<SiteGroundCoverPlacedMessage>();
        const TileCoord coord {payload.target_tile_x, payload.target_tile_y};
        emit_tile_ecology_changed(
            context,
            coord,
            apply_ground_cover(context.world, coord, payload));
        break;
    }

    case GameMessageType::SiteTilePlantingCompleted:
    {
        const auto& payload = message.payload_as<SiteTilePlantingCompletedMessage>();
        const TileCoord coord {payload.target_tile_x, payload.target_tile_y};
        for_each_tile_in_footprint(
            coord,
            resolve_plant_tile_footprint(PlantId {payload.plant_type_id}),
            [&](TileCoord footprint_coord) {
                emit_tile_ecology_changed(
                    context,
                    footprint_coord,
                    apply_planting(context.world, footprint_coord, payload));
            });
        break;
    }

    case GameMessageType::SiteTileWatered:
    {
        const auto& payload = message.payload_as<SiteTileWateredMessage>();
        const TileCoord coord {payload.target_tile_x, payload.target_tile_y};
        emit_tile_ecology_changed(
            context,
            coord,
            apply_watering(context.world, coord, payload));
        break;
    }

    case GameMessageType::SiteTileBurialCleared:
    {
        const auto& payload = message.payload_as<SiteTileBurialClearedMessage>();
        const TileCoord coord {payload.target_tile_x, payload.target_tile_y};
        emit_tile_ecology_changed(
            context,
            coord,
            apply_burial_cleared(context, coord, payload));
        break;
    }

    case GameMessageType::SiteTileHarvested:
    {
        const auto& payload = message.payload_as<SiteTileHarvestedMessage>();
        const TileCoord coord {payload.target_tile_x, payload.target_tile_y};
        const TileFootprint footprint =
            resolve_plant_tile_footprint(PlantId {payload.plant_type_id});
        const TileCoord anchor = align_tile_anchor_to_footprint(coord, footprint);
        for_each_tile_in_footprint(
            anchor,
            footprint,
            [&](TileCoord footprint_coord) {
                emit_tile_ecology_changed(
                    context,
                    footprint_coord,
                    apply_harvest(context.world, footprint_coord, payload));
            });
        break;
    }

    default:
        break;
    }

    return GS1_STATUS_OK;
}

void EcologySystem::run(SiteSystemContext<EcologySystem>& context)
{
    if (!context.world.has_world())
    {
        return;
    }

    const auto& modifiers = context.world.read_modifier().resolved_channel_totals;
    const auto& terrain_factor_modifiers =
        context.world.read_modifier().resolved_terrain_factor_modifiers;
    const auto& objective = context.world.read_objective();
    const float simulation_dt_minutes = static_cast<float>(
        runtime_minutes_from_real_seconds(context.fixed_step_seconds));
    std::uint32_t fully_grown_count = 0U;
    std::uint32_t tracked_living_plant_count = 0U;
    bool any_tracked_plant_withering = false;
    float highway_cover_sum = 0.0f;
    std::uint32_t highway_tile_count = 0U;

    const auto tile_count = context.world.tile_count();
    for (std::size_t index = 0; index < tile_count; ++index)
    {
        auto tile = context.world.read_tile_at_index(index);
        const auto previous_tile = tile;
        const auto coord = context.world.tile_coord(index);
        const bool is_highway_target =
            objective.type == SiteObjectiveType::HighwayProtection &&
            index < objective.target_tile_mask.size() &&
            objective.target_tile_mask[index] != 0U;

        if (is_highway_target)
        {
            const float next_cover = std::clamp(
                tile.ecology.soil_fertility +
                    compute_highway_sand_cover_delta(tile, simulation_dt_minutes),
                0.0f,
                k_meter_scale);
            if (std::fabs(next_cover - tile.ecology.soil_fertility) > k_density_epsilon_raw)
            {
                tile.ecology.soil_fertility = next_cover;
                context.world.write_tile_at_index(index, tile);
                emit_tile_ecology_changed(context, coord, TILE_ECOLOGY_CHANGED_FERTILITY);
            }

            highway_cover_sum += tile.ecology.soil_fertility;
            highway_tile_count += 1U;
            continue;
        }

        if (!has_tile_occupant(tile.ecology.plant_id, tile.ecology.ground_cover_type_id))
        {
            if (std::fabs(tile.ecology.growth_pressure) > k_density_epsilon_raw)
            {
                tile.ecology.growth_pressure = 0.0f;
                context.world.write_tile_at_index(index, tile);
                emit_tile_ecology_changed(
                    context,
                    coord,
                    TILE_ECOLOGY_CHANGED_GROWTH_PRESSURE);
            }
            continue;
        }

        const auto& plant_def = resolve_occupant_def(tile);
        std::uint32_t changed_mask = TILE_ECOLOGY_CHANGED_NONE;
        const float effective_wind =
            compute_effective_wind_exposure(context.world, coord, tile);

        const float next_moisture =
            compute_next_moisture(tile, terrain_factor_modifiers, simulation_dt_minutes);
        if (std::fabs(next_moisture - tile.ecology.moisture) > k_density_epsilon_raw)
        {
            tile.ecology.moisture = next_moisture;
            changed_mask |= TILE_ECOLOGY_CHANGED_MOISTURE;
        }

        const float next_fertility =
            compute_next_fertility(
                tile,
                terrain_factor_modifiers,
                simulation_dt_minutes);
        if (std::fabs(next_fertility - tile.ecology.soil_fertility) > k_density_epsilon_raw)
        {
            tile.ecology.soil_fertility = next_fertility;
            changed_mask |= TILE_ECOLOGY_CHANGED_FERTILITY;
        }

        const float next_salinity =
            compute_next_salinity(tile, terrain_factor_modifiers, simulation_dt_minutes);
        if (std::fabs(next_salinity - tile.ecology.soil_salinity) > k_density_epsilon_raw)
        {
            tile.ecology.soil_salinity = next_salinity;
            changed_mask |= TILE_ECOLOGY_CHANGED_SALINITY;
        }

        const float next_growth_pressure =
            compute_growth_pressure(tile, plant_def, modifiers, effective_wind);
        if (std::fabs(next_growth_pressure - tile.ecology.growth_pressure) > k_density_epsilon_raw)
        {
            tile.ecology.growth_pressure = next_growth_pressure;
            changed_mask |= TILE_ECOLOGY_CHANGED_GROWTH_PRESSURE;
        }

        const float salinity_cap =
            compute_salinity_density_cap(tile.ecology, plant_def, modifiers);
        const float next_density = std::clamp(
            tile.ecology.plant_density +
                compute_density_delta(
                    tile,
                    plant_def,
                    modifiers,
                    next_growth_pressure,
                    salinity_cap,
                    simulation_dt_minutes),
            0.0f,
            k_meter_scale);
        if (std::fabs(next_density - tile.ecology.plant_density) > k_density_epsilon_raw)
        {
            tile.ecology.plant_density = next_density;
            changed_mask |= TILE_ECOLOGY_CHANGED_DENSITY;
        }

        if (tile.ecology.plant_density <= k_density_epsilon_raw &&
            has_tile_occupant(tile.ecology.plant_id, tile.ecology.ground_cover_type_id))
        {
            tile.ecology.plant_density = 0.0f;
            tile.ecology.plant_id = PlantId {};
            tile.ecology.ground_cover_type_id = 0U;
            tile.ecology.growth_pressure = 0.0f;
            changed_mask |= TILE_ECOLOGY_CHANGED_OCCUPANCY |
                TILE_ECOLOGY_CHANGED_DENSITY |
                TILE_ECOLOGY_CHANGED_GROWTH_PRESSURE;
        }

        if (changed_mask != TILE_ECOLOGY_CHANGED_NONE)
        {
            context.world.write_tile_at_index(index, tile);
            emit_tile_ecology_changed(context, coord, changed_mask);
        }

        if (is_tracked_living_plant(previous_tile.ecology.plant_id))
        {
            if (!is_tracked_living_plant(tile.ecology.plant_id) ||
                tile.ecology.plant_id != previous_tile.ecology.plant_id ||
                tile.ecology.plant_density < previous_tile.ecology.plant_density - k_density_epsilon_raw)
            {
                any_tracked_plant_withering = true;
            }
        }

        if (is_tracked_living_plant(tile.ecology.plant_id))
        {
            ++tracked_living_plant_count;
        }

        if (has_tile_occupant(tile.ecology.plant_id, tile.ecology.ground_cover_type_id) &&
            tile.ecology.plant_density >= k_meter_scale - k_density_epsilon_raw)
        {
            ++fully_grown_count;
        }
    }

    if (objective.type == SiteObjectiveType::HighwayProtection)
    {
        const float average_cover =
            highway_tile_count == 0U
            ? 0.0f
            : (highway_cover_sum / static_cast<float>(highway_tile_count));
        update_living_plant_stability(
            context,
            tracked_living_plant_count,
            any_tracked_plant_withering);
        update_highway_protection_progress(context, average_cover);
        return;
    }

    update_living_plant_stability(
        context,
        tracked_living_plant_count,
        any_tracked_plant_withering);
    update_restoration_progress(context, fully_grown_count);
}
}  // namespace gs1
