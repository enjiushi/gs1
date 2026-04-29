#include "campaign/systems/technology_system.h"

#include "support/currency.h"

#include <algorithm>

namespace gs1
{
namespace
{
const FactionProgressState* find_faction_progress(
    const CampaignState& campaign,
    FactionId faction_id) noexcept
{
    for (const auto& faction_progress : campaign.faction_progress)
    {
        if (faction_progress.faction_id == faction_id)
        {
            return &faction_progress;
        }
    }

    return nullptr;
}
[[nodiscard]] bool recipe_is_baseline_unlocked(RecipeId recipe_id) noexcept
{
    switch (recipe_id.value)
    {
    case k_recipe_cook_food_pack:
    case k_recipe_cook_ephedra_stew:
        return true;
    default:
        return false;
    }
}

[[nodiscard]] bool structure_recipe_is_baseline_unlocked(StructureId structure_id) noexcept
{
    switch (structure_id.value)
    {
    case k_structure_camp_stove:
    case k_structure_storage_crate:
        return true;
    default:
        return false;
    }
}

[[nodiscard]] bool has_reputation_unlock(
    ReputationUnlockKind unlock_kind,
    std::uint32_t content_id) noexcept
{
    for (const auto& unlock_def : all_reputation_unlock_defs())
    {
        if (unlock_def.unlock_kind == unlock_kind &&
            unlock_def.content_id == content_id)
        {
            return true;
        }
    }

    return false;
}

[[nodiscard]] bool reputation_unlock_ready(
    const CampaignState& campaign,
    ReputationUnlockKind unlock_kind,
    std::uint32_t content_id) noexcept
{
    for (const auto& unlock_def : all_reputation_unlock_defs())
    {
        if (unlock_def.unlock_kind != unlock_kind ||
            unlock_def.content_id != content_id)
        {
            continue;
        }

        if (campaign.technology_state.total_reputation >= unlock_def.reputation_requirement)
        {
            return true;
        }
    }

    return false;
}
}  // namespace

bool TechnologySystem::subscribes_to(GameMessageType type) noexcept
{
    return type == GameMessageType::CampaignReputationAwardRequested;
}

bool TechnologySystem::node_purchased(
    const CampaignState& campaign,
    TechNodeId tech_node_id) noexcept
{
    const auto* node_def = find_technology_node_def(tech_node_id);
    if (node_def == nullptr)
    {
        return false;
    }

    return faction_reputation(campaign, node_def->faction_id) >=
        current_reputation_requirement(*node_def);
}

bool TechnologySystem::technology_tier_visible(
    const CampaignState& campaign,
    const TechnologyNodeDef& node_def) noexcept
{
    (void)campaign;
    (void)node_def;
    return true;
}

bool TechnologySystem::plant_unlocked(
    const CampaignState& campaign,
    PlantId plant_id) noexcept
{
    if (plant_id.value == 0U)
    {
        return false;
    }

    for (const auto starter_plant_id : all_initial_unlocked_plant_ids())
    {
        if (starter_plant_id == plant_id)
        {
            return true;
        }
    }

    return reputation_unlock_ready(campaign, ReputationUnlockKind::Plant, plant_id.value);
}

bool TechnologySystem::item_unlocked(
    const CampaignState& campaign,
    ItemId item_id) noexcept
{
    if (item_id.value == 0U)
    {
        return false;
    }

    const auto* item_def = find_item_def(item_id);
    if (item_def == nullptr)
    {
        return false;
    }

    if (item_def->linked_plant_id.value != 0U &&
        !plant_unlocked(campaign, item_def->linked_plant_id))
    {
        return false;
    }

    if (item_def->linked_structure_id.value != 0U &&
        !structure_recipe_unlocked(campaign, item_def->linked_structure_id))
    {
        return false;
    }

    if (!has_reputation_unlock(ReputationUnlockKind::Item, item_id.value))
    {
        return true;
    }

    return reputation_unlock_ready(campaign, ReputationUnlockKind::Item, item_id.value);
}

bool TechnologySystem::structure_recipe_unlocked(
    const CampaignState& campaign,
    StructureId structure_id) noexcept
{
    if (structure_id.value == 0U)
    {
        return false;
    }

    if (structure_recipe_is_baseline_unlocked(structure_id))
    {
        return true;
    }

    return reputation_unlock_ready(
        campaign,
        ReputationUnlockKind::StructureRecipe,
        structure_id.value);
}

bool TechnologySystem::recipe_unlocked(
    const CampaignState& campaign,
    RecipeId recipe_id) noexcept
{
    if (recipe_id.value == 0U)
    {
        return false;
    }

    if (recipe_is_baseline_unlocked(recipe_id))
    {
        return true;
    }

    if (reputation_unlock_ready(campaign, ReputationUnlockKind::Recipe, recipe_id.value))
    {
        return true;
    }

    const auto* recipe_def = find_craft_recipe_def(recipe_id);
    if (recipe_def != nullptr)
    {
        const auto* output_item_def = find_item_def(recipe_def->output_item_id);
        if (output_item_def != nullptr &&
            output_item_def->linked_structure_id.value != 0U &&
            structure_recipe_unlocked(campaign, output_item_def->linked_structure_id))
        {
            return true;
        }
    }

    for (const auto& node_def : all_technology_node_defs())
    {
        if (node_def.granted_content_kind == TechnologyGrantedContentKind::Recipe &&
            node_def.granted_content_id == recipe_id.value &&
            node_purchased(campaign, node_def.tech_node_id))
        {
            return true;
        }
    }

    return false;
}

bool TechnologySystem::craft_output_unlocked(
    const CampaignState& campaign,
    ItemId item_id) noexcept
{
    if (item_id.value == 0U)
    {
        return false;
    }

    for (const auto& recipe_def : all_craft_recipe_defs())
    {
        if (recipe_def.output_item_id == item_id &&
            recipe_unlocked(campaign, recipe_def.recipe_id))
        {
            return true;
        }
    }

    return false;
}

std::int32_t TechnologySystem::faction_reputation(
    const CampaignState& campaign,
    FactionId faction_id) noexcept
{
    const auto* faction_progress = find_faction_progress(campaign, faction_id);
    return faction_progress == nullptr ? 0 : std::max(0, faction_progress->faction_reputation);
}

std::int32_t TechnologySystem::current_reputation_requirement(
    const TechnologyNodeDef& node_def) noexcept
{
    return std::max(0, node_def.reputation_requirement);
}

std::uint32_t TechnologySystem::current_internal_cost_cash_points(
    const TechnologyNodeDef& node_def) noexcept
{
    return node_def.internal_cost_cash_points;
}

std::int32_t TechnologySystem::current_cash_cost(
    const CampaignState& /*campaign*/,
    const TechnologyNodeDef& node_def) noexcept
{
    return cash_from_cash_points(current_internal_cost_cash_points(node_def));
}

float TechnologySystem::current_effect_parameter(
    const CampaignState& campaign,
    const TechnologyNodeDef& node_def) noexcept
{
    const auto bonus_reputation = std::max(
        0,
        faction_reputation(campaign, node_def.faction_id) - current_reputation_requirement(node_def));
    const auto parameter =
        node_def.unlock_effect_parameter +
        static_cast<float>(bonus_reputation) * node_def.effect_parameter_per_bonus_reputation;
    return std::max(0.0f, parameter);
}

bool TechnologySystem::node_claimable(
    const CampaignState& /*campaign*/,
    const TechnologyNodeDef& /*node_def*/) noexcept
{
    return false;
}

Gs1Status TechnologySystem::process_message(
    CampaignSystemContext& context,
    const GameMessage& message)
{
    switch (message.type)
    {
    case GameMessageType::CampaignReputationAwardRequested:
    {
        const auto& payload = message.payload_as<CampaignReputationAwardRequestedMessage>();
        if (payload.delta <= 0)
        {
            return GS1_STATUS_OK;
        }

        context.campaign.technology_state.total_reputation += payload.delta;
        return GS1_STATUS_OK;
    }

    default:
        return GS1_STATUS_OK;
    }
}
}  // namespace gs1
