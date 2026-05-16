#include "site/systems/site_flow_system.h"

#include "runtime/game_runtime.h"
#include "site/defs/site_action_defs.h"

#include <algorithm>
#include <cmath>

namespace gs1
{
namespace
{
constexpr float k_worker_move_speed_tiles_per_second = 3.5f;
constexpr float k_radians_to_degrees = 57.2957795f;
constexpr float k_worker_position_epsilon = 0.0001f;

bool has_active_action(ConstActionStateRef action_state) noexcept
{
    return action_state.has_current_action_id &&
        action_state.action_kind != ActionKind::None;
}

bool action_waits_for_worker_approach(ConstActionStateRef action_state) noexcept
{
    return has_active_action(action_state) &&
        !action_state.awaiting_placement_reservation &&
        !action_state.has_started_at_world_minute &&
        action_state.has_approach_tile;
}

bool action_blocks_worker_movement(ConstActionStateRef action_state) noexcept
{
    return has_active_action(action_state) &&
        action_impacts_worker_movement(action_state.action_kind);
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

const char* SiteFlowSystem::name() const noexcept
{
    return access().system_name.data();
}

GameMessageSubscriptionSpan SiteFlowSystem::subscribed_game_messages() const noexcept
{
    return {};
}

HostMessageSubscriptionSpan SiteFlowSystem::subscribed_host_messages() const noexcept
{
    static constexpr Gs1HostMessageType subscriptions[] = {GS1_HOST_EVENT_SITE_MOVE_DIRECTION};
    return subscriptions;
}

std::optional<Gs1RuntimeProfileSystemId> SiteFlowSystem::profile_system_id() const noexcept
{
    return GS1_RUNTIME_PROFILE_SYSTEM_SITE_FLOW;
}

std::optional<std::uint32_t> SiteFlowSystem::fixed_step_order() const noexcept
{
    return 2U;
}

Gs1Status SiteFlowSystem::process_game_message(
    RuntimeInvocation& invocation,
    const GameMessage& message)
{
    (void)invocation;
    (void)message;
    return GS1_STATUS_OK;
}

Gs1Status SiteFlowSystem::process_host_message(
    RuntimeInvocation& invocation,
    const Gs1HostMessage& message)
{
    if (message.type != GS1_HOST_EVENT_SITE_MOVE_DIRECTION)
    {
        return GS1_STATUS_OK;
    }

    SiteWorldAccess<SiteFlowSystem> world {invocation};
    if (!world.has_world())
    {
        return GS1_STATUS_OK;
    }

    const auto& payload = message.payload.site_move_direction;
    auto& move_direction = runtime_invocation_state_ref<RuntimeMoveDirectionTag>(invocation);
    move_direction.world_move_x = payload.world_move_x;
    move_direction.world_move_y = payload.world_move_y;
    move_direction.world_move_z = payload.world_move_z;
    move_direction.present = true;
    return GS1_STATUS_OK;
}

void SiteFlowSystem::run(RuntimeInvocation& invocation)
{
    auto access = make_game_state_access<SiteFlowSystem>(invocation);
    SiteWorldAccess<SiteFlowSystem> world {invocation};
    if (!world.has_world())
    {
        return;
    }

    const auto move_direction = access.template read<RuntimeMoveDirectionTag>();
    const SiteMoveDirectionInput move_direction_input {
        move_direction.world_move_x,
        move_direction.world_move_y,
        move_direction.world_move_z,
        move_direction.present};
    const double fixed_step_seconds = access.template read<RuntimeFixedStepSecondsTag>();
    const std::uint32_t width = world.tile_width();
    const std::uint32_t height = world.tile_height();
    if (width == 0U || height == 0U)
    {
        return;
    }

    auto worker = world.read_worker();
    const auto action_state = world.read_action();
    const float movement_step =
        k_worker_move_speed_tiles_per_second * static_cast<float>(fixed_step_seconds);

    std::optional<TileCoord> move_goal_tile {};
    if (action_waits_for_worker_approach(action_state))
    {
        move_goal_tile = action_state.approach_tile;
    }
    else if (action_blocks_worker_movement(action_state))
    {
        if (action_state.has_target_tile)
        {
            const float face_dx =
                static_cast<float>(action_state.target_tile.x) - worker.position.tile_x;
            const float face_dy =
                static_cast<float>(action_state.target_tile.y) - worker.position.tile_y;
            const float next_facing = facing_degrees_for_direction(face_dx, face_dy);
            if (std::fabs(next_facing - worker.position.facing_degrees) > k_worker_position_epsilon)
            {
                worker.position.facing_degrees = next_facing;
                world.write_worker(worker);
            }
        }
        return;
    }

    float direction_x = 0.0f;
    float direction_y = 0.0f;
    float target_x = worker.position.tile_x;
    float target_y = worker.position.tile_y;

    if (move_goal_tile.has_value())
    {
        if (!world.tile_coord_in_bounds(*move_goal_tile) ||
            !world.read_tile(*move_goal_tile).static_data.traversable)
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
        if (!move_direction_input.present)
        {
            return;
        }

        const float move_x = move_direction_input.world_move_x;
        const float move_y = move_direction_input.world_move_y;
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
    if (!world.tile_coord_in_bounds(target_tile) ||
        !world.read_tile(target_tile).static_data.traversable)
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
    world.write_worker(worker);
}
}  // namespace gs1

