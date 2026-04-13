#include "site/systems/site_flow_system.h"

#include "site/site_projection_update_flags.h"
#include "site/site_run_state.h"

#include <algorithm>
#include <cmath>

namespace gs1
{
namespace
{
constexpr double k_step_minutes = 0.2;
constexpr double k_minutes_per_day = 1440.0;
constexpr float k_worker_move_speed_tiles_per_second = 3.5f;
constexpr float k_radians_to_degrees = 57.2957795f;

DayPhase resolve_day_phase(double world_time_minutes)
{
    const double minute_in_day = std::fmod(world_time_minutes, k_minutes_per_day);

    if (minute_in_day < 360.0)
    {
        return DayPhase::Dawn;
    }
    if (minute_in_day < 900.0)
    {
        return DayPhase::Day;
    }
    if (minute_in_day < 1140.0)
    {
        return DayPhase::Dusk;
    }
    return DayPhase::Night;
}

bool tile_coord_in_bounds(const TileGridState& tile_grid, TileCoord coord) noexcept
{
    return coord.x >= 0 &&
        coord.y >= 0 &&
        static_cast<std::uint32_t>(coord.x) < tile_grid.width &&
        static_cast<std::uint32_t>(coord.y) < tile_grid.height;
}
}  // namespace

void SiteFlowSystem::run(SiteSystemContext& context)
{
    auto& site_run = context.site_run;

    site_run.clock.world_time_minutes += k_step_minutes;
    site_run.clock.day_index =
        static_cast<std::uint32_t>(site_run.clock.world_time_minutes / k_minutes_per_day);
    site_run.clock.day_phase = resolve_day_phase(site_run.clock.world_time_minutes);

    if (!context.move_direction.present || site_run.tile_grid.width == 0U || site_run.tile_grid.height == 0U)
    {
        return;
    }

    const float move_x = context.move_direction.world_move_x;
    const float move_y = context.move_direction.world_move_y;
    const float move_length_squared = move_x * move_x + move_y * move_y;
    if (move_length_squared <= 0.0001f)
    {
        return;
    }

    const float move_length = std::sqrt(move_length_squared);
    const float direction_x = move_x / move_length;
    const float direction_y = move_y / move_length;
    const float movement_step =
        k_worker_move_speed_tiles_per_second * static_cast<float>(context.fixed_step_seconds);

    const float max_x = static_cast<float>(site_run.tile_grid.width - 1U);
    const float max_y = static_cast<float>(site_run.tile_grid.height - 1U);
    const float target_x = std::clamp(site_run.worker.tile_position_x + direction_x * movement_step, 0.0f, max_x);
    const float target_y = std::clamp(site_run.worker.tile_position_y + direction_y * movement_step, 0.0f, max_y);

    const TileCoord target_tile {
        static_cast<std::int32_t>(std::lround(target_x)),
        static_cast<std::int32_t>(std::lround(target_y))};
    if (!tile_coord_in_bounds(site_run.tile_grid, target_tile))
    {
        return;
    }

    const auto target_index = site_run.tile_grid.to_index(target_tile);
    if (!site_run.tile_grid.tile_traversable.empty() &&
        site_run.tile_grid.tile_traversable[target_index] == 0U)
    {
        return;
    }

    const bool position_changed =
        std::fabs(target_x - site_run.worker.tile_position_x) > 0.0001f ||
        std::fabs(target_y - site_run.worker.tile_position_y) > 0.0001f;
    if (!position_changed)
    {
        return;
    }

    site_run.worker.tile_position_x = target_x;
    site_run.worker.tile_position_y = target_y;
    site_run.worker.tile_coord = target_tile;
    site_run.worker.facing_degrees = std::atan2(direction_x, direction_y) * k_radians_to_degrees;
    site_run.pending_projection_update_flags |= SITE_PROJECTION_UPDATE_WORKER;
}
}  // namespace gs1
