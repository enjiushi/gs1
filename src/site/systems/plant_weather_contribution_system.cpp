#include "site/systems/plant_weather_contribution_system.h"

#include "content/defs/plant_defs.h"
#include "site/site_world_components.h"
#include "site/tile_footprint.h"
#include "site/weather_contribution_logic.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4127)
#endif
#include <flecs.h>

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace
{
template <typename Func>
void for_each_contribution_sample(std::uint8_t max_distance, Func&& func)
{
    for (int dy = -static_cast<int>(max_distance); dy <= static_cast<int>(max_distance); ++dy)
    {
        for (int dx = -static_cast<int>(max_distance); dx <= static_cast<int>(max_distance); ++dx)
        {
            const int manhattan_distance = std::abs(dx) + std::abs(dy);
            if (manhattan_distance > static_cast<int>(max_distance))
            {
                continue;
            }

            func(gs1::WeatherContributionSample {dx, dy, manhattan_distance});
        }
    }
}

[[nodiscard]] std::uint8_t resolve_effective_aura_distance(
    const gs1::PlantDef& plant_def) noexcept
{
    return gs1::scale_tile_distance_by_footprint_multiple(
        plant_def.aura_size,
        gs1::TileFootprint {plant_def.footprint_width, plant_def.footprint_height});
}

[[nodiscard]] std::uint8_t resolve_effective_wind_distance(
    const gs1::PlantDef& plant_def) noexcept
{
    return gs1::scale_tile_distance_by_footprint_multiple(
        plant_def.wind_protection_range,
        gs1::TileFootprint {plant_def.footprint_width, plant_def.footprint_height});
}

[[nodiscard]] std::uint8_t resolve_max_plant_contribution_distance() noexcept
{
    std::uint8_t max_distance = 0U;
    for (const auto& plant_def : gs1::all_plant_defs())
    {
        max_distance =
            std::max(max_distance, std::max(resolve_effective_aura_distance(plant_def), resolve_effective_wind_distance(plant_def)));
    }

    return max_distance;
}

const gs1::PlantDef& resolve_occupant_def(
    gs1::PlantId plant_id,
    std::uint32_t ground_cover_type_id) noexcept
{
    if (plant_id.value != 0U)
    {
        const auto* plant_def = gs1::find_plant_def(plant_id);
        if (plant_def != nullptr)
        {
            return *plant_def;
        }

        return gs1::k_generic_living_plant_def;
    }

    (void)ground_cover_type_id;
    return gs1::k_generic_ground_cover_def;
}

void ensure_runtime_buffers(
    gs1::PlantWeatherContributionState& state,
    std::size_t tile_count)
{
    if (state.dirty_tile_mask.size() == tile_count)
    {
        return;
    }

    state.dirty_tile_indices.clear();
    state.dirty_tile_mask.assign(tile_count, 0U);
    state.full_rebuild_pending = true;
    state.last_wind_direction_sector = 0xffU;
}

void mark_tile_dirty(gs1::PlantWeatherContributionState& state, std::uint32_t index)
{
    if (index >= state.dirty_tile_mask.size() || state.dirty_tile_mask[index] != 0U)
    {
        return;
    }

    state.dirty_tile_mask[index] = 1U;
    state.dirty_tile_indices.push_back(index);
}

void mark_all_tiles_dirty(
    gs1::PlantWeatherContributionState& state,
    std::size_t tile_count)
{
    state.dirty_tile_indices.clear();
    if (state.dirty_tile_mask.size() != tile_count)
    {
        state.dirty_tile_mask.assign(tile_count, 0U);
    }

    for (std::uint32_t index = 0U; index < tile_count; ++index)
    {
        state.dirty_tile_mask[index] = 1U;
        state.dirty_tile_indices.push_back(index);
    }

    state.full_rebuild_pending = false;
}

void clear_dirty_tiles(gs1::PlantWeatherContributionState& state) noexcept
{
    for (const std::uint32_t index : state.dirty_tile_indices)
    {
        if (index < state.dirty_tile_mask.size())
        {
            state.dirty_tile_mask[index] = 0U;
        }
    }

    state.dirty_tile_indices.clear();
}

void mark_tiles_affected_by_source(
    gs1::SiteSystemContext<gs1::PlantWeatherContributionSystem>& context,
    gs1::PlantWeatherContributionState& state,
    gs1::TileCoord source_coord)
{
    if (!context.world.tile_coord_in_bounds(source_coord))
    {
        return;
    }

    const std::uint8_t max_distance = resolve_max_plant_contribution_distance();
    for_each_contribution_sample(max_distance, [&](const gs1::WeatherContributionSample& sample)
    {
        const gs1::TileCoord target_coord {source_coord.x + sample.dx, source_coord.y + sample.dy};
        if (!context.world.tile_coord_in_bounds(target_coord))
        {
            return;
        }

        mark_tile_dirty(state, context.world.tile_index(target_coord));
    });
}

bool source_and_target_share_occupant_instance(
    gs1::SiteSystemContext<gs1::PlantWeatherContributionSystem>& context,
    flecs::world& ecs_world,
    gs1::TileCoord source_coord,
    gs1::TileCoord target_coord,
    gs1::PlantId source_plant_id) noexcept
{
    if (source_coord.x == target_coord.x && source_coord.y == target_coord.y)
    {
        return true;
    }

    if (source_plant_id.value == 0U || !context.world.tile_coord_in_bounds(target_coord))
    {
        return false;
    }

    const auto target_entity_id = context.site_run.site_world->tile_entity_id(target_coord);
    if (target_entity_id == 0U)
    {
        return false;
    }

    const auto target_entity = ecs_world.entity(target_entity_id);
    if (!target_entity.has<gs1::site_ecs::TileOccupantTag>())
    {
        return false;
    }

    const auto target_plant_slot = target_entity.get<gs1::site_ecs::TilePlantSlot>();
    if (target_plant_slot.plant_id != source_plant_id)
    {
        return false;
    }

    const auto footprint = gs1::resolve_plant_tile_footprint(source_plant_id);
    const gs1::TileCoord source_anchor = gs1::align_tile_anchor_to_footprint(source_coord, footprint);
    const gs1::TileCoord target_anchor = gs1::align_tile_anchor_to_footprint(target_coord, footprint);
    return source_anchor.x == target_anchor.x && source_anchor.y == target_anchor.y;
}

gs1::SiteWorld::TileWeatherContributionData recompute_tile_contribution(
    gs1::SiteSystemContext<gs1::PlantWeatherContributionSystem>& context,
    const gs1::WeatherDirectionStep& wind_direction,
    std::uint8_t max_distance,
    gs1::TileCoord target_coord)
{
    auto& ecs_world = context.site_run.site_world->ecs_world();
    gs1::SiteWorld::TileWeatherContributionData total = gs1::zero_weather_contribution();

    for_each_contribution_sample(max_distance, [&](const gs1::WeatherContributionSample& sample)
    {
        const gs1::TileCoord source_coord {target_coord.x - sample.dx, target_coord.y - sample.dy};
        if (!context.world.tile_coord_in_bounds(source_coord))
        {
            return;
        }

        const auto source_entity_id = context.site_run.site_world->tile_entity_id(source_coord);
        if (source_entity_id == 0U)
        {
            return;
        }

        auto source_entity = ecs_world.entity(source_entity_id);
        if (!source_entity.has<gs1::site_ecs::TileOccupantTag>())
        {
            return;
        }

        const auto density_component = source_entity.get<gs1::site_ecs::TilePlantDensity>();
        const float density =
            std::clamp(density_component.value * gs1::k_inverse_meter_scale, 0.0f, 1.0f);
        if (density <= gs1::k_weather_contribution_epsilon)
        {
            return;
        }

        const auto plant_slot = source_entity.get<gs1::site_ecs::TilePlantSlot>();
        const auto ground_cover_slot = source_entity.get<gs1::site_ecs::TileGroundCoverSlot>();
        const auto& plant_def =
            resolve_occupant_def(plant_slot.plant_id, ground_cover_slot.ground_cover_type_id);
        const bool same_occupant_instance =
            source_and_target_share_occupant_instance(
                context,
                ecs_world,
                source_coord,
                target_coord,
                plant_slot.plant_id);
        const std::uint8_t effective_aura_distance = resolve_effective_aura_distance(plant_def);
        const std::uint8_t effective_wind_distance = resolve_effective_wind_distance(plant_def);
        const float protection_ratio = std::clamp(plant_def.protection_ratio, 0.0f, 1.0f);

        gs1::SiteWorld::TileWeatherContributionData delta = gs1::zero_weather_contribution();
        const float contribution_scale = gs1::resolve_contribution_scale(sample.manhattan_distance);
        const bool within_shared_aura =
            sample.manhattan_distance == 0 ||
            sample.manhattan_distance <= static_cast<int>(effective_aura_distance);
        if (within_shared_aura)
        {
            delta.fertility_improve =
                plant_def.fertility_improve_power * density * contribution_scale;
            delta.salinity_reduction =
                derived_plant_salinity_reduction_power(plant_def) * density * contribution_scale;

            if (!same_occupant_instance)
            {
                delta.heat_protection =
                    plant_def.heat_tolerance * protection_ratio * density * contribution_scale;
                delta.dust_protection =
                    plant_def.dust_tolerance * protection_ratio * density * contribution_scale;
            }
        }

        if (!same_occupant_instance &&
            sample.manhattan_distance > 0 &&
            sample.manhattan_distance <= static_cast<int>(effective_wind_distance))
        {
            const float shadow_scale = gs1::compute_directional_wind_shadow_scale(
                source_coord.x,
                source_coord.y,
                target_coord.x,
                target_coord.y,
                effective_wind_distance,
                wind_direction);
            if (shadow_scale > gs1::k_weather_contribution_epsilon)
            {
                delta.wind_protection =
                    plant_def.wind_resistance *
                    protection_ratio *
                    density *
                    shadow_scale;
            }
        }

        gs1::accumulate_weather_contribution(total, delta);
    });

    return total;
}

void write_tile_contribution(
    gs1::SiteSystemContext<gs1::PlantWeatherContributionSystem>& context,
    std::uint32_t tile_index,
    const gs1::SiteWorld::TileWeatherContributionData& contribution)
{
    if (tile_index >= context.world.tile_count())
    {
        return;
    }

    const auto entity_id = context.site_run.site_world->tile_entity_id(context.world.tile_coord(tile_index));
    if (entity_id == 0U)
    {
        return;
    }

    context.site_run.site_world->ecs_world().entity(entity_id).set<gs1::site_ecs::TilePlantWeatherContribution>({
        contribution.heat_protection,
        contribution.wind_protection,
        contribution.dust_protection,
        contribution.fertility_improve,
        contribution.salinity_reduction,
        contribution.irrigation});
}
}  // namespace

