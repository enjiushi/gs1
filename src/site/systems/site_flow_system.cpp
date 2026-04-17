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
constexpr float k_worker_position_epsilon = 0.0001f;

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

bool has_active_action(const ActionState& action_state) noexcept
{
    return action_state.current_action_id.has_value() &&
        action_state.action_kind != ActionKind::None;
}

bool action_waits_for_worker_approach(const ActionState& action_state) noexcept
{
    return has_active_action(action_state) &&
        !action_state.awaiting_placement_reservation &&
        !action_state.started_at_world_minute.has_value() &&
        action_state.approach_tile.has_value();
}

bool has_pending_device_storage_open(const InventoryState& inventory) noexcept
{
    return inventory.pending_device_storage_open.active &&
        inventory.pending_device_storage_open.storage_id != 0U;
}

float facing_degrees_for_direction(float direction_x, float direction_y) noexcept
{
    if (std::fabs(direction_x) <= k_worker_position_epsilon &&
        std::fabs(direction_y) <= k_worker_position_epsilon)
    {
        return 0.0f;
    }

    return std::atan2(direction_x, direction_y) * k_radians_to_degrees;
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
    if (width == 0U || height == 0U)
    {
        return;
    }

    auto worker = context.world.read_worker();
    const auto& action_state = context.world.read_action();
    const auto& inventory = context.world.read_inventory();
    const float movement_step =
        k_worker_move_speed_tiles_per_second * static_cast<float>(context.fixed_step_seconds);

    std::optional<TileCoord> move_goal_tile {};
    if (action_waits_for_worker_approach(action_state))
    {
        move_goal_tile = action_state.approach_tile;
    }
    else if (has_active_action(action_state))
    {
        if (action_state.target_tile.has_value())
        {
            const float face_dx =
                static_cast<float>(action_state.target_tile->x) - worker.position.tile_x;
            const float face_dy =
                static_cast<float>(action_state.target_tile->y) - worker.position.tile_y;
            const float next_facing = facing_degrees_for_direction(face_dx, face_dy);
            if (std::fabs(next_facing - worker.position.facing_degrees) > k_worker_position_epsilon)
            {
                worker.position.facing_degrees = next_facing;
                context.world.write_worker(worker);
                context.world.mark_projection_dirty(SITE_PROJECTION_UPDATE_WORKER);
            }
        }
        return;
    }
    else if (has_pending_device_storage_open(inventory))
    {
        move_goal_tile = inventory.pending_device_storage_open.approach_tile;
    }

    float direction_x = 0.0f;
    float direction_y = 0.0f;
    float target_x = worker.position.tile_x;
    float target_y = worker.position.tile_y;

    if (move_goal_tile.has_value())
    {
        if (!context.world.tile_coord_in_bounds(*move_goal_tile) ||
            !context.world.read_tile(*move_goal_tile).static_data.traversable)
        {
            return;
        }

        const float delta_x =
            static_cast<float>(move_goal_tile->x) - worker.position.tile_x;
        const float delta_y =
            static_cast<float>(move_goal_tile->y) - worker.position.tile_y;
        const float distance_squared = delta_x * delta_x + delta_y * delta_y;
        if (distance_squared <= k_worker_position_epsilon)
        {
            target_x = static_cast<float>(move_goal_tile->x);
            target_y = static_cast<float>(move_goal_tile->y);
        }
        else
        {
            const float distance = std::sqrt(distance_squared);
            const float step = std::min(movement_step, distance);
            direction_x = delta_x / distance;
            direction_y = delta_y / distance;
            target_x = worker.position.tile_x + direction_x * step;
            target_y = worker.position.tile_y + direction_y * step;
        }
    }
    else
    {
        if (!context.move_direction.present)
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
        direction_x = move_x / move_length;
        direction_y = move_y / move_length;
        const float max_x = static_cast<float>(width - 1U);
        const float max_y = static_cast<float>(height - 1U);
        target_x =
            std::clamp(worker.position.tile_x + direction_x * movement_step, 0.0f, max_x);
        target_y =
            std::clamp(worker.position.tile_y + direction_y * movement_step, 0.0f, max_y);
    }

    const TileCoord target_tile {
        static_cast<std::int32_t>(std::lround(target_x)),
        static_cast<std::int32_t>(std::lround(target_y))};
    if (!context.world.tile_coord_in_bounds(target_tile) ||
        !context.world.read_tile(target_tile).static_data.traversable)
    {
        return;
    }

    const bool position_changed =
        std::fabs(target_x - worker.position.tile_x) > k_worker_position_epsilon ||
        std::fabs(target_y - worker.position.tile_y) > k_worker_position_epsilon;
    const float next_facing =
        facing_degrees_for_direction(direction_x, direction_y);
    const bool facing_changed =
        std::fabs(next_facing - worker.position.facing_degrees) > k_worker_position_epsilon &&
        (std::fabs(direction_x) > k_worker_position_epsilon ||
            std::fabs(direction_y) > k_worker_position_epsilon);
    if (!position_changed && !facing_changed)
    {
        return;
    }

    worker.position.tile_x = target_x;
    worker.position.tile_y = target_y;
    worker.position.tile_coord = target_tile;
    if (facing_changed)
    {
        worker.position.facing_degrees = next_facing;
    }
    context.world.write_worker(worker);
    context.world.mark_projection_dirty(SITE_PROJECTION_UPDATE_WORKER);
}
}  // namespace gs1
