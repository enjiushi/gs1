#pragma once

#include <cstdint>

namespace gs1
{
enum class MessageId : std::uint32_t
{
    Invalid = 0,
    StartNewCampaign = 1,
    SelectDeploymentSite = 2,
    DeploymentSiteSelectionChanged = 3,
    StartSiteAttempt = 4,
    ReturnToRegionalMap = 5,
    SiteAttemptEnded = 6,
    PresentLog = 7
};
}  // namespace gs1
