#pragma once

#include "shared_framework/runtime/foundation/type_list.h"

#include <tuple>

namespace gs1
{
template <class... Ts>
using type_list = shared_framework::runtime::type_list<Ts...>;

template <class List>
inline constexpr std::size_t type_list_size_v = shared_framework::runtime::type_list_size_v<List>;

template <class T, class List>
inline constexpr bool type_list_contains_v = shared_framework::runtime::type_list_contains_v<T, List>;

template <class... Lists>
using type_list_concat_t = shared_framework::runtime::type_list_concat_t<Lists...>;

template <class List>
inline constexpr bool type_list_is_unique_v = shared_framework::runtime::type_list_is_unique_v<List>;

template <class List>
using type_list_unique_t = shared_framework::runtime::type_list_unique_t<List>;

template <template <class> class Predicate, class List>
using type_list_filter_t = shared_framework::runtime::type_list_filter_t<Predicate, List>;

template <class List>
struct type_list_to_tuple;

template <class... Ts>
struct type_list_to_tuple<type_list<Ts...>>
{
    using type = std::tuple<Ts...>;
};

template <class List>
using type_list_to_tuple_t = typename type_list_to_tuple<List>::type;

template <class List, class Func>
constexpr void for_each_type(Func&& func)
{
    []<class... Ts>(type_list<Ts...>, Func&& inner)
    {
        (inner.template operator()<Ts>(), ...);
    }(List {}, std::forward<Func>(func));
}
}  // namespace gs1
