#pragma once

#include "campaign/systems/campaign_system_context.h"
#include "messages/game_message.h"
#include "gs1/status.h"

namespace gs1
{
class LoadoutPlannerSystem final
{
public:
    static void initialize_campaign_state(CampaignState& campaign);
    [[nodiscard]] static bool subscribes_to(GameMessageType type) noexcept;
    [[nodiscard]] static Gs1Status process_message(
        CampaignSystemContext& context,
        const GameMessage& message);
};
}  // namespace gs1
