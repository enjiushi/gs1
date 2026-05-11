#pragma once

#include "site/systems/site_system_context.h"
#include "gs1/status.h"

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

    [[nodiscard]] static bool subscribes_to_host_message(Gs1HostMessageType type) noexcept;
    [[nodiscard]] static Gs1Status process_host_message(
        SiteSystemContext<SiteFlowSystem>& context,
        const Gs1HostMessage& message);
    static void run(SiteSystemContext<SiteFlowSystem>& context);
};
}  // namespace gs1
