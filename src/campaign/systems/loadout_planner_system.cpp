#include "campaign/systems/loadout_planner_system.h"

#include "campaign/campaign_state.h"

namespace gs1
{
bool LoadoutPlannerSystem::subscribes_to(GameCommandType type) noexcept
{
    return type == GameCommandType::DeploymentSiteSelectionChanged;
}

Gs1Status LoadoutPlannerSystem::process_command(
    CampaignSystemContext& context,
    const GameCommand& command)
{
    if (!subscribes_to(command.type))
    {
        return GS1_STATUS_OK;
    }

    const auto& payload = command.payload_as<DeploymentSiteSelectionChangedCommand>();
    if (payload.selected_site_id == 0U)
    {
        context.campaign.loadout_planner_state.selected_target_site_id.reset();
    }
    else
    {
        context.campaign.loadout_planner_state.selected_target_site_id = SiteId {payload.selected_site_id};
    }

    return GS1_STATUS_OK;
}
}  // namespace gs1
