#include "campaign/systems/campaign_flow_system.h"

#include "app/campaign_factory.h"
#include "app/site_run_factory.h"
#include "runtime/game_runtime.h"
#include "runtime/runtime_split_state_compat.h"

namespace gs1
{
namespace
{
[[nodiscard]] RegionalMapState load_regional_map_state(RuntimeInvocation& invocation)
{
    return assemble_regional_map_state_from_state_sets(
        *invocation.owned_state(),
        *invocation.state_manager());
}

void store_regional_map_state(RuntimeInvocation& invocation, const RegionalMapState& regional_map)
{
    write_regional_map_state_to_state_sets(
        regional_map,
        *invocation.owned_state(),
        *invocation.state_manager());
}

[[nodiscard]] std::vector<SiteMetaState> load_campaign_sites(RuntimeInvocation& invocation)
{
    return assemble_sites_state_from_state_sets(*invocation.owned_state(), *invocation.state_manager());
}

void store_campaign_sites(RuntimeInvocation& invocation, const std::vector<SiteMetaState>& sites)
{
    write_sites_state_to_state_sets(sites, *invocation.owned_state(), *invocation.state_manager());
}

void set_campaign_app_state(RuntimeInvocation& invocation, Gs1AppState app_state)
{
    make_game_state_access<CampaignFlowSystem>(invocation).template write<RuntimeAppStateTag>() =
        app_state;
    auto& campaign_core =
        invocation.state_manager()->state<StateSetId::CampaignCore>(*invocation.owned_state());
    if (campaign_core.has_value())
    {
        campaign_core->app_state = app_state;
    }
}

void set_campaign_active_site_id(RuntimeInvocation& invocation, std::optional<SiteId> site_id)
{
    invocation.state_manager()->state<StateSetId::CampaignCore>(*invocation.owned_state())->active_site_id =
        site_id;
}

[[nodiscard]] std::uint64_t campaign_seed(RuntimeInvocation& invocation)
{
    return invocation.state_manager()->state<StateSetId::CampaignCore>(*invocation.owned_state())
        ->campaign_seed;
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

Gs1Status handle_open_main_menu(RuntimeInvocation& invocation) noexcept
{
    set_campaign_app_state(invocation, GS1_APP_STATE_MAIN_MENU);
    return GS1_STATUS_OK;
}

Gs1Status handle_start_new_campaign(
    RuntimeInvocation& invocation,
    const StartNewCampaignMessage& payload)
{
    const CampaignState next_campaign =
        CampaignFactory::create_prototype_campaign(payload.campaign_seed, payload.campaign_days);
    invocation.install_campaign_state(next_campaign);
    invocation.clear_site_run_state();
    auto regional_map = load_regional_map_state(invocation);
    const auto sites = load_campaign_sites(invocation);
    set_campaign_app_state(invocation, GS1_APP_STATE_REGIONAL_MAP);
    rebuild_regional_map_caches(regional_map, sites);
    store_regional_map_state(invocation, regional_map);
    return GS1_STATUS_OK;
}

Gs1Status handle_deployment_site_selection(
    RuntimeInvocation& invocation,
    std::optional<std::uint32_t> selected_site_id)
{
    if (!runtime_invocation_has_campaign(invocation))
    {
        return GS1_STATUS_INVALID_STATE;
    }

    auto regional_map = load_regional_map_state(invocation);
    auto sites = load_campaign_sites(invocation);
    if (selected_site_id.has_value())
    {
        auto* site = find_site_mut(sites, selected_site_id.value());
        if (site == nullptr)
        {
            return GS1_STATUS_NOT_FOUND;
        }

        if (site->site_state != GS1_SITE_STATE_AVAILABLE)
        {
            return GS1_STATUS_INVALID_STATE;
        }
    }

    auto& selection = regional_map.selected_site_id;
    if (!selected_site_id.has_value())
    {
        if (!selection.has_value())
        {
            return GS1_STATUS_OK;
        }

        selection.reset();
    }
    else if (selection.has_value() && selection->value == selected_site_id.value())
    {
        return GS1_STATUS_OK;
    }
    else
    {
        selection = SiteId {selected_site_id.value()};
    }

    store_regional_map_state(invocation, regional_map);
    store_campaign_sites(invocation, sites);
    invocation.emit_game_message(
        DeploymentSiteSelectionChangedMessage {selected_site_id.value_or(0U)});
    return GS1_STATUS_OK;
}

Gs1Status handle_start_site_attempt(
    RuntimeInvocation& invocation,
    std::uint32_t site_id)
{
    if (!runtime_invocation_has_campaign(invocation))
    {
        return GS1_STATUS_INVALID_STATE;
    }

    const auto& faction_progress = make_game_state_access<CampaignFlowSystem>(invocation)
        .template read<RuntimeCampaignFactionProgressTag>();
    auto sites = load_campaign_sites(invocation);
    auto* site = find_site_mut(sites, site_id);
    if (site == nullptr)
    {
        return GS1_STATUS_NOT_FOUND;
    }

    if (site->site_state != GS1_SITE_STATE_AVAILABLE)
    {
        return GS1_STATUS_INVALID_STATE;
    }

    site->attempt_count += 1U;
    SiteRunState active_site_run =
        SiteRunFactory::create_site_run(campaign_seed(invocation), faction_progress, *site);
    invocation.install_site_run_state(active_site_run);
    store_campaign_sites(invocation, sites);
    set_campaign_active_site_id(invocation, SiteId {site_id});
    set_campaign_app_state(invocation, GS1_APP_STATE_SITE_LOADING);
    invocation.emit_game_message(SiteRunStartedMessage {
        active_site_run.site_id.value,
        active_site_run.site_run_id.value,
        active_site_run.site_archetype_id,
        active_site_run.attempt_index,
        active_site_run.site_attempt_seed});
    return GS1_STATUS_OK;
}

Gs1Status handle_return_to_regional_map(RuntimeInvocation& invocation)
{
    if (!runtime_invocation_has_campaign(invocation))
    {
        return GS1_STATUS_INVALID_STATE;
    }

    auto regional_map = load_regional_map_state(invocation);
    const auto sites = load_campaign_sites(invocation);
    invocation.clear_site_run_state();
    set_campaign_active_site_id(invocation, std::nullopt);
    set_campaign_app_state(invocation, GS1_APP_STATE_REGIONAL_MAP);
    rebuild_regional_map_caches(regional_map, sites);
    store_regional_map_state(invocation, regional_map);
    return GS1_STATUS_OK;
}

Gs1Status handle_site_attempt_ended(
    RuntimeInvocation& invocation,
    const SiteAttemptEndedMessage& payload)
{
    if (!runtime_invocation_has_campaign(invocation) ||
        !runtime_invocation_has_active_site_run(invocation))
    {
        return GS1_STATUS_INVALID_STATE;
    }

    auto regional_map = load_regional_map_state(invocation);
    auto sites = load_campaign_sites(invocation);
    auto* site = find_site_mut(sites, payload.site_id);
    if (site == nullptr)
    {
        return GS1_STATUS_NOT_FOUND;
    }

    auto& site_run_meta =
        invocation.state_manager()->state<StateSetId::SiteRunMeta>(*invocation.owned_state()).value();
    site_run_meta.run_status =
        payload.result == GS1_SITE_ATTEMPT_RESULT_COMPLETED
            ? SiteRunStatus::Completed
            : SiteRunStatus::Failed;
    site_run_meta.result_newly_revealed_site_count = 0U;

    if (payload.result == GS1_SITE_ATTEMPT_RESULT_COMPLETED)
    {
        site->site_state = GS1_SITE_STATE_COMPLETED;

        for (const auto adjacent_site_id : site->adjacent_site_ids)
        {
            auto* adjacent_site = find_site_mut(sites, adjacent_site_id.value);
            if (adjacent_site != nullptr && adjacent_site->site_state == GS1_SITE_STATE_LOCKED)
            {
                adjacent_site->site_state = GS1_SITE_STATE_AVAILABLE;
                if (!contains_site_id(regional_map.revealed_site_ids, adjacent_site_id))
                {
                    regional_map.revealed_site_ids.push_back(adjacent_site_id);
                    site_run_meta.result_newly_revealed_site_count += 1U;
                }
            }
        }

    }

    set_campaign_app_state(invocation, GS1_APP_STATE_SITE_RESULT);
    rebuild_regional_map_caches(regional_map, sites);
    store_regional_map_state(invocation, regional_map);
    store_campaign_sites(invocation, sites);
    return GS1_STATUS_OK;
}
}  // namespace

const char* CampaignFlowSystem::name() const noexcept
{
    return "CampaignFlowSystem";
}

HostMessageSubscriptionSpan CampaignFlowSystem::subscribed_host_messages() const noexcept
{
    return {};
}

std::optional<Gs1RuntimeProfileSystemId> CampaignFlowSystem::profile_system_id() const noexcept
{
    return GS1_RUNTIME_PROFILE_SYSTEM_CAMPAIGN_FLOW;
}

std::optional<std::uint32_t> CampaignFlowSystem::fixed_step_order() const noexcept
{
    return std::nullopt;
}

Gs1Status CampaignFlowSystem::handle(
    RuntimeInvocation& invocation,
    const OpenMainMenuMessage& message)
{
    (void)message;
    return handle_open_main_menu(invocation);
}

Gs1Status CampaignFlowSystem::handle(
    RuntimeInvocation& invocation,
    const StartNewCampaignMessage& message)
{
    return handle_start_new_campaign(invocation, message);
}

Gs1Status CampaignFlowSystem::handle(
    RuntimeInvocation& invocation,
    const SelectDeploymentSiteMessage& message)
{
    return handle_deployment_site_selection(invocation, message.site_id);
}

Gs1Status CampaignFlowSystem::handle(
    RuntimeInvocation& invocation,
    const ClearDeploymentSiteSelectionMessage& message)
{
    (void)message;
    return handle_deployment_site_selection(invocation, std::nullopt);
}

Gs1Status CampaignFlowSystem::handle(
    RuntimeInvocation& invocation,
    const StartSiteAttemptMessage& message)
{
    return handle_start_site_attempt(invocation, message.site_id);
}

Gs1Status CampaignFlowSystem::handle(
    RuntimeInvocation& invocation,
    const ReturnToRegionalMapMessage& message)
{
    (void)message;
    return handle_return_to_regional_map(invocation);
}

Gs1Status CampaignFlowSystem::handle(
    RuntimeInvocation& invocation,
    const SiteAttemptEndedMessage& message)
{
    return handle_site_attempt_ended(invocation, message);
}

void CampaignFlowSystem::run(RuntimeInvocation& invocation)
{
    if (!runtime_invocation_has_campaign(invocation) ||
        !runtime_invocation_has_active_site_run(invocation))
    {
        return;
    }

    auto access = make_game_state_access<CampaignFlowSystem>(invocation);
    auto& app_state = access.template write<RuntimeAppStateTag>();
    if (app_state != GS1_APP_STATE_SITE_LOADING)
    {
        return;
    }

    app_state = GS1_APP_STATE_SITE_ACTIVE;
    set_campaign_app_state(invocation, app_state);
    invocation.emit_game_message(SiteSceneActivatedMessage {});
}
}  // namespace gs1
