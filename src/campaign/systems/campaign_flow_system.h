#pragma once

#include "campaign/systems/campaign_system_context.h"
#include "commands/game_command.h"
#include "site/site_run_state.h"
#include "gs1/status.h"
#include "gs1/types.h"

#include <optional>

namespace gs1
{
struct CampaignFlowCommandContext final
{
    std::optional<CampaignState>& campaign;
    std::optional<SiteRunState>& active_site_run;
    Gs1AppState& app_state;
    GameCommandQueue& command_queue;
};

struct CampaignFixedStepContext final
{
    CampaignState& campaign;
};

class CampaignFlowSystem final
{
public:
    [[nodiscard]] static bool subscribes_to(GameCommandType type) noexcept;
    [[nodiscard]] static Gs1Status process_command(
        CampaignFlowCommandContext& context,
        const GameCommand& command);
    static void run(CampaignFixedStepContext& context);
};
}  // namespace gs1
