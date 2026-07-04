#pragma once

#include "shared_framework/runtime/foundation/state_catalog.h"
#include "shared_framework/runtime/foundation/system_pack.h"
#include "shared_framework/runtime/foundation/system_traits.h"
#include "shared_framework/runtime/foundation/type_list.h"

namespace shared_framework::runtime
{
template <typename DescriptorList>
struct descriptor_keys_are_unique;

template <>
struct descriptor_keys_are_unique<type_list<>> : std::true_type
{
};

template <typename First, typename... Rest>
struct descriptor_keys_are_unique<type_list<First, Rest...>> : std::bool_constant<
    ((!std::same_as<state_descriptor_key_t<First>, state_descriptor_key_t<Rest>>) && ...) &&
    descriptor_keys_are_unique<type_list<Rest...>>::value>
{
};

template <typename DescriptorList>
inline constexpr bool descriptor_keys_are_unique_v = descriptor_keys_are_unique<DescriptorList>::value;

template <typename CandidateList, typename OwnedKeyList>
struct all_keys_are_owned;

template <typename OwnedKeyList>
struct all_keys_are_owned<type_list<>, OwnedKeyList> : std::true_type
{
};

template <typename First, typename... Rest, typename OwnedKeyList>
struct all_keys_are_owned<type_list<First, Rest...>, OwnedKeyList> : std::bool_constant<
    type_list_contains_v<First, OwnedKeyList> &&
    all_keys_are_owned<type_list<Rest...>, OwnedKeyList>::value>
{
};

template <typename System>
struct validate_single_system_contract
{
private:
    using owned = runtime_owned_states_t<System>;

public:
    static constexpr bool subscribed_messages_are_unique =
        type_list_is_unique_v<runtime_subscribed_messages_t<System>>;
    static constexpr bool owned_states_are_unique = descriptor_keys_are_unique_v<owned>;
    static constexpr bool accessed_states_are_unique =
        type_list_is_unique_v<runtime_accessed_states_t<System>>;
};

template <typename Pack>
struct validate_system_contracts;

template <typename... Systems>
struct validate_system_contracts<system_pack<Systems...>>
{
private:
    using owned_descriptors = typename runtime_state_catalog<system_pack<Systems...>>::descriptors;
    using owned_keys = typename runtime_state_catalog<system_pack<Systems...>>::keys;
    using accessed_keys = type_list_concat_t<runtime_accessed_states_t<Systems>...>;

public:
    static constexpr bool per_system_subscribed_messages_are_unique =
        (validate_single_system_contract<Systems>::subscribed_messages_are_unique && ...);

    static constexpr bool per_system_owned_states_are_unique =
        (validate_single_system_contract<Systems>::owned_states_are_unique && ...);

    static constexpr bool per_system_accessed_states_are_unique =
        (validate_single_system_contract<Systems>::accessed_states_are_unique && ...);

    static constexpr bool state_owners_are_unique = descriptor_keys_are_unique_v<owned_descriptors>;
    static constexpr bool all_accessed_states_are_owned =
        all_keys_are_owned<accessed_keys, owned_keys>::value;

    static consteval void validate()
    {
        static_assert(
            per_system_subscribed_messages_are_unique,
            "A system declared the same subscribed message more than once.");
        static_assert(
            per_system_owned_states_are_unique,
            "A system declared the same owned state more than once.");
        static_assert(
            per_system_accessed_states_are_unique,
            "A system declared the same accessed state more than once.");
        static_assert(
            state_owners_are_unique,
            "A state was owned by more than one system.");
        static_assert(
            all_accessed_states_are_owned,
            "A system accessed a state that is not owned by any system in the pack.");
    }
};
}  // namespace shared_framework::runtime
