#include "site/systems/action_execution_system.h"

#include "campaign/systems/technology_system.h"
#include "content/defs/craft_recipe_defs.h"
#include "content/defs/excavation_defs.h"
#include "content/defs/gameplay_tuning_defs.h"
#include "content/defs/item_defs.h"
#include "content/defs/plant_defs.h"
#include "content/defs/structure_defs.h"
#include "runtime/game_runtime.h"
#include "runtime/runtime_clock.h"
#include "site/device_interaction_logic.h"
#include "site/craft_logic.h"
#include "site/defs/site_action_defs.h"
#include "site/inventory_storage.h"
#include "site/placement_preview.h"
#include "site/site_run_state.h"
#include "support/id_types.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>

namespace gs1
{
template <>
struct system_state_tags<ActionExecutionSystem>
{
    using type = type_list<
        RuntimeCampaignFactionProgressTag,
        RuntimeFixedStepSecondsTag,
        RuntimeMoveDirectionTag>;
};

namespace
{
constexpr double k_minimum_action_duration_minutes = 0.25;
constexpr float k_repair_integrity_full = 1.0f;
constexpr float k_repair_integrity_epsilon = 0.0001f;
constexpr float k_harvest_density_epsilon_raw = 0.01f;
constexpr float k_minimum_action_efficiency = 0.4f;
constexpr float k_maximum_action_efficiency = 1.0f;
constexpr FactionId k_village_faction {k_faction_village_committee};

constexpr TechNodeId k_village_t12_access {base_technology_node_id(k_village_faction, 5U)};
constexpr TechNodeId k_village_t30_access {base_technology_node_id(k_village_faction, 20U)};

struct ExcavationTierPercents final
{
    float common {0.0f};
    float uncommon {0.0f};
    float rare {0.0f};
    float very_rare {0.0f};
    float jackpot {0.0f};
};

enum class HarvestBonusCategory : std::uint8_t
{
    None = 0,
    MainItem = 1,
    Seed = 2,
    Basic = 3
};

struct HarvestBonusRoll final
{
    HarvestBonusCategory category {HarvestBonusCategory::None};
    std::uint16_t quantity {0U};
};

void append_resolved_harvest_output(
    std::vector<ResolvedHarvestOutputStack>& resolved_outputs,
    ItemId item_id,
    std::uint16_t quantity) noexcept;

bool worker_pack_has_item(
    const ConstInventoryStateRef inventory,
    ItemId item_id) noexcept
{
    for (const auto& slot : inventory.worker_pack_slots)
    {
        if (slot.occupied && slot.item_id == item_id && slot.item_quantity > 0U)
        {
            return true;
        }
    }

    return false;
}

bool shovel_bonus_active(RuntimeInvocation& invocation) noexcept
{
    auto world = SiteWorldAccess<ActionExecutionSystem> {invocation};
    const auto& effects = world.read_modifier().resolved_village_technology_effects;
    return (effects.shovel_meter_cost_reduction > 0.0f ||
            effects.shovel_plant_duration_reduction > 0.0f ||
            effects.shovel_excavate_duration_reduction > 0.0f) &&
        worker_pack_has_item(world.read_inventory(), ItemId {k_item_shovel});
}

float shovel_meter_cost_reduction(RuntimeInvocation& invocation) noexcept
{
    if (!shovel_bonus_active(invocation))
    {
        return 0.0f;
    }

    auto world = SiteWorldAccess<ActionExecutionSystem> {invocation};
    return std::clamp(
        world.read_modifier().resolved_village_technology_effects.shovel_meter_cost_reduction,
        0.0f,
        0.95f);
}

float shovel_duration_reduction_for_action(
    RuntimeInvocation& invocation,
    ActionKind kind) noexcept
{
    if (!shovel_bonus_active(invocation) || (kind != ActionKind::Plant && kind != ActionKind::Excavate))
    {
        return 0.0f;
    }

    auto world = SiteWorldAccess<ActionExecutionSystem> {invocation};
    const auto& effects = world.read_modifier().resolved_village_technology_effects;
    const float reduction =
        kind == ActionKind::Plant
        ? effects.shovel_plant_duration_reduction
        : effects.shovel_excavate_duration_reduction;
    return std::clamp(reduction, 0.0f, 0.95f);
}

bool excavation_depth_matches_loot_rebalance(
    ConstModifierStateRef modifier_state,
    ExcavationDepth depth) noexcept
{
    const auto& effects = modifier_state.resolved_village_technology_effects;
    return (depth == ExcavationDepth::Careful && effects.careful_excavation_loot_rebalance) ||
        (depth == ExcavationDepth::Thorough && effects.thorough_excavation_loot_rebalance);
}

float excavation_meter_cost_reduction(
    RuntimeInvocation& invocation,
    ExcavationDepth depth) noexcept
{
    auto world = SiteWorldAccess<ActionExecutionSystem> {invocation};
    const auto& effects = world.read_modifier().resolved_village_technology_effects;
    if (depth == ExcavationDepth::Careful)
    {
        return effects.careful_excavation_meter_cost_reduction;
    }
    if (depth == ExcavationDepth::Thorough)
    {
        return effects.thorough_excavation_meter_cost_reduction;
    }
    return 0.0f;
}

float excavation_duration_reduction(
    RuntimeInvocation& invocation,
    ExcavationDepth depth) noexcept
{
    auto world = SiteWorldAccess<ActionExecutionSystem> {invocation};
    const auto& effects = world.read_modifier().resolved_village_technology_effects;
    if (depth == ExcavationDepth::Careful)
    {
        return effects.careful_excavation_duration_reduction;
    }
    if (depth == ExcavationDepth::Thorough)
    {
        return effects.thorough_excavation_duration_reduction;
    }
    return 0.0f;
}

ExcavationTierPercents depth_tier_percents(const ExcavationDepthDef& depth_def) noexcept
{
    return ExcavationTierPercents {
        depth_def.common_tier_percent,
        depth_def.uncommon_tier_percent,
        depth_def.rare_tier_percent,
        depth_def.very_rare_tier_percent,
        depth_def.jackpot_tier_percent};
}

ExcavationTierPercents blend_excavation_tier_percents(
    const ExcavationTierPercents& lhs,
    const ExcavationTierPercents& rhs,
    float rhs_weight) noexcept
{
    const float lhs_weight = 1.0f - rhs_weight;
    return ExcavationTierPercents {
        (lhs.common * lhs_weight) + (rhs.common * rhs_weight),
        (lhs.uncommon * lhs_weight) + (rhs.uncommon * rhs_weight),
        (lhs.rare * lhs_weight) + (rhs.rare * rhs_weight),
        (lhs.very_rare * lhs_weight) + (rhs.very_rare * rhs_weight),
        (lhs.jackpot * lhs_weight) + (rhs.jackpot * rhs_weight)};
}

std::uint64_t mix_excavation_seed(std::uint64_t value) noexcept
{
    value += 0x9e3779b97f4a7c15ULL;
    value = (value ^ (value >> 30U)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27U)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31U);
}

float percent_from_seed(std::uint64_t seed) noexcept
{
    return static_cast<float>(mix_excavation_seed(seed) % 10000ULL) * 0.01f;
}

std::uint64_t harvest_roll_seed(
    std::uint64_t site_attempt_seed,
    RuntimeActionId action_id,
    TileCoord target_tile,
    PlantId plant_id,
    std::size_t output_index,
    std::uint32_t salt = 0U) noexcept
{
    std::uint64_t seed = site_attempt_seed;
    seed ^= static_cast<std::uint64_t>(action_id.value) * 0x9e3779b97f4a7c15ULL;
    seed ^= static_cast<std::uint64_t>(plant_id.value) * 0xbf58476d1ce4e5b9ULL;
    seed ^= static_cast<std::uint64_t>(static_cast<std::uint32_t>(target_tile.x)) << 32U;
    seed ^= static_cast<std::uint64_t>(static_cast<std::uint32_t>(target_tile.y));
    seed ^= static_cast<std::uint64_t>(output_index + 1U) * 0x94d049bb133111ebULL;
    seed ^= static_cast<std::uint64_t>(salt + 1U) * 0xd6e8feb86659fd93ULL;
    return seed;
}

ExcavationDepth excavation_depth_from_subject_id(std::uint32_t subject_id) noexcept
{
    switch (subject_id)
    {
    case 1U:
        return ExcavationDepth::Rough;
    case 2U:
        return ExcavationDepth::Careful;
    case 3U:
        return ExcavationDepth::Thorough;
    default:
        return ExcavationDepth::None;
    }
}

std::uint32_t excavation_depth_subject_id(ExcavationDepth depth) noexcept
{
    return static_cast<std::uint32_t>(depth);
}

bool tile_has_excavation_blocking_occupier(const SiteWorld::TileData& tile) noexcept
{
    return tile.ecology.plant_id.value != 0U ||
        tile.ecology.ground_cover_type_id != 0U ||
        tile.device.structure_id.value != 0U;
}

ExcavationDepth max_excavation_depth_unlocked(
    RuntimeInvocation& invocation) noexcept
{
    auto access = make_game_state_access<ActionExecutionSystem>(invocation);
    const auto& faction_progress = access.template read<RuntimeCampaignFactionProgressTag>();
    if (TechnologySystem::node_purchased(
            std::span<const FactionProgressState> {faction_progress},
            k_village_t30_access))
    {
        return ExcavationDepth::Thorough;
    }

    if (TechnologySystem::node_purchased(
            std::span<const FactionProgressState> {faction_progress},
            k_village_t12_access))
    {
        return ExcavationDepth::Careful;
    }

    return ExcavationDepth::Rough;
}

ExcavationDepth resolve_excavation_depth_request(
    ExcavationDepth current_depth,
    ExcavationDepth requested_depth,
    ExcavationDepth max_unlocked_depth) noexcept
{
    const auto next_depth = next_excavation_depth(current_depth);
    if (current_depth == ExcavationDepth::Thorough ||
        static_cast<std::uint8_t>(next_depth) > static_cast<std::uint8_t>(max_unlocked_depth))
    {
        return ExcavationDepth::None;
    }

    if (requested_depth == ExcavationDepth::None)
    {
        return next_depth;
    }

    return requested_depth == next_depth &&
            static_cast<std::uint8_t>(requested_depth) <= static_cast<std::uint8_t>(max_unlocked_depth)
        ? requested_depth
        : ExcavationDepth::None;
}

ItemId repair_tool_item_id(std::uint32_t requested_item_id) noexcept
{
    return requested_item_id == 0U || requested_item_id == k_item_hammer
        ? ItemId {k_item_hammer}
        : ItemId {};
}

bool plant_def_is_harvestable(const PlantDef& plant_def) noexcept
{
    return plant_def.harvest_output_count > 0U &&
        plant_def.harvest_action_duration_minutes > 0.0f &&
        plant_def.harvest_density_required >= 0.0f &&
        plant_def.harvest_density_removed > 0.0f;
}

bool harvest_awards_item(const PlantDef& plant_def, float plant_density) noexcept
{
    return plant_density > (plant_def.harvest_density_required + k_harvest_density_epsilon_raw);
}

bool harvest_output_uses_authored_quantity(PlantHarvestOutputKind output_kind) noexcept
{
    return output_kind == PlantHarvestOutputKind::Basic ||
        output_kind == PlantHarvestOutputKind::Seed ||
        output_kind == PlantHarvestOutputKind::Bonus;
}

float resolve_harvest_output_chance_percent(
    const PlantHarvestOutputDef& harvest_output_def) noexcept
{
    switch (harvest_output_def.chance_mode)
    {
    case PlantHarvestOutputChanceMode::Guaranteed:
        return 100.0f;
    case PlantHarvestOutputChanceMode::FixedChance:
        return std::clamp(harvest_output_def.chance_percent, 0.0f, 100.0f);
    default:
        return 0.0f;
    }
}

std::uint16_t resolve_harvest_output_quantity(
    const PlantHarvestOutputDef& harvest_output_def) noexcept
{
    if (harvest_output_uses_authored_quantity(harvest_output_def.output_kind))
    {
        return harvest_output_def.quantity;
    }

    return 1U;
}

const PlantHarvestOutputDef* find_harvest_output_by_kind(
    const PlantDef& plant_def,
    PlantHarvestOutputKind output_kind) noexcept
{
    for (const auto& harvest_output_def : plant_harvest_output_defs(plant_def))
    {
        if (harvest_output_def.output_kind == output_kind)
        {
            return &harvest_output_def;
        }
    }

    return nullptr;
}

float harvest_bonus_proc_chance_percent(
    RuntimeInvocation& invocation) noexcept
{
    auto world = SiteWorldAccess<ActionExecutionSystem> {invocation};
    return std::clamp(
        world.read_modifier().resolved_bureau_technology_effects.harvest_bonus_proc_chance_percent,
        0.0f,
        100.0f);
}

HarvestBonusTier unlocked_harvest_bonus_tier(
    RuntimeInvocation& invocation) noexcept
{
    auto world = SiteWorldAccess<ActionExecutionSystem> {invocation};
    return world.read_modifier().resolved_bureau_technology_effects.unlocked_harvest_bonus_tier;
}

float harvest_bonus_higher_tier_bias_percent(
    RuntimeInvocation& invocation) noexcept
{
    auto world = SiteWorldAccess<ActionExecutionSystem> {invocation};
    return std::clamp(
        world.read_modifier()
            .resolved_bureau_technology_effects.harvest_bonus_higher_tier_bias_percent,
        0.0f,
        100.0f);
}

HarvestBonusRoll resolve_harvest_bonus_roll(
    RuntimeInvocation& invocation,
    RuntimeActionId action_id,
    TileCoord target_tile,
    const PlantDef& plant_def) noexcept
{
    auto world = SiteWorldAccess<ActionExecutionSystem> {invocation};
    const auto& harvest_effects = world.read_modifier().resolved_bureau_technology_effects;
    const auto unlocked_tier = harvest_effects.unlocked_harvest_bonus_tier;
    if (unlocked_tier == HarvestBonusTier::None)
    {
        return {};
    }

    const float proc_chance = std::clamp(
        harvest_effects.harvest_bonus_proc_chance_percent,
        0.0f,
        100.0f);
    if (proc_chance <= 0.0f)
    {
        return {};
    }

    const float proc_roll = percent_from_seed(
        harvest_roll_seed(
            world.site_attempt_seed(),
            action_id,
            target_tile,
            plant_def.plant_id,
            900U,
            0U));
    if (proc_roll >= proc_chance)
    {
        return {};
    }

    const float bias_percent = std::clamp(
        harvest_effects.harvest_bonus_higher_tier_bias_percent,
        0.0f,
        100.0f);
    const std::uint8_t max_tier = static_cast<std::uint8_t>(unlocked_tier);
    std::uint8_t rolled_tier = 1U;
    if (max_tier >= 3U)
    {
        float tier_three_weight = 6.0f + bias_percent;
        float tier_two_weight = 22.0f + (bias_percent * 0.6f);
        float tier_one_weight = 72.0f - (bias_percent * 1.6f);
        tier_one_weight = std::max(10.0f, tier_one_weight);
        const float total_weight = tier_one_weight + tier_two_weight + tier_three_weight;
        const float tier_roll =
            percent_from_seed(
                harvest_roll_seed(
                    world.site_attempt_seed(),
                    action_id,
                    target_tile,
                    plant_def.plant_id,
                    901U,
                    0U)) *
            (total_weight / 100.0f);
        if (tier_roll >= tier_one_weight + tier_two_weight)
        {
            rolled_tier = 3U;
        }
        else if (tier_roll >= tier_one_weight)
        {
            rolled_tier = 2U;
        }
    }
    else if (max_tier >= 2U)
    {
        float tier_two_weight = 28.0f + bias_percent;
        float tier_one_weight = 72.0f - bias_percent;
        tier_one_weight = std::max(15.0f, tier_one_weight);
        const float total_weight = tier_one_weight + tier_two_weight;
        const float tier_roll =
            percent_from_seed(
                harvest_roll_seed(
                    world.site_attempt_seed(),
                    action_id,
                    target_tile,
                    plant_def.plant_id,
                    901U,
                    0U)) *
            (total_weight / 100.0f);
        if (tier_roll >= tier_one_weight)
        {
            rolled_tier = 2U;
        }
    }

    const float category_roll = percent_from_seed(
        harvest_roll_seed(
            world.site_attempt_seed(),
            action_id,
            target_tile,
            plant_def.plant_id,
            902U,
            0U));
    HarvestBonusCategory category = HarvestBonusCategory::MainItem;
    if (category_roll >= 66.0f)
    {
        category = HarvestBonusCategory::Basic;
    }
    else if (category_roll >= 33.0f)
    {
        category = HarvestBonusCategory::Seed;
    }

    const auto* dedicated_output = find_harvest_output_by_kind(plant_def, PlantHarvestOutputKind::Dedicated);
    const auto* basic_output = find_harvest_output_by_kind(plant_def, PlantHarvestOutputKind::Basic);
    const auto* seed_output = find_harvest_output_by_kind(plant_def, PlantHarvestOutputKind::Seed);
    if ((category == HarvestBonusCategory::MainItem && dedicated_output == nullptr) ||
        (category == HarvestBonusCategory::Seed && seed_output == nullptr) ||
        (category == HarvestBonusCategory::Basic && basic_output == nullptr))
    {
        category = dedicated_output != nullptr
            ? HarvestBonusCategory::MainItem
            : (seed_output != nullptr ? HarvestBonusCategory::Seed : HarvestBonusCategory::Basic);
    }

    std::uint16_t quantity = 1U;
    if (rolled_tier == 2U)
    {
        quantity = category == HarvestBonusCategory::MainItem ? 2U : 1U;
    }
    else if (rolled_tier >= 3U)
    {
        quantity = category == HarvestBonusCategory::MainItem ? 2U : 2U;
    }

    return HarvestBonusRoll {category, quantity};
}

void append_harvest_bonus_output(
    std::vector<ResolvedHarvestOutputStack>& resolved_outputs,
    const PlantDef& plant_def,
    const HarvestBonusRoll& bonus_roll) noexcept
{
    if (bonus_roll.category == HarvestBonusCategory::None || bonus_roll.quantity == 0U)
    {
        return;
    }

    const PlantHarvestOutputDef* harvest_output_def = nullptr;
    switch (bonus_roll.category)
    {
    case HarvestBonusCategory::MainItem:
        harvest_output_def = find_harvest_output_by_kind(plant_def, PlantHarvestOutputKind::Dedicated);
        break;
    case HarvestBonusCategory::Seed:
        harvest_output_def = find_harvest_output_by_kind(plant_def, PlantHarvestOutputKind::Seed);
        break;
    case HarvestBonusCategory::Basic:
        harvest_output_def = find_harvest_output_by_kind(plant_def, PlantHarvestOutputKind::Basic);
        break;
    case HarvestBonusCategory::None:
    default:
        break;
    }

    if (harvest_output_def == nullptr)
    {
        return;
    }

    append_resolved_harvest_output(
        resolved_outputs,
        harvest_output_def->item_id,
        bonus_roll.quantity);
}

void append_resolved_harvest_output(
    std::vector<ResolvedHarvestOutputStack>& resolved_outputs,
    ItemId item_id,
    std::uint16_t quantity) noexcept
{
    if (quantity == 0U)
    {
        return;
    }

    const auto it = std::find_if(
        resolved_outputs.begin(),
        resolved_outputs.end(),
        [&](const ResolvedHarvestOutputStack& resolved_output) {
            return resolved_output.item_id == item_id.value;
        });
    if (it == resolved_outputs.end())
    {
        resolved_outputs.push_back(ResolvedHarvestOutputStack {
            item_id.value,
            quantity,
            {0U, 0U}});
        return;
    }

    const std::uint32_t merged_quantity =
        static_cast<std::uint32_t>(it->quantity) + static_cast<std::uint32_t>(quantity);
    it->quantity = static_cast<std::uint16_t>(std::min<std::uint32_t>(merged_quantity, 65535U));
}

std::uint32_t harvest_summary_item_id(
    const std::vector<ResolvedHarvestOutputStack>& resolved_outputs,
    const PlantDef& plant_def) noexcept
{
    for (const auto& harvest_output_def : plant_harvest_output_defs(plant_def))
    {
        if (harvest_output_def.output_kind != PlantHarvestOutputKind::Dedicated)
        {
            continue;
        }

        const auto it = std::find_if(
            resolved_outputs.begin(),
            resolved_outputs.end(),
            [&](const ResolvedHarvestOutputStack& resolved_output) {
                return resolved_output.item_id == harvest_output_def.item_id.value &&
                    resolved_output.quantity > 0U;
            });
        if (it != resolved_outputs.end())
        {
            return it->item_id;
        }
    }

    return resolved_outputs.empty() ? 0U : resolved_outputs.front().item_id;
}

std::uint16_t harvest_summary_item_quantity(
    const std::vector<ResolvedHarvestOutputStack>& resolved_outputs,
    std::uint32_t summary_item_id) noexcept
{
    if (summary_item_id == 0U)
    {
        return 0U;
    }

    std::uint32_t total_quantity = 0U;
    for (const auto& resolved_output : resolved_outputs)
    {
        if (resolved_output.item_id == summary_item_id)
        {
            total_quantity += resolved_output.quantity;
        }
    }

    return static_cast<std::uint16_t>(std::min<std::uint32_t>(total_quantity, 65535U));
}

std::vector<ResolvedHarvestOutputStack> resolve_harvest_outputs(
    RuntimeInvocation& invocation,
    RuntimeActionId action_id,
    TileCoord target_tile,
    const PlantDef& plant_def,
    float plant_density)
{
    auto world = SiteWorldAccess<ActionExecutionSystem> {invocation};
    std::vector<ResolvedHarvestOutputStack> resolved_outputs {};
    if (!harvest_awards_item(plant_def, plant_density))
    {
        return resolved_outputs;
    }

    const auto harvest_outputs = plant_harvest_output_defs(plant_def);
    resolved_outputs.reserve(harvest_outputs.size());
    for (std::size_t harvest_output_index = 0U;
         harvest_output_index < harvest_outputs.size();
         ++harvest_output_index)
    {
        const auto& harvest_output_def = harvest_outputs[harvest_output_index];
        const float chance_percent =
            resolve_harvest_output_chance_percent(harvest_output_def);
        if (chance_percent <= 0.0f)
        {
            continue;
        }

        const bool should_award =
            chance_percent >= 99.999f ||
            percent_from_seed(
                harvest_roll_seed(
                    world.site_attempt_seed(),
                    action_id,
                    target_tile,
                    plant_def.plant_id,
                    harvest_output_index,
                    0U)) < chance_percent;
        if (!should_award)
        {
            continue;
        }

        const std::uint16_t resolved_quantity =
            resolve_harvest_output_quantity(harvest_output_def);
        append_resolved_harvest_output(
            resolved_outputs,
            harvest_output_def.item_id,
            resolved_quantity);
    }

    append_harvest_bonus_output(
        resolved_outputs,
        plant_def,
        resolve_harvest_bonus_roll(invocation, action_id, target_tile, plant_def));

    return resolved_outputs;
}

bool inventory_can_fit_harvest_outputs(
    RuntimeInvocation& invocation,
    const std::vector<ResolvedHarvestOutputStack>& resolved_outputs) noexcept
{
    auto world = SiteWorldAccess<ActionExecutionSystem> {invocation};
    const auto worker_pack =
        inventory_storage::worker_pack_container(
            world.read_inventory(),
            world.ecs_world_ptr());
    if (!worker_pack.is_valid())
    {
        return resolved_outputs.empty();
    }

    for (const auto& resolved_output : resolved_outputs)
    {
        if (resolved_output.quantity == 0U)
        {
            continue;
        }

        if (!inventory_storage::can_fit_item_in_container(
                world.own_inventory(),
                world.ecs_world_ptr(),
                worker_pack,
                ItemId {resolved_output.item_id},
                resolved_output.quantity))
        {
            return false;
        }
    }

    return true;
}

ActionKind to_action_kind(Gs1SiteActionKind kind) noexcept
{
    switch (kind)
    {
    case GS1_SITE_ACTION_PLANT:
        return ActionKind::Plant;
    case GS1_SITE_ACTION_BUILD:
        return ActionKind::Build;
    case GS1_SITE_ACTION_REPAIR:
        return ActionKind::Repair;
    case GS1_SITE_ACTION_WATER:
        return ActionKind::Water;
    case GS1_SITE_ACTION_CLEAR_BURIAL:
        return ActionKind::ClearBurial;
    case GS1_SITE_ACTION_CRAFT:
        return ActionKind::Craft;
    case GS1_SITE_ACTION_DRINK:
        return ActionKind::Drink;
    case GS1_SITE_ACTION_EAT:
        return ActionKind::Eat;
    case GS1_SITE_ACTION_HARVEST:
        return ActionKind::Harvest;
    case GS1_SITE_ACTION_EXCAVATE:
        return ActionKind::Excavate;
    default:
        return ActionKind::None;
    }
}

Gs1SiteActionKind to_gs1_action_kind(ActionKind kind) noexcept
{
    switch (kind)
    {
    case ActionKind::Plant:
        return GS1_SITE_ACTION_PLANT;
    case ActionKind::Build:
        return GS1_SITE_ACTION_BUILD;
    case ActionKind::Repair:
        return GS1_SITE_ACTION_REPAIR;
    case ActionKind::Water:
        return GS1_SITE_ACTION_WATER;
    case ActionKind::ClearBurial:
        return GS1_SITE_ACTION_CLEAR_BURIAL;
    case ActionKind::Craft:
        return GS1_SITE_ACTION_CRAFT;
    case ActionKind::Drink:
        return GS1_SITE_ACTION_DRINK;
    case ActionKind::Eat:
        return GS1_SITE_ACTION_EAT;
    case ActionKind::Harvest:
        return GS1_SITE_ACTION_HARVEST;
    case ActionKind::Excavate:
        return GS1_SITE_ACTION_EXCAVATE;
    default:
        return GS1_SITE_ACTION_NONE;
    }
}

double base_duration_minutes(ActionKind kind) noexcept
{
    const auto* action_def = find_site_action_def(kind);
    return action_def == nullptr
        ? 1.0
        : std::max(
            static_cast<double>(action_def->duration_minutes_per_unit),
            k_minimum_action_duration_minutes);
}

float action_energy_cost(
    ActionKind kind,
    std::uint16_t quantity,
    std::uint32_t primary_subject_id = 0U,
    const CraftRecipeDef* craft_recipe = nullptr) noexcept
{
    const float scale = static_cast<float>(quantity == 0U ? 1U : quantity);
    if (kind == ActionKind::Craft && craft_recipe != nullptr)
    {
        return craft_recipe->energy_cost * scale;
    }

    const auto* action_def = find_site_action_def(kind);
    if (action_def == nullptr)
    {
        return 0.0f;
    }

    if (kind == ActionKind::Excavate)
    {
        const auto depth = excavation_depth_from_subject_id(primary_subject_id);
        const auto* depth_def = find_excavation_depth_def(depth);
        const float multiplier =
            depth_def == nullptr ? 1.0f : depth_def->energy_cost_multiplier;
        return action_def->energy_cost_per_unit * multiplier * scale;
    }

    return action_def->energy_cost_per_unit * scale;
}

float action_hydration_cost(
    ActionKind kind,
    std::uint16_t quantity,
    const CraftRecipeDef* craft_recipe = nullptr) noexcept
{
    const float scale = static_cast<float>(quantity == 0U ? 1U : quantity);
    if (kind == ActionKind::Craft && craft_recipe != nullptr)
    {
        return craft_recipe->hydration_cost * scale;
    }

    const auto* action_def = find_site_action_def(kind);
    return action_def == nullptr ? 0.0f : action_def->hydration_cost_per_unit * scale;
}

float action_nourishment_cost(
    ActionKind kind,
    std::uint16_t quantity,
    const CraftRecipeDef* craft_recipe = nullptr) noexcept
{
    const float scale = static_cast<float>(quantity == 0U ? 1U : quantity);
    if (kind == ActionKind::Craft && craft_recipe != nullptr)
    {
        return craft_recipe->nourishment_cost * scale;
    }

    const auto* action_def = find_site_action_def(kind);
    return action_def == nullptr ? 0.0f : action_def->nourishment_cost_per_unit * scale;
}

float action_morale_cost(
    ActionKind kind,
    std::uint16_t quantity,
    const CraftRecipeDef* craft_recipe = nullptr) noexcept
{
    const float scale = static_cast<float>(quantity == 0U ? 1U : quantity);
    if (kind == ActionKind::Craft && craft_recipe != nullptr)
    {
        (void)craft_recipe;
    }

    const auto* action_def = find_site_action_def(kind);
    return action_def == nullptr ? 0.0f : action_def->morale_cost_per_unit * scale;
}

double action_unit_duration_minutes(
    RuntimeInvocation& invocation,
    ActionKind kind,
    std::uint32_t primary_subject_id,
    std::uint32_t item_id,
    const CraftRecipeDef* craft_recipe = nullptr) noexcept
{
    if (kind == ActionKind::Craft && craft_recipe != nullptr)
    {
        return std::max(
            static_cast<double>(craft_recipe->craft_minutes),
            k_minimum_action_duration_minutes);
    }

    if ((kind == ActionKind::Plant || kind == ActionKind::Harvest) && item_id != 0U)
    {
        const auto* item_def = find_item_def(ItemId {item_id});
        if (item_def != nullptr && item_def->linked_plant_id.value != 0U)
        {
            const auto* plant_def = find_plant_def(item_def->linked_plant_id);
            if (plant_def != nullptr)
            {
                const double authored_duration =
                    kind == ActionKind::Harvest
                    ? static_cast<double>(plant_def->harvest_action_duration_minutes)
                    : static_cast<double>(plant_def->plant_action_duration_minutes);
                const double shovel_scale =
                    1.0 - static_cast<double>(shovel_duration_reduction_for_action(invocation, kind));
                return std::max(
                    authored_duration * shovel_scale,
                    k_minimum_action_duration_minutes);
            }
        }
    }

    if (kind == ActionKind::Excavate)
    {
        const auto depth = excavation_depth_from_subject_id(primary_subject_id);
        const auto* depth_def = find_excavation_depth_def(depth);
        if (depth_def != nullptr)
        {
            const double shovel_scale =
                1.0 - static_cast<double>(shovel_duration_reduction_for_action(invocation, kind));
            const double excavation_scale =
                1.0 - static_cast<double>(excavation_duration_reduction(invocation, depth));
            return std::max(
                static_cast<double>(depth_def->duration_minutes) * shovel_scale * excavation_scale,
                k_minimum_action_duration_minutes);
        }
    }

    return base_duration_minutes(kind);
}

float resolve_action_cost_multiplier(const SiteWorld::WorkerConditionData& worker) noexcept
{
    const float clamped_efficiency =
        std::clamp(worker.work_efficiency, k_minimum_action_efficiency, k_maximum_action_efficiency);
    return 2.0f - clamped_efficiency;
}

float resolve_weather_scaled_action_cost(
    float base_cost,
    const SiteWorld::TileLocalWeatherData& local_weather,
    float heat_to_cost,
    float wind_to_cost,
    float dust_to_cost) noexcept
{
    if (base_cost == 0.0f)
    {
        return 0.0f;
    }

    const float weather_multiplier =
        1.0f +
        std::max(local_weather.heat, 0.0f) * heat_to_cost +
        std::max(local_weather.wind, 0.0f) * wind_to_cost +
        std::max(local_weather.dust, 0.0f) * dust_to_cost;
    return base_cost * weather_multiplier;
}

float resolve_weather_scaled_morale_cost(
    float base_cost,
    const SiteWorld::TileLocalWeatherData& local_weather,
    float heat_to_cost,
    float wind_to_cost,
    float dust_to_cost) noexcept
{
    if (base_cost == 0.0f)
    {
        return 0.0f;
    }

    const float weather_fraction = std::max(
        std::max(local_weather.heat, 0.0f) * heat_to_cost,
        std::max(
            std::max(local_weather.wind, 0.0f) * wind_to_cost,
            std::max(local_weather.dust, 0.0f) * dust_to_cost));
    return base_cost * weather_fraction;
}

float apply_action_cost_modifier(
    float base_cost,
    float weight_delta,
    float bias) noexcept
{
    return std::max(0.0f, (base_cost * (1.0f + weight_delta)) + bias);
}

double compute_duration_minutes(
    RuntimeInvocation& invocation,
    ActionKind kind,
    std::uint16_t quantity,
    std::uint32_t primary_subject_id,
    std::uint32_t item_id,
    const CraftRecipeDef* craft_recipe = nullptr) noexcept
{
    auto world = SiteWorldAccess<ActionExecutionSystem> {invocation};
    const std::uint16_t safe_quantity = quantity == 0U ? 1U : quantity;
    const double duration =
        action_unit_duration_minutes(invocation, kind, primary_subject_id, item_id, craft_recipe) *
        static_cast<double>(safe_quantity);
    return std::max(
        k_minimum_action_duration_minutes,
        duration * static_cast<double>(resolve_action_cost_multiplier(world.read_worker().conditions)));
}

RuntimeActionId allocate_runtime_action_id() noexcept
{
    static std::uint32_t next_id = 1U;
    return RuntimeActionId{next_id++};
}

template <typename Payload>
void enqueue_message(GameMessageQueue& queue, GameMessageType type, const Payload& payload)
{
    GameMessage message {};
    message.type = type;
    message.set_payload(payload);
    queue.push_back(message);
}

void clear_action_state(ActionStateRef action_state) noexcept
{
    action_state.current_action_id = RuntimeActionId {};
    action_state.has_current_action_id = false;
    action_state.action_kind = ActionKind::None;
    action_state.target_tile = TileCoord {};
    action_state.has_target_tile = false;
    action_state.approach_tile = TileCoord {};
    action_state.has_approach_tile = false;
    action_state.primary_subject_id = 0U;
    action_state.secondary_subject_id = 0U;
    action_state.item_id = 0U;
    action_state.placement_reservation_token = 0U;
    action_state.quantity = 0U;
    action_state.request_flags = 0U;
    action_state.awaiting_placement_reservation = false;
    action_state.reactivate_placement_mode_on_completion = false;
    action_state.total_action_minutes = 0.0;
    action_state.remaining_action_minutes = 0.0;
    action_state.reserved_input_item_stacks.clear();
    action_state.resolved_harvest_outputs.clear();
    action_state.deferred_meter_delta = {};
    action_state.resolved_harvest_density = 0.0f;
    action_state.resolved_harvest_outputs_valid = false;
    action_state.started_at_world_minute = 0.0;
    action_state.has_started_at_world_minute = false;
    action_state.placement_mode = PlacementModeMetaState {};
}

void clear_placement_mode(PlacementModeMetaState& placement_mode) noexcept
{
    placement_mode.active = false;
    placement_mode.action_kind = ActionKind::None;
    placement_mode.target_tile = TileCoord {};
    placement_mode.has_target_tile = false;
    placement_mode.primary_subject_id = 0U;
    placement_mode.secondary_subject_id = 0U;
    placement_mode.item_id = 0U;
    placement_mode.quantity = 0U;
    placement_mode.request_flags = 0U;
    placement_mode.footprint_width = 1U;
    placement_mode.footprint_height = 1U;
    placement_mode.blocked_mask = 0ULL;
}

std::uint32_t action_id_value(const ConstActionStateRef action_state) noexcept
{
    if (action_state.has_current_action_id)
    {
        return action_state.current_action_id.value;
    }
    return 0U;
}

std::uint32_t action_id_value(const ActionStateRef action_state) noexcept
{
    return action_id_value(ConstActionStateRef {
        action_state.current_action_id,
        action_state.action_kind,
        action_state.target_tile,
        action_state.approach_tile,
        action_state.primary_subject_id,
        action_state.secondary_subject_id,
        action_state.item_id,
        action_state.placement_reservation_token,
        action_state.quantity,
        action_state.request_flags,
        action_state.awaiting_placement_reservation,
        action_state.reactivate_placement_mode_on_completion,
        action_state.total_action_minutes,
        action_state.remaining_action_minutes,
        action_state.deferred_meter_delta,
        action_state.resolved_harvest_density,
        action_state.resolved_harvest_outputs_valid,
        action_state.started_at_world_minute,
        action_state.placement_mode,
        action_state.has_current_action_id,
        action_state.has_target_tile,
        action_state.has_approach_tile,
        action_state.has_started_at_world_minute,
        action_state.reserved_input_item_stacks,
        action_state.resolved_harvest_outputs});
}

TileCoord action_target_tile(const ConstActionStateRef action_state) noexcept
{
    if (action_state.has_target_tile)
    {
        return action_state.target_tile;
    }
    return TileCoord {};
}

TileCoord action_target_tile(const ActionStateRef action_state) noexcept
{
    return action_target_tile(ConstActionStateRef {
        action_state.current_action_id,
        action_state.action_kind,
        action_state.target_tile,
        action_state.approach_tile,
        action_state.primary_subject_id,
        action_state.secondary_subject_id,
        action_state.item_id,
        action_state.placement_reservation_token,
        action_state.quantity,
        action_state.request_flags,
        action_state.awaiting_placement_reservation,
        action_state.reactivate_placement_mode_on_completion,
        action_state.total_action_minutes,
        action_state.remaining_action_minutes,
        action_state.deferred_meter_delta,
        action_state.resolved_harvest_density,
        action_state.resolved_harvest_outputs_valid,
        action_state.started_at_world_minute,
        action_state.placement_mode,
        action_state.has_current_action_id,
        action_state.has_target_tile,
        action_state.has_approach_tile,
        action_state.has_started_at_world_minute,
        action_state.reserved_input_item_stacks,
        action_state.resolved_harvest_outputs});
}

TileCoord action_approach_tile(const ConstActionStateRef action_state) noexcept
{
    if (action_state.has_approach_tile)
    {
        return action_state.approach_tile;
    }
    return action_target_tile(action_state);
}

[[nodiscard]] constexpr bool has_target_tile(const ConstActionStateRef action_state) noexcept
{
    return action_state.has_target_tile;
}

[[nodiscard]] constexpr bool has_approach_tile(const ConstActionStateRef action_state) noexcept
{
    return action_state.has_approach_tile;
}

[[nodiscard]] constexpr bool has_current_action_id(const ConstActionStateRef action_state) noexcept
{
    return action_state.has_current_action_id;
}

[[nodiscard]] constexpr bool has_started_at_world_minute(const ConstActionStateRef action_state) noexcept
{
    return action_state.has_started_at_world_minute;
}

bool should_request_placement_reservation(ActionKind action_kind) noexcept
{
    const auto* action_def = find_site_action_def(action_kind);
    return action_def != nullptr && action_def->requests_placement_reservation;
}

bool action_requires_worker_approach(ActionKind action_kind) noexcept
{
    const auto* action_def = find_site_action_def(action_kind);
    return action_def != nullptr && action_def->requires_worker_approach;
}

PlacementOccupancyLayer placement_occupancy_layer(ActionKind action_kind) noexcept
{
    const auto* action_def = find_site_action_def(action_kind);
    return action_def == nullptr ? PlacementOccupancyLayer::None : action_def->placement_occupancy_layer;
}

void emit_site_action_started(
    GameMessageQueue& queue,
    RuntimeActionId action_id,
    ActionKind action_kind,
    std::uint8_t flags,
    TileCoord target_tile,
    std::uint32_t primary_subject_id,
    float duration_minutes)
{
    enqueue_message(
        queue,
        GameMessageType::SiteActionStarted,
        SiteActionStartedMessage {
            action_id.value,
            to_gs1_action_kind(action_kind),
            flags,
            0U,
            target_tile.x,
            target_tile.y,
            primary_subject_id,
            duration_minutes});
}

void emit_site_action_completed(GameMessageQueue& queue, const ConstActionStateRef action_state)
{
    const TileCoord target_tile = action_target_tile(action_state);
    enqueue_message(
        queue,
        GameMessageType::SiteActionCompleted,
        SiteActionCompletedMessage {
            action_id_value(action_state),
            to_gs1_action_kind(action_state.action_kind),
            0U,
            0U,
            target_tile.x,
            target_tile.y,
        action_state.primary_subject_id,
        action_state.secondary_subject_id});
}

DeferredWorkerMeterDelta resolve_worker_meter_cost_delta(
    RuntimeInvocation& invocation,
    ActionKind action_kind,
    std::uint16_t quantity,
    std::uint32_t primary_subject_id = 0U,
    const CraftRecipeDef* craft_recipe = nullptr)
{
    auto world = SiteWorldAccess<ActionExecutionSystem> {invocation};
    const auto worker = world.read_worker();
    const auto local_weather = world.read_tile_local_weather(worker.position.tile_coord);
    const auto& action_cost_modifiers = world.read_modifier().resolved_action_cost_modifiers;
    const auto* action_def = find_site_action_def(action_kind);
    const float action_multiplier =
        resolve_action_cost_multiplier(world.read_worker().conditions);
    const float hydration_cost = apply_action_cost_modifier(
        action_def == nullptr
            ? action_hydration_cost(action_kind, quantity, craft_recipe)
            : resolve_weather_scaled_action_cost(
                action_hydration_cost(action_kind, quantity, craft_recipe),
                local_weather,
                action_def->heat_to_hydration_cost,
                action_def->wind_to_hydration_cost,
                action_def->dust_to_hydration_cost),
        action_cost_modifiers.hydration_weight_delta,
        action_cost_modifiers.hydration_bias);
    const float nourishment_cost = apply_action_cost_modifier(
        action_def == nullptr
            ? action_nourishment_cost(action_kind, quantity, craft_recipe)
            : resolve_weather_scaled_action_cost(
                action_nourishment_cost(action_kind, quantity, craft_recipe),
                local_weather,
                action_def->heat_to_nourishment_cost,
                action_def->wind_to_nourishment_cost,
                action_def->dust_to_nourishment_cost),
        action_cost_modifiers.nourishment_weight_delta,
        action_cost_modifiers.nourishment_bias);
    const float energy_cost = apply_action_cost_modifier(
        (action_def == nullptr
            ? action_energy_cost(action_kind, quantity, primary_subject_id, craft_recipe)
            : resolve_weather_scaled_action_cost(
                action_energy_cost(action_kind, quantity, primary_subject_id, craft_recipe),
                local_weather,
                action_def->heat_to_energy_cost,
                action_def->wind_to_energy_cost,
                action_def->dust_to_energy_cost)) * action_multiplier,
        action_cost_modifiers.energy_weight_delta,
        action_cost_modifiers.energy_bias);
    const float morale_cost = apply_action_cost_modifier(
        action_def == nullptr
            ? action_morale_cost(action_kind, quantity, craft_recipe)
            : resolve_weather_scaled_morale_cost(
                action_morale_cost(action_kind, quantity, craft_recipe),
                local_weather,
                action_def->heat_to_morale_cost,
                action_def->wind_to_morale_cost,
                action_def->dust_to_morale_cost),
        action_cost_modifiers.morale_weight_delta,
        action_cost_modifiers.morale_bias);
    const bool apply_shovel_cost_reduction =
        action_kind == ActionKind::Plant || action_kind == ActionKind::Excavate;
    const auto excavation_depth =
        action_kind == ActionKind::Excavate
        ? excavation_depth_from_subject_id(primary_subject_id)
        : ExcavationDepth::None;
    const float shovel_cost_multiplier =
        apply_shovel_cost_reduction
        ? (1.0f - shovel_meter_cost_reduction(invocation))
        : 1.0f;
    const float excavation_cost_multiplier =
        action_kind == ActionKind::Excavate
        ? (1.0f - excavation_meter_cost_reduction(invocation, excavation_depth))
        : 1.0f;
    const float combined_cost_multiplier =
        std::clamp(shovel_cost_multiplier * excavation_cost_multiplier, 0.0f, 1.0f);
    const float adjusted_hydration_cost = hydration_cost * combined_cost_multiplier;
    const float adjusted_nourishment_cost = nourishment_cost * combined_cost_multiplier;
    const float adjusted_energy_cost = energy_cost * combined_cost_multiplier;
    const float adjusted_morale_cost =
        action_kind == ActionKind::Excavate
        ? morale_cost * excavation_cost_multiplier
        : morale_cost;
    if (adjusted_hydration_cost == 0.0f &&
        adjusted_nourishment_cost == 0.0f &&
        adjusted_energy_cost == 0.0f &&
        adjusted_morale_cost == 0.0f)
    {
        return {};
    }

    DeferredWorkerMeterDelta meter_delta {};
    if (adjusted_hydration_cost != 0.0f)
    {
        meter_delta.flags |= WORKER_METER_CHANGED_HYDRATION;
    }
    if (adjusted_nourishment_cost != 0.0f)
    {
        meter_delta.flags |= WORKER_METER_CHANGED_NOURISHMENT;
    }
    if (adjusted_energy_cost != 0.0f)
    {
        meter_delta.flags |= WORKER_METER_CHANGED_ENERGY;
    }
    if (adjusted_morale_cost != 0.0f)
    {
        meter_delta.flags |= WORKER_METER_CHANGED_MORALE;
    }

    meter_delta.hydration_delta = -adjusted_hydration_cost;
    meter_delta.nourishment_delta = -adjusted_nourishment_cost;
    meter_delta.energy_delta = -adjusted_energy_cost;
    meter_delta.morale_delta = -adjusted_morale_cost;
    return meter_delta;
}

void emit_deferred_worker_meter_cost_request(
    GameMessageQueue& queue,
    const ConstActionStateRef action_state)
{
    if (!action_state.has_current_action_id ||
        action_state.deferred_meter_delta.flags == 0U)
    {
        return;
    }

    enqueue_message(
        queue,
        GameMessageType::WorkerMeterDeltaRequested,
        WorkerMeterDeltaRequestedMessage {
            action_state.current_action_id.value,
            action_state.deferred_meter_delta.flags,
            action_state.deferred_meter_delta.health_delta,
            action_state.deferred_meter_delta.hydration_delta,
            action_state.deferred_meter_delta.nourishment_delta,
            action_state.deferred_meter_delta.energy_cap_delta,
            action_state.deferred_meter_delta.energy_delta,
            action_state.deferred_meter_delta.morale_delta,
            action_state.deferred_meter_delta.work_efficiency_delta});
}

void emit_site_action_failed(
    GameMessageQueue& queue,
    std::uint32_t action_id,
    ActionKind action_kind,
    SiteActionFailureReason reason,
    std::uint16_t flags,
    TileCoord target_tile,
    std::uint32_t primary_subject_id,
    std::uint32_t secondary_subject_id)
{
    enqueue_message(
        queue,
        GameMessageType::SiteActionFailed,
        SiteActionFailedMessage {
            action_id,
            to_gs1_action_kind(action_kind),
            reason,
            flags,
            target_tile.x,
            target_tile.y,
            primary_subject_id,
            secondary_subject_id});
}

bool has_active_action(const ConstActionStateRef action_state) noexcept
{
    return action_state.has_current_action_id &&
        action_state.action_kind != ActionKind::None;
}

bool has_active_action(const ActionStateRef action_state) noexcept
{
    return has_active_action(ConstActionStateRef {
        action_state.current_action_id,
        action_state.action_kind,
        action_state.target_tile,
        action_state.approach_tile,
        action_state.primary_subject_id,
        action_state.secondary_subject_id,
        action_state.item_id,
        action_state.placement_reservation_token,
        action_state.quantity,
        action_state.request_flags,
        action_state.awaiting_placement_reservation,
        action_state.reactivate_placement_mode_on_completion,
        action_state.total_action_minutes,
        action_state.remaining_action_minutes,
        action_state.deferred_meter_delta,
        action_state.resolved_harvest_density,
        action_state.resolved_harvest_outputs_valid,
        action_state.started_at_world_minute,
        action_state.placement_mode,
        action_state.has_current_action_id,
        action_state.has_target_tile,
        action_state.has_approach_tile,
        action_state.has_started_at_world_minute,
        action_state.reserved_input_item_stacks,
        action_state.resolved_harvest_outputs});
}

bool has_active_placement_mode(const ConstActionStateRef action_state) noexcept
{
    return action_state.placement_mode.active &&
        action_state.placement_mode.action_kind != ActionKind::None;
}

bool has_active_placement_mode(const ActionStateRef action_state) noexcept
{
    return has_active_placement_mode(ConstActionStateRef {
        action_state.current_action_id,
        action_state.action_kind,
        action_state.target_tile,
        action_state.approach_tile,
        action_state.primary_subject_id,
        action_state.secondary_subject_id,
        action_state.item_id,
        action_state.placement_reservation_token,
        action_state.quantity,
        action_state.request_flags,
        action_state.awaiting_placement_reservation,
        action_state.reactivate_placement_mode_on_completion,
        action_state.total_action_minutes,
        action_state.remaining_action_minutes,
        action_state.deferred_meter_delta,
        action_state.resolved_harvest_density,
        action_state.resolved_harvest_outputs_valid,
        action_state.started_at_world_minute,
        action_state.placement_mode,
        action_state.has_current_action_id,
        action_state.has_target_tile,
        action_state.has_approach_tile,
        action_state.has_started_at_world_minute,
        action_state.reserved_input_item_stacks,
        action_state.resolved_harvest_outputs});
}

bool action_supports_deferred_target_selection(ActionKind action_kind) noexcept
{
    return action_kind == ActionKind::Plant || action_kind == ActionKind::Build;
}

TileFootprint resolve_action_placement_footprint(
    ActionKind action_kind,
    std::uint32_t item_id,
    std::uint32_t primary_subject_id) noexcept;

std::uint64_t full_blocked_mask_for_footprint(TileFootprint footprint) noexcept
{
    const std::uint32_t cell_count =
        static_cast<std::uint32_t>(footprint.width) *
        static_cast<std::uint32_t>(footprint.height);
    if (cell_count == 0U)
    {
        return 0ULL;
    }

    if (cell_count >= 64U)
    {
        return ~0ULL;
    }

    return (1ULL << cell_count) - 1ULL;
}

std::uint32_t available_worker_pack_item_quantity(
    ConstInventoryStateRef inventory,
    ItemId item_id) noexcept
{
    std::uint32_t total_quantity = 0U;
    for (const auto& slot : inventory.worker_pack_slots)
    {
        if (!slot.occupied || slot.item_id != item_id)
        {
            continue;
        }

        total_quantity += slot.item_quantity;
    }

    return total_quantity;
}

std::uint32_t available_worker_pack_item_quantity(
    InventoryStateRef inventory,
    ItemId item_id) noexcept
{
    std::uint32_t total_quantity = 0U;
    for (const auto& slot : inventory.worker_pack_slots)
    {
        if (slot.occupied && slot.item_id == item_id && slot.item_quantity > 0U)
        {
            total_quantity += slot.item_quantity;
        }
    }

    return total_quantity;
}

std::uint32_t reserved_worker_pack_item_quantity(
    ConstActionStateRef action_state,
    ItemId item_id) noexcept
{
    std::uint32_t total_quantity = 0U;
    for (const auto& reserved_stack : action_state.reserved_input_item_stacks)
    {
        if (reserved_stack.container_kind != GS1_INVENTORY_CONTAINER_WORKER_PACK ||
            reserved_stack.item_id != item_id)
        {
            continue;
        }

        total_quantity += reserved_stack.quantity;
    }

    return total_quantity;
}

std::uint32_t reserved_worker_pack_item_quantity(
    ActionStateRef action_state,
    ItemId item_id) noexcept
{
    return reserved_worker_pack_item_quantity(
        ConstActionStateRef {
            action_state.current_action_id,
            action_state.action_kind,
            action_state.target_tile,
            action_state.approach_tile,
            action_state.primary_subject_id,
            action_state.secondary_subject_id,
            action_state.item_id,
            action_state.placement_reservation_token,
            action_state.quantity,
            action_state.request_flags,
            action_state.awaiting_placement_reservation,
            action_state.reactivate_placement_mode_on_completion,
            action_state.total_action_minutes,
            action_state.remaining_action_minutes,
            action_state.deferred_meter_delta,
            action_state.resolved_harvest_density,
            action_state.resolved_harvest_outputs_valid,
            action_state.started_at_world_minute,
            action_state.placement_mode,
            action_state.has_current_action_id,
            action_state.has_target_tile,
            action_state.has_approach_tile,
            action_state.has_started_at_world_minute,
            action_state.reserved_input_item_stacks,
            action_state.resolved_harvest_outputs},
        item_id);
}

const ItemDef* action_item_def(
    ActionKind action_kind,
    std::uint32_t item_id) noexcept
{
    if (item_id == 0U)
    {
        return nullptr;
    }

    const auto* item_def = find_item_def(ItemId {item_id});
    if (item_def == nullptr)
    {
        return nullptr;
    }

    switch (action_kind)
    {
    case ActionKind::Plant:
        return item_has_capability(*item_def, ITEM_CAPABILITY_PLANT) &&
                item_def->linked_plant_id.value != 0U
            ? item_def
            : nullptr;

    case ActionKind::Build:
        return item_has_capability(*item_def, ITEM_CAPABILITY_DEPLOY) &&
                item_def->linked_structure_id.value != 0U
            ? item_def
            : nullptr;

    case ActionKind::Drink:
        return item_has_capability(*item_def, ITEM_CAPABILITY_DRINK) ? item_def : nullptr;

    case ActionKind::Eat:
        return item_has_capability(*item_def, ITEM_CAPABILITY_EAT) ? item_def : nullptr;

    default:
        return nullptr;
    }
}

bool action_requires_item(ActionKind action_kind, std::uint32_t item_id) noexcept
{
    if (action_kind == ActionKind::Build)
    {
        return true;
    }

    if (action_kind == ActionKind::Plant ||
        action_kind == ActionKind::Drink ||
        action_kind == ActionKind::Eat)
    {
        return item_id != 0U;
    }

    return false;
}

bool should_reactivate_plant_placement_mode_after_completion(
    RuntimeInvocation& invocation,
    const ConstActionStateRef action_state)
{
    if (!action_state.reactivate_placement_mode_on_completion ||
        action_state.action_kind != ActionKind::Plant)
    {
        return false;
    }

    const auto* item_def = action_item_def(action_state.action_kind, action_state.item_id);
    if (item_def == nullptr || item_def->linked_plant_id.value == 0U)
    {
        return false;
    }

    auto world = SiteWorldAccess<ActionExecutionSystem> {invocation};
    const std::uint32_t available_quantity =
        available_worker_pack_item_quantity(
            world.read_inventory(),
            item_def->item_id);
    const std::uint32_t reserved_quantity =
        reserved_worker_pack_item_quantity(
            action_state,
            item_def->item_id);
    return available_quantity > reserved_quantity;
}

void reactivate_plant_placement_mode_from_completed_action(
    ActionStateRef action_state)
{
    const TileCoord target_tile = action_target_tile(action_state);
    const TileFootprint footprint =
        resolve_action_placement_footprint(
            action_state.action_kind,
            action_state.item_id,
            action_state.primary_subject_id);

    auto& placement_mode = action_state.placement_mode;
    clear_placement_mode(placement_mode);
    placement_mode.active = true;
    placement_mode.action_kind = action_state.action_kind;
    placement_mode.target_tile = target_tile;
    placement_mode.primary_subject_id = action_state.primary_subject_id;
    placement_mode.secondary_subject_id = action_state.secondary_subject_id;
    placement_mode.item_id = action_state.item_id;
    placement_mode.quantity = 1U;
    placement_mode.request_flags = action_state.request_flags;
    placement_mode.footprint_width = footprint.width;
    placement_mode.footprint_height = footprint.height;
    placement_mode.blocked_mask = full_blocked_mask_for_footprint(footprint);
}

Gs1InventoryContainerKind reserved_item_container_kind(const ConstActionStateRef action_state) noexcept
{
    if (!action_state.reserved_input_item_stacks.empty())
    {
        return action_state.reserved_input_item_stacks.front().container_kind;
    }

    return GS1_INVENTORY_CONTAINER_WORKER_PACK;
}

void populate_reserved_input_items(ActionStateRef action_state)
{
    action_state.reserved_input_item_stacks.clear();
    if (!action_requires_item(action_state.action_kind, action_state.item_id))
    {
        return;
    }

    const auto* item_def = action_item_def(action_state.action_kind, action_state.item_id);
    if (item_def == nullptr)
    {
        return;
    }

    action_state.reserved_input_item_stacks.push_back(ReservedItemStack {
        item_def->item_id,
        static_cast<std::uint32_t>(action_state.quantity == 0U ? 1U : action_state.quantity),
        GS1_INVENTORY_CONTAINER_WORKER_PACK,
        {0U, 0U, 0U}});
}

SiteActionFailureReason validate_reserved_item_completion(
    RuntimeInvocation& invocation,
    ConstActionStateRef action_state)
{
    auto world = SiteWorldAccess<ActionExecutionSystem> {invocation};
    for (const auto& reserved_stack : action_state.reserved_input_item_stacks)
    {
        const auto available_quantity =
            reserved_stack.container_kind == GS1_INVENTORY_CONTAINER_WORKER_PACK
            ? available_worker_pack_item_quantity(world.read_inventory(), reserved_stack.item_id)
            : inventory_storage::available_item_quantity_in_container_kind(
                  world.own_inventory(),
                  world.ecs_world_ptr(),
                  reserved_stack.container_kind,
                  reserved_stack.item_id);
        if (available_quantity < reserved_stack.quantity)
        {
            return SiteActionFailureReason::InsufficientResources;
        }
    }

    return SiteActionFailureReason::None;
}

SiteActionFailureReason validate_repair_target(
    RuntimeInvocation& invocation,
    TileCoord target_tile) noexcept
{
    auto world = SiteWorldAccess<ActionExecutionSystem> {invocation};
    if (!world.tile_coord_in_bounds(target_tile))
    {
        return SiteActionFailureReason::InvalidTarget;
    }

    const auto tile = world.read_tile(target_tile);
    if (tile.device.structure_id.value == 0U || tile.device.device_integrity <= 0.0f)
    {
        return SiteActionFailureReason::InvalidTarget;
    }

    if (tile.device.device_integrity >= (k_repair_integrity_full - k_repair_integrity_epsilon))
    {
        return SiteActionFailureReason::InvalidState;
    }

    return SiteActionFailureReason::None;
}

SiteActionFailureReason validate_harvest_target(
    RuntimeInvocation& invocation,
    TileCoord target_tile,
    const PlantDef** out_plant_def = nullptr) noexcept
{
    auto world = SiteWorldAccess<ActionExecutionSystem> {invocation};
    if (!world.tile_coord_in_bounds(target_tile))
    {
        return SiteActionFailureReason::InvalidTarget;
    }

    const auto tile = world.read_tile(target_tile);
    if (tile.ecology.plant_id.value == 0U)
    {
        return SiteActionFailureReason::InvalidTarget;
    }

    const auto* plant_def = find_plant_def(tile.ecology.plant_id);
    if (plant_def == nullptr || !plant_def_is_harvestable(*plant_def))
    {
        return SiteActionFailureReason::InvalidTarget;
    }

    const bool awards_item = harvest_awards_item(*plant_def, tile.ecology.plant_density);
    if (awards_item &&
        !inventory_storage::worker_pack_container(
            world.read_inventory(),
            world.ecs_world_ptr())
             .is_valid())
    {
        return SiteActionFailureReason::InvalidState;
    }

    if (out_plant_def != nullptr)
    {
        *out_plant_def = plant_def;
    }

    return SiteActionFailureReason::None;
}

bool inventory_can_fit_all_excavation_rewards(
    RuntimeInvocation& invocation,
    ExcavationDepth depth) noexcept
{
    const auto* depth_def = find_excavation_depth_def(depth);
    if (depth_def == nullptr || depth_def->find_chance_percent <= 0.0f)
    {
        return true;
    }

    auto world = SiteWorldAccess<ActionExecutionSystem> {invocation};
    const auto worker_pack =
        inventory_storage::worker_pack_container(
            world.read_inventory(),
            world.ecs_world_ptr());
    if (!worker_pack.is_valid())
    {
        return false;
    }

    const bool include_common = depth_def->common_tier_percent > 0.0f;
    const bool include_uncommon = depth_def->uncommon_tier_percent > 0.0f;
    const bool include_rare = depth_def->rare_tier_percent > 0.0f;
    const bool include_very_rare = depth_def->very_rare_tier_percent > 0.0f;
    const bool include_jackpot = depth_def->jackpot_tier_percent > 0.0f;

    for (const auto& entry : all_excavation_loot_entry_defs())
    {
        if (entry.depth != depth)
        {
            continue;
        }

        const bool tier_enabled =
            (entry.tier == ExcavationLootTier::Common && include_common) ||
            (entry.tier == ExcavationLootTier::Uncommon && include_uncommon) ||
            (entry.tier == ExcavationLootTier::Rare && include_rare) ||
            (entry.tier == ExcavationLootTier::VeryRare && include_very_rare) ||
            (entry.tier == ExcavationLootTier::Jackpot && include_jackpot);
        if (!tier_enabled)
        {
            continue;
        }

        if (!inventory_storage::can_fit_item_in_container(
                world.own_inventory(),
                world.ecs_world_ptr(),
                worker_pack,
                entry.item_id,
                1U))
        {
            return false;
        }
    }

    return true;
}

SiteActionFailureReason validate_excavation_target(
    RuntimeInvocation& invocation,
    TileCoord target_tile,
    ExcavationDepth requested_depth) noexcept
{
    auto world = SiteWorldAccess<ActionExecutionSystem> {invocation};
    if (!world.tile_coord_in_bounds(target_tile))
    {
        return SiteActionFailureReason::InvalidTarget;
    }

    const auto tile = world.read_tile(target_tile);
    if (tile_has_excavation_blocking_occupier(tile))
    {
        return SiteActionFailureReason::InvalidState;
    }

    const auto max_unlocked_depth = max_excavation_depth_unlocked(invocation);
    const auto resolved_depth = resolve_excavation_depth_request(
        tile.excavation.depth,
        requested_depth,
        max_unlocked_depth);
    if (resolved_depth == ExcavationDepth::None)
    {
        return SiteActionFailureReason::InvalidState;
    }

    if (!inventory_can_fit_all_excavation_rewards(invocation, resolved_depth))
    {
        return SiteActionFailureReason::InvalidState;
    }

    return SiteActionFailureReason::None;
}

ExcavationTierPercents resolve_excavation_tier_percents(
    ConstModifierStateRef modifier_state,
    ExcavationDepth depth) noexcept
{
    const auto* depth_def = find_excavation_depth_def(depth);
    if (depth_def == nullptr)
    {
        return {};
    }

    auto current = depth_tier_percents(*depth_def);
    if (!excavation_depth_matches_loot_rebalance(modifier_state, depth))
    {
        return current;
    }

    if (depth == ExcavationDepth::Careful)
    {
        const auto* next_depth_def = find_excavation_depth_def(ExcavationDepth::Thorough);
        return next_depth_def == nullptr
            ? current
            : blend_excavation_tier_percents(current, depth_tier_percents(*next_depth_def), 0.5f);
    }

    if (depth == ExcavationDepth::Thorough)
    {
        const auto* previous_depth_def = find_excavation_depth_def(ExcavationDepth::Careful);
        if (previous_depth_def == nullptr)
        {
            return current;
        }

        const auto previous = depth_tier_percents(*previous_depth_def);
        ExcavationTierPercents extrapolated {
            std::max(0.0f, current.common + (current.common - previous.common)),
            std::max(0.0f, current.uncommon + (current.uncommon - previous.uncommon)),
            std::max(0.0f, current.rare + (current.rare - previous.rare)),
            std::max(0.0f, current.very_rare + (current.very_rare - previous.very_rare)),
            std::max(0.0f, current.jackpot + (current.jackpot - previous.jackpot))};
        const float total =
            extrapolated.common + extrapolated.uncommon + extrapolated.rare +
            extrapolated.very_rare + extrapolated.jackpot;
        if (total > 0.0f)
        {
            const float normalize = 100.0f / total;
            extrapolated.common *= normalize;
            extrapolated.uncommon *= normalize;
            extrapolated.rare *= normalize;
            extrapolated.very_rare *= normalize;
            extrapolated.jackpot *= normalize;
        }
        return blend_excavation_tier_percents(current, extrapolated, 0.5f);
    }

    return current;
}

ExcavationLootTier roll_excavation_loot_tier(
    ConstModifierStateRef modifier_state,
    ExcavationDepth depth,
    std::uint64_t seed) noexcept
{
    const auto percents = resolve_excavation_tier_percents(
        modifier_state,
        depth);

    const float roll = percent_from_seed(seed);
    float cursor = percents.common;
    if (roll < cursor)
    {
        return ExcavationLootTier::Common;
    }
    cursor += percents.uncommon;
    if (roll < cursor)
    {
        return ExcavationLootTier::Uncommon;
    }
    cursor += percents.rare;
    if (roll < cursor)
    {
        return ExcavationLootTier::Rare;
    }
    cursor += percents.very_rare;
    if (roll < cursor)
    {
        return ExcavationLootTier::VeryRare;
    }
    return ExcavationLootTier::Jackpot;
}

ItemId roll_excavation_loot_item(
    ExcavationDepth depth,
    ExcavationLootTier tier,
    std::uint64_t seed) noexcept
{
    const float roll = percent_from_seed(seed);
    float cursor = 0.0f;
    for (const auto& entry : all_excavation_loot_entry_defs())
    {
        if (entry.depth != depth || entry.tier != tier)
        {
            continue;
        }

        cursor += entry.percent_within_tier;
        if (roll < cursor)
        {
            return entry.item_id;
        }
    }

    return {};
}

ItemId resolve_excavation_reward_item(
    RuntimeInvocation& invocation,
    TileCoord target_tile,
    ExcavationDepth depth) noexcept
{
    auto world = SiteWorldAccess<ActionExecutionSystem> {invocation};
    const auto* depth_def = find_excavation_depth_def(depth);
    if (depth_def == nullptr)
    {
        return {};
    }

    const auto x = static_cast<std::uint32_t>(target_tile.x < 0 ? 0 : target_tile.x);
    const auto y = static_cast<std::uint32_t>(target_tile.y < 0 ? 0 : target_tile.y);
    const std::uint64_t base_seed =
        world.site_attempt_seed() ^
        (static_cast<std::uint64_t>(x) << 32U) ^
        (static_cast<std::uint64_t>(y) << 16U) ^
        static_cast<std::uint64_t>(depth);

    if (percent_from_seed(base_seed + 1U) >= depth_def->find_chance_percent)
    {
        return {};
    }

    const auto tier = roll_excavation_loot_tier(world.read_modifier(), depth, base_seed + 2U);
    return roll_excavation_loot_item(depth, tier, base_seed + 3U);
}

const CraftRecipeDef* resolve_craft_recipe_for_action(
    RuntimeInvocation& invocation,
    TileCoord target_tile,
    std::uint32_t output_item_id,
    std::uint64_t* out_device_entity_id = nullptr) noexcept
{
    auto world = SiteWorldAccess<ActionExecutionSystem> {invocation};
    if (output_item_id == 0U || !world.tile_coord_in_bounds(target_tile))
    {
        return nullptr;
    }

    const auto tile = world.read_tile(target_tile);
    if (!structure_is_crafting_station(tile.device.structure_id) || !world.has_world())
    {
        return nullptr;
    }

    if (out_device_entity_id != nullptr)
    {
        *out_device_entity_id = world.device_entity_id(target_tile);
    }

    return find_craft_recipe_def(tile.device.structure_id, ItemId {output_item_id});
}

bool craft_ingredients_available_for_action(
    RuntimeInvocation& invocation,
    TileCoord target_tile,
    std::uint64_t device_entity_id,
    const CraftRecipeDef& recipe_def)
{
    auto world = SiteWorldAccess<ActionExecutionSystem> {invocation};
    const auto cached_items = craft_logic::nearby_item_instance_ids_for_device(
        world.read_inventory(),
        world.ecs_world_ptr(),
        world.read_worker().position.tile_coord,
        world.read_craft_device_caches(),
        world.read_craft_nearby_items(),
        device_entity_id,
        target_tile);
    return craft_logic::can_satisfy_recipe_requirements(
        world.read_inventory(),
        world.ecs_world_ptr(),
        cached_items,
        recipe_def);
}

bool craft_output_fits_for_action(
    RuntimeInvocation& invocation,
    TileCoord target_tile,
    std::uint64_t device_entity_id,
    const CraftRecipeDef& recipe_def)
{
    auto world = SiteWorldAccess<ActionExecutionSystem> {invocation};
    const auto* structure_def = find_structure_def(recipe_def.station_structure_id);
    if (structure_def == nullptr || !structure_def->grants_storage || structure_def->storage_slot_count == 0U)
    {
        return false;
    }

    const auto source_containers =
        craft_logic::collect_nearby_source_containers(
            world.read_inventory(),
            world.ecs_world_ptr(),
            world.read_worker().position.tile_coord,
            target_tile);
    const auto output_container =
        inventory_storage::find_device_storage_container(
            world.own_inventory(),
            world.ecs_world_ptr(),
            device_entity_id);
    if (!output_container.is_valid())
    {
        auto remaining = static_cast<std::uint32_t>(recipe_def.output_quantity);
        const auto max_stack = item_stack_size(recipe_def.output_item_id);
        for (std::uint16_t slot_index = 0U;
             slot_index < structure_def->storage_slot_count && remaining > 0U;
             ++slot_index)
        {
            remaining = remaining > max_stack ? remaining - max_stack : 0U;
        }
        return remaining == 0U;
    }

    return craft_logic::can_store_output_after_recipe_consumption(
        world.read_inventory(),
        world.ecs_world_ptr(),
        output_container,
        source_containers,
        recipe_def);
}

bool is_action_waiting_for_placement(const ConstActionStateRef action_state) noexcept
{
    return has_active_action(action_state) && action_state.awaiting_placement_reservation;
}

bool action_has_started(const ConstActionStateRef action_state) noexcept
{
    return action_state.has_started_at_world_minute;
}

bool action_has_started(const ActionStateRef action_state) noexcept
{
    return action_has_started(ConstActionStateRef {
        action_state.current_action_id,
        action_state.action_kind,
        action_state.target_tile,
        action_state.approach_tile,
        action_state.primary_subject_id,
        action_state.secondary_subject_id,
        action_state.item_id,
        action_state.placement_reservation_token,
        action_state.quantity,
        action_state.request_flags,
        action_state.awaiting_placement_reservation,
        action_state.reactivate_placement_mode_on_completion,
        action_state.total_action_minutes,
        action_state.remaining_action_minutes,
        action_state.deferred_meter_delta,
        action_state.resolved_harvest_density,
        action_state.resolved_harvest_outputs_valid,
        action_state.started_at_world_minute,
        action_state.placement_mode,
        action_state.has_current_action_id,
        action_state.has_target_tile,
        action_state.has_approach_tile,
        action_state.has_started_at_world_minute,
        action_state.reserved_input_item_stacks,
        action_state.resolved_harvest_outputs});
}

bool is_action_waiting_for_worker_approach(const ConstActionStateRef action_state) noexcept
{
    return has_active_action(action_state) &&
        !action_state.awaiting_placement_reservation &&
        action_requires_worker_approach(action_state.action_kind) &&
        !action_has_started(action_state);
}

bool is_action_in_progress(const ConstActionStateRef action_state) noexcept
{
    return has_active_action(action_state) && action_has_started(action_state);
}

double current_action_total_duration_minutes(const ConstActionStateRef action_state) noexcept
{
    return std::max(action_state.total_action_minutes, k_minimum_action_duration_minutes);
}

float resolve_action_progress_scale(
    RuntimeInvocation& invocation,
    const ConstActionStateRef action_state) noexcept
{
    static_cast<void>(invocation);
    static_cast<void>(action_state);
    return 1.0f;
}

double action_elapsed_minutes_for_step(
    RuntimeInvocation& invocation,
    const ConstActionStateRef action_state) noexcept
{
    auto access = make_game_state_access<ActionExecutionSystem>(invocation);
    const double fixed_step_seconds = access.template read<RuntimeFixedStepSecondsTag>();
    return std::max(0.0, fixed_step_seconds) *
        k_runtime_minutes_per_real_second *
        static_cast<double>(resolve_action_progress_scale(invocation, action_state));
}

std::optional<TileCoord> resolve_action_approach_tile(
    const SiteWorld& site_world,
    const SiteWorld::WorkerData& worker,
    TileCoord target_tile,
    bool interaction_range_allowed)
{
    if (interaction_range_allowed)
    {
        return device_interaction_logic::resolve_interaction_range_approach_tile(
            site_world,
            worker,
            target_tile);
    }

    return device_interaction_logic::resolve_direct_approach_tile(
        site_world,
        worker,
        target_tile);
}

bool action_target_supports_interaction_range(
    RuntimeInvocation& invocation,
    TileCoord target_tile) noexcept
{
    auto world = SiteWorldAccess<ActionExecutionSystem> {invocation};
    if (!world.tile_coord_in_bounds(target_tile))
    {
        return false;
    }

    return world.read_tile(target_tile).device.structure_id.value != 0U;
}

bool worker_is_at_action_approach_tile(
    RuntimeInvocation& invocation,
    const ConstActionStateRef action_state) noexcept
{
    if (!action_state.has_approach_tile)
    {
        return true;
    }

    auto world = SiteWorldAccess<ActionExecutionSystem> {invocation};
    return device_interaction_logic::worker_is_at_tile(
        world.read_worker(),
        action_state.approach_tile);
}

void begin_action_execution(
    RuntimeInvocation& invocation,
    ActionStateRef action_state)
{
    auto world = SiteWorldAccess<ActionExecutionSystem> {invocation};
    if (!action_state.has_current_action_id || action_has_started(action_state))
    {
        return;
    }

    action_state.started_at_world_minute = world.read_time().world_time_minutes;
    action_state.has_started_at_world_minute = true;
    emit_site_action_started(
        invocation.game_message_queue(),
        action_state.current_action_id,
        action_state.action_kind,
        action_state.request_flags,
        action_target_tile(action_state),
        action_state.primary_subject_id,
        static_cast<float>(action_state.remaining_action_minutes));
    const auto* craft_recipe =
        action_state.action_kind == ActionKind::Craft && action_state.secondary_subject_id != 0U
        ? find_craft_recipe_def(RecipeId {action_state.secondary_subject_id})
        : nullptr;
    action_state.deferred_meter_delta = resolve_worker_meter_cost_delta(
        invocation,
        action_state.action_kind,
        action_state.quantity,
        action_state.primary_subject_id,
        craft_recipe);
}

void emit_placement_reservation_requested(
    GameMessageQueue& queue,
    std::uint32_t action_id,
    ActionKind action_kind,
    TileCoord target_tile,
    PlacementReservationSubjectKind subject_kind,
    std::uint32_t subject_id)
{
    enqueue_message(
        queue,
        GameMessageType::PlacementReservationRequested,
        PlacementReservationRequestedMessage {
            action_id,
            target_tile.x,
            target_tile.y,
            placement_occupancy_layer(action_kind),
            subject_kind,
            {0U, 0U},
            subject_id});
}

PlacementReservationSubjectKind placement_reservation_subject_kind(
    ActionKind action_kind,
    std::uint32_t item_id) noexcept
{
    if (action_kind == ActionKind::Build)
    {
        return PlacementReservationSubjectKind::StructureType;
    }

    if (action_kind != ActionKind::Plant)
    {
        return PlacementReservationSubjectKind::None;
    }

    const auto* item_def = action_item_def(action_kind, item_id);
    return item_def == nullptr
        ? PlacementReservationSubjectKind::GroundCoverType
        : PlacementReservationSubjectKind::PlantType;
}

std::uint32_t placement_reservation_subject_id(
    ActionKind action_kind,
    std::uint32_t item_id,
    std::uint32_t primary_subject_id) noexcept
{
    if (action_kind == ActionKind::Build)
    {
        const auto* item_def = action_item_def(action_kind, item_id);
        return item_def == nullptr ? 0U : item_def->linked_structure_id.value;
    }

    if (action_kind != ActionKind::Plant)
    {
        return primary_subject_id;
    }

    const auto* item_def = action_item_def(action_kind, item_id);
    return item_def == nullptr ? primary_subject_id : item_def->linked_plant_id.value;
}

TileFootprint resolve_action_placement_footprint(
    ActionKind action_kind,
    std::uint32_t item_id,
    std::uint32_t primary_subject_id) noexcept
{
    return resolve_placement_reservation_footprint(
        placement_occupancy_layer(action_kind),
        placement_reservation_subject_kind(action_kind, item_id),
        placement_reservation_subject_id(action_kind, item_id, primary_subject_id));
}

TileCoord align_action_target_tile(
    ActionKind action_kind,
    std::uint32_t item_id,
    std::uint32_t primary_subject_id,
    TileCoord target_tile) noexcept
{
    if (!should_request_placement_reservation(action_kind))
    {
        return target_tile;
    }

    return align_tile_anchor_to_footprint(
        target_tile,
        resolve_action_placement_footprint(
            action_kind,
            item_id,
            primary_subject_id));
}

TileCoord resolve_initial_placement_mode_tile(
    RuntimeInvocation& invocation,
    TileCoord requested_target_tile) noexcept
{
    auto world = SiteWorldAccess<ActionExecutionSystem> {invocation};
    if (world.tile_coord_in_bounds(requested_target_tile))
    {
        return requested_target_tile;
    }

    const auto worker_tile = world.read_worker().position.tile_coord;
    return world.tile_coord_in_bounds(worker_tile)
        ? worker_tile
        : TileCoord {};
}

void update_placement_mode_preview(
    RuntimeInvocation& invocation,
    PlacementModeMetaState placement_mode,
    TileCoord target_tile)
{
    auto world = SiteWorldAccess<ActionExecutionSystem> {invocation};
    const TileCoord aligned_target_tile = align_tile_anchor_to_footprint(
        target_tile,
        resolve_action_placement_footprint(
            placement_mode.action_kind,
            placement_mode.item_id,
            placement_mode.primary_subject_id));
    const auto preview = build_placement_preview(
        world,
        aligned_target_tile,
        placement_occupancy_layer(placement_mode.action_kind),
        placement_reservation_subject_kind(
            placement_mode.action_kind,
            placement_mode.item_id),
        placement_reservation_subject_id(
            placement_mode.action_kind,
            placement_mode.item_id,
            placement_mode.primary_subject_id));
    placement_mode.target_tile = aligned_target_tile;
    placement_mode.footprint_width = preview.footprint.width;
    placement_mode.footprint_height = preview.footprint.height;
    placement_mode.blocked_mask = preview.blocked_mask;
}

bool placement_mode_can_commit_to_tile(
    RuntimeInvocation& invocation,
    PlacementModeMetaState placement_mode,
    TileCoord target_tile)
{
    auto world = SiteWorldAccess<ActionExecutionSystem> {invocation};
    update_placement_mode_preview(invocation, placement_mode, target_tile);
    return placement_mode.has_target_tile &&
        world.tile_coord_in_bounds(placement_mode.target_tile) &&
        placement_mode.blocked_mask == 0ULL;
}

bool placement_mode_matches_request(
    const PlacementModeMetaState placement_mode,
    ActionKind action_kind,
    std::uint16_t quantity,
    std::uint32_t primary_subject_id,
    std::uint32_t secondary_subject_id,
    std::uint32_t item_id) noexcept
{
    return placement_mode.active &&
        placement_mode.action_kind == action_kind &&
        placement_mode.quantity == quantity &&
        placement_mode.primary_subject_id == primary_subject_id &&
        placement_mode.secondary_subject_id == secondary_subject_id &&
        placement_mode.item_id == item_id;
}

void emit_placement_reservation_released(
    GameMessageQueue& queue,
    ConstActionStateRef action_state)
{
    if (!action_state.has_current_action_id)
    {
        return;
    }

    enqueue_message(
        queue,
        GameMessageType::PlacementReservationReleased,
        PlacementReservationReleasedMessage {
            action_state.current_action_id.value,
            action_state.placement_reservation_token});
}

void emit_placement_mode_commit_rejected(
    GameMessageQueue& queue,
    const PlacementModeMetaState placement_mode,
    TileCoord target_tile)
{
    const TileCoord rejected_tile = placement_mode.has_target_tile ? placement_mode.target_tile : target_tile;
    enqueue_message(
        queue,
        GameMessageType::PlacementModeCommitRejected,
        PlacementModeCommitRejectedMessage {
            rejected_tile.x,
            rejected_tile.y,
            placement_mode.blocked_mask,
            to_gs1_action_kind(placement_mode.action_kind),
            placement_mode.item_id});
}

void emit_action_fact_messages(
    RuntimeInvocation& invocation,
    const ConstActionStateRef action_state)
{
    auto& queue = invocation.game_message_queue();
    const TileCoord target_tile = action_target_tile(action_state);
    const auto action_id = action_id_value(action_state);
    const auto flags = static_cast<std::uint32_t>(action_state.request_flags);
    const std::uint16_t safe_quantity = static_cast<std::uint16_t>(
        action_state.quantity == 0U ? 1U : action_state.quantity);

    switch (action_state.action_kind)
    {
    case ActionKind::Plant:
        if (const auto* item_def = action_item_def(
                action_state.action_kind,
                action_state.item_id))
        {
            enqueue_message(
                queue,
                GameMessageType::InventoryItemConsumeRequested,
                InventoryItemConsumeRequestedMessage {
                    item_def->item_id.value,
                    safe_quantity,
                    GS1_INVENTORY_CONTAINER_WORKER_PACK,
                    k_inventory_item_consume_flag_ignore_action_reservations});
            enqueue_message(
                queue,
                GameMessageType::SiteTilePlantingCompleted,
                SiteTilePlantingCompletedMessage {
                    action_id,
                    target_tile.x,
                    target_tile.y,
                    item_def->linked_plant_id.value,
                    100.0f,
                    flags});
        }
        else
        {
            enqueue_message(
                queue,
                GameMessageType::SiteGroundCoverPlaced,
                SiteGroundCoverPlacedMessage {
                    action_id,
                    target_tile.x,
                    target_tile.y,
                    action_state.primary_subject_id == 0U ? 1U : action_state.primary_subject_id,
                    std::min(100.0f, 25.0f * static_cast<float>(safe_quantity)),
                    flags});
        }
        break;

    case ActionKind::Water:
        enqueue_message(
            queue,
            GameMessageType::SiteTileWatered,
            SiteTileWateredMessage {
                action_id,
                target_tile.x,
                target_tile.y,
                static_cast<float>(action_state.quantity == 0U ? 1U : action_state.quantity),
                flags});
        break;

    case ActionKind::ClearBurial:
        enqueue_message(
            queue,
            GameMessageType::SiteTileBurialCleared,
            SiteTileBurialClearedMessage {
                action_id,
                target_tile.x,
                target_tile.y,
                35.0f * static_cast<float>(action_state.quantity == 0U ? 1U : action_state.quantity),
                flags});
        break;

    case ActionKind::Build:
        if (const auto* item_def = action_item_def(
                action_state.action_kind,
                action_state.item_id))
        {
            enqueue_message(
                queue,
                GameMessageType::InventoryItemConsumeRequested,
                InventoryItemConsumeRequestedMessage {
                    item_def->item_id.value,
                    safe_quantity,
                    GS1_INVENTORY_CONTAINER_WORKER_PACK,
                    k_inventory_item_consume_flag_ignore_action_reservations});
            enqueue_message(
                queue,
                GameMessageType::SiteDevicePlaced,
                SiteDevicePlacedMessage {
                    action_id,
                    target_tile.x,
                    target_tile.y,
                    item_def->linked_structure_id.value,
                    flags});
        }
        break;

    case ActionKind::Drink:
    case ActionKind::Eat:
        if (const auto* item_def = action_item_def(
                action_state.action_kind,
                action_state.item_id))
        {
            enqueue_message(
                queue,
                GameMessageType::InventoryItemConsumeRequested,
                InventoryItemConsumeRequestedMessage {
                    item_def->item_id.value,
                    safe_quantity,
                    reserved_item_container_kind(action_state),
                    k_inventory_item_consume_flag_ignore_action_reservations});
        }
        break;

    case ActionKind::Craft:
        if (action_state.secondary_subject_id != 0U)
        {
            enqueue_message(
                queue,
                GameMessageType::InventoryCraftCommitRequested,
                InventoryCraftCommitRequestedMessage {
                    action_state.secondary_subject_id,
                    target_tile.x,
                    target_tile.y,
                    flags});
        }
        break;

    case ActionKind::Repair:
        enqueue_message(
            queue,
            GameMessageType::SiteDeviceRepaired,
            SiteDeviceRepairedMessage {
                action_id,
                target_tile.x,
                target_tile.y,
                flags});
        break;

    case ActionKind::Harvest:
        if (const auto* plant_def = find_plant_def(PlantId {action_state.primary_subject_id});
            plant_def != nullptr && plant_def_is_harvestable(*plant_def))
        {
            for (const auto& resolved_output : action_state.resolved_harvest_outputs)
            {
                if (resolved_output.quantity == 0U)
                {
                    continue;
                }

                enqueue_message(
                    queue,
                    GameMessageType::InventoryWorkerPackInsertRequested,
                    InventoryWorkerPackInsertRequestedMessage {
                        resolved_output.item_id,
                        resolved_output.quantity,
                        0U});
            }
            const std::uint32_t summary_item_id =
                harvest_summary_item_id(action_state.resolved_harvest_outputs, *plant_def);
            const std::uint16_t summary_item_quantity =
                harvest_summary_item_quantity(action_state.resolved_harvest_outputs, summary_item_id);
            enqueue_message(
                queue,
                GameMessageType::SiteTileHarvested,
                SiteTileHarvestedMessage {
                    action_id,
                    target_tile.x,
                    target_tile.y,
                    plant_def->plant_id.value,
                    summary_item_id,
                    summary_item_quantity,
                    0U,
                    plant_def->harvest_density_removed});
        }
        break;

    case ActionKind::Excavate:
    {
        auto world = SiteWorldAccess<ActionExecutionSystem> {invocation};
        const auto depth = excavation_depth_from_subject_id(action_state.primary_subject_id);
        const auto item_id = resolve_excavation_reward_item(invocation, target_tile, depth);
        auto excavation = world.read_tile_excavation(target_tile);
        excavation.depth = depth;
        world.write_tile_excavation(target_tile, excavation);
        if (item_id.value != 0U)
        {
            enqueue_message(
                queue,
                GameMessageType::InventoryWorkerPackInsertRequested,
                InventoryWorkerPackInsertRequestedMessage {
                    item_id.value,
                    1U,
                    0U});
        }
        break;
    }

    case ActionKind::None:
    default:
        break;
    }
}

SiteActionFailureReason validate_craft_completion(
    RuntimeInvocation& invocation,
    const ConstActionStateRef action_state)
{
    auto world = SiteWorldAccess<ActionExecutionSystem> {invocation};
    if (!action_state.has_target_tile || action_state.secondary_subject_id == 0U)
    {
        return SiteActionFailureReason::InvalidState;
    }

    const auto* recipe_def = find_craft_recipe_def(RecipeId {action_state.secondary_subject_id});
    if (recipe_def == nullptr)
    {
        return SiteActionFailureReason::InvalidState;
    }

    if (!world.tile_coord_in_bounds(action_state.target_tile) || !world.has_world())
    {
        return SiteActionFailureReason::InvalidTarget;
    }

    const auto tile = world.read_tile(action_state.target_tile);
    if (tile.device.structure_id != recipe_def->station_structure_id)
    {
        return SiteActionFailureReason::InvalidTarget;
    }

    const auto device_entity_id = world.device_entity_id(action_state.target_tile);
    if (device_entity_id == 0U)
    {
        return SiteActionFailureReason::InvalidTarget;
    }

    if (!craft_ingredients_available_for_action(
            invocation,
            action_state.target_tile,
            device_entity_id,
            *recipe_def))
    {
        return SiteActionFailureReason::InsufficientResources;
    }

    if (!craft_output_fits_for_action(
            invocation,
            action_state.target_tile,
            device_entity_id,
            *recipe_def))
    {
        return SiteActionFailureReason::InvalidState;
    }

    return SiteActionFailureReason::None;
}

SiteActionFailureReason validate_repair_completion(
    RuntimeInvocation& invocation,
    const ConstActionStateRef action_state)
{
    if (!action_state.has_target_tile)
    {
        return SiteActionFailureReason::InvalidState;
    }

    return validate_repair_target(invocation, action_state.target_tile);
}

SiteActionFailureReason validate_harvest_completion(
    RuntimeInvocation& invocation,
    const ConstActionStateRef action_state)
{
    if (!action_state.has_target_tile)
    {
        return SiteActionFailureReason::InvalidState;
    }

    const auto failure_reason = validate_harvest_target(invocation, action_state.target_tile);
    if (failure_reason != SiteActionFailureReason::None)
    {
        return failure_reason;
    }

    if (!action_state.resolved_harvest_outputs_valid)
    {
        return SiteActionFailureReason::InvalidState;
    }

    return inventory_can_fit_harvest_outputs(invocation, action_state.resolved_harvest_outputs)
        ? SiteActionFailureReason::None
        : SiteActionFailureReason::InvalidState;
}

SiteActionFailureReason validate_excavation_completion(
    RuntimeInvocation& invocation,
    const ConstActionStateRef action_state)
{
    if (!action_state.has_target_tile)
    {
        return SiteActionFailureReason::InvalidState;
    }

    return validate_excavation_target(
        invocation,
        action_state.target_tile,
        excavation_depth_from_subject_id(action_state.primary_subject_id));
}

}  // namespace

