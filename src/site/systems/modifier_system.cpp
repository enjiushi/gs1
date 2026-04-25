#include "site/systems/modifier_system.h"

#include "campaign/campaign_state.h"
#include "content/defs/gameplay_tuning_defs.h"
#include "content/defs/modifier_defs.h"
#include "content/defs/technology_defs.h"
#include "site/site_projection_update_flags.h"
#include "site/site_run_state.h"

#include <algorithm>
#include <cmath>

namespace gs1
{
namespace
{
constexpr float k_modifier_change_epsilon = 1e-4f;

const ModifierSystemTuning& modifier_system_tuning() noexcept
{
    return gameplay_tuning_def().modifier_system;
}

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
    const auto& tuning = modifier_system_tuning();
    const float limit = tuning.modifier_channel_limit;
    totals.heat = std::clamp(totals.heat, -limit, limit);
    totals.wind = std::clamp(totals.wind, -limit, limit);
    totals.dust = std::clamp(totals.dust, -limit, limit);
    totals.moisture = std::clamp(totals.moisture, -limit, limit);
    totals.fertility = std::clamp(totals.fertility, -limit, limit);
    totals.salinity = std::clamp(totals.salinity, -limit, limit);
    totals.growth_pressure =
        std::clamp(totals.growth_pressure, -limit, limit);
    totals.salinity_density_cap =
        std::clamp(totals.salinity_density_cap, -limit, limit);
    totals.plant_density = std::clamp(totals.plant_density, -limit, limit);
    totals.health = std::clamp(totals.health, -limit, limit);
    totals.hydration = std::clamp(totals.hydration, -limit, limit);
    totals.nourishment = std::clamp(totals.nourishment, -limit, limit);
    totals.energy_cap = std::clamp(totals.energy_cap, -limit, limit);
    totals.energy = std::clamp(totals.energy, -limit, limit);
    totals.morale = std::clamp(totals.morale, -limit, limit);
    totals.work_efficiency =
        std::clamp(totals.work_efficiency, -limit, limit);
    return totals;
}

ModifierChannelTotals camp_comfort_bias(const CampState& camp) noexcept
{
    const auto& tuning = modifier_system_tuning();
    const float clamped_durability = std::clamp(camp.camp_durability, 0.0f, 100.0f);
    const float normalized = (clamped_durability * 0.01f * 2.0f) - 1.0f;

    ModifierChannelTotals totals {};
    totals.morale = normalized * tuning.camp_comfort_morale_scale;
    totals.work_efficiency = normalized * tuning.camp_comfort_work_efficiency_scale;
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

TerrainFactorModifierState resolve_terrain_factor_modifiers(
    const ModifierChannelTotals& totals) noexcept
{
    const auto& tuning = modifier_system_tuning();
    TerrainFactorModifierState factors {};

    factors.fertility_to_moisture_cap_weight =
        std::clamp(1.0f + totals.moisture, 0.0f, tuning.factor_weight_limit);
    factors.moisture_weight =
        std::clamp(1.0f + totals.moisture, 0.0f, tuning.factor_weight_limit);
    factors.heat_to_moisture_weight =
        std::clamp(1.0f - totals.moisture, 0.0f, tuning.factor_weight_limit);
    factors.wind_to_moisture_weight =
        std::clamp(1.0f - totals.moisture, 0.0f, tuning.factor_weight_limit);

    factors.salinity_to_fertility_cap_weight =
        std::clamp(1.0f - totals.salinity, 0.0f, tuning.factor_weight_limit);
    factors.fertility_weight =
        std::clamp(1.0f + totals.fertility, 0.0f, tuning.factor_weight_limit);
    factors.wind_to_fertility_weight =
        std::clamp(1.0f - totals.fertility, 0.0f, tuning.factor_weight_limit);
    factors.dust_to_fertility_weight =
        std::clamp(1.0f - totals.fertility, 0.0f, tuning.factor_weight_limit);

    factors.salinity_source_weight =
        std::clamp(1.0f - totals.salinity, 0.0f, tuning.factor_weight_limit);
    factors.salinity_reduction_weight =
        std::clamp(1.0f + totals.salinity, 0.0f, tuning.factor_weight_limit);

    factors.moisture_bias =
        std::clamp(totals.moisture, -tuning.factor_bias_limit, tuning.factor_bias_limit);
    factors.fertility_bias =
        std::clamp(totals.fertility, -tuning.factor_bias_limit, tuning.factor_bias_limit);
    factors.salinity_reduction_bias =
        std::clamp(totals.salinity, -tuning.factor_bias_limit, tuning.factor_bias_limit);
    factors.salinity_source_bias =
        std::clamp(-totals.salinity, -tuning.factor_bias_limit, tuning.factor_bias_limit);

    return factors;
}

bool terrain_factors_match(
    const TerrainFactorModifierState& lhs,
    const TerrainFactorModifierState& rhs) noexcept
{
    return
        std::fabs(lhs.fertility_to_moisture_cap_weight - rhs.fertility_to_moisture_cap_weight) <=
            k_modifier_change_epsilon &&
        std::fabs(lhs.fertility_to_moisture_cap_bias - rhs.fertility_to_moisture_cap_bias) <=
            k_modifier_change_epsilon &&
        std::fabs(lhs.moisture_weight - rhs.moisture_weight) <= k_modifier_change_epsilon &&
        std::fabs(lhs.moisture_bias - rhs.moisture_bias) <= k_modifier_change_epsilon &&
        std::fabs(lhs.heat_to_moisture_weight - rhs.heat_to_moisture_weight) <=
            k_modifier_change_epsilon &&
        std::fabs(lhs.heat_to_moisture_bias - rhs.heat_to_moisture_bias) <=
            k_modifier_change_epsilon &&
        std::fabs(lhs.wind_to_moisture_weight - rhs.wind_to_moisture_weight) <=
            k_modifier_change_epsilon &&
        std::fabs(lhs.wind_to_moisture_bias - rhs.wind_to_moisture_bias) <=
            k_modifier_change_epsilon &&
        std::fabs(lhs.salinity_to_fertility_cap_weight - rhs.salinity_to_fertility_cap_weight) <=
            k_modifier_change_epsilon &&
        std::fabs(lhs.salinity_to_fertility_cap_bias - rhs.salinity_to_fertility_cap_bias) <=
            k_modifier_change_epsilon &&
        std::fabs(lhs.fertility_weight - rhs.fertility_weight) <= k_modifier_change_epsilon &&
        std::fabs(lhs.fertility_bias - rhs.fertility_bias) <= k_modifier_change_epsilon &&
        std::fabs(lhs.wind_to_fertility_weight - rhs.wind_to_fertility_weight) <=
            k_modifier_change_epsilon &&
        std::fabs(lhs.wind_to_fertility_bias - rhs.wind_to_fertility_bias) <=
            k_modifier_change_epsilon &&
        std::fabs(lhs.dust_to_fertility_weight - rhs.dust_to_fertility_weight) <=
            k_modifier_change_epsilon &&
        std::fabs(lhs.dust_to_fertility_bias - rhs.dust_to_fertility_bias) <=
            k_modifier_change_epsilon &&
        std::fabs(lhs.salinity_source_weight - rhs.salinity_source_weight) <=
            k_modifier_change_epsilon &&
        std::fabs(lhs.salinity_source_bias - rhs.salinity_source_bias) <=
            k_modifier_change_epsilon &&
        std::fabs(lhs.salinity_reduction_weight - rhs.salinity_reduction_weight) <=
            k_modifier_change_epsilon &&
        std::fabs(lhs.salinity_reduction_bias - rhs.salinity_reduction_bias) <=
            k_modifier_change_epsilon;
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

    for (const auto& purchase : campaign.technology_state.purchased_nodes)
    {
        const auto* node_def = find_technology_node_def(purchase.tech_node_id);
        if (node_def == nullptr ||
            node_def->entry_kind != TechnologyEntryKind::GlobalModifier)
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
        accumulate_totals(totals, resolve_nearby_aura_modifier_preset(modifier_id));
    }

    for (const auto modifier_id : modifier_state.active_run_modifier_ids)
    {
        accumulate_totals(totals, resolve_run_modifier_preset(modifier_id));
    }

    accumulate_totals(totals, camp_comfort_bias(camp));
    return clamp_totals(totals);
}

void resolve_modifier_totals(SiteSystemContext<ModifierSystem>& context)
{
    const auto next_totals =
        resolve_owned_totals(context.world.read_modifier(), context.world.read_camp());
    auto& current_totals = context.world.own_modifier().resolved_channel_totals;
    const auto next_terrain_factors = resolve_terrain_factor_modifiers(next_totals);
    auto& current_terrain_factors = context.world.own_modifier().resolved_terrain_factor_modifiers;

    if (!totals_match(current_totals, next_totals) ||
        !terrain_factors_match(current_terrain_factors, next_terrain_factors))
    {
        current_totals = next_totals;
        current_terrain_factors = next_terrain_factors;
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
    modifier_state.resolved_terrain_factor_modifiers = {};

    const auto& aura_ids = context.campaign.loadout_planner_state.active_nearby_aura_modifier_ids;
    modifier_state.active_nearby_aura_modifier_ids.insert(
        modifier_state.active_nearby_aura_modifier_ids.end(),
        aura_ids.begin(),
        aura_ids.end());
    import_campaign_run_modifiers(context.campaign, modifier_state);

    resolve_modifier_totals(context);
}

void handle_run_modifier_award_requested(
    SiteSystemContext<ModifierSystem>& context,
    const RunModifierAwardRequestedMessage& payload) noexcept
{
    if (payload.modifier_id == 0U)
    {
        return;
    }

    auto& modifier_state = context.world.own_modifier();
    const ModifierId modifier_id {payload.modifier_id};
    if (std::find(
            modifier_state.active_run_modifier_ids.begin(),
            modifier_state.active_run_modifier_ids.end(),
            modifier_id) == modifier_state.active_run_modifier_ids.end())
    {
        modifier_state.active_run_modifier_ids.push_back(modifier_id);
        resolve_modifier_totals(context);
    }
}
}  // namespace

bool ModifierSystem::subscribes_to(GameMessageType type) noexcept
{
    return type == GameMessageType::SiteRunStarted ||
        type == GameMessageType::RunModifierAwardRequested;
}

Gs1Status ModifierSystem::process_message(
    SiteSystemContext<ModifierSystem>& context,
    const GameMessage& message)
{
    if (message.type == GameMessageType::SiteRunStarted)
    {
        handle_site_run_started(context, message.payload_as<SiteRunStartedMessage>());
    }
    else if (message.type == GameMessageType::RunModifierAwardRequested)
    {
        handle_run_modifier_award_requested(
            context,
            message.payload_as<RunModifierAwardRequestedMessage>());
    }

    return GS1_STATUS_OK;
}

void ModifierSystem::run(SiteSystemContext<ModifierSystem>& context)
{
    resolve_modifier_totals(context);
}
}  // namespace gs1
