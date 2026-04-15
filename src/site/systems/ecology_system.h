#pragma once

#include "site/systems/site_system_context.h"
#include "gs1/status.h"

namespace gs1
{
class EcologySystem final
{
public:
    [[nodiscard]] static constexpr SiteSystemAccess access() noexcept
    {
        return SiteSystemAccess {
            "EcologySystem",
            site_component_mask_of(
                SiteComponent::TileLayout,
                SiteComponent::TileEcology,
                SiteComponent::TileWeather,
                SiteComponent::Modifier,
                SiteComponent::Counters),
            site_component_mask_of(
                SiteComponent::TileEcology,
                SiteComponent::Counters)};
    }

    [[nodiscard]] static bool subscribes_to(GameCommandType type) noexcept;
    [[nodiscard]] static Gs1Status process_command(
        SiteSystemContext<EcologySystem>& context,
        const GameCommand& command);
    static void run(SiteSystemContext<EcologySystem>& context);
};
}  // namespace gs1
