#pragma once

#include "campaign/systems/campaign_system_context.h"
#include "commands/game_command.h"
#include "gs1/status.h"

namespace gs1
{
class LoadoutPlannerSystem final
{
public:
    [[nodiscard]] static bool subscribes_to(GameCommandType type) noexcept;
    [[nodiscard]] static Gs1Status process_command(
        CampaignSystemContext& context,
        const GameCommand& command);
};
}  // namespace gs1
