#pragma once

#include "site/systems/site_system_context.h"
#include "gs1/status.h"

namespace gs1
{
class WeatherEventSystem final
{
public:
    [[nodiscard]] static constexpr SiteSystemAccess access() noexcept
    {
        return SiteSystemAccess {
            "WeatherEventSystem",
            site_component_mask_of(
                SiteComponent::RunMeta,
                SiteComponent::Objective,
                SiteComponent::Weather,
                SiteComponent::Event),
            site_component_mask_of(
                SiteComponent::Weather,
                SiteComponent::Event)};
    }

    [[nodiscard]] static bool subscribes_to(GameMessageType type) noexcept;
    [[nodiscard]] static Gs1Status process_message(
        SiteSystemContext<WeatherEventSystem>& context,
        const GameMessage& message);
    static void run(SiteSystemContext<WeatherEventSystem>& context);
};
}  // namespace gs1
