#pragma once

namespace gs1
{
struct CampaignState;
struct SiteRunState;

struct SiteSystemContext final
{
    CampaignState& campaign;
    SiteRunState& site_run;
};
}  // namespace gs1
