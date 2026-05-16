#include "site/systems/device_weather_contribution_system.h"

#include "content/defs/structure_defs.h"
#include "runtime/game_runtime.h"
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
    gs1::DeviceWeatherContributionMetaState& meta,
    std::vector<std::uint32_t>& dirty_tile_indices,
    std::vector<std::uint8_t>& dirty_tile_mask,
    std::size_t tile_count)
{
    if (dirty_tile_mask.size() == tile_count)
    {
        return;
    }

    dirty_tile_indices.clear();
    dirty_tile_mask.assign(tile_count, 0U);
    meta.full_rebuild_pending = true;
    meta.last_wind_direction_sector = 0xffU;
}

void mark_tile_dirty(
    std::vector<std::uint32_t>& dirty_tile_indices,
    std::vector<std::uint8_t>& dirty_tile_mask,
    std::uint32_t index)
{
    if (index >= dirty_tile_mask.size() || dirty_tile_mask[index] != 0U)
    {
        return;
    }

    dirty_tile_mask[index] = 1U;
    dirty_tile_indices.push_back(index);
}

void mark_all_tiles_dirty(
    gs1::DeviceWeatherContributionMetaState& meta,
    std::vector<std::uint32_t>& dirty_tile_indices,
    std::vector<std::uint8_t>& dirty_tile_mask,
    std::size_t tile_count)
{
    dirty_tile_indices.clear();
    if (dirty_tile_mask.size() != tile_count)
    {
        dirty_tile_mask.assign(tile_count, 0U);
    }

    for (std::uint32_t index = 0U; index < tile_count; ++index)
    {
        dirty_tile_mask[index] = 1U;
        dirty_tile_indices.push_back(index);
    }

    meta.full_rebuild_pending = false;
}

void clear_dirty_tiles(
    std::vector<std::uint32_t>& dirty_tile_indices,
    std::vector<std::uint8_t>& dirty_tile_mask) noexcept
{
    for (const std::uint32_t index : dirty_tile_indices)
    {
        if (index < dirty_tile_mask.size())
        {
            dirty_tile_mask[index] = 0U;
        }
    }

    dirty_tile_indices.clear();
}

void mark_tiles_affected_by_source(
    gs1::SiteWorldAccess<gs1::DeviceWeatherContributionSystem>& world,
    std::vector<std::uint32_t>& dirty_tile_indices,
    std::vector<std::uint8_t>& dirty_tile_mask,
    gs1::TileCoord source_coord)
{
    if (!world.tile_coord_in_bounds(source_coord))
    {
        return;
    }

    for (const auto sample : gs1::k_weather_contribution_samples)
    {
        const gs1::TileCoord target_coord {source_coord.x + sample.dx, source_coord.y + sample.dy};
        if (!world.tile_coord_in_bounds(target_coord))
        {
            continue;
        }

        mark_tile_dirty(dirty_tile_indices, dirty_tile_mask, world.tile_index(target_coord));
    }
}

gs1::SiteWorld::TileWeatherContributionData recompute_tile_contribution(
    gs1::SiteWorldAccess<gs1::DeviceWeatherContributionSystem>& world,
    const gs1::WeatherDirectionStep& wind_direction,
    gs1::TileCoord target_coord)
{
    const auto* site_world = world.read_site_world();
    if (site_world == nullptr)
    {
        return {};
    }

    auto& ecs_world = site_world->ecs_world();
    gs1::SiteWorld::TileWeatherContributionData total = gs1::zero_weather_contribution();

    for (const auto sample : gs1::k_weather_contribution_samples)
    {
        const gs1::TileCoord source_coord {target_coord.x - sample.dx, target_coord.y - sample.dy};
        if (!world.tile_coord_in_bounds(source_coord))
        {
            continue;
        }

        const auto source_entity_id = world.device_entity_id(source_coord);
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
                structure_def->wind_protection_value * efficiency;
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
                    shadow_scale;
            }
        }

        gs1::accumulate_weather_contribution(total, delta);
    }

    return total;
}

void write_tile_contribution(
    gs1::SiteWorldAccess<gs1::DeviceWeatherContributionSystem>& world,
    std::uint32_t tile_index,
    const gs1::SiteWorld::TileWeatherContributionData& contribution)
{
    if (tile_index >= world.tile_count())
    {
        return;
    }

    const auto* site_world = world.own_site_world();
    if (site_world == nullptr)
    {
        return;
    }

    const auto entity_id = site_world->tile_entity_id(world.tile_coord(tile_index));
    if (entity_id == 0U)
    {
        return;
    }

    site_world->ecs_world().entity(entity_id).set<gs1::site_ecs::TileDeviceWeatherContribution>({
        contribution.heat_protection,
        contribution.wind_protection,
        contribution.dust_protection,
        contribution.fertility_improve,
        contribution.salinity_reduction,
        contribution.irrigation});
}

