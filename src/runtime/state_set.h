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

enum class StateSetId : std::uint8_t
{
    AppState = 0,
    FixedStepSeconds,
    MoveDirection,
    CampaignCore,
    CampaignRegionalMapMeta,
    CampaignRegionalMapRevealedSites,
    CampaignRegionalMapAvailableSites,
    CampaignRegionalMapCompletedSites,
    CampaignFactionProgress,
    CampaignTechnology,
    CampaignLoadoutPlannerMeta,
    CampaignLoadoutPlannerBaselineItems,
    CampaignLoadoutPlannerAvailableSupportItems,
    CampaignLoadoutPlannerSelectedSlots,
    CampaignLoadoutPlannerNearbyAuraModifiers,
    CampaignSiteMetaEntries,
    CampaignSiteAdjacentIds,
    CampaignSiteExportedSupportItems,
    CampaignSiteNearbyAuraModifierIds,
    SiteRunMeta,
    SiteClock,
    SiteCamp,
    SiteInventoryMeta,
    SiteInventoryStorageContainers,
    SiteInventoryStorageSlotItemIds,
    SiteInventoryWorkerPackSlots,
    SiteInventoryPendingDeliveries,
    SiteInventoryPendingDeliveryItemStacks,
    SiteContractorMeta,
    SiteContractorWorkOrders,
    SiteWeather,
    SiteEvent,
    SiteTaskBoardMeta,
    SiteTaskBoardVisibleTasks,
    SiteTaskBoardRewardDraftOptions,
    SiteTaskBoardTrackedTiles,
    SiteTaskBoardAcceptedTaskIds,
    SiteTaskBoardCompletedTaskIds,
    SiteTaskBoardClaimedTaskIds,
    SiteModifierMeta,
    SiteModifierNearbyAuraIds,
    SiteModifierActiveSiteModifiers,
    SiteEconomyMeta,
    SiteEconomyRevealedUnlockableIds,
    SiteEconomyDirectPurchaseUnlockableIds,
    SiteEconomyPhoneListings,
    SiteCraftDeviceCacheRuntime,
    SiteCraftDeviceCaches,
    SiteCraftNearbyItems,
    SiteCraftPhoneCacheMeta,
    SiteCraftPhoneItems,
    SiteCraftContextMeta,
    SiteCraftContextOptions,
    SiteActionMeta,
    SiteActionReservedInputItemStacks,
    SiteActionResolvedHarvestOutputs,
    SiteCounters,
    SiteObjectiveMeta,
    SiteObjectiveTargetTileIndices,
    SiteObjectiveTargetTileMask,
    SiteObjectiveConnectionStartTileIndices,
    SiteObjectiveConnectionStartTileMask,
    SiteObjectiveConnectionGoalTileIndices,
    SiteObjectiveConnectionGoalTileMask,
    SiteLocalWeatherResolveMeta,
    SiteLocalWeatherResolveLastTotalContributions,
    SitePlantWeatherContributionMeta,
    SitePlantWeatherContributionDirtyTileIndices,
    SitePlantWeatherContributionDirtyTileMask,
    SiteDeviceWeatherContributionMeta,
    SiteDeviceWeatherContributionDirtyTileIndices,
    SiteDeviceWeatherContributionDirtyTileMask,
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

template <class T>
struct ContainerStateSet final
{
    std::optional<T> value {};

    ContainerStateSet() = default;
    explicit ContainerStateSet(const std::optional<T>& initial_value)
        : value(initial_value)
    {
    }
    explicit ContainerStateSet(std::optional<T>&& initial_value)
        : value(std::move(initial_value))
    {
    }
    ContainerStateSet(const ContainerStateSet&) = default;
    ContainerStateSet(ContainerStateSet&&) noexcept = default;
    ContainerStateSet& operator=(const ContainerStateSet&) = default;
    ContainerStateSet& operator=(ContainerStateSet&&) noexcept = default;

    template <class U>
        requires std::assignable_from<std::optional<T>&, U&&>
    ContainerStateSet& operator=(U&& next_value)
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

template <class T>
struct ContainerStateSet<std::optional<T>> final
{
    std::optional<T> value {};

    ContainerStateSet() = default;
    explicit ContainerStateSet(const std::optional<T>& initial_value)
        : value(initial_value)
    {
    }
    explicit ContainerStateSet(std::optional<T>&& initial_value)
        : value(std::move(initial_value))
    {
    }
    ContainerStateSet(const ContainerStateSet&) = default;
    ContainerStateSet(ContainerStateSet&&) noexcept = default;
    ContainerStateSet& operator=(const ContainerStateSet&) = default;
    ContainerStateSet& operator=(ContainerStateSet&&) noexcept = default;

