#include "site/systems/task_board_system.h"

#include "site/site_projection_update_flags.h"
#include "site/site_run_state.h"

#include <algorithm>

namespace gs1
{
namespace
{
constexpr TaskInstanceId k_site1_task_instance_id {1U};
constexpr TaskTemplateId k_site1_task_template_id {1U};
constexpr FactionId k_site1_publisher_id {1U};
constexpr std::uint32_t k_site1_task_tier_id {1U};

std::uint32_t normalized_task_target(std::uint32_t requested_target) noexcept
{
    return requested_target == 0U ? 1U : requested_target;
}

TaskInstanceState make_site1_task(const SiteRunState& site_run) noexcept
{
    TaskInstanceState task {};
    task.task_instance_id = k_site1_task_instance_id;
    task.task_template_id = k_site1_task_template_id;
    task.publisher_faction_id = k_site1_publisher_id;
    task.task_tier_id = k_site1_task_tier_id;
    task.target_amount = normalized_task_target(site_run.counters.site_completion_tile_threshold);
    task.current_progress_amount =
        std::min(task.target_amount, site_run.counters.fully_grown_tile_count);
    task.runtime_list_kind = TaskRuntimeListKind::Visible;
    return task;
}

void reset_task_board(TaskBoardState& board) noexcept
{
    board.visible_tasks.clear();
    board.accepted_task_ids.clear();
    board.completed_task_ids.clear();
    board.active_chain_state.reset();
    board.task_pool_size = 0U;
    board.minutes_until_next_refresh = 0.0;
}

TaskInstanceState* find_task_instance(TaskBoardState& board, TaskInstanceId id) noexcept
{
    for (auto& task : board.visible_tasks)
    {
        if (task.task_instance_id == id)
        {
            return &task;
        }
    }
    return nullptr;
}

bool remove_task_id(std::vector<TaskInstanceId>& list, TaskInstanceId id) noexcept
{
    const auto new_end = std::remove(list.begin(), list.end(), id);
    if (new_end == list.end())
    {
        return false;
    }

    list.erase(new_end, list.end());
    return true;
}

void mark_task_projection_dirty(SiteRunState& site_run) noexcept
{
    site_run.pending_projection_update_flags |=
        SITE_PROJECTION_UPDATE_TASKS | SITE_PROJECTION_UPDATE_HUD;
}

bool complete_task(TaskBoardState& board, TaskInstanceState& task) noexcept
{
    if (task.runtime_list_kind == TaskRuntimeListKind::Completed)
    {
        return false;
    }

    task.runtime_list_kind = TaskRuntimeListKind::Completed;
    board.completed_task_ids.push_back(task.task_instance_id);
    remove_task_id(board.accepted_task_ids, task.task_instance_id);
    return true;
}

void handle_site_run_started(SiteSystemContext& context, const SiteRunStartedCommand& payload)
{
    auto& board = context.site_run.task_board;
    reset_task_board(board);

    if (payload.site_id != 1U)
    {
        return;
    }

    board.visible_tasks.push_back(make_site1_task(context.site_run));
    board.task_pool_size = static_cast<std::uint32_t>(board.visible_tasks.size());
    board.minutes_until_next_refresh = 0.0;
    mark_task_projection_dirty(context.site_run);
}

void handle_task_accept_requested(
    SiteSystemContext& context,
    const TaskAcceptRequestedCommand& payload)
{
    auto& board = context.site_run.task_board;
    if (board.accepted_task_ids.size() >= board.accepted_task_cap)
    {
        return;
    }

    const TaskInstanceId requested_id {payload.task_instance_id};
    TaskInstanceState* task = find_task_instance(board, requested_id);
    if (task == nullptr || task->runtime_list_kind != TaskRuntimeListKind::Visible)
    {
        return;
    }

    task->runtime_list_kind = TaskRuntimeListKind::Accepted;
    board.accepted_task_ids.push_back(task->task_instance_id);
    mark_task_projection_dirty(context.site_run);
}

void handle_restoration_progress(
    SiteSystemContext& context,
    const RestorationProgressChangedCommand& payload)
{
    auto& board = context.site_run.task_board;
    if (board.visible_tasks.empty())
    {
        return;
    }

    bool mutated = false;
    const std::uint32_t current_progress = payload.fully_grown_tile_count;

    for (auto& task : board.visible_tasks)
    {
        const std::uint32_t target = std::max<std::uint32_t>(1U, task.target_amount);
        const std::uint32_t clamped_progress =
            current_progress <= target ? current_progress : target;

        if (task.current_progress_amount != clamped_progress)
        {
            task.current_progress_amount = clamped_progress;
            mutated = true;
        }

        if (clamped_progress >= target && complete_task(board, task))
        {
            mutated = true;
        }
    }

    if (mutated)
    {
        mark_task_projection_dirty(context.site_run);
    }
}
}  // namespace

bool TaskBoardSystem::subscribes_to(GameCommandType type) noexcept
{
    switch (type)
    {
    case GameCommandType::SiteRunStarted:
    case GameCommandType::TaskAcceptRequested:
    case GameCommandType::RestorationProgressChanged:
        return true;
    default:
        return false;
    }
}

Gs1Status TaskBoardSystem::process_command(
    SiteSystemContext& context,
    const GameCommand& command)
{
    switch (command.type)
    {
    case GameCommandType::SiteRunStarted:
        handle_site_run_started(context, command.payload_as<SiteRunStartedCommand>());
        break;
    case GameCommandType::TaskAcceptRequested:
        handle_task_accept_requested(context, command.payload_as<TaskAcceptRequestedCommand>());
        break;
    case GameCommandType::RestorationProgressChanged:
        handle_restoration_progress(context, command.payload_as<RestorationProgressChangedCommand>());
        break;
    default:
        break;
    }

    return GS1_STATUS_OK;
}

void TaskBoardSystem::run(SiteSystemContext& context)
{
    (void)context;
}
}  // namespace gs1
