#pragma once

#include "site/systems/site_system_context.h"
#include "gs1/status.h"

namespace gs1
{
class WorkerConditionSystem final
{
public:
    [[nodiscard]] static constexpr SiteSystemAccess access() noexcept
    {
        return SiteSystemAccess {
            "WorkerConditionSystem",
            site_component_mask_of(
                SiteComponent::RunMeta,
                SiteComponent::TileWeather,
                SiteComponent::WorkerMotion,
                SiteComponent::WorkerNeeds,
                SiteComponent::Action),
            site_component_mask_of(SiteComponent::WorkerNeeds)};
    }

    [[nodiscard]] static bool subscribes_to(GameMessageType type) noexcept;
    [[nodiscard]] static Gs1Status process_message(
        SiteSystemContext<WorkerConditionSystem>& context,
        const GameMessage& message);
    static void run(SiteSystemContext<WorkerConditionSystem>& context);
};
}  // namespace gs1
