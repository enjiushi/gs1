#include "app/campaign_factory.h"

#include "content/prototype_content.h"

namespace gs1
{
CampaignState CampaignFactory::create_prototype_campaign(
    std::uint64_t campaign_seed,
    std::uint32_t campaign_days)
{
    CampaignState campaign {};
    const auto& content = get_prototype_campaign_content();

    campaign.campaign_id = CampaignId{1U};
    campaign.campaign_seed = campaign_seed;
    campaign.campaign_clock_minutes_elapsed = 0.0;
    campaign.campaign_days_total = campaign_days;
    campaign.campaign_days_remaining = campaign_days;
    campaign.app_state = GS1_APP_STATE_REGIONAL_MAP;
    campaign.faction_progress.push_back(FactionProgressState{FactionId{1U}});

    campaign.sites.reserve(content.sites.size());
    for (const auto& site_content : content.sites)
    {
        SiteMetaState site {};
        site.site_id = site_content.site_id;
        site.site_state = site_content.initial_state;
        site.adjacent_site_ids = site_content.adjacent_site_ids;
        site.site_archetype_id = site_content.site_archetype_id;
        campaign.sites.push_back(site);
    }

    campaign.regional_map_state.revealed_site_ids = content.initially_revealed_site_ids;

    return campaign;
}
}  // namespace gs1
