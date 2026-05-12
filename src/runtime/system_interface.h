#pragma once

#include "messages/game_message.h"
#include "runtime/runtime_state_access.h"
#include "gs1/status.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace gs1
{
inline constexpr std::size_t k_runtime_host_message_type_count =
    static_cast<std::size_t>(GS1_HOST_EVENT_SITE_SCENE_READY) + 1U;

using GameMessageSubscriptionSpan = std::span<const GameMessageType>;
using HostMessageSubscriptionSpan = std::span<const Gs1HostMessageType>;

class IRuntimeSystem
{
public:
    virtual ~IRuntimeSystem() = default;

    [[nodiscard]] virtual const char* name() const noexcept = 0;
    [[nodiscard]] virtual GameMessageSubscriptionSpan subscribed_game_messages() const noexcept = 0;
    [[nodiscard]] virtual HostMessageSubscriptionSpan subscribed_host_messages() const noexcept = 0;
    [[nodiscard]] virtual std::optional<Gs1RuntimeProfileSystemId> profile_system_id() const noexcept = 0;
    [[nodiscard]] virtual std::optional<std::uint32_t> fixed_step_order() const noexcept = 0;
    [[nodiscard]] virtual Gs1Status process_game_message(
        RuntimeInvocation& invocation,
        const GameMessage& message) = 0;
    [[nodiscard]] virtual Gs1Status process_host_message(
        RuntimeInvocation& invocation,
        const Gs1HostMessage& message) = 0;
    virtual void run(RuntimeInvocation& invocation) = 0;
};

using RuntimeGameMessageSubscribers = std::array<std::vector<IRuntimeSystem*>, k_game_message_type_count>;
using RuntimeHostMessageSubscribers =
    std::array<std::vector<IRuntimeSystem*>, k_runtime_host_message_type_count>;

template <typename EnumType>
[[nodiscard]] constexpr bool runtime_subscription_contains(
    std::span<const EnumType> subscriptions,
    EnumType type) noexcept
{
    for (const EnumType subscribed_type : subscriptions)
    {
        if (subscribed_type == type)
        {
            return true;
        }
    }

    return false;
}
}  // namespace gs1
