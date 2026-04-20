#pragma once

#include "campaign/systems/campaign_system_context.h"
#include "messages/game_message.h"
#include "site/site_run_state.h"
#include "gs1/status.h"
#include "gs1/types.h"

#include <optional>

namespace gs1
{
struct CampaignFlowMessageContext final
{
    std::optional<CampaignState>& campaign;
    std::optional<SiteRunState>& active_site_run;
    Gs1AppState& app_state;
    GameMessageQueue& message_queue;
};

class CampaignFlowSystem final
{
public:
    [[nodiscard]] static bool subscribes_to(GameMessageType type) noexcept;
    [[nodiscard]] static Gs1Status process_message(
        CampaignFlowMessageContext& context,
        const GameMessage& message);
};
}  // namespace gs1
