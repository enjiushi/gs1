#include "site/systems/task_board_system.h"

#include "content/defs/task_defs.h"
#include "messages/game_message.h"
#include "site/site_projection_update_flags.h"
#include "site/site_run_state.h"

#include <algorithm>

namespace gs1
{
namespace
{
std::uint32_t normalized_task_target(std::uint32_t requested_target) noexcept
{
    return requested_target == 0U ? 1U : requested_target;
}

TaskInstanceState make_task_instance(
    TaskInstanceId task_instance_id,
    const TaskTemplateDef& task_def) noexcept
{
    TaskInstanceState task {};
    task.task_instance_id = task_instance_id;
    task.task_template_id = task_def.task_template_id;
    task.publisher_faction_id = task_def.publisher_faction_id;
    task.task_tier_id = task_def.task_tier_id;
    task.target_amount = normalized_task_target(task_def.target_amount);
    task.current_progress_amount = 0U;
    task.runtime_list_kind = TaskRuntimeListKind::Visible;
    return task;
}

bool task_is_complete(const TaskInstanceState& task) noexcept
{
    return task.current_progress_amount >= normalized_task_target(task.target_amount);
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

void mark_task_projection_dirty(SiteSystemContext<TaskBoardSystem>& context) noexcept
{
    context.world.mark_projection_dirty(
        SITE_PROJECTION_UPDATE_TASKS | SITE_PROJECTION_UPDATE_HUD);
}

void queue_task_reward_delivery(
    SiteSystemContext<TaskBoardSystem>& context,
    const TaskTemplateDef& task_def)
{
    if (task_def.reward_delivery.item_id.value == 0U || task_def.reward_delivery.quantity == 0U)
    {
        return;
    }

    GameMessage reward_message {};
    reward_message.type = GameMessageType::InventoryDeliveryBatchRequested;

    InventoryDeliveryBatchRequestedMessage payload {};
    payload.minutes_until_arrival = task_def.reward_delivery.delivery_minutes;
    payload.entry_count = 1U;
    payload.entries[0].item_id = static_cast<std::uint16_t>(task_def.reward_delivery.item_id.value);
    payload.entries[0].quantity = task_def.reward_delivery.quantity;

    reward_message.set_payload(payload);
    context.message_queue.push_back(reward_message);
}

bool complete_task(
    SiteSystemContext<TaskBoardSystem>& context,
    TaskBoardState& board,
    TaskInstanceState& task) noexcept
{
    if (task.runtime_list_kind == TaskRuntimeListKind::Completed)
    {
        return false;
    }

    task.runtime_list_kind = TaskRuntimeListKind::Completed;
    board.completed_task_ids.push_back(task.task_instance_id);
    remove_task_id(board.accepted_task_ids, task.task_instance_id);
    if (const auto* task_def = find_task_template_def(task.task_template_id))
    {
        queue_task_reward_delivery(context, *task_def);
    }
    return true;
}

void evaluate_task_completion(
    SiteSystemContext<TaskBoardSystem>& context,
    TaskBoardState& board,
    TaskInstanceState& task,
    bool& mutated) noexcept
{
    if (task.runtime_list_kind != TaskRuntimeListKind::Accepted || !task_is_complete(task))
    {
        return;
    }

    if (complete_task(context, board, task))
    {
        mutated = true;
    }
}

void set_task_progress(
    SiteSystemContext<TaskBoardSystem>& context,
    TaskBoardState& board,
    TaskInstanceState& task,
    std::uint32_t next_progress,
    bool& mutated) noexcept
{
    const std::uint32_t clamped_progress =
        std::min(normalized_task_target(task.target_amount), next_progress);
    if (task.current_progress_amount != clamped_progress)
    {
        task.current_progress_amount = clamped_progress;
        mutated = true;
    }

    evaluate_task_completion(context, board, task, mutated);
}

void increment_task_progress(
    SiteSystemContext<TaskBoardSystem>& context,
    TaskBoardState& board,
    TaskInstanceState& task,
    std::uint32_t delta,
    bool& mutated) noexcept
{
    if (delta == 0U)
    {
        return;
    }

    const std::uint32_t max_progress = normalized_task_target(task.target_amount);
    const std::uint32_t next_progress =
        task.current_progress_amount >= max_progress - delta
            ? max_progress
            : (task.current_progress_amount + delta);
    set_task_progress(context, board, task, next_progress, mutated);
}

void seed_site1_onboarding_tasks(TaskBoardState& board) noexcept
{
    board.visible_tasks.clear();
    std::uint32_t next_task_instance_id = 1U;
    for (const auto& task_def : k_prototype_task_defs)
    {
        board.visible_tasks.push_back(
            make_task_instance(TaskInstanceId {next_task_instance_id++}, task_def));
    }
    board.task_pool_size = static_cast<std::uint32_t>(board.visible_tasks.size());
}

void handle_site_run_started(
    SiteSystemContext<TaskBoardSystem>& context,
    const SiteRunStartedMessage& payload)
{
    auto& board = context.world.own_task_board();
    reset_task_board(board);

    if (payload.site_id != 1U ||
        context.world.read_objective().type != SiteObjectiveType::DenseRestoration)
    {
        return;
    }

    board.accepted_task_cap = 2U;
    seed_site1_onboarding_tasks(board);
    board.minutes_until_next_refresh = 0.0;
    mark_task_projection_dirty(context);
}

void handle_task_accept_requested(
    SiteSystemContext<TaskBoardSystem>& context,
    const TaskAcceptRequestedMessage& payload)
{
    auto& board = context.world.own_task_board();
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

    bool mutated = true;
    evaluate_task_completion(context, board, *task, mutated);
    mark_task_projection_dirty(context);
}

void handle_site1_task_progress(
    SiteSystemContext<TaskBoardSystem>& context,
    TaskTemplateProgressKind progress_kind,
    std::uint32_t delta_or_value,
    bool use_absolute_progress) noexcept
{
    auto& board = context.world.own_task_board();
    if (board.visible_tasks.empty())
    {
        return;
    }

    bool mutated = false;
    for (auto& task : board.visible_tasks)
    {
        const auto* task_def = find_task_template_def(task.task_template_id);
        if (task_def == nullptr ||
            task_def->progress_kind != progress_kind ||
            task.runtime_list_kind == TaskRuntimeListKind::Completed)
        {
            continue;
        }

        if (use_absolute_progress)
        {
            set_task_progress(context, board, task, delta_or_value, mutated);
        }
        else
        {
            increment_task_progress(context, board, task, delta_or_value, mutated);
        }
    }

    if (mutated)
    {
        mark_task_projection_dirty(context);
    }
}

}  // namespace

bool TaskBoardSystem::subscribes_to(GameMessageType type) noexcept
{
    switch (type)
    {
    case GameMessageType::SiteRunStarted:
    case GameMessageType::TaskAcceptRequested:
    case GameMessageType::SiteTilePlantingCompleted:
    case GameMessageType::SiteTileWatered:
    case GameMessageType::SiteTileBurialCleared:
    case GameMessageType::SiteDeviceRepaired:
        return true;
    default:
        return false;
    }
}

Gs1Status TaskBoardSystem::process_message(
    SiteSystemContext<TaskBoardSystem>& context,
    const GameMessage& message)
{
    switch (message.type)
    {
    case GameMessageType::SiteRunStarted:
        handle_site_run_started(context, message.payload_as<SiteRunStartedMessage>());
        break;
    case GameMessageType::TaskAcceptRequested:
        handle_task_accept_requested(context, message.payload_as<TaskAcceptRequestedMessage>());
        break;
    case GameMessageType::SiteTilePlantingCompleted:
        handle_site1_task_progress(context, TaskTemplateProgressKind::PlantTileCount, 1U, false);
        break;
    case GameMessageType::SiteTileWatered:
        handle_site1_task_progress(context, TaskTemplateProgressKind::WaterTileCount, 1U, false);
        break;
    case GameMessageType::SiteTileBurialCleared:
        handle_site1_task_progress(context, TaskTemplateProgressKind::ClearBurialActionCount, 1U, false);
        break;
    case GameMessageType::SiteDeviceRepaired:
        handle_site1_task_progress(context, TaskTemplateProgressKind::RepairDeviceCount, 1U, false);
        break;
    default:
        break;
    }

    return GS1_STATUS_OK;
}

void TaskBoardSystem::run(SiteSystemContext<TaskBoardSystem>& context)
{
    (void)context;
}
}  // namespace gs1
