#include "campaign/systems/technology_system.h"

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

FactionId selected_reputation_faction_id(const CampaignState& campaign) noexcept
{
    return campaign.regional_map_state.selected_tech_tree_faction_id.value != 0U
        ? campaign.regional_map_state.selected_tech_tree_faction_id
        : FactionId {k_faction_village_committee};
}

std::int32_t committed_reputation_for_faction(
    const CampaignState& campaign,
    FactionId faction_id) noexcept
{
    std::int32_t committed_reputation = 0;
    for (const auto& purchase : campaign.technology_state.purchased_nodes)
    {
        if (purchase.reputation_faction_id == faction_id)
        {
            committed_reputation += std::max(0, purchase.reputation_spent);
        }
    }

    return committed_reputation;
}

std::uint8_t highest_purchased_tier_for_faction(
    const CampaignState& campaign,
    FactionId faction_id) noexcept
{
    std::uint8_t highest_tier = 0U;
    for (const auto& purchase : campaign.technology_state.purchased_nodes)
    {
        const auto* node_def = find_technology_node_def(purchase.tech_node_id);
        if (node_def == nullptr || node_def->faction_id != faction_id)
        {
            continue;
        }

        highest_tier = std::max(highest_tier, node_def->tier_index);
    }

    return highest_tier;
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
    if (node_def.tier_index <= 1U)
    {
        return true;
    }

    return node_purchased(
        campaign,
        previous_faction_technology_node_id(node_def.faction_id, node_def.tier_index));
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

std::int32_t TechnologySystem::available_faction_reputation(
    const CampaignState& campaign,
    FactionId faction_id) noexcept
{
    const auto* faction_progress = find_faction_progress(campaign, faction_id);
    if (faction_progress == nullptr)
    {
        return 0;
    }

    return std::max(
        0,
        faction_progress->faction_reputation - committed_reputation_for_faction(campaign, faction_id));
}

std::int32_t TechnologySystem::current_reputation_cost(
    const TechnologyNodeDef& node_def,
    FactionId reputation_faction_id) noexcept
{
    const auto base_cost = std::max(0, node_def.reputation_cost);
    if (reputation_faction_id.value == 0U)
    {
        return std::numeric_limits<std::int32_t>::max();
    }

    if (reputation_faction_id == node_def.faction_id)
    {
        return base_cost;
    }

    return base_cost + (base_cost + 1) / 2;
}

std::int32_t TechnologySystem::current_cash_cost(
    const CampaignState& /*campaign*/,
    const TechnologyNodeDef& node_def) noexcept
{
    return std::max(0, node_def.cash_cost);
}

bool TechnologySystem::node_claimable(
    const CampaignState& campaign,
    const TechnologyNodeDef& node_def,
    FactionId reputation_faction_id) noexcept
{
    if (node_purchased(campaign, node_def.tech_node_id))
    {
        return false;
    }

    if (reputation_faction_id.value == 0U)
    {
        return false;
    }

    if (campaign.cash < current_cash_cost(campaign, node_def))
    {
        return false;
    }

    if (!technology_tier_visible(campaign, node_def))
    {
        return false;
    }

    return available_faction_reputation(campaign, reputation_faction_id) >=
        current_reputation_cost(node_def, reputation_faction_id);
}

bool TechnologySystem::node_refundable(
    const CampaignState& campaign,
    const TechnologyNodeDef& node_def) noexcept
{
    const auto* purchase = find_purchase_record(campaign, node_def.tech_node_id);
    if (purchase == nullptr)
    {
        return false;
    }

    return highest_purchased_tier_for_faction(campaign, node_def.faction_id) == node_def.tier_index;
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

        const auto reputation_faction_id = FactionId {
            payload.reputation_faction_id == 0U
                ? selected_reputation_faction_id(context.campaign).value
                : payload.reputation_faction_id};
        if (!node_claimable(context.campaign, *node_def, reputation_faction_id))
        {
            return GS1_STATUS_INVALID_STATE;
        }

        context.campaign.cash -= current_cash_cost(context.campaign, *node_def);
        context.campaign.technology_state.purchased_nodes.push_back(TechnologyPurchaseRecord {
            node_def->tech_node_id,
            reputation_faction_id,
            current_reputation_cost(*node_def, reputation_faction_id)});
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

        purchases.erase(it);
        return GS1_STATUS_OK;
    }

    default:
        return GS1_STATUS_OK;
    }
}
}  // namespace gs1
