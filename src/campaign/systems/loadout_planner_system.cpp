#include "campaign/systems/loadout_planner_system.h"

#include "campaign/campaign_state.h"
#include "content/defs/item_defs.h"
#include "content/prototype_content.h"
#include "runtime/game_runtime.h"
#include "runtime/runtime_split_state_compat.h"

#include <algorithm>

namespace gs1
{
namespace
{
[[nodiscard]] LoadoutSlot make_loadout_slot(std::uint32_t item_id, std::uint32_t quantity) noexcept
{
    return LoadoutSlot {
        ItemId {item_id},
        quantity,
        item_id != 0U && quantity > 0U};
}

[[nodiscard]] std::vector<LoadoutSlot> make_baseline_deployment_items(
    const PrototypeCampaignContent& content)
{
    std::vector<LoadoutSlot> items {};
    items.reserve(content.baseline_deployment_items.size());
    for (const auto& item : content.baseline_deployment_items)
    {
        items.push_back(make_loadout_slot(item.item_id.value, item.quantity));
    }
    return items;
}

[[nodiscard]] const SiteMetaState* find_site(
    const std::vector<SiteMetaState>& sites,
    SiteId site_id) noexcept
{
    for (const auto& site : sites)
    {
        if (site.site_id == site_id)
        {
            return &site;
        }
    }

    return nullptr;
}

void merge_loadout_slot(
    std::vector<LoadoutSlot>& slots,
    ItemId item_id,
    std::uint32_t quantity)
{
    if (item_id.value == 0U || quantity == 0U)
    {
        return;
    }

    for (auto& slot : slots)
    {
        if (slot.occupied && slot.item_id == item_id)
        {
            slot.quantity = std::min(slot.quantity + quantity, item_stack_size(item_id));
            return;
        }
    }

    slots.push_back(LoadoutSlot {
        item_id,
        std::min(quantity, item_stack_size(item_id)),
        true});
}

void rebuild_selected_loadout(
    LoadoutPlannerState& planner,
    const std::vector<SiteMetaState>& sites)
{
    planner.available_exported_support_items.clear();
    planner.active_nearby_aura_modifier_ids.clear();
    planner.support_quota = 0U;
    planner.selected_loadout_slots = planner.baseline_deployment_items;

    if (!planner.selected_target_site_id.has_value())
    {
        return;
    }

    const auto* selected_site = find_site(sites, planner.selected_target_site_id.value());
    if (selected_site == nullptr)
    {
        planner.selected_target_site_id.reset();
        return;
    }

    for (const auto contributor_site_id : selected_site->adjacent_site_ids)
    {
        const auto* contributor = find_site(sites, contributor_site_id);
        if (contributor == nullptr || contributor->site_state != GS1_SITE_STATE_COMPLETED)
        {
            continue;
        }

        planner.support_quota += planner.support_quota_per_contributor;

        for (const auto& slot : contributor->exported_support_items)
        {
            if (!slot.occupied || slot.item_id.value == 0U || slot.quantity == 0U)
            {
                continue;
            }

            merge_loadout_slot(planner.available_exported_support_items, slot.item_id, slot.quantity);
            merge_loadout_slot(planner.selected_loadout_slots, slot.item_id, slot.quantity);
        }

        planner.active_nearby_aura_modifier_ids.insert(
            planner.active_nearby_aura_modifier_ids.end(),
            contributor->nearby_aura_modifier_ids.begin(),
            contributor->nearby_aura_modifier_ids.end());
    }
}

}  // namespace

Gs1Status process_loadout_planner_message(
    RuntimeInvocation& invocation,
    const GameMessage& message);

void LoadoutPlannerSystem::initialize_campaign_state(CampaignState& campaign)
{
    const auto& content = get_prototype_campaign_content();
    auto& planner = campaign.loadout_planner_state;
    planner.selected_target_site_id.reset();
    planner.baseline_deployment_items = make_baseline_deployment_items(content);
    planner.selected_loadout_slots.clear();
    planner.available_exported_support_items.clear();
    planner.active_nearby_aura_modifier_ids.clear();
    planner.support_quota_per_contributor = content.support_quota_per_contributor;
    planner.support_quota = 0U;
    rebuild_selected_loadout(planner, campaign.sites);
}

const char* LoadoutPlannerSystem::name() const noexcept
{
    return "LoadoutPlannerSystem";
}

GameMessageSubscriptionSpan LoadoutPlannerSystem::subscribed_game_messages() const noexcept
{
    static constexpr GameMessageType subscriptions[] = {
        GameMessageType::SelectDeploymentSite,
        GameMessageType::ClearDeploymentSiteSelection,
        GameMessageType::DeploymentSiteSelectionChanged,
    };
    return subscriptions;
}

HostMessageSubscriptionSpan LoadoutPlannerSystem::subscribed_host_messages() const noexcept
{
    return {};
}

std::optional<Gs1RuntimeProfileSystemId> LoadoutPlannerSystem::profile_system_id() const noexcept
{
    return GS1_RUNTIME_PROFILE_SYSTEM_LOADOUT_PLANNER;
}

std::optional<std::uint32_t> LoadoutPlannerSystem::fixed_step_order() const noexcept
{
    return std::nullopt;
}

Gs1Status LoadoutPlannerSystem::process_game_message(
    RuntimeInvocation& invocation,
    const GameMessage& message)
{
    if (!runtime_invocation_has_campaign(invocation))
    {
        return GS1_STATUS_INVALID_STATE;
    }

    return process_loadout_planner_message(invocation, message);
}

Gs1Status LoadoutPlannerSystem::process_host_message(
    RuntimeInvocation& invocation,
    const Gs1HostMessage& message)
{
    (void)invocation;
    (void)message;
    return GS1_STATUS_OK;
}

void LoadoutPlannerSystem::run(RuntimeInvocation& invocation)
{
    (void)invocation;
}

Gs1Status process_loadout_planner_message(
    RuntimeInvocation& invocation,
    const GameMessage& message)
{
    if (!runtime_invocation_has_campaign(invocation))
    {
        return GS1_STATUS_INVALID_STATE;
    }

    auto& planner =
        invocation.state_manager()->state<StateSetId::CampaignLoadoutPlannerMeta>(*invocation.owned_state())
            .value();
    auto& baseline_items = invocation.state_manager()
                               ->state<StateSetId::CampaignLoadoutPlannerBaselineItems>(
                                   *invocation.owned_state())
                               .value();
    auto& available_support_items = invocation.state_manager()
                                        ->state<StateSetId::CampaignLoadoutPlannerAvailableSupportItems>(
                                            *invocation.owned_state())
                                        .value();
    auto& selected_slots = invocation.state_manager()
                               ->state<StateSetId::CampaignLoadoutPlannerSelectedSlots>(
                                   *invocation.owned_state())
                               .value();
    auto& nearby_aura_ids = invocation.state_manager()
                                ->state<StateSetId::CampaignLoadoutPlannerNearbyAuraModifiers>(
                                    *invocation.owned_state())
                                .value();
    auto sites = assemble_sites_state_from_state_sets(*invocation.owned_state(), *invocation.state_manager());

    std::uint32_t selected_site_id = 0U;
    switch (message.type)
    {
    case GameMessageType::SelectDeploymentSite:
        selected_site_id = message.payload_as<SelectDeploymentSiteMessage>().site_id;
        break;

    case GameMessageType::ClearDeploymentSiteSelection:
        selected_site_id = 0U;
        break;

    case GameMessageType::DeploymentSiteSelectionChanged:
        selected_site_id = message.payload_as<DeploymentSiteSelectionChangedMessage>().selected_site_id;
        break;

    default:
        return GS1_STATUS_OK;
    }

    if (selected_site_id == 0U)
    {
        planner.has_selected_target_site_id = false;
        planner.selected_target_site_id = SiteId {};
    }
    else
    {
        planner.has_selected_target_site_id = true;
        planner.selected_target_site_id = SiteId {selected_site_id};
    }

    LoadoutPlannerState planner_state {};
    if (planner.has_selected_target_site_id)
    {
        planner_state.selected_target_site_id = planner.selected_target_site_id;
    }
    planner_state.baseline_deployment_items = baseline_items;
    planner_state.available_exported_support_items = available_support_items;
    planner_state.selected_loadout_slots = selected_slots;
    planner_state.active_nearby_aura_modifier_ids = nearby_aura_ids;
    planner_state.support_quota_per_contributor = planner.support_quota_per_contributor;
    planner_state.support_quota = planner.support_quota;

    rebuild_selected_loadout(planner_state, sites);

    planner = LoadoutPlannerMetaState {
        planner_state.selected_target_site_id.value_or(SiteId {}),
        planner_state.support_quota_per_contributor,
        planner_state.support_quota,
        planner_state.selected_target_site_id.has_value()};
    baseline_items = std::move(planner_state.baseline_deployment_items);
    available_support_items = std::move(planner_state.available_exported_support_items);
    selected_slots = std::move(planner_state.selected_loadout_slots);
    nearby_aura_ids = std::move(planner_state.active_nearby_aura_modifier_ids);
    return GS1_STATUS_OK;
}
}  // namespace gs1
