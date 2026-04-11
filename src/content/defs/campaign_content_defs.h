#pragma once

#include "support/id_types.h"

#include <vector>

namespace gs1
{
struct CampaignContentDef final
{
    std::vector<SiteId> entry_site_ids {};
    std::uint32_t campaign_days {30};
};
}  // namespace gs1
