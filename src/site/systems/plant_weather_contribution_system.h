#pragma once

#include "gs1/status.h"
#include "site/systems/site_system_context.h"

namespace gs1
{
class PlantWeatherContributionSystem final
{
public:
    [[nodiscard]] static constexpr SiteSystemAccess access() noexcept
    {
        return SiteSystemAccess {
            "PlantWeatherContributionSystem",
            site_component_mask_of(
                SiteComponent::TileLayout,
                SiteComponent::TileEcology,
                SiteComponent::TilePlantWeatherContribution,
                SiteComponent::Weather),
            site_component_mask_of(SiteComponent::TilePlantWeatherContribution)};
    }

    [[nodiscard]] static bool subscribes_to(GameMessageType type) noexcept;
    [[nodiscard]] static Gs1Status process_message(
        SiteSystemContext<PlantWeatherContributionSystem>& context,
        const GameMessage& message);
    static void run(SiteSystemContext<PlantWeatherContributionSystem>& context);
};
}  // namespace gs1
