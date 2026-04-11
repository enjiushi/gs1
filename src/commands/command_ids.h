#pragma once

#include <cstdint>

namespace gs1
{
enum class CommandId : std::uint32_t
{
    Invalid = 0,
    StartNewCampaign = 1,
    SelectDeploymentSite = 2,
    StartSiteAttempt = 3,
    ReturnToRegionalMap = 4,
    MarkSiteCompleted = 5,
    MarkSiteFailed = 6,
    PresentLog = 7
};
}  // namespace gs1
