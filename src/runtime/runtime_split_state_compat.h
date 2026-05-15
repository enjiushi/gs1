#pragma once

#include "campaign/campaign_state.h"
#include "runtime/game_state.h"
#include "runtime/state_manager.h"
#include "site/site_run_state.h"

#include <optional>

namespace gs1
{
[[nodiscard]] inline CampaignState assemble_campaign_state_from_state_sets(
    const GameState& state,
    const StateManager& state_manager)
{
    CampaignState campaign {};

    const auto& core = state_manager.query<StateSetId::CampaignCore>(state);
    const auto& regional_map = state_manager.query<StateSetId::CampaignRegionalMap>(state);
    const auto& faction_progress = state_manager.query<StateSetId::CampaignFactionProgress>(state);
    const auto& technology = state_manager.query<StateSetId::CampaignTechnology>(state);
    const auto& loadout = state_manager.query<StateSetId::CampaignLoadoutPlanner>(state);
    const auto& sites = state_manager.query<StateSetId::CampaignSites>(state);

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
    if (regional_map.has_value())
    {
        campaign.regional_map_state = *regional_map;
    }
    if (faction_progress.has_value())
    {
        campaign.faction_progress = *faction_progress;
    }
    if (technology.has_value())
    {
        campaign.technology_state = *technology;
    }
    if (loadout.has_value())
    {
        campaign.loadout_planner_state = *loadout;
    }
    if (sites.has_value())
    {
        campaign.sites = *sites;
    }

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
        state_manager.state<StateSetId::CampaignRegionalMap>(state).reset();
        state_manager.state<StateSetId::CampaignFactionProgress>(state).reset();
        state_manager.state<StateSetId::CampaignTechnology>(state).reset();
        state_manager.state<StateSetId::CampaignLoadoutPlanner>(state).reset();
        state_manager.state<StateSetId::CampaignSites>(state).reset();
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
    state_manager.state<StateSetId::CampaignRegionalMap>(state) = campaign->regional_map_state;
    state_manager.state<StateSetId::CampaignFactionProgress>(state) = campaign->faction_progress;
    state_manager.state<StateSetId::CampaignTechnology>(state) = campaign->technology_state;
    state_manager.state<StateSetId::CampaignLoadoutPlanner>(state) = campaign->loadout_planner_state;
    state_manager.state<StateSetId::CampaignSites>(state) = campaign->sites;
}

[[nodiscard]] inline SiteRunState assemble_site_run_state_from_state_sets(
    const GameState& state,
    const StateManager& state_manager)
{
    SiteRunState site_run {};

    const auto& meta = state_manager.query<StateSetId::SiteRunMeta>(state);
    const auto& world = state_manager.query<StateSetId::SiteWorld>(state);
    const auto& clock = state_manager.query<StateSetId::SiteClock>(state);
    const auto& camp = state_manager.query<StateSetId::SiteCamp>(state);
    const auto& inventory = state_manager.query<StateSetId::SiteInventory>(state);
    const auto& contractor = state_manager.query<StateSetId::SiteContractor>(state);
    const auto& weather = state_manager.query<StateSetId::SiteWeather>(state);
    const auto& event = state_manager.query<StateSetId::SiteEvent>(state);
    const auto& task_board = state_manager.query<StateSetId::SiteTaskBoard>(state);
    const auto& modifier = state_manager.query<StateSetId::SiteModifier>(state);
    const auto& economy = state_manager.query<StateSetId::SiteEconomy>(state);
    const auto& craft = state_manager.query<StateSetId::SiteCraft>(state);
    const auto& action = state_manager.query<StateSetId::SiteAction>(state);
    const auto& counters = state_manager.query<StateSetId::SiteCounters>(state);
    const auto& objective = state_manager.query<StateSetId::SiteObjective>(state);
    const auto& local_weather = state_manager.query<StateSetId::SiteLocalWeatherResolve>(state);
    const auto& plant_weather = state_manager.query<StateSetId::SitePlantWeatherContribution>(state);
    const auto& device_weather = state_manager.query<StateSetId::SiteDeviceWeatherContribution>(state);
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
    if (world.has_value())
    {
        site_run.site_world = world->site_world;
    }
    if (clock.has_value())
    {
        site_run.clock = *clock;
    }
    if (camp.has_value())
    {
        site_run.camp = *camp;
    }
    if (inventory.has_value())
    {
        site_run.inventory = *inventory;
    }
    if (contractor.has_value())
    {
        site_run.contractor = *contractor;
    }
    if (weather.has_value())
    {
        site_run.weather = *weather;
    }
    if (event.has_value())
    {
        site_run.event = *event;
    }
    if (task_board.has_value())
    {
        site_run.task_board = *task_board;
    }
    if (modifier.has_value())
    {
        site_run.modifier = *modifier;
    }
    if (economy.has_value())
    {
        site_run.economy = *economy;
    }
    if (craft.has_value())
    {
        site_run.craft = *craft;
    }
    if (action.has_value())
    {
        site_run.site_action = *action;
    }
    if (counters.has_value())
    {
        site_run.counters = *counters;
    }
    if (objective.has_value())
    {
        site_run.objective = *objective;
    }
    if (local_weather.has_value())
    {
        site_run.local_weather_resolve = *local_weather;
    }
    if (plant_weather.has_value())
    {
        site_run.plant_weather_contribution = *plant_weather;
    }
    if (device_weather.has_value())
    {
        site_run.device_weather_contribution = *device_weather;
    }

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
        state_manager.state<StateSetId::SiteWorld>(state).reset();
        state_manager.state<StateSetId::SiteClock>(state).reset();
        state_manager.state<StateSetId::SiteCamp>(state).reset();
        state_manager.state<StateSetId::SiteInventory>(state).reset();
        state_manager.state<StateSetId::SiteContractor>(state).reset();
        state_manager.state<StateSetId::SiteWeather>(state).reset();
        state_manager.state<StateSetId::SiteEvent>(state).reset();
        state_manager.state<StateSetId::SiteTaskBoard>(state).reset();
        state_manager.state<StateSetId::SiteModifier>(state).reset();
        state_manager.state<StateSetId::SiteEconomy>(state).reset();
        state_manager.state<StateSetId::SiteCraft>(state).reset();
        state_manager.state<StateSetId::SiteAction>(state).reset();
        state_manager.state<StateSetId::SiteCounters>(state).reset();
        state_manager.state<StateSetId::SiteObjective>(state).reset();
        state_manager.state<StateSetId::SiteLocalWeatherResolve>(state).reset();
        state_manager.state<StateSetId::SitePlantWeatherContribution>(state).reset();
        state_manager.state<StateSetId::SiteDeviceWeatherContribution>(state).reset();
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
    state_manager.state<StateSetId::SiteWorld>(state) = SiteWorldState {site_run->site_world};
    state_manager.state<StateSetId::SiteClock>(state) = site_run->clock;
    state_manager.state<StateSetId::SiteCamp>(state) = site_run->camp;
    state_manager.state<StateSetId::SiteInventory>(state) = site_run->inventory;
    state_manager.state<StateSetId::SiteContractor>(state) = site_run->contractor;
    state_manager.state<StateSetId::SiteWeather>(state) = site_run->weather;
    state_manager.state<StateSetId::SiteEvent>(state) = site_run->event;
    state_manager.state<StateSetId::SiteTaskBoard>(state) = site_run->task_board;
    state_manager.state<StateSetId::SiteModifier>(state) = site_run->modifier;
    state_manager.state<StateSetId::SiteEconomy>(state) = site_run->economy;
    state_manager.state<StateSetId::SiteCraft>(state) = site_run->craft;
    state_manager.state<StateSetId::SiteAction>(state) = site_run->site_action;
    state_manager.state<StateSetId::SiteCounters>(state) = site_run->counters;
    state_manager.state<StateSetId::SiteObjective>(state) = site_run->objective;
    state_manager.state<StateSetId::SiteLocalWeatherResolve>(state) = site_run->local_weather_resolve;
    state_manager.state<StateSetId::SitePlantWeatherContribution>(state) = site_run->plant_weather_contribution;
    state_manager.state<StateSetId::SiteDeviceWeatherContribution>(state) =
        site_run->device_weather_contribution;
    state_manager.state<StateSetId::MoveDirection>(state) = RuntimeMoveDirectionSnapshot {
        site_run->host_move_direction.world_move_x,
        site_run->host_move_direction.world_move_y,
        site_run->host_move_direction.world_move_z,
        site_run->host_move_direction.present};
}
}  // namespace gs1
