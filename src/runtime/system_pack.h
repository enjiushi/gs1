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
    static constexpr std::size_t resolve() noexcept
    {
        std::size_t index = 0U;
        bool found = false;
        ((std::same_as<System, Systems>
              ? (found = true, false)
              : (++index, false)) || ...);
        return found ? index : sizeof...(Systems);
    }

public:
    static constexpr std::size_t value = resolve();
};

template <typename System, typename SystemPack>
inline constexpr std::size_t system_pack_index_v = system_pack_index<System, SystemPack>::value;
}  // namespace gs1