Gs1Status handle_start_site_action(
    RuntimeInvocation& invocation,
    const StartSiteActionMessage& payload);

Gs1Status handle_cancel_site_action(
    RuntimeInvocation& invocation,
    const CancelSiteActionMessage& payload);

Gs1Status handle_placement_mode_cursor_moved(
    RuntimeInvocation& invocation,
    const PlacementModeCursorMovedMessage& payload);

Gs1Status handle_start_site_action(
    RuntimeInvocation& invocation,
    const StartSiteActionMessage& payload)
{
    auto world = SiteWorldAccess<ActionExecutionSystem> {invocation};
    auto action_state = world.own_action();
    const ActionKind action_kind = to_action_kind(payload.action_kind);
    const bool deferred_target_selection =
        (payload.flags & GS1_SITE_ACTION_REQUEST_FLAG_DEFERRED_TARGET_SELECTION) != 0U;
    const std::uint8_t request_flags = static_cast<std::uint8_t>(
        payload.flags & ~GS1_SITE_ACTION_REQUEST_FLAG_DEFERRED_TARGET_SELECTION);
    const std::uint16_t requested_quantity = payload.quantity == 0U ? 1U : payload.quantity;
    TileCoord target_tile {payload.target_tile_x, payload.target_tile_y};
    std::uint32_t primary_subject_id = payload.primary_subject_id;
    std::uint32_t secondary_subject_id = payload.secondary_subject_id;
    std::uint32_t item_id = payload.item_id;
    bool reactivate_plant_placement_mode_on_completion = false;

    if (has_active_placement_mode(action_state))
    {
        if (deferred_target_selection)
        {
            emit_site_action_failed(
                invocation.game_message_queue(),
                0U,
                action_kind,
                SiteActionFailureReason::Busy,
                request_flags,
                target_tile,
                primary_subject_id,
                secondary_subject_id);
            return GS1_STATUS_OK;
        }

        if (!placement_mode_matches_request(
                action_state.placement_mode,
                action_kind,
                requested_quantity,
                primary_subject_id,
                secondary_subject_id,
                item_id))
        {
            emit_site_action_failed(
                invocation.game_message_queue(),
                0U,
                action_kind,
                SiteActionFailureReason::Busy,
                request_flags,
                target_tile,
                primary_subject_id,
                secondary_subject_id);
            return GS1_STATUS_OK;
        }

        if (!placement_mode_can_commit_to_tile(
                invocation,
                action_state.placement_mode,
                target_tile))
        {
            emit_placement_mode_commit_rejected(
                invocation.game_message_queue(),
                action_state.placement_mode,
                target_tile);
            return GS1_STATUS_OK;
        }

            const auto placement_mode = action_state.placement_mode;
        reactivate_plant_placement_mode_on_completion =
            placement_mode.action_kind == ActionKind::Plant &&
            placement_mode.item_id != 0U;
        clear_placement_mode(action_state.placement_mode);
        target_tile = placement_mode.has_target_tile ? placement_mode.target_tile : target_tile;
        primary_subject_id = placement_mode.primary_subject_id;
        secondary_subject_id = placement_mode.secondary_subject_id;
        item_id = placement_mode.item_id;
    }
    else if (has_active_action(action_state))
    {
        emit_site_action_failed(
            invocation.game_message_queue(),
            0U,
            action_kind,
            SiteActionFailureReason::Busy,
            request_flags,
            target_tile,
            primary_subject_id,
            secondary_subject_id);
        return GS1_STATUS_OK;
    }

        if (action_kind == ActionKind::None)
        {
            emit_site_action_failed(
                invocation.game_message_queue(),
                0U,
                action_kind,
                SiteActionFailureReason::InvalidState,
                request_flags,
                target_tile,
                primary_subject_id,
                secondary_subject_id);
            return GS1_STATUS_OK;
        }

        if (deferred_target_selection &&
            !action_supports_deferred_target_selection(action_kind))
        {
            emit_site_action_failed(
                invocation.game_message_queue(),
                0U,
                action_kind,
                SiteActionFailureReason::InvalidState,
                request_flags,
                target_tile,
                primary_subject_id,
                secondary_subject_id);
            return GS1_STATUS_OK;
        }

        const ItemId repair_tool_id = action_kind == ActionKind::Repair
            ? repair_tool_item_id(item_id)
            : ItemId {};

        if (action_requires_item(action_kind, item_id))
        {
            const auto* item_def = action_item_def(action_kind, item_id);
            if (item_def == nullptr)
            {
                emit_site_action_failed(
                    invocation.game_message_queue(),
                    0U,
                    action_kind,
                    SiteActionFailureReason::InvalidState,
                    request_flags,
                    target_tile,
                    primary_subject_id,
                    secondary_subject_id);
                return GS1_STATUS_OK;
            }

            const auto required_quantity = requested_quantity;
            const auto available_quantity =
                available_worker_pack_item_quantity(world.read_inventory(), item_def->item_id);
            if (available_quantity < required_quantity)
            {
                emit_site_action_failed(
                    invocation.game_message_queue(),
                    0U,
                    action_kind,
                    SiteActionFailureReason::InsufficientResources,
                    request_flags,
                    target_tile,
                    primary_subject_id,
                    secondary_subject_id);
                return GS1_STATUS_OK;
            }
        }

        if (deferred_target_selection)
        {
            auto& placement_mode = action_state.placement_mode;
            clear_placement_mode(placement_mode);
            placement_mode.active = true;
            placement_mode.action_kind = action_kind;
            placement_mode.primary_subject_id = primary_subject_id;
            placement_mode.secondary_subject_id = secondary_subject_id;
            placement_mode.item_id = item_id;
            placement_mode.quantity = requested_quantity;
            placement_mode.request_flags = request_flags;
            update_placement_mode_preview(
                invocation,
                placement_mode,
                resolve_initial_placement_mode_tile(invocation, target_tile));
            return GS1_STATUS_OK;
        }

        if (!world.tile_coord_in_bounds(target_tile))
        {
            emit_site_action_failed(
                invocation.game_message_queue(),
                0U,
                action_kind,
                SiteActionFailureReason::InvalidTarget,
                request_flags,
                target_tile,
                primary_subject_id,
                secondary_subject_id);
            return GS1_STATUS_OK;
        }

        target_tile = align_action_target_tile(
            action_kind,
            item_id,
            primary_subject_id,
            target_tile);

        if (action_kind == ActionKind::Repair)
        {
            const auto repair_failure_reason = validate_repair_target(invocation, target_tile);
            if (repair_failure_reason != SiteActionFailureReason::None)
            {
                emit_site_action_failed(
                    invocation.game_message_queue(),
                    0U,
                    action_kind,
                    repair_failure_reason,
                    request_flags,
                    target_tile,
                    primary_subject_id,
                    secondary_subject_id);
                return GS1_STATUS_OK;
            }

            if (repair_tool_id.value == 0U)
            {
                emit_site_action_failed(
                    invocation.game_message_queue(),
                    0U,
                    action_kind,
                    SiteActionFailureReason::InvalidState,
                    request_flags,
                    target_tile,
                    primary_subject_id,
                    secondary_subject_id);
                return GS1_STATUS_OK;
            }

            if (available_worker_pack_item_quantity(world.read_inventory(), repair_tool_id) == 0U)
            {
                emit_site_action_failed(
                    invocation.game_message_queue(),
                    0U,
                    action_kind,
                    SiteActionFailureReason::InsufficientResources,
                    request_flags,
                    target_tile,
                    primary_subject_id,
                    secondary_subject_id);
                return GS1_STATUS_OK;
            }
        }

        const PlantDef* harvest_plant_def = nullptr;
        if (action_kind == ActionKind::Harvest)
        {
            const auto harvest_failure_reason =
                validate_harvest_target(invocation, target_tile, &harvest_plant_def);
            if (harvest_failure_reason != SiteActionFailureReason::None)
            {
                emit_site_action_failed(
                    invocation.game_message_queue(),
                    0U,
                    action_kind,
                    harvest_failure_reason,
                    request_flags,
                    target_tile,
                    primary_subject_id,
                    secondary_subject_id);
                return GS1_STATUS_OK;
            }

            if (harvest_plant_def != nullptr)
            {
                primary_subject_id = harvest_plant_def->plant_id.value;
                item_id = 0U;
            }
        }

        if (action_kind == ActionKind::Excavate)
        {
            const auto requested_depth = excavation_depth_from_subject_id(secondary_subject_id);
            const auto excavation_failure_reason =
                validate_excavation_target(invocation, target_tile, requested_depth);
            if (excavation_failure_reason != SiteActionFailureReason::None)
            {
                emit_site_action_failed(
                    invocation.game_message_queue(),
                    0U,
                    action_kind,
                    excavation_failure_reason,
                    request_flags,
                    target_tile,
                    primary_subject_id,
                    secondary_subject_id);
                return GS1_STATUS_OK;
            }

            const auto current_depth = world.read_tile_excavation(target_tile).depth;
            primary_subject_id = excavation_depth_subject_id(
                resolve_excavation_depth_request(
                    current_depth,
                    requested_depth,
                    max_excavation_depth_unlocked(invocation)));
            secondary_subject_id = 0U;
            item_id = 0U;
        }

        const CraftRecipeDef* craft_recipe = nullptr;
        std::uint64_t craft_device_entity_id = 0U;
        if (action_kind == ActionKind::Craft)
        {
            craft_recipe = resolve_craft_recipe_for_action(
                invocation,
                target_tile,
                item_id,
                &craft_device_entity_id);
            if (craft_recipe == nullptr || craft_device_entity_id == 0U)
            {
                emit_site_action_failed(
                    invocation.game_message_queue(),
                    0U,
                    action_kind,
                    SiteActionFailureReason::InvalidTarget,
                    request_flags,
                    target_tile,
                    primary_subject_id,
                    secondary_subject_id);
                return GS1_STATUS_OK;
            }

            if (!craft_ingredients_available_for_action(
                    invocation,
                    target_tile,
                    craft_device_entity_id,
                    *craft_recipe))
            {
                emit_site_action_failed(
                    invocation.game_message_queue(),
                    0U,
                    action_kind,
                    SiteActionFailureReason::InsufficientResources,
                    request_flags,
                    target_tile,
                    primary_subject_id,
                    secondary_subject_id);
                return GS1_STATUS_OK;
            }
        }

        const double duration_minutes = compute_duration_minutes(
            invocation,
            action_kind,
            requested_quantity,
            primary_subject_id,
            item_id,
            craft_recipe);
    const RuntimeActionId action_id = allocate_runtime_action_id();
        std::vector<ResolvedHarvestOutputStack> resolved_harvest_outputs {};
        float resolved_harvest_density = 0.0f;
        if (action_kind == ActionKind::Harvest && harvest_plant_def != nullptr)
        {
            resolved_harvest_density = world.read_tile(target_tile).ecology.plant_density;
            resolved_harvest_outputs =
                resolve_harvest_outputs(
                    invocation,
                    action_id,
                    target_tile,
                    *harvest_plant_def,
                    resolved_harvest_density);
            if (!inventory_can_fit_harvest_outputs(invocation, resolved_harvest_outputs))
            {
                emit_site_action_failed(
                    invocation.game_message_queue(),
                    0U,
                    action_kind,
                    SiteActionFailureReason::InvalidState,
                    request_flags,
                    target_tile,
                    primary_subject_id,
                    secondary_subject_id);
                return GS1_STATUS_OK;
            }

            item_id = harvest_summary_item_id(resolved_harvest_outputs, *harvest_plant_def);
        }

        action_state.current_action_id = action_id;
        action_state.has_current_action_id = true;
        action_state.action_kind = action_kind;
        action_state.target_tile = target_tile;
        action_state.has_target_tile = true;
        action_state.has_approach_tile = false;
        action_state.approach_tile = TileCoord {};
        action_state.primary_subject_id = primary_subject_id;
        action_state.secondary_subject_id =
            action_kind == ActionKind::Craft && craft_recipe != nullptr
            ? craft_recipe->recipe_id.value
            : secondary_subject_id;
        action_state.item_id = action_kind == ActionKind::Repair
            ? repair_tool_id.value
            : item_id;
        action_state.quantity =
            action_kind == ActionKind::Craft
            ? 1U
            : requested_quantity;
        action_state.request_flags = request_flags;
        action_state.reactivate_placement_mode_on_completion =
            reactivate_plant_placement_mode_on_completion;
        action_state.total_action_minutes = duration_minutes;
        action_state.remaining_action_minutes = duration_minutes;
        action_state.resolved_harvest_outputs = std::move(resolved_harvest_outputs);
        action_state.resolved_harvest_density = resolved_harvest_density;
        action_state.resolved_harvest_outputs_valid = action_kind == ActionKind::Harvest;
        action_state.has_started_at_world_minute = false;
        action_state.started_at_world_minute = 0.0;
        action_state.awaiting_placement_reservation =
            should_request_placement_reservation(action_kind);
        populate_reserved_input_items(action_state);

        if (action_requires_worker_approach(action_kind))
        {
            const auto worker = world.read_worker();
            const auto approach_tile =
                world.read_site_world() != nullptr
                    ? resolve_action_approach_tile(
                          *world.read_site_world(),
                          worker,
                          target_tile,
                          action_target_supports_interaction_range(invocation, target_tile))
                    : std::optional<TileCoord> {};
            if (!approach_tile.has_value())
            {
                clear_action_state(action_state);
                emit_site_action_failed(
                    invocation.game_message_queue(),
                    0U,
                    action_kind,
                    SiteActionFailureReason::InvalidTarget,
                    request_flags,
                    target_tile,
                    primary_subject_id,
                    secondary_subject_id);
                return GS1_STATUS_OK;
            }

        action_state.approach_tile = *approach_tile;
        action_state.has_approach_tile = true;
        }

        if (action_state.awaiting_placement_reservation)
        {
            emit_placement_reservation_requested(
                invocation.game_message_queue(),
                action_id.value,
                action_kind,
                target_tile,
                placement_reservation_subject_kind(
                    action_kind,
                    action_state.item_id),
                placement_reservation_subject_id(
                    action_kind,
                    action_state.item_id,
                    primary_subject_id));
        }
        else if (!action_requires_worker_approach(action_kind) ||
            worker_is_at_action_approach_tile(invocation, action_state))
        {
            begin_action_execution(invocation, action_state);
        }

        return GS1_STATUS_OK;
    }

