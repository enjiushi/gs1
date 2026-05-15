#pragma once

#include "gs1/state_view.h"

#include <vector>

namespace gs1
{
class SiteWorld;

struct RuntimeGameStateViewCache final
{
    Gs1GameStateView root {};
    Gs1CampaignStateView campaign {};
    Gs1SiteStateView site {};

    std::vector<Gs1FactionProgressView> faction_progress {};
    std::vector<Gs1CampaignSiteView> campaign_sites {};
    std::vector<std::uint32_t> revealed_site_ids {};
    std::vector<std::uint32_t> available_site_ids {};
    std::vector<std::uint32_t> completed_site_ids {};
    std::vector<std::uint32_t> site_adjacency_ids {};
    std::vector<Gs1LoadoutSlotView> site_exported_support_items {};
    std::vector<std::uint32_t> site_nearby_aura_modifier_ids {};
    std::vector<Gs1LoadoutSlotView> baseline_deployment_items {};
    std::vector<Gs1LoadoutSlotView> available_exported_support_items {};
    std::vector<Gs1LoadoutSlotView> selected_loadout_slots {};
    std::vector<std::uint32_t> active_nearby_aura_modifier_ids {};
    std::vector<Gs1ProgressionEntryView> progression_entries {};

    std::vector<Gs1InventoryStorageView> inventory_storages {};
    std::vector<Gs1InventorySlotView> inventory_slots {};
    std::vector<Gs1TaskView> tasks {};
    std::vector<Gs1TaskRewardDraftOptionView> task_reward_draft_options {};
    std::vector<Gs1SiteModifierView> active_modifiers {};
    std::vector<Gs1PhoneListingView> phone_listings {};
    std::vector<Gs1CraftContextOptionView> craft_context_options {};

    std::uint32_t revision {0};
};

class GameRuntime;

void rebuild_game_state_view_cache(
    const GameRuntime& runtime,
    RuntimeGameStateViewCache& cache);

[[nodiscard]] bool build_site_tile_view(
    const SiteWorld* site_world,
    std::uint32_t tile_index,
    Gs1SiteTileView& out_tile);
}  // namespace gs1
