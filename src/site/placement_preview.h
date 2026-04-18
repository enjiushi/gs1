#pragma once

#include "site/tile_footprint.h"

#include <cstdint>

namespace gs1
{
struct PlacementPreviewResult final
{
    TileFootprint footprint {1U, 1U};
    std::uint64_t blocked_mask {0ULL};
    bool in_bounds {true};
};

template <typename WorldAccess>
[[nodiscard]] inline PlacementPreviewResult build_placement_preview(
    const WorldAccess& world,
    TileCoord anchor_tile,
    PlacementOccupancyLayer occupancy_layer,
    PlacementReservationSubjectKind subject_kind,
    std::uint32_t subject_id) noexcept
{
    PlacementPreviewResult result {};
    result.footprint = resolve_placement_reservation_footprint(
        occupancy_layer,
        subject_kind,
        subject_id);

    std::uint32_t tile_index = 0U;
    for_each_tile_in_footprint(
        anchor_tile,
        result.footprint,
        [&](TileCoord coord) {
            const std::uint64_t tile_bit =
                tile_index < 64U ? (1ULL << static_cast<std::uint64_t>(tile_index)) : 0ULL;
            tile_index += 1U;

            if (!world.tile_coord_in_bounds(coord))
            {
                result.blocked_mask |= tile_bit;
                result.in_bounds = false;
                return;
            }

            const auto tile = world.read_tile(coord);
            if (occupancy_layer == PlacementOccupancyLayer::GroundCover)
            {
                if (!tile.static_data.plantable ||
                    tile.static_data.reserved_by_structure ||
                    tile.device.structure_id.value != 0U ||
                    tile.ecology.plant_id.value != 0U ||
                    tile.ecology.ground_cover_type_id != 0U)
                {
                    result.blocked_mask |= tile_bit;
                }
                return;
            }

            if (occupancy_layer == PlacementOccupancyLayer::Structure)
            {
                if (!tile.static_data.traversable ||
                    tile.device.structure_id.value != 0U)
                {
                    result.blocked_mask |= tile_bit;
                }
                return;
            }

            result.blocked_mask |= tile_bit;
        });

    return result;
}
}  // namespace gs1
