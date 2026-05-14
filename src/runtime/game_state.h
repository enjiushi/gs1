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
    RuntimeCampaignRegionalMapStateSet campaign_regional_map {};
    RuntimeCampaignFactionProgressStateSet campaign_faction_progress {};
    RuntimeCampaignTechnologyStateSet campaign_technology {};
    RuntimeCampaignLoadoutPlannerStateSet campaign_loadout_planner {};
    RuntimeCampaignSitesStateSet campaign_sites {};

    RuntimeSiteRunMetaStateSet site_run_meta {};
    RuntimeSiteWorldStateSet site_world {};
    RuntimeSiteClockStateSet site_clock {};
    RuntimeSiteCampStateSet site_camp {};
    RuntimeSiteInventoryStateSet site_inventory {};
    RuntimeSiteContractorStateSet site_contractor {};
    RuntimeSiteWeatherStateSet site_weather {};
    RuntimeSiteEventStateSet site_event {};
    RuntimeSiteTaskBoardStateSet site_task_board {};
    RuntimeSiteModifierStateSet site_modifier {};
    RuntimeSiteEconomyStateSet site_economy {};
    RuntimeSiteCraftStateSet site_craft {};
    RuntimeSiteActionStateSet site_action {};
    RuntimeSiteCountersStateSet site_counters {};
    RuntimeSiteObjectiveStateSet site_objective {};
    RuntimeSiteLocalWeatherResolveStateSet site_local_weather_resolve {};
    RuntimeSitePlantWeatherContributionStateSet site_plant_weather_contribution {};
    RuntimeSiteDeviceWeatherContributionStateSet site_device_weather_contribution {};

    GameMessageQueue message_queue {};
    std::deque<Gs1RuntimeMessage> runtime_messages {};
};
}  // namespace gs1