Gs1Status handle_cancel_site_action(
    RuntimeInvocation& invocation,
    const CancelSiteActionMessage& payload)
{
    auto world = SiteWorldAccess<ActionExecutionSystem> {invocation};
    auto action_state = world.own_action();
        const bool cancels_current =
            (payload.flags & GS1_SITE_ACTION_CANCEL_FLAG_CURRENT_ACTION) != 0U;
        const bool cancels_placement_mode =
            (payload.flags & GS1_SITE_ACTION_CANCEL_FLAG_PLACEMENT_MODE) != 0U;

        if (!has_active_action(action_state))
        {
            if (has_active_placement_mode(action_state) &&
                (cancels_current || cancels_placement_mode))
            {
                clear_placement_mode(action_state.placement_mode);
            }
            return GS1_STATUS_OK;
        }

        const bool matches_id =
            action_state.has_current_action_id &&
            payload.action_id == action_state.current_action_id.value;

        if (!cancels_current && !matches_id)
        {
            return GS1_STATUS_OK;
        }

        emit_placement_reservation_released(invocation.game_message_queue(), action_state);
        emit_site_action_failed(
            invocation.game_message_queue(),
            action_id_value(action_state),
            action_state.action_kind,
            SiteActionFailureReason::Cancelled,
            static_cast<std::uint16_t>(payload.flags),
            action_target_tile(action_state),
            0U,
            0U);

        clear_action_state(action_state);
        return GS1_STATUS_OK;
    }

