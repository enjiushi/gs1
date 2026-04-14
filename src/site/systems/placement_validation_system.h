#pragma once

#include "site/systems/site_system_context.h"
#include "gs1/status.h"

namespace gs1
{
class PlacementValidationSystem final
{
public:
    [[nodiscard]] static constexpr SiteSystemAccess access() noexcept
    {
        return SiteSystemAccess {
            "PlacementValidationSystem",
            site_component_mask_of(
                SiteComponent::RunMeta,
                SiteComponent::TileLayout,
                SiteComponent::TileEcology),
            0U};
    }

    [[nodiscard]] static bool subscribes_to(GameCommandType type) noexcept;
    [[nodiscard]] static Gs1Status process_command(
        SiteSystemContext<PlacementValidationSystem>& context,
        const GameCommand& command);
    static void run(SiteSystemContext<PlacementValidationSystem>& context);
};
}  // namespace gs1
