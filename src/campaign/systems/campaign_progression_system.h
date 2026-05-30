#pragma once

#include "messages/game_message.h"
#include "runtime/system_interface.h"
#include "gs1/status.h"

namespace gs1
{
class CampaignProgressionSystem final : public IRuntimeSystem
{
public:
    using subscribed_messages = type_list<
        ProgressionEventOccurredMessage,
        PurchaseEntrySelectedMessage,
        CampaignReputationAwardRequestedMessage,
        FactionReputationAwardRequestedMessage>;
    using emitted_runtime_messages = type_list<>;
    static constexpr std::optional<Gs1RuntimeProfileSystemId> profile_id =
        GS1_RUNTIME_PROFILE_SYSTEM_FACTION_REPUTATION;
    static constexpr std::optional<std::uint32_t> fixed_step_order_value = std::nullopt;

    static constexpr std::array<StateSetId, 2> k_owned_state_sets {
        StateSetId::CampaignFactionProgress,
        StateSetId::CampaignProgression};
    [[nodiscard]] std::span<const StateSetId> owned_state_sets() const noexcept override
    {
        return k_owned_state_sets;
    }

    [[nodiscard]] const char* name() const noexcept override;
    [[nodiscard]] HostMessageSubscriptionSpan subscribed_host_messages() const noexcept override;
    [[nodiscard]] std::optional<Gs1RuntimeProfileSystemId> profile_system_id() const noexcept override;
    [[nodiscard]] std::optional<std::uint32_t> fixed_step_order() const noexcept override;
    [[nodiscard]] Gs1Status handle(
        RuntimeInvocation& invocation,
        const ProgressionEventOccurredMessage& message);
    [[nodiscard]] Gs1Status handle(
        RuntimeInvocation& invocation,
        const PurchaseEntrySelectedMessage& message);
    [[nodiscard]] Gs1Status handle(
        RuntimeInvocation& invocation,
        const CampaignReputationAwardRequestedMessage& message);
    [[nodiscard]] Gs1Status handle(
        RuntimeInvocation& invocation,
        const FactionReputationAwardRequestedMessage& message);
    void run(RuntimeInvocation& invocation) override;
};
}  // namespace gs1
