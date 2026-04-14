#pragma once

#include "site/systems/site_system_context.h"
#include "gs1/status.h"

namespace gs1
{
class ModifierSystem final
{
public:
    [[nodiscard]] static constexpr SiteSystemAccess access() noexcept
    {
        return SiteSystemAccess {
            "ModifierSystem",
            site_component_mask_of(
                SiteComponent::Camp,
                SiteComponent::Modifier),
            site_component_mask_of(SiteComponent::Modifier)};
    }

    [[nodiscard]] static bool subscribes_to(GameCommandType type) noexcept;
    [[nodiscard]] static Gs1Status process_command(
        SiteSystemContext<ModifierSystem>& context,
        const GameCommand& command);
    static void run(SiteSystemContext<ModifierSystem>& context);
};
}  // namespace gs1
