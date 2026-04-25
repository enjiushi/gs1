#pragma once

#include "gs1/status.h"
#include "site/systems/site_system_context.h"

namespace gs1
{
class DeviceWeatherContributionSystem final
{
public:
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

    [[nodiscard]] static bool subscribes_to(GameMessageType type) noexcept;
    [[nodiscard]] static Gs1Status process_message(
        SiteSystemContext<DeviceWeatherContributionSystem>& context,
        const GameMessage& message);
    static void run(SiteSystemContext<DeviceWeatherContributionSystem>& context);
};
}  // namespace gs1
