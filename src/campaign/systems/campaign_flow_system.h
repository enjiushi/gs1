#pragma once

#include "campaign/systems/campaign_system_context.h"
#include "messages/game_message.h"
#include "runtime/system_interface.h"
#include "site/site_run_state.h"
#include "gs1/status.h"
#include "gs1/types.h"

#include <optional>

namespace gs1
{
struct CampaignFlowMessageContext final
{
    std::optional<CampaignState>& campaign;
    std::optional<SiteRunState>& active_site_run;
    Gs1AppState& app_state;
    GameMessageQueue& message_queue;
};

class CampaignFlowSystem final : public IRuntimeSystem
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

    [[nodiscard]] static bool subscribes_to_host_message(Gs1HostMessageType type) noexcept;
    [[nodiscard]] static Gs1Status process_host_message(
        CampaignFlowMessageContext& context,
        const Gs1HostMessage& message);
    [[nodiscard]] static bool subscribes_to(GameMessageType type) noexcept;
    [[nodiscard]] static Gs1Status process_message(
        CampaignFlowMessageContext& context,
        const GameMessage& message);
};
}  // namespace gs1
