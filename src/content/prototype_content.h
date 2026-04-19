#pragma once

#include "content/defs/faction_defs.h"
#include "support/id_types.h"
#include "support/site_objective_types.h"
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
    SiteObjectiveType objective_type {SiteObjectiveType::DenseRestoration};
    FactionId featured_faction_id {};
    Gs1SiteState initial_state {GS1_SITE_STATE_LOCKED};
    std::vector<SiteId> adjacent_site_ids {};
    std::uint32_t support_package_id {0};
    std::vector<PrototypeSupportItemContent> exported_support_items {};
    std::vector<ModifierId> nearby_aura_modifier_ids {};
    std::uint32_t site_completion_tile_threshold {0};
    double site_time_limit_minutes {0.0};
    SiteObjectiveTargetEdge objective_target_edge {SiteObjectiveTargetEdge::East};
    std::uint8_t objective_target_band_width {0U};
    float highway_max_average_sand_cover {0.0f};
    std::int32_t completion_reputation_reward {0};
    std::int32_t completion_faction_reputation_reward {0};
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
