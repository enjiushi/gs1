#include "content/defs/craft_recipe_defs.h"
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

namespace gs1
{
const GameplayTuningDef& gameplay_tuning_def() noexcept
{
    return prototype_content_database().gameplay_tuning;
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

std::span<const ModifierPresetDef> all_nearby_aura_modifier_presets() noexcept
{
    return prototype_content_database().nearby_aura_modifier_presets;
}

std::span<const ModifierPresetDef> all_run_modifier_presets() noexcept
{
    return prototype_content_database().run_modifier_presets;
}

ModifierChannelTotals resolve_nearby_aura_modifier_preset(ModifierId id) noexcept
{
    const auto presets = all_nearby_aura_modifier_presets();
    if (id.value == 0U || presets.empty())
    {
        return {};
    }

    const auto bucket = static_cast<std::size_t>(id.value) % presets.size();
    return presets[bucket].totals;
}

ModifierChannelTotals resolve_run_modifier_preset(ModifierId id) noexcept
{
    const auto presets = all_run_modifier_presets();
    if (id.value == 0U || presets.empty())
    {
        return {};
    }

    const auto bucket = static_cast<std::size_t>(id.value) % presets.size();
    return presets[bucket].totals;
}

std::span<const TechnologyTierDef> all_technology_tier_defs() noexcept
{
    return prototype_content_database().technology_tier_defs;
}

std::span<const FactionTechnologyTierDef> all_faction_technology_tier_defs() noexcept
{
    return prototype_content_database().faction_technology_tier_defs;
}

std::span<const TotalReputationTierDef> all_total_reputation_tier_defs() noexcept
{
    return prototype_content_database().total_reputation_tier_defs;
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

const FactionTechnologyTierDef* find_faction_technology_tier_def(
    FactionId faction_id,
    std::uint8_t tier_index) noexcept
{
    for (const auto& tier_def : all_faction_technology_tier_defs())
    {
        if (tier_def.faction_id == faction_id && tier_def.tier_index == tier_index)
        {
            return &tier_def;
        }
    }

    return nullptr;
}

const TotalReputationTierDef* find_total_reputation_tier_def(std::uint8_t tier_index) noexcept
{
    for (const auto& tier_def : all_total_reputation_tier_defs())
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
}  // namespace gs1
