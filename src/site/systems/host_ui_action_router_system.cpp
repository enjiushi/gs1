#include "site/systems/host_ui_action_router_system.h"

namespace gs1
{
namespace
{
void queue_game_message(RuntimeInvocation& invocation, const GameMessage& message)
{
    invocation.push_game_message(message);
}
}  // namespace

const char* HostUiActionRouterSystem::name() const noexcept
{
    return "HostUiActionRouterSystem";
}

GameMessageSubscriptionSpan HostUiActionRouterSystem::subscribed_game_messages() const noexcept
{
    return {};
}

HostMessageSubscriptionSpan HostUiActionRouterSystem::subscribed_host_messages() const noexcept
{
    static constexpr Gs1HostMessageType subscriptions[] = {GS1_HOST_EVENT_UI_ACTION};
    return subscriptions;
}

std::optional<Gs1RuntimeProfileSystemId> HostUiActionRouterSystem::profile_system_id() const noexcept
{
    return std::nullopt;
}

std::optional<std::uint32_t> HostUiActionRouterSystem::fixed_step_order() const noexcept
{
    return std::nullopt;
}

Gs1Status HostUiActionRouterSystem::process_game_message(
    RuntimeInvocation& invocation,
    const GameMessage& message)
{
    (void)invocation;
    (void)message;
    return GS1_STATUS_OK;
}

Gs1Status HostUiActionRouterSystem::process_host_message(
    RuntimeInvocation& invocation,
    const Gs1HostMessage& message)
{
    if (message.type != GS1_HOST_EVENT_UI_ACTION)
    {
        return GS1_STATUS_OK;
    }

    const auto& action = message.payload.ui_action.action;
    GameMessage gameplay_message {};

    switch (action.type)
    {
    case GS1_UI_ACTION_START_NEW_CAMPAIGN:
        gameplay_message.type = GameMessageType::StartNewCampaign;
        gameplay_message.set_payload(StartNewCampaignMessage {
            action.arg0,
            static_cast<std::uint32_t>(action.arg1)});
        queue_game_message(invocation, gameplay_message);
        return GS1_STATUS_OK;

    case GS1_UI_ACTION_SELECT_DEPLOYMENT_SITE:
        if (action.target_id == 0U)
        {
            return GS1_STATUS_INVALID_ARGUMENT;
        }
        gameplay_message.type = GameMessageType::SelectDeploymentSite;
        gameplay_message.set_payload(SelectDeploymentSiteMessage {action.target_id});
        queue_game_message(invocation, gameplay_message);
        return GS1_STATUS_OK;

    case GS1_UI_ACTION_CLEAR_DEPLOYMENT_SITE_SELECTION:
        gameplay_message.type = GameMessageType::ClearDeploymentSiteSelection;
        gameplay_message.set_payload(ClearDeploymentSiteSelectionMessage {});
        queue_game_message(invocation, gameplay_message);
        return GS1_STATUS_OK;

    case GS1_UI_ACTION_OPEN_REGIONAL_MAP_TECH_TREE:
        gameplay_message.type = GameMessageType::OpenRegionalMapTechTree;
        gameplay_message.set_payload(OpenRegionalMapTechTreeMessage {});
        queue_game_message(invocation, gameplay_message);
        return GS1_STATUS_OK;

    case GS1_UI_ACTION_CLOSE_REGIONAL_MAP_TECH_TREE:
        gameplay_message.type = GameMessageType::CloseRegionalMapTechTree;
        gameplay_message.set_payload(CloseRegionalMapTechTreeMessage {});
        queue_game_message(invocation, gameplay_message);
        return GS1_STATUS_OK;

    case GS1_UI_ACTION_SELECT_TECH_TREE_FACTION_TAB:
        if (action.target_id == 0U)
        {
            return GS1_STATUS_INVALID_ARGUMENT;
        }
        gameplay_message.type = GameMessageType::SelectRegionalMapTechTreeFaction;
        gameplay_message.set_payload(SelectRegionalMapTechTreeFactionMessage {action.target_id});
        queue_game_message(invocation, gameplay_message);
        return GS1_STATUS_OK;

    case GS1_UI_ACTION_START_SITE_ATTEMPT:
        if (action.target_id == 0U)
        {
            return GS1_STATUS_INVALID_ARGUMENT;
        }
        gameplay_message.type = GameMessageType::StartSiteAttempt;
        gameplay_message.set_payload(StartSiteAttemptMessage {action.target_id});
        queue_game_message(invocation, gameplay_message);
        return GS1_STATUS_OK;

    case GS1_UI_ACTION_RETURN_TO_REGIONAL_MAP:
        gameplay_message.type = GameMessageType::ReturnToRegionalMap;
        gameplay_message.set_payload(ReturnToRegionalMapMessage {});
        queue_game_message(invocation, gameplay_message);
        return GS1_STATUS_OK;

    case GS1_UI_ACTION_ACCEPT_TASK:
        if (action.target_id == 0U)
        {
            return GS1_STATUS_INVALID_ARGUMENT;
        }
        gameplay_message.type = GameMessageType::TaskAcceptRequested;
        gameplay_message.set_payload(TaskAcceptRequestedMessage {action.target_id});
        queue_game_message(invocation, gameplay_message);
        return GS1_STATUS_OK;

    case GS1_UI_ACTION_CLAIM_TASK_REWARD:
        if (action.target_id == 0U ||
            action.arg0 > static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max()))
        {
            return GS1_STATUS_INVALID_ARGUMENT;
        }
        gameplay_message.type = GameMessageType::TaskRewardClaimRequested;
        gameplay_message.set_payload(TaskRewardClaimRequestedMessage {
            action.target_id,
            static_cast<std::uint32_t>(action.arg0)});
        queue_game_message(invocation, gameplay_message);
        return GS1_STATUS_OK;

    case GS1_UI_ACTION_BUY_PHONE_LISTING:
        if (action.target_id == 0U)
        {
            return GS1_STATUS_INVALID_ARGUMENT;
        }
        gameplay_message.type = GameMessageType::PhoneListingPurchaseRequested;
        gameplay_message.set_payload(PhoneListingPurchaseRequestedMessage {
            action.target_id,
            static_cast<std::uint16_t>(action.arg0 == 0ULL ? 1ULL : action.arg0),
            0U});
        queue_game_message(invocation, gameplay_message);
        return GS1_STATUS_OK;

    case GS1_UI_ACTION_SELL_PHONE_LISTING:
        if (action.target_id == 0U)
        {
            return GS1_STATUS_INVALID_ARGUMENT;
        }
        gameplay_message.type = GameMessageType::PhoneListingSaleRequested;
        gameplay_message.set_payload(PhoneListingSaleRequestedMessage {
            action.target_id,
            static_cast<std::uint16_t>(action.arg0 == 0ULL ? 1ULL : action.arg0),
            0U});
        queue_game_message(invocation, gameplay_message);
        return GS1_STATUS_OK;

    case GS1_UI_ACTION_SET_PHONE_PANEL_SECTION:
        return GS1_STATUS_OK;

    case GS1_UI_ACTION_CLOSE_PHONE_PANEL:
        return GS1_STATUS_OK;

    case GS1_UI_ACTION_HIRE_CONTRACTOR:
        if (action.target_id == 0U)
        {
            return GS1_STATUS_INVALID_ARGUMENT;
        }
        gameplay_message.type = GameMessageType::ContractorHireRequested;
        gameplay_message.set_payload(ContractorHireRequestedMessage {
            action.target_id,
            static_cast<std::uint32_t>(action.arg0)});
        queue_game_message(invocation, gameplay_message);
        return GS1_STATUS_OK;

    case GS1_UI_ACTION_PURCHASE_SITE_UNLOCKABLE:
        if (action.target_id == 0U)
        {
            return GS1_STATUS_INVALID_ARGUMENT;
        }
        gameplay_message.type = GameMessageType::SiteUnlockablePurchaseRequested;
        gameplay_message.set_payload(SiteUnlockablePurchaseRequestedMessage {action.target_id});
        queue_game_message(invocation, gameplay_message);
        return GS1_STATUS_OK;

    case GS1_UI_ACTION_OPEN_SITE_PROTECTION_SELECTOR:
        return GS1_STATUS_OK;

    case GS1_UI_ACTION_CLOSE_SITE_PROTECTION_UI:
        return GS1_STATUS_OK;

    case GS1_UI_ACTION_SET_SITE_PROTECTION_OVERLAY_MODE:
        return GS1_STATUS_OK;

    case GS1_UI_ACTION_END_SITE_MODIFIER:
        if (action.target_id == 0U)
        {
            return GS1_STATUS_INVALID_ARGUMENT;
        }
        gameplay_message.type = GameMessageType::SiteModifierEndRequested;
        gameplay_message.set_payload(SiteModifierEndRequestedMessage {action.target_id});
        queue_game_message(invocation, gameplay_message);
        return GS1_STATUS_OK;

    case GS1_UI_ACTION_CLAIM_TECHNOLOGY_NODE:
        if (action.target_id == 0U)
        {
            return GS1_STATUS_INVALID_ARGUMENT;
        }
        gameplay_message.type = GameMessageType::TechnologyNodeClaimRequested;
        gameplay_message.set_payload(TechnologyNodeClaimRequestedMessage {
            action.target_id,
            static_cast<std::uint32_t>(action.arg0)});
        queue_game_message(invocation, gameplay_message);
        return GS1_STATUS_OK;

    case GS1_UI_ACTION_REFUND_TECHNOLOGY_NODE:
        if (action.target_id == 0U)
        {
            return GS1_STATUS_INVALID_ARGUMENT;
        }
        gameplay_message.type = GameMessageType::TechnologyNodeRefundRequested;
        gameplay_message.set_payload(TechnologyNodeRefundRequestedMessage {action.target_id});
        queue_game_message(invocation, gameplay_message);
        return GS1_STATUS_OK;

    case GS1_UI_ACTION_NONE:
    default:
        return GS1_STATUS_OK;
    }
}

void HostUiActionRouterSystem::run(RuntimeInvocation& invocation)
{
    (void)invocation;
}
}  // namespace gs1
