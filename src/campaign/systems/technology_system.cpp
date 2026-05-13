#include "campaign/systems/technology_system.h"

#include "runtime/game_runtime.h"
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
    case k_recipe_craft_hammer:
    case k_recipe_craft_shovel:
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
    case k_structure_workbench:
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

[[nodiscard]] bool has_technology_grant(
    TechnologyGrantedContentKind granted_content_kind,
    std::uint32_t granted_content_id) noexcept
{
    for (const auto& node_def : all_technology_node_defs())
    {
        if (node_def.granted_content_kind == granted_content_kind &&
            node_def.granted_content_id == granted_content_id)
        {
            return true;
        }
    }

    return false;
}

[[nodiscard]] bool technology_grant_unlocked(
    const CampaignState& campaign,
    TechnologyGrantedContentKind granted_content_kind,
    std::uint32_t granted_content_id) noexcept
{
    for (const auto& node_def : all_technology_node_defs())
    {
        if (node_def.granted_content_kind != granted_content_kind ||
            node_def.granted_content_id != granted_content_id)
        {
            continue;
        }

        if (TechnologySystem::node_purchased(campaign, node_def.tech_node_id))
        {
            return true;
        }
    }

    return false;
}

}  // namespace

Gs1Status process_technology_message(
    RuntimeInvocation& invocation,
    const GameMessage& message);

template <>
struct system_state_tags<TechnologySystem>
{
    using type = type_list<RuntimeCampaignTag>;
};

const char* TechnologySystem::name() const noexcept
{
    return "TechnologySystem";
}

GameMessageSubscriptionSpan TechnologySystem::subscribed_game_messages() const noexcept
{
    static constexpr GameMessageType subscriptions[] = {GameMessageType::CampaignReputationAwardRequested};
    return subscriptions;
}

HostMessageSubscriptionSpan TechnologySystem::subscribed_host_messages() const noexcept
{
    static constexpr Gs1HostMessageType subscriptions[] = {GS1_HOST_EVENT_GAMEPLAY_ACTION};
    return subscriptions;
}

std::optional<Gs1RuntimeProfileSystemId> TechnologySystem::profile_system_id() const noexcept
{
    return GS1_RUNTIME_PROFILE_SYSTEM_TECHNOLOGY;
}

std::optional<std::uint32_t> TechnologySystem::fixed_step_order() const noexcept
{
    return std::nullopt;
}

Gs1Status TechnologySystem::process_game_message(
    RuntimeInvocation& invocation,
    const GameMessage& message)
{
    auto access = make_game_state_access<TechnologySystem>(invocation);
    auto& campaign = access.template read<RuntimeCampaignTag>();
    if (!campaign.has_value())
    {
        return GS1_STATUS_INVALID_STATE;
    }

    return process_technology_message(invocation, message);
}

Gs1Status TechnologySystem::process_host_message(
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
    case GS1_GAMEPLAY_ACTION_CLAIM_TECHNOLOGY_NODE:
        if (action.target_id == 0U)
        {
            return GS1_STATUS_INVALID_ARGUMENT;
        }
        gameplay_message.type = GameMessageType::TechnologyNodeClaimRequested;
        gameplay_message.set_payload(TechnologyNodeClaimRequestedMessage {
            action.target_id,
            static_cast<std::uint32_t>(action.arg0)});
        invocation.push_game_message(gameplay_message);
        return GS1_STATUS_OK;

    case GS1_GAMEPLAY_ACTION_REFUND_TECHNOLOGY_NODE:
        if (action.target_id == 0U)
        {
            return GS1_STATUS_INVALID_ARGUMENT;
        }
        gameplay_message.type = GameMessageType::TechnologyNodeRefundRequested;
        gameplay_message.set_payload(TechnologyNodeRefundRequestedMessage {action.target_id});
        invocation.push_game_message(gameplay_message);
        return GS1_STATUS_OK;

    default:
        return GS1_STATUS_OK;
    }
}

void TechnologySystem::run(RuntimeInvocation& invocation)
{
    (void)invocation;
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

    if (has_technology_grant(TechnologyGrantedContentKind::Plant, plant_id.value))
    {
        return technology_grant_unlocked(
            campaign,
            TechnologyGrantedContentKind::Plant,
            plant_id.value);
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

    if (has_technology_grant(TechnologyGrantedContentKind::Item, item_id.value))
    {
        return technology_grant_unlocked(
            campaign,
            TechnologyGrantedContentKind::Item,
            item_id.value);
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

    if (has_technology_grant(TechnologyGrantedContentKind::Structure, structure_id.value))
    {
        return technology_grant_unlocked(
            campaign,
            TechnologyGrantedContentKind::Structure,
            structure_id.value);
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

    if (has_technology_grant(TechnologyGrantedContentKind::Recipe, recipe_id.value))
    {
        return technology_grant_unlocked(
            campaign,
            TechnologyGrantedContentKind::Recipe,
            recipe_id.value);
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

Gs1Status process_technology_message(
    RuntimeInvocation& invocation,
    const GameMessage& message)
{
    switch (message.type)
    {
    case GameMessageType::CampaignReputationAwardRequested:
    {
        auto access = make_game_state_access<TechnologySystem>(invocation);
        auto& campaign = access.template read<RuntimeCampaignTag>();
        if (!campaign.has_value())
        {
            return GS1_STATUS_INVALID_STATE;
        }

        const auto& payload = message.payload_as<CampaignReputationAwardRequestedMessage>();
        if (payload.delta <= 0)
        {
            return GS1_STATUS_OK;
        }

        campaign->technology_state.total_reputation += payload.delta;
        return GS1_STATUS_OK;
    }

    default:
        return GS1_STATUS_OK;
    }
}
}  // namespace gs1
