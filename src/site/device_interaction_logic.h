#pragma once

#include "site/site_run_state.h"
#include "site/site_world.h"

#include <array>
#include <cmath>
#include <limits>
#include <optional>

namespace gs1::device_interaction_logic
{
inline constexpr float k_worker_tile_epsilon_squared = 0.0025f;

inline bool tile_is_traversable(const SiteRunState& site_run, TileCoord coord) noexcept
{
    return site_run.site_world != nullptr &&
        site_run.site_world->contains(coord) &&
        site_run.site_world->tile_at(coord).static_data.traversable;
}

inline bool tile_in_interaction_range(TileCoord source, TileCoord target) noexcept
{
    return std::abs(target.x - source.x) <= 1 &&
        std::abs(target.y - source.y) <= 1;
}

inline bool worker_is_at_tile(
    const SiteWorld::WorkerData& worker,
    TileCoord tile) noexcept
{
    const float dx = worker.position.tile_x - static_cast<float>(tile.x);
    const float dy = worker.position.tile_y - static_cast<float>(tile.y);
    return (dx * dx + dy * dy) <= k_worker_tile_epsilon_squared;
}

inline std::optional<TileCoord> resolve_direct_approach_tile(
    const SiteRunState& site_run,
    const SiteWorld::WorkerData& worker,
    TileCoord target_tile)
{
    if (tile_is_traversable(site_run, target_tile))
    {
        return target_tile;
    }

    constexpr std::array<TileCoord, 8> k_neighbor_offsets {{
        TileCoord {0, -1},
        TileCoord {1, 0},
        TileCoord {0, 1},
        TileCoord {-1, 0},
        TileCoord {1, -1},
        TileCoord {1, 1},
        TileCoord {-1, 1},
        TileCoord {-1, -1},
    }};

    float best_distance_squared = std::numeric_limits<float>::max();
    std::optional<TileCoord> best_tile {};
    for (const auto offset : k_neighbor_offsets)
    {
        const TileCoord candidate {
            target_tile.x + offset.x,
            target_tile.y + offset.y};
        if (!tile_is_traversable(site_run, candidate))
        {
            continue;
        }

        const float dx = worker.position.tile_x - static_cast<float>(candidate.x);
        const float dy = worker.position.tile_y - static_cast<float>(candidate.y);
        const float distance_squared = dx * dx + dy * dy;
        if (distance_squared < best_distance_squared)
        {
            best_distance_squared = distance_squared;
            best_tile = candidate;
        }
    }

    return best_tile;
}

inline std::optional<TileCoord> resolve_interaction_range_approach_tile(
    const SiteRunState& site_run,
    const SiteWorld::WorkerData& worker,
    TileCoord target_tile)
{
    constexpr std::array<TileCoord, 9> k_range_offsets {{
        TileCoord {0, 0},
        TileCoord {0, -1},
        TileCoord {1, 0},
        TileCoord {0, 1},
        TileCoord {-1, 0},
        TileCoord {1, -1},
        TileCoord {1, 1},
        TileCoord {-1, 1},
        TileCoord {-1, -1},
    }};

    float best_distance_squared = std::numeric_limits<float>::max();
    std::optional<TileCoord> best_tile {};
    for (const auto offset : k_range_offsets)
    {
        const TileCoord candidate {
            target_tile.x + offset.x,
            target_tile.y + offset.y};
        if (!tile_is_traversable(site_run, candidate))
        {
            continue;
        }

        const float dx = worker.position.tile_x - static_cast<float>(candidate.x);
        const float dy = worker.position.tile_y - static_cast<float>(candidate.y);
        const float distance_squared = dx * dx + dy * dy;
        if (distance_squared < best_distance_squared)
        {
            best_distance_squared = distance_squared;
            best_tile = candidate;
        }
    }

    return best_tile;
}

inline bool worker_is_in_interaction_range(
    const SiteRunState& site_run,
    const SiteWorld::WorkerData& worker,
    TileCoord target_tile) noexcept
{
    if (!tile_in_interaction_range(target_tile, worker.position.tile_coord))
    {
        return false;
    }

    return tile_is_traversable(site_run, worker.position.tile_coord);
}
}  // namespace gs1::device_interaction_logic
