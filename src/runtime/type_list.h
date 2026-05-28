#pragma once

#include <concepts>
#include <tuple>
#include <type_traits>

namespace gs1
{
template <class... Ts>
struct type_list
{
};

template <class T, class List>
struct type_list_contains;

template <class T, class... Ts>
struct type_list_contains<T, type_list<Ts...>> : std::bool_constant<(std::same_as<T, Ts> || ...)>
{
};

template <class T, class List>
inline constexpr bool type_list_contains_v = type_list_contains<T, List>::value;

template <class... Lists>
struct type_list_concat;

template <>
struct type_list_concat<>
{
    using type = type_list<>;
};

template <class... Ts>
struct type_list_concat<type_list<Ts...>>
{
    using type = type_list<Ts...>;
};

template <class... Ts, class... Us, class... Rest>
struct type_list_concat<type_list<Ts...>, type_list<Us...>, Rest...>
{
    using type = typename type_list_concat<type_list<Ts..., Us...>, Rest...>::type;
};

template <class... Lists>
using type_list_concat_t = typename type_list_concat<Lists...>::type;

template <template <class> class Predicate, class List>
struct type_list_filter;

template <template <class> class Predicate>
struct type_list_filter<Predicate, type_list<>>
{
    using type = type_list<>;
};

template <template <class> class Predicate, class T, class... Rest>
struct type_list_filter<Predicate, type_list<T, Rest...>>
{
    using remainder = typename type_list_filter<Predicate, type_list<Rest...>>::type;
    using type = std::conditional_t<
        Predicate<T>::value,
        type_list_concat_t<type_list<T>, remainder>,
        remainder>;
};

template <template <class> class Predicate, class List>
using type_list_filter_t = typename type_list_filter<Predicate, List>::type;

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