    template <class U>
        requires std::assignable_from<std::optional<T>&, U&&>
    ContainerStateSet& operator=(U&& next_value)
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
using RuntimeCampaignRegionalMapMetaStateSet =
    CacheAlignedStateSet<std::optional<RegionalMapMetaState>>;
using RuntimeCampaignRegionalMapRevealedSitesStateSet =
    ContainerStateSet<std::optional<std::vector<SiteId>>>;
using RuntimeCampaignRegionalMapAvailableSitesStateSet =
    ContainerStateSet<std::optional<std::vector<SiteId>>>;
using RuntimeCampaignRegionalMapCompletedSitesStateSet =
    ContainerStateSet<std::optional<std::vector<SiteId>>>;
using RuntimeCampaignFactionProgressStateSet =
    ContainerStateSet<std::optional<std::vector<FactionProgressState>>>;
using RuntimeCampaignTechnologyStateSet = CacheAlignedStateSet<std::optional<TechnologyState>>;
using RuntimeCampaignLoadoutPlannerMetaStateSet =
    CacheAlignedStateSet<std::optional<LoadoutPlannerMetaState>>;
using RuntimeCampaignLoadoutPlannerBaselineItemsStateSet =
    ContainerStateSet<std::optional<std::vector<LoadoutSlot>>>;
using RuntimeCampaignLoadoutPlannerAvailableSupportItemsStateSet =
    ContainerStateSet<std::optional<std::vector<LoadoutSlot>>>;
using RuntimeCampaignLoadoutPlannerSelectedSlotsStateSet =
    ContainerStateSet<std::optional<std::vector<LoadoutSlot>>>;
using RuntimeCampaignLoadoutPlannerNearbyAuraModifiersStateSet =
    ContainerStateSet<std::optional<std::vector<ModifierId>>>;
using RuntimeCampaignSiteMetaEntriesStateSet =
    ContainerStateSet<std::optional<std::vector<SiteMetaEntryState>>>;
using RuntimeCampaignSiteAdjacentIdsStateSet =
    ContainerStateSet<std::optional<std::vector<SiteId>>>;
using RuntimeCampaignSiteExportedSupportItemsStateSet =
    ContainerStateSet<std::optional<std::vector<LoadoutSlot>>>;
using RuntimeCampaignSiteNearbyAuraModifierIdsStateSet =
    ContainerStateSet<std::optional<std::vector<ModifierId>>>;

using RuntimeSiteRunMetaStateSet = CacheAlignedStateSet<std::optional<SiteRunMetaState>>;
using RuntimeSiteClockStateSet = CacheAlignedStateSet<std::optional<SiteClockState>>;
using RuntimeSiteCampStateSet = CacheAlignedStateSet<std::optional<CampState>>;
using RuntimeSiteInventoryMetaStateSet = CacheAlignedStateSet<std::optional<InventoryMetaState>>;
using RuntimeSiteInventoryStorageContainersStateSet =
    ContainerStateSet<std::optional<std::vector<StorageContainerEntryState>>>;
using RuntimeSiteInventoryStorageSlotItemIdsStateSet =
    ContainerStateSet<std::optional<std::vector<std::uint64_t>>>;
using RuntimeSiteInventoryWorkerPackSlotsStateSet =
    ContainerStateSet<std::optional<std::vector<InventorySlot>>>;
using RuntimeSiteInventoryPendingDeliveriesStateSet =
    ContainerStateSet<std::optional<std::vector<PendingDeliveryEntryState>>>;
using RuntimeSiteInventoryPendingDeliveryItemStacksStateSet =
    ContainerStateSet<std::optional<std::vector<InventorySlot>>>;
using RuntimeSiteContractorMetaStateSet = CacheAlignedStateSet<std::optional<ContractorMetaState>>;
using RuntimeSiteContractorWorkOrdersStateSet =
    ContainerStateSet<std::optional<std::vector<WorkOrderEntryState>>>;
using RuntimeSiteWeatherStateSet = CacheAlignedStateSet<std::optional<WeatherState>>;
using RuntimeSiteEventStateSet = CacheAlignedStateSet<std::optional<EventState>>;
using RuntimeSiteTaskBoardMetaStateSet = CacheAlignedStateSet<std::optional<TaskBoardMetaState>>;
using RuntimeSiteTaskBoardVisibleTasksStateSet =
    ContainerStateSet<std::optional<std::vector<TaskInstanceEntryState>>>;
using RuntimeSiteTaskBoardRewardDraftOptionsStateSet =
    ContainerStateSet<std::optional<std::vector<TaskRewardDraftOption>>>;
using RuntimeSiteTaskBoardTrackedTilesStateSet =
    ContainerStateSet<std::optional<std::vector<TaskTrackedTileState>>>;
using RuntimeSiteTaskBoardAcceptedTaskIdsStateSet =
    ContainerStateSet<std::optional<std::vector<TaskInstanceId>>>;
using RuntimeSiteTaskBoardCompletedTaskIdsStateSet =
    ContainerStateSet<std::optional<std::vector<TaskInstanceId>>>;
using RuntimeSiteTaskBoardClaimedTaskIdsStateSet =
    ContainerStateSet<std::optional<std::vector<TaskInstanceId>>>;
using RuntimeSiteModifierMetaStateSet = CacheAlignedStateSet<std::optional<ModifierMetaState>>;
using RuntimeSiteModifierNearbyAuraIdsStateSet =
    ContainerStateSet<std::optional<std::vector<ModifierId>>>;
using RuntimeSiteModifierActiveSiteModifiersStateSet =
    ContainerStateSet<std::optional<std::vector<ActiveSiteModifierState>>>;
using RuntimeSiteEconomyMetaStateSet = CacheAlignedStateSet<std::optional<EconomyMetaState>>;
using RuntimeSiteEconomyRevealedUnlockableIdsStateSet =
    ContainerStateSet<std::optional<std::vector<std::uint32_t>>>;
using RuntimeSiteEconomyDirectPurchaseUnlockableIdsStateSet =
    ContainerStateSet<std::optional<std::vector<std::uint32_t>>>;
using RuntimeSiteEconomyPhoneListingsStateSet =
    ContainerStateSet<std::optional<std::vector<PhoneListingState>>>;
using RuntimeSiteCraftDeviceCacheRuntimeStateSet =
    CacheAlignedStateSet<std::optional<CraftDeviceCacheRuntimeState>>;
using RuntimeSiteCraftDeviceCachesStateSet =
    ContainerStateSet<std::optional<std::vector<CraftDeviceCacheEntryState>>>;
using RuntimeSiteCraftNearbyItemsStateSet =
    ContainerStateSet<std::optional<std::vector<std::uint64_t>>>;
using RuntimeSiteCraftPhoneCacheMetaStateSet =
    CacheAlignedStateSet<std::optional<PhoneInventoryCacheMetaState>>;
using RuntimeSiteCraftPhoneItemsStateSet =
    ContainerStateSet<std::optional<std::vector<std::uint64_t>>>;
using RuntimeSiteCraftContextMetaStateSet =
    CacheAlignedStateSet<std::optional<CraftContextMetaState>>;
using RuntimeSiteCraftContextOptionsStateSet =
    ContainerStateSet<std::optional<std::vector<CraftContextOptionState>>>;
using RuntimeSiteActionMetaStateSet = CacheAlignedStateSet<std::optional<ActionMetaState>>;
using RuntimeSiteActionReservedInputItemStacksStateSet =
    ContainerStateSet<std::optional<std::vector<ReservedItemStack>>>;
using RuntimeSiteActionResolvedHarvestOutputsStateSet =
    ContainerStateSet<std::optional<std::vector<ResolvedHarvestOutputStack>>>;
using RuntimeSiteCountersStateSet = CacheAlignedStateSet<std::optional<SiteCounters>>;
using RuntimeSiteObjectiveMetaStateSet = CacheAlignedStateSet<std::optional<SiteObjectiveMetaState>>;
using RuntimeSiteObjectiveTargetTileIndicesStateSet =
    ContainerStateSet<std::optional<std::vector<std::uint32_t>>>;
using RuntimeSiteObjectiveTargetTileMaskStateSet =
    ContainerStateSet<std::optional<std::vector<std::uint8_t>>>;
using RuntimeSiteObjectiveConnectionStartTileIndicesStateSet =
    ContainerStateSet<std::optional<std::vector<std::uint32_t>>>;
using RuntimeSiteObjectiveConnectionStartTileMaskStateSet =
    ContainerStateSet<std::optional<std::vector<std::uint8_t>>>;
using RuntimeSiteObjectiveConnectionGoalTileIndicesStateSet =
    ContainerStateSet<std::optional<std::vector<std::uint32_t>>>;
using RuntimeSiteObjectiveConnectionGoalTileMaskStateSet =
    ContainerStateSet<std::optional<std::vector<std::uint8_t>>>;
using RuntimeSiteLocalWeatherResolveMetaStateSet =
    CacheAlignedStateSet<std::optional<LocalWeatherResolveMetaState>>;
using RuntimeSiteLocalWeatherResolveLastTotalContributionsStateSet =
    ContainerStateSet<std::optional<std::vector<SiteWorld::TileWeatherContributionData>>>;
using RuntimeSitePlantWeatherContributionMetaStateSet =
    CacheAlignedStateSet<std::optional<PlantWeatherContributionMetaState>>;
using RuntimeSitePlantWeatherContributionDirtyTileIndicesStateSet =
    ContainerStateSet<std::optional<std::vector<std::uint32_t>>>;
using RuntimeSitePlantWeatherContributionDirtyTileMaskStateSet =
    ContainerStateSet<std::optional<std::vector<std::uint8_t>>>;
using RuntimeSiteDeviceWeatherContributionMetaStateSet =
    CacheAlignedStateSet<std::optional<DeviceWeatherContributionMetaState>>;
using RuntimeSiteDeviceWeatherContributionDirtyTileIndicesStateSet =
    ContainerStateSet<std::optional<std::vector<std::uint32_t>>>;
using RuntimeSiteDeviceWeatherContributionDirtyTileMaskStateSet =
    ContainerStateSet<std::optional<std::vector<std::uint8_t>>>;

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
    StateSetId::CampaignRegionalMapMeta,
    std::optional<RegionalMapMetaState>,
    RuntimeCampaignRegionalMapMetaStateSet,
    false);
GS1_DEFINE_STATE_TRAITS(
    StateSetId::CampaignRegionalMapRevealedSites,
    std::optional<std::vector<SiteId>>,
    RuntimeCampaignRegionalMapRevealedSitesStateSet,
    true);
GS1_DEFINE_STATE_TRAITS(
    StateSetId::CampaignRegionalMapAvailableSites,
    std::optional<std::vector<SiteId>>,
    RuntimeCampaignRegionalMapAvailableSitesStateSet,
    true);
GS1_DEFINE_STATE_TRAITS(
    StateSetId::CampaignRegionalMapCompletedSites,
    std::optional<std::vector<SiteId>>,
    RuntimeCampaignRegionalMapCompletedSitesStateSet,
    true);
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
    StateSetId::CampaignLoadoutPlannerMeta,
    std::optional<LoadoutPlannerMetaState>,
    RuntimeCampaignLoadoutPlannerMetaStateSet,
    false);
GS1_DEFINE_STATE_TRAITS(
    StateSetId::CampaignLoadoutPlannerBaselineItems,
    std::optional<std::vector<LoadoutSlot>>,
    RuntimeCampaignLoadoutPlannerBaselineItemsStateSet,
    true);
GS1_DEFINE_STATE_TRAITS(
    StateSetId::CampaignLoadoutPlannerAvailableSupportItems,
    std::optional<std::vector<LoadoutSlot>>,
    RuntimeCampaignLoadoutPlannerAvailableSupportItemsStateSet,
    true);
GS1_DEFINE_STATE_TRAITS(
    StateSetId::CampaignLoadoutPlannerSelectedSlots,
    std::optional<std::vector<LoadoutSlot>>,
    RuntimeCampaignLoadoutPlannerSelectedSlotsStateSet,
    true);
GS1_DEFINE_STATE_TRAITS(
    StateSetId::CampaignLoadoutPlannerNearbyAuraModifiers,
    std::optional<std::vector<ModifierId>>,
    RuntimeCampaignLoadoutPlannerNearbyAuraModifiersStateSet,
    true);
GS1_DEFINE_STATE_TRAITS(
    StateSetId::CampaignSiteMetaEntries,
    std::optional<std::vector<SiteMetaEntryState>>,
    RuntimeCampaignSiteMetaEntriesStateSet,
    true);
GS1_DEFINE_STATE_TRAITS(
    StateSetId::CampaignSiteAdjacentIds,
    std::optional<std::vector<SiteId>>,
    RuntimeCampaignSiteAdjacentIdsStateSet,
    true);
GS1_DEFINE_STATE_TRAITS(
    StateSetId::CampaignSiteExportedSupportItems,
    std::optional<std::vector<LoadoutSlot>>,
    RuntimeCampaignSiteExportedSupportItemsStateSet,
    true);
GS1_DEFINE_STATE_TRAITS(
    StateSetId::CampaignSiteNearbyAuraModifierIds,
    std::optional<std::vector<ModifierId>>,
    RuntimeCampaignSiteNearbyAuraModifierIdsStateSet,
    true);
GS1_DEFINE_STATE_TRAITS(
    StateSetId::SiteRunMeta,
    std::optional<SiteRunMetaState>,
    RuntimeSiteRunMetaStateSet,
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
    StateSetId::SiteInventoryMeta,
    std::optional<InventoryMetaState>,
    RuntimeSiteInventoryMetaStateSet,
    false);
GS1_DEFINE_STATE_TRAITS(
    StateSetId::SiteInventoryStorageContainers,
    std::optional<std::vector<StorageContainerEntryState>>,
    RuntimeSiteInventoryStorageContainersStateSet,
    true);
GS1_DEFINE_STATE_TRAITS(
    StateSetId::SiteInventoryStorageSlotItemIds,
    std::optional<std::vector<std::uint64_t>>,
    RuntimeSiteInventoryStorageSlotItemIdsStateSet,
    true);
GS1_DEFINE_STATE_TRAITS(
    StateSetId::SiteInventoryWorkerPackSlots,
    std::optional<std::vector<InventorySlot>>,
    RuntimeSiteInventoryWorkerPackSlotsStateSet,
    true);
GS1_DEFINE_STATE_TRAITS(
    StateSetId::SiteInventoryPendingDeliveries,
    std::optional<std::vector<PendingDeliveryEntryState>>,
    RuntimeSiteInventoryPendingDeliveriesStateSet,
    true);
GS1_DEFINE_STATE_TRAITS(
    StateSetId::SiteInventoryPendingDeliveryItemStacks,
    std::optional<std::vector<InventorySlot>>,
    RuntimeSiteInventoryPendingDeliveryItemStacksStateSet,
    true);
GS1_DEFINE_STATE_TRAITS(
    StateSetId::SiteContractorMeta,
    std::optional<ContractorMetaState>,
    RuntimeSiteContractorMetaStateSet,
    false);
GS1_DEFINE_STATE_TRAITS(
    StateSetId::SiteContractorWorkOrders,
    std::optional<std::vector<WorkOrderEntryState>>,
    RuntimeSiteContractorWorkOrdersStateSet,
    true);
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
    StateSetId::SiteTaskBoardMeta,
    std::optional<TaskBoardMetaState>,
    RuntimeSiteTaskBoardMetaStateSet,
    false);
