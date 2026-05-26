#pragma once

#include "runtime/runtime_split_state_compat.h"

#include <optional>
#include <utility>

namespace gs1
{
inline void populate_campaign_state_from_state_sets(
    CampaignState& campaign,
    const GameState& state,
    const StateManager& state_manager)
{
    campaign = {};
    const auto& core = state_manager.query<StateSetId::CampaignCore>(state);
    const auto& faction_progress =
        state_manager.query<StateSetId::CampaignFactionProgress>(state);
    const auto& technology = state_manager.query<StateSetId::CampaignTechnology>(state);

    if (!core.has_value())
    {
        return;
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
}

inline void apply_campaign_state_from_state_sets(
    CampaignState& campaign,
    const GameState& state,
    const StateManager& state_manager)
{
    populate_campaign_state_from_state_sets(campaign, state, state_manager);
}

[[nodiscard]] inline CampaignState assemble_campaign_state_from_state_sets(
    const GameState& state,
    const StateManager& state_manager)
{
    CampaignState campaign {};
    populate_campaign_state_from_state_sets(campaign, state, state_manager);
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

inline void populate_site_run_state_from_state_sets(
    SiteRunState& site_run,
    const GameState& state,
    const StateManager& state_manager,
    const SiteWorldHandle& site_world)
{
    site_run = {};
    const auto& meta = state_manager.query<StateSetId::SiteRunMeta>(state);
    const auto& clock = state_manager.query<StateSetId::SiteClock>(state);
    const auto& camp = state_manager.query<StateSetId::SiteCamp>(state);
    const auto& weather = state_manager.query<StateSetId::SiteWeather>(state);
    const auto& event = state_manager.query<StateSetId::SiteEvent>(state);
    const auto& counters = state_manager.query<StateSetId::SiteCounters>(state);
    const auto& move_direction = state_manager.query<StateSetId::MoveDirection>(state);

    if (!meta.has_value())
    {
        return;
    }

    site_run.site_run_id = meta->site_run_id;
    site_run.site_id = meta->site_id;
    site_run.site_archetype_id = meta->site_archetype_id;
    site_run.attempt_index = meta->attempt_index;
    site_run.site_attempt_seed = meta->site_attempt_seed;
    site_run.run_status = meta->run_status;
    site_run.result_newly_revealed_site_count = meta->result_newly_revealed_site_count;
    site_run.site_world = site_world;
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
}

inline void apply_site_run_state_from_state_sets(
    SiteRunState& site_run,
    const GameState& state,
    const StateManager& state_manager,
    const SiteWorldHandle& site_world)
{
    populate_site_run_state_from_state_sets(site_run, state, state_manager, site_world);
}

[[nodiscard]] inline SiteRunState assemble_site_run_state_from_state_sets(
    const GameState& state,
    const StateManager& state_manager,
    const SiteWorldHandle& site_world)
{
    SiteRunState site_run {};
    populate_site_run_state_from_state_sets(site_run, state, state_manager, site_world);
    return site_run;
}

inline void write_site_run_state_to_state_sets(
    const SiteRunState& site_run,
    GameState& state,
    const StateManager& state_manager)
{
    state_manager.state<StateSetId::SiteRunMeta>(state) = SiteRunMetaState {
        site_run.site_run_id,
        site_run.site_id,
        site_run.site_archetype_id,
        site_run.attempt_index,
        site_run.site_attempt_seed,
        site_run.run_status,
        site_run.result_newly_revealed_site_count};
    state_manager.state<StateSetId::SiteClock>(state) = site_run.clock;
    state_manager.state<StateSetId::SiteCamp>(state) = site_run.camp;
    write_inventory_state_to_state_sets(site_run.inventory, state, state_manager);
    write_contractor_state_to_state_sets(site_run.contractor, state, state_manager);
    state_manager.state<StateSetId::SiteWeather>(state) = site_run.weather;
    state_manager.state<StateSetId::SiteEvent>(state) = site_run.event;
    write_task_board_state_to_state_sets(site_run.task_board, state, state_manager);
    write_modifier_state_to_state_sets(site_run.modifier, state, state_manager);
    write_economy_state_to_state_sets(site_run.economy, state, state_manager);
    write_craft_state_to_state_sets(site_run.craft, state, state_manager);
    write_action_state_to_state_sets(site_run.site_action, state, state_manager);
    state_manager.state<StateSetId::SiteCounters>(state) = site_run.counters;
    write_site_objective_state_to_state_sets(site_run.objective, state, state_manager);
    write_local_weather_resolve_state_to_state_sets(
        site_run.local_weather_resolve,
        state,
        state_manager);
    write_plant_weather_contribution_state_to_state_sets(
        site_run.plant_weather_contribution,
        state,
        state_manager);
    write_device_weather_contribution_state_to_state_sets(
        site_run.device_weather_contribution,
        state,
        state_manager);
    state_manager.state<StateSetId::MoveDirection>(state) = RuntimeMoveDirectionSnapshot {
        site_run.host_move_direction.world_move_x,
        site_run.host_move_direction.world_move_y,
        site_run.host_move_direction.world_move_z,
        site_run.host_move_direction.present};
}
}  // namespace gs1
