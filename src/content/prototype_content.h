#pragma once

#include "support/id_types.h"
#include "gs1/types.h"

#include <cstdint>
#include <vector>

namespace gs1
{
struct PrototypeSupportItemContent final
{
    ItemId item_id {};
    std::uint32_t quantity {0};
};

struct PrototypeSiteContent final
{
    SiteId site_id {};
    std::uint32_t site_archetype_id {0};
    Gs1SiteState initial_state {GS1_SITE_STATE_LOCKED};
    std::vector<SiteId> adjacent_site_ids {};
    std::uint32_t support_package_id {0};
    std::vector<PrototypeSupportItemContent> exported_support_items {};
    std::vector<ModifierId> nearby_aura_modifier_ids {};
    std::uint32_t site_completion_tile_threshold {0};
    TileCoord camp_anchor_tile {};
};

struct PrototypeCampaignContent final
{
    std::vector<PrototypeSiteContent> sites {};
    std::vector<SiteId> initially_revealed_site_ids {};
    std::vector<SiteId> initially_available_site_ids {};
};

[[nodiscard]] const PrototypeCampaignContent& get_prototype_campaign_content() noexcept;
[[nodiscard]] const PrototypeSiteContent* find_prototype_site_content(SiteId site_id) noexcept;
}  // namespace gs1
