#pragma once

#include "site/site_run_state.h"
#include "site/site_world.h"
#include "site/site_world_components.h"

#include <flecs.h>

namespace gs1::site_world_access
{
inline bool has_world(const SiteRunState& site_run) noexcept
{
    return site_run.site_world != nullptr && site_run.site_world->initialized();
}

inline std::uint32_t width(const SiteRunState& site_run) noexcept
{
    return has_world(site_run) ? site_run.site_world->width() : 0U;
}

inline std::uint32_t height(const SiteRunState& site_run) noexcept
{
    return has_world(site_run) ? site_run.site_world->height() : 0U;
}

inline bool tile_coord_in_bounds(const SiteRunState& site_run, TileCoord coord) noexcept
{
    return has_world(site_run) && site_run.site_world->contains(coord);
}

inline std::size_t tile_count(const SiteRunState& site_run) noexcept
{
    return has_world(site_run) ? site_run.site_world->tile_count() : 0U;
}

inline std::uint32_t tile_index(const SiteRunState& site_run, TileCoord coord) noexcept
{
    return has_world(site_run) ? site_run.site_world->tile_index(coord) : 0U;
}

inline TileCoord tile_coord(const SiteRunState& site_run, std::size_t index) noexcept
{
    return has_world(site_run) ? site_run.site_world->tile_coord(index) : TileCoord {};
}

namespace detail
{
inline flecs::entity tile_entity(const SiteRunState& site_run, TileCoord coord) noexcept
{
    if (!tile_coord_in_bounds(site_run, coord))
    {
        return flecs::entity {};
    }

    auto& world = const_cast<SiteWorld*>(site_run.site_world.get())->ecs_world();
    return world.entity(site_run.site_world->tile_entity_id(coord));
}

inline flecs::entity device_entity(const SiteRunState& site_run, TileCoord coord) noexcept
{
    if (!tile_coord_in_bounds(site_run, coord))
    {
        return flecs::entity {};
    }

    const auto entity_id = site_run.site_world->device_entity_id(coord);
    if (entity_id == 0U)
    {
        return flecs::entity {};
    }

    auto& world = const_cast<SiteWorld*>(site_run.site_world.get())->ecs_world();
    return world.entity(entity_id);
}

inline flecs::entity worker_entity(const SiteRunState& site_run) noexcept
{
    if (!has_world(site_run))
    {
        return flecs::entity {};
    }

    auto& world = const_cast<SiteWorld*>(site_run.site_world.get())->ecs_world();
    return world.entity(site_run.site_world->worker_entity_id());
}

inline void sync_tile_occupant_tag(flecs::entity entity)
{
    const auto& plant = entity.get<site_ecs::TilePlantSlot>();
    const auto& ground_cover = entity.get<site_ecs::TileGroundCoverSlot>();
    if (plant.plant_id.value != 0U || ground_cover.ground_cover_type_id != 0U)
    {
        entity.add<site_ecs::TileOccupantTag>();
    }
    else
    {
        entity.remove<site_ecs::TileOccupantTag>();
    }
}
}  // namespace detail

inline SiteWorld::TileStaticData tile_static_data(const SiteRunState& site_run, TileCoord coord) noexcept
{
    return has_world(site_run) ? site_run.site_world->tile_static_data(coord) : SiteWorld::TileStaticData {};
}

inline SiteWorld::TileEcologyData tile_ecology(const SiteRunState& site_run, TileCoord coord) noexcept
{
    return has_world(site_run) ? site_run.site_world->tile_ecology(coord) : SiteWorld::TileEcologyData {};
}

inline void set_tile_ecology(
    SiteRunState& site_run,
    TileCoord coord,
    const SiteWorld::TileEcologyData& data)
{
    if (has_world(site_run))
    {
        site_run.site_world->set_tile_ecology(coord, data);
    }
}

inline SiteWorld::TileLocalWeatherData tile_local_weather(
    const SiteRunState& site_run,
    TileCoord coord) noexcept
{
    return has_world(site_run)
        ? site_run.site_world->tile_local_weather(coord)
        : SiteWorld::TileLocalWeatherData {};
}

inline void set_tile_local_weather(
    SiteRunState& site_run,
    TileCoord coord,
    const SiteWorld::TileLocalWeatherData& data)
{
    if (has_world(site_run))
    {
        site_run.site_world->set_tile_local_weather(coord, data);
    }
}

inline SiteWorld::TileDeviceData tile_device(const SiteRunState& site_run, TileCoord coord) noexcept
{
    return has_world(site_run) ? site_run.site_world->tile_device(coord) : SiteWorld::TileDeviceData {};
}

inline void set_tile_device(
    SiteRunState& site_run,
    TileCoord coord,
    const SiteWorld::TileDeviceData& data)
{
    if (has_world(site_run))
    {
        site_run.site_world->set_tile_device(coord, data);
    }
}

inline SiteWorld::WorkerPositionData worker_position(const SiteRunState& site_run) noexcept
{
    return has_world(site_run) ? site_run.site_world->worker_position() : SiteWorld::WorkerPositionData {};
}

inline void set_worker_position(SiteRunState& site_run, const SiteWorld::WorkerPositionData& data)
{
    if (has_world(site_run))
    {
        site_run.site_world->set_worker_position(data);
    }
}

inline SiteWorld::WorkerConditionData worker_conditions(const SiteRunState& site_run) noexcept
{
    return has_world(site_run) ? site_run.site_world->worker_conditions() : SiteWorld::WorkerConditionData {};
}

inline void set_worker_conditions(SiteRunState& site_run, const SiteWorld::WorkerConditionData& data)
{
    if (has_world(site_run))
    {
        site_run.site_world->set_worker_conditions(data);
    }
}

namespace movement
{
inline bool tile_traversable(const SiteRunState& site_run, TileCoord coord) noexcept
{
    const auto entity = detail::tile_entity(site_run, coord);
    return entity ? entity.get<site_ecs::TileTraversable>().value : false;
}

template <typename Func>
bool with_worker_mut(SiteRunState& site_run, Func&& func)
{
    auto entity = detail::worker_entity(site_run);
    if (!entity)
    {
        return false;
    }

    auto& position = entity.get_mut<site_ecs::WorkerTilePosition>();
    auto& facing = entity.get_mut<site_ecs::WorkerFacing>();
    const bool modified = func(position, facing);
    if (modified)
    {
        entity.modified<site_ecs::WorkerTilePosition>();
        entity.modified<site_ecs::WorkerFacing>();
    }
    return modified;
}
}  // namespace movement

namespace worker_condition
{
template <typename Func>
bool read_worker(const SiteRunState& site_run, Func&& func)
{
    auto entity = detail::worker_entity(site_run);
    if (!entity)
    {
        return false;
    }

    func(entity.get<site_ecs::WorkerVitals>());
    return true;
}

template <typename Func>
bool with_worker_mut(SiteRunState& site_run, Func&& func)
{
    auto entity = detail::worker_entity(site_run);
    if (!entity)
    {
        return false;
    }

    auto& vitals = entity.get_mut<site_ecs::WorkerVitals>();
    const bool modified = func(vitals);
    if (modified)
    {
        entity.modified<site_ecs::WorkerVitals>();
    }
    return modified;
}
}  // namespace worker_condition

namespace local_weather
{
template <typename Func>
void for_each_tile_mut(SiteRunState& site_run, Func&& func)
{
    if (!has_world(site_run))
    {
        return;
    }

    site_run.site_world->ecs_world()
        .query_builder<
            const site_ecs::TileCoordComponent,
            const site_ecs::TilePlantSlot,
            const site_ecs::TileGroundCoverSlot,
            const site_ecs::TilePlantDensity,
            site_ecs::TileHeat,
            site_ecs::TileWind,
            site_ecs::TileDust>()
        .with<site_ecs::TileTag>()
        .each(
            [&](flecs::entity entity,
                const site_ecs::TileCoordComponent& coord,
                const site_ecs::TilePlantSlot& plant,
                const site_ecs::TileGroundCoverSlot& ground_cover,
                const site_ecs::TilePlantDensity& density,
                site_ecs::TileHeat& heat,
                site_ecs::TileWind& wind,
                site_ecs::TileDust& dust) {
                const bool modified = func(coord.value, plant, ground_cover, density, heat, wind, dust);
                if (modified)
                {
                    entity.modified<site_ecs::TileHeat>();
                    entity.modified<site_ecs::TileWind>();
                    entity.modified<site_ecs::TileDust>();
                }
            });
}
}  // namespace local_weather

namespace device_support
{
template <typename Func>
void for_each_device_mut(SiteRunState& site_run, Func&& func)
{
    if (!has_world(site_run))
    {
        return;
    }

    site_run.site_world->ecs_world()
        .query_builder<
            const site_ecs::TileCoordComponent,
            const site_ecs::DeviceStructureId,
            const site_ecs::DeviceIntegrity,
            site_ecs::DeviceEfficiency,
            site_ecs::DeviceStoredWater>()
        .with<site_ecs::DeviceTag>()
        .each(
            [&](flecs::entity entity,
                const site_ecs::TileCoordComponent& coord,
                const site_ecs::DeviceStructureId& structure,
                const site_ecs::DeviceIntegrity& integrity,
                site_ecs::DeviceEfficiency& efficiency,
                site_ecs::DeviceStoredWater& stored_water) {
                const auto tile = detail::tile_entity(site_run, coord.value);
                const auto& heat = tile.get<site_ecs::TileHeat>();
                const bool modified =
                    func(coord.value, structure, integrity, heat, efficiency, stored_water);
                if (modified)
                {
                    entity.modified<site_ecs::DeviceEfficiency>();
                    entity.modified<site_ecs::DeviceStoredWater>();
                }
            });
}
}  // namespace device_support

namespace device_maintenance
{
template <typename Func>
void for_each_device_mut(SiteRunState& site_run, Func&& func)
{
    if (!has_world(site_run))
    {
        return;
    }

    site_run.site_world->ecs_world()
        .query_builder<
            const site_ecs::TileCoordComponent,
            const site_ecs::DeviceStructureId,
            site_ecs::DeviceIntegrity>()
        .with<site_ecs::DeviceTag>()
        .each(
            [&](flecs::entity entity,
                const site_ecs::TileCoordComponent& coord,
                const site_ecs::DeviceStructureId& structure,
                site_ecs::DeviceIntegrity& integrity) {
                const auto tile = detail::tile_entity(site_run, coord.value);
                const auto& burial = tile.get<site_ecs::TileSandBurial>();
                const bool modified = func(coord.value, structure, burial, integrity);
                if (modified)
                {
                    entity.modified<site_ecs::DeviceIntegrity>();
                }
            });
}
}  // namespace device_maintenance

namespace ecology
{
template <typename Func>
bool read_tile(const SiteRunState& site_run, TileCoord coord, Func&& func)
{
    auto entity = detail::tile_entity(site_run, coord);
    if (!entity)
    {
        return false;
    }

    func(
        entity.get<site_ecs::TileSandBurial>(),
        entity.get<site_ecs::TilePlantSlot>(),
        entity.get<site_ecs::TileGroundCoverSlot>(),
        entity.get<site_ecs::TilePlantDensity>());
    return true;
}

template <typename Func>
bool with_tile_mut(SiteRunState& site_run, TileCoord coord, Func&& func)
{
    auto entity = detail::tile_entity(site_run, coord);
    if (!entity)
    {
        return false;
    }

    auto& sand_burial = entity.get_mut<site_ecs::TileSandBurial>();
    auto& plant = entity.get_mut<site_ecs::TilePlantSlot>();
    auto& ground_cover = entity.get_mut<site_ecs::TileGroundCoverSlot>();
    auto& density = entity.get_mut<site_ecs::TilePlantDensity>();
    const bool modified = func(sand_burial, plant, ground_cover, density);
    if (modified)
    {
        detail::sync_tile_occupant_tag(entity);
        entity.modified<site_ecs::TileSandBurial>();
        entity.modified<site_ecs::TilePlantSlot>();
        entity.modified<site_ecs::TileGroundCoverSlot>();
        entity.modified<site_ecs::TilePlantDensity>();
    }
    return modified;
}

template <typename Func>
void for_each_occupied_tile_mut(SiteRunState& site_run, Func&& func)
{
    if (!has_world(site_run))
    {
        return;
    }

    site_run.site_world->ecs_world()
        .query_builder<
            const site_ecs::TileCoordComponent,
            site_ecs::TilePlantSlot,
            site_ecs::TileGroundCoverSlot,
            site_ecs::TilePlantDensity>()
        .with<site_ecs::TileTag>()
        .with<site_ecs::TileOccupantTag>()
        .each(
            [&](flecs::entity entity,
                const site_ecs::TileCoordComponent& coord,
                site_ecs::TilePlantSlot& plant,
                site_ecs::TileGroundCoverSlot& ground_cover,
                site_ecs::TilePlantDensity& density) {
                const bool modified = func(coord.value, plant, ground_cover, density);
                if (modified)
                {
                    detail::sync_tile_occupant_tag(entity);
                    entity.modified<site_ecs::TilePlantSlot>();
                    entity.modified<site_ecs::TileGroundCoverSlot>();
                    entity.modified<site_ecs::TilePlantDensity>();
                }
            });
}
}  // namespace ecology

namespace placement_validation
{
template <typename Func>
bool read_tile(const SiteRunState& site_run, TileCoord coord, Func&& func)
{
    auto tile = detail::tile_entity(site_run, coord);
    if (!tile)
    {
        return false;
    }

    const auto device = detail::device_entity(site_run, coord);
    const auto structure_id = device
        ? device.get<site_ecs::DeviceStructureId>().structure_id
        : StructureId {};

    func(
        tile.get<site_ecs::TileTraversable>(),
        tile.get<site_ecs::TilePlantable>(),
        tile.get<site_ecs::TileReservedByStructure>(),
        tile.get<site_ecs::TilePlantSlot>(),
        tile.get<site_ecs::TileGroundCoverSlot>(),
        structure_id);
    return true;
}
}  // namespace placement_validation
}  // namespace gs1::site_world_access
