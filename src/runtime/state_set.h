#pragma once

#include "campaign/campaign_state.h"
#include "campaign/faction_progress_state.h"
#include "campaign/loadout_planner_state.h"
#include "campaign/technology_state.h"
#include "site/action_state.h"
#include "site/contractor_state.h"
#include "site/craft_state.h"
#include "site/device_weather_contribution_state.h"
#include "site/economy_state.h"
#include "site/event_state.h"
#include "site/inventory_state.h"
#include "site/local_weather_resolve_state.h"
#include "site/modifier_state.h"
#include "site/plant_weather_contribution_state.h"
#include "site/site_objective_state.h"
#include "site/site_run_state.h"
#include "site/task_board_state.h"
#include "site/weather_state.h"
#include "support/id_types.h"
#include "gs1/types.h"

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

namespace gs1
{
class SiteWorld;

struct RuntimeMoveDirectionSnapshot final
{
    float world_move_x {0.0f};
    float world_move_y {0.0f};
    float world_move_z {0.0f};
    bool present {false};
};

struct CampaignCoreState final
{
    CampaignId campaign_id {};
    std::uint64_t campaign_seed {0};
    double campaign_clock_minutes_elapsed {0.0};
    std::uint32_t campaign_days_total {0};
    std::uint32_t campaign_days_remaining {0};
    Gs1AppState app_state {GS1_APP_STATE_BOOT};
    std::optional<SiteId> active_site_id {};
};

struct SiteRunMetaState final
{
    SiteRunId site_run_id {};
    SiteId site_id {};
    std::uint32_t site_archetype_id {0};
    std::uint32_t attempt_index {0};
    std::uint64_t site_attempt_seed {0};
    SiteRunStatus run_status {SiteRunStatus::Active};
    std::uint32_t result_newly_revealed_site_count {0};
};

struct SiteWorldState final
{
    std::shared_ptr<SiteWorld> site_world {};
};

enum class StateSetId : std::uint8_t
{
    AppState = 0,
    FixedStepSeconds,
    MoveDirection,
    CampaignCore,
    CampaignRegionalMap,
    CampaignFactionProgress,
    CampaignTechnology,
    CampaignLoadoutPlanner,
    CampaignSites,
    SiteRunMeta,
    SiteWorld,
    SiteClock,
    SiteCamp,
    SiteInventory,
    SiteContractor,
    SiteWeather,
    SiteEvent,
    SiteTaskBoard,
    SiteModifier,
    SiteEconomy,
    SiteCraft,
    SiteAction,
    SiteCounters,
    SiteObjective,
    SiteLocalWeatherResolve,
    SitePlantWeatherContribution,
    SiteDeviceWeatherContribution,
    Count
};

template <class T>
struct alignas(64) CacheAlignedStateSet final
{
    T value {};

    CacheAlignedStateSet() = default;
    explicit CacheAlignedStateSet(const T& initial_value)
        : value(initial_value)
    {
    }
    explicit CacheAlignedStateSet(T&& initial_value)
        : value(std::move(initial_value))
    {
    }
    CacheAlignedStateSet(const CacheAlignedStateSet&) = default;
    CacheAlignedStateSet(CacheAlignedStateSet&&) noexcept = default;
    CacheAlignedStateSet& operator=(const CacheAlignedStateSet&) = default;
    CacheAlignedStateSet& operator=(CacheAlignedStateSet&&) noexcept = default;

    template <class U>
        requires std::assignable_from<T&, U&&>
    CacheAlignedStateSet& operator=(U&& next_value)
    {
        value = std::forward<U>(next_value);
        return *this;
    }

    [[nodiscard]] T& get() noexcept { return value; }
    [[nodiscard]] const T& get() const noexcept { return value; }

    [[nodiscard]] operator T&() noexcept { return value; }
    [[nodiscard]] operator const T&() const noexcept { return value; }

    [[nodiscard]] T* operator->() noexcept { return &value; }
    [[nodiscard]] const T* operator->() const noexcept { return &value; }
};

template <class T>
struct alignas(64) CacheAlignedStateSet<std::optional<T>> final
{
    std::optional<T> value {};