GS1_DEFINE_STATE_TRAITS(
    StateSetId::SiteTaskBoardVisibleTasks,
    std::optional<std::vector<TaskInstanceEntryState>>,
    RuntimeSiteTaskBoardVisibleTasksStateSet,
    true);
GS1_DEFINE_STATE_TRAITS(
    StateSetId::SiteTaskBoardRewardDraftOptions,
    std::optional<std::vector<TaskRewardDraftOption>>,
    RuntimeSiteTaskBoardRewardDraftOptionsStateSet,
    true);
GS1_DEFINE_STATE_TRAITS(
    StateSetId::SiteTaskBoardTrackedTiles,
    std::optional<std::vector<TaskTrackedTileState>>,
    RuntimeSiteTaskBoardTrackedTilesStateSet,
    true);
GS1_DEFINE_STATE_TRAITS(
    StateSetId::SiteTaskBoardAcceptedTaskIds,
    std::optional<std::vector<TaskInstanceId>>,
    RuntimeSiteTaskBoardAcceptedTaskIdsStateSet,
    true);
GS1_DEFINE_STATE_TRAITS(
    StateSetId::SiteTaskBoardCompletedTaskIds,
    std::optional<std::vector<TaskInstanceId>>,
    RuntimeSiteTaskBoardCompletedTaskIdsStateSet,
    true);
