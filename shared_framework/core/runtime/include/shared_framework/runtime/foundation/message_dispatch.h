#pragma once

#include "shared_framework/runtime/foundation/system_pack.h"
#include "shared_framework/runtime/foundation/system_traits.h"
#include "shared_framework/runtime/foundation/type_list.h"

namespace shared_framework::runtime
{
template <typename Message, typename SystemPack>
struct typed_message_subscribers;

template <typename Message, typename... Systems>
struct typed_message_subscribers<Message, system_pack<Systems...>>
{
    template <typename System>
    using predicate = std::bool_constant<
        type_list_contains_v<Message, runtime_subscribed_messages_t<System>>>;

    using type = type_list_filter_t<predicate, type_list<Systems...>>;
};
}  // namespace shared_framework::runtime
