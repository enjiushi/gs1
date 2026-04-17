#pragma once

#include "site/systems/site_system_context.h"
#include "gs1/status.h"

namespace gs1
{
class DeviceMaintenanceSystem final
{
public:
    [[nodiscard]] static constexpr SiteSystemAccess access() noexcept
    {
        return SiteSystemAccess {
            "DeviceMaintenanceSystem",
            site_component_mask_of(
                SiteComponent::TileLayout,
                SiteComponent::TileEcology,
                SiteComponent::DeviceCondition,
                SiteComponent::Weather),
            site_component_mask_of(SiteComponent::DeviceCondition)};
    }

    [[nodiscard]] static bool subscribes_to(GameMessageType type) noexcept;
    [[nodiscard]] static Gs1Status process_message(
        SiteSystemContext<DeviceMaintenanceSystem>& context,
        const GameMessage& message);
    static void run(SiteSystemContext<DeviceMaintenanceSystem>& context);
};
}  // namespace gs1
