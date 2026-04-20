#include "site/systems/modifier_system.h"

#include "campaign/campaign_state.h"
#include "content/defs/technology_defs.h"
#include "site/site_projection_update_flags.h"
#include "site/site_run_state.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cmath>

namespace gs1
{
namespace
{
constexpr float k_modifier_channel_limit = 1.0f;
constexpr float k_modifier_change_epsilon = 1e-4f;

constexpr std::array<ModifierChannelTotals, 3> k_nearby_aura_presets = {
    ModifierChannelTotals{
        -0.15f, -0.08f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.08f, 0.0f},
    ModifierChannelTotals{
        0.0f, 0.0f, 0.0f, 0.12f, 0.07f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.04f, 0.0f},
    ModifierChannelTotals{
        0.0f, 0.0f, -0.1f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.1f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.05f}};

constexpr std::array<ModifierChannelTotals, 4> k_run_modifier_presets = {
    ModifierChannelTotals{
        0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.15f, 0.0f, 0.05f, 0.25f, 0.0f, 0.08f, 0.1f},
    ModifierChannelTotals{
        0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.2f, 0.05f, 0.0f, 0.0f, 0.12f, 0.3f},
    ModifierChannelTotals{
        0.0f, 0.0f, 0.0f, 0.14f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.25f, 0.15f, 0.0f, 0.0f, 0.04f, 0.0f},
    ModifierChannelTotals{
        0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, -0.18f, 0.0f, 0.2f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.05f}};

void accumulate_totals(
    ModifierChannelTotals& destination,
    const ModifierChannelTotals& source) noexcept
{
    destination.heat += source.heat;
    destination.wind += source.wind;
    destination.dust += source.dust;
    destination.moisture += source.moisture;
    destination.fertility += source.fertility;
    destination.salinity += source.salinity;
    destination.growth_pressure += source.growth_pressure;
    destination.salinity_density_cap += source.salinity_density_cap;
    destination.plant_density += source.plant_density;
    destination.health += source.health;
    destination.hydration += source.hydration;
    destination.nourishment += source.nourishment;
    destination.energy_cap += source.energy_cap;
    destination.energy += source.energy;
    destination.morale += source.morale;
    destination.work_efficiency += source.work_efficiency;
}

ModifierChannelTotals clamp_totals(ModifierChannelTotals totals) noexcept
{
    totals.heat = std::clamp(totals.heat, -k_modifier_channel_limit, k_modifier_channel_limit);
    totals.wind = std::clamp(totals.wind, -k_modifier_channel_limit, k_modifier_channel_limit);
    totals.dust = std::clamp(totals.dust, -k_modifier_channel_limit, k_modifier_channel_limit);
    totals.moisture = std::clamp(totals.moisture, -k_modifier_channel_limit, k_modifier_channel_limit);
    totals.fertility = std::clamp(totals.fertility, -k_modifier_channel_limit, k_modifier_channel_limit);
    totals.salinity = std::clamp(totals.salinity, -k_modifier_channel_limit, k_modifier_channel_limit);
    totals.growth_pressure =
        std::clamp(totals.growth_pressure, -k_modifier_channel_limit, k_modifier_channel_limit);
    totals.salinity_density_cap =
        std::clamp(totals.salinity_density_cap, -k_modifier_channel_limit, k_modifier_channel_limit);
    totals.plant_density =
        std::clamp(totals.plant_density, -k_modifier_channel_limit, k_modifier_channel_limit);
    totals.health = std::clamp(totals.health, -k_modifier_channel_limit, k_modifier_channel_limit);
    totals.hydration = std::clamp(totals.hydration, -k_modifier_channel_limit, k_modifier_channel_limit);
    totals.nourishment =
        std::clamp(totals.nourishment, -k_modifier_channel_limit, k_modifier_channel_limit);
    totals.energy_cap =
        std::clamp(totals.energy_cap, -k_modifier_channel_limit, k_modifier_channel_limit);
    totals.energy = std::clamp(totals.energy, -k_modifier_channel_limit, k_modifier_channel_limit);
    totals.morale = std::clamp(totals.morale, -k_modifier_channel_limit, k_modifier_channel_limit);
    totals.work_efficiency =
        std::clamp(totals.work_efficiency, -k_modifier_channel_limit, k_modifier_channel_limit);
    return totals;
}

ModifierChannelTotals apply_nearby_aura_modifier(ModifierId id) noexcept
{
    if (id.value == 0U)
    {
        return {};
    }

    const auto bucket = static_cast<std::size_t>(id.value) % k_nearby_aura_presets.size();
    return k_nearby_aura_presets[bucket];
}

ModifierChannelTotals apply_run_modifier(ModifierId id) noexcept
{
    if (id.value == 0U)
    {
        return {};
    }

    const auto bucket = static_cast<std::size_t>(id.value) % k_run_modifier_presets.size();
    return k_run_modifier_presets[bucket];
}

ModifierChannelTotals camp_comfort_bias(const CampState& camp) noexcept
{
    const float clamped_durability = std::clamp(camp.camp_durability, 0.0f, 100.0f);
    const float normalized = (clamped_durability * 0.01f * 2.0f) - 1.0f;

    ModifierChannelTotals totals {};
    totals.morale = normalized * 0.12f;
    totals.work_efficiency = normalized * 0.06f;
    return totals;
}

bool totals_match(const ModifierChannelTotals& lhs, const ModifierChannelTotals& rhs) noexcept
{
    return std::fabs(lhs.heat - rhs.heat) <= k_modifier_change_epsilon &&
        std::fabs(lhs.wind - rhs.wind) <= k_modifier_change_epsilon &&
        std::fabs(lhs.dust - rhs.dust) <= k_modifier_change_epsilon &&
        std::fabs(lhs.moisture - rhs.moisture) <= k_modifier_change_epsilon &&
        std::fabs(lhs.fertility - rhs.fertility) <= k_modifier_change_epsilon &&
        std::fabs(lhs.salinity - rhs.salinity) <= k_modifier_change_epsilon &&
        std::fabs(lhs.growth_pressure - rhs.growth_pressure) <= k_modifier_change_epsilon &&
        std::fabs(lhs.salinity_density_cap - rhs.salinity_density_cap) <= k_modifier_change_epsilon &&
        std::fabs(lhs.plant_density - rhs.plant_density) <= k_modifier_change_epsilon &&
        std::fabs(lhs.health - rhs.health) <= k_modifier_change_epsilon &&
        std::fabs(lhs.hydration - rhs.hydration) <= k_modifier_change_epsilon &&
        std::fabs(lhs.nourishment - rhs.nourishment) <= k_modifier_change_epsilon &&
        std::fabs(lhs.energy_cap - rhs.energy_cap) <= k_modifier_change_epsilon &&
        std::fabs(lhs.energy - rhs.energy) <= k_modifier_change_epsilon &&
        std::fabs(lhs.morale - rhs.morale) <= k_modifier_change_epsilon &&
        std::fabs(lhs.work_efficiency - rhs.work_efficiency) <= k_modifier_change_epsilon;
}

bool contains_modifier_id(
    const std::vector<ModifierId>& ids,
    ModifierId id) noexcept
{
    return std::find(ids.begin(), ids.end(), id) != ids.end();
}

void append_modifier_if_present(
    std::vector<ModifierId>& destination,
    ModifierId id)
{
    if (id.value == 0U || contains_modifier_id(destination, id))
    {
        return;
    }

    destination.push_back(id);
}

void import_campaign_run_modifiers(
    const CampaignState& campaign,
    ModifierState& modifier_state)
{
    for (const auto& faction_progress : campaign.faction_progress)
    {
        if (!faction_progress.has_unlocked_assistant_package)
        {
            continue;
        }

        append_modifier_if_present(
            modifier_state.active_run_modifier_ids,
            ModifierId {faction_progress.unlocked_assistant_package_id});
    }

    for (const auto tech_node_id : campaign.technology_state.purchased_node_ids)
    {
        const auto* node_def = find_technology_node_def(tech_node_id);
        if (node_def == nullptr ||
            node_def->entry_kind != TechnologyEntryKind::Amplification)
        {
            continue;
        }

        append_modifier_if_present(
            modifier_state.active_run_modifier_ids,
            node_def->linked_modifier_id);
    }
}

ModifierChannelTotals resolve_owned_totals(
    const ModifierState& modifier_state,
    const CampState& camp) noexcept
{
    ModifierChannelTotals totals {};

    for (const auto modifier_id : modifier_state.active_nearby_aura_modifier_ids)
    {
        accumulate_totals(totals, apply_nearby_aura_modifier(modifier_id));
    }

    for (const auto modifier_id : modifier_state.active_run_modifier_ids)
    {
        accumulate_totals(totals, apply_run_modifier(modifier_id));
    }

    accumulate_totals(totals, camp_comfort_bias(camp));
    return clamp_totals(totals);
}

void resolve_modifier_totals(SiteSystemContext<ModifierSystem>& context)
{
    const auto next_totals =
        resolve_owned_totals(context.world.read_modifier(), context.world.read_camp());
    auto& current_totals = context.world.own_modifier().resolved_channel_totals;

    if (!totals_match(current_totals, next_totals))
    {
        current_totals = next_totals;
        context.world.mark_projection_dirty(SITE_PROJECTION_UPDATE_HUD);
    }
}

void handle_site_run_started(
    SiteSystemContext<ModifierSystem>& context,
    const SiteRunStartedMessage& /*payload*/) noexcept
{
    auto& modifier_state = context.world.own_modifier();
    modifier_state.active_run_modifier_ids.clear();
    modifier_state.active_nearby_aura_modifier_ids.clear();
    modifier_state.resolved_channel_totals = {};

    const auto& aura_ids = context.campaign.loadout_planner_state.active_nearby_aura_modifier_ids;
    modifier_state.active_nearby_aura_modifier_ids.insert(
        modifier_state.active_nearby_aura_modifier_ids.end(),
        aura_ids.begin(),
        aura_ids.end());
    import_campaign_run_modifiers(context.campaign, modifier_state);

    resolve_modifier_totals(context);
}
}  // namespace

bool ModifierSystem::subscribes_to(GameMessageType type) noexcept
{
    return type == GameMessageType::SiteRunStarted;
}

Gs1Status ModifierSystem::process_message(
    SiteSystemContext<ModifierSystem>& context,
    const GameMessage& message)
{
    if (message.type == GameMessageType::SiteRunStarted)
    {
        handle_site_run_started(context, message.payload_as<SiteRunStartedMessage>());
    }

    return GS1_STATUS_OK;
}

void ModifierSystem::run(SiteSystemContext<ModifierSystem>& context)
{
    resolve_modifier_totals(context);
}
}  // namespace gs1
