#include "site/systems/device_weather_contribution_system.h"

#include "content/defs/structure_defs.h"
#include "site/site_world_components.h"
#include "site/tile_footprint.h"
#include "site/weather_contribution_logic.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4127)
#endif
#include <flecs.h>

#include <algorithm>
#include <cstddef>

namespace
{
[[nodiscard]] bool device_tile_is_support_anchor(
    gs1::TileCoord coord,
    gs1::StructureId structure_id) noexcept
{
    const auto anchor =
        gs1::align_tile_anchor_to_footprint(coord, gs1::resolve_structure_tile_footprint(structure_id));
    return anchor.x == coord.x && anchor.y == coord.y;
}

void ensure_runtime_buffers(
    gs1::DeviceWeatherContributionState& state,
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

void mark_tile_dirty(gs1::DeviceWeatherContributionState& state, std::uint32_t index)
{
    if (index >= state.dirty_tile_mask.size() || state.dirty_tile_mask[index] != 0U)
    {
        return;
    }

    state.dirty_tile_mask[index] = 1U;
    state.dirty_tile_indices.push_back(index);
}

void mark_all_tiles_dirty(
    gs1::DeviceWeatherContributionState& state,
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

void clear_dirty_tiles(gs1::DeviceWeatherContributionState& state) noexcept
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
    gs1::SiteSystemContext<gs1::DeviceWeatherContributionSystem>& context,
    gs1::DeviceWeatherContributionState& state,
    gs1::TileCoord source_coord)
{
    if (!context.world.tile_coord_in_bounds(source_coord))
    {
        return;
    }

    for (const auto sample : gs1::k_weather_contribution_samples)
    {
        const gs1::TileCoord target_coord {source_coord.x + sample.dx, source_coord.y + sample.dy};
        if (!context.world.tile_coord_in_bounds(target_coord))
        {
            continue;
        }

        mark_tile_dirty(state, context.world.tile_index(target_coord));
    }
}

gs1::SiteWorld::TileWeatherContributionData recompute_tile_contribution(
    gs1::SiteSystemContext<gs1::DeviceWeatherContributionSystem>& context,
    const gs1::WeatherUnitVector& wind_direction,
    gs1::TileCoord target_coord)
{
    auto& ecs_world = context.site_run.site_world->ecs_world();
    gs1::SiteWorld::TileWeatherContributionData total = gs1::zero_weather_contribution();

    for (const auto sample : gs1::k_weather_contribution_samples)
    {
        const gs1::TileCoord source_coord {target_coord.x - sample.dx, target_coord.y - sample.dy};
        if (!context.world.tile_coord_in_bounds(source_coord))
        {
            continue;
        }

        const auto source_entity_id = context.site_run.site_world->device_entity_id(source_coord);
        if (source_entity_id == 0U)
        {
            continue;
        }

        auto source_entity = ecs_world.entity(source_entity_id);
        const auto structure_component = source_entity.get<gs1::site_ecs::DeviceStructureId>();
        const gs1::StructureId structure_id = structure_component.structure_id;
        if (structure_id.value == 0U || !device_tile_is_support_anchor(source_coord, structure_id))
        {
            continue;
        }

        const auto* structure_def = gs1::find_structure_def(structure_id);
        if (structure_def == nullptr)
        {
            continue;
        }

        const auto efficiency_component = source_entity.get<gs1::site_ecs::DeviceEfficiency>();
        const float efficiency = std::clamp(efficiency_component.value, 0.0f, 1.0f);
        if (efficiency <= gs1::k_weather_contribution_epsilon)
        {
            continue;
        }

        gs1::SiteWorld::TileWeatherContributionData delta = gs1::zero_weather_contribution();
        const float contribution_scale = gs1::resolve_contribution_scale(sample.manhattan_distance);
        const bool within_shared_aura =
            sample.manhattan_distance == 0 ||
            sample.manhattan_distance <= static_cast<int>(structure_def->aura_size);
        if (within_shared_aura)
        {
            delta.heat_protection =
                structure_def->heat_protection_value * efficiency * contribution_scale;
            delta.dust_protection =
                structure_def->dust_protection_value * efficiency * contribution_scale;
            delta.fertility_improve =
                structure_def->fertility_improve_value * efficiency * contribution_scale;
            delta.salinity_reduction =
                structure_def->salinity_reduction_value * efficiency * contribution_scale;
            delta.irrigation =
                structure_def->irrigation_value * efficiency * contribution_scale;
        }

        if (sample.manhattan_distance == 0)
        {
            delta.wind_protection =
                structure_def->wind_protection_value * efficiency * contribution_scale;
        }
        else if (sample.manhattan_distance <= static_cast<int>(structure_def->wind_protection_range))
        {
            const float shadow_scale = gs1::compute_directional_wind_shadow_scale(
                source_coord.x,
                source_coord.y,
                target_coord.x,
                target_coord.y,
                structure_def->wind_protection_range,
                wind_direction);
            if (shadow_scale > gs1::k_weather_contribution_epsilon)
            {
                delta.wind_protection =
                    structure_def->wind_protection_value *
                    efficiency *
                    contribution_scale *
                    shadow_scale;
            }
        }

        gs1::accumulate_weather_contribution(total, delta);
    }

    return total;
}

void write_tile_contribution(
    gs1::SiteSystemContext<gs1::DeviceWeatherContributionSystem>& context,
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

    context.site_run.site_world->ecs_world().entity(entity_id).set<gs1::site_ecs::TileDeviceWeatherContribution>({
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
bool DeviceWeatherContributionSystem::subscribes_to(GameMessageType type) noexcept
{
    return type == GameMessageType::SiteRunStarted ||
        type == GameMessageType::SiteDevicePlaced ||
        type == GameMessageType::SiteDeviceBroken ||
        type == GameMessageType::SiteDeviceRepaired ||
        type == GameMessageType::SiteDeviceConditionChanged;
}

Gs1Status DeviceWeatherContributionSystem::process_message(
    SiteSystemContext<DeviceWeatherContributionSystem>& context,
    const GameMessage& message)
{
    if (!context.world.has_world())
    {
        return GS1_STATUS_OK;
    }

    auto& runtime = context.world.own_device_weather_runtime();
    ensure_runtime_buffers(runtime, context.world.tile_count());

    switch (message.type)
    {
    case GameMessageType::SiteRunStarted:
        runtime.full_rebuild_pending = true;
        return GS1_STATUS_OK;

    case GameMessageType::SiteDevicePlaced:
    {
        const auto& payload = message.payload_as<SiteDevicePlacedMessage>();
        mark_tiles_affected_by_source(
            context,
            runtime,
            TileCoord {payload.target_tile_x, payload.target_tile_y});
        return GS1_STATUS_OK;
    }

    case GameMessageType::SiteDeviceBroken:
    {
        const auto& payload = message.payload_as<SiteDeviceBrokenMessage>();
        mark_tiles_affected_by_source(
            context,
            runtime,
            TileCoord {payload.target_tile_x, payload.target_tile_y});
        return GS1_STATUS_OK;
    }

    case GameMessageType::SiteDeviceRepaired:
    {
        const auto& payload = message.payload_as<SiteDeviceRepairedMessage>();
        mark_tiles_affected_by_source(
            context,
            runtime,
            TileCoord {payload.target_tile_x, payload.target_tile_y});
        return GS1_STATUS_OK;
    }

    case GameMessageType::SiteDeviceConditionChanged:
    {
        const auto& payload = message.payload_as<SiteDeviceConditionChangedMessage>();
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

void DeviceWeatherContributionSystem::run(SiteSystemContext<DeviceWeatherContributionSystem>& context)
{
    if (!context.world.has_world())
    {
        return;
    }

    auto& runtime = context.world.own_device_weather_runtime();
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

    const WeatherUnitVector wind_direction =
        resolve_wind_direction_unit_vector(context.world.read_weather().weather_wind_direction_degrees);
    for (const std::uint32_t tile_index : runtime.dirty_tile_indices)
    {
        const TileCoord target_coord = context.world.tile_coord(tile_index);
        write_tile_contribution(
            context,
            tile_index,
            recompute_tile_contribution(context, wind_direction, target_coord));
    }

    clear_dirty_tiles(runtime);
}
}  // namespace gs1

#ifdef _MSC_VER
#pragma warning(pop)
#endif
