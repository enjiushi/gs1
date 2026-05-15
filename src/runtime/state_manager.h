#pragma once

#include "runtime/state_set.h"
#include "runtime/game_state.h"

#include <array>
#include <cstddef>
#include <stdexcept>

namespace gs1
{
class IRuntimeSystem;

class StateManager final
{
public:
    StateManager() = default;

    [[nodiscard]] GameState& game_state() noexcept { return state_; }
    [[nodiscard]] const GameState& game_state() const noexcept { return state_; }

    void register_resolver(IRuntimeSystem& system);
    void register_owned_state_set(IRuntimeSystem& system, StateSetId state_set);

    template <StateSetId Id>
    [[nodiscard]] const typename state_traits<Id>::type& query(const struct GameState& state) const noexcept;

    template <StateSetId Id>
    [[nodiscard]] typename state_traits<Id>::type& state(struct GameState& state) const noexcept;

    [[nodiscard]] IRuntimeSystem* active_resolver(StateSetId state_set) const noexcept
    {
        return active_resolver_by_state_set_[index(state_set)];
    }

    [[nodiscard]] IRuntimeSystem* default_resolver(StateSetId state_set) const noexcept
    {
        return default_resolver_by_state_set_[index(state_set)];
    }

private:
    static constexpr std::size_t index(StateSetId state_set) noexcept
    {
        return static_cast<std::size_t>(state_set);
    }

