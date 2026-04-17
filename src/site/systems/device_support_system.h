#pragma once

#include "site/systems/site_system_context.h"
#include "gs1/status.h"

namespace gs1
{
class DeviceSupportSystem final
{
public:
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

    [[nodiscard]] static bool subscribes_to(GameMessageType type) noexcept;
    [[nodiscard]] static Gs1Status process_message(
        SiteSystemContext<DeviceSupportSystem>& context,
        const GameMessage& message);
    static void run(SiteSystemContext<DeviceSupportSystem>& context);
};
}  // namespace gs1
