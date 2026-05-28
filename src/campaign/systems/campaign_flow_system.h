#pragma once

#include "messages/game_message.h"
#include "runtime/system_interface.h"
#include "site/site_run_state.h"
#include "gs1/status.h"
#include "gs1/types.h"

#include <optional>

namespace gs1
{
class CampaignFlowSystem final : public IRuntimeSystem
{
public:
    using subscribed_messages = type_list<
        OpenMainMenuMessage,
        StartNewCampaignMessage,
        SelectDeploymentSiteMessage,
        ClearDeploymentSiteSelectionMessage,
        StartSiteAttemptMessage,
        ReturnToRegionalMapMessage,
        SiteAttemptEndedMessage>;

    static constexpr std::array<StateSetId, 11> k_owned_state_sets {
        StateSetId::AppState,
        StateSetId::CampaignCore,
        StateSetId::CampaignRegionalMapMeta,
        StateSetId::CampaignRegionalMapRevealedSites,
        StateSetId::CampaignRegionalMapAvailableSites,
        StateSetId::CampaignRegionalMapCompletedSites,
        StateSetId::CampaignSiteMetaEntries,
        StateSetId::CampaignSiteAdjacentIds,
        StateSetId::CampaignSiteExportedSupportItems,
        StateSetId::CampaignSiteNearbyAuraModifierIds,
        StateSetId::SiteRunMeta};
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
    [[nodiscard]] Gs1Status handle(
        RuntimeInvocation& invocation,
        const OpenMainMenuMessage& message);
    [[nodiscard]] Gs1Status handle(
        RuntimeInvocation& invocation,
        const StartNewCampaignMessage& message);
    [[nodiscard]] Gs1Status handle(
        RuntimeInvocation& invocation,
        const SelectDeploymentSiteMessage& message);
    [[nodiscard]] Gs1Status handle(
        RuntimeInvocation& invocation,
        const ClearDeploymentSiteSelectionMessage& message);
    [[nodiscard]] Gs1Status handle(
        RuntimeInvocation& invocation,
        const StartSiteAttemptMessage& message);
    [[nodiscard]] Gs1Status handle(
        RuntimeInvocation& invocation,
        const ReturnToRegionalMapMessage& message);
    [[nodiscard]] Gs1Status handle(
        RuntimeInvocation& invocation,
        const SiteAttemptEndedMessage& message);
    void run(RuntimeInvocation& invocation) override;
};

template <>
struct system_state_tags<CampaignFlowSystem>
{
    using type = type_list<RuntimeAppStateTag, RuntimeCampaignFactionProgressTag>;
};
}  // namespace gs1