GS1_DEFINE_STATE_TRAITS(
    StateSetId::SiteTaskBoardClaimedTaskIds,
    std::optional<std::vector<TaskInstanceId>>,
    RuntimeSiteTaskBoardClaimedTaskIdsStateSet,
    true);
GS1_DEFINE_STATE_TRAITS(
    StateSetId::SiteModifierMeta,
    std::optional<ModifierMetaState>,
    RuntimeSiteModifierMetaStateSet,
    false);
GS1_DEFINE_STATE_TRAITS(
    StateSetId::SiteModifierNearbyAuraIds,
    std::optional<std::vector<ModifierId>>,
    RuntimeSiteModifierNearbyAuraIdsStateSet,
    true);
GS1_DEFINE_STATE_TRAITS(
    StateSetId::SiteModifierActiveSiteModifiers,
    std::optional<std::vector<ActiveSiteModifierState>>,
    RuntimeSiteModifierActiveSiteModifiersStateSet,
    true);
GS1_DEFINE_STATE_TRAITS(
    StateSetId::SiteEconomyMeta,
    std::optional<EconomyMetaState>,
    RuntimeSiteEconomyMetaStateSet,
    false);
GS1_DEFINE_STATE_TRAITS(
    StateSetId::SiteEconomyRevealedUnlockableIds,
    std::optional<std::vector<std::uint32_t>>,
    RuntimeSiteEconomyRevealedUnlockableIdsStateSet,
    true);