Gs1Status handle_placement_mode_cursor_moved(
    RuntimeInvocation& invocation,
    const PlacementModeCursorMovedMessage& payload)
{
    auto world = SiteWorldAccess<ActionExecutionSystem> {invocation};
    auto action_state = world.own_action();
        if (!has_active_placement_mode(action_state))
        {
            return GS1_STATUS_OK;
        }

        const TileCoord target_tile {payload.tile_x, payload.tile_y};
        update_placement_mode_preview(
            invocation,
            action_state.placement_mode,
            target_tile);
        return GS1_STATUS_OK;
}

const char* ActionExecutionSystem::name() const noexcept
{
    return "ActionExecutionSystem";
}

GameMessageSubscriptionSpan ActionExecutionSystem::subscribed_game_messages() const noexcept
{
    static constexpr GameMessageType subscriptions[] = {
        GameMessageType::StartSiteAction,
        GameMessageType::CancelSiteAction,
        GameMessageType::PlacementModeCursorMoved,
        GameMessageType::PlacementReservationAccepted,
        GameMessageType::PlacementReservationRejected,
    };
    return subscriptions;
}

HostMessageSubscriptionSpan ActionExecutionSystem::subscribed_host_messages() const noexcept
{
    static constexpr Gs1HostMessageType subscriptions[] = {
        GS1_HOST_EVENT_SITE_ACTION_REQUEST,
        GS1_HOST_EVENT_SITE_ACTION_CANCEL,
        GS1_HOST_EVENT_SITE_CONTEXT_REQUEST,
    };
    return subscriptions;
}

