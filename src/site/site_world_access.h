#pragma once

#include "site/site_run_state.h"
#include "site/site_world.h"

#include <cstddef>
#include <cstdint>

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

inline SiteWorld::TileExcavationData tile_excavation(
    const SiteRunState& site_run,
    TileCoord coord) noexcept
{
    return has_world(site_run) ? site_run.site_world->tile_excavation(coord) : SiteWorld::TileExcavationData {};
}

inline void set_tile_excavation(
    SiteRunState& site_run,
    TileCoord coord,
    const SiteWorld::TileExcavationData& data)
{
    if (has_world(site_run))
    {
        site_run.site_world->set_tile_excavation(coord, data);
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
}  // namespace gs1::site_world_access
