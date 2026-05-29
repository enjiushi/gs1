#pragma once

#include "runtime/system_interface.h"
#include "site/systems/site_system_context.h"
#include "gs1/status.h"

namespace gs1
{
class DeviceSupportSystem final : public IRuntimeSystem
{
public:
    using subscribed_messages = type_list<>;
    using emitted_runtime_messages = type_list<>;
    static constexpr std::optional<Gs1RuntimeProfileSystemId> profile_id =
        GS1_RUNTIME_PROFILE_SYSTEM_DEVICE_SUPPORT;
    static constexpr std::optional<std::uint32_t> fixed_step_order_value = 12U;

    [[nodiscard]] std::span<const StateSetId> owned_state_sets() const noexcept override
    {
        return site_access_owned_state_sets<DeviceSupportSystem>();
    }

    [[nodiscard]] const char* name() const noexcept override;
    [[nodiscard]] GameMessageSubscriptionSpan subscribed_game_messages() const noexcept override;
    [[nodiscard]] HostMessageSubscriptionSpan subscribed_host_messages() const noexcept override;
    [[nodiscard]] std::optional<Gs1RuntimeProfileSystemId> profile_system_id() const noexcept override;
    [[nodiscard]] std::optional<std::uint32_t> fixed_step_order() const noexcept override;
    void run(RuntimeInvocation& invocation) override;

    [[nodiscard]] static constexpr SiteSystemAccess access() noexcept
    {
        return SiteSystemAccess {
            "DeviceSupportSystem",
            site_component_mask_of(
                SiteComponent::TileLayout,
                SiteComponent::TileWeather,
                SiteComponent::DeviceCondition,
                SiteComponent::DeviceRuntime),
            site_component_mask_of(SiteComponent::DeviceRuntime)};
    }
};

template <>
struct system_state_tags<DeviceSupportSystem>
{
    using type = type_list<RuntimeFixedStepSecondsTag>;
};
}  // namespace gs1
