#pragma once

#include "site/systems/site_system_context.h"
#include "gs1/status.h"

namespace gs1
{
class LocalWeatherResolveSystem final
{
public:
    [[nodiscard]] static constexpr SiteSystemAccess access() noexcept
    {
        return SiteSystemAccess {
            "LocalWeatherResolveSystem",
            site_component_mask_of(
                SiteComponent::TileLayout,
                SiteComponent::TileEcology,
                SiteComponent::TileWeather,
                SiteComponent::Weather,
                SiteComponent::Modifier,
                SiteComponent::LocalWeatherRuntime),
            site_component_mask_of(
                SiteComponent::TileWeather,
                SiteComponent::LocalWeatherRuntime)};
    }

    [[nodiscard]] static bool subscribes_to(GameCommandType type) noexcept;
    [[nodiscard]] static Gs1Status process_command(
        SiteSystemContext<LocalWeatherResolveSystem>& context,
        const GameCommand& command);
    static void run(SiteSystemContext<LocalWeatherResolveSystem>& context);
};
}  // namespace gs1
