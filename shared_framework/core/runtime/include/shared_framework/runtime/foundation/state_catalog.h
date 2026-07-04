#pragma once

#include "shared_framework/runtime/foundation/state_descriptor.h"
#include "shared_framework/runtime/foundation/system_pack.h"
#include "shared_framework/runtime/foundation/system_traits.h"
#include "shared_framework/runtime/foundation/type_list.h"

#include <tuple>
#include <type_traits>

namespace shared_framework::runtime
{
template <typename Descriptor>
using state_descriptor_key_t = typename Descriptor::key;

template <typename Descriptor>
struct state_descriptor_key
{
    using type = typename Descriptor::key;
};

template <typename Key, typename DescriptorList>
struct find_state_descriptor_by_key;

template <typename Key>
struct find_state_descriptor_by_key<Key, type_list<>>
{
    using type = void;
};

template <typename Key, typename First, typename... Rest>
struct find_state_descriptor_by_key<Key, type_list<First, Rest...>>
{
    using type = std::conditional_t<
        std::same_as<Key, state_descriptor_key_t<First>>,
        First,
        typename find_state_descriptor_by_key<Key, type_list<Rest...>>::type>;
};

template <typename Key, typename DescriptorList>
using find_state_descriptor_by_key_t = typename find_state_descriptor_by_key<Key, DescriptorList>::type;

template <typename Key, typename DescriptorList>
struct state_descriptor_index_by_key;

template <typename Key>
struct state_descriptor_index_by_key<Key, type_list<>>
{
    static constexpr std::size_t value = static_cast<std::size_t>(-1);
};

template <typename Key, typename First, typename... Rest>
struct state_descriptor_index_by_key<Key, type_list<First, Rest...>>
{
private:
    static constexpr std::size_t rest_value = state_descriptor_index_by_key<Key, type_list<Rest...>>::value;

public:
    static constexpr std::size_t value =
        std::same_as<Key, state_descriptor_key_t<First>>
            ? 0U
            : (rest_value == static_cast<std::size_t>(-1) ? rest_value : (1U + rest_value));
};

template <typename Key, typename DescriptorList>
inline constexpr std::size_t state_descriptor_index_by_key_v =
    state_descriptor_index_by_key<Key, DescriptorList>::value;

template <typename... Systems>
struct collected_owned_state_descriptors
{
    using type = type_list_concat_t<runtime_owned_states_t<Systems>...>;
};

template <typename Pack>
struct runtime_state_catalog;

template <typename... Systems>
struct runtime_state_catalog<system_pack<Systems...>>
{
    using descriptors = typename collected_owned_state_descriptors<Systems...>::type;
    using keys = type_list_transform_t<state_descriptor_key, descriptors>;
};

template <typename DescriptorList>
class state_storage;

template <typename... Descriptors>
class state_storage<type_list<Descriptors...>>
{
public:
    template <typename Key>
    [[nodiscard]] auto& get() noexcept
    {
        constexpr std::size_t descriptor_index =
            state_descriptor_index_by_key_v<Key, type_list<Descriptors...>>;
        static_assert(
            descriptor_index != static_cast<std::size_t>(-1),
            "Requested state key is not present in this storage.");
        return std::get<descriptor_index>(slots_).get();
    }

    template <typename Key>
    [[nodiscard]] const auto& get() const noexcept
    {
        constexpr std::size_t descriptor_index =
            state_descriptor_index_by_key_v<Key, type_list<Descriptors...>>;
        static_assert(
            descriptor_index != static_cast<std::size_t>(-1),
            "Requested state key is not present in this storage.");
        return std::get<descriptor_index>(slots_).get();
    }

private:
    std::tuple<typename Descriptors::slot_type...> slots_ {};
};
}  // namespace shared_framework::runtime
