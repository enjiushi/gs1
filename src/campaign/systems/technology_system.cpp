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

FactionProgressState* find_faction_progress_mut(
    CampaignState& campaign,
    FactionId faction_id) noexcept
{
    for (auto& faction_progress : campaign.faction_progress)
    {
        if (faction_progress.faction_id == faction_id)
        {
            return &faction_progress;
        }
    }

    return nullptr;
}
}  // namespace

bool TechnologySystem::subscribes_to(GameMessageType type) noexcept
{
    return type == GameMessageType::CampaignCashDeltaRequested ||
        type == GameMessageType::CampaignReputationAwardRequested ||
        type == GameMessageType::TechnologyNodeClaimRequested;
}

bool TechnologySystem::node_purchased(
    const CampaignState& campaign,
    TechNodeId tech_node_id) noexcept
{
    return std::find(
               campaign.technology_state.purchased_node_ids.begin(),
               campaign.technology_state.purchased_node_ids.end(),
               tech_node_id) != campaign.technology_state.purchased_node_ids.end();
}

std::uint32_t TechnologySystem::purchased_count_in_tier(
    const CampaignState& campaign,
    FactionId faction_id,
    std::uint8_t tier_index) noexcept
{
    std::uint32_t count = 0U;
    for (const auto purchased_node_id : campaign.technology_state.purchased_node_ids)
    {
        const auto* node_def = find_technology_node_def(purchased_node_id);
        if (node_def != nullptr &&
            node_def->faction_id == faction_id &&
            node_def->tier_index == tier_index)
        {
            count += 1U;
        }
    }

    return count;
}

bool TechnologySystem::tier_unlocked(
    const CampaignState& campaign,
    const TechnologyTierDef& tier_def) noexcept
{
    const auto* faction_progress = find_faction_progress(campaign, tier_def.faction_id);
    if (faction_progress == nullptr)
    {
        return false;
    }

    return faction_progress->faction_reputation >= tier_def.reputation_requirement;
}

bool TechnologySystem::total_reputation_tier_unlocked(
    const CampaignState& campaign,
    const TotalReputationTierDef& tier_def) noexcept
{
    return campaign.technology_state.total_reputation >= tier_def.reputation_requirement;
}

bool TechnologySystem::plant_unlocked(
    const CampaignState& campaign,
    PlantId plant_id) noexcept
{
    if (plant_id.value == 0U)
    {
        return false;
    }

    for (const auto starter_plant_id : k_initial_unlocked_plant_ids)
    {
        if (starter_plant_id == plant_id)
        {
            return true;
        }
    }

    for (const auto& unlock_def : k_prototype_total_reputation_unlock_defs)
    {
        if (unlock_def.unlock_kind != ReputationUnlockKind::Plant ||
            unlock_def.content_id != plant_id.value)
        {
            continue;
        }

        const auto* tier_def = find_total_reputation_tier_def(unlock_def.tier_index);
        if (tier_def != nullptr && total_reputation_tier_unlocked(campaign, *tier_def))
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

    return std::max(0, faction_progress->faction_reputation);
}

std::int32_t TechnologySystem::current_cash_cost(
    const CampaignState& /*campaign*/,
    const TechnologyNodeDef& node_def) noexcept
{
    return std::max(0, node_def.cash_cost);
}

bool TechnologySystem::node_claimable(
    const CampaignState& campaign,
    const TechnologyNodeDef& node_def) noexcept
{
    if (node_purchased(campaign, node_def.tech_node_id))
    {
        return false;
    }

    const auto* tier_def = find_technology_tier_def(node_def.faction_id, node_def.tier_index);
    if (tier_def == nullptr || !tier_unlocked(campaign, *tier_def))
    {
        return false;
    }

    return campaign.cash >= current_cash_cost(campaign, node_def);
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

        auto* faction_progress = find_faction_progress_mut(context.campaign, node_def->faction_id);
        if (faction_progress == nullptr)
        {
            return GS1_STATUS_NOT_FOUND;
        }

        context.campaign.cash -= current_cash_cost(context.campaign, *node_def);
        context.campaign.technology_state.purchased_node_ids.push_back(node_def->tech_node_id);
        return GS1_STATUS_OK;
    }

    default:
        return GS1_STATUS_OK;
    }
}
}  // namespace gs1
