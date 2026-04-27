#include "campaign/systems/technology_system.h"

#include "support/currency.h"

#include <algorithm>
#include <limits>

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

[[nodiscard]] TechNodeId paired_base_tech_node_id(const TechnologyNodeDef& node_def) noexcept
{
    return TechNodeId {base_technology_node_id(node_def.faction_id, node_def.tier_index)};
}

[[nodiscard]] bool paired_base_node_purchased(
    const CampaignState& campaign,
    const TechnologyNodeDef& node_def) noexcept
{
    if (node_def.node_kind != TechnologyNodeKind::Enhancement)
    {
        return true;
    }

    return TechnologySystem::node_purchased(campaign, paired_base_tech_node_id(node_def));
}

[[nodiscard]] bool sibling_enhancement_purchased(
    const CampaignState& campaign,
    const TechnologyNodeDef& node_def) noexcept
{
    if (node_def.node_kind != TechnologyNodeKind::Enhancement)
    {
        return false;
    }

    for (const auto& purchase : campaign.technology_state.purchased_nodes)
    {
        const auto* purchased_node_def = find_technology_node_def(purchase.tech_node_id);
        if (purchased_node_def == nullptr ||
            purchased_node_def->node_kind != TechnologyNodeKind::Enhancement ||
            purchased_node_def->faction_id != node_def.faction_id ||
            purchased_node_def->tier_index != node_def.tier_index ||
            purchased_node_def->tech_node_id == node_def.tech_node_id)
        {
            continue;
        }

        return true;
    }

    return false;
}

[[nodiscard]] std::int32_t highest_purchased_reputation_requirement(const CampaignState& campaign) noexcept
{
    std::int32_t highest_requirement = 0;
    for (const auto& purchase : campaign.technology_state.purchased_nodes)
    {
        const auto* node_def = find_technology_node_def(purchase.tech_node_id);
        if (node_def != nullptr)
        {
            highest_requirement = std::max(highest_requirement, std::max(0, node_def->reputation_requirement));
        }
    }

    return highest_requirement;
}
}  // namespace

bool TechnologySystem::subscribes_to(GameMessageType type) noexcept
{
    return type == GameMessageType::CampaignCashDeltaRequested ||
        type == GameMessageType::CampaignReputationAwardRequested ||
        type == GameMessageType::TechnologyNodeClaimRequested ||
        type == GameMessageType::TechnologyNodeRefundRequested;
}

bool TechnologySystem::node_purchased(
    const CampaignState& campaign,
    TechNodeId tech_node_id) noexcept
{
    return find_purchase_record(campaign, tech_node_id) != nullptr;
}

const TechnologyPurchaseRecord* TechnologySystem::find_purchase_record(
    const CampaignState& campaign,
    TechNodeId tech_node_id) noexcept
{
    for (const auto& purchase : campaign.technology_state.purchased_nodes)
    {
        if (purchase.tech_node_id == tech_node_id)
        {
            return &purchase;
        }
    }

    return nullptr;
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

    for (const auto& unlock_def : all_reputation_unlock_defs())
    {
        if (unlock_def.unlock_kind != ReputationUnlockKind::Plant ||
            unlock_def.content_id != plant_id.value)
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
    const CampaignState& campaign,
    const TechnologyNodeDef& node_def) noexcept
{
    if (node_purchased(campaign, node_def.tech_node_id))
    {
        return false;
    }

    if (node_def.node_kind == TechnologyNodeKind::Enhancement &&
        find_technology_node_def(paired_base_tech_node_id(node_def)) == nullptr)
    {
        return false;
    }

    if (campaign.cash < static_cast<std::int32_t>(current_internal_cost_cash_points(node_def)))
    {
        return false;
    }

    if (!technology_tier_visible(campaign, node_def))
    {
        return false;
    }

    if (!paired_base_node_purchased(campaign, node_def))
    {
        return false;
    }

    if (sibling_enhancement_purchased(campaign, node_def))
    {
        return false;
    }

    return faction_reputation(campaign, node_def.faction_id) >=
        current_reputation_requirement(node_def);
}

bool TechnologySystem::node_refundable(
    const CampaignState& campaign,
    const TechnologyNodeDef& node_def) noexcept
{
    if (!node_purchased(campaign, node_def.tech_node_id))
    {
        return false;
    }

    return highest_purchased_reputation_requirement(campaign) ==
        current_reputation_requirement(node_def);
}

Gs1Status TechnologySystem::process_message(
    CampaignSystemContext& context,
    const GameMessage& message)
{
    switch (message.type)
    {
    case GameMessageType::CampaignCashDeltaRequested:
    {
        const auto& payload = message.payload_as<CampaignCashDeltaRequestedMessage>();
        const auto updated_cash = static_cast<std::int64_t>(context.campaign.cash) + payload.delta;
        if (updated_cash < 0 ||
            updated_cash > static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::max()))
        {
            return GS1_STATUS_INVALID_STATE;
        }

        context.campaign.cash = static_cast<std::int32_t>(updated_cash);
        return GS1_STATUS_OK;
    }

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

    case GameMessageType::TechnologyNodeClaimRequested:
    {
        const auto& payload = message.payload_as<TechnologyNodeClaimRequestedMessage>();
        const auto* node_def = find_technology_node_def(TechNodeId {payload.tech_node_id});
        if (node_def == nullptr)
        {
            return GS1_STATUS_NOT_FOUND;
        }

        if (!node_claimable(context.campaign, *node_def))
        {
            return GS1_STATUS_INVALID_STATE;
        }

        context.campaign.cash -= static_cast<std::int32_t>(current_internal_cost_cash_points(*node_def));
        context.campaign.technology_state.purchased_nodes.push_back(TechnologyPurchaseRecord {
            node_def->tech_node_id});
        return GS1_STATUS_OK;
    }

    case GameMessageType::TechnologyNodeRefundRequested:
    {
        const auto& payload = message.payload_as<TechnologyNodeRefundRequestedMessage>();
        const auto* node_def = find_technology_node_def(TechNodeId {payload.tech_node_id});
        if (node_def == nullptr)
        {
            return GS1_STATUS_NOT_FOUND;
        }

        if (!node_refundable(context.campaign, *node_def))
        {
            return GS1_STATUS_INVALID_STATE;
        }

        auto& purchases = context.campaign.technology_state.purchased_nodes;
        const auto it = std::find_if(
            purchases.begin(),
            purchases.end(),
            [node_def](const TechnologyPurchaseRecord& purchase) {
                return purchase.tech_node_id == node_def->tech_node_id;
            });
        if (it == purchases.end())
        {
            return GS1_STATUS_INVALID_STATE;
        }

        const auto refunded_cash =
            static_cast<std::int32_t>(current_internal_cost_cash_points(*node_def));
        const auto updated_cash = static_cast<std::int64_t>(context.campaign.cash) + refunded_cash;
        if (updated_cash > static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::max()))
        {
            return GS1_STATUS_INVALID_STATE;
        }

        purchases.erase(it);
        context.campaign.cash = static_cast<std::int32_t>(updated_cash);
        return GS1_STATUS_OK;
    }

    default:
        return GS1_STATUS_OK;
    }
}
}  // namespace gs1
