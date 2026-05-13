#include "site/systems/task_board_system.h"

#include "campaign/systems/technology_system.h"
#include "content/defs/gameplay_tuning_defs.h"
#include "content/defs/item_defs.h"
#include "content/defs/plant_defs.h"
#include "content/defs/reward_defs.h"
#include "content/defs/structure_defs.h"
#include "content/defs/task_defs.h"
#include "content/prototype_content.h"
#include "runtime/game_runtime.h"
#include "runtime/runtime_clock.h"
#include "site/defs/site_action_defs.h"
#include "site/site_run_state.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace gs1
{
template <>
struct system_state_tags<TaskBoardSystem>
{
    using type = type_list<
        RuntimeCampaignTag,
        RuntimeActiveSiteRunTag,
        RuntimeFixedStepSecondsTag,
        RuntimeMoveDirectionTag>;
};

namespace
{
enum class PlantProtectionChannel : std::uint8_t
{
    Wind = 0,
    Heat = 1,
    Dust = 2
};

[[nodiscard]] auto task_board_access(RuntimeInvocation& invocation)
    -> GameStateAccess<TaskBoardSystem>
{
    return make_game_state_access<TaskBoardSystem>(invocation);
}

[[nodiscard]] const CampaignState& task_board_campaign(RuntimeInvocation& invocation)
{
    auto access = task_board_access(invocation);
    auto& campaign = access.template read<RuntimeCampaignTag>();
    return *campaign;
}

[[nodiscard]] SiteRunState& task_board_site_run(RuntimeInvocation& invocation)
{
    auto access = task_board_access(invocation);
    auto& site_run = access.template read<RuntimeActiveSiteRunTag>();
    return *site_run;
}

[[nodiscard]] SiteWorldAccess<TaskBoardSystem> task_board_world(RuntimeInvocation& invocation)
{
    return SiteWorldAccess<TaskBoardSystem> {task_board_site_run(invocation)};
}

[[nodiscard]] GameMessageQueue& task_board_message_queue(RuntimeInvocation& invocation)
{
    return invocation.game_message_queue();
}

constexpr double k_progress_epsilon = 0.0001;
constexpr std::uint32_t k_cash_points_per_in_game_hour = 25U;
constexpr std::uint32_t k_reward_budget_multiplier_percent = 115U;

struct NeighborVisit final
{
    int dx;
    int dy;
};

constexpr NeighborVisit k_neighbor_visits[] {
    {-1, -1},
    {0, -1},
    {1, -1},
    {-1, 0},
    {1, 0},
    {-1, 1},
    {0, 1},
    {1, 1}};

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

std::uint32_t normalized_task_target(std::uint32_t requested_target) noexcept
{
    return requested_target == 0U ? 1U : requested_target;
}

std::uint32_t normalized_required_count(std::uint32_t requested_count) noexcept
{
    return requested_count == 0U ? 1U : requested_count;
}

std::uint32_t round_to_cash_points(double value) noexcept
{
    return value <= 0.0
        ? 0U
        : static_cast<std::uint32_t>(std::lround(value));
}

std::uint32_t scale_cash_points_by_percent(
    std::uint32_t cash_points,
    std::uint32_t percent) noexcept
{
    return percent == 0U
        ? 0U
        : round_to_cash_points(
              static_cast<double>(cash_points) * static_cast<double>(percent) / 100.0);
}

std::uint32_t scale_cash_points_by_multiplier(
    std::uint32_t cash_points,
    float multiplier) noexcept
{
    return multiplier <= 0.0f
        ? 0U
        : round_to_cash_points(static_cast<double>(cash_points) * static_cast<double>(multiplier));
}

std::uint32_t resolved_task_target_amount(
    const TaskTemplateDef& task_template_def,
    const SiteCounters& counters) noexcept
{
    if (task_template_def.target_amount != 0U)
    {
        return normalized_task_target(task_template_def.target_amount);
    }

    return normalized_task_target(counters.site_completion_tile_threshold);
}

std::uint32_t resolved_task_required_count(const TaskTemplateDef& task_template_def) noexcept
{
    return task_template_def.required_count == 0U
        ? 0U
        : normalized_required_count(task_template_def.required_count);
}

float resolved_task_threshold_value(const TaskTemplateDef& task_template_def) noexcept
{
    return task_template_def.threshold_value;
}

double resolved_expected_task_hours_in_game(const TaskTemplateDef& task_template_def) noexcept
{
    return static_cast<double>(std::max(task_template_def.expected_task_hours_in_game, 0.0f));
}

float resolved_task_risk_multiplier(const TaskTemplateDef& task_template_def) noexcept
{
    return std::max(task_template_def.risk_multiplier, 0.0f);
}

PlantId resolved_task_plant_id(const TaskTemplateDef& task_template_def) noexcept
{
    if (task_template_def.plant_id.value != 0U)
    {
        return task_template_def.plant_id;
    }

    if (task_template_def.item_id.value == 0U)
    {
        return PlantId {};
    }

    const auto* item_def = find_item_def(task_template_def.item_id);
    return item_def == nullptr ? PlantId {} : item_def->linked_plant_id;
}

bool tile_has_plant(const SiteWorld::TileData& tile) noexcept
{
    return tile.ecology.plant_id.value != 0U;
}

bool tracked_tile_has_plant(const TaskTrackedTileState& tile) noexcept
{
    return tile.populated_by_plant;
}

bool tracked_tile_coord_in_bounds(
    const TaskBoardState& board,
    TileCoord coord) noexcept
{
    return coord.x >= 0 &&
        coord.y >= 0 &&
        static_cast<std::uint32_t>(coord.x) < board.tracked_tile_width &&
        static_cast<std::uint32_t>(coord.y) < board.tracked_tile_height;
}

std::size_t tracked_tile_index(
    const TaskBoardState& board,
    TileCoord coord) noexcept
{
    return static_cast<std::size_t>(coord.y) * board.tracked_tile_width +
        static_cast<std::size_t>(coord.x);
}

TaskTrackedTileState* tracked_tile_state(
    TaskBoardState& board,
    TileCoord coord) noexcept
{
    if (!tracked_tile_coord_in_bounds(board, coord))
    {
        return nullptr;
    }

    const auto index = tracked_tile_index(board, coord);
    return index < board.tracked_tiles.size() ? &board.tracked_tiles[index] : nullptr;
}

const TaskTrackedTileState* tracked_tile_state(
    const TaskBoardState& board,
    TileCoord coord) noexcept
{
    if (!tracked_tile_coord_in_bounds(board, coord))
    {
        return nullptr;
    }

    const auto index = tracked_tile_index(board, coord);
    return index < board.tracked_tiles.size() ? &board.tracked_tiles[index] : nullptr;
}

void update_populated_neighbor_counts(
    TaskBoardState& board,
    TileCoord center,
    bool previously_populated,
    bool now_populated) noexcept
{
    if (previously_populated == now_populated)
    {
        return;
    }

    const int delta = now_populated ? 1 : -1;
    for (const auto visit : k_neighbor_visits)
    {
        const TileCoord neighbor {center.x + visit.dx, center.y + visit.dy};
        TaskTrackedTileState* neighbor_state = tracked_tile_state(board, neighbor);
        if (neighbor_state == nullptr)
        {
            continue;
        }

        const int next_count =
            static_cast<int>(neighbor_state->populated_neighbor_count) + delta;
        neighbor_state->populated_neighbor_count =
            static_cast<std::uint8_t>(std::clamp(next_count, 0, 8));
    }
}

void apply_tracked_plant_state(
    TaskBoardState& board,
    TileCoord coord,
    std::uint32_t plant_type_id,
    std::uint32_t ground_cover_type_id,
    float plant_density) noexcept
{
    TaskTrackedTileState* tracked = tracked_tile_state(board, coord);
    if (tracked == nullptr)
    {
        return;
    }

    const bool previously_populated = tracked->populated_by_plant;
    tracked->plant_type_id = plant_type_id;
    tracked->ground_cover_type_id = ground_cover_type_id;
    tracked->plant_density = plant_density;
    tracked->populated_by_plant = plant_type_id != 0U;
    update_populated_neighbor_counts(board, coord, previously_populated, tracked->populated_by_plant);
}

float density_from_centi(std::uint16_t plant_density_centi) noexcept
{
    return static_cast<float>(plant_density_centi) * 0.01f;
}

bool item_is_crafted(ItemId item_id) noexcept
{
    const auto* item_def = find_item_def(item_id);
    return item_def != nullptr && item_def->source_rule == ItemSourceRule::CraftOnly;
}

bool item_is_buyable_from_authored_phone_stock(
    RuntimeInvocation& invocation,
    ItemId item_id) noexcept;

float task_board_refresh_interval_minutes() noexcept
{
    return gameplay_tuning_def().task_board.refresh_interval_minutes;
}

std::uint32_t normal_task_pool_size() noexcept
{
    return gameplay_tuning_def().task_board.normal_task_pool_size;
}

ItemId find_structure_linked_item(StructureId structure_id) noexcept
{
    for (const auto& item_def : all_item_defs())
    {
        if (item_def.linked_structure_id == structure_id)
        {
            return item_def.item_id;
        }
    }

    return {};
}

std::uint32_t item_value_cash_points(
    ItemId item_id,
    std::uint32_t quantity = 1U) noexcept
{
    const auto item_cash_points = item_internal_price_cash_points(item_id);
    return quantity == 0U
        ? 0U
        : round_to_cash_points(
              static_cast<double>(item_cash_points) * static_cast<double>(quantity));
}

std::uint32_t structure_value_cash_points(StructureId structure_id) noexcept
{
    return item_value_cash_points(find_structure_linked_item(structure_id));
}

std::uint32_t action_direct_cost_cash_points(
    ActionKind action_kind,
    std::uint32_t quantity) noexcept
{
    const auto* action_def = find_site_action_def(action_kind);
    if (action_def == nullptr)
    {
        return 0U;
    }

    const float scale = static_cast<float>(quantity == 0U ? 1U : quantity);
    return player_meter_cost_internal_cash_points(
        0.0f,
        action_def->hydration_cost_per_unit * scale,
        action_def->nourishment_cost_per_unit * scale,
        action_def->energy_cost_per_unit * scale,
        action_def->morale_cost_per_unit * scale);
}

std::uint32_t resolved_task_direct_cost_cash_points(
    const TaskTemplateDef& task_template_def,
    const TaskInstanceState& task) noexcept
{
    switch (task_template_def.progress_kind)
    {
    case TaskProgressKind::BuyItem:
        return scale_cash_points_by_percent(
            item_value_cash_points(task.item_id, task.target_amount),
            35U);

    case TaskProgressKind::SellItem:
    case TaskProgressKind::SubmitItem:
    case TaskProgressKind::ConsumeItem:
        return item_value_cash_points(task.item_id, task.target_amount);

    case TaskProgressKind::TransferItem:
        return 0U;

    case TaskProgressKind::PerformAction:
        return action_direct_cost_cash_points(task.action_kind, task.target_amount);

    case TaskProgressKind::PlantItem:
        return scale_cash_points_by_percent(
            item_value_cash_points(task.item_id, task.target_amount),
            40U);

    case TaskProgressKind::CraftRecipe:
    {
        const auto* recipe_def = find_craft_recipe_def(task.recipe_id);
        if (recipe_def == nullptr)
        {
            return 0U;
        }

        return scale_cash_points_by_percent(
            item_value_cash_points(recipe_def->output_item_id, task.target_amount),
            50U);
    }

    case TaskProgressKind::CraftItem:
    case TaskProgressKind::CraftAnyItem:
        return scale_cash_points_by_percent(
            item_value_cash_points(task.item_id, task.target_amount),
            50U);

    case TaskProgressKind::BuildStructure:
    case TaskProgressKind::BuildAnyStructure:
        return scale_cash_points_by_percent(
            round_to_cash_points(
                static_cast<double>(structure_value_cash_points(task.structure_id)) *
                static_cast<double>(std::max(task.target_amount, 1U))),
            45U);

    case TaskProgressKind::BuildStructureSet:
        return scale_cash_points_by_percent(
            structure_value_cash_points(task.structure_id) +
                structure_value_cash_points(task.secondary_structure_id) +
                structure_value_cash_points(task.tertiary_structure_id),
            45U);

    case TaskProgressKind::SellCraftedItem:
        return item_value_cash_points(task.item_id, task.target_amount);

    default:
        return 0U;
    }
}

std::uint32_t resolved_task_time_cost_cash_points(const TaskTemplateDef& task_template_def) noexcept
{
    return round_to_cash_points(
        resolved_expected_task_hours_in_game(task_template_def) *
        static_cast<double>(k_cash_points_per_in_game_hour));
}

std::uint32_t resolved_task_risk_cost_cash_points(
    const TaskTemplateDef& task_template_def,
    std::uint32_t direct_cost_cash_points,
    std::uint32_t time_cost_cash_points) noexcept
{
    return scale_cash_points_by_multiplier(
        direct_cost_cash_points + time_cost_cash_points,
        resolved_task_risk_multiplier(task_template_def));
}

bool item_is_accessible_for_task(
    const CampaignState& campaign,
    ItemId item_id) noexcept
{
    if (item_id.value == 0U)
    {
        return true;
    }

    const auto* item_def = find_item_def(item_id);
    if (item_def == nullptr)
    {
        return false;
    }

    if (!TechnologySystem::item_unlocked(campaign, item_id))
    {
        return false;
    }

    switch (item_def->source_rule)
    {
    case ItemSourceRule::BuyOnly:
    case ItemSourceRule::BuyOrCraft:
        return true;
    case ItemSourceRule::CraftOnly:
        return TechnologySystem::craft_output_unlocked(campaign, item_id);
    case ItemSourceRule::HarvestOnly:
        return false;
    case ItemSourceRule::None:
    default:
        return false;
    }
}

bool structure_is_buildable_for_task(
    const CampaignState& campaign,
    StructureId structure_id) noexcept
{
    if (structure_id.value == 0U)
    {
        return false;
    }

    for (const auto& item_def : all_item_defs())
    {
        if (item_def.linked_structure_id == structure_id &&
            item_is_accessible_for_task(campaign, item_def.item_id))
        {
            return true;
        }
    }

    return false;
}

bool any_craftable_output_available(const CampaignState& campaign) noexcept
{
    for (const auto& recipe_def : all_craft_recipe_defs())
    {
        if (recipe_def.output_item_id.value != 0U &&
            TechnologySystem::recipe_unlocked(campaign, recipe_def.recipe_id))
        {
            return true;
        }
    }

    return false;
}

bool has_sellable_crafted_item() noexcept
{
    for (const auto& item_def : all_item_defs())
    {
        if (item_def.source_rule == ItemSourceRule::CraftOnly &&
            item_has_capability(item_def, ITEM_CAPABILITY_SELL))
        {
            return true;
        }
    }

    return false;
}

template <typename T>
T choose_candidate(
    const std::vector<T>& candidates,
    std::uint64_t seed) noexcept
{
    return candidates.empty()
        ? T {}
        : candidates[static_cast<std::size_t>(seed % candidates.size())];
}

std::vector<ItemId> collect_item_candidates(
    RuntimeInvocation& invocation,
    const TaskTemplateDef& task_template_def)
{
    if (task_template_def.progress_kind == TaskProgressKind::BuyItem)
    {
        if (task_template_def.item_id.value != 0U)
        {
            return item_is_buyable_from_authored_phone_stock(invocation, task_template_def.item_id)
                ? std::vector<ItemId> {task_template_def.item_id}
                : std::vector<ItemId> {};
        }

        std::vector<ItemId> candidates {};
        const auto* site_content = find_prototype_site_content(task_board_site_run(invocation).site_id);
        if (site_content == nullptr)
        {
            return candidates;
        }

        for (const auto& stock_entry : site_content->phone_buy_stock_pool)
        {
            if (item_is_buyable_from_authored_phone_stock(invocation, stock_entry.item_id))
            {
                candidates.push_back(stock_entry.item_id);
            }
        }

        return candidates;
    }

    if (task_template_def.item_id.value != 0U)
    {
        return item_is_accessible_for_task(task_board_campaign(invocation), task_template_def.item_id)
            ? std::vector<ItemId> {task_template_def.item_id}
            : std::vector<ItemId> {};
    }

    std::vector<ItemId> candidates {};
    for (const auto& item_def : all_item_defs())
    {
        if (!item_is_accessible_for_task(task_board_campaign(invocation), item_def.item_id))
        {
            continue;
        }

        switch (task_template_def.progress_kind)
        {
        case TaskProgressKind::SellItem:
            if (item_has_capability(item_def, ITEM_CAPABILITY_SELL))
            {
                candidates.push_back(item_def.item_id);
            }
            break;
        case TaskProgressKind::TransferItem:
            candidates.push_back(item_def.item_id);
            break;
        case TaskProgressKind::SubmitItem:
            candidates.push_back(item_def.item_id);
            break;
        case TaskProgressKind::ConsumeItem:
            if (item_def.consumable)
            {
                candidates.push_back(item_def.item_id);
            }
            break;
        case TaskProgressKind::CraftItem:
            if (item_def.source_rule == ItemSourceRule::CraftOnly)
            {
                candidates.push_back(item_def.item_id);
            }
            break;
        default:
            break;
        }
    }

    return candidates;
}

std::vector<PlantId> collect_plant_candidates(
    RuntimeInvocation& invocation,
    const TaskTemplateDef& task_template_def)
{
    if (task_template_def.plant_id.value != 0U)
    {
        return TechnologySystem::plant_unlocked(task_board_campaign(invocation), task_template_def.plant_id)
            ? std::vector<PlantId> {task_template_def.plant_id}
            : std::vector<PlantId> {};
    }

    if (task_template_def.item_id.value != 0U)
    {
        const auto* item_def = find_item_def(task_template_def.item_id);
        if (item_def != nullptr &&
            item_def->linked_plant_id.value != 0U &&
            TechnologySystem::plant_unlocked(task_board_campaign(invocation), item_def->linked_plant_id))
        {
            return {item_def->linked_plant_id};
        }
        return {};
    }

    std::vector<PlantId> candidates {};
    for (const auto& plant_def : all_plant_defs())
    {
        if (!TechnologySystem::plant_unlocked(task_board_campaign(invocation), plant_def.plant_id))
        {
            continue;
        }

        bool has_seed_item = false;
        for (const auto& item_def : all_item_defs())
        {
            if (item_def.linked_plant_id == plant_def.plant_id &&
                item_is_accessible_for_task(task_board_campaign(invocation), item_def.item_id))
            {
                has_seed_item = true;
                break;
            }
        }
        if (has_seed_item)
        {
            candidates.push_back(plant_def.plant_id);
        }
    }

    return candidates;
}

std::vector<RecipeId> collect_recipe_candidates(
    RuntimeInvocation& invocation,
    const TaskTemplateDef& task_template_def)
{
    if (task_template_def.recipe_id.value != 0U)
    {
        return find_craft_recipe_def(task_template_def.recipe_id) != nullptr &&
                TechnologySystem::recipe_unlocked(task_board_campaign(invocation), task_template_def.recipe_id)
            ? std::vector<RecipeId> {task_template_def.recipe_id}
            : std::vector<RecipeId> {};
    }

    std::vector<RecipeId> candidates {};
    for (const auto& recipe_def : all_craft_recipe_defs())
    {
        if (TechnologySystem::recipe_unlocked(task_board_campaign(invocation), recipe_def.recipe_id))
        {
            candidates.push_back(recipe_def.recipe_id);
        }
    }

    return candidates;
}

std::vector<StructureId> collect_structure_candidates(
    RuntimeInvocation& invocation,
    StructureId fixed_structure_id) noexcept
{
    if (fixed_structure_id.value != 0U)
    {
        return structure_is_buildable_for_task(task_board_campaign(invocation), fixed_structure_id)
            ? std::vector<StructureId> {fixed_structure_id}
            : std::vector<StructureId> {};
    }

    std::vector<StructureId> candidates {};
    for (const auto& structure_def : all_structure_defs())
    {
        if (structure_is_buildable_for_task(task_board_campaign(invocation), structure_def.structure_id))
        {
            candidates.push_back(structure_def.structure_id);
        }
    }

    return candidates;
}

std::vector<ActionKind> collect_action_candidates(const TaskTemplateDef& task_template_def)
{
    if (task_template_def.action_kind != ActionKind::None)
    {
        return {task_template_def.action_kind};
    }

    return {
        ActionKind::Plant,
        ActionKind::Build,
        ActionKind::Repair,
        ActionKind::Water,
        ActionKind::ClearBurial,
        ActionKind::Craft};
}

bool task_template_is_eligible(
    RuntimeInvocation& invocation,
    const TaskTemplateDef& task_template_def) noexcept
{
    switch (task_template_def.progress_kind)
    {
    case TaskProgressKind::None:
        return false;

    case TaskProgressKind::BuyItem:
    case TaskProgressKind::SellItem:
    case TaskProgressKind::TransferItem:
    case TaskProgressKind::SubmitItem:
    case TaskProgressKind::ConsumeItem:
    case TaskProgressKind::CraftItem:
        return !collect_item_candidates(invocation, task_template_def).empty();

    case TaskProgressKind::PlantItem:
    case TaskProgressKind::PlantCountAtDensity:
        return !collect_plant_candidates(invocation, task_template_def).empty();

    case TaskProgressKind::CraftRecipe:
        return !collect_recipe_candidates(invocation, task_template_def).empty();

    case TaskProgressKind::PerformAction:
        return !collect_action_candidates(task_template_def).empty();

    case TaskProgressKind::BuildStructure:
        return !collect_structure_candidates(invocation, task_template_def.structure_id).empty();

    case TaskProgressKind::BuildStructureSet:
    {
        const auto candidates = collect_structure_candidates(invocation, StructureId {});
        const auto fixed_count =
            (task_template_def.structure_id.value != 0U ? 1U : 0U) +
            (task_template_def.secondary_structure_id.value != 0U ? 1U : 0U) +
            (task_template_def.tertiary_structure_id.value != 0U ? 1U : 0U);
        return fixed_count == 3U || candidates.size() >= (3U - fixed_count);
    }

    case TaskProgressKind::BuildAnyStructure:
        return !collect_structure_candidates(invocation, StructureId {}).empty();

    case TaskProgressKind::CraftAnyItem:
        return any_craftable_output_available(task_board_campaign(invocation));

    case TaskProgressKind::SellCraftedItem:
        return has_sellable_crafted_item();

    case TaskProgressKind::RestorationTiles:
    case TaskProgressKind::PlantAny:
    case TaskProgressKind::EarnMoney:
    case TaskProgressKind::KeepAllWorkerMetersAboveForDuration:
    case TaskProgressKind::KeepTileMoistureAtLeastForDuration:
    case TaskProgressKind::KeepTileHeatAtMostForDuration:
    case TaskProgressKind::KeepTileDustAtMostForDuration:
    case TaskProgressKind::AnyPlantDensityAtLeast:
    case TaskProgressKind::PlantWindProtectionAtLeast:
    case TaskProgressKind::PlantHeatProtectionAtLeast:
    case TaskProgressKind::PlantDustProtectionAtLeast:
    case TaskProgressKind::KeepAllDevicesIntegrityAboveForDuration:
    case TaskProgressKind::NearbyPopulatedPlantTilesAtLeast:
    case TaskProgressKind::KeepAllLivingPlantsNotWitheringForDuration:
        return true;
    default:
        return false;
    }
}

std::vector<TaskRewardDraftOption> make_reward_draft_options(
    RuntimeInvocation& invocation,
    TaskInstanceId task_instance_id,
    const TaskTemplateDef& task_template_def,
    const SiteOnboardingTaskSeedDef* onboarding_seed)
{
    (void)invocation;
    (void)task_instance_id;
    (void)task_template_def;
    if (onboarding_seed == nullptr)
    {
        return {};
    }

    std::vector<TaskRewardDraftOption> options {};
    const RewardCandidateId reward_ids[] {
        onboarding_seed->reward_candidate_id,
        onboarding_seed->secondary_reward_candidate_id,
        onboarding_seed->tertiary_reward_candidate_id,
        onboarding_seed->quaternary_reward_candidate_id};
    for (const auto reward_candidate_id : reward_ids)
    {
        if (reward_candidate_id.value == 0U ||
            find_reward_candidate_def(reward_candidate_id) == nullptr)
        {
            continue;
        }

        const bool already_present = std::any_of(
            options.begin(),
            options.end(),
            [&](const TaskRewardDraftOption& option) {
                return option.reward_candidate_id == reward_candidate_id;
            });
        if (!already_present)
        {
            options.push_back(TaskRewardDraftOption {reward_candidate_id, false});
        }
    }

    return options;
}

const SiteOnboardingTaskSeedDef* find_onboarding_seed_def(
    SiteId site_id,
    TaskTemplateId task_template_id) noexcept
{
    for (const auto& seed_def : all_site_onboarding_task_seed_defs())
    {
        if (seed_def.site_id == site_id &&
            seed_def.task_template_id == task_template_id)
        {
            return &seed_def;
        }
    }

    return nullptr;
}

const SiteOnboardingTaskSeedDef* find_root_onboarding_seed(SiteId site_id) noexcept
{
    for (const auto& candidate_seed : all_site_onboarding_task_seed_defs())
    {
        if (candidate_seed.site_id != site_id ||
            candidate_seed.task_template_id.value == 0U)
        {
            continue;
        }

        bool referenced_as_follow_up = false;
        for (const auto& other_seed : all_site_onboarding_task_seed_defs())
        {
            if (other_seed.site_id != site_id)
            {
                continue;
            }

            if (other_seed.follow_up_task_template_id == candidate_seed.task_template_id)
            {
                referenced_as_follow_up = true;
                break;
            }
        }

        if (!referenced_as_follow_up)
        {
            return &candidate_seed;
        }
    }

    return nullptr;
}

bool task_template_is_onboarding_only(TaskTemplateId task_template_id) noexcept
{
    for (const auto& seed_def : all_site_onboarding_task_seed_defs())
    {
        if (seed_def.task_template_id == task_template_id)
        {
            return true;
        }
    }

    return false;
}

bool onboarding_chain_effective(
    SiteId site_id,
    const TaskBoardState& board) noexcept
{
    for (const auto& task : board.visible_tasks)
    {
        if (task.runtime_list_kind != TaskRuntimeListKind::Claimed &&
            find_onboarding_seed_def(site_id, task.task_template_id) != nullptr)
        {
            return true;
        }
    }

    return false;
}

std::uint32_t next_task_instance_id(const TaskBoardState& board) noexcept
{
    std::uint32_t next_id = 1U;
    for (const auto& task : board.visible_tasks)
    {
        next_id = std::max(next_id, task.task_instance_id.value + 1U);
    }

    return next_id;
}

TaskInstanceState make_task_instance(
    RuntimeInvocation& invocation,
    TaskInstanceId task_instance_id,
    const TaskTemplateDef& task_template_def,
    const SiteCounters& counters,
    const SiteOnboardingTaskSeedDef* onboarding_seed = nullptr)
{
    const auto task_seed = mix_seed(
        task_board_world(invocation).site_attempt_seed(),
        task_template_def.task_template_id.value,
        task_instance_id.value);
    TaskInstanceState task {};
    task.task_instance_id = task_instance_id;
    task.task_template_id = task_template_def.task_template_id;
    task.publisher_faction_id = task_template_def.publisher_faction_id;
    task.task_tier_id = task_template_def.task_tier_id;
    task.target_amount =
        (onboarding_seed != nullptr && onboarding_seed->target_amount != 0U)
        ? normalized_task_target(onboarding_seed->target_amount)
        : resolved_task_target_amount(task_template_def, counters);
    task.required_count =
        (onboarding_seed != nullptr && onboarding_seed->required_count != 0U)
        ? normalized_required_count(onboarding_seed->required_count)
        : resolved_task_required_count(task_template_def);
    task.item_id =
        (onboarding_seed != nullptr && onboarding_seed->item_id.value != 0U)
        ? onboarding_seed->item_id
        : choose_candidate(
              collect_item_candidates(invocation, task_template_def),
              mix_seed(task_seed, 2U, 0U));
    task.plant_id =
        (onboarding_seed != nullptr && onboarding_seed->plant_id.value != 0U)
        ? onboarding_seed->plant_id
        : choose_candidate(
              collect_plant_candidates(invocation, task_template_def),
              mix_seed(task_seed, 3U, 0U));
    if (task_template_def.progress_kind == TaskProgressKind::KeepAllLivingPlantsNotWitheringForDuration &&
        task_template_def.plant_id.value == 0U)
    {
        task.plant_id = PlantId {};
    }
    if (task.item_id.value == 0U && task.plant_id.value != 0U)
    {
        for (const auto& item_def : all_item_defs())
        {
            if (item_def.linked_plant_id == task.plant_id &&
                item_is_accessible_for_task(task_board_campaign(invocation), item_def.item_id))
            {
                task.item_id = item_def.item_id;
                break;
            }
        }
    }
    else if (task.plant_id.value == 0U && task.item_id.value != 0U)
    {
        const auto* item_def = find_item_def(task.item_id);
        if (item_def != nullptr)
        {
            task.plant_id = item_def->linked_plant_id;
        }
    }
    task.recipe_id =
        (onboarding_seed != nullptr && onboarding_seed->recipe_id.value != 0U)
        ? onboarding_seed->recipe_id
        : choose_candidate(
              collect_recipe_candidates(invocation, task_template_def),
              mix_seed(task_seed, 4U, 0U));
    task.structure_id =
        (onboarding_seed != nullptr && onboarding_seed->structure_id.value != 0U)
        ? onboarding_seed->structure_id
        : choose_candidate(
              collect_structure_candidates(invocation, task_template_def.structure_id),
              mix_seed(task_seed, 5U, 0U));
    task.secondary_structure_id =
        (onboarding_seed != nullptr && onboarding_seed->secondary_structure_id.value != 0U)
        ? onboarding_seed->secondary_structure_id
        : task_template_def.secondary_structure_id;
    task.tertiary_structure_id =
        (onboarding_seed != nullptr && onboarding_seed->tertiary_structure_id.value != 0U)
        ? onboarding_seed->tertiary_structure_id
        : task_template_def.tertiary_structure_id;
    if (task_template_def.progress_kind == TaskProgressKind::BuildStructureSet)
    {
        auto candidates = collect_structure_candidates(invocation, StructureId {});
        auto erase_if_present = [&](StructureId structure_id) {
            if (structure_id.value == 0U)
            {
                return;
            }
            candidates.erase(
                std::remove(candidates.begin(), candidates.end(), structure_id),
                candidates.end());
        };

        if (task.structure_id.value == 0U)
        {
            task.structure_id = choose_candidate(candidates, mix_seed(task_seed, 5U, 1U));
        }
        erase_if_present(task.structure_id);
        if (task.secondary_structure_id.value == 0U)
        {
            task.secondary_structure_id = choose_candidate(candidates, mix_seed(task_seed, 6U, 1U));
        }
        erase_if_present(task.secondary_structure_id);
        if (task.tertiary_structure_id.value == 0U)
        {
            task.tertiary_structure_id = choose_candidate(candidates, mix_seed(task_seed, 7U, 1U));
        }
    }
    task.action_kind =
        (onboarding_seed != nullptr && onboarding_seed->action_kind != ActionKind::None)
        ? onboarding_seed->action_kind
        : choose_candidate(
              collect_action_candidates(task_template_def),
              mix_seed(task_seed, 8U, 0U));
    task.threshold_value =
        (onboarding_seed != nullptr && std::fabs(onboarding_seed->threshold_value) > 0.0001f)
        ? onboarding_seed->threshold_value
        : resolved_task_threshold_value(task_template_def);
    task.expected_task_hours_in_game = static_cast<float>(resolved_expected_task_hours_in_game(task_template_def));
    task.risk_multiplier = resolved_task_risk_multiplier(task_template_def);
    task.direct_cost_cash_points = resolved_task_direct_cost_cash_points(task_template_def, task);
    task.time_cost_cash_points = resolved_task_time_cost_cash_points(task_template_def);
    task.risk_cost_cash_points = resolved_task_risk_cost_cash_points(
        task_template_def,
        task.direct_cost_cash_points,
        task.time_cost_cash_points);
    task.difficulty_cash_points =
        task.direct_cost_cash_points +
        task.time_cost_cash_points +
        task.risk_cost_cash_points;
    task.reward_budget_cash_points = scale_cash_points_by_percent(
        task.difficulty_cash_points,
        k_reward_budget_multiplier_percent);
    task.current_progress_amount = 0U;
    task.reward_draft_options =
        make_reward_draft_options(invocation, task_instance_id, task_template_def, onboarding_seed);
    task.runtime_list_kind = TaskRuntimeListKind::Visible;
    if (onboarding_seed != nullptr && onboarding_seed->follow_up_task_template_id.value != 0U)
    {
        task.follow_up_task_template_id = onboarding_seed->follow_up_task_template_id;
        task.has_follow_up_task_template = true;
    }
    return task;
}

void reset_task_board(TaskBoardState& board) noexcept
{
    board.visible_tasks.clear();
    board.tracked_tiles.clear();
    board.accepted_task_ids.clear();
    board.completed_task_ids.clear();
    board.claimed_task_ids.clear();
    board.active_chain_state.reset();
    board.task_pool_size = 0U;
    board.tracked_tile_width = 0U;
    board.tracked_tile_height = 0U;
    board.fully_grown_tile_count = 0U;
    board.site_completion_tile_threshold = 0U;
    board.tracked_living_plant_count = 0U;
    board.refresh_generation = 0U;
    board.minutes_until_next_refresh = 0.0;
    board.worker = TaskTrackedWorkerState {};
    board.all_tracked_living_plants_stable = false;
}

void initialize_task_tracking_cache(
    RuntimeInvocation& invocation,
    TaskBoardState& board) noexcept
{
    board.tracked_tile_width = task_board_world(invocation).tile_width();
    board.tracked_tile_height = task_board_world(invocation).tile_height();
    const auto tile_count = task_board_world(invocation).tile_count();
    board.tracked_tiles.assign(tile_count, TaskTrackedTileState {});
    board.site_completion_tile_threshold = task_board_world(invocation).read_counters().site_completion_tile_threshold;
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

bool complete_task(
    RuntimeInvocation& invocation,
    TaskBoardState& board,
    TaskInstanceState& task) noexcept;

bool is_snapshot_progress_kind(TaskProgressKind progress_kind) noexcept;
bool is_duration_progress_kind(TaskProgressKind progress_kind) noexcept;
bool refresh_snapshot_task_progress(
    RuntimeInvocation& invocation,
    TaskBoardState& board,
    TaskInstanceState& task) noexcept;
bool refresh_duration_task_condition(
    RuntimeInvocation& invocation,
    TaskInstanceState& task) noexcept;

void activate_task_instance(
    RuntimeInvocation& invocation,
    TaskBoardState& board,
    TaskInstanceState& task) noexcept
{
    if (task.runtime_list_kind != TaskRuntimeListKind::Visible)
    {
        return;
    }

    task.runtime_list_kind = TaskRuntimeListKind::Accepted;
    task.progress_accumulator = 0.0;
    task.requirement_mask = 0U;
    board.accepted_task_ids.push_back(task.task_instance_id);
    const auto* task_template_def = find_task_template_def(task.task_template_id);
    if (task_template_def != nullptr && is_snapshot_progress_kind(task_template_def->progress_kind))
    {
        (void)refresh_snapshot_task_progress(invocation, board, task);
    }
    else if (task_template_def != nullptr && is_duration_progress_kind(task_template_def->progress_kind))
    {
        if (task_template_def->progress_kind == TaskProgressKind::KeepAllLivingPlantsNotWitheringForDuration)
        {
            const auto& board_state = task_board_world(invocation).read_task_board();
            task.requirement_mask =
                (board_state.all_tracked_living_plants_stable &&
                    board_state.tracked_living_plant_count >=
                        normalized_required_count(task.required_count))
                ? 1U
                : 0U;
        }
        else
        {
            (void)refresh_duration_task_condition(invocation, task);
        }
    }
    else if (task.current_progress_amount >= std::max<std::uint32_t>(1U, task.target_amount))
    {
        (void)complete_task(invocation, board, task);
    }
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

void mark_task_projection_dirty(RuntimeInvocation& invocation) noexcept
{
    (void)invocation;
}

bool queue_reward_message(
    RuntimeInvocation& invocation,
    const RewardCandidateDef& reward_candidate_def)
{
    GameMessage reward_message {};

    switch (reward_candidate_def.effect_kind)
    {
    case RewardEffectKind::Money:
        if (reward_candidate_def.money_delta_cash_points == 0)
        {
            return false;
        }
        reward_message.type = GameMessageType::EconomyMoneyAwardRequested;
        reward_message.set_payload(EconomyMoneyAwardRequestedMessage {
            reward_candidate_def.money_delta_cash_points});
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
        if (reward_candidate_def.reputation_delta == 0)
        {
            return false;
        }
        reward_message.type = GameMessageType::CampaignReputationAwardRequested;
        reward_message.set_payload(CampaignReputationAwardRequestedMessage {
            reward_candidate_def.reputation_delta});
        break;
    case RewardEffectKind::FactionReputation:
        if (reward_candidate_def.faction_id.value == 0U ||
            reward_candidate_def.reputation_delta == 0)
        {
            return false;
        }
        reward_message.type = GameMessageType::FactionReputationAwardRequested;
        reward_message.set_payload(FactionReputationAwardRequestedMessage {
            reward_candidate_def.faction_id.value,
            reward_candidate_def.reputation_delta});
        break;
    case RewardEffectKind::None:
    default:
        return false;
    }

    task_board_message_queue(invocation).push_back(reward_message);
    return true;
}

bool complete_task(
    RuntimeInvocation& invocation,
    TaskBoardState& board,
    TaskInstanceState& task) noexcept
{
    (void)invocation;
    if (task.runtime_list_kind != TaskRuntimeListKind::Accepted)
    {
        return false;
    }

    task.runtime_list_kind = TaskRuntimeListKind::PendingClaim;
    board.completed_task_ids.push_back(task.task_instance_id);
    remove_task_id(board.accepted_task_ids, task.task_instance_id);
    return true;
}

bool item_matches_task(
    const TaskInstanceState& task,
    ItemId item_id) noexcept
{
    return task.item_id.value == 0U || task.item_id == item_id;
}

bool plant_matches_task(
    const TaskInstanceState& task,
    PlantId planted_plant_id) noexcept
{
    return task.plant_id.value == 0U || task.plant_id == planted_plant_id;
}

bool structure_matches_task(
    const TaskInstanceState& task,
    StructureId structure_id) noexcept
{
    return task.structure_id.value == 0U || task.structure_id == structure_id;
}

bool action_matches_task(
    const TaskInstanceState& task,
    ActionKind action_kind) noexcept
{
    return task.action_kind == ActionKind::None || task.action_kind == action_kind;
}

template <typename Matcher>
void advance_matching_accepted_tasks(
    RuntimeInvocation& invocation,
    std::uint32_t amount,
    Matcher matcher)
{
    auto& board = task_board_world(invocation).own_task_board();
    bool mutated = false;

    for (auto& task : board.visible_tasks)
    {
        if (task.runtime_list_kind != TaskRuntimeListKind::Accepted)
        {
            continue;
        }

        const auto* task_template_def = find_task_template_def(task.task_template_id);
        if (task_template_def == nullptr || !matcher(*task_template_def, task))
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

        if (next_progress >= target && complete_task(invocation, board, task))
        {
            mutated = true;
        }
    }

    if (mutated)
    {
        mark_task_projection_dirty(invocation);
    }
}

std::uint8_t build_structure_set_mask(
    const TaskInstanceState& task,
    StructureId structure_id) noexcept
{
    std::uint8_t mask = 0U;
    if (task.structure_id == structure_id)
    {
        mask |= 1U << 0U;
    }
    if (task.secondary_structure_id == structure_id)
    {
        mask |= 1U << 1U;
    }
    if (task.tertiary_structure_id == structure_id)
    {
        mask |= 1U << 2U;
    }
    return mask;
}

std::uint32_t popcount_mask(std::uint8_t mask) noexcept
{
    std::uint32_t count = 0U;
    for (; mask != 0U; mask &= static_cast<std::uint8_t>(mask - 1U))
    {
        ++count;
    }
    return count;
}

void advance_build_structure_set_tasks(
    RuntimeInvocation& invocation,
    StructureId structure_id)
{
    auto& board = task_board_world(invocation).own_task_board();
    bool mutated = false;

    for (auto& task : board.visible_tasks)
    {
        if (task.runtime_list_kind != TaskRuntimeListKind::Accepted)
        {
            continue;
        }

        const auto* task_template_def = find_task_template_def(task.task_template_id);
        if (task_template_def == nullptr ||
            task_template_def->progress_kind != TaskProgressKind::BuildStructureSet)
        {
            continue;
        }

        const auto next_mask =
            static_cast<std::uint8_t>(task.requirement_mask | build_structure_set_mask(task, structure_id));
        if (next_mask == task.requirement_mask)
        {
            continue;
        }

        task.requirement_mask = next_mask;
        const auto next_progress = std::min(
            task.target_amount,
            popcount_mask(task.requirement_mask));
        if (next_progress != task.current_progress_amount)
        {
            task.current_progress_amount = next_progress;
            mutated = true;
        }

        if (next_progress >= std::max<std::uint32_t>(1U, task.target_amount) &&
            complete_task(invocation, board, task))
        {
            mutated = true;
        }
    }

    if (mutated)
    {
        mark_task_projection_dirty(invocation);
    }
}

bool is_snapshot_progress_kind(TaskProgressKind progress_kind) noexcept
{
    switch (progress_kind)
    {
    case TaskProgressKind::RestorationTiles:
    case TaskProgressKind::PlantCountAtDensity:
    case TaskProgressKind::AnyPlantDensityAtLeast:
    case TaskProgressKind::PlantWindProtectionAtLeast:
    case TaskProgressKind::PlantHeatProtectionAtLeast:
    case TaskProgressKind::PlantDustProtectionAtLeast:
    case TaskProgressKind::NearbyPopulatedPlantTilesAtLeast:
        return true;
    default:
        return false;
    }
}

bool is_duration_progress_kind(TaskProgressKind progress_kind) noexcept
{
    switch (progress_kind)
    {
    case TaskProgressKind::KeepAllWorkerMetersAboveForDuration:
    case TaskProgressKind::KeepTileMoistureAtLeastForDuration:
    case TaskProgressKind::KeepTileHeatAtMostForDuration:
    case TaskProgressKind::KeepTileDustAtMostForDuration:
    case TaskProgressKind::KeepAllDevicesIntegrityAboveForDuration:
    case TaskProgressKind::KeepAllLivingPlantsNotWitheringForDuration:
        return true;
    default:
        return false;
    }
}

bool matches_density_task_filter(
    const TaskInstanceState& task,
    PlantId plant_id) noexcept
{
    if (plant_id.value == 0U)
    {
        return false;
    }

    if (task.plant_id.value != 0U)
    {
        return task.plant_id == plant_id;
    }

    return plant_id.value != k_plant_straw_checkerboard;
}

std::uint32_t count_plants_with_density_at_least(
    const TaskBoardState& board,
    const TaskInstanceState& task) noexcept
{
    std::uint32_t count = 0U;
    for (const auto& tile : board.tracked_tiles)
    {
        if (!matches_density_task_filter(task, PlantId {tile.plant_type_id}) ||
            tile.plant_density < task.threshold_value)
        {
            continue;
        }

        ++count;
    }

    return count;
}

std::uint32_t count_plants_with_protection_at_least(
    const TaskBoardState& board,
    const TaskInstanceState& task,
    PlantProtectionChannel channel) noexcept
{
    std::uint32_t count = 0U;
    for (const auto& tile : board.tracked_tiles)
    {
        if (!tracked_tile_has_plant(tile))
        {
            continue;
        }

        if (task.plant_id.value != 0U && tile.plant_type_id != task.plant_id.value)
        {
            continue;
        }

        float protection = 0.0f;
        switch (channel)
        {
        case PlantProtectionChannel::Wind:
            protection = tile.wind_protection;
            break;
        case PlantProtectionChannel::Heat:
            protection = tile.heat_protection;
            break;
        case PlantProtectionChannel::Dust:
            protection = tile.dust_protection;
            break;
        default:
            break;
        }

        if (protection >= task.threshold_value)
        {
            ++count;
        }
    }

    return count;
}

std::uint32_t count_nearby_populated_plant_tiles(
    const TaskBoardState& board) noexcept
{
    std::uint32_t count = 0U;
    for (const auto& tile : board.tracked_tiles)
    {
        if (!tracked_tile_has_plant(tile))
        {
            continue;
        }

        if (tile.populated_neighbor_count > 0U)
        {
            ++count;
        }
    }

    return count;
}

std::uint32_t calculate_snapshot_progress(
    RuntimeInvocation& invocation,
    const TaskTemplateDef& task_template_def,
    const TaskInstanceState& task) noexcept
{
    const auto& board = task_board_world(invocation).read_task_board();
    switch (task_template_def.progress_kind)
    {
    case TaskProgressKind::RestorationTiles:
        return board.fully_grown_tile_count;
    case TaskProgressKind::PlantCountAtDensity:
    case TaskProgressKind::AnyPlantDensityAtLeast:
        return count_plants_with_density_at_least(board, task);
    case TaskProgressKind::PlantWindProtectionAtLeast:
        return count_plants_with_protection_at_least(
            board,
            task,
            PlantProtectionChannel::Wind);
    case TaskProgressKind::PlantHeatProtectionAtLeast:
        return count_plants_with_protection_at_least(
            board,
            task,
            PlantProtectionChannel::Heat);
    case TaskProgressKind::PlantDustProtectionAtLeast:
        return count_plants_with_protection_at_least(
            board,
            task,
            PlantProtectionChannel::Dust);
    case TaskProgressKind::NearbyPopulatedPlantTilesAtLeast:
        return count_nearby_populated_plant_tiles(board);
    default:
        return 0U;
    }
}

bool all_worker_meters_at_least(
    const TaskBoardState& board,
    float threshold) noexcept
{
    return board.worker.valid &&
        board.worker.health >= threshold &&
        board.worker.hydration >= threshold &&
        board.worker.nourishment >= threshold &&
        board.worker.energy >= threshold &&
        board.worker.morale >= threshold;
}

bool all_devices_integrity_at_least(
    const TaskBoardState& board,
    float threshold) noexcept
{
    bool found_device = false;
    for (const auto& tile : board.tracked_tiles)
    {
        if (tile.device_structure_id == 0U)
        {
            continue;
        }

        found_device = true;
        if ((tile.device_integrity * 100.0f) < threshold)
        {
            return false;
        }
    }

    return found_device;
}

std::uint32_t count_tiles_moisture_at_least(
    const TaskBoardState& board,
    float threshold) noexcept
{
    std::uint32_t count = 0U;
    for (const auto& tile : board.tracked_tiles)
    {
        if (tile.moisture >= threshold)
        {
            ++count;
        }
    }

    return count;
}

std::uint32_t count_tiles_heat_at_most(
    const TaskBoardState& board,
    float threshold) noexcept
{
    std::uint32_t count = 0U;
    for (const auto& tile : board.tracked_tiles)
    {
        if (tile.heat <= threshold)
        {
            ++count;
        }
    }

    return count;
}

std::uint32_t count_tiles_dust_at_most(
    const TaskBoardState& board,
    float threshold) noexcept
{
    std::uint32_t count = 0U;
    for (const auto& tile : board.tracked_tiles)
    {
        if (tile.dust <= threshold)
        {
            ++count;
        }
    }

    return count;
}

bool living_plant_duration_condition_met(
    const TaskInstanceState& task,
    const LivingPlantStabilityChangedMessage& payload) noexcept
{
    return (payload.status_flags & LIVING_PLANT_STABILITY_ALL_TRACKED_PLANTS_STABLE) != 0U &&
        payload.tracked_living_plant_count >= normalized_required_count(task.required_count);
}

bool duration_condition_met(
    RuntimeInvocation& invocation,
    const TaskTemplateDef& task_template_def,
    TaskInstanceState& task) noexcept
{
    const auto& board = task_board_world(invocation).read_task_board();
    switch (task_template_def.progress_kind)
    {
    case TaskProgressKind::KeepAllWorkerMetersAboveForDuration:
        return all_worker_meters_at_least(board, task.threshold_value);
    case TaskProgressKind::KeepAllDevicesIntegrityAboveForDuration:
        return all_devices_integrity_at_least(board, task.threshold_value);
    case TaskProgressKind::KeepTileMoistureAtLeastForDuration:
        return count_tiles_moisture_at_least(board, task.threshold_value) >=
            normalized_required_count(task.required_count);
    case TaskProgressKind::KeepTileHeatAtMostForDuration:
        return count_tiles_heat_at_most(board, task.threshold_value) >=
            normalized_required_count(task.required_count);
    case TaskProgressKind::KeepTileDustAtMostForDuration:
        return count_tiles_dust_at_most(board, task.threshold_value) >=
            normalized_required_count(task.required_count);
    default:
        return false;
    }
}

bool refresh_snapshot_task_progress(
    RuntimeInvocation& invocation,
    TaskBoardState& board,
    TaskInstanceState& task) noexcept;

bool refresh_duration_task_condition(
    RuntimeInvocation& invocation,
    TaskInstanceState& task) noexcept
{
    const auto* task_template_def = find_task_template_def(task.task_template_id);
    if (task_template_def == nullptr || !is_duration_progress_kind(task_template_def->progress_kind))
    {
        return false;
    }

    const std::uint8_t next_mask =
        duration_condition_met(invocation, *task_template_def, task) ? 1U : 0U;
    if (task.requirement_mask == next_mask)
    {
        return false;
    }

    task.requirement_mask = next_mask;
    return true;
}

template <typename Predicate>
bool refresh_snapshot_tasks_if(
    RuntimeInvocation& invocation,
    Predicate predicate) noexcept
{
    auto& board = task_board_world(invocation).own_task_board();
    bool mutated = false;
    for (auto& task : board.visible_tasks)
    {
        const auto* task_template_def = find_task_template_def(task.task_template_id);
        if (task_template_def == nullptr || !predicate(*task_template_def))
        {
            continue;
        }

        mutated = refresh_snapshot_task_progress(invocation, board, task) || mutated;
    }
    return mutated;
}

template <typename Predicate>
bool refresh_duration_task_conditions_if(
    RuntimeInvocation& invocation,
    Predicate predicate) noexcept
{
    auto& board = task_board_world(invocation).own_task_board();
    bool mutated = false;
    for (auto& task : board.visible_tasks)
    {
        if (task.runtime_list_kind != TaskRuntimeListKind::Accepted)
        {
            continue;
        }

        const auto* task_template_def = find_task_template_def(task.task_template_id);
        if (task_template_def == nullptr || !predicate(*task_template_def))
        {
            continue;
        }

        mutated = refresh_duration_task_condition(invocation, task) || mutated;
    }
    return mutated;
}

bool refresh_snapshot_task_progress(
    RuntimeInvocation& invocation,
    TaskBoardState& board,
    TaskInstanceState& task) noexcept
{
    if (task.runtime_list_kind == TaskRuntimeListKind::PendingClaim ||
        task.runtime_list_kind == TaskRuntimeListKind::Claimed)
    {
        return false;
    }

    const auto* task_template_def = find_task_template_def(task.task_template_id);
    if (task_template_def == nullptr || !is_snapshot_progress_kind(task_template_def->progress_kind))
    {
        return false;
    }

    const auto target = std::max<std::uint32_t>(1U, task.target_amount);
    const auto next_progress =
        std::min(target, calculate_snapshot_progress(invocation, *task_template_def, task));
    bool mutated = false;
    if (next_progress != task.current_progress_amount)
    {
        task.current_progress_amount = next_progress;
        mutated = true;
    }

    if (task.runtime_list_kind == TaskRuntimeListKind::Accepted &&
        next_progress >= target &&
        complete_task(invocation, board, task))
    {
        mutated = true;
    }

    return mutated;
}

bool refresh_snapshot_tasks(
    RuntimeInvocation& invocation) noexcept
{
    auto& board = task_board_world(invocation).own_task_board();
    bool mutated = false;
    for (auto& task : board.visible_tasks)
    {
        mutated = refresh_snapshot_task_progress(invocation, board, task) || mutated;
    }
    return mutated;
}

bool refresh_normal_task_pool(
    RuntimeInvocation& invocation,
    bool force_dirty = false) noexcept
{
    auto& board = task_board_world(invocation).own_task_board();
    if (onboarding_chain_effective(task_board_site_run(invocation).site_id, board))
    {
        return false;
    }

    const std::uint32_t pool_size = normal_task_pool_size();
    bool mutated = board.task_pool_size != pool_size;
    board.task_pool_size = pool_size;

    const auto visible_begin = std::remove_if(
        board.visible_tasks.begin(),
        board.visible_tasks.end(),
        [](const TaskInstanceState& task) {
            return task.runtime_list_kind == TaskRuntimeListKind::Visible;
        });
    if (visible_begin != board.visible_tasks.end())
    {
        board.visible_tasks.erase(visible_begin, board.visible_tasks.end());
        mutated = true;
    }

    std::vector<const TaskTemplateDef*> candidates {};
    candidates.reserve(all_task_template_defs().size());
    for (const auto& task_template_def : all_task_template_defs())
    {
        if (task_template_is_onboarding_only(task_template_def.task_template_id) ||
            !task_template_is_eligible(invocation, task_template_def))
        {
            continue;
        }

        bool already_present = false;
        for (const auto& task : board.visible_tasks)
        {
            if (task.runtime_list_kind != TaskRuntimeListKind::Claimed &&
                task.task_template_id == task_template_def.task_template_id)
            {
                already_present = true;
                break;
            }
        }
        if (!already_present)
        {
            candidates.push_back(&task_template_def);
        }
    }

    const auto next_generation = board.refresh_generation + 1U;
    for (std::uint32_t slot_index = 0U;
         slot_index < pool_size && !candidates.empty();
         ++slot_index)
    {
        const std::size_t selected_index = static_cast<std::size_t>(
            mix_seed(task_board_world(invocation).site_attempt_seed(), next_generation, slot_index) %
            candidates.size());
        const TaskTemplateDef& task_template_def = *candidates[selected_index];
        board.visible_tasks.push_back(make_task_instance(
            invocation,
            TaskInstanceId {next_task_instance_id(board)},
            task_template_def,
            task_board_world(invocation).read_counters()));
        candidates.erase(candidates.begin() + static_cast<std::ptrdiff_t>(selected_index));
        mutated = true;
    }

    board.refresh_generation = next_generation;
    board.minutes_until_next_refresh = static_cast<double>(task_board_refresh_interval_minutes());
    mutated = refresh_snapshot_tasks(invocation) || mutated;
    if (mutated || force_dirty)
    {
        mark_task_projection_dirty(invocation);
    }

    return mutated;
}

bool update_duration_tasks(
    RuntimeInvocation& invocation) noexcept
{
    auto& board = task_board_world(invocation).own_task_board();
    const double step_minutes =
        runtime_minutes_from_real_seconds(
            make_game_state_access<TaskBoardSystem>(invocation).template read<RuntimeFixedStepSecondsTag>());
    bool mutated = false;

    for (auto& task : board.visible_tasks)
    {
        if (task.runtime_list_kind != TaskRuntimeListKind::Accepted)
        {
            continue;
        }

        const auto* task_template_def = find_task_template_def(task.task_template_id);
        if (task_template_def == nullptr || !is_duration_progress_kind(task_template_def->progress_kind))
        {
            continue;
        }

        if (task_template_def->progress_kind != TaskProgressKind::KeepAllLivingPlantsNotWitheringForDuration)
        {
            (void)refresh_duration_task_condition(invocation, task);
        }

        const bool satisfied = task.requirement_mask != 0U;
        const double next_accumulator = satisfied
            ? std::min<double>(static_cast<double>(task.target_amount), task.progress_accumulator + step_minutes)
            : 0.0;
        if (std::fabs(next_accumulator - task.progress_accumulator) > k_progress_epsilon)
        {
            task.progress_accumulator = next_accumulator;
            mutated = true;
        }

        const auto next_progress = std::min(
            std::max<std::uint32_t>(1U, task.target_amount),
            static_cast<std::uint32_t>(std::floor(task.progress_accumulator + k_progress_epsilon)));
        if (next_progress != task.current_progress_amount)
        {
            task.current_progress_amount = next_progress;
            mutated = true;
        }

        if (satisfied &&
            next_progress >= std::max<std::uint32_t>(1U, task.target_amount) &&
            complete_task(invocation, board, task))
        {
            mutated = true;
        }
    }

    return mutated;
}

void handle_site_run_started(
    RuntimeInvocation& invocation,
    const SiteRunStartedMessage& payload)
{
    auto& board = task_board_world(invocation).own_task_board();
    reset_task_board(board);
    initialize_task_tracking_cache(invocation, board);
    std::uint32_t site_seed_count = 0U;
    for (const auto& seed_def : all_site_onboarding_task_seed_defs())
    {
        if (seed_def.site_id.value == payload.site_id &&
            seed_def.task_template_id.value != 0U)
        {
            site_seed_count += 1U;
        }
    }

    board.task_pool_size = site_seed_count;
    board.minutes_until_next_refresh = 0.0;

    if (const auto* root_seed = find_root_onboarding_seed(SiteId {payload.site_id});
        root_seed != nullptr)
    {
        const auto* task_template_def = find_task_template_def(root_seed->task_template_id);
        if (task_template_def != nullptr)
        {
            board.visible_tasks.push_back(make_task_instance(
                invocation,
                TaskInstanceId {1U},
                *task_template_def,
                task_board_world(invocation).read_counters(),
                root_seed));
            activate_task_instance(invocation, board, board.visible_tasks.back());
        }
    }

    if (!board.visible_tasks.empty())
    {
        mark_task_projection_dirty(invocation);
    }
    else
    {
        (void)refresh_normal_task_pool(invocation, true);
    }
}

bool item_is_buyable_from_authored_phone_stock(
    RuntimeInvocation& invocation,
    ItemId item_id) noexcept
{
    if (item_id.value == 0U)
    {
        return false;
    }

    const auto* site_content = find_prototype_site_content(task_board_site_run(invocation).site_id);
    if (site_content == nullptr)
    {
        return false;
    }

    const auto* item_def = find_item_def(item_id);
    if (item_def == nullptr || item_buy_price_cash_points(item_id) == 0U)
    {
        return false;
    }

    bool authored = false;
    for (const auto& stock_entry : site_content->phone_buy_stock_pool)
    {
        if (stock_entry.item_id == item_id)
        {
            authored = true;
            break;
        }
    }
    if (!authored)
    {
        return false;
    }

    if (item_def->linked_plant_id.value != 0U)
    {
        return TechnologySystem::plant_unlocked(task_board_campaign(invocation), item_def->linked_plant_id);
    }

    return true;
}

void handle_task_accept_requested(
    RuntimeInvocation& invocation,
    const TaskAcceptRequestedMessage& payload)
{
    auto& board = task_board_world(invocation).own_task_board();
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

    activate_task_instance(invocation, board, *task);
    mark_task_projection_dirty(invocation);
}

void handle_task_reward_claim_requested(
    RuntimeInvocation& invocation,
    const TaskRewardClaimRequestedMessage& payload)
{
    auto& board = task_board_world(invocation).own_task_board();
    TaskInstanceState* task = find_task_instance(board, TaskInstanceId {payload.task_instance_id});
    if (task == nullptr || task->runtime_list_kind != TaskRuntimeListKind::PendingClaim)
    {
        return;
    }

    if (has_selected_reward(*task))
    {
        return;
    }

    TaskRewardDraftOption* reward_option = nullptr;
    if (payload.reward_candidate_id != 0U)
    {
        reward_option = find_reward_option(*task, RewardCandidateId {payload.reward_candidate_id});
    }
    else if (!task->reward_draft_options.empty())
    {
        reward_option = &task->reward_draft_options.front();
    }

    if (reward_option == nullptr)
    {
        return;
    }

    for (const auto& reward_bundle_option : task->reward_draft_options)
    {
        const auto* reward_candidate_def = find_reward_candidate_def(reward_bundle_option.reward_candidate_id);
        if (reward_candidate_def == nullptr || !queue_reward_message(invocation, *reward_candidate_def))
        {
            return;
        }
    }

    const auto claimed_task_instance_id = task->task_instance_id.value;
    const auto claimed_task_template_id = task->task_template_id.value;
    const auto reward_candidate_count =
        static_cast<std::uint32_t>(task->reward_draft_options.size());

    for (auto& reward_bundle_option : task->reward_draft_options)
    {
        reward_bundle_option.selected = true;
    }
    task->runtime_list_kind = TaskRuntimeListKind::Claimed;
    remove_task_id(board.completed_task_ids, task->task_instance_id);
    board.claimed_task_ids.push_back(task->task_instance_id);

    const bool claimed_onboarding_task =
        find_onboarding_seed_def(task_board_site_run(invocation).site_id, task->task_template_id) != nullptr;
    const bool had_follow_up_task_template = task->has_follow_up_task_template;
    if (task->has_follow_up_task_template)
    {
        const auto* follow_up_seed =
            find_onboarding_seed_def(task_board_site_run(invocation).site_id, task->follow_up_task_template_id);
        const auto* follow_up_template_def =
            find_task_template_def(task->follow_up_task_template_id);
        if (follow_up_seed != nullptr && follow_up_template_def != nullptr)
        {
            board.visible_tasks.push_back(make_task_instance(
                invocation,
                TaskInstanceId {next_task_instance_id(board)},
                *follow_up_template_def,
                task_board_world(invocation).read_counters(),
                follow_up_seed));
            activate_task_instance(invocation, board, board.visible_tasks.back());
        }
    }

    const bool onboarding_finished =
        claimed_onboarding_task &&
        !had_follow_up_task_template &&
        !onboarding_chain_effective(task_board_site_run(invocation).site_id, board);
    if (onboarding_finished)
    {
        (void)refresh_normal_task_pool(invocation, true);
    }
    else
    {
        mark_task_projection_dirty(invocation);
    }

}

void handle_restoration_progress(
    RuntimeInvocation& invocation,
    const RestorationProgressChangedMessage& payload)
{
    auto& board = task_board_world(invocation).own_task_board();
    board.fully_grown_tile_count = payload.fully_grown_tile_count;
    board.site_completion_tile_threshold = payload.site_completion_tile_threshold;
    bool mutated = false;

    for (auto& task : board.visible_tasks)
    {
        if (task.runtime_list_kind == TaskRuntimeListKind::PendingClaim ||
            task.runtime_list_kind == TaskRuntimeListKind::Claimed)
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

        if (task.runtime_list_kind == TaskRuntimeListKind::Accepted &&
            clamped_progress >= target &&
            complete_task(invocation, board, task))
        {
            mutated = true;
        }
    }

    if (mutated)
    {
        mark_task_projection_dirty(invocation);
    }
}

void handle_tile_ecology_changed(
    RuntimeInvocation& invocation,
    const TileEcologyChangedMessage& payload)
{
    if ((payload.changed_mask &
            (TILE_ECOLOGY_CHANGED_OCCUPANCY | TILE_ECOLOGY_CHANGED_DENSITY)) == 0U)
    {
        return;
    }

    auto& board = task_board_world(invocation).own_task_board();
    apply_tracked_plant_state(
        board,
        TileCoord {payload.target_tile_x, payload.target_tile_y},
        payload.plant_type_id,
        payload.ground_cover_type_id,
        payload.plant_density);
    const bool snapshot_changed = refresh_snapshot_tasks_if(invocation, [](const TaskTemplateDef& task_template_def) {
        return task_template_def.progress_kind == TaskProgressKind::PlantCountAtDensity ||
            task_template_def.progress_kind == TaskProgressKind::AnyPlantDensityAtLeast ||
            task_template_def.progress_kind == TaskProgressKind::NearbyPopulatedPlantTilesAtLeast;
    });
    if (snapshot_changed)
    {
        mark_task_projection_dirty(invocation);
    }
}

void handle_tile_ecology_batch_changed(
    RuntimeInvocation& invocation,
    const TileEcologyBatchChangedMessage& payload)
{
    auto& board = task_board_world(invocation).own_task_board();
    bool any_relevant_entry = false;
    for (std::size_t index = 0U;
         index < payload.entry_count && index < k_tile_ecology_batch_entry_count;
         ++index)
    {
        const auto& entry = payload.entries[index];
        if ((entry.changed_mask &
                (TILE_ECOLOGY_CHANGED_OCCUPANCY | TILE_ECOLOGY_CHANGED_DENSITY)) == 0U)
        {
            continue;
        }

        any_relevant_entry = true;
        apply_tracked_plant_state(
            board,
            TileCoord {
                static_cast<std::int32_t>(entry.target_tile_x),
                static_cast<std::int32_t>(entry.target_tile_y)},
            entry.plant_type_id,
            entry.ground_cover_type_id,
            density_from_centi(entry.plant_density_centi));
    }

    if (!any_relevant_entry)
    {
        return;
    }

    const bool snapshot_changed = refresh_snapshot_tasks_if(invocation, [](const TaskTemplateDef& task_template_def) {
        return task_template_def.progress_kind == TaskProgressKind::PlantCountAtDensity ||
            task_template_def.progress_kind == TaskProgressKind::AnyPlantDensityAtLeast ||
            task_template_def.progress_kind == TaskProgressKind::NearbyPopulatedPlantTilesAtLeast;
    });
    if (snapshot_changed)
    {
        mark_task_projection_dirty(invocation);
    }
}

void handle_site_tile_state_changed(
    RuntimeInvocation& invocation,
    const SiteTileStateChangedMessage& payload)
{
    auto& board = task_board_world(invocation).own_task_board();
    apply_tracked_plant_state(
        board,
        TileCoord {payload.target_tile_x, payload.target_tile_y},
        payload.plant_type_id,
        0U,
        payload.plant_density);
    TaskTrackedTileState* tracked = tracked_tile_state(
        board,
        TileCoord {payload.target_tile_x, payload.target_tile_y});
    if (tracked != nullptr)
    {
        tracked->moisture = payload.moisture;
        tracked->heat = payload.heat;
        tracked->dust = payload.dust;
        tracked->wind_protection = payload.wind_protection;
        tracked->heat_protection = payload.heat_protection;
        tracked->dust_protection = payload.dust_protection;
    }
    const bool snapshot_changed = refresh_snapshot_tasks_if(invocation, [](const TaskTemplateDef& task_template_def) {
        return task_template_def.progress_kind == TaskProgressKind::PlantWindProtectionAtLeast ||
            task_template_def.progress_kind == TaskProgressKind::PlantHeatProtectionAtLeast ||
            task_template_def.progress_kind == TaskProgressKind::PlantDustProtectionAtLeast;
    });
    const bool duration_changed = refresh_duration_task_conditions_if(invocation, [](const TaskTemplateDef& task_template_def) {
        return task_template_def.progress_kind == TaskProgressKind::KeepTileHeatAtMostForDuration ||
            task_template_def.progress_kind == TaskProgressKind::KeepTileDustAtMostForDuration;
    });
    if (snapshot_changed || duration_changed)
    {
        mark_task_projection_dirty(invocation);
    }
}

void handle_worker_meters_changed(
    RuntimeInvocation& invocation,
    const WorkerMetersChangedMessage& payload)
{
    auto& board = task_board_world(invocation).own_task_board();
    board.worker.health = payload.player_health;
    board.worker.hydration = payload.player_hydration;
    board.worker.nourishment = payload.player_nourishment;
    board.worker.energy = payload.player_energy;
    board.worker.morale = payload.player_morale;
    board.worker.valid = true;
    bool mutated = false;
    for (auto& task : board.visible_tasks)
    {
        if (task.runtime_list_kind != TaskRuntimeListKind::Accepted)
        {
            continue;
        }

        const auto* task_template_def = find_task_template_def(task.task_template_id);
        if (task_template_def == nullptr ||
            task_template_def->progress_kind != TaskProgressKind::KeepAllWorkerMetersAboveForDuration)
        {
            continue;
        }

        const std::uint8_t next_mask =
            (payload.player_health >= task.threshold_value &&
                payload.player_hydration >= task.threshold_value &&
                payload.player_nourishment >= task.threshold_value &&
                payload.player_energy >= task.threshold_value &&
                payload.player_morale >= task.threshold_value)
            ? 1U
            : 0U;
        if (task.requirement_mask != next_mask)
        {
            task.requirement_mask = next_mask;
            mutated = true;
        }
    }

    if (mutated)
    {
        mark_task_projection_dirty(invocation);
    }
}

void handle_site_device_condition_changed(
    RuntimeInvocation& invocation,
    const SiteDeviceConditionChangedMessage& payload)
{
    auto& board = task_board_world(invocation).own_task_board();
    TaskTrackedTileState* tracked = tracked_tile_state(
        board,
        TileCoord {payload.target_tile_x, payload.target_tile_y});
    if (tracked != nullptr)
    {
        tracked->device_structure_id = payload.structure_id;
        tracked->device_integrity = payload.device_integrity;
    }
    if (refresh_duration_task_conditions_if(invocation, [](const TaskTemplateDef& task_template_def) {
            return task_template_def.progress_kind == TaskProgressKind::KeepAllDevicesIntegrityAboveForDuration;
        }))
    {
        mark_task_projection_dirty(invocation);
    }
}

void handle_living_plant_stability_changed(
    RuntimeInvocation& invocation,
    const LivingPlantStabilityChangedMessage& payload)
{
    auto& board = task_board_world(invocation).own_task_board();
    board.tracked_living_plant_count = payload.tracked_living_plant_count;
    board.all_tracked_living_plants_stable =
        (payload.status_flags & LIVING_PLANT_STABILITY_ALL_TRACKED_PLANTS_STABLE) != 0U;
    bool mutated = false;
    for (auto& task : board.visible_tasks)
    {
        if (task.runtime_list_kind != TaskRuntimeListKind::Accepted)
        {
            continue;
        }

        const auto* task_template_def = find_task_template_def(task.task_template_id);
        if (task_template_def == nullptr ||
            task_template_def->progress_kind !=
                TaskProgressKind::KeepAllLivingPlantsNotWitheringForDuration)
        {
            continue;
        }

        const std::uint8_t next_mask =
            living_plant_duration_condition_met(task, payload) ? 1U : 0U;
        if (task.requirement_mask != next_mask)
        {
            task.requirement_mask = next_mask;
            mutated = true;
        }
    }

    if (mutated)
    {
        mark_task_projection_dirty(invocation);
    }
}

void handle_phone_listing_purchased(
    RuntimeInvocation& invocation,
    const PhoneListingPurchasedMessage& payload)
{
    advance_matching_accepted_tasks(
        invocation,
        payload.quantity == 0U ? 1U : payload.quantity,
        [&](const TaskTemplateDef& task_template_def, const TaskInstanceState& task) {
            return task_template_def.progress_kind == TaskProgressKind::BuyItem &&
                item_matches_task(task, ItemId {payload.item_id});
        });
}

void handle_phone_listing_sold(
    RuntimeInvocation& invocation,
    const PhoneListingSoldMessage& payload)
{
    const auto quantity = payload.quantity == 0U ? 1U : payload.quantity;
    advance_matching_accepted_tasks(
        invocation,
        quantity,
        [&](const TaskTemplateDef& task_template_def, const TaskInstanceState& task) {
            return task_template_def.progress_kind == TaskProgressKind::SellItem &&
                item_matches_task(task, ItemId {payload.item_id});
        });

    if (item_is_crafted(ItemId {payload.item_id}))
    {
        advance_matching_accepted_tasks(
            invocation,
            quantity,
            [&](const TaskTemplateDef& task_template_def, const TaskInstanceState& task) {
                (void)task;
                return task_template_def.progress_kind == TaskProgressKind::SellCraftedItem;
            });
    }

    const auto sell_price_cash_points = item_sell_price_cash_points(ItemId {payload.item_id});
    if (sell_price_cash_points > 0U)
    {
        advance_matching_accepted_tasks(
            invocation,
            sell_price_cash_points * quantity,
            [&](const TaskTemplateDef& task_template_def, const TaskInstanceState& task) {
                (void)task;
                return task_template_def.progress_kind == TaskProgressKind::EarnMoney;
            });
    }
}

void handle_inventory_transfer_completed(
    RuntimeInvocation& invocation,
    const InventoryTransferCompletedMessage& payload)
{
    advance_matching_accepted_tasks(
        invocation,
        payload.quantity == 0U ? 1U : payload.quantity,
        [&](const TaskTemplateDef& task_template_def, const TaskInstanceState& task) {
            return task_template_def.progress_kind == TaskProgressKind::TransferItem &&
                item_matches_task(task, ItemId {payload.item_id});
        });
}

void handle_inventory_item_submitted(
    RuntimeInvocation& invocation,
    const InventoryItemSubmittedMessage& payload)
{
    advance_matching_accepted_tasks(
        invocation,
        payload.quantity == 0U ? 1U : payload.quantity,
        [&](const TaskTemplateDef& task_template_def, const TaskInstanceState& task) {
            return task_template_def.progress_kind == TaskProgressKind::SubmitItem &&
                item_matches_task(task, ItemId {payload.item_id});
        });
}

void handle_site_tile_planting_completed(
    RuntimeInvocation& invocation,
    const SiteTilePlantingCompletedMessage& payload)
{
    advance_matching_accepted_tasks(
        invocation,
        1U,
        [&](const TaskTemplateDef& task_template_def, const TaskInstanceState& task) {
            return task_template_def.progress_kind == TaskProgressKind::PlantItem &&
                plant_matches_task(task, PlantId {payload.plant_type_id});
        });

    advance_matching_accepted_tasks(
        invocation,
        1U,
        [&](const TaskTemplateDef& task_template_def, const TaskInstanceState& task) {
            (void)task;
            return task_template_def.progress_kind == TaskProgressKind::PlantAny;
        });
}

void handle_inventory_craft_completed(
    RuntimeInvocation& invocation,
    const InventoryCraftCompletedMessage& payload)
{
    const auto quantity = payload.output_quantity == 0U ? 1U : payload.output_quantity;
    advance_matching_accepted_tasks(
        invocation,
        quantity,
        [&](const TaskTemplateDef& task_template_def, const TaskInstanceState& task) {
            return task_template_def.progress_kind == TaskProgressKind::CraftRecipe &&
                (task.recipe_id.value == 0U ||
                    task.recipe_id.value == payload.recipe_id);
        });

    advance_matching_accepted_tasks(
        invocation,
        quantity,
        [&](const TaskTemplateDef& task_template_def, const TaskInstanceState& task) {
            return task_template_def.progress_kind == TaskProgressKind::CraftItem &&
                item_matches_task(task, ItemId {payload.output_item_id});
        });

    advance_matching_accepted_tasks(
        invocation,
        quantity,
        [&](const TaskTemplateDef& task_template_def, const TaskInstanceState& task) {
            (void)task;
            return task_template_def.progress_kind == TaskProgressKind::CraftAnyItem;
        });
}

void handle_inventory_item_use_completed(
    RuntimeInvocation& invocation,
    const InventoryItemUseCompletedMessage& payload)
{
    advance_matching_accepted_tasks(
        invocation,
        payload.quantity == 0U ? 1U : payload.quantity,
        [&](const TaskTemplateDef& task_template_def, const TaskInstanceState& task) {
            return task_template_def.progress_kind == TaskProgressKind::ConsumeItem &&
                item_matches_task(task, ItemId {payload.item_id});
        });
}

void handle_site_action_completed(
    RuntimeInvocation& invocation,
    const SiteActionCompletedMessage& payload)
{
    advance_matching_accepted_tasks(
        invocation,
        1U,
        [&](const TaskTemplateDef& task_template_def, const TaskInstanceState& task) {
            return task_template_def.progress_kind == TaskProgressKind::PerformAction &&
                action_matches_task(task, action_kind_from_gs1(payload.action_kind));
        });

    if (payload.action_kind != GS1_SITE_ACTION_DRINK &&
        payload.action_kind != GS1_SITE_ACTION_EAT)
    {
        return;
    }

    advance_matching_accepted_tasks(
        invocation,
        1U,
        [&](const TaskTemplateDef& task_template_def, const TaskInstanceState& task) {
            (void)task;
            return task_template_def.progress_kind == TaskProgressKind::ConsumeItem;
        });
}

void handle_site_device_placed(
    RuntimeInvocation& invocation,
    const SiteDevicePlacedMessage& payload)
{
    advance_matching_accepted_tasks(
        invocation,
        1U,
        [&](const TaskTemplateDef& task_template_def, const TaskInstanceState& task) {
            return task_template_def.progress_kind == TaskProgressKind::BuildStructure &&
                structure_matches_task(task, StructureId {payload.structure_id});
        });

    advance_matching_accepted_tasks(
        invocation,
        1U,
        [&](const TaskTemplateDef& task_template_def, const TaskInstanceState& task) {
            (void)task;
            return task_template_def.progress_kind == TaskProgressKind::BuildAnyStructure;
        });

    advance_build_structure_set_tasks(invocation, StructureId {payload.structure_id});
}

void handle_money_award_requested(
    RuntimeInvocation& invocation,
    const EconomyMoneyAwardRequestedMessage& payload)
{
    if (payload.delta <= 0)
    {
        return;
    }

    advance_matching_accepted_tasks(
        invocation,
        static_cast<std::uint32_t>(payload.delta),
        [&](const TaskTemplateDef& task_template_def, const TaskInstanceState& task) {
            (void)task;
            return task_template_def.progress_kind == TaskProgressKind::EarnMoney;
        });
}
}  // namespace

const char* TaskBoardSystem::name() const noexcept
{
    return "TaskBoardSystem";
}

GameMessageSubscriptionSpan TaskBoardSystem::subscribed_game_messages() const noexcept
{
    static constexpr GameMessageType subscriptions[] = {
        GameMessageType::SiteRunStarted,
        GameMessageType::SiteRefreshTick,
        GameMessageType::TaskAcceptRequested,
        GameMessageType::TaskRewardClaimRequested,
        GameMessageType::RestorationProgressChanged,
        GameMessageType::TileEcologyChanged,
        GameMessageType::TileEcologyBatchChanged,
        GameMessageType::LivingPlantStabilityChanged,
        GameMessageType::SiteTileStateChanged,
        GameMessageType::WorkerMetersChanged,
        GameMessageType::PhoneListingPurchased,
        GameMessageType::PhoneListingSold,
        GameMessageType::InventoryTransferCompleted,
        GameMessageType::InventoryItemSubmitted,
        GameMessageType::InventoryItemUseCompleted,
        GameMessageType::InventoryCraftCompleted,
        GameMessageType::SiteTilePlantingCompleted,
        GameMessageType::SiteActionCompleted,
        GameMessageType::SiteDevicePlaced,
        GameMessageType::SiteDeviceConditionChanged,
        GameMessageType::EconomyMoneyAwardRequested,
    };
    return subscriptions;
}

HostMessageSubscriptionSpan TaskBoardSystem::subscribed_host_messages() const noexcept
{
    static constexpr Gs1HostMessageType subscriptions[] = {GS1_HOST_EVENT_GAMEPLAY_ACTION};
    return subscriptions;
}

std::optional<Gs1RuntimeProfileSystemId> TaskBoardSystem::profile_system_id() const noexcept
{
    return GS1_RUNTIME_PROFILE_SYSTEM_TASK_BOARD;
}

std::optional<std::uint32_t> TaskBoardSystem::fixed_step_order() const noexcept
{
    return 16U;
}

Gs1Status TaskBoardSystem::process_game_message(
    RuntimeInvocation& invocation,
    const GameMessage& message)
{
    auto access = make_game_state_access<TaskBoardSystem>(invocation);
    auto& campaign = access.template read<RuntimeCampaignTag>();
    auto& site_run = access.template read<RuntimeActiveSiteRunTag>();
    const double fixed_step_seconds = access.template read<RuntimeFixedStepSecondsTag>();
    if (!campaign.has_value() || !site_run.has_value())
    {
        return GS1_STATUS_INVALID_STATE;
    }
    switch (message.type)
    {
    case GameMessageType::SiteRunStarted:
        handle_site_run_started(invocation, message.payload_as<SiteRunStartedMessage>());
        break;
    case GameMessageType::SiteRefreshTick:
        if ((message.payload_as<SiteRefreshTickMessage>().refresh_mask &
                SITE_REFRESH_TICK_TASK_BOARD) != 0U &&
            !onboarding_chain_effective(
                task_board_site_run(invocation).site_id,
                task_board_world(invocation).read_task_board()))
        {
            (void)refresh_normal_task_pool(invocation, true);
        }
        break;
    case GameMessageType::TaskAcceptRequested:
        handle_task_accept_requested(invocation, message.payload_as<TaskAcceptRequestedMessage>());
        break;
    case GameMessageType::TaskRewardClaimRequested:
        handle_task_reward_claim_requested(
            invocation,
            message.payload_as<TaskRewardClaimRequestedMessage>());
        break;
    case GameMessageType::RestorationProgressChanged:
        handle_restoration_progress(invocation, message.payload_as<RestorationProgressChangedMessage>());
        break;
    case GameMessageType::TileEcologyChanged:
        handle_tile_ecology_changed(invocation, message.payload_as<TileEcologyChangedMessage>());
        break;
    case GameMessageType::TileEcologyBatchChanged:
        handle_tile_ecology_batch_changed(
            invocation,
            message.payload_as<TileEcologyBatchChangedMessage>());
        break;
    case GameMessageType::LivingPlantStabilityChanged:
        handle_living_plant_stability_changed(
            invocation,
            message.payload_as<LivingPlantStabilityChangedMessage>());
        break;
    case GameMessageType::SiteTileStateChanged:
        handle_site_tile_state_changed(invocation, message.payload_as<SiteTileStateChangedMessage>());
        break;
    case GameMessageType::WorkerMetersChanged:
        handle_worker_meters_changed(invocation, message.payload_as<WorkerMetersChangedMessage>());
        break;
    case GameMessageType::PhoneListingPurchased:
        handle_phone_listing_purchased(invocation, message.payload_as<PhoneListingPurchasedMessage>());
        break;
    case GameMessageType::PhoneListingSold:
        handle_phone_listing_sold(invocation, message.payload_as<PhoneListingSoldMessage>());
        break;
    case GameMessageType::InventoryTransferCompleted:
        handle_inventory_transfer_completed(
            invocation,
            message.payload_as<InventoryTransferCompletedMessage>());
        break;
    case GameMessageType::InventoryItemSubmitted:
        handle_inventory_item_submitted(
            invocation,
            message.payload_as<InventoryItemSubmittedMessage>());
        break;
    case GameMessageType::InventoryItemUseCompleted:
        handle_inventory_item_use_completed(
            invocation,
            message.payload_as<InventoryItemUseCompletedMessage>());
        break;
    case GameMessageType::InventoryCraftCompleted:
        handle_inventory_craft_completed(
            invocation,
            message.payload_as<InventoryCraftCompletedMessage>());
        break;
    case GameMessageType::SiteTilePlantingCompleted:
        handle_site_tile_planting_completed(
            invocation,
            message.payload_as<SiteTilePlantingCompletedMessage>());
        break;
    case GameMessageType::SiteActionCompleted:
        handle_site_action_completed(invocation, message.payload_as<SiteActionCompletedMessage>());
        break;
    case GameMessageType::SiteDevicePlaced:
        handle_site_device_placed(invocation, message.payload_as<SiteDevicePlacedMessage>());
        break;
    case GameMessageType::SiteDeviceConditionChanged:
        handle_site_device_condition_changed(
            invocation,
            message.payload_as<SiteDeviceConditionChangedMessage>());
        break;
    case GameMessageType::EconomyMoneyAwardRequested:
        handle_money_award_requested(
            invocation,
            message.payload_as<EconomyMoneyAwardRequestedMessage>());
        break;
    default:
        break;
    }

    return GS1_STATUS_OK;
}

Gs1Status TaskBoardSystem::process_host_message(
    RuntimeInvocation& invocation,
    const Gs1HostMessage& message)
{
    if (message.type != GS1_HOST_EVENT_GAMEPLAY_ACTION)
    {
        return GS1_STATUS_OK;
    }

    const auto& action = message.payload.gameplay_action.action;
    GameMessage gameplay_message {};
    switch (action.type)
    {
    case GS1_GAMEPLAY_ACTION_ACCEPT_TASK:
        if (action.target_id == 0U)
        {
            return GS1_STATUS_INVALID_ARGUMENT;
        }
        gameplay_message.type = GameMessageType::TaskAcceptRequested;
        gameplay_message.set_payload(TaskAcceptRequestedMessage {action.target_id});
        invocation.push_game_message(gameplay_message);
        return GS1_STATUS_OK;

    case GS1_GAMEPLAY_ACTION_CLAIM_TASK_REWARD:
        if (action.target_id == 0U ||
            action.arg0 > static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max()))
        {
            return GS1_STATUS_INVALID_ARGUMENT;
        }
        gameplay_message.type = GameMessageType::TaskRewardClaimRequested;
        gameplay_message.set_payload(TaskRewardClaimRequestedMessage {
            action.target_id,
            static_cast<std::uint32_t>(action.arg0)});
        invocation.push_game_message(gameplay_message);
        return GS1_STATUS_OK;

    default:
        return GS1_STATUS_OK;
    }
}

void TaskBoardSystem::run(RuntimeInvocation& invocation)
{
    auto access = make_game_state_access<TaskBoardSystem>(invocation);
    auto& campaign = access.template read<RuntimeCampaignTag>();
    auto& site_run = access.template read<RuntimeActiveSiteRunTag>();
    const double fixed_step_seconds = access.template read<RuntimeFixedStepSecondsTag>();
    if (!campaign.has_value() || !site_run.has_value())
    {
        return;
    }
    auto& board = task_board_world(invocation).own_task_board();
    if (onboarding_chain_effective(task_board_site_run(invocation).site_id, board))
    {
        board.minutes_until_next_refresh = 0.0;
    }
    else if (board.task_pool_size != normal_task_pool_size())
    {
        (void)refresh_normal_task_pool(invocation, true);
    }
    else if (board.minutes_until_next_refresh > 0.0)
    {
        board.minutes_until_next_refresh = std::max(
            0.0,
            board.minutes_until_next_refresh -
                runtime_minutes_from_real_seconds(fixed_step_seconds));
    }

    if (update_duration_tasks(invocation))
    {
        mark_task_projection_dirty(invocation);
    }
}
}  // namespace gs1

