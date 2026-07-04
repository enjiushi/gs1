#pragma once

#include "messages/game_message.h"
#include "runtime/runtime_state_access.h"
#include "runtime/system_pack.h"
#include "runtime/state_set.h"
#include "site/systems/site_system_context.h"
#include "gs1/status.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <type_traits>

namespace gs1
{
class IRuntimeSystem
{
public:
    virtual ~IRuntimeSystem() = default;

    [[nodiscard]] virtual const char* name() const noexcept = 0;
    [[nodiscard]] virtual std::optional<Gs1RuntimeProfileSystemId> profile_system_id() const noexcept = 0;
    [[nodiscard]] virtual std::optional<std::uint32_t> fixed_step_order() const noexcept = 0;
    virtual void run(RuntimeInvocation& invocation) = 0;
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
struct runtime_profile_system_id
{
    static constexpr std::optional<Gs1RuntimeProfileSystemId> value = std::nullopt;
};

template <typename System>
struct runtime_profile_system_id<System, std::void_t<decltype(System::profile_id)>>
{
    static constexpr std::optional<Gs1RuntimeProfileSystemId> value = System::profile_id;
};

template <typename System>
inline constexpr std::optional<Gs1RuntimeProfileSystemId> runtime_profile_system_id_v =
    runtime_profile_system_id<System>::value;

template <typename System, typename = void>
struct runtime_fixed_step_order
{
    static constexpr std::optional<std::uint32_t> value = std::nullopt;
};

template <typename System>
struct runtime_fixed_step_order<System, std::void_t<decltype(System::fixed_step_order_value)>>
{
    static constexpr std::optional<std::uint32_t> value = System::fixed_step_order_value;
};

template <typename System>
inline constexpr std::optional<std::uint32_t> runtime_fixed_step_order_v =
    runtime_fixed_step_order<System>::value;

template <typename System>
[[nodiscard]] std::optional<Gs1RuntimeProfileSystemId> runtime_profile_system_id_for(
    const IRuntimeSystem& system) noexcept
{
    if constexpr (runtime_profile_system_id_v<System>.has_value())
    {
        return runtime_profile_system_id_v<System>;
    }
    else
    {
        return system.profile_system_id();
    }
}

template <typename System>
[[nodiscard]] std::optional<std::uint32_t> runtime_fixed_step_order_for(
    const IRuntimeSystem& system) noexcept
{
    if constexpr (runtime_fixed_step_order_v<System>.has_value())
    {
        return runtime_fixed_step_order_v<System>;
    }
    else
    {
        return system.fixed_step_order();
    }
}
}  // namespace gs1