GS1_DEFINE_STATE_TRAITS(
    StateSetId::SiteEconomyDirectPurchaseUnlockableIds,
    std::optional<std::vector<std::uint32_t>>,
    RuntimeSiteEconomyDirectPurchaseUnlockableIdsStateSet,
    true);
GS1_DEFINE_STATE_TRAITS(
    StateSetId::SiteEconomyPhoneListings,
    std::optional<std::vector<PhoneListingState>>,
    RuntimeSiteEconomyPhoneListingsStateSet,
    true);
GS1_DEFINE_STATE_TRAITS(
    StateSetId::SiteCraftDeviceCacheRuntime,
    std::optional<CraftDeviceCacheRuntimeState>,
    RuntimeSiteCraftDeviceCacheRuntimeStateSet,
    false);
GS1_DEFINE_STATE_TRAITS(
    StateSetId::SiteCraftDeviceCaches,
    std::optional<std::vector<CraftDeviceCacheEntryState>>,
    RuntimeSiteCraftDeviceCachesStateSet,
    true);
GS1_DEFINE_STATE_TRAITS(
    StateSetId::SiteCraftNearbyItems,
    std::optional<std::vector<std::uint64_t>>,
    RuntimeSiteCraftNearbyItemsStateSet,
    true);
GS1_DEFINE_STATE_TRAITS(
    StateSetId::SiteCraftPhoneCacheMeta,
    std::optional<PhoneInventoryCacheMetaState>,
    RuntimeSiteCraftPhoneCacheMetaStateSet,
    false);
