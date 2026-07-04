#pragma once

#include "shared_framework/runtime/foundation/system_contract_validation.h"
#include "runtime/state_set.h"
#include "site/systems/site_system_context.h"

#include <array>
#include <cstddef>
#include <span>
#include <type_traits>
#include <utility>

namespace gs1
{
template <StateSetId Id>
using state_key = std::integral_constant<StateSetId, Id>;

template <StateSetId Id>
using owned_state_descriptor_t = shared_framework::runtime::state_descriptor<
    state_key<Id>,
    typename state_traits<Id>::type>;

template <typename Key>
struct descriptor_from_state_key;

template <StateSetId Id>
struct descriptor_from_state_key<state_key<Id>>
{
    using type = owned_state_descriptor_t<Id>;
};

template <auto& Array, typename IndexSequence>
struct owned_state_descriptors_from_array;

template <auto& Array, std::size_t... Indices>
struct owned_state_descriptors_from_array<Array, std::index_sequence<Indices...>>
{
    using type = type_list<owned_state_descriptor_t<Array[Indices]>...>;
};

template <auto& Array>
using owned_state_descriptors_from_array_t = typename owned_state_descriptors_from_array<
    Array,
    std::make_index_sequence<std::tuple_size_v<std::remove_cvref_t<decltype(Array)>>>>::type;

template <auto& Array, typename IndexSequence>
struct state_keys_from_array;

template <auto& Array, std::size_t... Indices>
struct state_keys_from_array<Array, std::index_sequence<Indices...>>
{
    using type = type_list<state_key<Array[Indices]>...>;
};

template <auto& Array>
using state_keys_from_array_t = typename state_keys_from_array<
    Array,
    std::make_index_sequence<std::tuple_size_v<std::remove_cvref_t<decltype(Array)>>>>::type;

template <SiteComponent Component>
struct site_component_state_keys
{
    using type = type_list<>;
};

template <>
struct site_component_state_keys<SiteComponent::RunMeta>
{
    using type = type_list<state_key<StateSetId::SiteRunMeta>>;
};

template <>
struct site_component_state_keys<SiteComponent::Time>
{
    using type = type_list<state_key<StateSetId::SiteClock>>;
};

template <>
struct site_component_state_keys<SiteComponent::Camp>
{
    using type = type_list<state_key<StateSetId::SiteCamp>>;
};

template <>
struct site_component_state_keys<SiteComponent::Inventory>
{
    using type = type_list<
        state_key<StateSetId::SiteInventoryMeta>,
        state_key<StateSetId::SiteInventoryStorageContainers>,
        state_key<StateSetId::SiteInventoryStorageSlotItemIds>,
        state_key<StateSetId::SiteInventoryWorkerPackSlots>,
        state_key<StateSetId::SiteInventoryPendingDeliveries>,
        state_key<StateSetId::SiteInventoryPendingDeliveryItemStacks>>;
};

template <>
struct site_component_state_keys<SiteComponent::Contractor>
{
    using type = type_list<
        state_key<StateSetId::SiteContractorMeta>,
        state_key<StateSetId::SiteContractorWorkOrders>>;
};

template <>
struct site_component_state_keys<SiteComponent::Weather>
{
    using type = type_list<state_key<StateSetId::SiteWeather>>;
};

template <>
struct site_component_state_keys<SiteComponent::LocalWeatherRuntime>
{
    using type = type_list<
        state_key<StateSetId::SiteLocalWeatherResolveMeta>,
        state_key<StateSetId::SiteLocalWeatherResolveLastTotalContributions>>;
};

template <>
struct site_component_state_keys<SiteComponent::Event>
{
    using type = type_list<state_key<StateSetId::SiteEvent>>;
};

template <>
struct site_component_state_keys<SiteComponent::TaskBoard>
{
    using type = type_list<
        state_key<StateSetId::SiteTaskBoardMeta>,
        state_key<StateSetId::SiteTaskBoardVisibleTasks>,
        state_key<StateSetId::SiteTaskBoardRewardDraftOptions>,
        state_key<StateSetId::SiteTaskBoardTrackedTiles>,
        state_key<StateSetId::SiteTaskBoardAcceptedTaskIds>,
        state_key<StateSetId::SiteTaskBoardCompletedTaskIds>,
        state_key<StateSetId::SiteTaskBoardClaimedTaskIds>>;
};

template <>
struct site_component_state_keys<SiteComponent::Modifier>
{
    using type = type_list<
        state_key<StateSetId::SiteModifierMeta>,
        state_key<StateSetId::SiteModifierNearbyAuraIds>,
        state_key<StateSetId::SiteModifierActiveSiteModifiers>>;
};

template <>
struct site_component_state_keys<SiteComponent::Economy>
{
    using type = type_list<
        state_key<StateSetId::SiteEconomyMeta>,
        state_key<StateSetId::SiteEconomyRevealedUnlockableIds>,
        state_key<StateSetId::SiteEconomyDirectPurchaseUnlockableIds>,
        state_key<StateSetId::SiteEconomyPhoneListings>>;
};

template <>
struct site_component_state_keys<SiteComponent::Craft>
{
    using type = type_list<
        state_key<StateSetId::SiteCraftDeviceCacheRuntime>,
        state_key<StateSetId::SiteCraftDeviceCaches>,
        state_key<StateSetId::SiteCraftNearbyItems>,
        state_key<StateSetId::SiteCraftPhoneCacheMeta>,
        state_key<StateSetId::SiteCraftPhoneItems>,
        state_key<StateSetId::SiteCraftContextMeta>,
        state_key<StateSetId::SiteCraftContextOptions>>;
};

template <>
struct site_component_state_keys<SiteComponent::Action>
{
    using type = type_list<
        state_key<StateSetId::SiteActionMeta>,
        state_key<StateSetId::SiteActionReservedInputItemStacks>,
        state_key<StateSetId::SiteActionResolvedHarvestOutputs>>;
};

template <>
struct site_component_state_keys<SiteComponent::Counters>
{
    using type = type_list<state_key<StateSetId::SiteCounters>>;
};

template <>
struct site_component_state_keys<SiteComponent::Objective>
{
    using type = type_list<
        state_key<StateSetId::SiteObjectiveMeta>,
        state_key<StateSetId::SiteObjectiveTargetTileIndices>,
        state_key<StateSetId::SiteObjectiveTargetTileMask>,
        state_key<StateSetId::SiteObjectiveConnectionStartTileIndices>,
        state_key<StateSetId::SiteObjectiveConnectionStartTileMask>,
        state_key<StateSetId::SiteObjectiveConnectionGoalTileIndices>,
        state_key<StateSetId::SiteObjectiveConnectionGoalTileMask>>;
};

template <>
struct site_component_state_keys<SiteComponent::PlantWeatherRuntime>
{
    using type = type_list<
        state_key<StateSetId::SitePlantWeatherContributionMeta>,
        state_key<StateSetId::SitePlantWeatherContributionDirtyTileIndices>,
        state_key<StateSetId::SitePlantWeatherContributionDirtyTileMask>>;
};

template <>
struct site_component_state_keys<SiteComponent::DeviceWeatherRuntime>
{
    using type = type_list<
        state_key<StateSetId::SiteDeviceWeatherContributionMeta>,
        state_key<StateSetId::SiteDeviceWeatherContributionDirtyTileIndices>,
        state_key<StateSetId::SiteDeviceWeatherContributionDirtyTileMask>>;
};

template <SiteComponentMask Mask, std::size_t Index = 0U>
struct site_mask_state_keys
{
private:
    static constexpr SiteComponent component = static_cast<SiteComponent>(Index);
    using current = std::conditional_t<
        (Mask & site_component_bit(component)) != 0U,
        typename site_component_state_keys<component>::type,
        type_list<>>;
    using rest = typename site_mask_state_keys<Mask, Index + 1U>::type;

public:
    using type = type_list_concat_t<current, rest>;
};

template <SiteComponentMask Mask>
struct site_mask_state_keys<Mask, static_cast<std::size_t>(SiteComponent::Count)>
{
    using type = type_list<>;
};

template <typename AccessedKeys, typename OwnedKeys>
struct remove_owned_state_keys;

template <typename OwnedKeys>
struct remove_owned_state_keys<type_list<>, OwnedKeys>
{
    using type = type_list<>;
};

template <typename First, typename... Rest, typename OwnedKeys>
struct remove_owned_state_keys<type_list<First, Rest...>, OwnedKeys>
{
private:
    using rest = typename remove_owned_state_keys<type_list<Rest...>, OwnedKeys>::type;

public:
    using type = std::conditional_t<
        type_list_contains_v<First, OwnedKeys>,
        rest,
        type_list_concat_t<type_list<First>, rest>>;
};

template <typename ReadKeys, typename OwnedKeys>
using remove_owned_state_keys_t = typename remove_owned_state_keys<ReadKeys, OwnedKeys>::type;

template <typename KeyList>
using state_descriptors_from_keys_t =
    shared_framework::runtime::type_list_transform_t<descriptor_from_state_key, KeyList>;

template <typename DescriptorList>
struct state_set_ids_from_descriptors;

template <typename... Descriptors>
struct state_set_ids_from_descriptors<type_list<Descriptors...>>
{
    static constexpr std::array<StateSetId, sizeof...(Descriptors)> value {
        static_cast<StateSetId>(shared_framework::runtime::state_descriptor_key_t<Descriptors>::value)...};
};

template <typename System>
[[nodiscard]] constexpr std::span<const StateSetId> runtime_owned_state_set_ids() noexcept
{
    using owned_states = shared_framework::runtime::runtime_owned_states_t<System>;
    return std::span<const StateSetId> {state_set_ids_from_descriptors<owned_states>::value};
}

template <typename System, typename = void>
struct explicit_owned_state_keys
{
    using type = type_list<>;
};

template <typename System>
struct explicit_owned_state_keys<System, std::void_t<decltype(System::k_owned_state_sets)>>
{
    using type = state_keys_from_array_t<System::k_owned_state_sets>;
};

template <typename System, typename = void>
struct site_access_owned_state_keys
{
    using type = type_list<>;
};

template <typename System>
struct site_access_owned_state_keys<System, std::void_t<decltype(System::access())>>
{
    using type = typename site_mask_state_keys<System::access().own_components>::type;
};

template <typename System, typename = void>
struct site_access_read_state_keys
{
    using type = type_list<>;
};

template <typename System>
struct site_access_read_state_keys<System, std::void_t<decltype(System::access())>>
{
    using type = typename site_mask_state_keys<System::access().read_components>::type;
};
}  // namespace gs1

namespace shared_framework::runtime
{
template <typename System>
struct runtime_system_contract_adapter<System, void>
{
private:
    using explicit_owned_keys =
        gs1::type_list_unique_t<typename gs1::explicit_owned_state_keys<System>::type>;
    using access_owned_keys =
        gs1::type_list_unique_t<typename gs1::site_access_owned_state_keys<System>::type>;
    using owned_keys =
        gs1::type_list_unique_t<gs1::type_list_concat_t<explicit_owned_keys, access_owned_keys>>;
    using read_keys =
        gs1::type_list_unique_t<typename gs1::site_access_read_state_keys<System>::type>;

public:
    using owned_states = gs1::state_descriptors_from_keys_t<owned_keys>;
    using accessed_states = gs1::type_list_unique_t<gs1::remove_owned_state_keys_t<read_keys, owned_keys>>;
};
}  // namespace shared_framework::runtime
