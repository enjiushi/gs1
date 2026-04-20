#pragma once

#include "campaign/systems/campaign_system_context.h"
#include "runtime/runtime_clock.h"

namespace gs1
{
struct CampaignFixedStepContext final
{
    CampaignState& campaign;
    double fixed_step_seconds {k_default_fixed_step_seconds};
};

class CampaignTimeSystem final
{
public:
    static void run(CampaignFixedStepContext& context);
};
}  // namespace gs1