GS1_DEFINE_STATE_TRAITS(
    StateSetId::SiteCraftPhoneItems,
    std::optional<std::vector<std::uint64_t>>,
    RuntimeSiteCraftPhoneItemsStateSet,
    true);
GS1_DEFINE_STATE_TRAITS(
    StateSetId::SiteCraftContextMeta,
    std::optional<CraftContextMetaState>,
    RuntimeSiteCraftContextMetaStateSet,
    false);
GS1_DEFINE_STATE_TRAITS(
    StateSetId::SiteCraftContextOptions,
    std::optional<std::vector<CraftContextOptionState>>,
    RuntimeSiteCraftContextOptionsStateSet,
    true);
GS1_DEFINE_STATE_TRAITS(
    StateSetId::SiteActionMeta,
    std::optional<ActionMetaState>,
    RuntimeSiteActionMetaStateSet,
    false);
GS1_DEFINE_STATE_TRAITS(
    StateSetId::SiteActionReservedInputItemStacks,
    std::optional<std::vector<ReservedItemStack>>,
    RuntimeSiteActionReservedInputItemStacksStateSet,
    true);
GS1_DEFINE_STATE_TRAITS(
    StateSetId::SiteActionResolvedHarvestOutputs,
    std::optional<std::vector<ResolvedHarvestOutputStack>>,
    RuntimeSiteActionResolvedHarvestOutputsStateSet,
    true);
GS1_DEFINE_STATE_TRAITS(
    StateSetId::SiteCounters,
    std::optional<SiteCounters>,
    RuntimeSiteCountersStateSet,
    false);
GS1_DEFINE_STATE_TRAITS(
    StateSetId::SiteObjectiveMeta,
    std::optional<SiteObjectiveMetaState>,
    RuntimeSiteObjectiveMetaStateSet,
    false);
GS1_DEFINE_STATE_TRAITS(
    StateSetId::SiteObjectiveTargetTileIndices,
    std::optional<std::vector<std::uint32_t>>,
    RuntimeSiteObjectiveTargetTileIndicesStateSet,
    true);
GS1_DEFINE_STATE_TRAITS(
    StateSetId::SiteObjectiveTargetTileMask,
    std::optional<std::vector<std::uint8_t>>,
    RuntimeSiteObjectiveTargetTileMaskStateSet,
    true);
GS1_DEFINE_STATE_TRAITS(
    StateSetId::SiteObjectiveConnectionStartTileIndices,
    std::optional<std::vector<std::uint32_t>>,
    RuntimeSiteObjectiveConnectionStartTileIndicesStateSet,
    true);
GS1_DEFINE_STATE_TRAITS(
    StateSetId::SiteObjectiveConnectionStartTileMask,
    std::optional<std::vector<std::uint8_t>>,
    RuntimeSiteObjectiveConnectionStartTileMaskStateSet,
    true);
GS1_DEFINE_STATE_TRAITS(
    StateSetId::SiteObjectiveConnectionGoalTileIndices,
    std::optional<std::vector<std::uint32_t>>,
    RuntimeSiteObjectiveConnectionGoalTileIndicesStateSet,
    true);
GS1_DEFINE_STATE_TRAITS(
    StateSetId::SiteObjectiveConnectionGoalTileMask,
    std::optional<std::vector<std::uint8_t>>,
    RuntimeSiteObjectiveConnectionGoalTileMaskStateSet,
    true);
GS1_DEFINE_STATE_TRAITS(
    StateSetId::SiteLocalWeatherResolveMeta,
    std::optional<LocalWeatherResolveMetaState>,
    RuntimeSiteLocalWeatherResolveMetaStateSet,
    false);
GS1_DEFINE_STATE_TRAITS(
    StateSetId::SiteLocalWeatherResolveLastTotalContributions,
    std::optional<std::vector<SiteWorld::TileWeatherContributionData>>,
    RuntimeSiteLocalWeatherResolveLastTotalContributionsStateSet,
    true);
GS1_DEFINE_STATE_TRAITS(
    StateSetId::SitePlantWeatherContributionMeta,
    std::optional<PlantWeatherContributionMetaState>,
    RuntimeSitePlantWeatherContributionMetaStateSet,
    false);
