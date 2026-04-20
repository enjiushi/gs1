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
                SiteComponent::TileLayout,
                SiteComponent::WorkerMotion,
                SiteComponent::Action,
                SiteComponent::Inventory),
            site_component_mask_of(
                SiteComponent::WorkerMotion)};
    }

    static void run(SiteSystemContext<SiteFlowSystem>& context);
};
}  // namespace gs1
