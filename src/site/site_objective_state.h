#pragma once

#include "support/site_objective_types.h"

#include <cstdint>
#include <vector>

namespace gs1
{
struct SiteObjectiveState final
{
    SiteObjectiveType type {SiteObjectiveType::DenseRestoration};
    double time_limit_minutes {0.0};
    SiteObjectiveTargetEdge target_edge {SiteObjectiveTargetEdge::East};
    std::uint8_t target_band_width {0U};
    float highway_max_average_sand_cover {0.0f};
    std::vector<std::uint32_t> target_tile_indices {};
    std::vector<std::uint8_t> target_tile_mask {};
};
}  // namespace gs1