    CacheAlignedStateSet() = default;
    explicit CacheAlignedStateSet(const std::optional<T>& initial_value)
        : value(initial_value)
    {
    }
    explicit CacheAlignedStateSet(std::optional<T>&& initial_value)
        : value(std::move(initial_value))
    {
    }
    CacheAlignedStateSet(const CacheAlignedStateSet&) = default;
    CacheAlignedStateSet(CacheAlignedStateSet&&) noexcept = default;
    CacheAlignedStateSet& operator=(const CacheAlignedStateSet&) = default;
    CacheAlignedStateSet& operator=(CacheAlignedStateSet&&) noexcept = default;

    template <class U>
        requires std::assignable_from<std::optional<T>&, U&&>
    CacheAlignedStateSet& operator=(U&& next_value)
    {
        value = std::forward<U>(next_value);
        return *this;
    }

    [[nodiscard]] std::optional<T>& get() noexcept { return value; }
    [[nodiscard]] const std::optional<T>& get() const noexcept { return value; }

    [[nodiscard]] operator std::optional<T>&() noexcept { return value; }
    [[nodiscard]] operator const std::optional<T>&() const noexcept { return value; }

    [[nodiscard]] bool has_value() const noexcept { return value.has_value(); }
    [[nodiscard]] T& value_ref() { return value.value(); }
    [[nodiscard]] const T& value_ref() const { return value.value(); }
    [[nodiscard]] T& value_or_die() { return value.value(); }
    [[nodiscard]] const T& value_or_die() const { return value.value(); }
    void reset() noexcept { value.reset(); }

