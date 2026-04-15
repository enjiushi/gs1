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
                SiteComponent::WorkerMotion,
                SiteComponent::Inventory,
                SiteComponent::Action),
            site_component_mask_of(SiteComponent::Action)};
    }

    [[nodiscard]] static bool subscribes_to(GameCommandType type) noexcept;
    [[nodiscard]] static Gs1Status process_command(
        SiteSystemContext<ActionExecutionSystem>& context,
        const GameCommand& command);
    static void run(SiteSystemContext<ActionExecutionSystem>& context);
};
}  // namespace gs1
