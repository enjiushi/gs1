#pragma once

#include "site/systems/site_system_context.h"
#include "gs1/status.h"

namespace gs1
{
class PhonePanelSystem final
{
public:
    [[nodiscard]] static constexpr SiteSystemAccess access() noexcept
    {
        return SiteSystemAccess {
            "PhonePanelSystem",
            site_component_mask_of(
                SiteComponent::Inventory,
                SiteComponent::TaskBoard,
                SiteComponent::Economy,
                SiteComponent::PhonePanel,
                SiteComponent::Craft,
                SiteComponent::Action),
            site_component_mask_of(SiteComponent::PhonePanel)};
    }

    [[nodiscard]] static bool subscribes_to_host_message(Gs1HostMessageType type) noexcept;
    [[nodiscard]] static Gs1Status process_host_message(
        SiteSystemContext<PhonePanelSystem>& context,
        const Gs1HostMessage& message);
    [[nodiscard]] static bool subscribes_to(GameMessageType type) noexcept;
    [[nodiscard]] static Gs1Status process_message(
        SiteSystemContext<PhonePanelSystem>& context,
        const GameMessage& message);
    static void run(SiteSystemContext<PhonePanelSystem>& context);
};
}  // namespace gs1
