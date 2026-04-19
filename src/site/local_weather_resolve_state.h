#pragma once

#include <cstdint>
#include <vector>

namespace gs1
{
struct LocalWeatherResolveState final
{
    std::vector<std::uint32_t> pending_tile_indices {};
    std::vector<std::uint8_t> pending_tile_mask {};
    std::uint32_t next_full_refresh_tile_index {0};
    std::uint32_t full_refresh_remaining_tiles {0};
    float last_base_heat {0.0f};
    float last_base_wind {0.0f};
    float last_base_dust {0.0f};
    float last_base_wind_direction_degrees {0.0f};
    bool base_inputs_cached {false};
};
}  // namespace gs1
