#include "campaign/systems/campaign_flow_system.h"

#include "app/campaign_factory.h"
#include "app/site_run_factory.h"
#include "campaign/campaign_state.h"
#include "campaign/systems/campaign_system_context.h"
#include "content/prototype_content.h"
#include "runtime/game_runtime.h"

namespace gs1
{
namespace
{
using CampaignFlowSystemTags =
    type_list<RuntimeAppStateTag, RuntimeCampaignTag>;

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

void rebuild_regional_map_caches(
    RegionalMapState& map,
    const std::vector<SiteMetaState>& sites)
{
    map.available_site_ids.clear();
    map.completed_site_ids.clear();

    for (const auto& site : sites)
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

SiteMetaState* find_site_mut(std::vector<SiteMetaState>& sites, std::uint32_t site_id) noexcept
{
    for (auto& site : sites)
    {
        if (site.site_id.value == site_id)
        {
            return &site;
        }
    }

    return nullptr;
}
}  // namespace

Gs1Status process_campaign_flow_host_message(
    RuntimeInvocation& invocation,
    const Gs1HostMessage& message);

Gs1Status process_campaign_flow_message(
    RuntimeInvocation& invocation,
    const GameMessage& message);

template <>
struct system_state_tags<CampaignFlowSystem>
{
    using type = CampaignFlowSystemTags;
};

Gs1Status process_campaign_flow_host_message(
    RuntimeInvocation& invocation,
    const Gs1HostMessage& message)
{
    auto campaign = make_campaign_state_access(invocation);
    auto& app_state = campaign.app_state();

    switch (message.type)
    {
    case GS1_HOST_EVENT_SITE_SCENE_READY:
        if (!campaign.has_campaign() ||
            !runtime_invocation_has_active_site_run(invocation) ||
            app_state != GS1_APP_STATE_SITE_LOADING)
        {
            return GS1_STATUS_OK;
        }

        app_state = GS1_APP_STATE_SITE_ACTIVE;
        campaign.app_state() = app_state;
        {
            GameMessage site_scene_activated {};
            site_scene_activated.type = GameMessageType::SiteSceneActivated;
            site_scene_activated.set_payload(SiteSceneActivatedMessage {});
            invocation.push_game_message(site_scene_activated);
        }
        return GS1_STATUS_OK;

    case GS1_HOST_EVENT_GAMEPLAY_ACTION:
    {
        const auto& action = message.payload.gameplay_action.action;
        GameMessage gameplay_message {};

        switch (action.type)
        {
        case GS1_GAMEPLAY_ACTION_START_NEW_CAMPAIGN:
            gameplay_message.type = GameMessageType::StartNewCampaign;
            gameplay_message.set_payload(StartNewCampaignMessage {
                action.arg0,
                static_cast<std::uint32_t>(action.arg1)});
            invocation.push_game_message(gameplay_message);
            return GS1_STATUS_OK;

        case GS1_GAMEPLAY_ACTION_SELECT_DEPLOYMENT_SITE:
            if (action.target_id == 0U)
            {
                return GS1_STATUS_INVALID_ARGUMENT;
            }
            gameplay_message.type = GameMessageType::SelectDeploymentSite;
            gameplay_message.set_payload(SelectDeploymentSiteMessage {action.target_id});
            invocation.push_game_message(gameplay_message);
            return GS1_STATUS_OK;

        case GS1_GAMEPLAY_ACTION_CLEAR_DEPLOYMENT_SITE_SELECTION:
            gameplay_message.type = GameMessageType::ClearDeploymentSiteSelection;
            gameplay_message.set_payload(ClearDeploymentSiteSelectionMessage {});
            invocation.push_game_message(gameplay_message);
            return GS1_STATUS_OK;

        case GS1_GAMEPLAY_ACTION_START_SITE_ATTEMPT:
            if (action.target_id == 0U)
            {
                return GS1_STATUS_INVALID_ARGUMENT;
            }
            gameplay_message.type = GameMessageType::StartSiteAttempt;
            gameplay_message.set_payload(StartSiteAttemptMessage {action.target_id});
            invocation.push_game_message(gameplay_message);
            return GS1_STATUS_OK;

        case GS1_GAMEPLAY_ACTION_RETURN_TO_REGIONAL_MAP:
            gameplay_message.type = GameMessageType::ReturnToRegionalMap;
            gameplay_message.set_payload(ReturnToRegionalMapMessage {});
            invocation.push_game_message(gameplay_message);
            return GS1_STATUS_OK;

        case GS1_GAMEPLAY_ACTION_OPEN_REGIONAL_MAP_TECH_TREE:
        case GS1_GAMEPLAY_ACTION_CLOSE_REGIONAL_MAP_TECH_TREE:
        case GS1_GAMEPLAY_ACTION_SELECT_TECH_TREE_FACTION_TAB:
        case GS1_GAMEPLAY_ACTION_SET_PHONE_PANEL_SECTION:
        case GS1_GAMEPLAY_ACTION_CLOSE_PHONE_PANEL:
        case GS1_GAMEPLAY_ACTION_OPEN_SITE_PROTECTION_SELECTOR:
        case GS1_GAMEPLAY_ACTION_CLOSE_SITE_PROTECTION_UI:
        case GS1_GAMEPLAY_ACTION_SET_SITE_PROTECTION_OVERLAY_MODE:
        case GS1_GAMEPLAY_ACTION_NONE:
        default:
            return GS1_STATUS_OK;
        }
    }

    default:
        return GS1_STATUS_OK;
    }
}

const char* CampaignFlowSystem::name() const noexcept
{
    return "CampaignFlowSystem";
}

GameMessageSubscriptionSpan CampaignFlowSystem::subscribed_game_messages() const noexcept
{
    static constexpr GameMessageType subscriptions[] = {
        GameMessageType::OpenMainMenu,
        GameMessageType::StartNewCampaign,
        GameMessageType::SelectDeploymentSite,
        GameMessageType::ClearDeploymentSiteSelection,
        GameMessageType::StartSiteAttempt,
        GameMessageType::ReturnToRegionalMap,
        GameMessageType::SiteAttemptEnded,
    };
    return subscriptions;
}

HostMessageSubscriptionSpan CampaignFlowSystem::subscribed_host_messages() const noexcept
{
    static constexpr Gs1HostMessageType subscriptions[] = {
        GS1_HOST_EVENT_GAMEPLAY_ACTION,
        GS1_HOST_EVENT_SITE_SCENE_READY};
    return subscriptions;
}

std::optional<Gs1RuntimeProfileSystemId> CampaignFlowSystem::profile_system_id() const noexcept
{
    return GS1_RUNTIME_PROFILE_SYSTEM_CAMPAIGN_FLOW;
}

std::optional<std::uint32_t> CampaignFlowSystem::fixed_step_order() const noexcept
{
    return std::nullopt;
}

Gs1Status CampaignFlowSystem::process_game_message(
    RuntimeInvocation& invocation,
    const GameMessage& message)
{
    return process_campaign_flow_message(invocation, message);
}

Gs1Status CampaignFlowSystem::process_host_message(
    RuntimeInvocation& invocation,
    const Gs1HostMessage& message)
{
    return process_campaign_flow_host_message(invocation, message);
}

void CampaignFlowSystem::run(RuntimeInvocation& invocation)
{
    (void)invocation;
}

Gs1Status process_campaign_flow_message(
    RuntimeInvocation& invocation,
    const GameMessage& message)
{
    auto campaign = make_campaign_state_access(invocation);
    auto& app_state = campaign.app_state();

    switch (message.type)
    {
    case GameMessageType::OpenMainMenu:
        app_state = GS1_APP_STATE_MAIN_MENU;
        return GS1_STATUS_OK;

    case GameMessageType::StartNewCampaign:
    {
        const auto& payload = message.payload_as<StartNewCampaignMessage>();
        const CampaignState next_campaign =
            CampaignFactory::create_prototype_campaign(payload.campaign_seed, payload.campaign_days);
        runtime_invocation_clear_active_site_run(invocation);
        campaign.assign_from(next_campaign);
        app_state = GS1_APP_STATE_REGIONAL_MAP;
        campaign.app_state() = app_state;
        rebuild_regional_map_caches(campaign.regional_map(), campaign.sites());
        return GS1_STATUS_OK;
    }

    case GameMessageType::SelectDeploymentSite:
    {
        if (!campaign.has_campaign())
        {
            return GS1_STATUS_INVALID_STATE;
        }

        const auto& payload = message.payload_as<SelectDeploymentSiteMessage>();
        auto* site = find_site_mut(campaign.sites(), payload.site_id);
        if (site == nullptr)
        {
            return GS1_STATUS_NOT_FOUND;
        }

        if (site->site_state != GS1_SITE_STATE_AVAILABLE)
        {
            return GS1_STATUS_INVALID_STATE;
        }

        auto& selection = campaign.regional_map().selected_site_id;
        if (selection.has_value() && selection->value == payload.site_id)
        {
            return GS1_STATUS_OK;
        }

        selection = SiteId {payload.site_id};

        GameMessage selection_changed {};
        selection_changed.type = GameMessageType::DeploymentSiteSelectionChanged;
        selection_changed.set_payload(DeploymentSiteSelectionChangedMessage {payload.site_id});
        invocation.push_game_message(selection_changed);
        return GS1_STATUS_OK;
    }

    case GameMessageType::ClearDeploymentSiteSelection:
    {
        if (!campaign.has_campaign())
        {
            return GS1_STATUS_INVALID_STATE;
        }

        auto& selection = campaign.regional_map().selected_site_id;
        if (!selection.has_value())
        {
            return GS1_STATUS_OK;
        }

        selection.reset();

        GameMessage selection_changed {};
        selection_changed.type = GameMessageType::DeploymentSiteSelectionChanged;
        selection_changed.set_payload(DeploymentSiteSelectionChangedMessage {0U});
        invocation.push_game_message(selection_changed);
        return GS1_STATUS_OK;
    }

    case GameMessageType::StartSiteAttempt:
    {
        if (!campaign.has_campaign())
        {
            return GS1_STATUS_INVALID_STATE;
        }

        const auto& payload = message.payload_as<StartSiteAttemptMessage>();
        auto* site = find_site_mut(campaign.sites(), payload.site_id);
        if (site == nullptr)
        {
            return GS1_STATUS_NOT_FOUND;
        }

        if (site->site_state != GS1_SITE_STATE_AVAILABLE)
        {
            return GS1_STATUS_INVALID_STATE;
        }

        site->attempt_count += 1U;
        CampaignState campaign_snapshot {};
        campaign_snapshot.campaign_id = campaign.campaign_id();
        campaign_snapshot.campaign_seed = campaign.campaign_seed();
        campaign_snapshot.campaign_clock_minutes_elapsed = campaign.campaign_clock_minutes_elapsed();
        campaign_snapshot.campaign_days_total = campaign.campaign_days_total();
        campaign_snapshot.campaign_days_remaining = campaign.campaign_days_remaining();
        campaign_snapshot.app_state = campaign.app_state();
        campaign_snapshot.active_site_id = campaign.active_site_id();
        campaign_snapshot.regional_map_state = campaign.regional_map();
        campaign_snapshot.faction_progress = campaign.faction_progress();
        campaign_snapshot.technology_state = campaign.technology();
        campaign_snapshot.loadout_planner_state = campaign.loadout_planner();
        campaign_snapshot.sites = campaign.sites();
        SiteRunState active_site_run = SiteRunFactory::create_site_run(campaign_snapshot, *site);
        runtime_invocation_assign_active_site_run(invocation, active_site_run);
        campaign.active_site_id() = SiteId {payload.site_id};
        app_state = GS1_APP_STATE_SITE_LOADING;
        campaign.app_state() = app_state;

        GameMessage site_run_started {};
        site_run_started.type = GameMessageType::SiteRunStarted;
        site_run_started.set_payload(SiteRunStartedMessage {
            active_site_run.site_id.value,
            active_site_run.site_run_id.value,
            active_site_run.site_archetype_id,
            active_site_run.attempt_index,
            active_site_run.site_attempt_seed});
        invocation.push_game_message(site_run_started);
        return GS1_STATUS_OK;
    }

    case GameMessageType::ReturnToRegionalMap:
    {
        if (!campaign.has_campaign())
        {
            return GS1_STATUS_INVALID_STATE;
        }

        runtime_invocation_clear_active_site_run(invocation);
        campaign.active_site_id().reset();
        app_state = GS1_APP_STATE_REGIONAL_MAP;
        campaign.app_state() = app_state;
        rebuild_regional_map_caches(campaign.regional_map(), campaign.sites());
        return GS1_STATUS_OK;
    }

    case GameMessageType::SiteAttemptEnded:
    {
        if (!campaign.has_campaign() || !runtime_invocation_has_active_site_run(invocation))
        {
            return GS1_STATUS_INVALID_STATE;
        }

        const auto& payload = message.payload_as<SiteAttemptEndedMessage>();
        auto* site = find_site_mut(campaign.sites(), payload.site_id);
        if (site == nullptr)
        {
            return GS1_STATUS_NOT_FOUND;
        }

        SiteRunState active_site_run = runtime_invocation_active_site_run_copy(invocation);
        active_site_run.run_status =
            payload.result == GS1_SITE_ATTEMPT_RESULT_COMPLETED
                ? SiteRunStatus::Completed
                : SiteRunStatus::Failed;
        active_site_run.result_newly_revealed_site_count = 0U;

        if (payload.result == GS1_SITE_ATTEMPT_RESULT_COMPLETED)
        {
            site->site_state = GS1_SITE_STATE_COMPLETED;

            for (const auto adjacent_site_id : site->adjacent_site_ids)
            {
                auto* adjacent_site = find_site_mut(campaign.sites(), adjacent_site_id.value);
                if (adjacent_site != nullptr && adjacent_site->site_state == GS1_SITE_STATE_LOCKED)
                {
                    adjacent_site->site_state = GS1_SITE_STATE_AVAILABLE;
                    if (!contains_site_id(campaign.regional_map().revealed_site_ids, adjacent_site_id))
                    {
                        campaign.regional_map().revealed_site_ids.push_back(adjacent_site_id);
                        active_site_run.result_newly_revealed_site_count += 1U;
                    }
                }
            }

            if (site->completion_reputation_reward > 0)
            {
                GameMessage reputation_award {};
                reputation_award.type = GameMessageType::CampaignReputationAwardRequested;
                reputation_award.set_payload(CampaignReputationAwardRequestedMessage {
                    site->completion_reputation_reward});
                invocation.push_game_message(reputation_award);
            }

            if (site->featured_faction_id.value != 0U &&
                site->completion_faction_reputation_reward > 0)
            {
                GameMessage faction_award {};
                faction_award.type = GameMessageType::FactionReputationAwardRequested;
                faction_award.set_payload(FactionReputationAwardRequestedMessage {
                    site->featured_faction_id.value,
                    site->completion_faction_reputation_reward});
                invocation.push_game_message(faction_award);
            }
        }

        app_state = GS1_APP_STATE_SITE_RESULT;
        campaign.app_state() = app_state;
        rebuild_regional_map_caches(campaign.regional_map(), campaign.sites());
        runtime_invocation_assign_active_site_run(invocation, active_site_run);
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
