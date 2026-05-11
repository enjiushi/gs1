#include "campaign/systems/campaign_time_system.h"

#include "campaign/campaign_state.h"
#include "runtime/game_runtime.h"

namespace gs1
{
void CampaignTimeSystem::run(CampaignFixedStepContext& context)
{
    context.campaign.campaign_clock_minutes_elapsed +=
        runtime_minutes_from_real_seconds(context.fixed_step_seconds);

    const auto elapsed_days =
        static_cast<std::uint32_t>(
            context.campaign.campaign_clock_minutes_elapsed / k_runtime_minutes_per_day);
    context.campaign.campaign_days_remaining =
        (elapsed_days >= context.campaign.campaign_days_total)
            ? 0U
            : (context.campaign.campaign_days_total - elapsed_days);
}

GS1_IMPLEMENT_RUNTIME_CAMPAIGN_FIXED_STEP_SYSTEM(
    CampaignTimeSystem,
    GS1_RUNTIME_PROFILE_SYSTEM_CAMPAIGN_TIME,
    0U,
    CampaignFixedStepContext)
}  // namespace gs1
