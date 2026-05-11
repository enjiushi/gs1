#pragma once

#include "campaign/systems/campaign_system_context.h"
#include "messages/game_message.h"
#include "runtime/system_interface.h"
#include "gs1/status.h"

namespace gs1
{
class FactionReputationSystem final : public IRuntimeSystem
{
public:
    [[nodiscard]] const char* name() const noexcept override;
    void register_game_message_subscriptions(RuntimeSystemRegistration& registration) override;
    void register_host_message_subscriptions(RuntimeSystemRegistration& registration) override;
    void register_feedback_event_subscriptions(RuntimeSystemRegistration& registration) override;
    [[nodiscard]] std::optional<Gs1RuntimeProfileSystemId> profile_system_id() const noexcept override;
    [[nodiscard]] std::optional<std::uint32_t> fixed_step_order() const noexcept override;
    [[nodiscard]] Gs1Status process_game_message(
        GameRuntimeTempBridge& bridge,
        const GameMessage& message) override;
    [[nodiscard]] Gs1Status process_host_message(
        GameRuntimeTempBridge& bridge,
        const Gs1HostMessage& message) override;
    void run(GameRuntimeTempBridge& bridge) override;

    [[nodiscard]] static bool subscribes_to(GameMessageType type) noexcept;
    [[nodiscard]] static Gs1Status process_message(
        CampaignSystemContext& context,
        const GameMessage& message);
};
}  // namespace gs1