    [[nodiscard]] T* operator->() noexcept { return value.operator->(); }
    [[nodiscard]] const T* operator->() const noexcept { return value.operator->(); }
    [[nodiscard]] T& operator*() noexcept { return value.operator*(); }
    [[nodiscard]] const T& operator*() const noexcept { return value.operator*(); }
};

using RuntimeAppStateSet = CacheAlignedStateSet<Gs1AppState>;
using RuntimeFixedStepSecondsStateSet = CacheAlignedStateSet<double>;
using RuntimeMoveDirectionStateSet = CacheAlignedStateSet<RuntimeMoveDirectionSnapshot>;

using RuntimeCampaignCoreStateSet = CacheAlignedStateSet<std::optional<CampaignCoreState>>;
using RuntimeCampaignRegionalMapStateSet = CacheAlignedStateSet<std::optional<RegionalMapState>>;
using RuntimeCampaignFactionProgressStateSet =
    CacheAlignedStateSet<std::optional<std::vector<FactionProgressState>>>;
using RuntimeCampaignTechnologyStateSet = CacheAlignedStateSet<std::optional<TechnologyState>>;
using RuntimeCampaignLoadoutPlannerStateSet = CacheAlignedStateSet<std::optional<LoadoutPlannerState>>;
using RuntimeCampaignSitesStateSet = CacheAlignedStateSet<std::optional<std::vector<SiteMetaState>>>;

using RuntimeSiteRunMetaStateSet = CacheAlignedStateSet<std::optional<SiteRunMetaState>>;
using RuntimeSiteWorldStateSet = CacheAlignedStateSet<std::optional<SiteWorldState>>;
using RuntimeSiteClockStateSet = CacheAlignedStateSet<std::optional<SiteClockState>>;
using RuntimeSiteCampStateSet = CacheAlignedStateSet<std::optional<CampState>>;
using RuntimeSiteInventoryStateSet = CacheAlignedStateSet<std::optional<InventoryState>>;
using RuntimeSiteContractorStateSet = CacheAlignedStateSet<std::optional<ContractorState>>;
using RuntimeSiteWeatherStateSet = CacheAlignedStateSet<std::optional<WeatherState>>;
using RuntimeSiteEventStateSet = CacheAlignedStateSet<std::optional<EventState>>;
using RuntimeSiteTaskBoardStateSet = CacheAlignedStateSet<std::optional<TaskBoardState>>;
using RuntimeSiteModifierStateSet = CacheAlignedStateSet<std::optional<ModifierState>>;
using RuntimeSiteEconomyStateSet = CacheAlignedStateSet<std::optional<EconomyState>>;
using RuntimeSiteCraftStateSet = CacheAlignedStateSet<std::optional<CraftState>>;
using RuntimeSiteActionStateSet = CacheAlignedStateSet<std::optional<ActionState>>;
using RuntimeSiteCountersStateSet = CacheAlignedStateSet<std::optional<SiteCounters>>;
using RuntimeSiteObjectiveStateSet = CacheAlignedStateSet<std::optional<SiteObjectiveState>>;
using RuntimeSiteLocalWeatherResolveStateSet =
    CacheAlignedStateSet<std::optional<LocalWeatherResolveState>>;
using RuntimeSitePlantWeatherContributionStateSet =
    CacheAlignedStateSet<std::optional<PlantWeatherContributionState>>;
using RuntimeSiteDeviceWeatherContributionStateSet =
    CacheAlignedStateSet<std::optional<DeviceWeatherContributionState>>;

template <StateSetId>
struct state_traits;

#define GS1_DEFINE_STATE_TRAITS(state_id, value_type, wrapper, is_container_value) \
    template <> \
    struct state_traits<state_id> \
    { \
        using type = value_type; \
        using wrapper_type = wrapper; \
        static constexpr bool k_is_container = is_container_value; \
    }

GS1_DEFINE_STATE_TRAITS(StateSetId::AppState, Gs1AppState, RuntimeAppStateSet, false);
GS1_DEFINE_STATE_TRAITS(
    StateSetId::FixedStepSeconds,
    double,
    RuntimeFixedStepSecondsStateSet,
    false);
GS1_DEFINE_STATE_TRAITS(
    StateSetId::MoveDirection,
    RuntimeMoveDirectionSnapshot,
    RuntimeMoveDirectionStateSet,
    false);
GS1_DEFINE_STATE_TRAITS(
    StateSetId::CampaignCore,
    std::optional<CampaignCoreState>,
    RuntimeCampaignCoreStateSet,
    false);
GS1_DEFINE_STATE_TRAITS(
    StateSetId::CampaignRegionalMap,
    std::optional<RegionalMapState>,
    RuntimeCampaignRegionalMapStateSet,
    false);
GS1_DEFINE_STATE_TRAITS(
    StateSetId::CampaignFactionProgress,
    std::optional<std::vector<FactionProgressState>>,
    RuntimeCampaignFactionProgressStateSet,
    true);
GS1_DEFINE_STATE_TRAITS(
    StateSetId::CampaignTechnology,
    std::optional<TechnologyState>,
    RuntimeCampaignTechnologyStateSet,
    false);
GS1_DEFINE_STATE_TRAITS(
    StateSetId::CampaignLoadoutPlanner,
    std::optional<LoadoutPlannerState>,
    RuntimeCampaignLoadoutPlannerStateSet,
    false);
GS1_DEFINE_STATE_TRAITS(
    StateSetId::CampaignSites,
    std::optional<std::vector<SiteMetaState>>,
    RuntimeCampaignSitesStateSet,
    true);
GS1_DEFINE_STATE_TRAITS(
    StateSetId::SiteRunMeta,
    std::optional<SiteRunMetaState>,
    RuntimeSiteRunMetaStateSet,
    false);
GS1_DEFINE_STATE_TRAITS(
    StateSetId::SiteWorld,
    std::optional<SiteWorldState>,
    RuntimeSiteWorldStateSet,
    false);
GS1_DEFINE_STATE_TRAITS(
    StateSetId::SiteClock,
    std::optional<SiteClockState>,
    RuntimeSiteClockStateSet,
    false);
GS1_DEFINE_STATE_TRAITS(
    StateSetId::SiteCamp,
    std::optional<CampState>,
    RuntimeSiteCampStateSet,
    false);
GS1_DEFINE_STATE_TRAITS(
    StateSetId::SiteInventory,
    std::optional<InventoryState>,
    RuntimeSiteInventoryStateSet,
    false);
GS1_DEFINE_STATE_TRAITS(
    StateSetId::SiteContractor,
    std::optional<ContractorState>,
    RuntimeSiteContractorStateSet,
    false);
GS1_DEFINE_STATE_TRAITS(
    StateSetId::SiteWeather,
    std::optional<WeatherState>,
    RuntimeSiteWeatherStateSet,
    false);
GS1_DEFINE_STATE_TRAITS(
    StateSetId::SiteEvent,
    std::optional<EventState>,
    RuntimeSiteEventStateSet,
    false);
GS1_DEFINE_STATE_TRAITS(
    StateSetId::SiteTaskBoard,
    std::optional<TaskBoardState>,
    RuntimeSiteTaskBoardStateSet,
    false);
GS1_DEFINE_STATE_TRAITS(
    StateSetId::SiteModifier,
    std::optional<ModifierState>,
    RuntimeSiteModifierStateSet,
    false);
GS1_DEFINE_STATE_TRAITS(
    StateSetId::SiteEconomy,
    std::optional<EconomyState>,
    RuntimeSiteEconomyStateSet,
    false);
GS1_DEFINE_STATE_TRAITS(
    StateSetId::SiteCraft,
    std::optional<CraftState>,
    RuntimeSiteCraftStateSet,
    false);
GS1_DEFINE_STATE_TRAITS(
    StateSetId::SiteAction,
    std::optional<ActionState>,
    RuntimeSiteActionStateSet,
    false);
GS1_DEFINE_STATE_TRAITS(
    StateSetId::SiteCounters,
    std::optional<SiteCounters>,
    RuntimeSiteCountersStateSet,
    false);
GS1_DEFINE_STATE_TRAITS(
    StateSetId::SiteObjective,
    std::optional<SiteObjectiveState>,
    RuntimeSiteObjectiveStateSet,
    false);
GS1_DEFINE_STATE_TRAITS(
    StateSetId::SiteLocalWeatherResolve,
    std::optional<LocalWeatherResolveState>,
    RuntimeSiteLocalWeatherResolveStateSet,
    false);
GS1_DEFINE_STATE_TRAITS(
    StateSetId::SitePlantWeatherContribution,
    std::optional<PlantWeatherContributionState>,
    RuntimeSitePlantWeatherContributionStateSet,
    false);
GS1_DEFINE_STATE_TRAITS(
    StateSetId::SiteDeviceWeatherContribution,
    std::optional<DeviceWeatherContributionState>,
    RuntimeSiteDeviceWeatherContributionStateSet,
    false);

#undef GS1_DEFINE_STATE_TRAITS

template <StateSetId Id>
consteval bool state_contract_is_valid()
{
    using wrapper_type = typename state_traits<Id>::wrapper_type;
    if constexpr (!state_traits<Id>::k_is_container)
    {
        return alignof(wrapper_type) >= 64U;
    }

    return true;
}

static_assert(state_contract_is_valid<StateSetId::AppState>());
static_assert(state_contract_is_valid<StateSetId::FixedStepSeconds>());
static_assert(state_contract_is_valid<StateSetId::MoveDirection>());
static_assert(state_contract_is_valid<StateSetId::CampaignCore>());
static_assert(state_contract_is_valid<StateSetId::CampaignRegionalMap>());
static_assert(state_contract_is_valid<StateSetId::CampaignTechnology>());
static_assert(state_contract_is_valid<StateSetId::CampaignLoadoutPlanner>());
static_assert(state_contract_is_valid<StateSetId::SiteRunMeta>());
static_assert(state_contract_is_valid<StateSetId::SiteWorld>());
static_assert(state_contract_is_valid<StateSetId::SiteClock>());
static_assert(state_contract_is_valid<StateSetId::SiteCamp>());
static_assert(state_contract_is_valid<StateSetId::SiteInventory>());
static_assert(state_contract_is_valid<StateSetId::SiteContractor>());
static_assert(state_contract_is_valid<StateSetId::SiteWeather>());
static_assert(state_contract_is_valid<StateSetId::SiteEvent>());
static_assert(state_contract_is_valid<StateSetId::SiteTaskBoard>());
static_assert(state_contract_is_valid<StateSetId::SiteModifier>());
static_assert(state_contract_is_valid<StateSetId::SiteEconomy>());
static_assert(state_contract_is_valid<StateSetId::SiteCraft>());
static_assert(state_contract_is_valid<StateSetId::SiteAction>());
static_assert(state_contract_is_valid<StateSetId::SiteCounters>());
static_assert(state_contract_is_valid<StateSetId::SiteObjective>());
static_assert(state_contract_is_valid<StateSetId::SiteLocalWeatherResolve>());
static_assert(state_contract_is_valid<StateSetId::SitePlantWeatherContribution>());
static_assert(state_contract_is_valid<StateSetId::SiteDeviceWeatherContribution>());
}  // namespace gs1
