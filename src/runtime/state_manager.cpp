#include "runtime/state_manager.h"

#include "runtime/system_interface.h"

#include <stdexcept>

namespace gs1
{
void StateManager::register_resolver(IRuntimeSystem& system)
{
    for (const StateSetId state_set : system.owned_state_sets())
    {
        register_owned_state_set(system, state_set);
    }
}

void StateManager::register_owned_state_set(IRuntimeSystem& system, StateSetId state_set)
{
    const auto slot_index = index(state_set);
    if (active_resolver_by_state_set_[slot_index] != nullptr &&
        active_resolver_by_state_set_[slot_index] != &system)
    {
        throw std::logic_error("State set has multiple active resolvers.");
    }

    if (default_resolver_by_state_set_[slot_index] == nullptr)
    {
        default_resolver_by_state_set_[slot_index] = &system;
    }

    active_resolver_by_state_set_[slot_index] = &system;
}
}  // namespace gs1
