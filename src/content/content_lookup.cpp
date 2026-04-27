#include "content/defs/craft_recipe_defs.h"
#include "content/defs/excavation_defs.h"
#include "content/defs/gameplay_tuning_defs.h"
#include "content/defs/item_defs.h"
#include "content/defs/modifier_defs.h"
#include "content/defs/plant_defs.h"
#include "content/defs/reward_defs.h"
#include "content/defs/structure_defs.h"
#include "content/defs/task_defs.h"
#include "content/defs/technology_defs.h"
#include "content/content_loader.h"
#include "site/defs/site_action_defs.h"

#include <cmath>

namespace gs1
{
namespace
{
constexpr std::uint32_t k_timed_buff_internal_cash_points = 200U;

std::uint32_t round_to_cash_points(double value) noexcept
{
    return value <= 0.0
        ? 0U
        : static_cast<std::uint32_t>(std::lround(value));
}

std::uint32_t truncate_to_cash_points(double value) noexcept
{
    return value <= 0.0
        ? 0U
        : static_cast<std::uint32_t>(std::floor(value + 0.0001));
}
}  // namespace

const GameplayTuningDef& gameplay_tuning_def() noexcept
{
    return prototype_content_database().gameplay_tuning;
}

double player_meter_cash_point_value(
    float health_delta,
    float hydration_delta,
    float nourishment_delta,
    float energy_delta,
    float morale_delta) noexcept
{
    const auto& tuning = gameplay_tuning_def().player_meter_cash_points;
    return static_cast<double>(health_delta) * static_cast<double>(tuning.health_per_point) +
        static_cast<double>(hydration_delta) * static_cast<double>(tuning.hydration_per_point) +
        static_cast<double>(nourishment_delta) * static_cast<double>(tuning.nourishment_per_point) +
        static_cast<double>(energy_delta) * static_cast<double>(tuning.energy_per_point) +
        static_cast<double>(morale_delta) * static_cast<double>(tuning.morale_per_point);
}

std::uint32_t player_meter_gain_internal_cash_points(
    float health_delta,
    float hydration_delta,
    float nourishment_delta,
    float energy_delta,
    float morale_delta) noexcept
{
    return round_to_cash_points(
        player_meter_cash_point_value(
            health_delta,
            hydration_delta,
            nourishment_delta,
            energy_delta,
            morale_delta));
}

std::uint32_t player_meter_cost_internal_cash_points(
    float health_cost,
    float hydration_cost,
    float nourishment_cost,
    float energy_cost,
    float morale_cost) noexcept
{
    return round_to_cash_points(
        player_meter_cash_point_value(
            health_cost,
            hydration_cost,
            nourishment_cost,
            energy_cost,
            morale_cost));
}

std::span<const ItemDef> all_item_defs() noexcept
{
    return prototype_content_database().item_defs;
}

const ItemDef* find_item_def(ItemId item_id) noexcept
{
    const auto& content = prototype_content_database();
    const auto it = content.index.item_by_id.find(item_id.value);
    return it == content.index.item_by_id.end() ? nullptr : &content.item_defs[it->second];
}

std::uint32_t item_stack_size(ItemId item_id) noexcept
{
    const auto* item_def = find_item_def(item_id);
    return item_def == nullptr ? 1U : static_cast<std::uint32_t>(item_def->stack_size);
}

std::uint32_t item_internal_price_cash_points(ItemId item_id) noexcept
{
    const auto* item_def = find_item_def(item_id);
    if (item_def == nullptr)
    {
        return 0U;
    }

    const auto derived_cash_points = player_meter_gain_internal_cash_points(
        item_def->health_delta,
        item_def->hydration_delta,
        item_def->nourishment_delta,
        item_def->energy_delta,
        item_def->morale_delta);
    const auto base_cash_points =
        derived_cash_points != 0U ? derived_cash_points : item_def->internal_price_cash_points;
    const auto* modifier_def = find_modifier_def(item_def->modifier_id);
    return modifier_def == nullptr || modifier_def->duration_eight_hour_blocks == 0U
        ? base_cash_points
        : base_cash_points + k_timed_buff_internal_cash_points;
}

std::uint32_t item_buy_price_cash_points(ItemId item_id) noexcept
{
    const auto* item_def = find_item_def(item_id);
    if (item_def == nullptr)
    {
        return 0U;
    }

    if (item_def->source_rule != ItemSourceRule::BuyOnly &&
        item_def->source_rule != ItemSourceRule::BuyOrCraft)
    {
        return 0U;
    }

    const auto base_cash_points = item_internal_price_cash_points(item_id);
    const auto multiplier =
        static_cast<double>(gameplay_tuning_def().player_meter_cash_points.buy_price_multiplier);
    return truncate_to_cash_points(static_cast<double>(base_cash_points) * multiplier);
}

std::uint32_t item_sell_price_cash_points(ItemId item_id) noexcept
{
    const auto* item_def = find_item_def(item_id);
    if (item_def == nullptr || !item_has_capability(*item_def, ITEM_CAPABILITY_SELL))
    {
        return 0U;
    }

    const auto base_cash_points = item_internal_price_cash_points(item_id);
    const auto multiplier =
        static_cast<double>(gameplay_tuning_def().player_meter_cash_points.sell_price_multiplier);
    return truncate_to_cash_points(static_cast<double>(base_cash_points) * multiplier);
}

std::span<const PlantDef> all_plant_defs() noexcept
{
    return prototype_content_database().plant_defs;
}

const PlantDef* find_plant_def(PlantId plant_id) noexcept
{
    const auto& content = prototype_content_database();
    const auto it = content.index.plant_by_id.find(plant_id.value);
    return it == content.index.plant_by_id.end() ? nullptr : &content.plant_defs[it->second];
}

std::span<const StructureDef> all_structure_defs() noexcept
{
    return prototype_content_database().structure_defs;
}

const StructureDef* find_structure_def(StructureId structure_id) noexcept
{
    const auto& content = prototype_content_database();
    const auto it = content.index.structure_by_id.find(structure_id.value);
    return it == content.index.structure_by_id.end() ? nullptr : &content.structure_defs[it->second];
}

bool structure_has_storage(StructureId structure_id) noexcept
{
    const auto* structure_def = find_structure_def(structure_id);
    return structure_def != nullptr && structure_def->grants_storage && structure_def->storage_slot_count > 0U;
}

bool structure_is_crafting_station(StructureId structure_id) noexcept
{
    const auto* structure_def = find_structure_def(structure_id);
    return structure_def != nullptr &&
        structure_def->crafting_station_kind != CraftingStationKind::None;
}

std::span<const CraftRecipeDef> all_craft_recipe_defs() noexcept
{
    return prototype_content_database().craft_recipe_defs;
}

std::span<const ExcavationDepthDef> all_excavation_depth_defs() noexcept
{
    return prototype_content_database().excavation_depth_defs;
}

const ExcavationDepthDef* find_excavation_depth_def(ExcavationDepth depth) noexcept
{
    for (const auto& depth_def : all_excavation_depth_defs())
    {
        if (depth_def.depth == depth)
        {
            return &depth_def;
        }
    }

    return nullptr;
}

std::span<const ExcavationLootEntryDef> all_excavation_loot_entry_defs() noexcept
{
    return prototype_content_database().excavation_loot_entry_defs;
}

const CraftRecipeDef* find_craft_recipe_def(
    StructureId station_structure_id,
    ItemId output_item_id) noexcept
{
    for (const auto& recipe_def : all_craft_recipe_defs())
    {
        if (recipe_def.station_structure_id == station_structure_id &&
            recipe_def.output_item_id == output_item_id)
        {
            return &recipe_def;
        }
    }

    return nullptr;
}

const CraftRecipeDef* find_craft_recipe_def(RecipeId recipe_id) noexcept
{
    const auto& content = prototype_content_database();
    const auto it = content.index.craft_recipe_by_id.find(recipe_id.value);
    return it == content.index.craft_recipe_by_id.end()
        ? nullptr
        : &content.craft_recipe_defs[it->second];
}

std::span<const TaskTemplateDef> all_task_template_defs() noexcept
{
    return prototype_content_database().task_template_defs;
}

const TaskTemplateDef* find_task_template_def(TaskTemplateId task_template_id) noexcept
{
    const auto& content = prototype_content_database();
    const auto it = content.index.task_template_by_id.find(task_template_id.value);
    return it == content.index.task_template_by_id.end()
        ? nullptr
        : &content.task_template_defs[it->second];
}

std::span<const SiteOnboardingTaskSeedDef> all_site_onboarding_task_seed_defs() noexcept
{
    return prototype_content_database().site_onboarding_task_seed_defs;
}

std::span<const RewardCandidateDef> all_reward_candidate_defs() noexcept
{
    return prototype_content_database().reward_candidate_defs;
}

const RewardCandidateDef* find_reward_candidate_def(RewardCandidateId reward_candidate_id) noexcept
{
    const auto& content = prototype_content_database();
    const auto it = content.index.reward_candidate_by_id.find(reward_candidate_id.value);
    return it == content.index.reward_candidate_by_id.end()
        ? nullptr
        : &content.reward_candidate_defs[it->second];
}

std::span<const SiteActionDef> all_site_action_defs() noexcept
{
    return prototype_content_database().site_action_defs;
}

const SiteActionDef* find_site_action_def(ActionKind action_kind) noexcept
{
    const auto& content = prototype_content_database();
    const auto it = content.index.site_action_by_kind.find(static_cast<std::uint32_t>(action_kind));
    return it == content.index.site_action_by_kind.end()
        ? nullptr
        : &content.site_action_defs[it->second];
}

std::span<const ModifierDef> all_modifier_defs() noexcept
{
    return prototype_content_database().modifier_defs;
}

const ModifierDef* find_modifier_def(ModifierId id) noexcept
{
    const auto& content = prototype_content_database();
    const auto it = content.index.modifier_by_id.find(id.value);
    return it == content.index.modifier_by_id.end()
        ? nullptr
        : &content.modifier_defs[it->second];
}

ModifierChannelTotals resolve_nearby_aura_modifier_preset(ModifierId id) noexcept
{
    const auto* modifier_def = find_modifier_def(id);
    if (modifier_def == nullptr ||
        modifier_def->preset_kind != ModifierPresetKind::NearbyAura)
    {
        return {};
    }

    return modifier_def->totals;
}

ModifierChannelTotals resolve_run_modifier_preset(ModifierId id) noexcept
{
    const auto* modifier_def = find_modifier_def(id);
    if (modifier_def == nullptr ||
        modifier_def->preset_kind != ModifierPresetKind::RunModifier)
    {
        return {};
    }

    return modifier_def->totals;
}

std::span<const TechnologyTierDef> all_technology_tier_defs() noexcept
{
    return prototype_content_database().technology_tier_defs;
}

std::span<const ReputationUnlockDef> all_reputation_unlock_defs() noexcept
{
    return prototype_content_database().reputation_unlock_defs;
}

std::span<const TechnologyNodeDef> all_technology_node_defs() noexcept
{
    return prototype_content_database().technology_node_defs;
}

std::span<const PlantId> all_initial_unlocked_plant_ids() noexcept
{
    return prototype_content_database().initial_unlocked_plant_ids;
}

const TechnologyTierDef* find_technology_tier_def(std::uint8_t tier_index) noexcept
{
    for (const auto& tier_def : all_technology_tier_defs())
    {
        if (tier_def.tier_index == tier_index)
        {
            return &tier_def;
        }
    }

    return nullptr;
}

const ReputationUnlockDef* find_reputation_unlock_def(std::uint32_t unlock_id) noexcept
{
    for (const auto& unlock_def : all_reputation_unlock_defs())
    {
        if (unlock_def.unlock_id == unlock_id)
        {
            return &unlock_def;
        }
    }

    return nullptr;
}

const TechnologyNodeDef* find_technology_node_def(TechNodeId tech_node_id) noexcept
{
    for (const auto& node_def : all_technology_node_defs())
    {
        if (node_def.tech_node_id == tech_node_id)
        {
            return &node_def;
        }
    }

    return nullptr;
}

const TechnologyNodeDef* find_faction_technology_node_def(
    FactionId faction_id,
    std::uint8_t tier_index) noexcept
{
    for (const auto& node_def : all_technology_node_defs())
    {
        if (node_def.faction_id == faction_id &&
            node_def.tier_index == tier_index &&
            node_def.node_kind == TechnologyNodeKind::BaseTech)
        {
            return &node_def;
        }
    }

    return nullptr;
}
}  // namespace gs1
