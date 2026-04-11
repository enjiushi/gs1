#include "content/prototype_content.h"

namespace gs1
{
namespace
{
const PrototypeCampaignContent k_content = {
    .sites = {
        PrototypeSiteContent{SiteId{1}, 101U, GS1_SITE_STATE_AVAILABLE, {SiteId{2}}, 10U, TileCoord{4, 4}},
        PrototypeSiteContent{SiteId{2}, 102U, GS1_SITE_STATE_LOCKED, {SiteId{1}, SiteId{3}}, 12U, TileCoord{5, 4}},
        PrototypeSiteContent{SiteId{3}, 103U, GS1_SITE_STATE_LOCKED, {SiteId{2}, SiteId{4}}, 14U, TileCoord{5, 5}},
        PrototypeSiteContent{SiteId{4}, 104U, GS1_SITE_STATE_LOCKED, {SiteId{3}}, 16U, TileCoord{6, 5}}
    },
    .initially_revealed_site_ids = {SiteId{1}},
    .initially_available_site_ids = {SiteId{1}}
};
}  // namespace

const PrototypeCampaignContent& get_prototype_campaign_content() noexcept
{
    return k_content;
}

const PrototypeSiteContent* find_prototype_site_content(SiteId site_id) noexcept
{
    for (const auto& site : k_content.sites)
    {
        if (site.site_id == site_id)
        {
            return &site;
        }
    }

    return nullptr;
}
}  // namespace gs1
