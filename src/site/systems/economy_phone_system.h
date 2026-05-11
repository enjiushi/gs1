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
                SiteComponent::Action,
                SiteComponent::TaskBoard,
                SiteComponent::Economy),
            site_component_mask_of(SiteComponent::Economy)};
    }

    [[nodiscard]] static bool subscribes_to_host_message(Gs1HostMessageType type) noexcept;
    [[nodiscard]] static Gs1Status process_host_message(
        SiteSystemContext<EconomyPhoneSystem>& context,
        const Gs1HostMessage& message);
    [[nodiscard]] static bool subscribes_to(GameMessageType type) noexcept;
    [[nodiscard]] static Gs1Status process_message(
        SiteSystemContext<EconomyPhoneSystem>& context,
        const GameMessage& message);
    static void run(SiteSystemContext<EconomyPhoneSystem>& context);
};
}  // namespace gs1
