#include "content/prototype_content.h"

#include "content/defs/faction_defs.h"
#include "content/defs/item_defs.h"

namespace gs1
{
namespace
{
const PrototypeCampaignContent k_content = {
    .sites = {
        PrototypeSiteContent {
            SiteId {1},
            101U,
            FactionId {k_faction_village_committee},
            GS1_SITE_STATE_AVAILABLE,
            {SiteId {2}},
            1001U,
            {
                PrototypeSupportItemContent {ItemId {k_item_saltbush_seed_bundle}, 4U},
                PrototypeSupportItemContent {ItemId {k_item_wood_bundle}, 2U},
            },
            {ModifierId {1U}},
            10U,
            10,
            10,
            TileCoord {15, 15}},
        PrototypeSiteContent {
            SiteId {2},
            102U,
            FactionId {k_faction_forestry_bureau},
            GS1_SITE_STATE_LOCKED,
            {SiteId {1}, SiteId {3}},
            1002U,
            {
                PrototypeSupportItemContent {ItemId {k_item_shade_cactus_seed_bundle}, 3U},
                PrototypeSupportItemContent {ItemId {k_item_iron_bundle}, 2U},
            },
            {ModifierId {2U}},
            12U,
            10,
            10,
            TileCoord {16, 15}},
        PrototypeSiteContent {
            SiteId {3},
            103U,
            FactionId {k_faction_agricultural_university},
            GS1_SITE_STATE_LOCKED,
            {SiteId {2}, SiteId {4}},
            1003U,
            {
                PrototypeSupportItemContent {ItemId {k_item_sunfruit_vine_seed_bundle}, 3U},
                PrototypeSupportItemContent {ItemId {k_item_food_pack}, 1U},
            },
            {ModifierId {3U}},
            14U,
            10,
            10,
            TileCoord {16, 16}},
        PrototypeSiteContent {
            SiteId {4},
            104U,
            FactionId {k_faction_village_committee},
            GS1_SITE_STATE_LOCKED,
            {SiteId {3}},
            1004U,
            {
                PrototypeSupportItemContent {ItemId {k_item_medicine_pack}, 1U},
                PrototypeSupportItemContent {ItemId {k_item_storage_crate_kit}, 1U},
            },
            {ModifierId {4U}},
            16U,
            10,
            10,
            TileCoord {17, 16}}
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
