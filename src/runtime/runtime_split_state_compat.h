#pragma once

#include "campaign/campaign_state.h"
#include "runtime/game_state.h"
#include "runtime/state_manager.h"
#include "site/site_run_state.h"

#include <algorithm>
#include <cstddef>
#include <optional>

namespace gs1
{
[[nodiscard]] inline CraftState assemble_craft_state_from_state_sets(
    const GameState& state,
    const StateManager& state_manager)
{
    CraftState craft {};

    const auto& device_cache_runtime =
        state_manager.query<StateSetId::SiteCraftDeviceCacheRuntime>(state);
    const auto& device_caches = state_manager.query<StateSetId::SiteCraftDeviceCaches>(state);
    const auto& nearby_items = state_manager.query<StateSetId::SiteCraftNearbyItems>(state);
    const auto& phone_cache_meta =
        state_manager.query<StateSetId::SiteCraftPhoneCacheMeta>(state);
    const auto& phone_items = state_manager.query<StateSetId::SiteCraftPhoneItems>(state);
    const auto& context_meta = state_manager.query<StateSetId::SiteCraftContextMeta>(state);
    const auto& context_options =
        state_manager.query<StateSetId::SiteCraftContextOptions>(state);

    if (device_cache_runtime.has_value())
    {
        static_cast<CraftDeviceCacheRuntimeState&>(craft) = *device_cache_runtime;
    }
    if (phone_cache_meta.has_value())
    {
        static_cast<PhoneInventoryCacheMetaState&>(craft.phone_cache) = *phone_cache_meta;
    }
    if (phone_items.has_value())
    {
        craft.phone_cache.item_instance_ids = *phone_items;
    }
    if (context_meta.has_value())
    {
        static_cast<CraftContextMetaState&>(craft.context) = *context_meta;
    }
    if (context_options.has_value())
    {
        craft.context.options = *context_options;
    }

    if (device_caches.has_value())
    {
        craft.device_caches.reserve(device_caches->size());
        for (const auto& entry : *device_caches)
        {
            CraftDeviceCacheState cache {};
            static_cast<CraftDeviceCacheEntryState&>(cache) = entry;
            if (nearby_items.has_value())
            {
                const auto offset = static_cast<std::size_t>(entry.nearby_item_offset);
                const auto count = static_cast<std::size_t>(entry.nearby_item_count);
                if (offset <= nearby_items->size())
                {
                    const auto remaining = nearby_items->size() - offset;
                    const auto safe_count = std::min(count, remaining);
                    cache.nearby_item_instance_ids.insert(
                        cache.nearby_item_instance_ids.end(),
                        nearby_items->begin() + static_cast<std::ptrdiff_t>(offset),
                        nearby_items->begin() + static_cast<std::ptrdiff_t>(offset + safe_count));
                }
            }
            craft.device_caches.push_back(std::move(cache));
        }
    }

    return craft;
}

inline void write_craft_state_to_state_sets(
    const CraftState& craft,
    GameState& state,
    const StateManager& state_manager)
{
    state_manager.state<StateSetId::SiteCraftDeviceCacheRuntime>(state) =
        CraftDeviceCacheRuntimeState {
            craft.device_cache_source_membership_revision,
            craft.device_cache_worker_tile,
            craft.device_caches_dirty};

    state_manager.state<StateSetId::SiteCraftPhoneCacheMeta>(state) =
        PhoneInventoryCacheMetaState {craft.phone_cache.source_membership_revision};
    state_manager.state<StateSetId::SiteCraftPhoneItems>(state) =
        craft.phone_cache.item_instance_ids;

    state_manager.state<StateSetId::SiteCraftContextMeta>(state) =
        CraftContextMetaState {craft.context.occupied, craft.context.tile_coord};
    state_manager.state<StateSetId::SiteCraftContextOptions>(state) = craft.context.options;

    std::vector<CraftDeviceCacheEntryState> cache_entries {};
    std::vector<std::uint64_t> cache_nearby_items {};
    cache_entries.reserve(craft.device_caches.size());
    for (const auto& cache : craft.device_caches)
    {
        const auto offset = static_cast<std::uint32_t>(cache_nearby_items.size());
        cache_nearby_items.insert(
            cache_nearby_items.end(),
            cache.nearby_item_instance_ids.begin(),
            cache.nearby_item_instance_ids.end());
        cache_entries.push_back(CraftDeviceCacheEntryState {
            cache.device_entity_id,
            cache.source_membership_revision,
            cache.worker_pack_included,
            offset,
            static_cast<std::uint32_t>(cache.nearby_item_instance_ids.size())});
    }
    state_manager.state<StateSetId::SiteCraftDeviceCaches>(state) = std::move(cache_entries);
    state_manager.state<StateSetId::SiteCraftNearbyItems>(state) = std::move(cache_nearby_items);
}

[[nodiscard]] inline RegionalMapState assemble_regional_map_state_from_state_sets(
    const GameState& state,
    const StateManager& state_manager)
{
    RegionalMapState regional_map {};

    const auto& meta = state_manager.query<StateSetId::CampaignRegionalMapMeta>(state);
    const auto& revealed = state_manager.query<StateSetId::CampaignRegionalMapRevealedSites>(state);
    const auto& available = state_manager.query<StateSetId::CampaignRegionalMapAvailableSites>(state);
    const auto& completed = state_manager.query<StateSetId::CampaignRegionalMapCompletedSites>(state);

    if (meta.has_value() && meta->has_selected_site_id)
    {
        regional_map.selected_site_id = meta->selected_site_id;
    }
    if (revealed.has_value())
    {
        regional_map.revealed_site_ids = *revealed;
    }
    if (available.has_value())
    {
        regional_map.available_site_ids = *available;
    }
    if (completed.has_value())
    {
        regional_map.completed_site_ids = *completed;
    }

    return regional_map;
}

inline void write_regional_map_state_to_state_sets(
    const RegionalMapState& regional_map,
    GameState& state,
    const StateManager& state_manager)
{
    state_manager.state<StateSetId::CampaignRegionalMapMeta>(state) = RegionalMapMetaState {
        regional_map.selected_site_id.value_or(SiteId {}),
        regional_map.selected_site_id.has_value()};
    state_manager.state<StateSetId::CampaignRegionalMapRevealedSites>(state) =
        regional_map.revealed_site_ids;
    state_manager.state<StateSetId::CampaignRegionalMapAvailableSites>(state) =
        regional_map.available_site_ids;
    state_manager.state<StateSetId::CampaignRegionalMapCompletedSites>(state) =
        regional_map.completed_site_ids;
}

[[nodiscard]] inline LoadoutPlannerState assemble_loadout_planner_state_from_state_sets(
    const GameState& state,
    const StateManager& state_manager)
{
    LoadoutPlannerState planner {};

    const auto& meta = state_manager.query<StateSetId::CampaignLoadoutPlannerMeta>(state);
    const auto& baseline =
        state_manager.query<StateSetId::CampaignLoadoutPlannerBaselineItems>(state);
    const auto& available_support =
        state_manager.query<StateSetId::CampaignLoadoutPlannerAvailableSupportItems>(state);
    const auto& selected_slots =
        state_manager.query<StateSetId::CampaignLoadoutPlannerSelectedSlots>(state);
    const auto& nearby_aura =
        state_manager.query<StateSetId::CampaignLoadoutPlannerNearbyAuraModifiers>(state);

    if (meta.has_value())
    {
        if (meta->has_selected_target_site_id)
        {
            planner.selected_target_site_id = meta->selected_target_site_id;
        }
        planner.support_quota_per_contributor = meta->support_quota_per_contributor;
        planner.support_quota = meta->support_quota;
    }
    if (baseline.has_value())
    {
        planner.baseline_deployment_items = *baseline;
    }
    if (available_support.has_value())
    {
        planner.available_exported_support_items = *available_support;
    }
    if (selected_slots.has_value())
    {
        planner.selected_loadout_slots = *selected_slots;
    }
    if (nearby_aura.has_value())
    {
        planner.active_nearby_aura_modifier_ids = *nearby_aura;
    }

    return planner;
}

inline void write_loadout_planner_state_to_state_sets(
    const LoadoutPlannerState& planner,
    GameState& state,
    const StateManager& state_manager)
{
    state_manager.state<StateSetId::CampaignLoadoutPlannerMeta>(state) = LoadoutPlannerMetaState {
        planner.selected_target_site_id.value_or(SiteId {}),
        planner.support_quota_per_contributor,
        planner.support_quota,
        planner.selected_target_site_id.has_value()};
    state_manager.state<StateSetId::CampaignLoadoutPlannerBaselineItems>(state) =
        planner.baseline_deployment_items;
    state_manager.state<StateSetId::CampaignLoadoutPlannerAvailableSupportItems>(state) =
        planner.available_exported_support_items;
    state_manager.state<StateSetId::CampaignLoadoutPlannerSelectedSlots>(state) =
        planner.selected_loadout_slots;
    state_manager.state<StateSetId::CampaignLoadoutPlannerNearbyAuraModifiers>(state) =
        planner.active_nearby_aura_modifier_ids;
}

[[nodiscard]] inline std::vector<SiteMetaState> assemble_sites_state_from_state_sets(
    const GameState& state,
    const StateManager& state_manager)
{
    std::vector<SiteMetaState> sites {};

    const auto& entries = state_manager.query<StateSetId::CampaignSiteMetaEntries>(state);
    const auto& adjacent = state_manager.query<StateSetId::CampaignSiteAdjacentIds>(state);
    const auto& support_items =
        state_manager.query<StateSetId::CampaignSiteExportedSupportItems>(state);
    const auto& aura_ids =
        state_manager.query<StateSetId::CampaignSiteNearbyAuraModifierIds>(state);

    if (!entries.has_value())
    {
        return sites;
    }

    sites.reserve(entries->size());
    for (const auto& entry : *entries)
    {
        SiteMetaState site {};
        site.site_id = entry.site_id;
        site.site_state = entry.site_state;
        site.regional_map_tile = entry.regional_map_tile;
        site.site_archetype_id = entry.site_archetype_id;
        site.featured_faction_id = entry.featured_faction_id;
        site.attempt_count = entry.attempt_count;
        site.support_package_id = entry.support_package_id;
        site.has_support_package_id = entry.has_support_package_id;
        site.completion_reputation_reward = entry.completion_reputation_reward;
        site.completion_faction_reputation_reward = entry.completion_faction_reputation_reward;

        if (adjacent.has_value() &&
            entry.adjacent_site_offset <= adjacent->size())
        {
            const auto offset = static_cast<std::size_t>(entry.adjacent_site_offset);
            const auto count = static_cast<std::size_t>(entry.adjacent_site_count);
            const auto safe_count = std::min(count, adjacent->size() - offset);
            site.adjacent_site_ids.insert(
                site.adjacent_site_ids.end(),
                adjacent->begin() + static_cast<std::ptrdiff_t>(offset),
                adjacent->begin() + static_cast<std::ptrdiff_t>(offset + safe_count));
        }

        if (support_items.has_value() &&
            entry.exported_support_item_offset <= support_items->size())
        {
            const auto offset = static_cast<std::size_t>(entry.exported_support_item_offset);
            const auto count = static_cast<std::size_t>(entry.exported_support_item_count);
            const auto safe_count = std::min(count, support_items->size() - offset);
            site.exported_support_items.insert(
                site.exported_support_items.end(),
                support_items->begin() + static_cast<std::ptrdiff_t>(offset),
                support_items->begin() + static_cast<std::ptrdiff_t>(offset + safe_count));
        }

        if (aura_ids.has_value() &&
            entry.nearby_aura_modifier_offset <= aura_ids->size())
        {
            const auto offset = static_cast<std::size_t>(entry.nearby_aura_modifier_offset);
            const auto count = static_cast<std::size_t>(entry.nearby_aura_modifier_count);
            const auto safe_count = std::min(count, aura_ids->size() - offset);
            site.nearby_aura_modifier_ids.insert(
                site.nearby_aura_modifier_ids.end(),
                aura_ids->begin() + static_cast<std::ptrdiff_t>(offset),
                aura_ids->begin() + static_cast<std::ptrdiff_t>(offset + safe_count));
        }

        sites.push_back(std::move(site));
    }

    return sites;
}

inline void write_sites_state_to_state_sets(
    const std::vector<SiteMetaState>& sites,
    GameState& state,
    const StateManager& state_manager)
{
    std::vector<SiteMetaEntryState> entries {};
    std::vector<SiteId> adjacent_ids {};
    std::vector<LoadoutSlot> support_items {};
    std::vector<ModifierId> aura_ids {};

    entries.reserve(sites.size());
    for (const auto& site : sites)
    {
        const auto adjacent_offset = static_cast<std::uint32_t>(adjacent_ids.size());
        adjacent_ids.insert(
            adjacent_ids.end(),
            site.adjacent_site_ids.begin(),
            site.adjacent_site_ids.end());

        const auto support_offset = static_cast<std::uint32_t>(support_items.size());
        support_items.insert(
            support_items.end(),
            site.exported_support_items.begin(),
            site.exported_support_items.end());

        const auto aura_offset = static_cast<std::uint32_t>(aura_ids.size());
        aura_ids.insert(
            aura_ids.end(),
            site.nearby_aura_modifier_ids.begin(),
            site.nearby_aura_modifier_ids.end());

        entries.push_back(SiteMetaEntryState {
            site.site_id,
            site.site_state,
            site.regional_map_tile,
            site.site_archetype_id,
            site.featured_faction_id,
            site.attempt_count,
            site.support_package_id,
            adjacent_offset,
            static_cast<std::uint32_t>(site.adjacent_site_ids.size()),
            support_offset,
            static_cast<std::uint32_t>(site.exported_support_items.size()),
            aura_offset,
            static_cast<std::uint32_t>(site.nearby_aura_modifier_ids.size()),
            site.completion_reputation_reward,
            site.completion_faction_reputation_reward,
            site.has_support_package_id});
    }

    state_manager.state<StateSetId::CampaignSiteMetaEntries>(state) = std::move(entries);
    state_manager.state<StateSetId::CampaignSiteAdjacentIds>(state) = std::move(adjacent_ids);
    state_manager.state<StateSetId::CampaignSiteExportedSupportItems>(state) =
        std::move(support_items);
    state_manager.state<StateSetId::CampaignSiteNearbyAuraModifierIds>(state) = std::move(aura_ids);
}

[[nodiscard]] inline CampaignState assemble_campaign_state_from_state_sets(
    const GameState& state,
    const StateManager& state_manager)
{
    CampaignState campaign {};

    const auto& core = state_manager.query<StateSetId::CampaignCore>(state);
    const auto& faction_progress =
        state_manager.query<StateSetId::CampaignFactionProgress>(state);
    const auto& technology = state_manager.query<StateSetId::CampaignTechnology>(state);

    if (!core.has_value())
    {
        return campaign;
    }

    campaign.campaign_id = core->campaign_id;
    campaign.campaign_seed = core->campaign_seed;
    campaign.campaign_clock_minutes_elapsed = core->campaign_clock_minutes_elapsed;
    campaign.campaign_days_total = core->campaign_days_total;
    campaign.campaign_days_remaining = core->campaign_days_remaining;
    campaign.app_state = core->app_state;
    campaign.active_site_id = core->active_site_id;
    campaign.regional_map_state = assemble_regional_map_state_from_state_sets(state, state_manager);
    if (faction_progress.has_value())
    {
        campaign.faction_progress = *faction_progress;
    }
    if (technology.has_value())
    {
        campaign.technology_state = *technology;
    }
    campaign.loadout_planner_state =
        assemble_loadout_planner_state_from_state_sets(state, state_manager);
    campaign.sites = assemble_sites_state_from_state_sets(state, state_manager);

    return campaign;
}

inline void write_campaign_state_to_state_sets(
    const std::optional<CampaignState>& campaign,
    GameState& state,
    const StateManager& state_manager)
{
    if (!campaign.has_value())
    {
        state_manager.state<StateSetId::CampaignCore>(state).reset();
        state_manager.state<StateSetId::CampaignRegionalMapMeta>(state).reset();
        state_manager.state<StateSetId::CampaignRegionalMapRevealedSites>(state).reset();
        state_manager.state<StateSetId::CampaignRegionalMapAvailableSites>(state).reset();
        state_manager.state<StateSetId::CampaignRegionalMapCompletedSites>(state).reset();
        state_manager.state<StateSetId::CampaignFactionProgress>(state).reset();
        state_manager.state<StateSetId::CampaignTechnology>(state).reset();
        state_manager.state<StateSetId::CampaignLoadoutPlannerMeta>(state).reset();
        state_manager.state<StateSetId::CampaignLoadoutPlannerBaselineItems>(state).reset();
        state_manager.state<StateSetId::CampaignLoadoutPlannerAvailableSupportItems>(state).reset();
        state_manager.state<StateSetId::CampaignLoadoutPlannerSelectedSlots>(state).reset();
        state_manager.state<StateSetId::CampaignLoadoutPlannerNearbyAuraModifiers>(state).reset();
        state_manager.state<StateSetId::CampaignSiteMetaEntries>(state).reset();
        state_manager.state<StateSetId::CampaignSiteAdjacentIds>(state).reset();
        state_manager.state<StateSetId::CampaignSiteExportedSupportItems>(state).reset();
        state_manager.state<StateSetId::CampaignSiteNearbyAuraModifierIds>(state).reset();
        return;
    }

    state_manager.state<StateSetId::CampaignCore>(state) = CampaignCoreState {
        campaign->campaign_id,
        campaign->campaign_seed,
        campaign->campaign_clock_minutes_elapsed,
        campaign->campaign_days_total,
        campaign->campaign_days_remaining,
        campaign->app_state,
        campaign->active_site_id};
    write_regional_map_state_to_state_sets(campaign->regional_map_state, state, state_manager);
    state_manager.state<StateSetId::CampaignFactionProgress>(state) = campaign->faction_progress;
    state_manager.state<StateSetId::CampaignTechnology>(state) = campaign->technology_state;
    write_loadout_planner_state_to_state_sets(
        campaign->loadout_planner_state,
        state,
        state_manager);
    write_sites_state_to_state_sets(campaign->sites, state, state_manager);
}

[[nodiscard]] inline InventoryState assemble_inventory_state_from_state_sets(
    const GameState& state,
    const StateManager& state_manager)
{
    InventoryState inventory {};

    const auto& meta = state_manager.query<StateSetId::SiteInventoryMeta>(state);
    const auto& storage_containers =
        state_manager.query<StateSetId::SiteInventoryStorageContainers>(state);
    const auto& storage_slot_item_ids =
        state_manager.query<StateSetId::SiteInventoryStorageSlotItemIds>(state);
    const auto& worker_pack_slots =
        state_manager.query<StateSetId::SiteInventoryWorkerPackSlots>(state);
    const auto& pending_deliveries =
        state_manager.query<StateSetId::SiteInventoryPendingDeliveries>(state);
    const auto& pending_item_stacks =
        state_manager.query<StateSetId::SiteInventoryPendingDeliveryItemStacks>(state);

    if (meta.has_value())
    {
        inventory.next_storage_id = meta->next_storage_id;
        inventory.worker_pack_slot_count = meta->worker_pack_slot_count;
        inventory.worker_pack_storage_id = meta->worker_pack_storage_id;
        inventory.worker_pack_container_entity_id = meta->worker_pack_container_entity_id;
        inventory.item_membership_revision = meta->item_membership_revision;
        inventory.item_quantity_revision = meta->item_quantity_revision;
    }
    if (worker_pack_slots.has_value())
    {
        inventory.worker_pack_slots = *worker_pack_slots;
    }
    if (storage_containers.has_value())
    {
        inventory.storage_containers.reserve(storage_containers->size());
        for (const auto& entry : *storage_containers)
        {
            StorageContainerState container {};
            container.storage_id = entry.storage_id;
            container.container_entity_id = entry.container_entity_id;
            container.owner_device_entity_id = entry.owner_device_entity_id;
            container.container_kind = entry.container_kind;
            container.tile_coord = entry.tile_coord;
            container.flags = entry.flags;
            if (storage_slot_item_ids.has_value() &&
                entry.slot_item_offset <= storage_slot_item_ids->size())
            {
                const auto offset = static_cast<std::size_t>(entry.slot_item_offset);
                const auto count = static_cast<std::size_t>(entry.slot_item_count);
                const auto safe_count = std::min(count, storage_slot_item_ids->size() - offset);
                container.slot_item_instance_ids.insert(
                    container.slot_item_instance_ids.end(),
                    storage_slot_item_ids->begin() + static_cast<std::ptrdiff_t>(offset),
                    storage_slot_item_ids->begin() + static_cast<std::ptrdiff_t>(offset + safe_count));
            }
            inventory.storage_containers.push_back(std::move(container));
        }
    }
    if (pending_deliveries.has_value())
    {
        inventory.pending_delivery_queue.reserve(pending_deliveries->size());
        for (const auto& entry : *pending_deliveries)
        {
            PendingDelivery delivery {};
            delivery.delivery_id = entry.delivery_id;
            delivery.state = entry.state;
            if (pending_item_stacks.has_value() &&
                entry.item_stack_offset <= pending_item_stacks->size())
            {
                const auto offset = static_cast<std::size_t>(entry.item_stack_offset);
                const auto count = static_cast<std::size_t>(entry.item_stack_count);
                const auto safe_count = std::min(count, pending_item_stacks->size() - offset);
                delivery.item_stacks.insert(
                    delivery.item_stacks.end(),
                    pending_item_stacks->begin() + static_cast<std::ptrdiff_t>(offset),
                    pending_item_stacks->begin() + static_cast<std::ptrdiff_t>(offset + safe_count));
            }
            inventory.pending_delivery_queue.push_back(std::move(delivery));
        }
    }

    return inventory;
}

inline void write_inventory_state_to_state_sets(
    const InventoryState& inventory,
    GameState& state,
    const StateManager& state_manager)
{
    state_manager.state<StateSetId::SiteInventoryMeta>(state) = InventoryMetaState {
        inventory.next_storage_id,
        inventory.worker_pack_slot_count,
        inventory.worker_pack_storage_id,
        inventory.worker_pack_container_entity_id,
        inventory.item_membership_revision,
        inventory.item_quantity_revision};
    state_manager.state<StateSetId::SiteInventoryWorkerPackSlots>(state) =
        inventory.worker_pack_slots;

    std::vector<StorageContainerEntryState> storage_entries {};
    std::vector<std::uint64_t> slot_item_ids {};
    storage_entries.reserve(inventory.storage_containers.size());
    for (const auto& container : inventory.storage_containers)
    {
        const auto offset = static_cast<std::uint32_t>(slot_item_ids.size());
        slot_item_ids.insert(
            slot_item_ids.end(),
            container.slot_item_instance_ids.begin(),
            container.slot_item_instance_ids.end());
        storage_entries.push_back(StorageContainerEntryState {
            container.storage_id,
            container.container_entity_id,
            container.owner_device_entity_id,
            container.container_kind,
            container.tile_coord,
            container.flags,
            offset,
            static_cast<std::uint32_t>(container.slot_item_instance_ids.size())});
    }
    state_manager.state<StateSetId::SiteInventoryStorageContainers>(state) =
        std::move(storage_entries);
    state_manager.state<StateSetId::SiteInventoryStorageSlotItemIds>(state) =
        std::move(slot_item_ids);

    std::vector<PendingDeliveryEntryState> delivery_entries {};
    std::vector<InventorySlot> delivery_item_stacks {};
    delivery_entries.reserve(inventory.pending_delivery_queue.size());
    for (const auto& delivery : inventory.pending_delivery_queue)
    {
        const auto offset = static_cast<std::uint32_t>(delivery_item_stacks.size());
        delivery_item_stacks.insert(
            delivery_item_stacks.end(),
            delivery.item_stacks.begin(),
            delivery.item_stacks.end());
        delivery_entries.push_back(PendingDeliveryEntryState {
            delivery.delivery_id,
            offset,
            static_cast<std::uint32_t>(delivery.item_stacks.size()),
            delivery.state});
    }
    state_manager.state<StateSetId::SiteInventoryPendingDeliveries>(state) =
        std::move(delivery_entries);
    state_manager.state<StateSetId::SiteInventoryPendingDeliveryItemStacks>(state) =
        std::move(delivery_item_stacks);
}

[[nodiscard]] inline ContractorState assemble_contractor_state_from_state_sets(
    const GameState& state,
    const StateManager& state_manager)
{
    ContractorState contractor {};

    const auto& meta = state_manager.query<StateSetId::SiteContractorMeta>(state);
    const auto& work_orders = state_manager.query<StateSetId::SiteContractorWorkOrders>(state);

    if (meta.has_value())
    {
        contractor.available_work_units = meta->available_work_units;
    }
    if (work_orders.has_value())
    {
        contractor.assigned_work_orders.reserve(work_orders->size());
        for (const auto& entry : *work_orders)
        {
            contractor.assigned_work_orders.push_back(WorkOrderState {
                entry.work_order_id,
                entry.task_kind,
                entry.has_target_tile ? std::optional<TileCoord> {entry.target_tile} : std::nullopt,
                entry.remaining_work_units});
        }
    }

    return contractor;
}

inline void write_contractor_state_to_state_sets(
    const ContractorState& contractor,
    GameState& state,
    const StateManager& state_manager)
{
    state_manager.state<StateSetId::SiteContractorMeta>(state) =
        ContractorMetaState {contractor.available_work_units};

    std::vector<WorkOrderEntryState> work_orders {};
    work_orders.reserve(contractor.assigned_work_orders.size());
    for (const auto& order : contractor.assigned_work_orders)
    {
        work_orders.push_back(WorkOrderEntryState {
            order.work_order_id,
            order.task_kind,
            order.target_tile.value_or(TileCoord {}),
            order.remaining_work_units,
            order.target_tile.has_value()});
    }
    state_manager.state<StateSetId::SiteContractorWorkOrders>(state) = std::move(work_orders);
}

[[nodiscard]] inline TaskBoardState assemble_task_board_state_from_state_sets(
    const GameState& state,
    const StateManager& state_manager)
{
    TaskBoardState board {};

    const auto& meta = state_manager.query<StateSetId::SiteTaskBoardMeta>(state);
    const auto& visible_tasks =
        state_manager.query<StateSetId::SiteTaskBoardVisibleTasks>(state);
    const auto& reward_options =
        state_manager.query<StateSetId::SiteTaskBoardRewardDraftOptions>(state);
    const auto& tracked_tiles =
        state_manager.query<StateSetId::SiteTaskBoardTrackedTiles>(state);
    const auto& accepted_task_ids =
        state_manager.query<StateSetId::SiteTaskBoardAcceptedTaskIds>(state);
    const auto& completed_task_ids =
        state_manager.query<StateSetId::SiteTaskBoardCompletedTaskIds>(state);
    const auto& claimed_task_ids =
        state_manager.query<StateSetId::SiteTaskBoardClaimedTaskIds>(state);

    if (meta.has_value())
    {
        board.task_pool_size = meta->task_pool_size;
        board.accepted_task_cap = meta->accepted_task_cap;
        board.tracked_tile_width = meta->tracked_tile_width;
        board.tracked_tile_height = meta->tracked_tile_height;
        board.fully_grown_tile_count = meta->fully_grown_tile_count;
        board.site_completion_tile_threshold = meta->site_completion_tile_threshold;
        board.tracked_living_plant_count = meta->tracked_living_plant_count;
        board.refresh_generation = meta->refresh_generation;
        board.minutes_until_next_refresh = meta->minutes_until_next_refresh;
        board.worker = meta->worker;
        board.all_tracked_living_plants_stable = meta->all_tracked_living_plants_stable;
        if (meta->has_active_chain_state)
        {
            board.active_chain_state = TaskChainState {
                meta->active_chain_task_chain_id,
                meta->active_chain_has_current_accepted_step_index
                    ? std::optional<std::uint32_t> {meta->active_chain_current_accepted_step_index}
                    : std::nullopt,
                meta->active_chain_has_surfaced_follow_up_task_instance_id
                    ? std::optional<TaskInstanceId> {meta->active_chain_surfaced_follow_up_task_instance_id}
                    : std::nullopt,
                meta->active_chain_is_broken};
        }
    }
    if (tracked_tiles.has_value())
    {
        board.tracked_tiles = *tracked_tiles;
    }
    if (accepted_task_ids.has_value())
    {
        board.accepted_task_ids = *accepted_task_ids;
    }
    if (completed_task_ids.has_value())
    {
        board.completed_task_ids = *completed_task_ids;
    }
    if (claimed_task_ids.has_value())
    {
        board.claimed_task_ids = *claimed_task_ids;
    }
    if (visible_tasks.has_value())
    {
        board.visible_tasks.reserve(visible_tasks->size());
        for (const auto& entry : *visible_tasks)
        {
            TaskInstanceState task {};
            task.task_instance_id = entry.task_instance_id;
            task.task_template_id = entry.task_template_id;
            task.publisher_faction_id = entry.publisher_faction_id;
            task.task_tier_id = entry.task_tier_id;
            task.target_amount = entry.target_amount;
            task.required_count = entry.required_count;
            task.current_progress_amount = entry.current_progress_amount;
            task.item_id = entry.item_id;
            task.plant_id = entry.plant_id;
            task.recipe_id = entry.recipe_id;
            task.structure_id = entry.structure_id;
            task.secondary_structure_id = entry.secondary_structure_id;
            task.tertiary_structure_id = entry.tertiary_structure_id;
            task.action_kind = entry.action_kind;
            task.threshold_value = entry.threshold_value;
            task.expected_task_hours_in_game = entry.expected_task_hours_in_game;
            task.risk_multiplier = entry.risk_multiplier;
            task.direct_cost_cash_points = entry.direct_cost_cash_points;
            task.time_cost_cash_points = entry.time_cost_cash_points;
            task.risk_cost_cash_points = entry.risk_cost_cash_points;
            task.difficulty_cash_points = entry.difficulty_cash_points;
            task.reward_budget_cash_points = entry.reward_budget_cash_points;
            task.runtime_list_kind = entry.runtime_list_kind;
            task.chain_id = entry.chain_id;
            task.chain_step_index = entry.chain_step_index;
            task.follow_up_task_template_id = entry.follow_up_task_template_id;
            task.progress_accumulator = entry.progress_accumulator;
            task.requirement_mask = entry.requirement_mask;
            task.has_chain = entry.has_chain;
            task.has_follow_up_task_template = entry.has_follow_up_task_template;
            if (reward_options.has_value() &&
                entry.reward_draft_option_offset <= reward_options->size())
            {
                const auto offset = static_cast<std::size_t>(entry.reward_draft_option_offset);
                const auto count = static_cast<std::size_t>(entry.reward_draft_option_count);
                const auto safe_count = std::min(count, reward_options->size() - offset);
                task.reward_draft_options.insert(
                    task.reward_draft_options.end(),
                    reward_options->begin() + static_cast<std::ptrdiff_t>(offset),
                    reward_options->begin() + static_cast<std::ptrdiff_t>(offset + safe_count));
            }
            board.visible_tasks.push_back(std::move(task));
        }
    }

    return board;
}

inline void write_task_board_state_to_state_sets(
    const TaskBoardState& board,
    GameState& state,
    const StateManager& state_manager)
{
    TaskBoardMetaState meta {};
    meta.task_pool_size = board.task_pool_size;
    meta.accepted_task_cap = board.accepted_task_cap;
    meta.tracked_tile_width = board.tracked_tile_width;
    meta.tracked_tile_height = board.tracked_tile_height;
    meta.fully_grown_tile_count = board.fully_grown_tile_count;
    meta.site_completion_tile_threshold = board.site_completion_tile_threshold;
    meta.tracked_living_plant_count = board.tracked_living_plant_count;
    meta.refresh_generation = board.refresh_generation;
    meta.minutes_until_next_refresh = board.minutes_until_next_refresh;
    meta.worker = board.worker;
    meta.all_tracked_living_plants_stable = board.all_tracked_living_plants_stable;
    if (board.active_chain_state.has_value())
    {
        meta.has_active_chain_state = true;
        meta.active_chain_task_chain_id = board.active_chain_state->task_chain_id;
        meta.active_chain_has_current_accepted_step_index =
            board.active_chain_state->current_accepted_step_index.has_value();
        meta.active_chain_current_accepted_step_index =
            board.active_chain_state->current_accepted_step_index.value_or(0U);
        meta.active_chain_has_surfaced_follow_up_task_instance_id =
            board.active_chain_state->surfaced_follow_up_task_instance_id.has_value();
        meta.active_chain_surfaced_follow_up_task_instance_id =
            board.active_chain_state->surfaced_follow_up_task_instance_id.value_or(TaskInstanceId {});
        meta.active_chain_is_broken = board.active_chain_state->is_broken;
    }
    state_manager.state<StateSetId::SiteTaskBoardMeta>(state) = meta;
    state_manager.state<StateSetId::SiteTaskBoardTrackedTiles>(state) = board.tracked_tiles;
    state_manager.state<StateSetId::SiteTaskBoardAcceptedTaskIds>(state) = board.accepted_task_ids;
    state_manager.state<StateSetId::SiteTaskBoardCompletedTaskIds>(state) =
        board.completed_task_ids;
    state_manager.state<StateSetId::SiteTaskBoardClaimedTaskIds>(state) = board.claimed_task_ids;

    std::vector<TaskInstanceEntryState> task_entries {};
    std::vector<TaskRewardDraftOption> reward_entries {};
    task_entries.reserve(board.visible_tasks.size());
    for (const auto& task : board.visible_tasks)
    {
        const auto offset = static_cast<std::uint32_t>(reward_entries.size());
        reward_entries.insert(
            reward_entries.end(),
            task.reward_draft_options.begin(),
            task.reward_draft_options.end());
        TaskInstanceEntryState entry {};
        entry.task_instance_id = task.task_instance_id;
        entry.task_template_id = task.task_template_id;
        entry.publisher_faction_id = task.publisher_faction_id;
        entry.task_tier_id = task.task_tier_id;
        entry.target_amount = task.target_amount;
        entry.required_count = task.required_count;
        entry.current_progress_amount = task.current_progress_amount;
        entry.item_id = task.item_id;
        entry.plant_id = task.plant_id;
        entry.recipe_id = task.recipe_id;
        entry.structure_id = task.structure_id;
        entry.secondary_structure_id = task.secondary_structure_id;
        entry.tertiary_structure_id = task.tertiary_structure_id;
        entry.action_kind = task.action_kind;
        entry.threshold_value = task.threshold_value;
        entry.expected_task_hours_in_game = task.expected_task_hours_in_game;
        entry.risk_multiplier = task.risk_multiplier;
        entry.direct_cost_cash_points = task.direct_cost_cash_points;
        entry.time_cost_cash_points = task.time_cost_cash_points;
        entry.risk_cost_cash_points = task.risk_cost_cash_points;
        entry.difficulty_cash_points = task.difficulty_cash_points;
        entry.reward_budget_cash_points = task.reward_budget_cash_points;
        entry.runtime_list_kind = task.runtime_list_kind;
        entry.chain_id = task.chain_id;
        entry.chain_step_index = task.chain_step_index;
        entry.follow_up_task_template_id = task.follow_up_task_template_id;
        entry.progress_accumulator = task.progress_accumulator;
        entry.requirement_mask = task.requirement_mask;
        entry.has_chain = task.has_chain;
        entry.has_follow_up_task_template = task.has_follow_up_task_template;
        entry.reward_draft_option_offset = offset;
        entry.reward_draft_option_count =
            static_cast<std::uint32_t>(task.reward_draft_options.size());
        task_entries.push_back(entry);
    }
    state_manager.state<StateSetId::SiteTaskBoardVisibleTasks>(state) = std::move(task_entries);
    state_manager.state<StateSetId::SiteTaskBoardRewardDraftOptions>(state) =
        std::move(reward_entries);
}

[[nodiscard]] inline ModifierState assemble_modifier_state_from_state_sets(
    const GameState& state,
    const StateManager& state_manager)
{
    ModifierState modifier {};

    const auto& meta = state_manager.query<StateSetId::SiteModifierMeta>(state);
    const auto& nearby_aura_ids =
        state_manager.query<StateSetId::SiteModifierNearbyAuraIds>(state);
    const auto& active_modifiers =
        state_manager.query<StateSetId::SiteModifierActiveSiteModifiers>(state);

    if (meta.has_value())
    {
        modifier.resolved_channel_totals = meta->resolved_channel_totals;
        modifier.resolved_terrain_factor_modifiers = meta->resolved_terrain_factor_modifiers;
        modifier.resolved_action_cost_modifiers = meta->resolved_action_cost_modifiers;
        modifier.resolved_harvest_output_modifiers = meta->resolved_harvest_output_modifiers;
        modifier.resolved_village_technology_effects = meta->resolved_village_technology_effects;
        modifier.resolved_bureau_technology_effects = meta->resolved_bureau_technology_effects;
    }
    if (nearby_aura_ids.has_value())
    {
        modifier.active_nearby_aura_modifier_ids = *nearby_aura_ids;
    }
    if (active_modifiers.has_value())
    {
        modifier.active_site_modifiers = *active_modifiers;
    }

    return modifier;
}

inline void write_modifier_state_to_state_sets(
    const ModifierState& modifier,
    GameState& state,
    const StateManager& state_manager)
{
    state_manager.state<StateSetId::SiteModifierMeta>(state) = ModifierMetaState {
        modifier.resolved_channel_totals,
        modifier.resolved_terrain_factor_modifiers,
        modifier.resolved_action_cost_modifiers,
        modifier.resolved_harvest_output_modifiers,
        modifier.resolved_village_technology_effects,
        modifier.resolved_bureau_technology_effects};
    state_manager.state<StateSetId::SiteModifierNearbyAuraIds>(state) =
        modifier.active_nearby_aura_modifier_ids;
    state_manager.state<StateSetId::SiteModifierActiveSiteModifiers>(state) =
        modifier.active_site_modifiers;
}

[[nodiscard]] inline EconomyState assemble_economy_state_from_state_sets(
    const GameState& state,
    const StateManager& state_manager)
{
    EconomyState economy {};

    const auto& meta = state_manager.query<StateSetId::SiteEconomyMeta>(state);
    const auto& revealed_unlockables =
        state_manager.query<StateSetId::SiteEconomyRevealedUnlockableIds>(state);
    const auto& direct_purchase_unlockables =
        state_manager.query<StateSetId::SiteEconomyDirectPurchaseUnlockableIds>(state);
    const auto& phone_listings =
        state_manager.query<StateSetId::SiteEconomyPhoneListings>(state);

    if (meta.has_value())
    {
        economy.current_cash = meta->current_cash;
        economy.phone_delivery_fee = meta->phone_delivery_fee;
        economy.phone_delivery_minutes = meta->phone_delivery_minutes;
        economy.phone_listing_source_membership_revision =
            meta->phone_listing_source_membership_revision;
        economy.phone_listing_source_quantity_revision =
            meta->phone_listing_source_quantity_revision;
        economy.phone_listing_source_action_reservation_signature =
            meta->phone_listing_source_action_reservation_signature;
        economy.phone_buy_stock_refresh_generation = meta->phone_buy_stock_refresh_generation;
    }
    if (revealed_unlockables.has_value())
    {
        economy.revealed_site_unlockable_ids = *revealed_unlockables;
    }
    if (direct_purchase_unlockables.has_value())
    {
        economy.direct_purchase_unlockable_ids = *direct_purchase_unlockables;
    }
    if (phone_listings.has_value())
    {
        economy.available_phone_listings = *phone_listings;
    }

    return economy;
}

inline void write_economy_state_to_state_sets(
    const EconomyState& economy,
    GameState& state,
    const StateManager& state_manager)
{
    state_manager.state<StateSetId::SiteEconomyMeta>(state) = EconomyMetaState {
        economy.current_cash,
        economy.phone_delivery_fee,
        economy.phone_delivery_minutes,
        economy.phone_listing_source_membership_revision,
        economy.phone_listing_source_quantity_revision,
        economy.phone_listing_source_action_reservation_signature,
        economy.phone_buy_stock_refresh_generation};
    state_manager.state<StateSetId::SiteEconomyRevealedUnlockableIds>(state) =
        economy.revealed_site_unlockable_ids;
    state_manager.state<StateSetId::SiteEconomyDirectPurchaseUnlockableIds>(state) =
        economy.direct_purchase_unlockable_ids;
    state_manager.state<StateSetId::SiteEconomyPhoneListings>(state) =
        economy.available_phone_listings;
}

[[nodiscard]] inline ActionState assemble_action_state_from_state_sets(
    const GameState& state,
    const StateManager& state_manager)
{
    ActionState action {};

    const auto& meta = state_manager.query<StateSetId::SiteActionMeta>(state);
    const auto& reserved_inputs =
        state_manager.query<StateSetId::SiteActionReservedInputItemStacks>(state);
    const auto& harvest_outputs =
        state_manager.query<StateSetId::SiteActionResolvedHarvestOutputs>(state);

    if (meta.has_value())
    {
        if (meta->has_current_action_id)
        {
            action.current_action_id = meta->current_action_id;
        }
        action.action_kind = meta->action_kind;
        if (meta->has_target_tile)
        {
            action.target_tile = meta->target_tile;
        }
        if (meta->has_approach_tile)
        {
            action.approach_tile = meta->approach_tile;
        }
        action.primary_subject_id = meta->primary_subject_id;
        action.secondary_subject_id = meta->secondary_subject_id;
        action.item_id = meta->item_id;
        action.placement_reservation_token = meta->placement_reservation_token;
        action.quantity = meta->quantity;
        action.request_flags = meta->request_flags;
        action.awaiting_placement_reservation = meta->awaiting_placement_reservation;
        action.reactivate_placement_mode_on_completion =
            meta->reactivate_placement_mode_on_completion;
        action.total_action_minutes = meta->total_action_minutes;
        action.remaining_action_minutes = meta->remaining_action_minutes;
        action.deferred_meter_delta = meta->deferred_meter_delta;
        action.resolved_harvest_density = meta->resolved_harvest_density;
        action.resolved_harvest_outputs_valid = meta->resolved_harvest_outputs_valid;
        if (meta->has_started_at_world_minute)
        {
            action.started_at_world_minute = meta->started_at_world_minute;
        }
        action.placement_mode.active = meta->placement_mode.active;
        action.placement_mode.action_kind = meta->placement_mode.action_kind;
        if (meta->placement_mode.has_target_tile)
        {
            action.placement_mode.target_tile = meta->placement_mode.target_tile;
        }
        action.placement_mode.primary_subject_id = meta->placement_mode.primary_subject_id;
        action.placement_mode.secondary_subject_id = meta->placement_mode.secondary_subject_id;
        action.placement_mode.item_id = meta->placement_mode.item_id;
        action.placement_mode.quantity = meta->placement_mode.quantity;
        action.placement_mode.request_flags = meta->placement_mode.request_flags;
        action.placement_mode.footprint_width = meta->placement_mode.footprint_width;
        action.placement_mode.footprint_height = meta->placement_mode.footprint_height;
        action.placement_mode.blocked_mask = meta->placement_mode.blocked_mask;
    }
    if (reserved_inputs.has_value())
    {
        action.reserved_input_item_stacks = *reserved_inputs;
    }
    if (harvest_outputs.has_value())
    {
        action.resolved_harvest_outputs = *harvest_outputs;
    }

    return action;
}

inline void write_action_state_to_state_sets(
    const ActionState& action,
    GameState& state,
    const StateManager& state_manager)
{
    state_manager.state<StateSetId::SiteActionMeta>(state) = ActionMetaState {
        action.current_action_id.value_or(RuntimeActionId {}),
        action.action_kind,
        action.target_tile.value_or(TileCoord {}),
        action.approach_tile.value_or(TileCoord {}),
        action.primary_subject_id,
        action.secondary_subject_id,
        action.item_id,
        action.placement_reservation_token,
        action.quantity,
        action.request_flags,
        action.awaiting_placement_reservation,
        action.reactivate_placement_mode_on_completion,
        action.total_action_minutes,
        action.remaining_action_minutes,
        action.deferred_meter_delta,
        action.resolved_harvest_density,
        action.resolved_harvest_outputs_valid,
        action.started_at_world_minute.value_or(0.0),
        PlacementModeMetaState {
            action.placement_mode.active,
            action.placement_mode.action_kind,
            action.placement_mode.target_tile.value_or(TileCoord {}),
            action.placement_mode.primary_subject_id,
            action.placement_mode.secondary_subject_id,
            action.placement_mode.item_id,
            action.placement_mode.quantity,
            action.placement_mode.request_flags,
            action.placement_mode.footprint_width,
            action.placement_mode.footprint_height,
            {},
            action.placement_mode.blocked_mask,
            action.placement_mode.target_tile.has_value()},
        action.current_action_id.has_value(),
        action.target_tile.has_value(),
        action.approach_tile.has_value(),
        action.started_at_world_minute.has_value()};
    state_manager.state<StateSetId::SiteActionReservedInputItemStacks>(state) =
        action.reserved_input_item_stacks;
    state_manager.state<StateSetId::SiteActionResolvedHarvestOutputs>(state) =
        action.resolved_harvest_outputs;
}

[[nodiscard]] inline SiteObjectiveState assemble_site_objective_state_from_state_sets(
    const GameState& state,
    const StateManager& state_manager)
{
    SiteObjectiveState objective {};

    const auto& meta = state_manager.query<StateSetId::SiteObjectiveMeta>(state);
    const auto& target_indices =
        state_manager.query<StateSetId::SiteObjectiveTargetTileIndices>(state);
    const auto& target_mask =
        state_manager.query<StateSetId::SiteObjectiveTargetTileMask>(state);
    const auto& start_indices =
        state_manager.query<StateSetId::SiteObjectiveConnectionStartTileIndices>(state);
    const auto& start_mask =
        state_manager.query<StateSetId::SiteObjectiveConnectionStartTileMask>(state);
    const auto& goal_indices =
        state_manager.query<StateSetId::SiteObjectiveConnectionGoalTileIndices>(state);
    const auto& goal_mask =
        state_manager.query<StateSetId::SiteObjectiveConnectionGoalTileMask>(state);

    if (meta.has_value())
    {
        objective.type = meta->type;
        objective.time_limit_minutes = meta->time_limit_minutes;
        objective.completion_hold_minutes = meta->completion_hold_minutes;
        objective.completion_hold_progress_minutes = meta->completion_hold_progress_minutes;
        objective.paused_main_timer_minutes = meta->paused_main_timer_minutes;
        objective.last_evaluated_world_time_minutes = meta->last_evaluated_world_time_minutes;
        objective.target_cash_points = meta->target_cash_points;
        objective.target_edge = meta->target_edge;
        objective.target_band_width = meta->target_band_width;
        objective.highway_max_average_sand_cover = meta->highway_max_average_sand_cover;
        objective.last_target_average_sand_level = meta->last_target_average_sand_level;
        objective.has_hold_baseline = meta->has_hold_baseline;
    }
    if (target_indices.has_value())
    {
        objective.target_tile_indices = *target_indices;
    }
    if (target_mask.has_value())
    {
        objective.target_tile_mask = *target_mask;
    }
    if (start_indices.has_value())
    {
        objective.connection_start_tile_indices = *start_indices;
    }
    if (start_mask.has_value())
    {
        objective.connection_start_tile_mask = *start_mask;
    }
    if (goal_indices.has_value())
    {
        objective.connection_goal_tile_indices = *goal_indices;
    }
    if (goal_mask.has_value())
    {
        objective.connection_goal_tile_mask = *goal_mask;
    }

    return objective;
}

inline void write_site_objective_state_to_state_sets(
    const SiteObjectiveState& objective,
    GameState& state,
    const StateManager& state_manager)
{
    state_manager.state<StateSetId::SiteObjectiveMeta>(state) = SiteObjectiveMetaState {
        objective.type,
        objective.time_limit_minutes,
        objective.completion_hold_minutes,
        objective.completion_hold_progress_minutes,
        objective.paused_main_timer_minutes,
        objective.last_evaluated_world_time_minutes,
        objective.target_cash_points,
        objective.target_edge,
        objective.target_band_width,
        objective.highway_max_average_sand_cover,
        objective.last_target_average_sand_level,
        objective.has_hold_baseline};
    state_manager.state<StateSetId::SiteObjectiveTargetTileIndices>(state) =
        objective.target_tile_indices;
    state_manager.state<StateSetId::SiteObjectiveTargetTileMask>(state) =
        objective.target_tile_mask;
    state_manager.state<StateSetId::SiteObjectiveConnectionStartTileIndices>(state) =
        objective.connection_start_tile_indices;
    state_manager.state<StateSetId::SiteObjectiveConnectionStartTileMask>(state) =
        objective.connection_start_tile_mask;
    state_manager.state<StateSetId::SiteObjectiveConnectionGoalTileIndices>(state) =
        objective.connection_goal_tile_indices;
    state_manager.state<StateSetId::SiteObjectiveConnectionGoalTileMask>(state) =
        objective.connection_goal_tile_mask;
}

[[nodiscard]] inline LocalWeatherResolveState assemble_local_weather_resolve_state_from_state_sets(
    const GameState& state,
    const StateManager& state_manager)
{
    LocalWeatherResolveState resolve {};

    const auto& meta = state_manager.query<StateSetId::SiteLocalWeatherResolveMeta>(state);
    const auto& contributions =
        state_manager.query<StateSetId::SiteLocalWeatherResolveLastTotalContributions>(state);
    if (meta.has_value())
    {
        resolve.emit_full_snapshot_on_next_run = meta->emit_full_snapshot_on_next_run;
    }
    if (contributions.has_value())
    {
        resolve.last_total_weather_contributions = *contributions;
    }

    return resolve;
}

inline void write_local_weather_resolve_state_to_state_sets(
    const LocalWeatherResolveState& resolve,
    GameState& state,
    const StateManager& state_manager)
{
    state_manager.state<StateSetId::SiteLocalWeatherResolveMeta>(state) =
        LocalWeatherResolveMetaState {resolve.emit_full_snapshot_on_next_run};
    state_manager.state<StateSetId::SiteLocalWeatherResolveLastTotalContributions>(state) =
        resolve.last_total_weather_contributions;
}

[[nodiscard]] inline PlantWeatherContributionState assemble_plant_weather_contribution_state_from_state_sets(
    const GameState& state,
    const StateManager& state_manager)
{
    PlantWeatherContributionState contribution {};

    const auto& meta =
        state_manager.query<StateSetId::SitePlantWeatherContributionMeta>(state);
    const auto& indices =
        state_manager.query<StateSetId::SitePlantWeatherContributionDirtyTileIndices>(state);
    const auto& mask =
        state_manager.query<StateSetId::SitePlantWeatherContributionDirtyTileMask>(state);
    if (meta.has_value())
    {
        contribution.last_wind_direction_sector = meta->last_wind_direction_sector;
        contribution.full_rebuild_pending = meta->full_rebuild_pending;
    }
    if (indices.has_value())
    {
        contribution.dirty_tile_indices = *indices;
    }
    if (mask.has_value())
    {
        contribution.dirty_tile_mask = *mask;
    }

    return contribution;
}

inline void write_plant_weather_contribution_state_to_state_sets(
    const PlantWeatherContributionState& contribution,
    GameState& state,
    const StateManager& state_manager)
{
    state_manager.state<StateSetId::SitePlantWeatherContributionMeta>(state) =
        PlantWeatherContributionMetaState {
            contribution.last_wind_direction_sector,
            contribution.full_rebuild_pending};
    state_manager.state<StateSetId::SitePlantWeatherContributionDirtyTileIndices>(state) =
        contribution.dirty_tile_indices;
    state_manager.state<StateSetId::SitePlantWeatherContributionDirtyTileMask>(state) =
        contribution.dirty_tile_mask;
}

[[nodiscard]] inline DeviceWeatherContributionState assemble_device_weather_contribution_state_from_state_sets(
    const GameState& state,
    const StateManager& state_manager)
{
    DeviceWeatherContributionState contribution {};

    const auto& meta =
        state_manager.query<StateSetId::SiteDeviceWeatherContributionMeta>(state);
    const auto& indices =
        state_manager.query<StateSetId::SiteDeviceWeatherContributionDirtyTileIndices>(state);
    const auto& mask =
        state_manager.query<StateSetId::SiteDeviceWeatherContributionDirtyTileMask>(state);
    if (meta.has_value())
    {
        contribution.last_wind_direction_sector = meta->last_wind_direction_sector;
        contribution.full_rebuild_pending = meta->full_rebuild_pending;
    }
    if (indices.has_value())
    {
        contribution.dirty_tile_indices = *indices;
    }
    if (mask.has_value())
    {
        contribution.dirty_tile_mask = *mask;
    }

    return contribution;
}

inline void write_device_weather_contribution_state_to_state_sets(
    const DeviceWeatherContributionState& contribution,
    GameState& state,
    const StateManager& state_manager)
{
    state_manager.state<StateSetId::SiteDeviceWeatherContributionMeta>(state) =
        DeviceWeatherContributionMetaState {
            contribution.last_wind_direction_sector,
            contribution.full_rebuild_pending};
    state_manager.state<StateSetId::SiteDeviceWeatherContributionDirtyTileIndices>(state) =
        contribution.dirty_tile_indices;
    state_manager.state<StateSetId::SiteDeviceWeatherContributionDirtyTileMask>(state) =
        contribution.dirty_tile_mask;
}

[[nodiscard]] inline SiteRunState assemble_site_run_state_from_state_sets(
    const GameState& state,
    const StateManager& state_manager,
    const SiteWorld* site_world)
{
    SiteRunState site_run {};

    const auto& meta = state_manager.query<StateSetId::SiteRunMeta>(state);
    const auto& clock = state_manager.query<StateSetId::SiteClock>(state);
    const auto& camp = state_manager.query<StateSetId::SiteCamp>(state);
    const auto& weather = state_manager.query<StateSetId::SiteWeather>(state);
    const auto& event = state_manager.query<StateSetId::SiteEvent>(state);
    const auto& counters = state_manager.query<StateSetId::SiteCounters>(state);
    const auto& move_direction = state_manager.query<StateSetId::MoveDirection>(state);

    if (!meta.has_value())
    {
        return site_run;
    }

    site_run.site_run_id = meta->site_run_id;
    site_run.site_id = meta->site_id;
    site_run.site_archetype_id = meta->site_archetype_id;
    site_run.attempt_index = meta->attempt_index;
    site_run.site_attempt_seed = meta->site_attempt_seed;
    site_run.run_status = meta->run_status;
    site_run.result_newly_revealed_site_count = meta->result_newly_revealed_site_count;
    site_run.site_world = const_cast<SiteWorld*>(site_world);
    if (clock.has_value())
    {
        site_run.clock = *clock;
    }
    if (camp.has_value())
    {
        site_run.camp = *camp;
    }
    site_run.inventory = assemble_inventory_state_from_state_sets(state, state_manager);
    site_run.contractor = assemble_contractor_state_from_state_sets(state, state_manager);
    if (weather.has_value())
    {
        site_run.weather = *weather;
    }
    if (event.has_value())
    {
        site_run.event = *event;
    }
    site_run.task_board = assemble_task_board_state_from_state_sets(state, state_manager);
    site_run.modifier = assemble_modifier_state_from_state_sets(state, state_manager);
    site_run.economy = assemble_economy_state_from_state_sets(state, state_manager);
    site_run.craft = assemble_craft_state_from_state_sets(state, state_manager);
    site_run.site_action = assemble_action_state_from_state_sets(state, state_manager);
    if (counters.has_value())
    {
        site_run.counters = *counters;
    }
    site_run.objective = assemble_site_objective_state_from_state_sets(state, state_manager);
    site_run.local_weather_resolve =
        assemble_local_weather_resolve_state_from_state_sets(state, state_manager);
    site_run.plant_weather_contribution =
        assemble_plant_weather_contribution_state_from_state_sets(state, state_manager);
    site_run.device_weather_contribution =
        assemble_device_weather_contribution_state_from_state_sets(state, state_manager);

    site_run.host_move_direction = SiteHostMoveDirectionState {
        move_direction.world_move_x,
        move_direction.world_move_y,
        move_direction.world_move_z,
        move_direction.present};
    return site_run;
}

inline void write_site_run_state_to_state_sets(
    const std::optional<SiteRunState>& site_run,
    GameState& state,
    const StateManager& state_manager)
{
    if (!site_run.has_value())
    {
        state_manager.state<StateSetId::SiteRunMeta>(state).reset();
        state_manager.state<StateSetId::SiteClock>(state).reset();
        state_manager.state<StateSetId::SiteCamp>(state).reset();
        state_manager.state<StateSetId::SiteInventoryMeta>(state).reset();
        state_manager.state<StateSetId::SiteInventoryStorageContainers>(state).reset();
        state_manager.state<StateSetId::SiteInventoryStorageSlotItemIds>(state).reset();
        state_manager.state<StateSetId::SiteInventoryWorkerPackSlots>(state).reset();
        state_manager.state<StateSetId::SiteInventoryPendingDeliveries>(state).reset();
        state_manager.state<StateSetId::SiteInventoryPendingDeliveryItemStacks>(state).reset();
        state_manager.state<StateSetId::SiteContractorMeta>(state).reset();
        state_manager.state<StateSetId::SiteContractorWorkOrders>(state).reset();
        state_manager.state<StateSetId::SiteWeather>(state).reset();
        state_manager.state<StateSetId::SiteEvent>(state).reset();
        state_manager.state<StateSetId::SiteTaskBoardMeta>(state).reset();
        state_manager.state<StateSetId::SiteTaskBoardVisibleTasks>(state).reset();
        state_manager.state<StateSetId::SiteTaskBoardRewardDraftOptions>(state).reset();
        state_manager.state<StateSetId::SiteTaskBoardTrackedTiles>(state).reset();
        state_manager.state<StateSetId::SiteTaskBoardAcceptedTaskIds>(state).reset();
        state_manager.state<StateSetId::SiteTaskBoardCompletedTaskIds>(state).reset();
        state_manager.state<StateSetId::SiteTaskBoardClaimedTaskIds>(state).reset();
        state_manager.state<StateSetId::SiteModifierMeta>(state).reset();
        state_manager.state<StateSetId::SiteModifierNearbyAuraIds>(state).reset();
        state_manager.state<StateSetId::SiteModifierActiveSiteModifiers>(state).reset();
        state_manager.state<StateSetId::SiteEconomyMeta>(state).reset();
        state_manager.state<StateSetId::SiteEconomyRevealedUnlockableIds>(state).reset();
        state_manager.state<StateSetId::SiteEconomyDirectPurchaseUnlockableIds>(state).reset();
        state_manager.state<StateSetId::SiteEconomyPhoneListings>(state).reset();
        state_manager.state<StateSetId::SiteCraftDeviceCacheRuntime>(state).reset();
        state_manager.state<StateSetId::SiteCraftDeviceCaches>(state).reset();
        state_manager.state<StateSetId::SiteCraftNearbyItems>(state).reset();
        state_manager.state<StateSetId::SiteCraftPhoneCacheMeta>(state).reset();
        state_manager.state<StateSetId::SiteCraftPhoneItems>(state).reset();
        state_manager.state<StateSetId::SiteCraftContextMeta>(state).reset();
        state_manager.state<StateSetId::SiteCraftContextOptions>(state).reset();
        state_manager.state<StateSetId::SiteActionMeta>(state).reset();
        state_manager.state<StateSetId::SiteActionReservedInputItemStacks>(state).reset();
        state_manager.state<StateSetId::SiteActionResolvedHarvestOutputs>(state).reset();
        state_manager.state<StateSetId::SiteCounters>(state).reset();
        state_manager.state<StateSetId::SiteObjectiveMeta>(state).reset();
        state_manager.state<StateSetId::SiteObjectiveTargetTileIndices>(state).reset();
        state_manager.state<StateSetId::SiteObjectiveTargetTileMask>(state).reset();
        state_manager.state<StateSetId::SiteObjectiveConnectionStartTileIndices>(state).reset();
        state_manager.state<StateSetId::SiteObjectiveConnectionStartTileMask>(state).reset();
        state_manager.state<StateSetId::SiteObjectiveConnectionGoalTileIndices>(state).reset();
        state_manager.state<StateSetId::SiteObjectiveConnectionGoalTileMask>(state).reset();
        state_manager.state<StateSetId::SiteLocalWeatherResolveMeta>(state).reset();
        state_manager.state<StateSetId::SiteLocalWeatherResolveLastTotalContributions>(state).reset();
        state_manager.state<StateSetId::SitePlantWeatherContributionMeta>(state).reset();
        state_manager.state<StateSetId::SitePlantWeatherContributionDirtyTileIndices>(state).reset();
        state_manager.state<StateSetId::SitePlantWeatherContributionDirtyTileMask>(state).reset();
        state_manager.state<StateSetId::SiteDeviceWeatherContributionMeta>(state).reset();
        state_manager.state<StateSetId::SiteDeviceWeatherContributionDirtyTileIndices>(state).reset();
        state_manager.state<StateSetId::SiteDeviceWeatherContributionDirtyTileMask>(state).reset();
        state_manager.state<StateSetId::MoveDirection>(state) = RuntimeMoveDirectionSnapshot {};
        return;
    }

    state_manager.state<StateSetId::SiteRunMeta>(state) = SiteRunMetaState {
        site_run->site_run_id,
        site_run->site_id,
        site_run->site_archetype_id,
        site_run->attempt_index,
        site_run->site_attempt_seed,
        site_run->run_status,
        site_run->result_newly_revealed_site_count};
    state_manager.state<StateSetId::SiteClock>(state) = site_run->clock;
    state_manager.state<StateSetId::SiteCamp>(state) = site_run->camp;
    write_inventory_state_to_state_sets(site_run->inventory, state, state_manager);
    write_contractor_state_to_state_sets(site_run->contractor, state, state_manager);
    state_manager.state<StateSetId::SiteWeather>(state) = site_run->weather;
    state_manager.state<StateSetId::SiteEvent>(state) = site_run->event;
    write_task_board_state_to_state_sets(site_run->task_board, state, state_manager);
    write_modifier_state_to_state_sets(site_run->modifier, state, state_manager);
    write_economy_state_to_state_sets(site_run->economy, state, state_manager);
    write_craft_state_to_state_sets(site_run->craft, state, state_manager);
    write_action_state_to_state_sets(site_run->site_action, state, state_manager);
    state_manager.state<StateSetId::SiteCounters>(state) = site_run->counters;
    write_site_objective_state_to_state_sets(site_run->objective, state, state_manager);
    write_local_weather_resolve_state_to_state_sets(
        site_run->local_weather_resolve,
        state,
        state_manager);
    write_plant_weather_contribution_state_to_state_sets(
        site_run->plant_weather_contribution,
        state,
        state_manager);
    write_device_weather_contribution_state_to_state_sets(
        site_run->device_weather_contribution,
        state,
        state_manager);
    state_manager.state<StateSetId::MoveDirection>(state) = RuntimeMoveDirectionSnapshot {
        site_run->host_move_direction.world_move_x,
        site_run->host_move_direction.world_move_y,
        site_run->host_move_direction.world_move_z,
        site_run->host_move_direction.present};
}
}  // namespace gs1
