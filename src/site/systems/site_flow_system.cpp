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
}  // namespace

void SiteFlowSystem::run(SiteSystemContext<SiteFlowSystem>& context)
{
    auto& clock = context.world.own_time();
    clock.world_time_minutes += k_step_minutes;
    clock.day_index =
        static_cast<std::uint32_t>(clock.world_time_minutes / k_minutes_per_day);
    clock.day_phase = resolve_day_phase(clock.world_time_minutes);

    const std::uint32_t width = context.world.tile_width();
    const std::uint32_t height = context.world.tile_height();
    if (!context.move_direction.present || width == 0U || height == 0U)
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

    auto worker = context.world.read_worker();
    const float max_x = static_cast<float>(width - 1U);
    const float max_y = static_cast<float>(height - 1U);
    const float target_x =
        std::clamp(worker.position.tile_x + direction_x * movement_step, 0.0f, max_x);
    const float target_y =
        std::clamp(worker.position.tile_y + direction_y * movement_step, 0.0f, max_y);

    const TileCoord target_tile {
        static_cast<std::int32_t>(std::lround(target_x)),
        static_cast<std::int32_t>(std::lround(target_y))};
    if (!context.world.tile_coord_in_bounds(target_tile))
    {
        return;
    }

    if (!context.world.read_tile(target_tile).static_data.traversable)
    {
        return;
    }

    const bool position_changed =
        std::fabs(target_x - worker.position.tile_x) > 0.0001f ||
        std::fabs(target_y - worker.position.tile_y) > 0.0001f;
    if (!position_changed)
    {
        return;
    }

    worker.position.tile_x = target_x;
    worker.position.tile_y = target_y;
    worker.position.tile_coord = target_tile;
    worker.position.facing_degrees = std::atan2(direction_x, direction_y) * k_radians_to_degrees;
    context.world.write_worker(worker);
    context.world.mark_projection_dirty(SITE_PROJECTION_UPDATE_WORKER);
}
}  // namespace gs1