GS1_DEFINE_STATE_TRAITS(
    StateSetId::SitePlantWeatherContributionDirtyTileIndices,
    std::optional<std::vector<std::uint32_t>>,
    RuntimeSitePlantWeatherContributionDirtyTileIndicesStateSet,
    true);
GS1_DEFINE_STATE_TRAITS(
    StateSetId::SitePlantWeatherContributionDirtyTileMask,
    std::optional<std::vector<std::uint8_t>>,
    RuntimeSitePlantWeatherContributionDirtyTileMaskStateSet,
    true);
GS1_DEFINE_STATE_TRAITS(
    StateSetId::SiteDeviceWeatherContributionMeta,
    std::optional<DeviceWeatherContributionMetaState>,
    RuntimeSiteDeviceWeatherContributionMetaStateSet,
    false);
GS1_DEFINE_STATE_TRAITS(
    StateSetId::SiteDeviceWeatherContributionDirtyTileIndices,
    std::optional<std::vector<std::uint32_t>>,
    RuntimeSiteDeviceWeatherContributionDirtyTileIndicesStateSet,
    true);
GS1_DEFINE_STATE_TRAITS(
    StateSetId::SiteDeviceWeatherContributionDirtyTileMask,
    std::optional<std::vector<std::uint8_t>>,
    RuntimeSiteDeviceWeatherContributionDirtyTileMaskStateSet,
    true);

#undef GS1_DEFINE_STATE_TRAITS

template <class T>
struct state_optional_payload
{
    using type = T;
};

template <class T>
struct state_optional_payload<std::optional<T>>
{
    using type = T;
};

template <class T>
consteval bool state_payload_is_pointer_free() noexcept
{
    return !std::is_pointer_v<T> && !std::is_member_pointer_v<T>;
}

template <StateSetId Id>
consteval bool state_contract_is_valid()
{
    using state_type = typename state_traits<Id>::type;
    using wrapper_type = typename state_traits<Id>::wrapper_type;
    if constexpr (!state_traits<Id>::k_is_container)
    {
        using payload_type = typename state_optional_payload<state_type>::type;
        return
            alignof(wrapper_type) >= 64U &&
            std::is_standard_layout_v<payload_type> &&
            std::is_trivially_copyable_v<payload_type> &&
            state_payload_is_pointer_free<payload_type>();
    }
    else
    {
        using container_type = typename state_optional_payload<state_type>::type;
        using element_type = typename container_type::value_type;

        // Container-backed state sets are wrapper handles around dynamic storage,
        // so they intentionally do not participate in the 64-byte alignment rule.
        // Their element payloads must still stay POD-like and pointer-free.
        return std::is_standard_layout_v<element_type> &&
            std::is_trivially_copyable_v<element_type> &&
            state_payload_is_pointer_free<element_type>();
    }
}

