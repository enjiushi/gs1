#include "site/systems/ecology_system.h"

#include "content/defs/plant_defs.h"
#include "site/site_projection_update_flags.h"
#include "site/site_run_state.h"
#include "site/tile_footprint.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace gs1
{
namespace
{
constexpr float k_density_epsilon = 0.0001f;
constexpr float k_moisture_gain_per_water_unit = 0.22f;
constexpr float k_moisture_drain_heat_scale = 0.00025f;
constexpr float k_moisture_drain_wind_scale = 0.00018f;
constexpr float k_fertility_gain_scale = 0.00002f;
constexpr float k_fertility_loss_wind_scale = 0.00008f;
constexpr float k_fertility_loss_dust_scale = 0.00006f;
constexpr float k_fertility_loss_burial_scale = 0.0012f;
constexpr float k_salinity_reduction_scale = 0.00003f;
constexpr float k_growth_relief_from_moisture = 0.55f;
constexpr float k_growth_relief_from_fertility = 0.35f;
constexpr float k_growth_pressure_heat_scale = 0.04f;
constexpr float k_growth_pressure_wind_scale = 0.04f;
constexpr float k_growth_pressure_dust_scale = 0.04f;
constexpr float k_growth_gain_scale = 0.0012f;
constexpr float k_growth_loss_scale = 0.0024f;
constexpr float k_salinity_cap_softening = 0.75f;

bool has_tile_occupant(PlantId plant_id, std::uint32_t ground_cover_type_id) noexcept
{
    return plant_id.value != 0U || ground_cover_type_id != 0U;
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

float compute_salinity_density_cap(
    const SiteWorld::TileEcologyData& ecology,
    const PlantDef& plant_def,
    const ModifierChannelTotals& modifiers) noexcept
{
    const float salinity_penalty =
        ecology.soil_salinity *
        std::max(0.0f, 1.0f - (plant_def.salt_tolerance * 0.01f));
    float salinity_cap = 1.0f - (salinity_penalty * k_salinity_cap_softening);
    salinity_cap *= 1.0f + (modifiers.salinity_density_cap * 0.35f);
    return std::clamp(salinity_cap, 0.15f, 1.0f);
}

float compute_growth_pressure(
    const SiteWorld::TileData& tile,
    const PlantDef& plant_def,
    const ModifierChannelTotals& modifiers) noexcept
{
    if (!has_tile_occupant(tile.ecology.plant_id, tile.ecology.ground_cover_type_id))
    {
        return 0.0f;
    }

    const float heat_term = std::clamp(
        (tile.local_weather.heat * k_growth_pressure_heat_scale) -
            (plant_def.heat_tolerance * 0.01f),
        0.0f,
        1.0f);
    const float wind_term = std::clamp(
        (tile.local_weather.wind * k_growth_pressure_wind_scale) -
            (plant_def.wind_resistance * 0.01f),
        0.0f,
        1.0f);
    const float dust_term = std::clamp(
        ((tile.local_weather.dust + tile.ecology.sand_burial * 12.0f) * k_growth_pressure_dust_scale) -
            (plant_def.dust_tolerance * 0.01f),
        0.0f,
        1.0f);
    const float salinity_term =
        tile.ecology.soil_salinity *
        std::max(0.0f, 1.0f - (plant_def.salt_tolerance * 0.01f)) *
        0.45f;
    const float moisture_relief = tile.ecology.moisture * k_growth_relief_from_moisture;
    const float fertility_relief =
        tile.ecology.soil_fertility * k_growth_relief_from_fertility;

    float pressure =
        0.2f +
        heat_term * 0.9f +
        wind_term * 0.75f +
        dust_term * 0.65f +
        tile.ecology.sand_burial * 0.55f +
        salinity_term -
        moisture_relief -
        fertility_relief;
    pressure += modifiers.growth_pressure * 0.35f;
    return std::clamp(pressure, 0.0f, 1.0f);
}

float compute_next_moisture(
    const SiteWorld::TileData& tile,
    const ModifierChannelTotals& modifiers,
    double fixed_step_seconds) noexcept
{
    const float density = std::clamp(tile.ecology.plant_density, 0.0f, 1.0f);
    const float retention = density * 0.015f;
    const float moisture_drain =
        std::max(
            0.0f,
            (tile.local_weather.heat * k_moisture_drain_heat_scale +
                tile.local_weather.wind * k_moisture_drain_wind_scale) *
                    static_cast<float>(fixed_step_seconds) -
                retention);

    float next_moisture =
        tile.ecology.moisture - moisture_drain +
        (modifiers.moisture * 0.02f * static_cast<float>(fixed_step_seconds));
    return std::clamp(next_moisture, 0.0f, 1.0f);
}

float compute_next_fertility(
    const SiteWorld::TileData& tile,
    const PlantDef& plant_def,
    const ModifierChannelTotals& modifiers,
    double fixed_step_seconds) noexcept
{
    const float density = std::clamp(tile.ecology.plant_density, 0.0f, 1.0f);
    const float healthy_factor = std::clamp(1.0f - tile.ecology.growth_pressure, 0.0f, 1.0f);
    const float fertility_gain =
        plant_def.fertility_improve_power *
        density *
        healthy_factor *
        k_fertility_gain_scale *
        static_cast<float>(fixed_step_seconds);
    const float erosion_loss = std::max(
        0.0f,
        (tile.local_weather.wind * k_fertility_loss_wind_scale +
            tile.local_weather.dust * k_fertility_loss_dust_scale +
            tile.ecology.sand_burial * k_fertility_loss_burial_scale) *
                static_cast<float>(fixed_step_seconds) -
            fertility_gain * 0.4f);

    float next_fertility =
        tile.ecology.soil_fertility +
        fertility_gain -
        erosion_loss +
        (modifiers.fertility * 0.015f * static_cast<float>(fixed_step_seconds));
    return std::clamp(next_fertility, 0.0f, 1.0f);
}

float compute_next_salinity(
    const SiteWorld::TileData& tile,
    const PlantDef& plant_def,
    const ModifierChannelTotals& modifiers,
    double fixed_step_seconds) noexcept
{
    const float density = std::clamp(tile.ecology.plant_density, 0.0f, 1.0f);
    const float healthy_factor = std::clamp(1.0f - tile.ecology.growth_pressure, 0.0f, 1.0f);
    const float salinity_reduction =
        plant_def.salinity_reduction_power *
        density *
        healthy_factor *
        k_salinity_reduction_scale *
        static_cast<float>(fixed_step_seconds);

    float next_salinity =
        tile.ecology.soil_salinity -
        salinity_reduction -
        (modifiers.salinity * 0.012f * static_cast<float>(fixed_step_seconds));
    return std::clamp(next_salinity, 0.0f, 1.0f);
}

float compute_density_delta(
    const SiteWorld::TileData& tile,
    const PlantDef& plant_def,
    const ModifierChannelTotals& modifiers,
    float growth_pressure,
    float salinity_cap,
    double fixed_step_seconds) noexcept
{
    const float density = std::clamp(tile.ecology.plant_density, 0.0f, 1.0f);
    const float seconds = static_cast<float>(fixed_step_seconds);

    float density_gain = 0.0f;
    if (plant_def.growable && growth_pressure < 0.45f && density < 1.0f - k_density_epsilon)
    {
        const float growth_headroom = (0.45f - growth_pressure) / 0.45f;
        const float establishment_bonus = density < 0.3f ? 1.15f : 1.0f;
        density_gain =
            growth_headroom *
            (k_growth_gain_scale +
                tile.ecology.moisture * 0.0008f +
                tile.ecology.soil_fertility * 0.0006f) *
            establishment_bonus *
            seconds;
    }

    const float fragility = 1.25f - density * 0.5f;
    float density_loss = 0.0f;
    if (growth_pressure > 0.55f)
    {
        density_loss +=
            ((growth_pressure - 0.55f) / 0.45f) *
            k_growth_loss_scale *
            fragility *
            seconds;
    }

    if (density > salinity_cap + k_density_epsilon)
    {
        density_loss += (density - salinity_cap) * 1.8f * seconds;
    }

    if (plant_def.constant_wither_rate > 0.0f)
    {
        density_loss +=
            (plant_def.constant_wither_rate / 100.0f) * 0.08f * seconds;
    }

    float net_delta = density_gain - density_loss;
    net_delta *= 1.0f + (modifiers.plant_density * 0.35f);
    return net_delta;
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

    const float target_density = std::clamp(payload.initial_density, 0.0f, 1.0f);
    if (std::fabs(tile.ecology.plant_density - target_density) > k_density_epsilon)
    {
        tile.ecology.plant_density = target_density;
        changed_mask |= TILE_ECOLOGY_CHANGED_DENSITY;
        modified = true;
    }

    if (std::fabs(tile.ecology.growth_pressure) > k_density_epsilon)
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

    const float target_density = std::clamp(payload.initial_density, 0.0f, 1.0f);
    if (std::fabs(tile.ecology.plant_density - target_density) > k_density_epsilon)
    {
        tile.ecology.plant_density = target_density;
        changed_mask |= TILE_ECOLOGY_CHANGED_DENSITY;
        modified = true;
    }

    if (std::fabs(tile.ecology.growth_pressure) > k_density_epsilon)
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
        1.0f);
    if (std::fabs(next_moisture - tile.ecology.moisture) <= k_density_epsilon)
    {
        return TILE_ECOLOGY_CHANGED_NONE;
    }

    tile.ecology.moisture = next_moisture;
    world.write_tile(coord, tile);
    return TILE_ECOLOGY_CHANGED_MOISTURE;
}

std::uint32_t apply_burial_cleared(
    SiteWorldAccess<EcologySystem>& world,
    TileCoord coord,
    const SiteTileBurialClearedMessage& payload)
{
    if (!world.tile_coord_in_bounds(coord))
    {
        return TILE_ECOLOGY_CHANGED_NONE;
    }

    auto tile = world.read_tile(coord);
    const float reduction = std::max(0.0f, payload.cleared_amount);
    const float next_burial = std::max(0.0f, tile.ecology.sand_burial - reduction);
    if (std::fabs(tile.ecology.sand_burial - next_burial) <= k_density_epsilon)
    {
        return TILE_ECOLOGY_CHANGED_NONE;
    }

    tile.ecology.sand_burial = next_burial;
    world.write_tile(coord, tile);
    return TILE_ECOLOGY_CHANGED_SAND_BURIAL;
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

    GameMessage progress_message {};
    progress_message.type = GameMessageType::RestorationProgressChanged;
    progress_message.set_payload(RestorationProgressChangedMessage {
        new_count,
        counters.site_completion_tile_threshold,
        normalized_progress});
    context.message_queue.push_back(progress_message);
    context.world.mark_projection_dirty(SITE_PROJECTION_UPDATE_HUD);
}
}  // namespace

bool EcologySystem::subscribes_to(GameMessageType type) noexcept
{
    switch (type)
    {
    case GameMessageType::SiteGroundCoverPlaced:
    case GameMessageType::SiteTilePlantingCompleted:
    case GameMessageType::SiteTileWatered:
    case GameMessageType::SiteTileBurialCleared:
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
            apply_burial_cleared(context.world, coord, payload));
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
    std::uint32_t fully_grown_count = 0U;

    const auto tile_count = context.world.tile_count();
    for (std::size_t index = 0; index < tile_count; ++index)
    {
        auto tile = context.world.read_tile_at_index(index);
        const auto coord = context.world.tile_coord(index);

        if (!has_tile_occupant(tile.ecology.plant_id, tile.ecology.ground_cover_type_id))
        {
            if (std::fabs(tile.ecology.growth_pressure) > k_density_epsilon)
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

        const float next_moisture =
            compute_next_moisture(tile, modifiers, context.fixed_step_seconds);
        if (std::fabs(next_moisture - tile.ecology.moisture) > k_density_epsilon)
        {
            tile.ecology.moisture = next_moisture;
            changed_mask |= TILE_ECOLOGY_CHANGED_MOISTURE;
        }

        const float next_fertility =
            compute_next_fertility(tile, plant_def, modifiers, context.fixed_step_seconds);
        if (std::fabs(next_fertility - tile.ecology.soil_fertility) > k_density_epsilon)
        {
            tile.ecology.soil_fertility = next_fertility;
            changed_mask |= TILE_ECOLOGY_CHANGED_FERTILITY;
        }

        const float next_salinity =
            compute_next_salinity(tile, plant_def, modifiers, context.fixed_step_seconds);
        if (std::fabs(next_salinity - tile.ecology.soil_salinity) > k_density_epsilon)
        {
            tile.ecology.soil_salinity = next_salinity;
            changed_mask |= TILE_ECOLOGY_CHANGED_SALINITY;
        }

        const float next_growth_pressure =
            compute_growth_pressure(tile, plant_def, modifiers);
        if (std::fabs(next_growth_pressure - tile.ecology.growth_pressure) > k_density_epsilon)
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
                    context.fixed_step_seconds),
            0.0f,
            1.0f);
        if (std::fabs(next_density - tile.ecology.plant_density) > k_density_epsilon)
        {
            tile.ecology.plant_density = next_density;
            changed_mask |= TILE_ECOLOGY_CHANGED_DENSITY;
        }

        if (tile.ecology.plant_density <= k_density_epsilon &&
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

        if (has_tile_occupant(tile.ecology.plant_id, tile.ecology.ground_cover_type_id) &&
            tile.ecology.plant_density >= 1.0f - k_density_epsilon)
        {
            ++fully_grown_count;
        }
    }

    update_restoration_progress(context, fully_grown_count);
}
}  // namespace gs1
