#pragma once

#include "content/defs/plant_defs.h"
#include "content/defs/structure_defs.h"
#include "messages/game_message.h"

#include <cstdint>
#include <type_traits>
#include <utility>

namespace gs1
{
struct TileFootprint final
{
    std::uint8_t width;
    std::uint8_t height;
};

[[nodiscard]] inline constexpr TileFootprint normalize_tile_footprint(TileFootprint footprint) noexcept
{
        return TileFootprint {
        footprint.width == 0U ? 1U : footprint.width,
        footprint.height == 0U ? 1U : footprint.height};
}

template <typename Func>
inline void for_each_tile_in_footprint(
    TileCoord anchor,
    TileFootprint footprint,
    Func&& func)
{
    const TileFootprint normalized = normalize_tile_footprint(footprint);
    for (std::int32_t offset_y = 0; offset_y < static_cast<std::int32_t>(normalized.height); ++offset_y)
    {
        for (std::int32_t offset_x = 0; offset_x < static_cast<std::int32_t>(normalized.width); ++offset_x)
        {
            func(TileCoord {anchor.x + offset_x, anchor.y + offset_y});
        }
    }
}

[[nodiscard]] inline constexpr TileFootprint resolve_plant_tile_footprint(PlantId plant_id) noexcept
{
    const auto* plant_def = find_plant_def(plant_id);
    if (plant_def == nullptr)
    {
        return TileFootprint {1U, 1U};
    }

    return normalize_tile_footprint(
        TileFootprint {plant_def->footprint_width, plant_def->footprint_height});
}

[[nodiscard]] inline constexpr TileFootprint resolve_structure_tile_footprint(
    StructureId structure_id) noexcept
{
    const auto* structure_def = find_structure_def(structure_id);
    if (structure_def == nullptr)
    {
        return TileFootprint {1U, 1U};
    }

    return normalize_tile_footprint(
        TileFootprint {structure_def->footprint_width, structure_def->footprint_height});
}

[[nodiscard]] inline constexpr TileFootprint resolve_placement_reservation_footprint(
    PlacementOccupancyLayer occupancy_layer,
    PlacementReservationSubjectKind subject_kind,
    std::uint32_t subject_id) noexcept
{
    switch (subject_kind)
    {
    case PlacementReservationSubjectKind::PlantType:
        return occupancy_layer == PlacementOccupancyLayer::GroundCover
            ? resolve_plant_tile_footprint(PlantId {subject_id})
            : TileFootprint {1U, 1U};

    case PlacementReservationSubjectKind::StructureType:
        return occupancy_layer == PlacementOccupancyLayer::Structure
            ? resolve_structure_tile_footprint(StructureId {subject_id})
            : TileFootprint {1U, 1U};

    case PlacementReservationSubjectKind::GroundCoverType:
    case PlacementReservationSubjectKind::None:
    default:
        return TileFootprint {1U, 1U};
    }
}

static_assert(std::is_standard_layout_v<TileFootprint>, "TileFootprint must remain standard layout.");
static_assert(std::is_trivial_v<TileFootprint>, "TileFootprint must remain trivial.");
static_assert(std::is_trivially_copyable_v<TileFootprint>, "TileFootprint must remain trivially copyable.");
}  // namespace gs1
