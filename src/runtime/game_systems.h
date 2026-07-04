#pragma once

#include "campaign/systems/campaign_flow_system.h"
#include "campaign/systems/campaign_progression_system.h"
#include "campaign/systems/campaign_time_system.h"
#include "campaign/systems/loadout_planner_system.h"
#include "campaign/systems/technology_system.h"
#include "site/systems/action_execution_system.h"
#include "site/systems/camp_durability_system.h"
#include "site/systems/craft_system.h"
#include "site/systems/device_maintenance_system.h"
#include "site/systems/device_support_system.h"
#include "site/systems/device_weather_contribution_system.h"
#include "site/systems/ecology_system.h"
#include "site/systems/economy_phone_system.h"
#include "site/systems/failure_recovery_system.h"
#include "site/systems/inventory_system.h"
#include "site/systems/local_weather_resolve_system.h"
#include "site/systems/modifier_system.h"
#include "site/systems/plant_weather_contribution_system.h"
#include "site/systems/placement_validation_system.h"
#include "site/systems/site_completion_system.h"
#include "site/systems/site_flow_system.h"
#include "site/systems/site_time_system.h"
#include "site/systems/task_board_system.h"
#include "site/systems/weather_event_system.h"
#include "site/systems/worker_condition_system.h"
#include "runtime/shared_framework_contracts.h"
#include "runtime/system_pack.h"

namespace gs1
{
using GameSystems = system_pack<
    CampaignFlowSystem,
    LoadoutPlannerSystem,
    CampaignProgressionSystem,
    TechnologySystem,
    ActionExecutionSystem,
    WeatherEventSystem,
    WorkerConditionSystem,
    EcologySystem,
    PlantWeatherContributionSystem,
    DeviceWeatherContributionSystem,
    TaskBoardSystem,
    PlacementValidationSystem,
    LocalWeatherResolveSystem,
    DeviceMaintenanceSystem,
    InventorySystem,
    CraftSystem,
    EconomyPhoneSystem,
    CampDurabilitySystem,
    DeviceSupportSystem,
    ModifierSystem,
    CampaignTimeSystem,
    SiteTimeSystem,
    SiteFlowSystem,
    FailureRecoverySystem,
    SiteCompletionSystem>;

inline constexpr bool k_game_system_contracts_valid = []() consteval
{
    shared_framework::runtime::validate_system_contracts<GameSystems>::validate();
    return true;
}();

static_assert(k_game_system_contracts_valid);
}  // namespace gs1