std::optional<Gs1RuntimeProfileSystemId> ActionExecutionSystem::profile_system_id() const noexcept
{
    return GS1_RUNTIME_PROFILE_SYSTEM_ACTION_EXECUTION;
}

std::optional<std::uint32_t> ActionExecutionSystem::fixed_step_order() const noexcept
{
    return 5U;
}

Gs1Status ActionExecutionSystem::process_game_message(
    RuntimeInvocation& invocation,
    const GameMessage& message)
{
    auto access = make_game_state_access<ActionExecutionSystem>(invocation);
    auto world = SiteWorldAccess<ActionExecutionSystem> {invocation};
    if (!runtime_invocation_has_campaign(invocation) || !world.has_world())
    {
        return GS1_STATUS_INVALID_STATE;
    }
    auto action_state = world.own_action();
    const auto& payload_type = message.type;
    switch (payload_type)
    {
    case GameMessageType::StartSiteAction:
        return handle_start_site_action(invocation, message.payload_as<StartSiteActionMessage>());

    case GameMessageType::CancelSiteAction:
        return handle_cancel_site_action(invocation, message.payload_as<CancelSiteActionMessage>());

    case GameMessageType::PlacementModeCursorMoved:
        return handle_placement_mode_cursor_moved(invocation, message.payload_as<PlacementModeCursorMovedMessage>());

    case GameMessageType::PlacementReservationAccepted:
    {
        if (!is_action_waiting_for_placement(action_state))
        {
            return GS1_STATUS_OK;
        }

        const auto& payload = message.payload_as<PlacementReservationAcceptedMessage>();
        if (payload.action_id != action_id_value(action_state))
        {
            return GS1_STATUS_OK;
        }

        action_state.awaiting_placement_reservation = false;
        action_state.placement_reservation_token = payload.reservation_token;
        if (!action_requires_worker_approach(action_state.action_kind) ||
            worker_is_at_action_approach_tile(invocation, action_state))
        {
            begin_action_execution(invocation, action_state);
        }
        else
        {
        }
        return GS1_STATUS_OK;
    }

    case GameMessageType::PlacementReservationRejected:
    {
        if (!is_action_waiting_for_placement(action_state))
        {
            return GS1_STATUS_OK;
        }

        const auto& payload = message.payload_as<PlacementReservationRejectedMessage>();
        if (payload.action_id != action_id_value(action_state))
        {
            return GS1_STATUS_OK;
        }

        emit_site_action_failed(
            invocation.game_message_queue(),
            action_id_value(action_state),
            action_state.action_kind,
            SiteActionFailureReason::InvalidTarget,
            action_state.request_flags,
            action_target_tile(action_state),
            action_state.primary_subject_id,
            action_state.secondary_subject_id);
        clear_action_state(action_state);
        return GS1_STATUS_OK;
    }

    default:
        return GS1_STATUS_OK;
    }
}

