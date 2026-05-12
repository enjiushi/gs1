#include "campaign/systems/loadout_planner_system.h"

#include "campaign/campaign_state.h"
#include "campaign/systems/campaign_system_context.h"
#include "content/defs/item_defs.h"
#include "content/prototype_content.h"
#include "runtime/game_runtime.h"

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
    const CampaignState& campaign,
    SiteId site_id) noexcept
{
    for (const auto& site : campaign.sites)
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

void rebuild_selected_loadout(CampaignState& campaign)
{
    auto& planner = campaign.loadout_planner_state;
    planner.available_exported_support_items.clear();
    planner.active_nearby_aura_modifier_ids.clear();
    planner.support_quota = 0U;
    planner.selected_loadout_slots = planner.baseline_deployment_items;

    if (!planner.selected_target_site_id.has_value())
    {
        return;
    }

    const auto* selected_site = find_site(campaign, planner.selected_target_site_id.value());
    if (selected_site == nullptr)
    {
        planner.selected_target_site_id.reset();
        return;
    }

    for (const auto contributor_site_id : selected_site->adjacent_site_ids)
    {
        const auto* contributor = find_site(campaign, contributor_site_id);
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
    CampaignSystemContext& context,
    const GameMessage& message);

template <>
struct system_state_tags<LoadoutPlannerSystem>
{
    using type = type_list<RuntimeCampaignTag>;
};

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
    rebuild_selected_loadout(campaign);
}

bool LoadoutPlannerSystem::subscribes_to(GameMessageType type) noexcept
{
    return type == GameMessageType::SelectDeploymentSite ||
        type == GameMessageType::ClearDeploymentSiteSelection ||
        type == GameMessageType::DeploymentSiteSelectionChanged;
}

const char* LoadoutPlannerSystem::name() const noexcept
{
    return "LoadoutPlannerSystem";
}

GameMessageSubscriptionSpan LoadoutPlannerSystem::subscribed_game_messages() const noexcept
{
    return runtime_subscription_list<
        GameMessageType,
        k_game_message_type_count,
        LoadoutPlannerSystem::subscribes_to>();
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
    auto access = make_game_state_access<LoadoutPlannerSystem>(invocation);
    auto& campaign = access.template read<RuntimeCampaignTag>();
    if (!campaign.has_value())
    {
        return GS1_STATUS_INVALID_STATE;
    }

    CampaignSystemContext context {*campaign};
    return process_loadout_planner_message(context, message);
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
    CampaignSystemContext& context,
    const GameMessage& message)
{
    if (!LoadoutPlannerSystem::subscribes_to(message.type))
    {
        return GS1_STATUS_OK;
    }

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
        context.campaign.loadout_planner_state.selected_target_site_id.reset();
    }
    else
    {
        context.campaign.loadout_planner_state.selected_target_site_id = SiteId {selected_site_id};
    }

    rebuild_selected_loadout(context.campaign);
    return GS1_STATUS_OK;
}
}  // namespace gs1
