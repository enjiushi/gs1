#pragma once

#include <cstdint>
#include <vector>

namespace gs1
{
struct PlantWeatherContributionMetaState final
{
    std::uint8_t last_wind_direction_sector {0xffU};
    bool full_rebuild_pending {true};
};

struct PlantWeatherContributionState final
{
    std::vector<std::uint32_t> dirty_tile_indices {};
    std::vector<std::uint8_t> dirty_tile_mask {};
    std::uint8_t last_wind_direction_sector {0xffU};
    bool full_rebuild_pending {true};
};
}  // namespace gs1
