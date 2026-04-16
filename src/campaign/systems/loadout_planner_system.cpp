#include "campaign/systems/loadout_planner_system.h"

#include "campaign/campaign_state.h"
#include "content/defs/item_defs.h"

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

[[nodiscard]] std::vector<LoadoutSlot> make_baseline_deployment_items()
{
    // Keep the baseline package small and fixed: survival basics, one starter seed line,
    // and enough raw materials to let the starter workshop matter immediately.
    return {
        make_loadout_slot(k_item_water_container, 2U),
        make_loadout_slot(k_item_food_pack, 1U),
        make_loadout_slot(k_item_medicine_pack, 1U),
        make_loadout_slot(k_item_wind_reed_seed_bundle, 8U),
        make_loadout_slot(k_item_wood_bundle, 6U),
        make_loadout_slot(k_item_iron_bundle, 4U)};
}

void rebuild_selected_loadout(CampaignState& campaign)
{
    auto& planner = campaign.loadout_planner_state;
    planner.available_exported_support_items.clear();
    planner.active_nearby_aura_modifier_ids.clear();
    planner.selected_loadout_slots = planner.baseline_deployment_items;
}
}  // namespace

void LoadoutPlannerSystem::initialize_campaign_state(CampaignState& campaign)
{
    auto& planner = campaign.loadout_planner_state;
    planner.selected_target_site_id.reset();
    planner.baseline_deployment_items = make_baseline_deployment_items();
    planner.selected_loadout_slots.clear();
    planner.available_exported_support_items.clear();
    planner.active_nearby_aura_modifier_ids.clear();
    planner.support_quota = 0U;
    rebuild_selected_loadout(campaign);
}

bool LoadoutPlannerSystem::subscribes_to(GameCommandType type) noexcept
{
    return type == GameCommandType::DeploymentSiteSelectionChanged;
}

Gs1Status LoadoutPlannerSystem::process_command(
    CampaignSystemContext& context,
    const GameCommand& command)
{
    if (!subscribes_to(command.type))
    {
        return GS1_STATUS_OK;
    }

    const auto& payload = command.payload_as<DeploymentSiteSelectionChangedCommand>();
    if (payload.selected_site_id == 0U)
    {
        context.campaign.loadout_planner_state.selected_target_site_id.reset();
    }
    else
    {
        context.campaign.loadout_planner_state.selected_target_site_id = SiteId {payload.selected_site_id};
    }

    rebuild_selected_loadout(context.campaign);
    return GS1_STATUS_OK;
}
}  // namespace gs1
