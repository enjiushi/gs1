#pragma once

#include "runtime/type_list.h"

#include <cstddef>
#include <tuple>

namespace gs1
{
template <class... Systems>
struct system_pack
{
    using list = type_list<Systems...>;
    using tuple_type = std::tuple<Systems...>;

    static constexpr std::size_t size = sizeof...(Systems);
};

template <typename System, typename SystemPack>
struct system_pack_index;

template <typename System, typename... Systems>
struct system_pack_index<System, system_pack<Systems...>>
{
private:
    template <std::size_t Index, typename First, typename... Rest>
    static consteval std::size_t resolve_impl() noexcept
    {
        if constexpr (std::same_as<System, First>)
        {
            return Index;
        }
        else if constexpr (sizeof...(Rest) == 0U)
        {
            return sizeof...(Systems);
        }
        else
        {
            return resolve_impl<Index + 1U, Rest...>();
        }
    }

public:
    static constexpr std::size_t value = resolve_impl<0U, Systems...>();
};

template <typename System, typename SystemPack>
inline constexpr std::size_t system_pack_index_v = system_pack_index<System, SystemPack>::value;
}  // namespace gs1