Gs1Status ActionExecutionSystem::process_host_message(
    RuntimeInvocation& invocation,
    const Gs1HostMessage& message)
{
    auto access = make_game_state_access<ActionExecutionSystem>(invocation);
    auto world = SiteWorldAccess<ActionExecutionSystem> {invocation};
    if (!runtime_invocation_has_campaign(invocation) || !world.has_world())
    {
        return GS1_STATUS_INVALID_STATE;
    }

    switch (message.type)
    {
    case GS1_HOST_EVENT_SITE_ACTION_REQUEST:
        if (message.payload.site_action_request.action_kind == GS1_SITE_ACTION_NONE)
        {
            return GS1_STATUS_INVALID_ARGUMENT;
        }
        return handle_start_site_action(
            invocation,
            StartSiteActionMessage {
                message.payload.site_action_request.action_kind,
                message.payload.site_action_request.flags,
                message.payload.site_action_request.quantity == 0U
                    ? 1U
                    : message.payload.site_action_request.quantity,
                message.payload.site_action_request.target_tile_x,
                message.payload.site_action_request.target_tile_y,
                message.payload.site_action_request.primary_subject_id,
                message.payload.site_action_request.secondary_subject_id,
                message.payload.site_action_request.item_id});

    case GS1_HOST_EVENT_SITE_ACTION_CANCEL:
        if (message.payload.site_action_cancel.action_id == 0U &&
            (message.payload.site_action_cancel.flags &
                (GS1_SITE_ACTION_CANCEL_FLAG_CURRENT_ACTION |
                    GS1_SITE_ACTION_CANCEL_FLAG_PLACEMENT_MODE)) == 0U)
        {
            return GS1_STATUS_INVALID_ARGUMENT;
        }
        return handle_cancel_site_action(
            invocation,
            CancelSiteActionMessage {
                message.payload.site_action_cancel.action_id,
                message.payload.site_action_cancel.flags});

    case GS1_HOST_EVENT_SITE_CONTEXT_REQUEST:
        if (!world.read_action().placement_mode.active)
        {
            return GS1_STATUS_OK;
        }
        return handle_placement_mode_cursor_moved(
            invocation,
            PlacementModeCursorMovedMessage {
                message.payload.site_context_request.tile_x,
                message.payload.site_context_request.tile_y,
                message.payload.site_context_request.flags});

    default:
        return GS1_STATUS_OK;
    }
}

