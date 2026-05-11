#pragma once

#include "runtime/system_interface.h"
#include "site/systems/site_system_context.h"

namespace gs1
{
class SiteCompletionSystem final : public IRuntimeSystem
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
            "SiteCompletionSystem",
            site_component_mask_of(
                SiteComponent::RunMeta,
                SiteComponent::Time,
                SiteComponent::Counters,
                SiteComponent::TileLayout,
                SiteComponent::TileEcology,
                SiteComponent::Objective),
            0U};
    }

    static void run(SiteSystemContext<SiteCompletionSystem>& context);
};
}  // namespace gs1
