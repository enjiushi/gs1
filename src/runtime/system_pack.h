#pragma once

#include "shared_framework/runtime/foundation/system_pack.h"

namespace gs1
{
template <class... Systems>
using system_pack = shared_framework::runtime::system_pack<Systems...>;

template <typename System, typename SystemPack>
inline constexpr std::size_t system_pack_index_v =
    shared_framework::runtime::system_pack_index_v<System, SystemPack>;
}  // namespace gs1
