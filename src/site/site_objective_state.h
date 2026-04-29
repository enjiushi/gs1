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
    double completion_hold_minutes {0.0};
    double completion_hold_progress_minutes {0.0};
    double paused_main_timer_minutes {0.0};
    double last_evaluated_world_time_minutes {0.0};
    std::int32_t target_cash_points {0};
    SiteObjectiveTargetEdge target_edge {SiteObjectiveTargetEdge::East};
    std::uint8_t target_band_width {0U};
    float highway_max_average_sand_cover {0.0f};
    float last_target_average_sand_level {0.0f};
    bool has_hold_baseline {false};
    std::vector<std::uint32_t> target_tile_indices {};
    std::vector<std::uint8_t> target_tile_mask {};
    std::vector<std::uint32_t> connection_start_tile_indices {};
    std::vector<std::uint8_t> connection_start_tile_mask {};
    std::vector<std::uint32_t> connection_goal_tile_indices {};
    std::vector<std::uint8_t> connection_goal_tile_mask {};
};
}  // namespace gs1
