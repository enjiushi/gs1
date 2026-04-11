#pragma once

#include "support/id_types.h"

#include <cstdint>

namespace gs1
{
enum class RuntimeEventType : std::uint32_t
{
    None = 0,
    CampaignStarted = 1,
    SiteAttemptStarted = 2,
    SiteCompleted = 3,
    SiteFailed = 4
};

struct RuntimeEvent final
{
    RuntimeEventType type {RuntimeEventType::None};
    SiteId site_id {};
    std::uint32_t arg0 {0};
    std::uint32_t arg1 {0};
};
}  // namespace gs1
