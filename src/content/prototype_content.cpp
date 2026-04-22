#include "content/prototype_content.h"

#include "content/content_loader.h"

namespace gs1
{
const PrototypeCampaignContent& get_prototype_campaign_content() noexcept
{
    return prototype_content_database().prototype_campaign;
}

const PrototypeSiteContent* find_prototype_site_content(SiteId site_id) noexcept
{
    const auto& content = prototype_content_database().prototype_campaign;
    const auto it = prototype_content_database().index.site_by_id.find(site_id.value);
    if (it == prototype_content_database().index.site_by_id.end())
    {
        return nullptr;
    }
    return &content.sites[it->second];
}
}  // namespace gs1
