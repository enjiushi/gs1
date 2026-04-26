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

[[nodiscard]] inline constexpr bool is_power_of_two_tile_span(std::uint8_t span) noexcept
{
    return span != 0U && (span & static_cast<std::uint8_t>(span - 1U)) == 0U;
}

[[nodiscard]] inline constexpr bool is_power_of_two_tile_footprint(TileFootprint footprint) noexcept
{
    const TileFootprint normalized = normalize_tile_footprint(footprint);
    return is_power_of_two_tile_span(normalized.width) &&
        is_power_of_two_tile_span(normalized.height);
}

[[nodiscard]] inline constexpr std::int32_t align_tile_axis_to_span(
    std::int32_t coord,
    std::uint8_t span) noexcept
{
    const std::int32_t normalized_span =
        static_cast<std::int32_t>(span == 0U ? 1U : span);
    const std::int32_t remainder = coord % normalized_span;
    if (remainder == 0)
    {
        return coord;
    }

    return coord - (remainder < 0 ? (remainder + normalized_span) : remainder);
}

[[nodiscard]] inline constexpr TileCoord align_tile_anchor_to_footprint(
    TileCoord anchor,
    TileFootprint footprint) noexcept
{
    const TileFootprint normalized = normalize_tile_footprint(footprint);
    return TileCoord {
        align_tile_axis_to_span(anchor.x, normalized.width),
        align_tile_axis_to_span(anchor.y, normalized.height)};
}

[[nodiscard]] inline constexpr bool is_tile_anchor_aligned_to_footprint(
    TileCoord anchor,
    TileFootprint footprint) noexcept
{
    const TileCoord aligned_anchor = align_tile_anchor_to_footprint(anchor, footprint);
    return aligned_anchor.x == anchor.x && aligned_anchor.y == anchor.y;
}

[[nodiscard]] inline constexpr std::uint8_t resolve_tile_footprint_distance_scale(
    TileFootprint footprint) noexcept
{
    const TileFootprint normalized = normalize_tile_footprint(footprint);
    return normalized.width >= normalized.height ? normalized.width : normalized.height;
}

[[nodiscard]] inline constexpr std::uint8_t scale_tile_distance_by_footprint_multiple(
    std::uint8_t authored_distance,
    TileFootprint footprint) noexcept
{
    const std::uint16_t scaled_distance =
        static_cast<std::uint16_t>(authored_distance) *
        static_cast<std::uint16_t>(resolve_tile_footprint_distance_scale(footprint));
    return scaled_distance >= 0xffU
        ? 0xffU
        : static_cast<std::uint8_t>(scaled_distance);
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

[[nodiscard]] inline TileFootprint resolve_plant_tile_footprint(PlantId plant_id) noexcept
{
    const auto* plant_def = find_plant_def(plant_id);
    if (plant_def == nullptr)
    {
        return TileFootprint {1U, 1U};
    }

    return normalize_tile_footprint(
        TileFootprint {plant_def->footprint_width, plant_def->footprint_height});
}

[[nodiscard]] inline TileFootprint resolve_structure_tile_footprint(
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

[[nodiscard]] inline TileFootprint resolve_placement_reservation_footprint(
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
