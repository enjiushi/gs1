#pragma once

#include "site/systems/site_system_context.h"

namespace gs1
{
class SiteFlowSystem final
{
public:
    [[nodiscard]] static constexpr SiteSystemAccess access() noexcept
    {
        return SiteSystemAccess {
            "SiteFlowSystem",
            site_component_mask_of(
                SiteComponent::Time,
                SiteComponent::TileLayout,
                SiteComponent::WorkerMotion),
            site_component_mask_of(
                SiteComponent::Time,
                SiteComponent::WorkerMotion)};
    }

    static void run(SiteSystemContext<SiteFlowSystem>& context);
};
}  // namespace gs1