namespace gs1
{
bool PlantWeatherContributionSystem::subscribes_to(GameMessageType type) noexcept
{
    return type == GameMessageType::SiteRunStarted ||
        type == GameMessageType::TileEcologyChanged;
}

Gs1Status PlantWeatherContributionSystem::process_message(
    SiteSystemContext<PlantWeatherContributionSystem>& context,
    const GameMessage& message)
{
    if (!context.world.has_world())
    {
        return GS1_STATUS_OK;
    }

    auto& runtime = context.world.own_plant_weather_runtime();
    ensure_runtime_buffers(runtime, context.world.tile_count());

    switch (message.type)
    {
    case GameMessageType::SiteRunStarted:
        runtime.full_rebuild_pending = true;
        return GS1_STATUS_OK;

    case GameMessageType::TileEcologyChanged:
    {
        const auto& payload = message.payload_as<TileEcologyChangedMessage>();
        if ((payload.changed_mask & (TILE_ECOLOGY_CHANGED_OCCUPANCY | TILE_ECOLOGY_CHANGED_DENSITY)) == 0U)
        {
            return GS1_STATUS_OK;
        }

        mark_tiles_affected_by_source(
            context,
            runtime,
            TileCoord {payload.target_tile_x, payload.target_tile_y});
        return GS1_STATUS_OK;
    }

    default:
        return GS1_STATUS_OK;
    }
}

void PlantWeatherContributionSystem::run(SiteSystemContext<PlantWeatherContributionSystem>& context)
{
    if (!context.world.has_world())
    {
        return;
    }

    auto& runtime = context.world.own_plant_weather_runtime();
    ensure_runtime_buffers(runtime, context.world.tile_count());

    const std::uint8_t wind_sector = quantize_wind_direction_sector(
        context.world.read_weather().weather_wind_direction_degrees);
    if (runtime.last_wind_direction_sector != wind_sector)
    {
        runtime.last_wind_direction_sector = wind_sector;
        runtime.full_rebuild_pending = true;
    }

    if (runtime.full_rebuild_pending)
    {
        mark_all_tiles_dirty(runtime, context.world.tile_count());
    }

    if (runtime.dirty_tile_indices.empty())
    {
        return;
    }

    const WeatherDirectionStep wind_direction =
        resolve_wind_direction_step(context.world.read_weather().weather_wind_direction_degrees);
    const std::uint8_t max_distance = resolve_max_plant_contribution_distance();
    for (const std::uint32_t tile_index : runtime.dirty_tile_indices)
    {
        const TileCoord target_coord = context.world.tile_coord(tile_index);
        write_tile_contribution(
            context,
            tile_index,
            recompute_tile_contribution(context, wind_direction, max_distance, target_coord));
    }

    clear_dirty_tiles(runtime);
}
}  // namespace gs1

#ifdef _MSC_VER
#pragma warning(pop)
#endif
