#pragma once

#include "site/systems/site_system_context.h"
#include "gs1/status.h"

namespace gs1
{
class CampDurabilitySystem final
{
public:
    [[nodiscard]] static constexpr SiteSystemAccess access() noexcept
    {
        return SiteSystemAccess {
            "CampDurabilitySystem",
            site_component_mask_of(
                SiteComponent::RunMeta,
                SiteComponent::Camp,
                SiteComponent::Weather,
                SiteComponent::Event),
            site_component_mask_of(SiteComponent::Camp)};
    }

    [[nodiscard]] static bool subscribes_to(GameCommandType type) noexcept;
    [[nodiscard]] static Gs1Status process_command(
        SiteSystemContext<CampDurabilitySystem>& context,
        const GameCommand& command);
    static void run(SiteSystemContext<CampDurabilitySystem>& context);
};
}  // namespace gs1
