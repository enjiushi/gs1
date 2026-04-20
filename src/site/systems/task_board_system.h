#pragma once

#include "site/systems/site_system_context.h"
#include "gs1/status.h"

namespace gs1
{
class TaskBoardSystem final
{
public:
    [[nodiscard]] static constexpr SiteSystemAccess access() noexcept
    {
        return SiteSystemAccess {
            "TaskBoardSystem",
            site_component_mask_of(
                SiteComponent::TaskBoard,
                SiteComponent::Counters,
                SiteComponent::Objective,
                SiteComponent::Economy,
                SiteComponent::Modifier),
            site_component_mask_of(SiteComponent::TaskBoard)};
    }

    [[nodiscard]] static bool subscribes_to(GameMessageType type) noexcept;
    [[nodiscard]] static Gs1Status process_message(
        SiteSystemContext<TaskBoardSystem>& context,
        const GameMessage& message);
    static void run(SiteSystemContext<TaskBoardSystem>& context);
};
}  // namespace gs1
