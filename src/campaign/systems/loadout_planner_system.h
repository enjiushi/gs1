#pragma once

#include "campaign/systems/campaign_system_context.h"

namespace gs1
{
class LoadoutPlannerSystem final
{
public:
    static void run(CampaignSystemContext& context);
};
}  // namespace gs1
