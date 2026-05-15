#pragma once

#include "campaign/campaign_state.h"
#include "site/site_run_state.h"

#include <span>

namespace gs1
{
class SiteRunFactory final
{
public:
    [[nodiscard]] static SiteRunState create_site_run(
        std::uint64_t campaign_seed,
        std::span<const FactionProgressState> faction_progress,
        const SiteMetaState& site_meta);
    [[nodiscard]] static SiteRunState create_site_run(
        const CampaignState& campaign,
        const SiteMetaState& site_meta);
};
}  // namespace gs1
