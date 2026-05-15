#pragma once

#include "messages/game_message.h"
#include "runtime/runtime_clock.h"
#include "runtime/state_set.h"
#include "gs1/types.h"

#include <deque>

namespace gs1
{
struct GameState final
{
    RuntimeFixedStepSecondsStateSet fixed_step_seconds {k_default_fixed_step_seconds};
    RuntimeAppStateSet app_state {GS1_APP_STATE_BOOT};
    RuntimeMoveDirectionStateSet move_direction {};

    RuntimeCampaignCoreStateSet campaign_core {};
    RuntimeCampaignRegionalMapMetaStateSet campaign_regional_map_meta {};
    RuntimeCampaignRegionalMapRevealedSitesStateSet campaign_regional_map_revealed_sites {};
    RuntimeCampaignRegionalMapAvailableSitesStateSet campaign_regional_map_available_sites {};
    RuntimeCampaignRegionalMapCompletedSitesStateSet campaign_regional_map_completed_sites {};
    RuntimeCampaignFactionProgressStateSet campaign_faction_progress {};
    RuntimeCampaignTechnologyStateSet campaign_technology {};
    RuntimeCampaignLoadoutPlannerMetaStateSet campaign_loadout_planner_meta {};
    RuntimeCampaignLoadoutPlannerBaselineItemsStateSet campaign_loadout_planner_baseline_items {};
    RuntimeCampaignLoadoutPlannerAvailableSupportItemsStateSet
        campaign_loadout_planner_available_support_items {};
    RuntimeCampaignLoadoutPlannerSelectedSlotsStateSet campaign_loadout_planner_selected_slots {};
    RuntimeCampaignLoadoutPlannerNearbyAuraModifiersStateSet
        campaign_loadout_planner_nearby_aura_modifiers {};
    RuntimeCampaignSiteMetaEntriesStateSet campaign_site_meta_entries {};
    RuntimeCampaignSiteAdjacentIdsStateSet campaign_site_adjacent_ids {};
    RuntimeCampaignSiteExportedSupportItemsStateSet campaign_site_exported_support_items {};
    RuntimeCampaignSiteNearbyAuraModifierIdsStateSet campaign_site_nearby_aura_modifier_ids {};