    GameState state_ {};
    std::array<IRuntimeSystem*, static_cast<std::size_t>(StateSetId::Count)> default_resolver_by_state_set_ {};
    std::array<IRuntimeSystem*, static_cast<std::size_t>(StateSetId::Count)> active_resolver_by_state_set_ {};
};

template <StateSetId Id>
[[nodiscard]] inline const typename state_traits<Id>::type& StateManager::query(
    const GameState& state) const noexcept
{
    if constexpr (Id == StateSetId::AppState)
    {
        return state.app_state.get();
    }
    else if constexpr (Id == StateSetId::FixedStepSeconds)
    {
        return state.fixed_step_seconds.get();
    }
    else if constexpr (Id == StateSetId::MoveDirection)
    {
        return state.move_direction.get();
    }
    else if constexpr (Id == StateSetId::CampaignCore)
    {
        return state.campaign_core.get();
    }
    else if constexpr (Id == StateSetId::CampaignRegionalMapMeta)
    {
        return state.campaign_regional_map_meta.get();
    }
    else if constexpr (Id == StateSetId::CampaignRegionalMapRevealedSites)
    {
        return state.campaign_regional_map_revealed_sites.get();
    }
    else if constexpr (Id == StateSetId::CampaignRegionalMapAvailableSites)
    {
        return state.campaign_regional_map_available_sites.get();
    }
    else if constexpr (Id == StateSetId::CampaignRegionalMapCompletedSites)
    {
        return state.campaign_regional_map_completed_sites.get();
    }
    else if constexpr (Id == StateSetId::CampaignFactionProgress)
    {
        return state.campaign_faction_progress.get();
    }
    else if constexpr (Id == StateSetId::CampaignTechnology)
    {
        return state.campaign_technology.get();
    }
    else if constexpr (Id == StateSetId::CampaignLoadoutPlannerMeta)
    {
        return state.campaign_loadout_planner_meta.get();
    }
    else if constexpr (Id == StateSetId::CampaignLoadoutPlannerBaselineItems)
    {
        return state.campaign_loadout_planner_baseline_items.get();
    }
    else if constexpr (Id == StateSetId::CampaignLoadoutPlannerAvailableSupportItems)
    {
        return state.campaign_loadout_planner_available_support_items.get();
    }
    else if constexpr (Id == StateSetId::CampaignLoadoutPlannerSelectedSlots)
    {
        return state.campaign_loadout_planner_selected_slots.get();
    }
    else if constexpr (Id == StateSetId::CampaignLoadoutPlannerNearbyAuraModifiers)
    {
        return state.campaign_loadout_planner_nearby_aura_modifiers.get();
    }
    else if constexpr (Id == StateSetId::CampaignSiteMetaEntries)
    {
        return state.campaign_site_meta_entries.get();
    }
    else if constexpr (Id == StateSetId::CampaignSiteAdjacentIds)
    {
        return state.campaign_site_adjacent_ids.get();
    }
    else if constexpr (Id == StateSetId::CampaignSiteExportedSupportItems)
    {
        return state.campaign_site_exported_support_items.get();
    }
    else if constexpr (Id == StateSetId::CampaignSiteNearbyAuraModifierIds)
    {
        return state.campaign_site_nearby_aura_modifier_ids.get();
    }
    else if constexpr (Id == StateSetId::SiteRunMeta)
    {
        return state.site_run_meta.get();
    }
    else if constexpr (Id == StateSetId::SiteClock)
    {
        return state.site_clock.get();
    }
    else if constexpr (Id == StateSetId::SiteCamp)
    {
        return state.site_camp.get();
    }
    else if constexpr (Id == StateSetId::SiteInventoryMeta)
    {
        return state.site_inventory_meta.get();
    }
    else if constexpr (Id == StateSetId::SiteInventoryStorageContainers)
    {
        return state.site_inventory_storage_containers.get();
    }
    else if constexpr (Id == StateSetId::SiteInventoryStorageSlotItemIds)
    {
        return state.site_inventory_storage_slot_item_ids.get();
    }
    else if constexpr (Id == StateSetId::SiteInventoryWorkerPackSlots)
    {
        return state.site_inventory_worker_pack_slots.get();
    }
    else if constexpr (Id == StateSetId::SiteInventoryPendingDeliveries)
    {
        return state.site_inventory_pending_deliveries.get();
    }
    else if constexpr (Id == StateSetId::SiteInventoryPendingDeliveryItemStacks)
    {
        return state.site_inventory_pending_delivery_item_stacks.get();
    }
    else if constexpr (Id == StateSetId::SiteContractorMeta)
    {
        return state.site_contractor_meta.get();
    }
    else if constexpr (Id == StateSetId::SiteContractorWorkOrders)
    {
        return state.site_contractor_work_orders.get();
    }
    else if constexpr (Id == StateSetId::SiteWeather)
    {
        return state.site_weather.get();
    }
    else if constexpr (Id == StateSetId::SiteEvent)
    {
        return state.site_event.get();
    }
    else if constexpr (Id == StateSetId::SiteTaskBoardMeta)
    {
        return state.site_task_board_meta.get();
    }
    else if constexpr (Id == StateSetId::SiteTaskBoardVisibleTasks)
    {
        return state.site_task_board_visible_tasks.get();
    }
    else if constexpr (Id == StateSetId::SiteTaskBoardRewardDraftOptions)
    {
        return state.site_task_board_reward_draft_options.get();
    }
    else if constexpr (Id == StateSetId::SiteTaskBoardTrackedTiles)
    {
        return state.site_task_board_tracked_tiles.get();
    }
    else if constexpr (Id == StateSetId::SiteTaskBoardAcceptedTaskIds)
    {
        return state.site_task_board_accepted_task_ids.get();
    }
    else if constexpr (Id == StateSetId::SiteTaskBoardCompletedTaskIds)
    {
        return state.site_task_board_completed_task_ids.get();
    }
    else if constexpr (Id == StateSetId::SiteTaskBoardClaimedTaskIds)
    {
        return state.site_task_board_claimed_task_ids.get();
    }
    else if constexpr (Id == StateSetId::SiteModifierMeta)
    {
        return state.site_modifier_meta.get();
    }
    else if constexpr (Id == StateSetId::SiteModifierNearbyAuraIds)
    {
        return state.site_modifier_nearby_aura_ids.get();
    }
    else if constexpr (Id == StateSetId::SiteModifierActiveSiteModifiers)
    {
        return state.site_modifier_active_site_modifiers.get();
    }
    else if constexpr (Id == StateSetId::SiteEconomyMeta)
    {
        return state.site_economy_meta.get();
    }
    else if constexpr (Id == StateSetId::SiteEconomyRevealedUnlockableIds)
    {
        return state.site_economy_revealed_unlockable_ids.get();
    }
    else if constexpr (Id == StateSetId::SiteEconomyDirectPurchaseUnlockableIds)
    {
        return state.site_economy_direct_purchase_unlockable_ids.get();
    }
    else if constexpr (Id == StateSetId::SiteEconomyPhoneListings)
    {
        return state.site_economy_phone_listings.get();
    }
    else if constexpr (Id == StateSetId::SiteCraftDeviceCacheRuntime)
    {
        return state.site_craft_device_cache_runtime.get();
    }
    else if constexpr (Id == StateSetId::SiteCraftDeviceCaches)
    {
        return state.site_craft_device_caches.get();
    }
    else if constexpr (Id == StateSetId::SiteCraftNearbyItems)
    {
        return state.site_craft_nearby_items.get();
    }
    else if constexpr (Id == StateSetId::SiteCraftPhoneCacheMeta)
    {
        return state.site_craft_phone_cache_meta.get();
    }
    else if constexpr (Id == StateSetId::SiteCraftPhoneItems)
    {
        return state.site_craft_phone_items.get();
    }
    else if constexpr (Id == StateSetId::SiteCraftContextMeta)
    {
        return state.site_craft_context_meta.get();
    }
    else if constexpr (Id == StateSetId::SiteCraftContextOptions)
    {
        return state.site_craft_context_options.get();
    }
    else if constexpr (Id == StateSetId::SiteActionMeta)
    {
        return state.site_action_meta.get();
    }
    else if constexpr (Id == StateSetId::SiteActionReservedInputItemStacks)
    {
        return state.site_action_reserved_input_item_stacks.get();
    }
    else if constexpr (Id == StateSetId::SiteActionResolvedHarvestOutputs)
    {
        return state.site_action_resolved_harvest_outputs.get();
    }
    else if constexpr (Id == StateSetId::SiteCounters)
    {
        return state.site_counters.get();
    }
    else if constexpr (Id == StateSetId::SiteObjectiveMeta)
    {
        return state.site_objective_meta.get();
    }
    else if constexpr (Id == StateSetId::SiteObjectiveTargetTileIndices)
    {
        return state.site_objective_target_tile_indices.get();
    }
    else if constexpr (Id == StateSetId::SiteObjectiveTargetTileMask)
    {
        return state.site_objective_target_tile_mask.get();
    }
    else if constexpr (Id == StateSetId::SiteObjectiveConnectionStartTileIndices)
    {
        return state.site_objective_connection_start_tile_indices.get();
    }
    else if constexpr (Id == StateSetId::SiteObjectiveConnectionStartTileMask)
    {
        return state.site_objective_connection_start_tile_mask.get();
    }
    else if constexpr (Id == StateSetId::SiteObjectiveConnectionGoalTileIndices)
    {
        return state.site_objective_connection_goal_tile_indices.get();
    }
    else if constexpr (Id == StateSetId::SiteObjectiveConnectionGoalTileMask)
    {
        return state.site_objective_connection_goal_tile_mask.get();
    }
    else if constexpr (Id == StateSetId::SiteLocalWeatherResolveMeta)
    {
        return state.site_local_weather_resolve_meta.get();
    }
    else if constexpr (Id == StateSetId::SiteLocalWeatherResolveLastTotalContributions)
    {
        return state.site_local_weather_resolve_last_total_contributions.get();
    }
    else if constexpr (Id == StateSetId::SitePlantWeatherContributionMeta)
    {
        return state.site_plant_weather_contribution_meta.get();
    }
    else if constexpr (Id == StateSetId::SitePlantWeatherContributionDirtyTileIndices)
    {
        return state.site_plant_weather_contribution_dirty_tile_indices.get();
    }
    else if constexpr (Id == StateSetId::SitePlantWeatherContributionDirtyTileMask)
    {
        return state.site_plant_weather_contribution_dirty_tile_mask.get();
    }
    else if constexpr (Id == StateSetId::SiteDeviceWeatherContributionMeta)
    {
        return state.site_device_weather_contribution_meta.get();
    }
    else if constexpr (Id == StateSetId::SiteDeviceWeatherContributionDirtyTileIndices)
    {
        return state.site_device_weather_contribution_dirty_tile_indices.get();
    }
    else if constexpr (Id == StateSetId::SiteDeviceWeatherContributionDirtyTileMask)
    {
        return state.site_device_weather_contribution_dirty_tile_mask.get();
    }
}

template <StateSetId Id>
[[nodiscard]] inline typename state_traits<Id>::type& StateManager::state(
    GameState& state) const noexcept
{
    if constexpr (Id == StateSetId::AppState)
    {
        return state.app_state.get();
    }
    else if constexpr (Id == StateSetId::FixedStepSeconds)
    {
        return state.fixed_step_seconds.get();
    }
    else if constexpr (Id == StateSetId::MoveDirection)
    {
        return state.move_direction.get();
    }
    else if constexpr (Id == StateSetId::CampaignCore)
    {
        return state.campaign_core.get();
    }
    else if constexpr (Id == StateSetId::CampaignRegionalMapMeta)
    {
        return state.campaign_regional_map_meta.get();
    }
    else if constexpr (Id == StateSetId::CampaignRegionalMapRevealedSites)
    {
        return state.campaign_regional_map_revealed_sites.get();
    }
    else if constexpr (Id == StateSetId::CampaignRegionalMapAvailableSites)
    {
        return state.campaign_regional_map_available_sites.get();
    }
    else if constexpr (Id == StateSetId::CampaignRegionalMapCompletedSites)
    {
        return state.campaign_regional_map_completed_sites.get();
    }
    else if constexpr (Id == StateSetId::CampaignFactionProgress)
    {
        return state.campaign_faction_progress.get();
    }
    else if constexpr (Id == StateSetId::CampaignTechnology)
    {
        return state.campaign_technology.get();
    }
    else if constexpr (Id == StateSetId::CampaignLoadoutPlannerMeta)
    {
        return state.campaign_loadout_planner_meta.get();
    }
    else if constexpr (Id == StateSetId::CampaignLoadoutPlannerBaselineItems)
    {
        return state.campaign_loadout_planner_baseline_items.get();
    }
    else if constexpr (Id == StateSetId::CampaignLoadoutPlannerAvailableSupportItems)
    {
        return state.campaign_loadout_planner_available_support_items.get();
    }
    else if constexpr (Id == StateSetId::CampaignLoadoutPlannerSelectedSlots)
    {
        return state.campaign_loadout_planner_selected_slots.get();
    }
    else if constexpr (Id == StateSetId::CampaignLoadoutPlannerNearbyAuraModifiers)
    {
        return state.campaign_loadout_planner_nearby_aura_modifiers.get();
    }
    else if constexpr (Id == StateSetId::CampaignSiteMetaEntries)
    {
        return state.campaign_site_meta_entries.get();
    }
    else if constexpr (Id == StateSetId::CampaignSiteAdjacentIds)
    {
        return state.campaign_site_adjacent_ids.get();
    }
    else if constexpr (Id == StateSetId::CampaignSiteExportedSupportItems)
    {
        return state.campaign_site_exported_support_items.get();
    }
    else if constexpr (Id == StateSetId::CampaignSiteNearbyAuraModifierIds)
    {
        return state.campaign_site_nearby_aura_modifier_ids.get();
    }
    else if constexpr (Id == StateSetId::SiteRunMeta)
    {
        return state.site_run_meta.get();
    }
    else if constexpr (Id == StateSetId::SiteClock)
    {
        return state.site_clock.get();
    }
    else if constexpr (Id == StateSetId::SiteCamp)
    {
        return state.site_camp.get();
    }
    else if constexpr (Id == StateSetId::SiteInventoryMeta)
    {
        return state.site_inventory_meta.get();
    }
    else if constexpr (Id == StateSetId::SiteInventoryStorageContainers)
    {
        return state.site_inventory_storage_containers.get();
    }
    else if constexpr (Id == StateSetId::SiteInventoryStorageSlotItemIds)
    {
        return state.site_inventory_storage_slot_item_ids.get();
    }
    else if constexpr (Id == StateSetId::SiteInventoryWorkerPackSlots)
    {
        return state.site_inventory_worker_pack_slots.get();
    }
    else if constexpr (Id == StateSetId::SiteInventoryPendingDeliveries)
    {
        return state.site_inventory_pending_deliveries.get();
    }
    else if constexpr (Id == StateSetId::SiteInventoryPendingDeliveryItemStacks)
    {
        return state.site_inventory_pending_delivery_item_stacks.get();
    }
    else if constexpr (Id == StateSetId::SiteContractorMeta)
    {
        return state.site_contractor_meta.get();
    }
    else if constexpr (Id == StateSetId::SiteContractorWorkOrders)
    {
        return state.site_contractor_work_orders.get();
    }
    else if constexpr (Id == StateSetId::SiteWeather)
    {
        return state.site_weather.get();
    }
    else if constexpr (Id == StateSetId::SiteEvent)
    {
        return state.site_event.get();
    }
    else if constexpr (Id == StateSetId::SiteTaskBoardMeta)
    {
        return state.site_task_board_meta.get();
    }
    else if constexpr (Id == StateSetId::SiteTaskBoardVisibleTasks)
    {
        return state.site_task_board_visible_tasks.get();
    }
    else if constexpr (Id == StateSetId::SiteTaskBoardRewardDraftOptions)
    {
        return state.site_task_board_reward_draft_options.get();
    }
    else if constexpr (Id == StateSetId::SiteTaskBoardTrackedTiles)
    {
        return state.site_task_board_tracked_tiles.get();
    }
    else if constexpr (Id == StateSetId::SiteTaskBoardAcceptedTaskIds)
    {
        return state.site_task_board_accepted_task_ids.get();
    }
    else if constexpr (Id == StateSetId::SiteTaskBoardCompletedTaskIds)
    {
        return state.site_task_board_completed_task_ids.get();
    }
    else if constexpr (Id == StateSetId::SiteTaskBoardClaimedTaskIds)
    {
        return state.site_task_board_claimed_task_ids.get();
    }
    else if constexpr (Id == StateSetId::SiteModifierMeta)
    {
        return state.site_modifier_meta.get();
    }
    else if constexpr (Id == StateSetId::SiteModifierNearbyAuraIds)
    {
        return state.site_modifier_nearby_aura_ids.get();
    }
    else if constexpr (Id == StateSetId::SiteModifierActiveSiteModifiers)
    {
        return state.site_modifier_active_site_modifiers.get();
    }
    else if constexpr (Id == StateSetId::SiteEconomyMeta)
    {
        return state.site_economy_meta.get();
    }
    else if constexpr (Id == StateSetId::SiteEconomyRevealedUnlockableIds)
    {
        return state.site_economy_revealed_unlockable_ids.get();
    }
    else if constexpr (Id == StateSetId::SiteEconomyDirectPurchaseUnlockableIds)
    {
        return state.site_economy_direct_purchase_unlockable_ids.get();
    }
    else if constexpr (Id == StateSetId::SiteEconomyPhoneListings)
    {
        return state.site_economy_phone_listings.get();
    }
    else if constexpr (Id == StateSetId::SiteCraftDeviceCacheRuntime)
    {
        return state.site_craft_device_cache_runtime.get();
    }
    else if constexpr (Id == StateSetId::SiteCraftDeviceCaches)
    {
        return state.site_craft_device_caches.get();
    }
    else if constexpr (Id == StateSetId::SiteCraftNearbyItems)
    {
        return state.site_craft_nearby_items.get();
    }
    else if constexpr (Id == StateSetId::SiteCraftPhoneCacheMeta)
    {
        return state.site_craft_phone_cache_meta.get();
    }
    else if constexpr (Id == StateSetId::SiteCraftPhoneItems)
    {
        return state.site_craft_phone_items.get();
    }
    else if constexpr (Id == StateSetId::SiteCraftContextMeta)
    {
        return state.site_craft_context_meta.get();
    }
    else if constexpr (Id == StateSetId::SiteCraftContextOptions)
    {
        return state.site_craft_context_options.get();
    }
    else if constexpr (Id == StateSetId::SiteActionMeta)
    {
        return state.site_action_meta.get();
    }
    else if constexpr (Id == StateSetId::SiteActionReservedInputItemStacks)
    {
        return state.site_action_reserved_input_item_stacks.get();
    }
    else if constexpr (Id == StateSetId::SiteActionResolvedHarvestOutputs)
    {
        return state.site_action_resolved_harvest_outputs.get();
    }
    else if constexpr (Id == StateSetId::SiteCounters)
    {
        return state.site_counters.get();
    }
    else if constexpr (Id == StateSetId::SiteObjectiveMeta)
    {
        return state.site_objective_meta.get();
    }
    else if constexpr (Id == StateSetId::SiteObjectiveTargetTileIndices)
    {
        return state.site_objective_target_tile_indices.get();
    }
    else if constexpr (Id == StateSetId::SiteObjectiveTargetTileMask)
    {
        return state.site_objective_target_tile_mask.get();
    }
    else if constexpr (Id == StateSetId::SiteObjectiveConnectionStartTileIndices)
    {
        return state.site_objective_connection_start_tile_indices.get();
    }
    else if constexpr (Id == StateSetId::SiteObjectiveConnectionStartTileMask)
    {
        return state.site_objective_connection_start_tile_mask.get();
    }
    else if constexpr (Id == StateSetId::SiteObjectiveConnectionGoalTileIndices)
    {
        return state.site_objective_connection_goal_tile_indices.get();
    }
    else if constexpr (Id == StateSetId::SiteObjectiveConnectionGoalTileMask)
    {
        return state.site_objective_connection_goal_tile_mask.get();
    }
    else if constexpr (Id == StateSetId::SiteLocalWeatherResolveMeta)
    {
        return state.site_local_weather_resolve_meta.get();
    }
    else if constexpr (Id == StateSetId::SiteLocalWeatherResolveLastTotalContributions)
    {
        return state.site_local_weather_resolve_last_total_contributions.get();
    }
    else if constexpr (Id == StateSetId::SitePlantWeatherContributionMeta)
    {
        return state.site_plant_weather_contribution_meta.get();
    }
    else if constexpr (Id == StateSetId::SitePlantWeatherContributionDirtyTileIndices)
    {
        return state.site_plant_weather_contribution_dirty_tile_indices.get();
    }
    else if constexpr (Id == StateSetId::SitePlantWeatherContributionDirtyTileMask)
    {
        return state.site_plant_weather_contribution_dirty_tile_mask.get();
    }
    else if constexpr (Id == StateSetId::SiteDeviceWeatherContributionMeta)
    {
        return state.site_device_weather_contribution_meta.get();
    }
    else if constexpr (Id == StateSetId::SiteDeviceWeatherContributionDirtyTileIndices)
    {
        return state.site_device_weather_contribution_dirty_tile_indices.get();
    }
    else if constexpr (Id == StateSetId::SiteDeviceWeatherContributionDirtyTileMask)
    {
        return state.site_device_weather_contribution_dirty_tile_mask.get();
    }
}
}  // namespace gs1
