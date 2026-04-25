#include "app/campaign_factory.h"

#include "campaign/systems/loadout_planner_system.h"
#include "content/defs/faction_defs.h"
#include "content/prototype_content.h"

#include <cstdlib>

namespace gs1
{
namespace
{
[[nodiscard]] LoadoutSlot make_support_loadout_slot(ItemId item_id, std::uint32_t quantity) noexcept
{
    return LoadoutSlot {
        item_id,
        quantity,
        item_id.value != 0U && quantity > 0U};
}

[[nodiscard]] bool are_regional_map_tiles_adjacent(
    const TileCoord& left,
    const TileCoord& right) noexcept
{
    const auto delta_x = std::abs(left.x - right.x);
    const auto delta_y = std::abs(left.y - right.y);
    return (delta_x > 0 || delta_y > 0) && delta_x <= 1 && delta_y <= 1;
}

void rebuild_regional_map_adjacency(CampaignState& campaign)
{
    for (auto& site : campaign.sites)
    {
        site.adjacent_site_ids.clear();
    }

    for (std::size_t left_index = 0; left_index < campaign.sites.size(); ++left_index)
    {
        for (std::size_t right_index = left_index + 1U; right_index < campaign.sites.size(); ++right_index)
        {
            auto& left = campaign.sites[left_index];
            auto& right = campaign.sites[right_index];
            if (!are_regional_map_tiles_adjacent(left.regional_map_tile, right.regional_map_tile))
            {
                continue;
            }

            left.adjacent_site_ids.push_back(right.site_id);
            right.adjacent_site_ids.push_back(left.site_id);
        }
    }
}
}  // namespace

CampaignState CampaignFactory::create_prototype_campaign(
    std::uint64_t campaign_seed,
    std::uint32_t campaign_days)
{
    CampaignState campaign {};
    const auto& content = get_prototype_campaign_content();

    campaign.campaign_id = CampaignId{1U};
    campaign.campaign_seed = campaign_seed;
    campaign.cash = content.starting_campaign_cash;
    campaign.campaign_clock_minutes_elapsed = 0.0;
    campaign.campaign_days_total = campaign_days;
    campaign.campaign_days_remaining = campaign_days;
    campaign.app_state = GS1_APP_STATE_REGIONAL_MAP;
    campaign.regional_map_state.tech_tree_open = false;
    campaign.regional_map_state.selected_tech_tree_faction_id = FactionId {k_faction_village_committee};
    campaign.faction_progress.reserve(k_prototype_faction_defs.size());
    for (const auto& faction_def : k_prototype_faction_defs)
    {
        campaign.faction_progress.push_back(FactionProgressState {faction_def.faction_id});
    }
    campaign.technology_state.total_reputation = 0;
    LoadoutPlannerSystem::initialize_campaign_state(campaign);

    campaign.sites.reserve(content.sites.size());
    for (const auto& site_content : content.sites)
    {
        SiteMetaState site {};
        site.site_id = site_content.site_id;
        site.site_state = site_content.initial_state;
        site.regional_map_tile = site_content.regional_map_tile;
        site.site_archetype_id = site_content.site_archetype_id;
        site.featured_faction_id = site_content.featured_faction_id;
        site.support_package_id = site_content.support_package_id;
        site.has_support_package_id = site.support_package_id != 0U;
        site.completion_reputation_reward = site_content.completion_reputation_reward;
        site.completion_faction_reputation_reward = site_content.completion_faction_reputation_reward;
        site.exported_support_items.reserve(site_content.exported_support_items.size());
        for (const auto& item : site_content.exported_support_items)
        {
            site.exported_support_items.push_back(
                make_support_loadout_slot(item.item_id, item.quantity));
        }
        site.nearby_aura_modifier_ids = site_content.nearby_aura_modifier_ids;
        campaign.sites.push_back(site);
    }

    rebuild_regional_map_adjacency(campaign);
    campaign.regional_map_state.revealed_site_ids = content.initially_revealed_site_ids;

    return campaign;
}
}  // namespace gs1
