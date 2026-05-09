#include "campaign/systems/campaign_flow_system.h"

#include "app/campaign_factory.h"
#include "app/site_run_factory.h"
#include "campaign/campaign_state.h"
#include "content/prototype_content.h"

namespace gs1
{
namespace
{
[[nodiscard]] bool app_state_supports_technology_tree(Gs1AppState app_state) noexcept
{
    return app_state == GS1_APP_STATE_REGIONAL_MAP ||
        app_state == GS1_APP_STATE_SITE_ACTIVE;
}

[[nodiscard]] bool contains_site_id(
    const std::vector<SiteId>& site_ids,
    SiteId site_id) noexcept
{
    for (const auto candidate : site_ids)
    {
        if (candidate == site_id)
        {
            return true;
        }
    }
    return false;
}

void rebuild_regional_map_caches(CampaignState& campaign)
{
    auto& map = campaign.regional_map_state;
    map.available_site_ids.clear();
    map.completed_site_ids.clear();

    for (const auto& site : campaign.sites)
    {
        if (site.site_state == GS1_SITE_STATE_AVAILABLE)
        {
            map.available_site_ids.push_back(site.site_id);
        }
        else if (site.site_state == GS1_SITE_STATE_COMPLETED)
        {
            map.completed_site_ids.push_back(site.site_id);
        }
    }

    if (map.selected_site_id.has_value())
    {
        bool selected_site_still_valid = false;
        for (const auto available_site_id : map.available_site_ids)
        {
            if (available_site_id == map.selected_site_id.value())
            {
                selected_site_still_valid = true;
                break;
            }
        }

        if (!selected_site_still_valid)
        {
            map.selected_site_id.reset();
        }
    }
}

SiteMetaState* find_site_mut(CampaignState& campaign, std::uint32_t site_id) noexcept
{
    for (auto& site : campaign.sites)
    {
        if (site.site_id.value == site_id)
        {
            return &site;
        }
    }

    return nullptr;
}
}  // namespace

bool CampaignFlowSystem::subscribes_to(GameMessageType type) noexcept
{
    switch (type)
    {
    case GameMessageType::OpenMainMenu:
    case GameMessageType::StartNewCampaign:
    case GameMessageType::SelectDeploymentSite:
    case GameMessageType::ClearDeploymentSiteSelection:
    case GameMessageType::OpenRegionalMapTechTree:
    case GameMessageType::CloseRegionalMapTechTree:
    case GameMessageType::SelectRegionalMapTechTreeFaction:
    case GameMessageType::StartSiteAttempt:
    case GameMessageType::ReturnToRegionalMap:
    case GameMessageType::SiteAttemptEnded:
        return true;

    case GameMessageType::DeploymentSiteSelectionChanged:
    case GameMessageType::CampaignReputationAwardRequested:
    case GameMessageType::FactionReputationAwardRequested:
    case GameMessageType::TechnologyNodeClaimRequested:
    case GameMessageType::TechnologyNodeRefundRequested:
    case GameMessageType::PresentLog:
    default:
        return false;
    }
}

Gs1Status CampaignFlowSystem::process_message(
    CampaignFlowMessageContext& context,
    const GameMessage& message)
{
    if (!subscribes_to(message.type))
    {
        return GS1_STATUS_OK;
    }

    switch (message.type)
    {
    case GameMessageType::OpenMainMenu:
        context.app_state = GS1_APP_STATE_MAIN_MENU;
        return GS1_STATUS_OK;

    case GameMessageType::StartNewCampaign:
    {
        const auto& payload = message.payload_as<StartNewCampaignMessage>();
        context.campaign = CampaignFactory::create_prototype_campaign(payload.campaign_seed, payload.campaign_days);
        context.active_site_run.reset();
        context.app_state = GS1_APP_STATE_REGIONAL_MAP;
        rebuild_regional_map_caches(*context.campaign);
        context.campaign->app_state = context.app_state;
        return GS1_STATUS_OK;
    }

    case GameMessageType::SelectDeploymentSite:
    {
        if (!context.campaign.has_value())
        {
            return GS1_STATUS_INVALID_STATE;
        }

        const auto& payload = message.payload_as<SelectDeploymentSiteMessage>();
        auto* site = find_site_mut(*context.campaign, payload.site_id);
        if (site == nullptr)
        {
            return GS1_STATUS_NOT_FOUND;
        }

        if (site->site_state != GS1_SITE_STATE_AVAILABLE)
        {
            return GS1_STATUS_INVALID_STATE;
        }

        auto& selection = context.campaign->regional_map_state.selected_site_id;
        if (selection.has_value() && selection->value == payload.site_id)
        {
            return GS1_STATUS_OK;
        }

        selection = SiteId {payload.site_id};

        GameMessage selection_changed {};
        selection_changed.type = GameMessageType::DeploymentSiteSelectionChanged;
        selection_changed.set_payload(DeploymentSiteSelectionChangedMessage {payload.site_id});
        context.message_queue.push_back(selection_changed);
        return GS1_STATUS_OK;
    }

    case GameMessageType::ClearDeploymentSiteSelection:
    {
        if (!context.campaign.has_value())
        {
            return GS1_STATUS_INVALID_STATE;
        }

        auto& selection = context.campaign->regional_map_state.selected_site_id;
        if (!selection.has_value())
        {
            return GS1_STATUS_OK;
        }

        selection.reset();

        GameMessage selection_changed {};
        selection_changed.type = GameMessageType::DeploymentSiteSelectionChanged;
        selection_changed.set_payload(DeploymentSiteSelectionChangedMessage {0U});
        context.message_queue.push_back(selection_changed);
        return GS1_STATUS_OK;
    }

    case GameMessageType::OpenRegionalMapTechTree:
    {
        if (!context.campaign.has_value() || !app_state_supports_technology_tree(context.app_state))
        {
            return GS1_STATUS_INVALID_STATE;
        }

        context.campaign->regional_map_state.tech_tree_open = true;
        return GS1_STATUS_OK;
    }

    case GameMessageType::CloseRegionalMapTechTree:
    {
        if (!context.campaign.has_value())
        {
            return GS1_STATUS_INVALID_STATE;
        }

        context.campaign->regional_map_state.tech_tree_open = false;
        return GS1_STATUS_OK;
    }

    case GameMessageType::SelectRegionalMapTechTreeFaction:
    {
        if (!context.campaign.has_value() || !app_state_supports_technology_tree(context.app_state))
        {
            return GS1_STATUS_INVALID_STATE;
        }

        const auto& payload = message.payload_as<SelectRegionalMapTechTreeFactionMessage>();
        context.campaign->regional_map_state.selected_tech_tree_faction_id = FactionId {payload.faction_id};
        return GS1_STATUS_OK;
    }

    case GameMessageType::StartSiteAttempt:
    {
        if (!context.campaign.has_value())
        {
            return GS1_STATUS_INVALID_STATE;
        }

        const auto& payload = message.payload_as<StartSiteAttemptMessage>();
        auto* site = find_site_mut(*context.campaign, payload.site_id);
        if (site == nullptr)
        {
            return GS1_STATUS_NOT_FOUND;
        }

        if (site->site_state != GS1_SITE_STATE_AVAILABLE)
        {
            return GS1_STATUS_INVALID_STATE;
        }

        site->attempt_count += 1U;
        context.campaign->regional_map_state.tech_tree_open = false;
        context.active_site_run = SiteRunFactory::create_site_run(*context.campaign, *site);
        context.campaign->active_site_id = SiteId {payload.site_id};
        context.app_state = GS1_APP_STATE_SITE_ACTIVE;
        context.campaign->app_state = context.app_state;

        GameMessage site_run_started {};
        site_run_started.type = GameMessageType::SiteRunStarted;
        site_run_started.set_payload(SiteRunStartedMessage {
            context.active_site_run->site_id.value,
            context.active_site_run->site_run_id.value,
            context.active_site_run->site_archetype_id,
            context.active_site_run->attempt_index,
            context.active_site_run->site_attempt_seed});
        context.message_queue.push_back(site_run_started);
        return GS1_STATUS_OK;
    }

    case GameMessageType::ReturnToRegionalMap:
    {
        if (!context.campaign.has_value())
        {
            return GS1_STATUS_INVALID_STATE;
        }

        context.active_site_run.reset();
        context.campaign->active_site_id.reset();
        context.campaign->regional_map_state.tech_tree_open = false;
        context.app_state = GS1_APP_STATE_REGIONAL_MAP;
        context.campaign->app_state = context.app_state;
        rebuild_regional_map_caches(*context.campaign);
        return GS1_STATUS_OK;
    }

    case GameMessageType::SiteAttemptEnded:
    {
        if (!context.campaign.has_value() || !context.active_site_run.has_value())
        {
            return GS1_STATUS_INVALID_STATE;
        }

        const auto& payload = message.payload_as<SiteAttemptEndedMessage>();
        auto* site = find_site_mut(*context.campaign, payload.site_id);
        if (site == nullptr)
        {
            return GS1_STATUS_NOT_FOUND;
        }

        context.active_site_run->run_status =
            payload.result == GS1_SITE_ATTEMPT_RESULT_COMPLETED
                ? SiteRunStatus::Completed
                : SiteRunStatus::Failed;
        context.active_site_run->result_newly_revealed_site_count = 0U;

        if (payload.result == GS1_SITE_ATTEMPT_RESULT_COMPLETED)
        {
            site->site_state = GS1_SITE_STATE_COMPLETED;

            for (const auto adjacent_site_id : site->adjacent_site_ids)
            {
                auto* adjacent_site = find_site_mut(*context.campaign, adjacent_site_id.value);
                if (adjacent_site != nullptr && adjacent_site->site_state == GS1_SITE_STATE_LOCKED)
                {
                    adjacent_site->site_state = GS1_SITE_STATE_AVAILABLE;
                    if (!contains_site_id(context.campaign->regional_map_state.revealed_site_ids, adjacent_site_id))
                    {
                        context.campaign->regional_map_state.revealed_site_ids.push_back(adjacent_site_id);
                        context.active_site_run->result_newly_revealed_site_count += 1U;
                    }
                }
            }

            if (site->completion_reputation_reward > 0)
            {
                GameMessage reputation_award {};
                reputation_award.type = GameMessageType::CampaignReputationAwardRequested;
                reputation_award.set_payload(CampaignReputationAwardRequestedMessage {
                    site->completion_reputation_reward});
                context.message_queue.push_back(reputation_award);
            }

            if (site->featured_faction_id.value != 0U &&
                site->completion_faction_reputation_reward > 0)
            {
                GameMessage faction_award {};
                faction_award.type = GameMessageType::FactionReputationAwardRequested;
                faction_award.set_payload(FactionReputationAwardRequestedMessage {
                    site->featured_faction_id.value,
                    site->completion_faction_reputation_reward});
                context.message_queue.push_back(faction_award);
            }
        }

        context.campaign->regional_map_state.tech_tree_open = false;
        context.app_state = GS1_APP_STATE_SITE_RESULT;
        context.campaign->app_state = context.app_state;
        rebuild_regional_map_caches(*context.campaign);
        return GS1_STATUS_OK;
    }

    case GameMessageType::DeploymentSiteSelectionChanged:
    case GameMessageType::CampaignReputationAwardRequested:
    case GameMessageType::FactionReputationAwardRequested:
    case GameMessageType::TechnologyNodeClaimRequested:
    case GameMessageType::TechnologyNodeRefundRequested:
    case GameMessageType::PresentLog:
    default:
        return GS1_STATUS_OK;
    }
}

}  // namespace gs1
