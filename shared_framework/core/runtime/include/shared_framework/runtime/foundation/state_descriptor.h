#pragma once

#include "shared_framework/runtime/foundation/state_slot.h"

namespace shared_framework::runtime
{
template <typename Key, typename Value, template <typename> typename Slot = cache_aligned_state_slot>
struct state_descriptor
{
    using key = Key;
    using value_type = Value;
    using slot_type = Slot<Value>;
};
}  // namespace shared_framework::runtime
