#include "site/systems/task_board_system.h"

#include "content/defs/item_defs.h"
#include "content/defs/reward_defs.h"
#include "content/defs/task_defs.h"
#include "site/site_projection_update_flags.h"
#include "site/site_run_state.h"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace gs1
{
namespace
{
std::uint32_t normalized_task_target(std::uint32_t requested_target) noexcept
{
    return requested_target == 0U ? 1U : requested_target;
}

std::uint32_t resolved_task_target_amount(
    const TaskTemplateDef& task_template_def,
    const SiteCounters& counters) noexcept
{
    return normalized_task_target(
        task_template_def.target_amount == 0U
            ? counters.site_completion_tile_threshold
            : task_template_def.target_amount);
}

std::uint64_t mix_seed(
    std::uint64_t base_seed,
    std::uint32_t a,
    std::uint32_t b) noexcept
{
    std::uint64_t value = base_seed ^ (static_cast<std::uint64_t>(a) << 32U) ^ static_cast<std::uint64_t>(b);
    value ^= value >> 33U;
    value *= 0xff51afd7ed558ccdULL;
    value ^= value >> 33U;
    value *= 0xc4ceb9fe1a85ec53ULL;
    value ^= value >> 33U;
    return value;
}

bool contains_unlockable(
    const std::vector<std::uint32_t>& unlockable_ids,
    std::uint32_t unlockable_id) noexcept
{
    return std::find(unlockable_ids.begin(), unlockable_ids.end(), unlockable_id) != unlockable_ids.end();
}

bool contains_modifier(
    const std::vector<ModifierId>& modifier_ids,
    ModifierId modifier_id) noexcept
{
    return std::find(modifier_ids.begin(), modifier_ids.end(), modifier_id) != modifier_ids.end();
}

std::vector<RewardCandidateId> build_reward_candidate_pool(
    SiteSystemContext<TaskBoardSystem>& context)
{
    std::vector<RewardCandidateId> reward_pool {};
    const auto& revealed_unlockables = context.world.read_economy().revealed_site_unlockable_ids;
    const auto& active_run_modifiers = context.world.read_modifier().active_run_modifier_ids;

    for (const auto& reward_candidate_def : all_reward_candidate_defs())
    {
        switch (reward_candidate_def.effect_kind)
        {
        case RewardEffectKind::Money:
        case RewardEffectKind::ItemDelivery:
            reward_pool.push_back(reward_candidate_def.reward_candidate_id);
            break;

        case RewardEffectKind::RevealUnlockable:
            if (!contains_unlockable(revealed_unlockables, reward_candidate_def.unlockable_id))
            {
                reward_pool.push_back(reward_candidate_def.reward_candidate_id);
            }
            break;

        case RewardEffectKind::RunModifier:
            if (!contains_modifier(active_run_modifiers, reward_candidate_def.modifier_id))
            {
                reward_pool.push_back(reward_candidate_def.reward_candidate_id);
            }
            break;

        case RewardEffectKind::CampaignReputation:
        case RewardEffectKind::FactionReputation:
        case RewardEffectKind::None:
        default:
            break;
        }
    }

    if (reward_pool.empty())
    {
        reward_pool.push_back(RewardCandidateId {k_reward_candidate_site1_money_stipend});
    }

    return reward_pool;
}

std::vector<TaskRewardDraftOption> make_reward_draft_options(
    SiteSystemContext<TaskBoardSystem>& context,
    TaskInstanceId task_instance_id,
    const TaskTemplateDef& task_template_def)
{
    constexpr std::size_t k_reward_option_count = 2U;

    const auto reward_pool = build_reward_candidate_pool(context);
    std::vector<TaskRewardDraftOption> reward_options {};
    reward_options.reserve(k_reward_option_count);

    if (reward_pool.empty())
    {
        return reward_options;
    }

    const auto seed =
        mix_seed(
            context.world.site_attempt_seed(),
            task_template_def.task_template_id.value,
            task_instance_id.value);
    const auto first_index = static_cast<std::size_t>(seed % reward_pool.size());
    reward_options.push_back(TaskRewardDraftOption {reward_pool[first_index], false});

    if (reward_pool.size() > 1U)
    {
        const auto second_index =
            static_cast<std::size_t>((first_index + 1U + ((seed >> 17U) % (reward_pool.size() - 1U))) % reward_pool.size());
        reward_options.push_back(TaskRewardDraftOption {reward_pool[second_index], false});
    }

    return reward_options;
}

TaskInstanceState make_task_instance(
    SiteSystemContext<TaskBoardSystem>& context,
    TaskInstanceId task_instance_id,
    const TaskTemplateDef& task_template_def,
    const SiteCounters& counters)
{
    TaskInstanceState task {};
    task.task_instance_id = task_instance_id;
    task.task_template_id = task_template_def.task_template_id;
    task.publisher_faction_id = task_template_def.publisher_faction_id;
    task.task_tier_id = task_template_def.task_tier_id;
    task.target_amount = resolved_task_target_amount(task_template_def, counters);
    task.current_progress_amount = 0U;
    task.reward_draft_options =
        make_reward_draft_options(context, task_instance_id, task_template_def);
    task.runtime_list_kind = TaskRuntimeListKind::Visible;
    return task;
}

void reset_task_board(TaskBoardState& board) noexcept
{
    board.visible_tasks.clear();
    board.accepted_task_ids.clear();
    board.completed_task_ids.clear();
    board.claimed_task_ids.clear();
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

TaskRewardDraftOption* find_reward_option(
    TaskInstanceState& task,
    RewardCandidateId reward_candidate_id) noexcept
{
    for (auto& reward_option : task.reward_draft_options)
    {
        if (reward_option.reward_candidate_id == reward_candidate_id)
        {
            return &reward_option;
        }
    }

    return nullptr;
}

bool has_selected_reward(const TaskInstanceState& task) noexcept
{
    for (const auto& reward_option : task.reward_draft_options)
    {
        if (reward_option.selected)
        {
            return true;
        }
    }

    return false;
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
        SITE_PROJECTION_UPDATE_TASKS | SITE_PROJECTION_UPDATE_PHONE | SITE_PROJECTION_UPDATE_HUD);
}

void queue_faction_reputation_award(
    SiteSystemContext<TaskBoardSystem>& context,
    const TaskInstanceState& task) noexcept
{
    const auto* task_template_def = find_task_template_def(task.task_template_id);
    if (task_template_def == nullptr ||
        task_template_def->completion_faction_reputation_delta <= 0 ||
        task.publisher_faction_id.value == 0U)
    {
        return;
    }

    GameMessage reward_message {};
    reward_message.type = GameMessageType::FactionReputationAwardRequested;
    reward_message.set_payload(FactionReputationAwardRequestedMessage {
        task.publisher_faction_id.value,
        task_template_def->completion_faction_reputation_delta});
    context.message_queue.push_back(reward_message);
}

bool queue_reward_message(
    SiteSystemContext<TaskBoardSystem>& context,
    const RewardCandidateDef& reward_candidate_def)
{
    GameMessage reward_message {};

    switch (reward_candidate_def.effect_kind)
    {
    case RewardEffectKind::Money:
        if (reward_candidate_def.money_delta == 0)
        {
            return false;
        }
        reward_message.type = GameMessageType::EconomyMoneyAwardRequested;
        reward_message.set_payload(EconomyMoneyAwardRequestedMessage {
            reward_candidate_def.money_delta});
        break;

    case RewardEffectKind::ItemDelivery:
        if (reward_candidate_def.item_id.value == 0U || reward_candidate_def.quantity == 0U)
        {
            return false;
        }
        reward_message.type = GameMessageType::InventoryDeliveryRequested;
        reward_message.set_payload(InventoryDeliveryRequestedMessage {
            reward_candidate_def.item_id.value,
            static_cast<std::uint16_t>(std::min<std::uint32_t>(reward_candidate_def.quantity, 65535U)),
            reward_candidate_def.delivery_minutes});
        break;

    case RewardEffectKind::RevealUnlockable:
        if (reward_candidate_def.unlockable_id == 0U)
        {
            return false;
        }
        reward_message.type = GameMessageType::SiteUnlockableRevealRequested;
        reward_message.set_payload(SiteUnlockableRevealRequestedMessage {
            reward_candidate_def.unlockable_id});
        break;

    case RewardEffectKind::RunModifier:
        if (reward_candidate_def.modifier_id.value == 0U)
        {
            return false;
        }
        reward_message.type = GameMessageType::RunModifierAwardRequested;
        reward_message.set_payload(RunModifierAwardRequestedMessage {
            reward_candidate_def.modifier_id.value});
        break;

    case RewardEffectKind::CampaignReputation:
    case RewardEffectKind::FactionReputation:
    case RewardEffectKind::None:
    default:
        return false;
    }

    context.message_queue.push_back(reward_message);
    return true;
}

bool complete_task(
    SiteSystemContext<TaskBoardSystem>& context,
    TaskBoardState& board,
    TaskInstanceState& task) noexcept
{
    if (task.runtime_list_kind != TaskRuntimeListKind::Accepted)
    {
        return false;
    }

    task.runtime_list_kind = TaskRuntimeListKind::Completed;
    board.completed_task_ids.push_back(task.task_instance_id);
    remove_task_id(board.accepted_task_ids, task.task_instance_id);
    queue_faction_reputation_award(context, task);
    return true;
}

bool item_matches_task(
    const TaskTemplateDef& task_template_def,
    ItemId item_id) noexcept
{
    return task_template_def.item_id.value == 0U || task_template_def.item_id == item_id;
}

bool plant_matches_task(
    const TaskTemplateDef& task_template_def,
    PlantId planted_plant_id) noexcept
{
    if (task_template_def.item_id.value == 0U)
    {
        return true;
    }

    const auto* item_def = find_item_def(task_template_def.item_id);
    return item_def != nullptr && item_def->linked_plant_id == planted_plant_id;
}

template <typename Matcher>
void advance_matching_accepted_tasks(
    SiteSystemContext<TaskBoardSystem>& context,
    std::uint32_t amount,
    Matcher matcher)
{
    auto& board = context.world.own_task_board();
    bool mutated = false;

    for (auto& task : board.visible_tasks)
    {
        if (task.runtime_list_kind != TaskRuntimeListKind::Accepted)
        {
            continue;
        }

        const auto* task_template_def = find_task_template_def(task.task_template_id);
        if (task_template_def == nullptr || !matcher(*task_template_def))
        {
            continue;
        }

        const auto target = std::max<std::uint32_t>(1U, task.target_amount);
        const auto next_progress = std::min(target, task.current_progress_amount + amount);
        if (next_progress != task.current_progress_amount)
        {
            task.current_progress_amount = next_progress;
            mutated = true;
        }

        if (next_progress >= target && complete_task(context, board, task))
        {
            mutated = true;
        }
    }

    if (mutated)
    {
        mark_task_projection_dirty(context);
    }
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

    std::uint32_t next_task_instance_id = 1U;
    for (const auto& task_template_def : all_task_template_defs())
    {
        if (task_template_def.task_template_id.value == 0U)
        {
            continue;
        }

        auto task = make_task_instance(
            context,
            TaskInstanceId {next_task_instance_id++},
            task_template_def,
            context.world.read_counters());
        if (task_template_def.progress_kind == TaskProgressKind::RestorationTiles)
        {
            task.current_progress_amount = std::min(
                task.target_amount,
                context.world.read_counters().fully_grown_tile_count);
        }
        board.visible_tasks.push_back(std::move(task));
    }

    board.task_pool_size = static_cast<std::uint32_t>(board.visible_tasks.size());
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
    if (task->current_progress_amount >= std::max<std::uint32_t>(1U, task->target_amount))
    {
        (void)complete_task(context, board, *task);
    }
    mark_task_projection_dirty(context);
}

void handle_task_reward_claim_requested(
    SiteSystemContext<TaskBoardSystem>& context,
    const TaskRewardClaimRequestedMessage& payload)
{
    auto& board = context.world.own_task_board();
    TaskInstanceState* task = find_task_instance(board, TaskInstanceId {payload.task_instance_id});
    if (task == nullptr || task->runtime_list_kind != TaskRuntimeListKind::Completed)
    {
        return;
    }

    if (has_selected_reward(*task))
    {
        return;
    }

    TaskRewardDraftOption* reward_option =
        find_reward_option(*task, RewardCandidateId {payload.reward_candidate_id});
    if (reward_option == nullptr)
    {
        return;
    }

    const auto* reward_candidate_def = find_reward_candidate_def(reward_option->reward_candidate_id);
    if (reward_candidate_def == nullptr || !queue_reward_message(context, *reward_candidate_def))
    {
        return;
    }

    reward_option->selected = true;
    task->runtime_list_kind = TaskRuntimeListKind::Claimed;
    remove_task_id(board.completed_task_ids, task->task_instance_id);
    board.claimed_task_ids.push_back(task->task_instance_id);
    mark_task_projection_dirty(context);
}

void handle_restoration_progress(
    SiteSystemContext<TaskBoardSystem>& context,
    const RestorationProgressChangedMessage& payload)
{
    auto& board = context.world.own_task_board();
    bool mutated = false;

    for (auto& task : board.visible_tasks)
    {
        if (task.runtime_list_kind == TaskRuntimeListKind::Claimed)
        {
            continue;
        }

        const auto* task_template_def = find_task_template_def(task.task_template_id);
        if (task_template_def == nullptr ||
            task_template_def->progress_kind != TaskProgressKind::RestorationTiles)
        {
            continue;
        }

        const auto target = std::max<std::uint32_t>(1U, task.target_amount);
        const auto clamped_progress = std::min(target, payload.fully_grown_tile_count);
        if (task.current_progress_amount != clamped_progress)
        {
            task.current_progress_amount = clamped_progress;
            mutated = true;
        }

        if (clamped_progress >= target && complete_task(context, board, task))
        {
            mutated = true;
        }
    }

    if (mutated)
    {
        mark_task_projection_dirty(context);
    }
}

void handle_phone_listing_purchased(
    SiteSystemContext<TaskBoardSystem>& context,
    const PhoneListingPurchasedMessage& payload)
{
    advance_matching_accepted_tasks(context, payload.quantity == 0U ? 1U : payload.quantity, [&](const TaskTemplateDef& task_template_def) {
        return task_template_def.progress_kind == TaskProgressKind::BuyItem &&
            item_matches_task(task_template_def, ItemId {payload.item_id});
    });
}

void handle_phone_listing_sold(
    SiteSystemContext<TaskBoardSystem>& context,
    const PhoneListingSoldMessage& payload)
{
    advance_matching_accepted_tasks(context, payload.quantity == 0U ? 1U : payload.quantity, [&](const TaskTemplateDef& task_template_def) {
        return task_template_def.progress_kind == TaskProgressKind::SellItem &&
            item_matches_task(task_template_def, ItemId {payload.item_id});
    });
}

void handle_inventory_transfer_completed(
    SiteSystemContext<TaskBoardSystem>& context,
    const InventoryTransferCompletedMessage& payload)
{
    advance_matching_accepted_tasks(context, payload.quantity == 0U ? 1U : payload.quantity, [&](const TaskTemplateDef& task_template_def) {
        return task_template_def.progress_kind == TaskProgressKind::TransferItem &&
            item_matches_task(task_template_def, ItemId {payload.item_id});
    });
}

void handle_site_tile_planting_completed(
    SiteSystemContext<TaskBoardSystem>& context,
    const SiteTilePlantingCompletedMessage& payload)
{
    advance_matching_accepted_tasks(context, 1U, [&](const TaskTemplateDef& task_template_def) {
        return task_template_def.progress_kind == TaskProgressKind::PlantItem &&
            plant_matches_task(task_template_def, PlantId {payload.plant_type_id});
    });
}

void handle_inventory_craft_completed(
    SiteSystemContext<TaskBoardSystem>& context,
    const InventoryCraftCompletedMessage& payload)
{
    advance_matching_accepted_tasks(context, payload.output_quantity == 0U ? 1U : payload.output_quantity, [&](const TaskTemplateDef& task_template_def) {
        return task_template_def.progress_kind == TaskProgressKind::CraftRecipe &&
            (task_template_def.recipe_id.value == 0U || task_template_def.recipe_id.value == payload.recipe_id);
    });
}

void handle_inventory_item_use_completed(
    SiteSystemContext<TaskBoardSystem>& context,
    const InventoryItemUseCompletedMessage& payload)
{
    advance_matching_accepted_tasks(context, payload.quantity == 0U ? 1U : payload.quantity, [&](const TaskTemplateDef& task_template_def) {
        return task_template_def.progress_kind == TaskProgressKind::ConsumeItem &&
            item_matches_task(task_template_def, ItemId {payload.item_id});
    });
}

void handle_site_action_completed(
    SiteSystemContext<TaskBoardSystem>& context,
    const SiteActionCompletedMessage& payload)
{
    if (payload.action_kind != GS1_SITE_ACTION_DRINK &&
        payload.action_kind != GS1_SITE_ACTION_EAT)
    {
        return;
    }

    advance_matching_accepted_tasks(context, 1U, [&](const TaskTemplateDef& task_template_def) {
        return task_template_def.progress_kind == TaskProgressKind::ConsumeItem;
    });
}
}  // namespace

bool TaskBoardSystem::subscribes_to(GameMessageType type) noexcept
{
    switch (type)
    {
    case GameMessageType::SiteRunStarted:
    case GameMessageType::TaskAcceptRequested:
    case GameMessageType::TaskRewardClaimRequested:
    case GameMessageType::RestorationProgressChanged:
    case GameMessageType::PhoneListingPurchased:
    case GameMessageType::PhoneListingSold:
    case GameMessageType::InventoryTransferCompleted:
    case GameMessageType::InventoryItemUseCompleted:
    case GameMessageType::InventoryCraftCompleted:
    case GameMessageType::SiteTilePlantingCompleted:
    case GameMessageType::SiteActionCompleted:
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
    case GameMessageType::TaskRewardClaimRequested:
        handle_task_reward_claim_requested(
            context,
            message.payload_as<TaskRewardClaimRequestedMessage>());
        break;
    case GameMessageType::RestorationProgressChanged:
        handle_restoration_progress(context, message.payload_as<RestorationProgressChangedMessage>());
        break;
    case GameMessageType::PhoneListingPurchased:
        handle_phone_listing_purchased(context, message.payload_as<PhoneListingPurchasedMessage>());
        break;
    case GameMessageType::PhoneListingSold:
        handle_phone_listing_sold(context, message.payload_as<PhoneListingSoldMessage>());
        break;
    case GameMessageType::InventoryTransferCompleted:
        handle_inventory_transfer_completed(
            context,
            message.payload_as<InventoryTransferCompletedMessage>());
        break;
    case GameMessageType::InventoryItemUseCompleted:
        handle_inventory_item_use_completed(
            context,
            message.payload_as<InventoryItemUseCompletedMessage>());
        break;
    case GameMessageType::InventoryCraftCompleted:
        handle_inventory_craft_completed(
            context,
            message.payload_as<InventoryCraftCompletedMessage>());
        break;
    case GameMessageType::SiteTilePlantingCompleted:
        handle_site_tile_planting_completed(
            context,
            message.payload_as<SiteTilePlantingCompletedMessage>());
        break;
    case GameMessageType::SiteActionCompleted:
        handle_site_action_completed(context, message.payload_as<SiteActionCompletedMessage>());
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
