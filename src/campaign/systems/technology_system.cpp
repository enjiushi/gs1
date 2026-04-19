#include "campaign/systems/technology_system.h"

#include <algorithm>
#include <cmath>

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
    return type == GameMessageType::CampaignReputationAwardRequested ||
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

std::uint32_t TechnologySystem::purchased_base_tech_count_in_tier(
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
            node_def->tier_index == tier_index &&
            node_def->entry_kind == TechnologyEntryKind::BaseTech)
        {
            count += 1U;
        }
    }

    return count;
}

std::uint32_t TechnologySystem::purchased_amplification_count_in_tier(
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
            node_def->tier_index == tier_index &&
            node_def->entry_kind == TechnologyEntryKind::Amplification)
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
    if (tier_def.tier_index <= 1U)
    {
        return true;
    }

    const auto previous_tier = static_cast<std::uint8_t>(tier_def.tier_index - 1U);
    return purchased_base_tech_count_in_tier(campaign, tier_def.faction_id, previous_tier) > 0U &&
        purchased_amplification_count_in_tier(campaign, tier_def.faction_id, previous_tier) > 0U;
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

    return std::max(0, faction_progress->faction_reputation - faction_progress->occupied_reputation);
}

std::int32_t TechnologySystem::current_reputation_cost(
    const CampaignState& campaign,
    const TechnologyNodeDef& node_def) noexcept
{
    const auto* tier_def = find_technology_tier_def(node_def.faction_id, node_def.tier_index);
    if (tier_def == nullptr)
    {
        return 0;
    }

    const auto purchased_count =
        static_cast<float>(purchased_count_in_tier(campaign, node_def.faction_id, node_def.tier_index));
    const auto multiplier = std::pow(k_tier_additional_purchase_cost_multiplier, purchased_count);
    return static_cast<std::int32_t>(std::ceil(static_cast<float>(tier_def->base_reputation_cost) * multiplier));
}

bool TechnologySystem::amplification_choice_locked(
    const CampaignState& campaign,
    const TechnologyNodeDef& amplification_def) noexcept
{
    if (amplification_def.entry_kind != TechnologyEntryKind::Amplification)
    {
        return false;
    }

    for (const auto purchased_node_id : campaign.technology_state.purchased_node_ids)
    {
        const auto* purchased_def = find_technology_node_def(purchased_node_id);
        if (purchased_def != nullptr &&
            purchased_def->entry_kind == TechnologyEntryKind::Amplification &&
            purchased_def->parent_base_tech_id == amplification_def.parent_base_tech_id &&
            purchased_def->tech_node_id != amplification_def.tech_node_id)
        {
            return true;
        }
    }

    return false;
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

    if (node_def.entry_kind == TechnologyEntryKind::Amplification)
    {
        if (!node_purchased(campaign, node_def.parent_base_tech_id) ||
            amplification_choice_locked(campaign, node_def))
        {
            return false;
        }
    }

    return available_faction_reputation(campaign, node_def.faction_id) >=
        current_reputation_cost(campaign, node_def);
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
        context.campaign.technology_state.reputation =
            std::max(0, context.campaign.technology_state.reputation + payload.delta);
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

        faction_progress->occupied_reputation += current_reputation_cost(context.campaign, *node_def);
        context.campaign.technology_state.purchased_node_ids.push_back(node_def->tech_node_id);
        return GS1_STATUS_OK;
    }

    default:
        return GS1_STATUS_OK;
    }
}
}  // namespace gs1
