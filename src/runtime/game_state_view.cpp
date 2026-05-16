#include "runtime/game_state_view.h"

#include "content/defs/technology_defs.h"
#include "campaign/systems/technology_system.h"
#include "runtime/game_runtime.h"
#include "runtime/state_manager.h"
#include "site/site_world.h"

#include <algorithm>

namespace gs1
{
namespace
{
[[nodiscard]] std::uint32_t id_value(ItemId id) noexcept { return id.value; }
[[nodiscard]] std::uint32_t id_value(PlantId id) noexcept { return id.value; }
[[nodiscard]] std::uint32_t id_value(RecipeId id) noexcept { return id.value; }
[[nodiscard]] std::uint32_t id_value(StructureId id) noexcept { return id.value; }
[[nodiscard]] std::uint32_t id_value(SiteId id) noexcept { return id.value; }
[[nodiscard]] std::uint32_t id_value(FactionId id) noexcept { return id.value; }
[[nodiscard]] std::uint32_t id_value(ModifierId id) noexcept { return id.value; }
[[nodiscard]] std::uint32_t id_value(TaskTemplateId id) noexcept { return id.value; }
[[nodiscard]] std::uint32_t id_value(TaskInstanceId id) noexcept { return id.value; }
[[nodiscard]] std::uint32_t id_value(RewardCandidateId id) noexcept { return id.value; }
[[nodiscard]] std::uint32_t id_value(CampaignId id) noexcept { return id.value; }
[[nodiscard]] std::uint32_t id_value(SiteRunId id) noexcept { return id.value; }
[[nodiscard]] std::uint32_t id_value(TechNodeId id) noexcept { return id.value; }
[[nodiscard]] std::uint32_t id_value(EventTemplateId id) noexcept { return id.value; }
[[nodiscard]] std::uint32_t id_value(RuntimeActionId id) noexcept { return id.value; }

template <typename SourceSlots>
void append_loadout_slots(
    const SourceSlots& source,
    std::vector<Gs1LoadoutSlotView>& destination)
{
    for (const auto& slot : source)
    {
        destination.push_back(Gs1LoadoutSlotView {
            id_value(slot.item_id),
            slot.quantity,
            slot.occupied ? 1U : 0U,
            {0U, 0U, 0U}});
    }
}

void rebuild_campaign_progression_entries(
    const TechnologyState& technology,
    std::span<const FactionProgressState> faction_progress,
    std::vector<Gs1ProgressionEntryView>& entries)
{
    entries.clear();
    entries.reserve(all_reputation_unlock_defs().size() + all_technology_node_defs().size());

    std::uint16_t entry_id = 1U;
    for (const auto& unlock_def : all_reputation_unlock_defs())
    {
        std::uint16_t flags = 0U;
        if (technology.total_reputation >= unlock_def.reputation_requirement)
        {
            flags |= GS1_PROGRESSION_ENTRY_FLAG_UNLOCKED;
        }
        else
        {
            flags |= GS1_PROGRESSION_ENTRY_FLAG_LOCKED;
        }

        entries.push_back(Gs1ProgressionEntryView {
            entry_id++,
            static_cast<std::uint16_t>(std::clamp(unlock_def.reputation_requirement, 0, 65535)),
            static_cast<std::uint16_t>(unlock_def.content_id),
            0U,
            0U,
            GS1_PROGRESSION_ENTRY_REPUTATION_UNLOCK,
            flags,
            static_cast<std::uint8_t>(unlock_def.unlock_kind),
            0U});
    }

    for (const auto& node_def : all_technology_node_defs())
    {
        std::uint16_t flags = TechnologySystem::node_purchased(faction_progress, node_def.tech_node_id)
            ? GS1_PROGRESSION_ENTRY_FLAG_UNLOCKED
            : GS1_PROGRESSION_ENTRY_FLAG_LOCKED;
        if (TechnologySystem::node_claimable(faction_progress, node_def))
        {
            flags |= GS1_PROGRESSION_ENTRY_FLAG_ACTIONABLE;
        }

        entries.push_back(Gs1ProgressionEntryView {
            entry_id++,
            static_cast<std::uint16_t>(std::clamp(
                TechnologySystem::current_reputation_requirement(node_def),
                0,
                65535)),
            static_cast<std::uint16_t>(node_def.granted_content_id),
            static_cast<std::uint16_t>(id_value(node_def.tech_node_id)),
            static_cast<std::uint8_t>(id_value(node_def.faction_id)),
            GS1_PROGRESSION_ENTRY_TECHNOLOGY_NODE,
            flags,
            static_cast<std::uint8_t>(node_def.granted_content_kind),
            static_cast<std::uint8_t>(node_def.tier_index)});
    }
}

void rebuild_campaign_view(
    const CampaignCoreState& campaign_core,
    const RegionalMapMetaState& regional_map,
    std::span<const SiteId> revealed_site_ids,
    std::span<const SiteId> available_site_ids,
    std::span<const SiteId> completed_site_ids,
    std::span<const FactionProgressState> faction_progress,
    const TechnologyState& technology,
    const LoadoutPlannerMetaState& loadout_planner,
    std::span<const LoadoutSlot> baseline_deployment_items,
    std::span<const LoadoutSlot> available_exported_support_items,
    std::span<const LoadoutSlot> selected_loadout_slots,
    std::span<const ModifierId> active_nearby_aura_modifier_ids,
    std::span<const SiteMetaEntryState> sites,
    std::span<const SiteId> site_adjacency_ids,
    std::span<const LoadoutSlot> site_exported_support_items,
    std::span<const ModifierId> site_nearby_aura_modifier_ids,
    RuntimeGameStateViewCache& cache)
{
    cache.faction_progress.clear();
    cache.faction_progress.reserve(faction_progress.size());
    for (const auto& progress : faction_progress)
    {
        cache.faction_progress.push_back(Gs1FactionProgressView {
            id_value(progress.faction_id),
            progress.faction_reputation,
            progress.unlocked_assistant_package_id,
            progress.has_unlocked_assistant_package ? 1U : 0U,
            progress.onboarding_completed ? 1U : 0U,
            progress.tutorial_completed ? 1U : 0U,
            0U});
    }

    cache.revealed_site_ids.clear();
    cache.available_site_ids.clear();
    cache.completed_site_ids.clear();
    cache.site_adjacency_ids.clear();
    cache.site_exported_support_items.clear();
    cache.site_nearby_aura_modifier_ids.clear();
    cache.campaign_sites.clear();
    cache.campaign_sites.reserve(sites.size());

    for (const auto site_id : revealed_site_ids)
    {
        cache.revealed_site_ids.push_back(id_value(site_id));
    }
    for (const auto site_id : available_site_ids)
    {
        cache.available_site_ids.push_back(id_value(site_id));
    }
    for (const auto site_id : completed_site_ids)
    {
        cache.completed_site_ids.push_back(id_value(site_id));
    }

    for (const auto& site : sites)
    {
        const std::uint32_t adjacency_offset = static_cast<std::uint32_t>(cache.site_adjacency_ids.size());
        for (std::uint32_t index = 0U; index < site.adjacent_site_count; ++index)
        {
            const auto flat_index =
                static_cast<std::size_t>(site.adjacent_site_offset + index);
            if (flat_index < site_adjacency_ids.size())
            {
                cache.site_adjacency_ids.push_back(id_value(site_adjacency_ids[flat_index]));
            }
        }

        const std::uint32_t support_offset = static_cast<std::uint32_t>(cache.site_exported_support_items.size());
        for (std::uint32_t index = 0U; index < site.exported_support_item_count; ++index)
        {
            const auto flat_index =
                static_cast<std::size_t>(site.exported_support_item_offset + index);
            if (flat_index < site_exported_support_items.size())
            {
                append_loadout_slots(
                    std::span<const LoadoutSlot> {&site_exported_support_items[flat_index], 1U},
                    cache.site_exported_support_items);
            }
        }

        const std::uint32_t aura_offset = static_cast<std::uint32_t>(cache.site_nearby_aura_modifier_ids.size());
        for (std::uint32_t index = 0U; index < site.nearby_aura_modifier_count; ++index)
        {
            const auto flat_index =
                static_cast<std::size_t>(site.nearby_aura_modifier_offset + index);
            if (flat_index < site_nearby_aura_modifier_ids.size())
            {
                cache.site_nearby_aura_modifier_ids.push_back(
                    id_value(site_nearby_aura_modifier_ids[flat_index]));
            }
        }

        cache.campaign_sites.push_back(Gs1CampaignSiteView {
            id_value(site.site_id),
            site.site_state,
            {0U, 0U, 0U},
            site.regional_map_tile.x,
            site.regional_map_tile.y,
            site.site_archetype_id,
            id_value(site.featured_faction_id),
            site.attempt_count,
            site.support_package_id,
            static_cast<std::uint32_t>(site.completion_reputation_reward),
            site.completion_faction_reputation_reward,
            adjacency_offset,
            site.adjacent_site_count,
            support_offset,
            site.exported_support_item_count,
            aura_offset,
            site.nearby_aura_modifier_count,
            site.has_support_package_id ? 1U : 0U,
            {0U, 0U, 0U}});
    }

    cache.baseline_deployment_items.clear();
    cache.available_exported_support_items.clear();
    cache.selected_loadout_slots.clear();
    cache.active_nearby_aura_modifier_ids.clear();
    append_loadout_slots(
        baseline_deployment_items,
        cache.baseline_deployment_items);
    append_loadout_slots(
        available_exported_support_items,
        cache.available_exported_support_items);
    append_loadout_slots(
        selected_loadout_slots,
        cache.selected_loadout_slots);
    for (const auto modifier_id : active_nearby_aura_modifier_ids)
    {
        cache.active_nearby_aura_modifier_ids.push_back(id_value(modifier_id));
    }

    rebuild_campaign_progression_entries(technology, faction_progress, cache.progression_entries);

    cache.campaign = Gs1CampaignStateView {
        id_value(campaign_core.campaign_id),
        campaign_core.campaign_seed,
        campaign_core.campaign_clock_minutes_elapsed,
        campaign_core.campaign_days_total,
        campaign_core.campaign_days_remaining,
        static_cast<std::uint32_t>(std::max(0, technology.total_reputation)),
        regional_map.has_selected_site_id
            ? static_cast<std::int32_t>(id_value(regional_map.selected_site_id))
            : -1,
        campaign_core.active_site_id.has_value()
            ? static_cast<std::int32_t>(id_value(campaign_core.active_site_id.value()))
            : -1,
        cache.faction_progress.empty() ? nullptr : cache.faction_progress.data(),
        static_cast<std::uint32_t>(cache.faction_progress.size()),
        cache.campaign_sites.empty() ? nullptr : cache.campaign_sites.data(),
        static_cast<std::uint32_t>(cache.campaign_sites.size()),
        cache.revealed_site_ids.empty() ? nullptr : cache.revealed_site_ids.data(),
        static_cast<std::uint32_t>(cache.revealed_site_ids.size()),
        cache.available_site_ids.empty() ? nullptr : cache.available_site_ids.data(),
        static_cast<std::uint32_t>(cache.available_site_ids.size()),
        cache.completed_site_ids.empty() ? nullptr : cache.completed_site_ids.data(),
        static_cast<std::uint32_t>(cache.completed_site_ids.size()),
        cache.site_adjacency_ids.empty() ? nullptr : cache.site_adjacency_ids.data(),
        static_cast<std::uint32_t>(cache.site_adjacency_ids.size()),
        cache.site_exported_support_items.empty() ? nullptr : cache.site_exported_support_items.data(),
        static_cast<std::uint32_t>(cache.site_exported_support_items.size()),
        cache.site_nearby_aura_modifier_ids.empty() ? nullptr : cache.site_nearby_aura_modifier_ids.data(),
        static_cast<std::uint32_t>(cache.site_nearby_aura_modifier_ids.size()),
        cache.baseline_deployment_items.empty() ? nullptr : cache.baseline_deployment_items.data(),
        static_cast<std::uint32_t>(cache.baseline_deployment_items.size()),
        cache.available_exported_support_items.empty() ? nullptr : cache.available_exported_support_items.data(),
        static_cast<std::uint32_t>(cache.available_exported_support_items.size()),
        cache.selected_loadout_slots.empty() ? nullptr : cache.selected_loadout_slots.data(),
        static_cast<std::uint32_t>(cache.selected_loadout_slots.size()),
        cache.active_nearby_aura_modifier_ids.empty() ? nullptr : cache.active_nearby_aura_modifier_ids.data(),
        static_cast<std::uint32_t>(cache.active_nearby_aura_modifier_ids.size()),
        loadout_planner.support_quota_per_contributor,
        loadout_planner.support_quota,
        cache.progression_entries.empty() ? nullptr : cache.progression_entries.data(),
        static_cast<std::uint32_t>(cache.progression_entries.size())};
}

void rebuild_site_view(
    const SiteRunMetaState& meta,
    const SiteWorld* site_world,
    const SiteClockState& clock,
    const CampState& camp,
    const InventoryMetaState& inventory,
    std::span<const StorageContainerEntryState> storage_containers,
    std::span<const std::uint64_t> storage_slot_item_instance_ids,
    std::span<const InventorySlot> worker_pack_slots,
    const WeatherState& weather,
    const EventState& event,
    std::span<const TaskInstanceEntryState> visible_tasks,
    std::span<const TaskRewardDraftOption> reward_draft_options,
    std::span<const ActiveSiteModifierState> active_modifiers,
    const EconomyMetaState& economy,
    std::span<const PhoneListingState> phone_listings,
    const CraftContextMetaState& craft_context,
    std::span<const CraftContextOptionState> craft_context_options,
    const ActionMetaState& site_action,
    const SiteCounters& counters,
    const SiteObjectiveMetaState& objective,
    RuntimeGameStateViewCache& cache)
{
    cache.inventory_storages.clear();
    cache.inventory_slots.clear();
    cache.tasks.clear();
    cache.task_reward_draft_options.clear();
    cache.active_modifiers.clear();
    cache.phone_listings.clear();
    cache.craft_context_options.clear();

    for (const auto& storage : storage_containers)
    {
        const std::uint32_t slot_offset = static_cast<std::uint32_t>(cache.inventory_slots.size());
        for (std::uint32_t slot_index = 0U; slot_index < storage.slot_item_count; ++slot_index)
        {
            const bool worker_pack_storage = storage.storage_id == inventory.worker_pack_storage_id;
            const InventorySlot* source_slot =
                worker_pack_storage && slot_index < worker_pack_slots.size()
                ? &worker_pack_slots[slot_index]
                : nullptr;
            Gs1InventorySlotView slot_view {
                storage.storage_id,
                storage.container_kind,
                static_cast<std::uint16_t>(slot_index),
                0U,
                0U,
                0U,
                0U,
                0U,
                1.0f,
                1.0f};
            if (source_slot != nullptr)
            {
                slot_view.occupied = source_slot->occupied ? 1U : 0U;
                slot_view.item_instance_id = source_slot->item_instance_id;
                slot_view.item_id = id_value(source_slot->item_id);
                slot_view.quantity = source_slot->item_quantity;
                slot_view.condition = source_slot->item_condition;
                slot_view.freshness = source_slot->item_freshness;
            }
            else
            {
                const auto flat_slot_index =
                    static_cast<std::size_t>(storage.slot_item_offset + slot_index);
                const auto instance_id =
                    flat_slot_index < storage_slot_item_instance_ids.size()
                    ? storage_slot_item_instance_ids[flat_slot_index]
                    : 0U;
                slot_view.occupied = instance_id != 0U ? 1U : 0U;
                slot_view.item_instance_id = static_cast<std::uint32_t>(instance_id);
            }
            cache.inventory_slots.push_back(slot_view);
        }

        cache.inventory_storages.push_back(Gs1InventoryStorageView {
            storage.storage_id,
            storage.container_entity_id,
            storage.owner_device_entity_id,
            storage.container_kind,
            {0U, 0U, 0U},
            storage.tile_coord.x,
            storage.tile_coord.y,
            storage.flags,
            slot_offset,
            storage.slot_item_count});
    }

    for (const auto& task : visible_tasks)
    {
        const std::uint32_t reward_offset = static_cast<std::uint32_t>(cache.task_reward_draft_options.size());
        for (std::uint32_t index = 0U; index < task.reward_draft_option_count; ++index)
        {
            const auto reward_index =
                static_cast<std::size_t>(task.reward_draft_option_offset + index);
            if (reward_index >= reward_draft_options.size())
            {
                break;
            }

            const auto& reward = reward_draft_options[
                static_cast<std::size_t>(task.reward_draft_option_offset + index)];
            cache.task_reward_draft_options.push_back(Gs1TaskRewardDraftOptionView {
                id_value(reward.reward_candidate_id),
                reward.selected ? 1U : 0U,
                {0U, 0U, 0U}});
        }

        cache.tasks.push_back(Gs1TaskView {
            id_value(task.task_instance_id),
            id_value(task.task_template_id),
            id_value(task.publisher_faction_id),
            task.task_tier_id,
            task.target_amount,
            task.required_count,
            task.current_progress_amount,
            id_value(task.item_id),
            id_value(task.plant_id),
            id_value(task.recipe_id),
            id_value(task.structure_id),
            id_value(task.secondary_structure_id),
            id_value(task.tertiary_structure_id),
            static_cast<Gs1SiteActionKind>(task.action_kind),
            static_cast<std::uint8_t>(task.runtime_list_kind),
            task.requirement_mask,
            0U,
            task.threshold_value,
            task.expected_task_hours_in_game,
            task.risk_multiplier,
            task.direct_cost_cash_points,
            task.time_cost_cash_points,
            task.risk_cost_cash_points,
            task.difficulty_cash_points,
            task.reward_budget_cash_points,
            reward_offset,
            task.reward_draft_option_count,
            task.chain_id,
            task.chain_step_index,
            id_value(task.follow_up_task_template_id),
            task.progress_accumulator,
            task.has_chain ? 1U : 0U,
            task.has_follow_up_task_template ? 1U : 0U,
            {0U, 0U}});
    }

    for (const auto& active_modifier : active_modifiers)
    {
        cache.active_modifiers.push_back(Gs1SiteModifierView {
            id_value(active_modifier.modifier_id),
            id_value(active_modifier.source_item_id),
            active_modifier.duration_world_minutes,
            active_modifier.remaining_world_minutes,
            active_modifier.duration_world_minutes > 0.0 ? 1U : 0U,
            {0U, 0U, 0U, 0U, 0U, 0U, 0U}});
    }

    for (const auto& listing : phone_listings)
    {
        cache.phone_listings.push_back(Gs1PhoneListingView {
            listing.listing_id,
            id_value(listing.item_id),
            listing.price,
            listing.quantity,
            listing.stock_refresh_generation,
            static_cast<std::uint8_t>(listing.kind),
            listing.occupied ? 1U : 0U,
            listing.generated_from_stock ? 1U : 0U,
            0U});
    }

    if (craft_context.occupied)
    {
        cache.craft_context_options.reserve(craft_context_options.size());
        for (const auto& option : craft_context_options)
        {
            cache.craft_context_options.push_back(Gs1CraftContextOptionView {
                option.recipe_id,
                option.output_item_id});
        }
    }

    const SiteWorld::WorkerData worker =
        site_world != nullptr ? site_world->worker() : SiteWorld::WorkerData {};
    const auto event_template_id = event.active_event_template_id.has_value()
        ? id_value(event.active_event_template_id.value())
        : 0U;
    const auto current_action_id = site_action.has_current_action_id
        ? id_value(site_action.current_action_id)
        : 0U;
    const auto& placement_mode = site_action.placement_mode;

    cache.site = Gs1SiteStateView {
        id_value(meta.site_run_id),
        id_value(meta.site_id),
        meta.site_archetype_id,
        meta.attempt_index,
        meta.site_attempt_seed,
        static_cast<std::uint32_t>(meta.run_status),
        clock.world_time_minutes,
        clock.day_index,
        static_cast<std::uint32_t>(clock.day_phase),
        clock.ecology_pulse_accumulator,
        clock.task_refresh_accumulator,
        clock.delivery_accumulator,
        clock.accumulator_real_seconds,
        Gs1CampStateView {
            camp.camp_anchor_tile.x,
            camp.camp_anchor_tile.y,
            camp.starter_storage_tile.x,
            camp.starter_storage_tile.y,
            camp.delivery_box_tile.x,
            camp.delivery_box_tile.y,
            camp.camp_durability,
            camp.camp_protection_resolved ? 1U : 0U,
            camp.delivery_point_operational ? 1U : 0U,
            camp.shared_storage_access_enabled ? 1U : 0U,
            0U},
        Gs1SiteCountersView {
            counters.fully_grown_tile_count,
            counters.site_completion_tile_threshold,
            counters.tracked_living_plant_count,
            counters.objective_progress_normalized,
            counters.highway_average_sand_cover,
            counters.all_tracked_living_plants_stable ? 1U : 0U,
            {0U, 0U, 0U}},
        Gs1WeatherStateView {
            weather.weather_heat,
            weather.weather_wind,
            weather.weather_dust,
            weather.weather_wind_direction_degrees,
            weather.forecast_profile_state.forecast_profile_id,
            weather.site_weather_bias},
        Gs1SiteEventView {
            event_template_id,
            event.active_event_template_id.has_value() ? 1U : 0U,
            {0U, 0U, 0U},
            event.start_time_minutes,
            event.peak_time_minutes,
            event.peak_duration_minutes,
            event.end_time_minutes,
            event.minutes_until_next_wave,
            event.event_heat_pressure,
            event.event_wind_pressure,
            event.event_dust_pressure,
            event.wave_sequence_index},
        Gs1SiteActionView {
            current_action_id,
            site_action.has_current_action_id ? 1U : 0U,
            static_cast<Gs1SiteActionKind>(site_action.action_kind),
            site_action.quantity,
            site_action.has_target_tile ? site_action.target_tile.x : 0,
            site_action.has_target_tile ? site_action.target_tile.y : 0,
            site_action.has_target_tile ? 1U : 0U,
            {0U, 0U, 0U},
            site_action.has_approach_tile ? site_action.approach_tile.x : 0,
            site_action.has_approach_tile ? site_action.approach_tile.y : 0,
            site_action.has_approach_tile ? 1U : 0U,
            {0U, 0U, 0U},
            site_action.primary_subject_id,
            site_action.secondary_subject_id,
            site_action.item_id,
            site_action.placement_reservation_token,
            site_action.request_flags,
            site_action.awaiting_placement_reservation ? 1U : 0U,
            site_action.reactivate_placement_mode_on_completion ? 1U : 0U,
            0U,
            site_action.total_action_minutes,
            site_action.remaining_action_minutes,
            site_action.started_at_world_minute,
            site_action.has_started_at_world_minute ? 1U : 0U,
            {0U, 0U, 0U, 0U, 0U, 0U, 0U}},
        Gs1PlacementModeView {
            static_cast<Gs1SiteActionKind>(placement_mode.action_kind),
            placement_mode.active ? 1U : 0U,
            placement_mode.quantity,
            placement_mode.has_target_tile ? placement_mode.target_tile.x : 0,
            placement_mode.has_target_tile ? placement_mode.target_tile.y : 0,
            placement_mode.has_target_tile ? 1U : 0U,
            placement_mode.request_flags,
            placement_mode.footprint_width,
            placement_mode.footprint_height,
            placement_mode.primary_subject_id,
            placement_mode.secondary_subject_id,
            placement_mode.item_id,
            placement_mode.blocked_mask},
        Gs1CraftContextView {
            craft_context.tile_coord.x,
            craft_context.tile_coord.y,
            craft_context.occupied ? 1U : 0U,
            {0U, 0U, 0U},
            cache.craft_context_options.empty() ? nullptr : cache.craft_context_options.data(),
            static_cast<std::uint32_t>(cache.craft_context_options.size())},
        Gs1SiteObjectiveView {
            static_cast<std::uint8_t>(objective.type),
            static_cast<std::uint8_t>(objective.target_edge),
            objective.target_band_width,
            objective.has_hold_baseline ? 1U : 0U,
            objective.time_limit_minutes,
            objective.completion_hold_minutes,
            objective.completion_hold_progress_minutes,
            objective.paused_main_timer_minutes,
            objective.last_evaluated_world_time_minutes,
            objective.target_cash_points,
            objective.highway_max_average_sand_cover,
            objective.last_target_average_sand_level},
        Gs1WorkerStateView {
            site_world != nullptr ? site_world->worker_entity_id() : 0U,
            worker.position.tile_coord.x,
            worker.position.tile_coord.y,
            worker.position.tile_x,
            worker.position.tile_y,
            worker.position.facing_degrees,
            worker.conditions.health,
            worker.conditions.hydration,
            worker.conditions.nourishment,
            worker.conditions.energy_cap,
            worker.conditions.energy,
            worker.conditions.morale,
            worker.conditions.work_efficiency,
            worker.conditions.is_sheltered ? 1U : 0U,
            {0U, 0U, 0U}},
        economy.current_cash,
        economy.phone_delivery_fee,
        economy.phone_delivery_minutes,
        0U,
        inventory.worker_pack_storage_id,
        inventory.next_storage_id,
        static_cast<std::uint32_t>(cache.inventory_storages.size()),
        cache.inventory_storages.empty() ? nullptr : cache.inventory_storages.data(),
        cache.inventory_slots.empty() ? nullptr : cache.inventory_slots.data(),
        static_cast<std::uint32_t>(cache.inventory_slots.size()),
        cache.tasks.empty() ? nullptr : cache.tasks.data(),
        static_cast<std::uint32_t>(cache.tasks.size()),
        cache.task_reward_draft_options.empty() ? nullptr : cache.task_reward_draft_options.data(),
        static_cast<std::uint32_t>(cache.task_reward_draft_options.size()),
        cache.active_modifiers.empty() ? nullptr : cache.active_modifiers.data(),
        static_cast<std::uint32_t>(cache.active_modifiers.size()),
        cache.phone_listings.empty() ? nullptr : cache.phone_listings.data(),
        static_cast<std::uint32_t>(cache.phone_listings.size()),
        meta.result_newly_revealed_site_count,
        site_world != nullptr ? static_cast<std::uint32_t>(site_world->tile_count()) : 0U,
        site_world != nullptr ? site_world->width() : 0U,
        site_world != nullptr ? site_world->height() : 0U};
}
}  // namespace

void rebuild_game_state_view_cache(
    const GameRuntime& runtime,
    RuntimeGameStateViewCache& cache)
{
    const GameState& state = runtime.state();
    const auto& state_manager = runtime.state_manager();

    cache.revision += 1U;
    cache.root = {};
    cache.root.struct_size = sizeof(Gs1GameStateView);
    cache.root.revision = cache.revision;
    cache.root.app_state = state.app_state;
    cache.root.has_campaign = state.campaign_core.has_value() ? 1U : 0U;
    cache.root.has_active_site = state.site_run_meta.has_value() ? 1U : 0U;
    cache.root.reserved0 = 0U;

    if (state.campaign_core.has_value())
    {
        const auto& regional_map =
            *state_manager.query<StateSetId::CampaignRegionalMapMeta>(state);
        const auto& revealed_site_ids =
            *state_manager.query<StateSetId::CampaignRegionalMapRevealedSites>(state);
        const auto& available_site_ids =
            *state_manager.query<StateSetId::CampaignRegionalMapAvailableSites>(state);
        const auto& completed_site_ids =
            *state_manager.query<StateSetId::CampaignRegionalMapCompletedSites>(state);
        const auto& loadout =
            *state_manager.query<StateSetId::CampaignLoadoutPlannerMeta>(state);
        const auto& baseline_items =
            *state_manager.query<StateSetId::CampaignLoadoutPlannerBaselineItems>(state);
        const auto& available_support_items =
            *state_manager.query<StateSetId::CampaignLoadoutPlannerAvailableSupportItems>(state);
        const auto& selected_slots =
            *state_manager.query<StateSetId::CampaignLoadoutPlannerSelectedSlots>(state);
        const auto& nearby_aura_ids =
            *state_manager.query<StateSetId::CampaignLoadoutPlannerNearbyAuraModifiers>(state);
        const auto& site_entries =
            *state_manager.query<StateSetId::CampaignSiteMetaEntries>(state);
        const auto& site_adjacency_ids =
            *state_manager.query<StateSetId::CampaignSiteAdjacentIds>(state);
        const auto& site_exported_support_items =
            *state_manager.query<StateSetId::CampaignSiteExportedSupportItems>(state);
        const auto& site_nearby_aura_modifier_ids =
            *state_manager.query<StateSetId::CampaignSiteNearbyAuraModifierIds>(state);
        rebuild_campaign_view(
            *state_manager.query<StateSetId::CampaignCore>(state),
            regional_map,
            revealed_site_ids,
            available_site_ids,
            completed_site_ids,
            *state_manager.query<StateSetId::CampaignFactionProgress>(state),
            *state_manager.query<StateSetId::CampaignTechnology>(state),
            loadout,
            baseline_items,
            available_support_items,
            selected_slots,
            nearby_aura_ids,
            site_entries,
            site_adjacency_ids,
            site_exported_support_items,
            site_nearby_aura_modifier_ids,
            cache);
        cache.root.campaign = &cache.campaign;
    }
    else
    {
        cache.campaign = {};
        cache.root.campaign = nullptr;
    }

    if (state.site_run_meta.has_value())
    {
        rebuild_site_view(
            *state_manager.query<StateSetId::SiteRunMeta>(state),
            runtime.site_world(),
            *state_manager.query<StateSetId::SiteClock>(state),
            *state_manager.query<StateSetId::SiteCamp>(state),
            *state_manager.query<StateSetId::SiteInventoryMeta>(state),
            *state_manager.query<StateSetId::SiteInventoryStorageContainers>(state),
            *state_manager.query<StateSetId::SiteInventoryStorageSlotItemIds>(state),
            *state_manager.query<StateSetId::SiteInventoryWorkerPackSlots>(state),
            *state_manager.query<StateSetId::SiteWeather>(state),
            *state_manager.query<StateSetId::SiteEvent>(state),
            *state_manager.query<StateSetId::SiteTaskBoardVisibleTasks>(state),
            *state_manager.query<StateSetId::SiteTaskBoardRewardDraftOptions>(state),
            *state_manager.query<StateSetId::SiteModifierActiveSiteModifiers>(state),
            *state_manager.query<StateSetId::SiteEconomyMeta>(state),
            *state_manager.query<StateSetId::SiteEconomyPhoneListings>(state),
            *state_manager.query<StateSetId::SiteCraftContextMeta>(state),
            *state_manager.query<StateSetId::SiteCraftContextOptions>(state),
            *state_manager.query<StateSetId::SiteActionMeta>(state),
            *state_manager.query<StateSetId::SiteCounters>(state),
            *state_manager.query<StateSetId::SiteObjectiveMeta>(state),
            cache);
        cache.root.active_site = &cache.site;
    }
    else
    {
        cache.site = {};
        cache.root.active_site = nullptr;
    }
}

bool build_site_tile_view(
    const SiteWorld* site_world,
    std::uint32_t tile_index,
    Gs1SiteTileView& out_tile)
{
    if (site_world == nullptr || tile_index >= site_world->tile_count())
    {
        return false;
    }

    const TileCoord coord = site_world->tile_coord(tile_index);
    const auto tile = site_world->tile_at_index(tile_index);
    out_tile = Gs1SiteTileView {
        tile_index,
        coord.x,
        coord.y,
        site_world->tile_entity_id(coord),
        site_world->device_entity_id(coord),
        tile.static_data.terrain_type_id,
        tile.ecology.ground_cover_type_id,
        id_value(tile.ecology.plant_id),
        id_value(tile.device.structure_id),
        tile.ecology.soil_fertility,
        tile.ecology.moisture,
        tile.ecology.soil_salinity,
        tile.ecology.sand_burial,
        tile.ecology.plant_density,
        tile.ecology.growth_pressure,
        tile.static_data.traversable ? 1U : 0U,
        tile.static_data.plantable ? 1U : 0U,
        tile.static_data.reserved_by_structure ? 1U : 0U,
        static_cast<std::uint8_t>(tile.excavation.depth),
        tile.local_weather.heat,
        tile.local_weather.wind,
        tile.local_weather.dust,
        tile.plant_weather_contribution.heat_protection,
        tile.plant_weather_contribution.wind_protection,
        tile.plant_weather_contribution.dust_protection,
        tile.device_weather_contribution.heat_protection,
        tile.device_weather_contribution.wind_protection,
        tile.device_weather_contribution.dust_protection,
        tile.device.device_integrity,
        tile.device.device_efficiency,
        tile.device.device_stored_water,
        tile.device.fixed_integrity ? 1U : 0U,
        {0U, 0U, 0U}};
    return true;
}
}  // namespace gs1
