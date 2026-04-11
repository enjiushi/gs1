#pragma once

#include "support/id_types.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace gs1
{
struct TileGridState final
{
    std::uint32_t width {0};
    std::uint32_t height {0};

    std::vector<std::uint32_t> terrain_type_ids {};
    std::vector<std::uint8_t> tile_traversable {};
    std::vector<std::uint8_t> tile_plantable {};
    std::vector<std::uint8_t> tile_reserved_by_structure {};

    std::vector<float> tile_soil_fertility {};
    std::vector<float> tile_moisture {};
    std::vector<float> tile_soil_salinity {};
    std::vector<float> tile_sand_burial {};

    std::vector<float> tile_heat {};
    std::vector<float> tile_wind {};
    std::vector<float> tile_dust {};

    std::vector<PlantId> plant_type_ids {};
    std::vector<std::uint32_t> ground_cover_type_ids {};
    std::vector<float> tile_plant_density {};
    std::vector<float> growth_pressure {};

    std::vector<StructureId> structure_type_ids {};
    std::vector<float> device_integrity {};
    std::vector<float> device_efficiency {};
    std::vector<float> device_stored_water {};

    [[nodiscard]] std::size_t tile_count() const noexcept
    {
        return static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    }

    [[nodiscard]] std::size_t to_index(TileCoord coord) const noexcept
    {
        return static_cast<std::size_t>(coord.y) * static_cast<std::size_t>(width) +
            static_cast<std::size_t>(coord.x);
    }
};
}  // namespace gs1