void ActionExecutionSystem::run(RuntimeInvocation& invocation)
{
    auto access = make_game_state_access<ActionExecutionSystem>(invocation);
    auto world = SiteWorldAccess<ActionExecutionSystem> {invocation};
    if (!runtime_invocation_has_campaign(invocation) || !world.has_world())
    {
        return;
    }
    auto action_state = world.own_action();
    if (is_action_waiting_for_worker_approach(action_state) &&
        worker_is_at_action_approach_tile(invocation, action_state))
    {
        begin_action_execution(invocation, action_state);
    }

    if (!is_action_in_progress(action_state))
    {
        return;
    }

    action_state.remaining_action_minutes =
        std::max(
            0.0,
            action_state.remaining_action_minutes -
                action_elapsed_minutes_for_step(invocation, action_state));

    if (action_state.remaining_action_minutes > 0.0)
    {
        return;
    }

    if (action_state.action_kind == ActionKind::Craft)
    {
        const auto failure_reason = validate_craft_completion(invocation, action_state);
        if (failure_reason != SiteActionFailureReason::None)
        {
            emit_site_action_failed(
                invocation.game_message_queue(),
                action_id_value(action_state),
                action_state.action_kind,
                failure_reason,
                action_state.request_flags,
                action_target_tile(action_state),
                action_state.primary_subject_id,
                action_state.secondary_subject_id);
            emit_placement_reservation_released(invocation.game_message_queue(), action_state);
            clear_action_state(action_state);
            return;
        }
    }
    else if (action_state.action_kind == ActionKind::Repair)
    {
        const auto failure_reason = validate_repair_completion(invocation, action_state);
        if (failure_reason != SiteActionFailureReason::None)
        {
            emit_site_action_failed(
                invocation.game_message_queue(),
                action_id_value(action_state),
                action_state.action_kind,
                failure_reason,
                action_state.request_flags,
                action_target_tile(action_state),
                action_state.primary_subject_id,
                action_state.secondary_subject_id);
            emit_placement_reservation_released(invocation.game_message_queue(), action_state);
            clear_action_state(action_state);
            return;
        }
    }
    else if (action_state.action_kind == ActionKind::Harvest)
    {
        const auto failure_reason = validate_harvest_completion(invocation, action_state);
        if (failure_reason != SiteActionFailureReason::None)
        {
            emit_site_action_failed(
                invocation.game_message_queue(),
                action_id_value(action_state),
                action_state.action_kind,
                failure_reason,
                action_state.request_flags,
                action_target_tile(action_state),
                action_state.primary_subject_id,
                action_state.secondary_subject_id);
            emit_placement_reservation_released(invocation.game_message_queue(), action_state);
            clear_action_state(action_state);
            return;
        }

    if (action_state.has_target_tile)
    {
        action_state.secondary_subject_id = static_cast<std::uint32_t>(std::lround(
            world.read_tile(action_state.target_tile).ecology.plant_density * 100.0f));
    }
    }
    else if (action_state.action_kind == ActionKind::Excavate)
    {
        const auto failure_reason = validate_excavation_completion(invocation, action_state);
        if (failure_reason != SiteActionFailureReason::None)
        {
            emit_site_action_failed(
                invocation.game_message_queue(),
                action_id_value(action_state),
                action_state.action_kind,
                failure_reason,
                action_state.request_flags,
                action_target_tile(action_state),
                action_state.primary_subject_id,
                action_state.secondary_subject_id);
            emit_placement_reservation_released(invocation.game_message_queue(), action_state);
            clear_action_state(action_state);
            return;
        }
    }

    const auto item_failure_reason = validate_reserved_item_completion(invocation, action_state);
    if (item_failure_reason != SiteActionFailureReason::None)
    {
        emit_site_action_failed(
            invocation.game_message_queue(),
            action_id_value(action_state),
            action_state.action_kind,
            item_failure_reason,
            action_state.request_flags,
            action_target_tile(action_state),
            action_state.primary_subject_id,
            action_state.secondary_subject_id);
        emit_placement_reservation_released(invocation.game_message_queue(), action_state);
        clear_action_state(action_state);
        return;
    }

    emit_site_action_completed(invocation.game_message_queue(), action_state);
    emit_deferred_worker_meter_cost_request(invocation.game_message_queue(), action_state);
    emit_action_fact_messages(invocation, action_state);
    emit_placement_reservation_released(invocation.game_message_queue(), action_state);
    const bool reactivated_placement_mode =
        should_reactivate_plant_placement_mode_after_completion(
            invocation,
            action_state);
    const ActionKind completed_action_kind = action_state.action_kind;
    const auto completed_target_tile = action_state.target_tile;
    if (reactivated_placement_mode)
    {
        reactivate_plant_placement_mode_from_completed_action(action_state);
    }
    clear_action_state(action_state);
    (void)completed_action_kind;
    (void)completed_target_tile;
    (void)reactivated_placement_mode;
}
}  // namespace gs1

