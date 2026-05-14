#pragma once

#include "messages/game_message.h"
#include "runtime/runtime_state_access.h"
#include "runtime/state_set.h"
#include "site/systems/site_system_context.h"
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

    [[nodiscard]] virtual std::span<const StateSetId> owned_state_sets() const noexcept
    {
        return {};
    }
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

template <typename System>
[[nodiscard]] constexpr std::span<const StateSetId> runtime_declared_owned_state_sets() noexcept
{
    if constexpr (requires { System::k_owned_state_sets; })
    {
        return std::span<const StateSetId> {System::k_owned_state_sets};
    }
    else
    {
        return {};
    }
}

[[nodiscard]] constexpr std::optional<StateSetId> site_component_owner_state_set(
    SiteComponent component) noexcept
{
    switch (component)
    {
    case SiteComponent::RunMeta:
        return StateSetId::SiteRunMeta;
    case SiteComponent::Time:
        return StateSetId::SiteClock;
    case SiteComponent::Camp:
        return StateSetId::SiteCamp;
    case SiteComponent::Inventory:
        return StateSetId::SiteInventory;
    case SiteComponent::Contractor:
        return StateSetId::SiteContractor;
    case SiteComponent::Weather:
        return StateSetId::SiteWeather;
    case SiteComponent::LocalWeatherRuntime:
        return StateSetId::SiteLocalWeatherResolve;
    case SiteComponent::Event:
        return StateSetId::SiteEvent;
    case SiteComponent::TaskBoard:
        return StateSetId::SiteTaskBoard;
    case SiteComponent::Modifier:
        return StateSetId::SiteModifier;
    case SiteComponent::Economy:
        return StateSetId::SiteEconomy;
    case SiteComponent::Craft:
        return StateSetId::SiteCraft;
    case SiteComponent::Action:
        return StateSetId::SiteAction;
    case SiteComponent::Counters:
        return StateSetId::SiteCounters;
    case SiteComponent::Objective:
        return StateSetId::SiteObjective;
    case SiteComponent::PlantWeatherRuntime:
        return StateSetId::SitePlantWeatherContribution;
    case SiteComponent::DeviceWeatherRuntime:
        return StateSetId::SiteDeviceWeatherContribution;
    default:
        return std::nullopt;
    }
}

template <typename System>
[[nodiscard]] std::span<const StateSetId> site_access_owned_state_sets() noexcept
{
    if constexpr (requires { System::access(); })
    {
        static constexpr auto resolved = []()
        {
            std::array<StateSetId, static_cast<std::size_t>(SiteComponent::Count)> values {};
            std::size_t count = 0U;

            for (std::size_t index = 0; index < static_cast<std::size_t>(SiteComponent::Count); ++index)
            {
                const auto component = static_cast<SiteComponent>(index);
                if ((System::access().own_components & site_component_bit(component)) == 0U)
                {
                    continue;
                }

                const auto mapped = site_component_owner_state_set(component);
                if (!mapped.has_value())
                {
                    continue;
                }

                bool already_present = false;
                for (std::size_t existing = 0; existing < count; ++existing)
                {
                    if (values[existing] == *mapped)
                    {
                        already_present = true;
                        break;
                    }
                }

                if (!already_present)
                {
                    values[count++] = *mapped;
                }
            }

            return std::pair {values, count};
        }();

        return std::span<const StateSetId> {resolved.first.data(), resolved.second};
    }
    else
    {
        return {};
    }
}
}  // namespace gs1
