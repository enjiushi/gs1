#pragma once

#include "messages/game_message.h"
#include "runtime/runtime_state_access.h"
#include "runtime/state_set.h"
#include "site/systems/site_system_context.h"
#include "gs1/status.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace gs1
{
inline constexpr std::size_t k_runtime_host_message_type_count =
    static_cast<std::size_t>(GS1_HOST_EVENT_SITE_SCENE_READY) + 1U;

using GameMessageSubscriptionSpan = std::span<const GameMessageType>;
using HostMessageSubscriptionSpan = std::span<const Gs1HostMessageType>;

class IRuntimeSystem
{
public:
    virtual ~IRuntimeSystem() = default;

    [[nodiscard]] virtual const char* name() const noexcept = 0;
    [[nodiscard]] virtual GameMessageSubscriptionSpan subscribed_game_messages() const noexcept = 0;
    [[nodiscard]] virtual HostMessageSubscriptionSpan subscribed_host_messages() const noexcept = 0;
    [[nodiscard]] virtual std::optional<Gs1RuntimeProfileSystemId> profile_system_id() const noexcept = 0;
    [[nodiscard]] virtual std::optional<std::uint32_t> fixed_step_order() const noexcept = 0;
    [[nodiscard]] virtual Gs1Status process_game_message(
        RuntimeInvocation& invocation,
        const GameMessage& message) = 0;
    [[nodiscard]] virtual Gs1Status process_host_message(
        RuntimeInvocation& invocation,
        const Gs1HostMessage& message) = 0;
    virtual void run(RuntimeInvocation& invocation) = 0;

    [[nodiscard]] virtual std::span<const StateSetId> owned_state_sets() const noexcept
    {
        return {};
    }
};

using RuntimeGameMessageSubscribers = std::array<std::vector<IRuntimeSystem*>, k_game_message_type_count>;
using RuntimeHostMessageSubscribers =
    std::array<std::vector<IRuntimeSystem*>, k_runtime_host_message_type_count>;

inline constexpr StateSetId k_site_component_run_meta_state_sets[] = {
    StateSetId::SiteRunMeta};
inline constexpr StateSetId k_site_component_time_state_sets[] = {
    StateSetId::SiteClock};
inline constexpr StateSetId k_site_component_camp_state_sets[] = {
    StateSetId::SiteCamp};
inline constexpr StateSetId k_site_component_inventory_state_sets[] = {
    StateSetId::SiteInventoryMeta,
    StateSetId::SiteInventoryStorageContainers,
    StateSetId::SiteInventoryStorageSlotItemIds,
    StateSetId::SiteInventoryWorkerPackSlots,
    StateSetId::SiteInventoryPendingDeliveries,
    StateSetId::SiteInventoryPendingDeliveryItemStacks};
inline constexpr StateSetId k_site_component_contractor_state_sets[] = {
    StateSetId::SiteContractorMeta,
    StateSetId::SiteContractorWorkOrders};
inline constexpr StateSetId k_site_component_weather_state_sets[] = {
    StateSetId::SiteWeather};
inline constexpr StateSetId k_site_component_local_weather_state_sets[] = {
    StateSetId::SiteLocalWeatherResolveMeta,
    StateSetId::SiteLocalWeatherResolveLastTotalContributions};
inline constexpr StateSetId k_site_component_event_state_sets[] = {
    StateSetId::SiteEvent};
inline constexpr StateSetId k_site_component_task_board_state_sets[] = {
    StateSetId::SiteTaskBoardMeta,
    StateSetId::SiteTaskBoardVisibleTasks,
    StateSetId::SiteTaskBoardRewardDraftOptions,
    StateSetId::SiteTaskBoardTrackedTiles,
    StateSetId::SiteTaskBoardAcceptedTaskIds,
    StateSetId::SiteTaskBoardCompletedTaskIds,
    StateSetId::SiteTaskBoardClaimedTaskIds};
inline constexpr StateSetId k_site_component_modifier_state_sets[] = {
    StateSetId::SiteModifierMeta,
    StateSetId::SiteModifierNearbyAuraIds,
    StateSetId::SiteModifierActiveSiteModifiers};
inline constexpr StateSetId k_site_component_economy_state_sets[] = {
    StateSetId::SiteEconomyMeta,
    StateSetId::SiteEconomyRevealedUnlockableIds,
    StateSetId::SiteEconomyDirectPurchaseUnlockableIds,
    StateSetId::SiteEconomyPhoneListings};
inline constexpr StateSetId k_site_component_craft_state_sets[] = {
    StateSetId::SiteCraftDeviceCacheRuntime,
    StateSetId::SiteCraftDeviceCaches,
    StateSetId::SiteCraftNearbyItems,
    StateSetId::SiteCraftPhoneCacheMeta,
    StateSetId::SiteCraftPhoneItems,
    StateSetId::SiteCraftContextMeta,
    StateSetId::SiteCraftContextOptions};
inline constexpr StateSetId k_site_component_action_state_sets[] = {
    StateSetId::SiteActionMeta,
    StateSetId::SiteActionReservedInputItemStacks,
    StateSetId::SiteActionResolvedHarvestOutputs};
inline constexpr StateSetId k_site_component_counters_state_sets[] = {
    StateSetId::SiteCounters};
inline constexpr StateSetId k_site_component_objective_state_sets[] = {
    StateSetId::SiteObjectiveMeta,
    StateSetId::SiteObjectiveTargetTileIndices,
    StateSetId::SiteObjectiveTargetTileMask,
    StateSetId::SiteObjectiveConnectionStartTileIndices,
    StateSetId::SiteObjectiveConnectionStartTileMask,
    StateSetId::SiteObjectiveConnectionGoalTileIndices,
    StateSetId::SiteObjectiveConnectionGoalTileMask};
inline constexpr StateSetId k_site_component_plant_weather_state_sets[] = {
    StateSetId::SitePlantWeatherContributionMeta,
    StateSetId::SitePlantWeatherContributionDirtyTileIndices,
    StateSetId::SitePlantWeatherContributionDirtyTileMask};
inline constexpr StateSetId k_site_component_device_weather_state_sets[] = {
    StateSetId::SiteDeviceWeatherContributionMeta,
    StateSetId::SiteDeviceWeatherContributionDirtyTileIndices,
    StateSetId::SiteDeviceWeatherContributionDirtyTileMask};

template <typename EnumType>
[[nodiscard]] constexpr bool runtime_subscription_contains(
    std::span<const EnumType> subscriptions,
    EnumType type) noexcept
{
    for (const EnumType subscribed_type : subscriptions)
    {
        if (subscribed_type == type)
        {
            return true;
        }
    }

    return false;
}

template <typename System>
[[nodiscard]] constexpr std::span<const StateSetId> runtime_declared_owned_state_sets() noexcept
{
    if constexpr (requires { System::k_owned_state_sets; })
    {
        return std::span<const StateSetId> {System::k_owned_state_sets};
    }
    else
    {
        return {};
    }
}

[[nodiscard]] constexpr std::span<const StateSetId> site_component_owner_state_sets(
    SiteComponent component) noexcept
{
    switch (component)
    {
    case SiteComponent::RunMeta:
        return k_site_component_run_meta_state_sets;
    case SiteComponent::Time:
        return k_site_component_time_state_sets;
    case SiteComponent::Camp:
        return k_site_component_camp_state_sets;
    case SiteComponent::Inventory:
        return k_site_component_inventory_state_sets;
    case SiteComponent::Contractor:
        return k_site_component_contractor_state_sets;
    case SiteComponent::Weather:
        return k_site_component_weather_state_sets;
    case SiteComponent::LocalWeatherRuntime:
        return k_site_component_local_weather_state_sets;
    case SiteComponent::Event:
        return k_site_component_event_state_sets;
    case SiteComponent::TaskBoard:
        return k_site_component_task_board_state_sets;
    case SiteComponent::Modifier:
        return k_site_component_modifier_state_sets;
    case SiteComponent::Economy:
        return k_site_component_economy_state_sets;
    case SiteComponent::Craft:
        return k_site_component_craft_state_sets;
    case SiteComponent::Action:
        return k_site_component_action_state_sets;
    case SiteComponent::Counters:
        return k_site_component_counters_state_sets;
    case SiteComponent::Objective:
        return k_site_component_objective_state_sets;
    case SiteComponent::PlantWeatherRuntime:
        return k_site_component_plant_weather_state_sets;
    case SiteComponent::DeviceWeatherRuntime:
        return k_site_component_device_weather_state_sets;
    default:
        return {};
    }
}

template <typename System>
[[nodiscard]] std::span<const StateSetId> site_access_owned_state_sets() noexcept
{
    if constexpr (requires { System::access(); })
    {
        static constexpr auto resolved = []()
        {
            std::array<StateSetId, static_cast<std::size_t>(SiteComponent::Count)> values {};
            std::size_t count = 0U;

            for (std::size_t index = 0; index < static_cast<std::size_t>(SiteComponent::Count); ++index)
            {
                const auto component = static_cast<SiteComponent>(index);
                if ((System::access().own_components & site_component_bit(component)) == 0U)
                {
                    continue;
                }

                for (const auto mapped : site_component_owner_state_sets(component))
                {
                    bool already_present = false;
                    for (std::size_t existing = 0; existing < count; ++existing)
                    {
                        if (values[existing] == mapped)
                        {
                            already_present = true;
                            break;
                        }
                    }

                    if (!already_present)
                    {
                        values[count++] = mapped;
                    }
                }
            }

            return std::pair {values, count};
        }();

        return std::span<const StateSetId> {resolved.first.data(), resolved.second};
    }
    else
    {
        return {};
    }
}
}  // namespace gs1
