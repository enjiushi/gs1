#pragma once

#include "support/id_types.h"

#include <cstdint>
#include <optional>
#include <vector>

namespace gs1
{
enum class TaskRuntimeListKind : std::uint32_t
{
    Visible = 0,
    Accepted = 1,
    Completed = 2
};

struct TaskRewardDraftOption final
{
    RewardCandidateId reward_candidate_id {};
    bool selected {false};
};

struct TaskInstanceState final
{
    TaskInstanceId task_instance_id {};
    TaskTemplateId task_template_id {};
    FactionId publisher_faction_id {};
    std::uint32_t task_tier_id {0};
    std::uint32_t target_amount {0};
    std::uint32_t current_progress_amount {0};
    std::vector<TaskRewardDraftOption> reward_draft_options {};
    TaskRuntimeListKind runtime_list_kind {TaskRuntimeListKind::Visible};
    std::uint32_t chain_id {0};
    std::uint32_t chain_step_index {0};
    TaskTemplateId follow_up_task_template_id {};
    bool has_chain {false};
    bool has_follow_up_task_template {false};
};

struct TaskChainState final
{
    std::uint32_t task_chain_id {0};
    std::optional<std::uint32_t> current_accepted_step_index {};
    std::optional<TaskInstanceId> surfaced_follow_up_task_instance_id {};
    bool is_broken {false};
};

struct TaskBoardState final
{
    std::vector<TaskInstanceState> visible_tasks {};
    std::vector<TaskInstanceId> accepted_task_ids {};
    std::vector<TaskInstanceId> completed_task_ids {};
    std::optional<TaskChainState> active_chain_state {};
    std::uint32_t task_pool_size {0};
    std::uint32_t accepted_task_cap {0};
    double minutes_until_next_refresh {0.0};
};
}  // namespace gs1
