#pragma once

#include "site/systems/site_system_context.h"
#include "gs1/status.h"

namespace gs1
{
class ActionExecutionSystem final
{
public:
    [[nodiscard]] static constexpr SiteSystemAccess access() noexcept
    {
        return SiteSystemAccess {
            "ActionExecutionSystem",
            site_component_mask_of(
                SiteComponent::Time,
                SiteComponent::TileLayout,
                SiteComponent::TileExcavation,
                SiteComponent::TileWeather,
                SiteComponent::DeviceCondition,
                SiteComponent::WorkerMotion,
                SiteComponent::WorkerNeeds,
                SiteComponent::Inventory,
                SiteComponent::Modifier,
                SiteComponent::Craft,
                SiteComponent::Action),
            site_component_mask_of(
                SiteComponent::TileExcavation,
                SiteComponent::Action)};
    }

    [[nodiscard]] static bool subscribes_to(GameMessageType type) noexcept;
    [[nodiscard]] static Gs1Status process_message(
        SiteSystemContext<ActionExecutionSystem>& context,
        const GameMessage& message);
    static void run(SiteSystemContext<ActionExecutionSystem>& context);
};
}  // namespace gs1
