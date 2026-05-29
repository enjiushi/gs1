#pragma once

#include "gs1/status.h"
#include "runtime/system_interface.h"
#include "site/systems/site_system_context.h"

namespace gs1
{
class DeviceWeatherContributionSystem final : public IRuntimeSystem
{
public:
    using subscribed_messages = type_list<
        SiteRunStartedMessage,
        SiteDevicePlacedMessage,
        SiteDeviceBrokenMessage,
        SiteDeviceRepairedMessage,
        SiteDeviceConditionChangedMessage>;
    using emitted_runtime_messages = type_list<>;
    static constexpr std::optional<Gs1RuntimeProfileSystemId> profile_id =
        GS1_RUNTIME_PROFILE_SYSTEM_LOCAL_WEATHER_RESOLVE;
    static constexpr std::optional<std::uint32_t> fixed_step_order_value = 7U;

    [[nodiscard]] std::span<const StateSetId> owned_state_sets() const noexcept override
    {
        return site_access_owned_state_sets<DeviceWeatherContributionSystem>();
    }

    [[nodiscard]] const char* name() const noexcept override;
    [[nodiscard]] GameMessageSubscriptionSpan subscribed_game_messages() const noexcept override;
    [[nodiscard]] HostMessageSubscriptionSpan subscribed_host_messages() const noexcept override;
    [[nodiscard]] std::optional<Gs1RuntimeProfileSystemId> profile_system_id() const noexcept override;
    [[nodiscard]] std::optional<std::uint32_t> fixed_step_order() const noexcept override;
    [[nodiscard]] Gs1Status handle(
        RuntimeInvocation& invocation,
        const SiteRunStartedMessage& message);
    [[nodiscard]] Gs1Status handle(
        RuntimeInvocation& invocation,
        const SiteDevicePlacedMessage& message);
    [[nodiscard]] Gs1Status handle(
        RuntimeInvocation& invocation,
        const SiteDeviceBrokenMessage& message);
    [[nodiscard]] Gs1Status handle(
        RuntimeInvocation& invocation,
        const SiteDeviceRepairedMessage& message);
    [[nodiscard]] Gs1Status handle(
        RuntimeInvocation& invocation,
        const SiteDeviceConditionChangedMessage& message);
    void run(RuntimeInvocation& invocation) override;

    [[nodiscard]] static constexpr SiteSystemAccess access() noexcept
    {
        return SiteSystemAccess {
            "DeviceWeatherContributionSystem",
            site_component_mask_of(
                SiteComponent::TileLayout,
                SiteComponent::TileDeviceWeatherContribution,
                SiteComponent::DeviceCondition,
                SiteComponent::DeviceRuntime,
                SiteComponent::Weather,
                SiteComponent::DeviceWeatherRuntime),
            site_component_mask_of(
                SiteComponent::TileDeviceWeatherContribution,
                SiteComponent::DeviceWeatherRuntime)};
    }
};

template <>
struct system_state_tags<DeviceWeatherContributionSystem>
{
    using type = type_list<>;
};
}  // namespace gs1
