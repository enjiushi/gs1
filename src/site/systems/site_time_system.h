#pragma once

#include "site/systems/site_system_context.h"

namespace gs1
{
class SiteTimeSystem final
{
public:
    [[nodiscard]] static constexpr SiteSystemAccess access() noexcept
    {
        return SiteSystemAccess {
            "SiteTimeSystem",
            site_component_mask_of(SiteComponent::Time),
            site_component_mask_of(SiteComponent::Time)};
    }

    static void run(SiteSystemContext<SiteTimeSystem>& context);
};
}  // namespace gs1
