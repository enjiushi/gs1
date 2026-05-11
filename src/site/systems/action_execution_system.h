#pragma once

#include "runtime/system_interface.h"
#include "site/systems/site_system_context.h"
#include "gs1/status.h"

namespace gs1
{
class ActionExecutionSystem final : public IRuntimeSystem
{
public:
    [[nodiscard]] const char* name() const noexcept override;
    [[nodiscard]] GameMessageSubscriptionSpan subscribed_game_messages() const noexcept override;
    [[nodiscard]] HostMessageSubscriptionSpan subscribed_host_messages() const noexcept override;
    [[nodiscard]] FeedbackEventSubscriptionSpan subscribed_feedback_events() const noexcept override;
    [[nodiscard]] std::optional<Gs1RuntimeProfileSystemId> profile_system_id() const noexcept override;
    [[nodiscard]] std::optional<std::uint32_t> fixed_step_order() const noexcept override;
    [[nodiscard]] Gs1Status process_game_message(
        GameRuntimeTempBridge& bridge,
        const GameMessage& message) override;
    [[nodiscard]] Gs1Status process_host_message(
        GameRuntimeTempBridge& bridge,
        const Gs1HostMessage& message) override;
    void run(GameRuntimeTempBridge& bridge) override;

    [[nodiscard]] static constexpr SiteSystemAccess access() noexcept
    {
        return SiteSystemAccess {
            "ActionExecutionSystem",
            site_component_mask_of(
                SiteComponent::Time,
                SiteComponent::TileLayout,
                SiteComponent::TileExcavation,
                SiteComponent::TileWeather,
                SiteComponent::DeviceCondition,
                SiteComponent::WorkerMotion,
                SiteComponent::WorkerNeeds,
                SiteComponent::Inventory,
                SiteComponent::Modifier,
                SiteComponent::Craft,
                SiteComponent::Action),
            site_component_mask_of(
                SiteComponent::TileExcavation,
                SiteComponent::Action)};
    }

    [[nodiscard]] static bool subscribes_to_host_message(Gs1HostMessageType type) noexcept;
    [[nodiscard]] static Gs1Status process_host_message(
        SiteSystemContext<ActionExecutionSystem>& context,
        const Gs1HostMessage& message);
    [[nodiscard]] static bool subscribes_to(GameMessageType type) noexcept;
    [[nodiscard]] static Gs1Status process_message(
        SiteSystemContext<ActionExecutionSystem>& context,
        const GameMessage& message);
    static void run(SiteSystemContext<ActionExecutionSystem>& context);
};
}  // namespace gs1
