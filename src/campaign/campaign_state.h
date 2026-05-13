#pragma once

#include "campaign/faction_progress_state.h"
#include "campaign/loadout_planner_state.h"
#include "campaign/technology_state.h"
#include "support/id_types.h"
#include "gs1/types.h"

#include <optional>
#include <vector>

namespace gs1
{
struct SiteMetaState final
{
    SiteId site_id {};
    Gs1SiteState site_state {GS1_SITE_STATE_LOCKED};
    TileCoord regional_map_tile {};
    std::vector<SiteId> adjacent_site_ids {};
    std::uint32_t site_archetype_id {0};
    FactionId featured_faction_id {};
    std::uint32_t attempt_count {0};
    std::uint32_t support_package_id {0};
    bool has_support_package_id {false};
    std::vector<LoadoutSlot> exported_support_items {};
    std::vector<ModifierId> nearby_aura_modifier_ids {};
    std::int32_t completion_reputation_reward {0};
    std::int32_t completion_faction_reputation_reward {0};
};

struct RegionalMapState final
{
    std::vector<SiteId> revealed_site_ids {};
    std::vector<SiteId> available_site_ids {};
    std::vector<SiteId> completed_site_ids {};
    std::optional<SiteId> selected_site_id {};
};

struct CampaignState final
{
    CampaignId campaign_id {};
    std::uint64_t campaign_seed {0};
    double campaign_clock_minutes_elapsed {0.0};
    std::uint32_t campaign_days_total {0};
    std::uint32_t campaign_days_remaining {0};
    Gs1AppState app_state {GS1_APP_STATE_BOOT};
    RegionalMapState regional_map_state {};
    std::vector<FactionProgressState> faction_progress {};
    TechnologyState technology_state {};
    LoadoutPlannerState loadout_planner_state {};
    std::vector<SiteMetaState> sites {};
    std::optional<SiteId> active_site_id {};
};
}  // namespace gs1