    RuntimeSiteRunMetaStateSet site_run_meta {};
    RuntimeSiteClockStateSet site_clock {};
    RuntimeSiteCampStateSet site_camp {};
    RuntimeSiteInventoryMetaStateSet site_inventory_meta {};
    RuntimeSiteInventoryStorageContainersStateSet site_inventory_storage_containers {};
    RuntimeSiteInventoryStorageSlotItemIdsStateSet site_inventory_storage_slot_item_ids {};
    RuntimeSiteInventoryWorkerPackSlotsStateSet site_inventory_worker_pack_slots {};
    RuntimeSiteInventoryPendingDeliveriesStateSet site_inventory_pending_deliveries {};
    RuntimeSiteInventoryPendingDeliveryItemStacksStateSet site_inventory_pending_delivery_item_stacks {};
    RuntimeSiteContractorMetaStateSet site_contractor_meta {};
    RuntimeSiteContractorWorkOrdersStateSet site_contractor_work_orders {};
    RuntimeSiteWeatherStateSet site_weather {};
    RuntimeSiteEventStateSet site_event {};
    RuntimeSiteTaskBoardMetaStateSet site_task_board_meta {};
    RuntimeSiteTaskBoardVisibleTasksStateSet site_task_board_visible_tasks {};
    RuntimeSiteTaskBoardRewardDraftOptionsStateSet site_task_board_reward_draft_options {};
    RuntimeSiteTaskBoardTrackedTilesStateSet site_task_board_tracked_tiles {};
    RuntimeSiteTaskBoardAcceptedTaskIdsStateSet site_task_board_accepted_task_ids {};
    RuntimeSiteTaskBoardCompletedTaskIdsStateSet site_task_board_completed_task_ids {};
    RuntimeSiteTaskBoardClaimedTaskIdsStateSet site_task_board_claimed_task_ids {};
    RuntimeSiteModifierMetaStateSet site_modifier_meta {};
    RuntimeSiteModifierNearbyAuraIdsStateSet site_modifier_nearby_aura_ids {};
    RuntimeSiteModifierActiveSiteModifiersStateSet site_modifier_active_site_modifiers {};
    RuntimeSiteEconomyMetaStateSet site_economy_meta {};
    RuntimeSiteEconomyRevealedUnlockableIdsStateSet site_economy_revealed_unlockable_ids {};
    RuntimeSiteEconomyDirectPurchaseUnlockableIdsStateSet
        site_economy_direct_purchase_unlockable_ids {};
    RuntimeSiteEconomyPhoneListingsStateSet site_economy_phone_listings {};
    RuntimeSiteCraftDeviceCacheRuntimeStateSet site_craft_device_cache_runtime {};
    RuntimeSiteCraftDeviceCachesStateSet site_craft_device_caches {};
    RuntimeSiteCraftNearbyItemsStateSet site_craft_nearby_items {};
    RuntimeSiteCraftPhoneCacheMetaStateSet site_craft_phone_cache_meta {};
    RuntimeSiteCraftPhoneItemsStateSet site_craft_phone_items {};
    RuntimeSiteCraftContextMetaStateSet site_craft_context_meta {};
    RuntimeSiteCraftContextOptionsStateSet site_craft_context_options {};
    RuntimeSiteActionMetaStateSet site_action_meta {};
    RuntimeSiteActionReservedInputItemStacksStateSet site_action_reserved_input_item_stacks {};
    RuntimeSiteActionResolvedHarvestOutputsStateSet site_action_resolved_harvest_outputs {};
    RuntimeSiteCountersStateSet site_counters {};
    RuntimeSiteObjectiveMetaStateSet site_objective_meta {};
    RuntimeSiteObjectiveTargetTileIndicesStateSet site_objective_target_tile_indices {};
    RuntimeSiteObjectiveTargetTileMaskStateSet site_objective_target_tile_mask {};
    RuntimeSiteObjectiveConnectionStartTileIndicesStateSet site_objective_connection_start_tile_indices {};
    RuntimeSiteObjectiveConnectionStartTileMaskStateSet site_objective_connection_start_tile_mask {};
    RuntimeSiteObjectiveConnectionGoalTileIndicesStateSet site_objective_connection_goal_tile_indices {};
    RuntimeSiteObjectiveConnectionGoalTileMaskStateSet site_objective_connection_goal_tile_mask {};
    RuntimeSiteLocalWeatherResolveMetaStateSet site_local_weather_resolve_meta {};
    RuntimeSiteLocalWeatherResolveLastTotalContributionsStateSet
        site_local_weather_resolve_last_total_contributions {};
    RuntimeSitePlantWeatherContributionMetaStateSet site_plant_weather_contribution_meta {};
    RuntimeSitePlantWeatherContributionDirtyTileIndicesStateSet
        site_plant_weather_contribution_dirty_tile_indices {};
    RuntimeSitePlantWeatherContributionDirtyTileMaskStateSet
        site_plant_weather_contribution_dirty_tile_mask {};
    RuntimeSiteDeviceWeatherContributionMetaStateSet site_device_weather_contribution_meta {};
    RuntimeSiteDeviceWeatherContributionDirtyTileIndicesStateSet
        site_device_weather_contribution_dirty_tile_indices {};
    RuntimeSiteDeviceWeatherContributionDirtyTileMaskStateSet
        site_device_weather_contribution_dirty_tile_mask {};

    GameMessageQueue message_queue {};
    std::deque<Gs1RuntimeMessage> runtime_messages {};
};
}  // namespace gs1