Gs1Status process_message(
    gs1::RuntimeInvocation& invocation,
    const gs1::GameMessage& message)
{
    gs1::SiteWorldAccess<gs1::DeviceWeatherContributionSystem> world {invocation};
    if (!world.has_world())
    {
        return GS1_STATUS_INVALID_STATE;
    }

    auto& runtime_meta = world.own_device_weather_runtime_meta();
    auto& dirty_tile_indices = world.own_device_weather_dirty_tile_indices();
    auto& dirty_tile_mask = world.own_device_weather_dirty_tile_mask();
    ensure_runtime_buffers(runtime_meta, dirty_tile_indices, dirty_tile_mask, world.tile_count());

    switch (message.type)
    {
    case gs1::GameMessageType::SiteRunStarted:
        runtime_meta.full_rebuild_pending = true;
        return GS1_STATUS_OK;

    case gs1::GameMessageType::SiteDevicePlaced:
    {
        const auto& payload = message.payload_as<gs1::SiteDevicePlacedMessage>();
        mark_tiles_affected_by_source(
            world,
            dirty_tile_indices,
            dirty_tile_mask,
            gs1::TileCoord {payload.target_tile_x, payload.target_tile_y});
        return GS1_STATUS_OK;
    }

    case gs1::GameMessageType::SiteDeviceBroken:
    {
        const auto& payload = message.payload_as<gs1::SiteDeviceBrokenMessage>();
        mark_tiles_affected_by_source(
            world,
            dirty_tile_indices,
            dirty_tile_mask,
            gs1::TileCoord {payload.target_tile_x, payload.target_tile_y});
        return GS1_STATUS_OK;
    }

    case gs1::GameMessageType::SiteDeviceRepaired:
    {
        const auto& payload = message.payload_as<gs1::SiteDeviceRepairedMessage>();
        mark_tiles_affected_by_source(
            world,
            dirty_tile_indices,
            dirty_tile_mask,
            gs1::TileCoord {payload.target_tile_x, payload.target_tile_y});
        return GS1_STATUS_OK;
    }

    case gs1::GameMessageType::SiteDeviceConditionChanged:
    {
        const auto& payload = message.payload_as<gs1::SiteDeviceConditionChangedMessage>();
        mark_tiles_affected_by_source(
            world,
            dirty_tile_indices,
            dirty_tile_mask,
            gs1::TileCoord {payload.target_tile_x, payload.target_tile_y});
        return GS1_STATUS_OK;
    }

    default:
        return GS1_STATUS_OK;
    }
}

void run_system(gs1::RuntimeInvocation& invocation)
{
    gs1::SiteWorldAccess<gs1::DeviceWeatherContributionSystem> world {invocation};
    if (!world.has_world())
    {
        return;
    }

    auto& runtime_meta = world.own_device_weather_runtime_meta();
    auto& dirty_tile_indices = world.own_device_weather_dirty_tile_indices();
    auto& dirty_tile_mask = world.own_device_weather_dirty_tile_mask();
    ensure_runtime_buffers(runtime_meta, dirty_tile_indices, dirty_tile_mask, world.tile_count());

    const std::uint8_t wind_sector = gs1::quantize_wind_direction_sector(
        world.read_weather().weather_wind_direction_degrees);
    if (runtime_meta.last_wind_direction_sector != wind_sector)
    {
        runtime_meta.last_wind_direction_sector = wind_sector;
        runtime_meta.full_rebuild_pending = true;
    }

    if (runtime_meta.full_rebuild_pending)
    {
        mark_all_tiles_dirty(runtime_meta, dirty_tile_indices, dirty_tile_mask, world.tile_count());
    }

    if (dirty_tile_indices.empty())
    {
        return;
    }

    const gs1::WeatherDirectionStep wind_direction =
        gs1::resolve_wind_direction_step(world.read_weather().weather_wind_direction_degrees);
    for (const std::uint32_t tile_index : dirty_tile_indices)
    {
        const gs1::TileCoord target_coord = world.tile_coord(tile_index);
        write_tile_contribution(
            world,
            tile_index,
            recompute_tile_contribution(world, wind_direction, target_coord));
    }

    clear_dirty_tiles(dirty_tile_indices, dirty_tile_mask);
}
}  // namespace

namespace gs1
{
const char* DeviceWeatherContributionSystem::name() const noexcept
{
    return access().system_name.data();
}

GameMessageSubscriptionSpan DeviceWeatherContributionSystem::subscribed_game_messages() const noexcept
{
    static constexpr GameMessageType subscriptions[] = {
        GameMessageType::SiteRunStarted,
        GameMessageType::SiteDevicePlaced,
        GameMessageType::SiteDeviceBroken,
        GameMessageType::SiteDeviceRepaired,
        GameMessageType::SiteDeviceConditionChanged,
    };
    return subscriptions;
}

HostMessageSubscriptionSpan DeviceWeatherContributionSystem::subscribed_host_messages() const noexcept
{
    return {};
}

std::optional<Gs1RuntimeProfileSystemId> DeviceWeatherContributionSystem::profile_system_id() const noexcept
{
    return GS1_RUNTIME_PROFILE_SYSTEM_LOCAL_WEATHER_RESOLVE;
}

std::optional<std::uint32_t> DeviceWeatherContributionSystem::fixed_step_order() const noexcept
{
    return 7U;
}

Gs1Status DeviceWeatherContributionSystem::process_game_message(
    RuntimeInvocation& invocation,
    const GameMessage& message)
{
    return process_message(invocation, message);
}

Gs1Status DeviceWeatherContributionSystem::process_host_message(
    RuntimeInvocation& invocation,
    const Gs1HostMessage& message)
{
    (void)message;
    (void)invocation;
    return GS1_STATUS_OK;
}

void DeviceWeatherContributionSystem::run(RuntimeInvocation& invocation)
{
    run_system(invocation);
}
}  // namespace gs1

#ifdef _MSC_VER
#pragma warning(pop)
#endif

