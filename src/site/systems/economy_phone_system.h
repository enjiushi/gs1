#pragma once

#include "site/systems/site_system_context.h"
#include "gs1/status.h"

namespace gs1
{
class EconomyPhoneSystem final
{
public:
    [[nodiscard]] static constexpr SiteSystemAccess access() noexcept
    {
        return SiteSystemAccess {
            "EconomyPhoneSystem",
            site_component_mask_of(
                SiteComponent::Inventory,
                SiteComponent::Craft,
                SiteComponent::Economy),
            site_component_mask_of(SiteComponent::Economy)};
    }

    [[nodiscard]] static bool subscribes_to(GameCommandType type) noexcept;
    [[nodiscard]] static Gs1Status process_command(
        SiteSystemContext<EconomyPhoneSystem>& context,
        const GameCommand& command);
    static void run(SiteSystemContext<EconomyPhoneSystem>& context);
};
}  // namespace gs1
