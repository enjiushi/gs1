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
    else if constexpr (Id == StateSetId::CampaignRegionalMap)
    {
        return state.campaign_regional_map.get();
    }
    else if constexpr (Id == StateSetId::CampaignFactionProgress)
    {
        return state.campaign_faction_progress.get();
    }
    else if constexpr (Id == StateSetId::CampaignTechnology)
    {
        return state.campaign_technology.get();
    }
    else if constexpr (Id == StateSetId::CampaignLoadoutPlanner)
    {
        return state.campaign_loadout_planner.get();
    }
    else if constexpr (Id == StateSetId::CampaignSites)
    {
        return state.campaign_sites.get();
    }
    else if constexpr (Id == StateSetId::SiteRunMeta)
    {
        return state.site_run_meta.get();
    }
    else if constexpr (Id == StateSetId::SiteWorld)
    {
        return state.site_world.get();
    }
    else if constexpr (Id == StateSetId::SiteClock)
    {
        return state.site_clock.get();
    }
    else if constexpr (Id == StateSetId::SiteCamp)
    {
        return state.site_camp.get();
    }
    else if constexpr (Id == StateSetId::SiteInventory)
    {
        return state.site_inventory.get();
    }
    else if constexpr (Id == StateSetId::SiteContractor)
    {
        return state.site_contractor.get();
    }
    else if constexpr (Id == StateSetId::SiteWeather)
    {
        return state.site_weather.get();
    }
    else if constexpr (Id == StateSetId::SiteEvent)
    {
        return state.site_event.get();
    }
    else if constexpr (Id == StateSetId::SiteTaskBoard)
    {
        return state.site_task_board.get();
    }
    else if constexpr (Id == StateSetId::SiteModifier)
    {
        return state.site_modifier.get();
    }
    else if constexpr (Id == StateSetId::SiteEconomy)
    {
        return state.site_economy.get();
    }
    else if constexpr (Id == StateSetId::SiteCraft)
    {
        return state.site_craft.get();
    }
    else if constexpr (Id == StateSetId::SiteAction)
    {
        return state.site_action.get();
    }
    else if constexpr (Id == StateSetId::SiteCounters)
    {
        return state.site_counters.get();
    }
    else if constexpr (Id == StateSetId::SiteObjective)
    {
        return state.site_objective.get();
    }
    else if constexpr (Id == StateSetId::SiteLocalWeatherResolve)
    {
        return state.site_local_weather_resolve.get();
    }
    else if constexpr (Id == StateSetId::SitePlantWeatherContribution)
    {
        return state.site_plant_weather_contribution.get();
    }
    else if constexpr (Id == StateSetId::SiteDeviceWeatherContribution)
    {
        return state.site_device_weather_contribution.get();
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
    else if constexpr (Id == StateSetId::CampaignRegionalMap)
    {
        return state.campaign_regional_map.get();
    }
    else if constexpr (Id == StateSetId::CampaignFactionProgress)
    {
        return state.campaign_faction_progress.get();
    }
    else if constexpr (Id == StateSetId::CampaignTechnology)
    {
        return state.campaign_technology.get();
    }
    else if constexpr (Id == StateSetId::CampaignLoadoutPlanner)
    {
        return state.campaign_loadout_planner.get();
    }
    else if constexpr (Id == StateSetId::CampaignSites)
    {
        return state.campaign_sites.get();
    }
    else if constexpr (Id == StateSetId::SiteRunMeta)
    {
        return state.site_run_meta.get();
    }
    else if constexpr (Id == StateSetId::SiteWorld)
    {
        return state.site_world.get();
    }
    else if constexpr (Id == StateSetId::SiteClock)
    {
        return state.site_clock.get();
    }
    else if constexpr (Id == StateSetId::SiteCamp)
    {
        return state.site_camp.get();
    }
    else if constexpr (Id == StateSetId::SiteInventory)
    {
        return state.site_inventory.get();
    }
    else if constexpr (Id == StateSetId::SiteContractor)
    {
        return state.site_contractor.get();
    }
    else if constexpr (Id == StateSetId::SiteWeather)
    {
        return state.site_weather.get();
    }
    else if constexpr (Id == StateSetId::SiteEvent)
    {
        return state.site_event.get();
    }
    else if constexpr (Id == StateSetId::SiteTaskBoard)
    {
        return state.site_task_board.get();
    }
    else if constexpr (Id == StateSetId::SiteModifier)
    {
        return state.site_modifier.get();
    }
    else if constexpr (Id == StateSetId::SiteEconomy)
    {
        return state.site_economy.get();
    }
    else if constexpr (Id == StateSetId::SiteCraft)
    {
        return state.site_craft.get();
    }
    else if constexpr (Id == StateSetId::SiteAction)
    {
        return state.site_action.get();
    }
    else if constexpr (Id == StateSetId::SiteCounters)
    {
        return state.site_counters.get();
    }
    else if constexpr (Id == StateSetId::SiteObjective)
    {
        return state.site_objective.get();
    }
    else if constexpr (Id == StateSetId::SiteLocalWeatherResolve)
    {
        return state.site_local_weather_resolve.get();
    }
    else if constexpr (Id == StateSetId::SitePlantWeatherContribution)
    {
        return state.site_plant_weather_contribution.get();
    }
    else if constexpr (Id == StateSetId::SiteDeviceWeatherContribution)
    {
        return state.site_device_weather_contribution.get();
    }
}
}  // namespace gs1