static_assert(state_contract_is_valid<StateSetId::AppState>());
static_assert(state_contract_is_valid<StateSetId::FixedStepSeconds>());
static_assert(state_contract_is_valid<StateSetId::MoveDirection>());
static_assert(state_contract_is_valid<StateSetId::CampaignCore>());
static_assert(state_contract_is_valid<StateSetId::CampaignRegionalMapMeta>());
static_assert(state_contract_is_valid<StateSetId::CampaignRegionalMapRevealedSites>());
static_assert(state_contract_is_valid<StateSetId::CampaignRegionalMapAvailableSites>());
static_assert(state_contract_is_valid<StateSetId::CampaignRegionalMapCompletedSites>());
static_assert(state_contract_is_valid<StateSetId::CampaignFactionProgress>());
static_assert(state_contract_is_valid<StateSetId::CampaignTechnology>());
static_assert(state_contract_is_valid<StateSetId::CampaignLoadoutPlannerMeta>());
static_assert(state_contract_is_valid<StateSetId::CampaignLoadoutPlannerBaselineItems>());
static_assert(state_contract_is_valid<StateSetId::CampaignLoadoutPlannerAvailableSupportItems>());
static_assert(state_contract_is_valid<StateSetId::CampaignLoadoutPlannerSelectedSlots>());
static_assert(state_contract_is_valid<StateSetId::CampaignLoadoutPlannerNearbyAuraModifiers>());
static_assert(state_contract_is_valid<StateSetId::CampaignSiteMetaEntries>());
static_assert(state_contract_is_valid<StateSetId::CampaignSiteAdjacentIds>());
static_assert(state_contract_is_valid<StateSetId::CampaignSiteExportedSupportItems>());
static_assert(state_contract_is_valid<StateSetId::CampaignSiteNearbyAuraModifierIds>());
static_assert(state_contract_is_valid<StateSetId::SiteRunMeta>());
static_assert(state_contract_is_valid<StateSetId::SiteClock>());
static_assert(state_contract_is_valid<StateSetId::SiteCamp>());
static_assert(state_contract_is_valid<StateSetId::SiteInventoryMeta>());
static_assert(state_contract_is_valid<StateSetId::SiteInventoryStorageContainers>());
static_assert(state_contract_is_valid<StateSetId::SiteInventoryStorageSlotItemIds>());
static_assert(state_contract_is_valid<StateSetId::SiteInventoryWorkerPackSlots>());
static_assert(state_contract_is_valid<StateSetId::SiteInventoryPendingDeliveries>());
static_assert(state_contract_is_valid<StateSetId::SiteInventoryPendingDeliveryItemStacks>());
static_assert(state_contract_is_valid<StateSetId::SiteContractorMeta>());
static_assert(state_contract_is_valid<StateSetId::SiteContractorWorkOrders>());
static_assert(state_contract_is_valid<StateSetId::SiteWeather>());
static_assert(state_contract_is_valid<StateSetId::SiteEvent>());
static_assert(state_contract_is_valid<StateSetId::SiteTaskBoardMeta>());
static_assert(state_contract_is_valid<StateSetId::SiteTaskBoardVisibleTasks>());
static_assert(state_contract_is_valid<StateSetId::SiteTaskBoardRewardDraftOptions>());
static_assert(state_contract_is_valid<StateSetId::SiteTaskBoardTrackedTiles>());
static_assert(state_contract_is_valid<StateSetId::SiteTaskBoardAcceptedTaskIds>());
static_assert(state_contract_is_valid<StateSetId::SiteTaskBoardCompletedTaskIds>());
static_assert(state_contract_is_valid<StateSetId::SiteTaskBoardClaimedTaskIds>());
static_assert(state_contract_is_valid<StateSetId::SiteModifierMeta>());
static_assert(state_contract_is_valid<StateSetId::SiteModifierNearbyAuraIds>());
static_assert(state_contract_is_valid<StateSetId::SiteModifierActiveSiteModifiers>());
static_assert(state_contract_is_valid<StateSetId::SiteEconomyMeta>());
static_assert(state_contract_is_valid<StateSetId::SiteEconomyRevealedUnlockableIds>());
static_assert(state_contract_is_valid<StateSetId::SiteEconomyDirectPurchaseUnlockableIds>());
static_assert(state_contract_is_valid<StateSetId::SiteEconomyPhoneListings>());
static_assert(state_contract_is_valid<StateSetId::SiteCraftDeviceCacheRuntime>());
static_assert(state_contract_is_valid<StateSetId::SiteCraftDeviceCaches>());
static_assert(state_contract_is_valid<StateSetId::SiteCraftNearbyItems>());
static_assert(state_contract_is_valid<StateSetId::SiteCraftPhoneCacheMeta>());
static_assert(state_contract_is_valid<StateSetId::SiteCraftPhoneItems>());
static_assert(state_contract_is_valid<StateSetId::SiteCraftContextMeta>());
static_assert(state_contract_is_valid<StateSetId::SiteCraftContextOptions>());
static_assert(state_contract_is_valid<StateSetId::SiteActionMeta>());
static_assert(state_contract_is_valid<StateSetId::SiteActionReservedInputItemStacks>());
static_assert(state_contract_is_valid<StateSetId::SiteActionResolvedHarvestOutputs>());
static_assert(state_contract_is_valid<StateSetId::SiteCounters>());
static_assert(state_contract_is_valid<StateSetId::SiteObjectiveMeta>());
static_assert(state_contract_is_valid<StateSetId::SiteObjectiveTargetTileIndices>());
static_assert(state_contract_is_valid<StateSetId::SiteObjectiveTargetTileMask>());
static_assert(state_contract_is_valid<StateSetId::SiteObjectiveConnectionStartTileIndices>());
static_assert(state_contract_is_valid<StateSetId::SiteObjectiveConnectionStartTileMask>());
static_assert(state_contract_is_valid<StateSetId::SiteObjectiveConnectionGoalTileIndices>());
static_assert(state_contract_is_valid<StateSetId::SiteObjectiveConnectionGoalTileMask>());
static_assert(state_contract_is_valid<StateSetId::SiteLocalWeatherResolveMeta>());
static_assert(state_contract_is_valid<StateSetId::SiteLocalWeatherResolveLastTotalContributions>());
static_assert(state_contract_is_valid<StateSetId::SitePlantWeatherContributionMeta>());
static_assert(state_contract_is_valid<StateSetId::SitePlantWeatherContributionDirtyTileIndices>());
static_assert(state_contract_is_valid<StateSetId::SitePlantWeatherContributionDirtyTileMask>());
static_assert(state_contract_is_valid<StateSetId::SiteDeviceWeatherContributionMeta>());
static_assert(state_contract_is_valid<StateSetId::SiteDeviceWeatherContributionDirtyTileIndices>());
static_assert(state_contract_is_valid<StateSetId::SiteDeviceWeatherContributionDirtyTileMask>());
}  // namespace gs1
