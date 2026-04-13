#include "campaign/systems/campaign_flow_system.h"

#include "app/campaign_factory.h"
#include "app/site_run_factory.h"
#include "campaign/campaign_state.h"

namespace gs1
{
namespace
{
constexpr double k_site_fixed_step_minutes = 0.2;
constexpr double k_minutes_per_day = 1440.0;

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

bool CampaignFlowSystem::subscribes_to(GameCommandType type) noexcept
{
    switch (type)
    {
    case GameCommandType::OpenMainMenu:
    case GameCommandType::StartNewCampaign:
    case GameCommandType::SelectDeploymentSite:
    case GameCommandType::ClearDeploymentSiteSelection:
    case GameCommandType::StartSiteAttempt:
    case GameCommandType::ReturnToRegionalMap:
    case GameCommandType::SiteAttemptEnded:
        return true;

    case GameCommandType::DeploymentSiteSelectionChanged:
    case GameCommandType::PresentLog:
    default:
        return false;
    }
}

Gs1Status CampaignFlowSystem::process_command(
    CampaignFlowCommandContext& context,
    const GameCommand& command)
{
    if (!subscribes_to(command.type))
    {
        return GS1_STATUS_OK;
    }

    switch (command.type)
    {
    case GameCommandType::OpenMainMenu:
        context.app_state = GS1_APP_STATE_MAIN_MENU;
        return GS1_STATUS_OK;

    case GameCommandType::StartNewCampaign:
    {
        const auto& payload = command.payload_as<StartNewCampaignCommand>();
        context.campaign = CampaignFactory::create_prototype_campaign(payload.campaign_seed, payload.campaign_days);
        context.active_site_run.reset();
        context.app_state = GS1_APP_STATE_REGIONAL_MAP;
        rebuild_regional_map_caches(*context.campaign);
        context.campaign->app_state = context.app_state;
        return GS1_STATUS_OK;
    }

    case GameCommandType::SelectDeploymentSite:
    {
        if (!context.campaign.has_value())
        {
            return GS1_STATUS_INVALID_STATE;
        }

        const auto& payload = command.payload_as<SelectDeploymentSiteCommand>();
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

        GameCommand selection_changed {};
        selection_changed.type = GameCommandType::DeploymentSiteSelectionChanged;
        selection_changed.set_payload(DeploymentSiteSelectionChangedCommand {payload.site_id});
        context.command_queue.push_back(selection_changed);
        return GS1_STATUS_OK;
    }

    case GameCommandType::ClearDeploymentSiteSelection:
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

        GameCommand selection_changed {};
        selection_changed.type = GameCommandType::DeploymentSiteSelectionChanged;
        selection_changed.set_payload(DeploymentSiteSelectionChangedCommand {0U});
        context.command_queue.push_back(selection_changed);
        return GS1_STATUS_OK;
    }

    case GameCommandType::StartSiteAttempt:
    {
        if (!context.campaign.has_value())
        {
            return GS1_STATUS_INVALID_STATE;
        }

        const auto& payload = command.payload_as<StartSiteAttemptCommand>();
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
        context.active_site_run = SiteRunFactory::create_site_run(*context.campaign, *site);
        context.campaign->active_site_id = SiteId {payload.site_id};
        context.app_state = GS1_APP_STATE_SITE_ACTIVE;
        context.campaign->app_state = context.app_state;
        return GS1_STATUS_OK;
    }

    case GameCommandType::ReturnToRegionalMap:
    {
        if (!context.campaign.has_value())
        {
            return GS1_STATUS_INVALID_STATE;
        }

        context.active_site_run.reset();
        context.campaign->active_site_id.reset();
        context.app_state = GS1_APP_STATE_REGIONAL_MAP;
        context.campaign->app_state = context.app_state;
        rebuild_regional_map_caches(*context.campaign);
        return GS1_STATUS_OK;
    }

    case GameCommandType::SiteAttemptEnded:
    {
        if (!context.campaign.has_value() || !context.active_site_run.has_value())
        {
            return GS1_STATUS_INVALID_STATE;
        }

        const auto& payload = command.payload_as<SiteAttemptEndedCommand>();
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
                    context.campaign->regional_map_state.revealed_site_ids.push_back(adjacent_site_id);
                    context.active_site_run->result_newly_revealed_site_count += 1U;
                }
            }
        }

        context.app_state = GS1_APP_STATE_SITE_RESULT;
        context.campaign->app_state = context.app_state;
        rebuild_regional_map_caches(*context.campaign);
        return GS1_STATUS_OK;
    }

    case GameCommandType::DeploymentSiteSelectionChanged:
    case GameCommandType::PresentLog:
    default:
        return GS1_STATUS_OK;
    }
}

void CampaignFlowSystem::run(CampaignFixedStepContext& context)
{
    context.campaign.campaign_clock_minutes_elapsed += k_site_fixed_step_minutes;

    const auto elapsed_days =
        static_cast<std::uint32_t>(context.campaign.campaign_clock_minutes_elapsed / k_minutes_per_day);
    context.campaign.campaign_days_remaining =
        (elapsed_days >= context.campaign.campaign_days_total)
            ? 0U
            : (context.campaign.campaign_days_total - elapsed_days);
}
}  // namespace gs1
