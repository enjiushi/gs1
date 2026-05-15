#pragma once

#include "messages/game_message.h"
#include "runtime/system_interface.h"
#include "gs1/status.h"

namespace gs1
{
class LoadoutPlannerSystem final : public IRuntimeSystem
{
public:
    static constexpr std::array<StateSetId, 5> k_owned_state_sets {
        StateSetId::CampaignLoadoutPlannerMeta,
        StateSetId::CampaignLoadoutPlannerBaselineItems,
        StateSetId::CampaignLoadoutPlannerAvailableSupportItems,
        StateSetId::CampaignLoadoutPlannerSelectedSlots,
        StateSetId::CampaignLoadoutPlannerNearbyAuraModifiers};
    [[nodiscard]] std::span<const StateSetId> owned_state_sets() const noexcept override
    {
        return k_owned_state_sets;
    }

    [[nodiscard]] const char* name() const noexcept override;
    [[nodiscard]] GameMessageSubscriptionSpan subscribed_game_messages() const noexcept override;
    [[nodiscard]] HostMessageSubscriptionSpan subscribed_host_messages() const noexcept override;
    [[nodiscard]] std::optional<Gs1RuntimeProfileSystemId> profile_system_id() const noexcept override;
    [[nodiscard]] std::optional<std::uint32_t> fixed_step_order() const noexcept override;
    [[nodiscard]] Gs1Status process_game_message(
        RuntimeInvocation& invocation,
        const GameMessage& message) override;
    [[nodiscard]] Gs1Status process_host_message(
        RuntimeInvocation& invocation,
        const Gs1HostMessage& message) override;
    void run(RuntimeInvocation& invocation) override;

    static void initialize_campaign_state(CampaignState& campaign);
};
}  // namespace gs1
