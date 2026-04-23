#pragma once

#include "campaign/campaign_state.h"
#include "content/defs/technology_defs.h"
#include "campaign/systems/campaign_system_context.h"
#include "messages/game_message.h"
#include "gs1/status.h"

namespace gs1
{
class TechnologySystem final
{
public:
    [[nodiscard]] static bool subscribes_to(GameMessageType type) noexcept;
    [[nodiscard]] static Gs1Status process_message(
        CampaignSystemContext& context,
        const GameMessage& message);
    [[nodiscard]] static bool node_purchased(
        const CampaignState& campaign,
        TechNodeId tech_node_id) noexcept;
    [[nodiscard]] static bool technology_tier_unlocked(
        const CampaignState& campaign,
        const TechnologyTierDef& tier_def) noexcept;
    [[nodiscard]] static bool faction_tier_unlocked(
        const CampaignState& campaign,
        const FactionTechnologyTierDef& tier_def) noexcept;
    [[nodiscard]] static bool total_reputation_tier_unlocked(
        const CampaignState& campaign,
        const TotalReputationTierDef& tier_def) noexcept;
    [[nodiscard]] static bool plant_unlocked(
        const CampaignState& campaign,
        PlantId plant_id) noexcept;
    [[nodiscard]] static std::int32_t available_faction_reputation(
        const CampaignState& campaign,
        FactionId faction_id) noexcept;
    [[nodiscard]] static std::int32_t current_cash_cost(
        const CampaignState& campaign,
        const TechnologyNodeDef& node_def) noexcept;
    [[nodiscard]] static bool node_claimable(
        const CampaignState& campaign,
        const TechnologyNodeDef& node_def) noexcept;
};
}  // namespace gs1
