#pragma once

#include "shared_framework/runtime/foundation/type_list.h"

#include <type_traits>

namespace shared_framework::runtime
{
template <typename System, typename = void>
struct runtime_system_contract_adapter
{
};

template <typename System, typename = void>
struct runtime_subscribed_messages
{
    using type = type_list<>;
};

template <typename System>
struct runtime_subscribed_messages<System, std::void_t<typename System::subscribed_messages>>
{
    using type = typename System::subscribed_messages;
};

template <typename System>
using runtime_subscribed_messages_t = typename runtime_subscribed_messages<System>::type;

template <typename System, typename = void>
struct runtime_owned_states
{
    using type = type_list<>;
};

template <typename System>
struct runtime_owned_states<System, std::void_t<typename System::owned_states>>
{
    using type = typename System::owned_states;
};

template <typename System>
struct runtime_owned_states<
    System,
    std::void_t<typename runtime_system_contract_adapter<System>::owned_states>>
{
    using type = typename runtime_system_contract_adapter<System>::owned_states;
};

template <typename System>
using runtime_owned_states_t = typename runtime_owned_states<System>::type;

template <typename System, typename = void>
struct runtime_accessed_states
{
    using type = type_list<>;
};

template <typename System>
struct runtime_accessed_states<System, std::void_t<typename System::accessed_states>>
{
    using type = typename System::accessed_states;
};

template <typename System>
struct runtime_accessed_states<
    System,
    std::void_t<typename runtime_system_contract_adapter<System>::accessed_states>>
{
    using type = typename runtime_system_contract_adapter<System>::accessed_states;
};

template <typename System>
using runtime_accessed_states_t = typename runtime_accessed_states<System>::type;
}  // namespace shared_framework::runtime
