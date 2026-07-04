#pragma once

#include <cstddef>
#include <type_traits>

namespace shared_framework::runtime
{
template <typename... Types>
struct type_list
{
};

template <typename List>
struct type_list_size;

template <typename... Types>
struct type_list_size<type_list<Types...>> : std::integral_constant<std::size_t, sizeof...(Types)>
{
};

template <typename List>
inline constexpr std::size_t type_list_size_v = type_list_size<List>::value;

template <typename Type, typename List>
struct type_list_contains;

template <typename Type, typename... Types>
struct type_list_contains<Type, type_list<Types...>> : std::bool_constant<(std::same_as<Type, Types> || ...)>
{
};

template <typename Type, typename List>
inline constexpr bool type_list_contains_v = type_list_contains<Type, List>::value;

template <typename List>
struct type_list_is_unique;

template <>
struct type_list_is_unique<type_list<>> : std::true_type
{
};

template <typename First, typename... Rest>
struct type_list_is_unique<type_list<First, Rest...>> : std::bool_constant<
    (!type_list_contains_v<First, type_list<Rest...>>) &&
    type_list_is_unique<type_list<Rest...>>::value>
{
};

template <typename List>
inline constexpr bool type_list_is_unique_v = type_list_is_unique<List>::value;

template <typename... Lists>
struct type_list_concat;

template <>
struct type_list_concat<>
{
    using type = type_list<>;
};

template <typename... Types>
struct type_list_concat<type_list<Types...>>
{
    using type = type_list<Types...>;
};

template <typename... Left, typename... Right, typename... Rest>
struct type_list_concat<type_list<Left...>, type_list<Right...>, Rest...>
{
    using type = typename type_list_concat<type_list<Left..., Right...>, Rest...>::type;
};

template <typename... Lists>
using type_list_concat_t = typename type_list_concat<Lists...>::type;

template <template <typename> typename Predicate, typename List>
struct type_list_filter;

template <template <typename> typename Predicate>
struct type_list_filter<Predicate, type_list<>>
{
    using type = type_list<>;
};

template <template <typename> typename Predicate, typename First, typename... Rest>
struct type_list_filter<Predicate, type_list<First, Rest...>>
{
private:
    using rest = typename type_list_filter<Predicate, type_list<Rest...>>::type;

public:
    using type = std::conditional_t<
        Predicate<First>::value,
        type_list_concat_t<type_list<First>, rest>,
        rest>;
};

template <template <typename> typename Predicate, typename List>
using type_list_filter_t = typename type_list_filter<Predicate, List>::type;

template <typename List, typename Type>
struct type_list_append_unique;

template <typename... Types, typename Type>
struct type_list_append_unique<type_list<Types...>, Type>
{
    using type = std::conditional_t<
        type_list_contains_v<Type, type_list<Types...>>,
        type_list<Types...>,
        type_list<Types..., Type>>;
};

template <typename List, typename Type>
using type_list_append_unique_t = typename type_list_append_unique<List, Type>::type;

template <typename Accumulator, typename List>
struct type_list_unique_accumulate;

template <typename... Accumulated>
struct type_list_unique_accumulate<type_list<Accumulated...>, type_list<>>
{
    using type = type_list<Accumulated...>;
};

template <typename... Accumulated, typename First, typename... Rest>
struct type_list_unique_accumulate<type_list<Accumulated...>, type_list<First, Rest...>>
{
    using next = type_list_append_unique_t<type_list<Accumulated...>, First>;
    using type = typename type_list_unique_accumulate<next, type_list<Rest...>>::type;
};

template <typename List>
struct type_list_unique
{
    using type = typename type_list_unique_accumulate<type_list<>, List>::type;
};

template <typename List>
using type_list_unique_t = typename type_list_unique<List>::type;

template <template <typename> typename Mapper, typename List>
struct type_list_transform;

template <template <typename> typename Mapper>
struct type_list_transform<Mapper, type_list<>>
{
    using type = type_list<>;
};

template <template <typename> typename Mapper, typename... Types>
struct type_list_transform<Mapper, type_list<Types...>>
{
    using type = type_list<typename Mapper<Types>::type...>;
};

template <template <typename> typename Mapper, typename List>
using type_list_transform_t = typename type_list_transform<Mapper, List>::type;
}  // namespace shared_framework::runtime
