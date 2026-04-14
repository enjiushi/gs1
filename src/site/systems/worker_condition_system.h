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
                SiteComponent::WorkerNeeds),
            site_component_mask_of(SiteComponent::WorkerNeeds)};
    }

    [[nodiscard]] static bool subscribes_to(GameCommandType type) noexcept;
    [[nodiscard]] static Gs1Status process_command(
        SiteSystemContext<WorkerConditionSystem>& context,
        const GameCommand& command);
    static void run(SiteSystemContext<WorkerConditionSystem>& context);
};
}  // namespace gs1
