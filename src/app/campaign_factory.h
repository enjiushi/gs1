#pragma once

#include "campaign/campaign_state.h"

#include <cstdint>

namespace gs1
{
class CampaignFactory final
{
public:
    [[nodiscard]] static CampaignState create_prototype_campaign(
        std::uint64_t campaign_seed,
        std::uint32_t campaign_days);
};
}  // namespace gs1
