#include "site/systems/site_completion_system.h"

#include "runtime/game_runtime.h"
#include "site/site_run_state.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace gs1
{
namespace
{
constexpr float k_objective_progress_epsilon = 0.0001f;

bool has_pending_site_transition_message(
    const GameMessageQueue& message_queue,
    std::uint32_t site_id)
{
    for (const auto& message : message_queue)
    {
        if (message.type == GameMessageType::SiteAttemptEnded &&
            message.payload_as<SiteAttemptEndedMessage>().site_id == site_id)
        {
            return true;
        }
    }

    return false;
}

void enqueue_site_attempt_result(
    GameMessageQueue& message_queue,
    std::uint32_t site_id,
    Gs1SiteAttemptResult result)
{
    GameMessage message {};
    message.type = GameMessageType::SiteAttemptEnded;
    message.set_payload(SiteAttemptEndedMessage {site_id, result});
    message_queue.push_back(message);
}

bool tile_has_objective_occupant(const SiteWorld::TileData& tile) noexcept
{
    return tile.ecology.plant_id.value != 0U || tile.ecology.ground_cover_type_id != 0U;
}

void update_objective_progress(
    SiteWorldAccess<SiteCompletionSystem>& world,
    float normalized_progress) noexcept
{
    const float clamped_progress = std::clamp(normalized_progress, 0.0f, 1.0f);
    auto& counters = world.own_counters();
    if (std::fabs(counters.objective_progress_normalized - clamped_progress) <=
        k_objective_progress_epsilon)
    {
        return;
    }

    counters.objective_progress_normalized = clamped_progress;
}

float average_target_sand_level(
    const SiteWorldAccess<SiteCompletionSystem>& world,
    bool use_highway_cover) noexcept
{
    const auto& objective = world.read_objective();
    if (objective.target_tile_indices.empty())
    {
        return 0.0f;
    }

    float total = 0.0f;
    std::uint32_t count = 0U;
    const auto tile_count = world.tile_count();
    for (const auto tile_index : objective.target_tile_indices)
    {
        if (tile_index >= tile_count)
        {
            continue;
        }

        const auto tile = world.read_tile_at_index(tile_index);
        total += use_highway_cover ? tile.ecology.soil_fertility : tile.ecology.sand_burial;
        ++count;
    }

    return count == 0U ? 0.0f : (total / static_cast<float>(count));
}

bool green_wall_regions_connected(const SiteWorldAccess<SiteCompletionSystem>& world)
{
    const auto& objective = world.read_objective();
    const auto tile_count = world.tile_count();
    if (objective.connection_start_tile_indices.empty() ||
        objective.connection_goal_tile_indices.empty() ||
        objective.connection_start_tile_mask.size() != tile_count ||
        objective.connection_goal_tile_mask.size() != tile_count)
    {
        return false;
    }

    std::vector<std::uint8_t> visited(tile_count, 0U);
    std::vector<std::uint32_t> frontier {};
    frontier.reserve(tile_count);

    for (const auto tile_index : objective.connection_start_tile_indices)
    {
        if (tile_index >= tile_count ||
            visited[tile_index] != 0U ||
            !tile_has_objective_occupant(world.read_tile_at_index(tile_index)))
        {
            continue;
        }

        visited[tile_index] = 1U;
        frontier.push_back(tile_index);
    }

    for (std::size_t frontier_index = 0; frontier_index < frontier.size(); ++frontier_index)
    {
        const auto tile_index = frontier[frontier_index];
        if (objective.connection_goal_tile_mask[tile_index] != 0U)
        {
            return true;
        }

        const auto coord = world.tile_coord(tile_index);
        const TileCoord neighbors[] = {
            TileCoord {coord.x, coord.y - 1},
            TileCoord {coord.x + 1, coord.y},
            TileCoord {coord.x, coord.y + 1},
            TileCoord {coord.x - 1, coord.y},
        };

        for (const auto neighbor : neighbors)
        {
            if (!world.tile_coord_in_bounds(neighbor))
            {
                continue;
            }

            const auto neighbor_index = static_cast<std::uint32_t>(world.tile_index(neighbor));
            if (visited[neighbor_index] != 0U ||
                !tile_has_objective_occupant(world.read_tile(neighbor)))
            {
                continue;
            }

            visited[neighbor_index] = 1U;
            frontier.push_back(neighbor_index);
        }
    }

    return false;
}

float normalized_cash_target_progress(
    ConstSiteObjectiveStateRef objective,
    ConstEconomyStateRef economy) noexcept
{
    if (objective.target_cash_points <= 0)
    {
        return 0.0f;
    }

    return static_cast<float>(std::clamp(
        static_cast<double>(std::max(economy.current_cash, 0)) /
            static_cast<double>(objective.target_cash_points),
        0.0,
        1.0));
}

void run_green_wall_connection(
    SiteWorldAccess<SiteCompletionSystem>& world,
    GameMessageQueue& message_queue,
    const SiteClockState& clock)
{
    auto objective = world.own_objective();
    const double delta_minutes = std::max(
        0.0,
        clock.world_time_minutes - objective.last_evaluated_world_time_minutes);
    objective.last_evaluated_world_time_minutes = clock.world_time_minutes;

    const bool connected = green_wall_regions_connected(world);
    const float average_sand_level = average_target_sand_level(world, false);
    if (!connected)
    {
        objective.completion_hold_progress_minutes = 0.0;
        objective.has_hold_baseline = false;
        update_objective_progress(world, 0.0f);
    }
    else
    {
        if (!objective.has_hold_baseline)
        {
            objective.last_target_average_sand_level = average_sand_level;
            objective.has_hold_baseline = true;
        }

        if (objective.has_hold_baseline &&
            average_sand_level > objective.last_target_average_sand_level + k_objective_progress_epsilon)
        {
            objective.completion_hold_progress_minutes = 0.0;
            objective.last_target_average_sand_level = average_sand_level;
        }
        else
        {
            objective.last_target_average_sand_level =
                std::min(objective.last_target_average_sand_level, average_sand_level);
            objective.completion_hold_progress_minutes += delta_minutes;
            objective.paused_main_timer_minutes += delta_minutes;
        }

        const float hold_progress =
            objective.completion_hold_minutes <= 0.0
            ? 1.0f
            : static_cast<float>(
                  std::clamp(
                      objective.completion_hold_progress_minutes / objective.completion_hold_minutes,
                      0.0,
                      1.0));
        update_objective_progress(world, 0.5f + hold_progress * 0.5f);

        if (objective.completion_hold_minutes <= 0.0 ||
            objective.completion_hold_progress_minutes + k_objective_progress_epsilon >=
                objective.completion_hold_minutes)
        {
            enqueue_site_attempt_result(
                message_queue,
                world.site_id_value(),
                GS1_SITE_ATTEMPT_RESULT_COMPLETED);
            return;
        }
    }

    if (objective.time_limit_minutes <= 0.0)
    {
        return;
    }

    const double effective_elapsed_minutes =
        std::max(0.0, clock.world_time_minutes - objective.paused_main_timer_minutes);
    if (effective_elapsed_minutes < objective.time_limit_minutes)
    {
        return;
    }

    enqueue_site_attempt_result(
        message_queue,
        world.site_id_value(),
        GS1_SITE_ATTEMPT_RESULT_FAILED);
}
}  // namespace

const char* SiteCompletionSystem::name() const noexcept
{
    return access().system_name.data();
}

GameMessageSubscriptionSpan SiteCompletionSystem::subscribed_game_messages() const noexcept
{
    return {};
}

HostMessageSubscriptionSpan SiteCompletionSystem::subscribed_host_messages() const noexcept
{
    return {};
}

std::optional<Gs1RuntimeProfileSystemId> SiteCompletionSystem::profile_system_id() const noexcept
{
    return GS1_RUNTIME_PROFILE_SYSTEM_SITE_COMPLETION;
}

std::optional<std::uint32_t> SiteCompletionSystem::fixed_step_order() const noexcept
{
    return 20U;
}

Gs1Status SiteCompletionSystem::process_game_message(
    RuntimeInvocation& invocation,
    const GameMessage& message)
{
    (void)invocation;
    (void)message;
    return GS1_STATUS_OK;
}

Gs1Status SiteCompletionSystem::process_host_message(
    RuntimeInvocation& invocation,
    const Gs1HostMessage& message)
{
    (void)invocation;
    (void)message;
    return GS1_STATUS_OK;
}

void SiteCompletionSystem::run(RuntimeInvocation& invocation)
{
    SiteWorldAccess<SiteCompletionSystem> world {invocation};
    if (!world.has_world())
    {
        return;
    }

    auto& message_queue = invocation.game_message_queue();
    const auto& counters = world.read_counters();
    const auto& objective = world.read_objective();
    const auto& clock = world.read_time();
    if (world.run_status() != SiteRunStatus::Active ||
        has_pending_site_transition_message(
            message_queue,
            world.site_id_value()))
    {
        return;
    }

    switch (objective.type)
    {
    case SiteObjectiveType::DenseRestoration:
        if (counters.site_completion_tile_threshold == 0U ||
            counters.fully_grown_tile_count < counters.site_completion_tile_threshold)
        {
            return;
        }

        enqueue_site_attempt_result(
            message_queue,
            world.site_id_value(),
            GS1_SITE_ATTEMPT_RESULT_COMPLETED);
        return;

    case SiteObjectiveType::HighwayProtection:
    {
        const float cover_threshold =
            objective.highway_max_average_sand_cover > 0.0f &&
                objective.highway_max_average_sand_cover <= 1.0f
            ? objective.highway_max_average_sand_cover * 100.0f
            : objective.highway_max_average_sand_cover;
        if (cover_threshold <= 0.0f ||
            objective.time_limit_minutes <= 0.0 ||
            objective.target_tile_indices.empty())
        {
            return;
        }

        if (counters.highway_average_sand_cover >= cover_threshold)
        {
            enqueue_site_attempt_result(
                message_queue,
                world.site_id_value(),
                GS1_SITE_ATTEMPT_RESULT_FAILED);
            return;
        }

        if (clock.world_time_minutes < objective.time_limit_minutes)
        {
            return;
        }

        enqueue_site_attempt_result(
            message_queue,
            world.site_id_value(),
            GS1_SITE_ATTEMPT_RESULT_COMPLETED);
        return;
    }

    case SiteObjectiveType::GreenWallConnection:
        run_green_wall_connection(world, message_queue, clock);
        return;

    case SiteObjectiveType::PureSurvival:
        if (objective.time_limit_minutes <= 0.0)
        {
            return;
        }

        update_objective_progress(
            world,
            static_cast<float>(std::clamp(
                clock.world_time_minutes / objective.time_limit_minutes,
                0.0,
                1.0)));
        if (clock.world_time_minutes < objective.time_limit_minutes)
        {
            return;
        }

        enqueue_site_attempt_result(
            message_queue,
            world.site_id_value(),
            GS1_SITE_ATTEMPT_RESULT_COMPLETED);
        return;

    case SiteObjectiveType::CashTargetSurvival:
    {
        const auto& economy = world.read_economy();
        update_objective_progress(
            world,
            normalized_cash_target_progress(objective, economy));
        if (objective.target_cash_points <= 0 ||
            economy.current_cash < objective.target_cash_points)
        {
            return;
        }

        enqueue_site_attempt_result(
            message_queue,
            world.site_id_value(),
            GS1_SITE_ATTEMPT_RESULT_COMPLETED);
        return;
    }

    default:
        return;
    }
}
}  // namespace gs1

