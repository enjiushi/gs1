#include "app/campaign_presentation_ops.h"

#include "app/game_presentation_coordinator.h"

namespace gs1
{
void campaign_presentation_handle_message(
    GamePresentationCoordinator& owner,
    GamePresentationRuntimeContext& context,
    const GameMessage& message)
{
    owner.active_context_ = &context;

    switch (message.type)
    {
    case GameMessageType::OpenMainMenu:
        owner.queue_close_active_normal_ui_if_open();
        owner.queue_close_ui_setup_if_open(GS1_UI_SETUP_REGIONAL_MAP_TECH_TREE);
        owner.queue_close_progression_view_if_open(GS1_PROGRESSION_VIEW_REGIONAL_MAP_TECH_TREE);
        owner.queue_clear_ui_panel_messages(GS1_UI_PANEL_REGIONAL_MAP);
        owner.queue_clear_ui_panel_messages(GS1_UI_PANEL_REGIONAL_MAP_SELECTION);
        owner.queue_app_state_message(owner.app_state());
        owner.queue_main_menu_ui_messages();
        owner.queue_log_message("Entered main menu.");
        break;

    case GameMessageType::StartNewCampaign:
        owner.queue_clear_ui_panel_messages(GS1_UI_PANEL_MAIN_MENU);
        owner.queue_app_state_message(owner.app_state());
        owner.queue_campaign_resources_message();
        owner.queue_regional_map_snapshot_messages();
        owner.queue_regional_map_menu_ui_messages();
        owner.queue_regional_map_selection_ui_messages();
        owner.queue_log_message("Started new GS1 campaign.");
        break;

    case GameMessageType::SelectDeploymentSite:
        owner.queue_regional_map_snapshot_messages();
        owner.queue_regional_map_menu_ui_messages();
        owner.queue_regional_map_selection_ui_messages();
        owner.queue_log_message("Selected deployment site.");
        break;

    case GameMessageType::ClearDeploymentSiteSelection:
        owner.queue_regional_map_snapshot_messages();
        owner.queue_regional_map_menu_ui_messages();
        owner.queue_clear_ui_panel_messages(GS1_UI_PANEL_REGIONAL_MAP_SELECTION);
        owner.queue_log_message("Cleared deployment site selection.");
        break;

    case GameMessageType::OpenRegionalMapTechTree:
        owner.queue_regional_map_menu_ui_messages();
        owner.queue_regional_map_tech_tree_ui_messages();
        break;

    case GameMessageType::CloseRegionalMapTechTree:
        owner.queue_regional_map_menu_ui_messages();
        owner.queue_close_ui_setup_if_open(GS1_UI_SETUP_REGIONAL_MAP_TECH_TREE);
        owner.queue_close_progression_view_if_open(GS1_PROGRESSION_VIEW_REGIONAL_MAP_TECH_TREE);
        break;

    case GameMessageType::SelectRegionalMapTechTreeFaction:
        owner.queue_regional_map_menu_ui_messages();
        owner.queue_regional_map_tech_tree_ui_messages();
        break;

    case GameMessageType::CampaignCashDeltaRequested:
    case GameMessageType::CampaignReputationAwardRequested:
    case GameMessageType::FactionReputationAwardRequested:
    case GameMessageType::TechnologyNodeClaimRequested:
    case GameMessageType::TechnologyNodeRefundRequested:
    case GameMessageType::EconomyMoneyAwardRequested:
    case GameMessageType::PhoneCartCheckoutRequested:
        owner.queue_campaign_resources_message();
        if (owner.app_state() == GS1_APP_STATE_REGIONAL_MAP ||
            owner.app_state() == GS1_APP_STATE_SITE_ACTIVE)
        {
            owner.queue_regional_map_menu_ui_messages();
            owner.queue_regional_map_tech_tree_ui_messages();
        }
        if (message.type == GameMessageType::CampaignReputationAwardRequested ||
            message.type == GameMessageType::FactionReputationAwardRequested ||
            message.type == GameMessageType::TechnologyNodeClaimRequested ||
            message.type == GameMessageType::TechnologyNodeRefundRequested)
        {
            owner.sync_campaign_unlock_presentations();
        }
        break;

    case GameMessageType::StartSiteAttempt:
        owner.queue_clear_ui_panel_messages(GS1_UI_PANEL_REGIONAL_MAP_SELECTION);
        owner.queue_clear_ui_panel_messages(GS1_UI_PANEL_REGIONAL_MAP);
        owner.queue_close_ui_setup_if_open(GS1_UI_SETUP_REGIONAL_MAP_TECH_TREE);
        owner.queue_close_progression_view_if_open(GS1_PROGRESSION_VIEW_REGIONAL_MAP_TECH_TREE);
        owner.queue_app_state_message(owner.app_state());
        break;

    case GameMessageType::SiteSceneActivated:
        owner.queue_app_state_message(owner.app_state());
        owner.queue_site_ready_bootstrap_messages();
        break;

    case GameMessageType::ReturnToRegionalMap:
        owner.queue_app_state_message(owner.app_state());
        owner.queue_campaign_resources_message();
        owner.queue_close_ui_setup_if_open(GS1_UI_SETUP_SITE_RESULT);
        owner.queue_regional_map_snapshot_messages();
        owner.queue_regional_map_menu_ui_messages();
        owner.queue_regional_map_selection_ui_messages();
        break;

    case GameMessageType::SiteAttemptEnded:
    {
        const auto& payload = message.payload_as<SiteAttemptEndedMessage>();
        const auto newly_revealed_site_count =
            owner.active_site_run().has_value() ? owner.active_site_run()->result_newly_revealed_site_count : 0U;
        owner.queue_close_ui_setup_if_open(GS1_UI_SETUP_REGIONAL_MAP_TECH_TREE);
        owner.queue_close_progression_view_if_open(GS1_PROGRESSION_VIEW_REGIONAL_MAP_TECH_TREE);
        owner.queue_app_state_message(owner.app_state());
        owner.queue_site_result_ui_messages(payload.site_id, payload.result);
        owner.queue_site_result_ready_message(
            payload.site_id,
            payload.result,
            newly_revealed_site_count);
        break;
    }

    case GameMessageType::PresentLog:
    {
        const auto& payload = message.payload_as<PresentLogMessage>();
        owner.queue_log_message(payload.text, payload.level);
        break;
    }

    case GameMessageType::PhoneListingPurchaseRequested:
    case GameMessageType::PhoneListingSaleRequested:
    case GameMessageType::ContractorHireRequested:
    case GameMessageType::SiteUnlockablePurchaseRequested:
        owner.queue_campaign_resources_message();
        break;

    default:
        break;
    }
}
}  // namespace gs1
