#pragma once

#include "site/action_state.h"
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
    Completed = 2,
    Claimed = 3
};

struct TaskRewardDraftOption final
{
    RewardCandidateId reward_candidate_id {};
    bool selected {false};
};

struct TaskTrackedTileState final
{
    std::uint32_t plant_type_id {0};
    std::uint32_t ground_cover_type_id {0};
    std::uint32_t device_structure_id {0};
    float plant_density {0.0f};
    float moisture {0.0f};
    float heat {0.0f};
    float dust {0.0f};
    float wind_protection {0.0f};
    float heat_protection {0.0f};
    float dust_protection {0.0f};
    float device_integrity {0.0f};
    std::uint8_t populated_neighbor_count {0U};
    bool populated_by_plant {false};
};

struct TaskTrackedWorkerState final
{
    float health {0.0f};
    float hydration {0.0f};
    float nourishment {0.0f};
    float energy {0.0f};
    float morale {0.0f};
    bool valid {false};
};

struct TaskInstanceState final
{
    TaskInstanceId task_instance_id {};
    TaskTemplateId task_template_id {};
    FactionId publisher_faction_id {};
    std::uint32_t task_tier_id {0};
    std::uint32_t target_amount {0};
    std::uint32_t required_count {0};
    std::uint32_t current_progress_amount {0};
    ItemId item_id {};
    PlantId plant_id {};
    RecipeId recipe_id {};
    StructureId structure_id {};
    StructureId secondary_structure_id {};
    StructureId tertiary_structure_id {};
    ActionKind action_kind {ActionKind::None};
    float threshold_value {0.0f};
    float expected_task_hours_in_game {0.0f};
    float risk_multiplier {0.0f};
    std::uint32_t direct_cost_cash_points {0U};
    std::uint32_t time_cost_cash_points {0U};
    std::uint32_t risk_cost_cash_points {0U};
    std::uint32_t difficulty_cash_points {0U};
    std::uint32_t reward_budget_cash_points {0U};
    std::vector<TaskRewardDraftOption> reward_draft_options {};
    TaskRuntimeListKind runtime_list_kind {TaskRuntimeListKind::Visible};
    std::uint32_t chain_id {0};
    std::uint32_t chain_step_index {0};
    TaskTemplateId follow_up_task_template_id {};
    double progress_accumulator {0.0};
    std::uint8_t requirement_mask {0U};
    std::uint8_t reserved0[3] {};
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
    std::vector<TaskTrackedTileState> tracked_tiles {};
    std::vector<TaskInstanceId> accepted_task_ids {};
    std::vector<TaskInstanceId> completed_task_ids {};
    std::vector<TaskInstanceId> claimed_task_ids {};
    std::optional<TaskChainState> active_chain_state {};
    std::uint32_t task_pool_size {0};
    std::uint32_t accepted_task_cap {0};
    std::uint32_t tracked_tile_width {0};
    std::uint32_t tracked_tile_height {0};
    std::uint32_t fully_grown_tile_count {0};
    std::uint32_t site_completion_tile_threshold {0};
    std::uint32_t tracked_living_plant_count {0};
    double minutes_until_next_refresh {0.0};
    TaskTrackedWorkerState worker {};
    bool all_tracked_living_plants_stable {false};
};
}  // namespace gs1
