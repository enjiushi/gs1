#include "site/systems/modifier_system.h"

#include "campaign/campaign_state.h"
#include "campaign/systems/technology_system.h"
#include "content/defs/gameplay_tuning_defs.h"
#include "content/defs/item_defs.h"
#include "content/defs/modifier_defs.h"
#include "content/defs/technology_defs.h"
#include "runtime/runtime_clock.h"
#include "site/site_projection_update_flags.h"
#include "site/site_run_state.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace gs1
{
namespace
{
constexpr float k_modifier_change_epsilon = 1e-4f;
constexpr double k_timed_buff_block_world_minutes = 480.0;
constexpr FactionId k_village_faction {k_faction_village_committee};
constexpr TechNodeId k_village_t1 {base_technology_node_id(k_village_faction, 1U)};
constexpr TechNodeId k_village_t2 {base_technology_node_id(k_village_faction, 2U)};
constexpr TechNodeId k_village_t3 {base_technology_node_id(k_village_faction, 3U)};
constexpr TechNodeId k_village_t4 {base_technology_node_id(k_village_faction, 4U)};
constexpr TechNodeId k_village_t5 {base_technology_node_id(k_village_faction, 5U)};
constexpr TechNodeId k_village_t6 {base_technology_node_id(k_village_faction, 6U)};
constexpr TechNodeId k_village_t7 {base_technology_node_id(k_village_faction, 7U)};
constexpr TechNodeId k_village_t8 {base_technology_node_id(k_village_faction, 8U)};
constexpr TechNodeId k_village_t9 {base_technology_node_id(k_village_faction, 9U)};
constexpr TechNodeId k_village_t10 {base_technology_node_id(k_village_faction, 10U)};
constexpr TechNodeId k_village_t11 {base_technology_node_id(k_village_faction, 11U)};
constexpr TechNodeId k_village_t12 {base_technology_node_id(k_village_faction, 12U)};
constexpr TechNodeId k_village_t13 {base_technology_node_id(k_village_faction, 13U)};
constexpr TechNodeId k_village_t14 {base_technology_node_id(k_village_faction, 14U)};
constexpr TechNodeId k_village_t15 {base_technology_node_id(k_village_faction, 15U)};
constexpr TechNodeId k_village_t16 {base_technology_node_id(k_village_faction, 16U)};
constexpr TechNodeId k_village_t17 {base_technology_node_id(k_village_faction, 17U)};
constexpr TechNodeId k_village_t18 {base_technology_node_id(k_village_faction, 18U)};
constexpr TechNodeId k_village_t19 {base_technology_node_id(k_village_faction, 19U)};
constexpr TechNodeId k_village_t20 {base_technology_node_id(k_village_faction, 20U)};
constexpr TechNodeId k_village_t21 {base_technology_node_id(k_village_faction, 21U)};
constexpr TechNodeId k_village_t22 {base_technology_node_id(k_village_faction, 22U)};
constexpr TechNodeId k_village_t23 {base_technology_node_id(k_village_faction, 23U)};
constexpr TechNodeId k_village_t24 {base_technology_node_id(k_village_faction, 24U)};
constexpr TechNodeId k_village_t25 {base_technology_node_id(k_village_faction, 25U)};
constexpr TechNodeId k_village_t26 {base_technology_node_id(k_village_faction, 26U)};
constexpr TechNodeId k_village_t27 {base_technology_node_id(k_village_faction, 27U)};
constexpr TechNodeId k_village_t28 {base_technology_node_id(k_village_faction, 28U)};
constexpr TechNodeId k_village_t29 {base_technology_node_id(k_village_faction, 29U)};
constexpr TechNodeId k_village_t30 {base_technology_node_id(k_village_faction, 30U)};
constexpr TechNodeId k_village_t31 {base_technology_node_id(k_village_faction, 31U)};
constexpr TechNodeId k_village_t32 {base_technology_node_id(k_village_faction, 32U)};

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
    destination.timed_buff_cap_bias += source.timed_buff_cap_bias;
}

void accumulate_action_cost_modifiers(
    ActionCostModifierState& destination,
    const ActionCostModifierState& source) noexcept
{
    destination.hydration_weight_delta += source.hydration_weight_delta;
    destination.hydration_bias += source.hydration_bias;
    destination.nourishment_weight_delta += source.nourishment_weight_delta;
    destination.nourishment_bias += source.nourishment_bias;
    destination.energy_weight_delta += source.energy_weight_delta;
    destination.energy_bias += source.energy_bias;
    destination.morale_weight_delta += source.morale_weight_delta;
    destination.morale_bias += source.morale_bias;
}

void accumulate_harvest_output_modifiers(
    HarvestOutputModifierState& destination,
    const HarvestOutputModifierState& source) noexcept
{
    destination.cash_point_multiplier_delta += source.cash_point_multiplier_delta;
}

ActionCostModifierState scale_action_cost_modifiers(
    const ActionCostModifierState& modifiers,
    float scale) noexcept
{
    ActionCostModifierState scaled = modifiers;
    scaled.hydration_weight_delta *= scale;
    scaled.hydration_bias *= scale;
    scaled.nourishment_weight_delta *= scale;
    scaled.nourishment_bias *= scale;
    scaled.energy_weight_delta *= scale;
    scaled.energy_bias *= scale;
    scaled.morale_weight_delta *= scale;
    scaled.morale_bias *= scale;
    return scaled;
}

HarvestOutputModifierState scale_harvest_output_modifiers(
    const HarvestOutputModifierState& modifiers,
    float scale) noexcept
{
    HarvestOutputModifierState scaled = modifiers;
    scaled.cash_point_multiplier_delta *= scale;
    return scaled;
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

ModifierChannelTotals scale_totals(
    const ModifierChannelTotals& totals,
    float scale) noexcept
{
    ModifierChannelTotals scaled = totals;
    scaled.heat *= scale;
    scaled.wind *= scale;
    scaled.dust *= scale;
    scaled.moisture *= scale;
    scaled.fertility *= scale;
    scaled.salinity *= scale;
    scaled.growth_pressure *= scale;
    scaled.salinity_density_cap *= scale;
    scaled.plant_density *= scale;
    scaled.health *= scale;
    scaled.hydration *= scale;
    scaled.nourishment *= scale;
    scaled.energy_cap *= scale;
    scaled.energy *= scale;
    scaled.morale *= scale;
    scaled.work_efficiency *= scale;
    scaled.timed_buff_cap_bias *= scale;
    return scaled;
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
        std::fabs(lhs.work_efficiency - rhs.work_efficiency) <= k_modifier_change_epsilon &&
        std::fabs(lhs.timed_buff_cap_bias - rhs.timed_buff_cap_bias) <= k_modifier_change_epsilon;
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

bool action_cost_modifiers_match(
    const ActionCostModifierState& lhs,
    const ActionCostModifierState& rhs) noexcept
{
    return std::fabs(lhs.hydration_weight_delta - rhs.hydration_weight_delta) <=
            k_modifier_change_epsilon &&
        std::fabs(lhs.hydration_bias - rhs.hydration_bias) <=
            k_modifier_change_epsilon &&
        std::fabs(lhs.nourishment_weight_delta - rhs.nourishment_weight_delta) <=
            k_modifier_change_epsilon &&
        std::fabs(lhs.nourishment_bias - rhs.nourishment_bias) <=
            k_modifier_change_epsilon &&
        std::fabs(lhs.energy_weight_delta - rhs.energy_weight_delta) <=
            k_modifier_change_epsilon &&
        std::fabs(lhs.energy_bias - rhs.energy_bias) <=
            k_modifier_change_epsilon &&
        std::fabs(lhs.morale_weight_delta - rhs.morale_weight_delta) <=
            k_modifier_change_epsilon &&
        std::fabs(lhs.morale_bias - rhs.morale_bias) <=
            k_modifier_change_epsilon;
}

bool harvest_output_modifiers_match(
    const HarvestOutputModifierState& lhs,
    const HarvestOutputModifierState& rhs) noexcept
{
    return
        std::fabs(lhs.cash_point_multiplier_delta - rhs.cash_point_multiplier_delta) <=
        k_modifier_change_epsilon;
}

bool village_technology_effects_match(
    const VillageTechnologyEffectState& lhs,
    const VillageTechnologyEffectState& rhs) noexcept
{
    return
        std::fabs(lhs.shovel_meter_cost_reduction - rhs.shovel_meter_cost_reduction) <=
            k_modifier_change_epsilon &&
        std::fabs(lhs.shovel_plant_duration_reduction - rhs.shovel_plant_duration_reduction) <=
            k_modifier_change_epsilon &&
        std::fabs(
            lhs.shovel_excavate_duration_reduction - rhs.shovel_excavate_duration_reduction) <=
            k_modifier_change_epsilon &&
        std::fabs(
            lhs.careful_excavation_meter_cost_reduction -
            rhs.careful_excavation_meter_cost_reduction) <= k_modifier_change_epsilon &&
        std::fabs(
            lhs.careful_excavation_duration_reduction -
            rhs.careful_excavation_duration_reduction) <= k_modifier_change_epsilon &&
        std::fabs(
            lhs.thorough_excavation_meter_cost_reduction -
            rhs.thorough_excavation_meter_cost_reduction) <= k_modifier_change_epsilon &&
        std::fabs(
            lhs.thorough_excavation_duration_reduction -
            rhs.thorough_excavation_duration_reduction) <= k_modifier_change_epsilon &&
        std::fabs(
            lhs.weather_nourishment_hydration_loss_reduction -
            rhs.weather_nourishment_hydration_loss_reduction) <= k_modifier_change_epsilon &&
        std::fabs(
            lhs.weather_health_morale_loss_reduction -
            rhs.weather_health_morale_loss_reduction) <= k_modifier_change_epsilon &&
        std::fabs(lhs.timed_buff_effect_multiplier - rhs.timed_buff_effect_multiplier) <=
            k_modifier_change_epsilon &&
        lhs.careful_excavation_loot_rebalance == rhs.careful_excavation_loot_rebalance &&
        lhs.thorough_excavation_loot_rebalance == rhs.thorough_excavation_loot_rebalance &&
        lhs.tier_two_food_buffs_upgraded == rhs.tier_two_food_buffs_upgraded &&
        lhs.tier_five_food_buffs_upgraded == rhs.tier_five_food_buffs_upgraded &&
        lhs.tier_eight_food_buffs_upgraded == rhs.tier_eight_food_buffs_upgraded;
}

double modifier_duration_world_minutes(const ModifierDef& modifier_def) noexcept
{
    return static_cast<double>(modifier_def.duration_eight_hour_blocks) *
        k_timed_buff_block_world_minutes;
}

bool is_timed_modifier(const ActiveSiteModifierState& modifier) noexcept
{
    return modifier.duration_world_minutes > 0.0;
}

std::uint16_t projected_remaining_game_hours(double remaining_world_minutes) noexcept
{
    if (remaining_world_minutes <= k_modifier_change_epsilon)
    {
        return 0U;
    }

    const auto projected_hours = static_cast<std::uint32_t>(std::ceil(
        std::max(0.0, remaining_world_minutes - static_cast<double>(k_modifier_change_epsilon)) /
        60.0));
    return static_cast<std::uint16_t>(std::min<std::uint32_t>(
        projected_hours,
        std::numeric_limits<std::uint16_t>::max()));
}

std::uint32_t count_active_timed_buffs(const ModifierState& modifier_state) noexcept
{
    return static_cast<std::uint32_t>(std::count_if(
        modifier_state.active_site_modifiers.begin(),
        modifier_state.active_site_modifiers.end(),
        [](const ActiveSiteModifierState& modifier) {
            return is_timed_modifier(modifier);
        }));
}

std::int32_t discrete_timed_buff_cap_bias(float bias) noexcept
{
    if (bias >= 0.0f)
    {
        return static_cast<std::int32_t>(std::floor(static_cast<double>(bias) + 1e-4));
    }

    return static_cast<std::int32_t>(std::ceil(static_cast<double>(bias) - 1e-4));
}

std::uint32_t resolved_active_timed_buff_cap(const ModifierState& modifier_state) noexcept
{
    const auto& tuning = modifier_system_tuning();
    const std::int32_t base_cap = static_cast<std::int32_t>(tuning.active_timed_buff_cap);
    const std::int32_t cap_bias =
        discrete_timed_buff_cap_bias(modifier_state.resolved_channel_totals.timed_buff_cap_bias);
    return static_cast<std::uint32_t>(std::max(0, base_cap + cap_bias));
}

ActiveSiteModifierState make_active_modifier_state(
    const ModifierDef& modifier_def,
    ItemId source_item_id,
    float effect_scale = 1.0f) noexcept
{
    ActiveSiteModifierState modifier {};
    modifier.modifier_id = modifier_def.modifier_id;
    modifier.source_item_id = source_item_id;
    modifier.duration_world_minutes = modifier_duration_world_minutes(modifier_def);
    modifier.remaining_world_minutes = modifier.duration_world_minutes;
    modifier.totals = scale_totals(modifier_def.totals, effect_scale);
    modifier.action_cost_modifiers =
        scale_action_cost_modifiers(modifier_def.action_cost_modifiers, effect_scale);
    modifier.harvest_output_modifiers =
        scale_harvest_output_modifiers(modifier_def.harvest_output_modifiers, effect_scale);
    return modifier;
}

std::vector<ActiveSiteModifierState>::iterator find_active_modifier(
    ModifierState& modifier_state,
    ModifierId modifier_id) noexcept
{
    return std::find_if(
        modifier_state.active_site_modifiers.begin(),
        modifier_state.active_site_modifiers.end(),
        [&](const ActiveSiteModifierState& modifier) {
            return modifier.modifier_id == modifier_id;
        });
}

bool apply_modifier(
    ModifierState& modifier_state,
    const ModifierDef& modifier_def,
    ItemId source_item_id,
    float effect_scale = 1.0f) noexcept
{
    if (modifier_def.modifier_id.value == 0U)
    {
        return false;
    }

    auto existing_it = find_active_modifier(modifier_state, modifier_def.modifier_id);
    if (existing_it != modifier_state.active_site_modifiers.end())
    {
        *existing_it = make_active_modifier_state(modifier_def, source_item_id, effect_scale);
        return true;
    }

    const double duration_world_minutes = modifier_duration_world_minutes(modifier_def);
    if (duration_world_minutes > 0.0 &&
        count_active_timed_buffs(modifier_state) >= resolved_active_timed_buff_cap(modifier_state))
    {
        return false;
    }

    modifier_state.active_site_modifiers.push_back(
        make_active_modifier_state(modifier_def, source_item_id, effect_scale));
    return true;
}

VillageTechnologyEffectState resolve_village_technology_effects(
    const CampaignState& campaign) noexcept
{
    VillageTechnologyEffectState effects {};

    if (TechnologySystem::node_purchased(campaign, k_village_t1))
    {
        effects.shovel_meter_cost_reduction += 0.05f;
    }
    if (TechnologySystem::node_purchased(campaign, k_village_t2))
    {
        // First timed-buff slot comes from the linked GlobalModifier preset.
    }
    if (TechnologySystem::node_purchased(campaign, k_village_t3))
    {
        effects.shovel_plant_duration_reduction += 0.05f;
    }
    if (TechnologySystem::node_purchased(campaign, k_village_t4))
    {
        // Worker-pack expansion resolves in site_run_factory.
    }
    if (TechnologySystem::node_purchased(campaign, k_village_t5))
    {
        // Careful-depth access resolves in action_execution_system.
    }
    if (TechnologySystem::node_purchased(campaign, k_village_t6))
    {
        effects.shovel_excavate_duration_reduction += 0.05f;
    }
    if (TechnologySystem::node_purchased(campaign, k_village_t7))
    {
        effects.weather_nourishment_hydration_loss_reduction += 0.15f;
    }
    if (TechnologySystem::node_purchased(campaign, k_village_t8))
    {
        effects.tier_two_food_buffs_upgraded = true;
    }
    if (TechnologySystem::node_purchased(campaign, k_village_t9))
    {
        effects.shovel_plant_duration_reduction += 0.05f;
    }
    if (TechnologySystem::node_purchased(campaign, k_village_t10))
    {
        effects.careful_excavation_loot_rebalance = true;
    }
    if (TechnologySystem::node_purchased(campaign, k_village_t11))
    {
        // Worker-pack expansion resolves in site_run_factory.
    }
    if (TechnologySystem::node_purchased(campaign, k_village_t12))
    {
        effects.weather_health_morale_loss_reduction += 0.15f;
    }
    if (TechnologySystem::node_purchased(campaign, k_village_t13))
    {
        effects.shovel_excavate_duration_reduction += 0.05f;
    }
    if (TechnologySystem::node_purchased(campaign, k_village_t14))
    {
        effects.weather_nourishment_hydration_loss_reduction += 0.15f;
    }
    if (TechnologySystem::node_purchased(campaign, k_village_t15))
    {
        effects.careful_excavation_meter_cost_reduction = 0.20f;
        effects.careful_excavation_duration_reduction = 0.30f;
    }
    if (TechnologySystem::node_purchased(campaign, k_village_t16))
    {
        effects.tier_five_food_buffs_upgraded = true;
    }

    if (TechnologySystem::node_purchased(campaign, k_village_t17))
    {
        effects.shovel_meter_cost_reduction += 0.05f;
    }
    if (TechnologySystem::node_purchased(campaign, k_village_t18))
    {
        effects.shovel_plant_duration_reduction += 0.05f;
    }
    if (TechnologySystem::node_purchased(campaign, k_village_t19))
    {
        // Second timed-buff slot comes from the linked GlobalModifier preset.
    }
    if (TechnologySystem::node_purchased(campaign, k_village_t21))
    {
        effects.shovel_meter_cost_reduction += 0.05f;
    }
    if (TechnologySystem::node_purchased(campaign, k_village_t22))
    {
        effects.tier_eight_food_buffs_upgraded = true;
    }
    if (TechnologySystem::node_purchased(campaign, k_village_t23))
    {
        effects.shovel_excavate_duration_reduction += 0.05f;
    }
    if (TechnologySystem::node_purchased(campaign, k_village_t24))
    {
        effects.weather_nourishment_hydration_loss_reduction += 0.20f;
    }
    if (TechnologySystem::node_purchased(campaign, k_village_t25))
    {
        effects.thorough_excavation_loot_rebalance = true;
    }
    if (TechnologySystem::node_purchased(campaign, k_village_t26))
    {
        effects.shovel_plant_duration_reduction += 0.20f;
    }
    if (TechnologySystem::node_purchased(campaign, k_village_t27))
    {
        effects.weather_health_morale_loss_reduction += 0.15f;
    }
    if (TechnologySystem::node_purchased(campaign, k_village_t28))
    {
        effects.timed_buff_effect_multiplier = 1.10f;
    }
    if (TechnologySystem::node_purchased(campaign, k_village_t29))
    {
        effects.shovel_plant_duration_reduction += 0.20f;
    }
    if (TechnologySystem::node_purchased(campaign, k_village_t30))
    {
        effects.thorough_excavation_meter_cost_reduction = 0.20f;
        effects.thorough_excavation_duration_reduction = 0.30f;
    }
    if (TechnologySystem::node_purchased(campaign, k_village_t31))
    {
        effects.weather_nourishment_hydration_loss_reduction += 0.20f;
    }
    if (TechnologySystem::node_purchased(campaign, k_village_t32))
    {
        effects.timed_buff_effect_multiplier = 1.25f;
    }
    return effects;
}

ModifierId custom_consumable_modifier_id(
    const VillageTechnologyEffectState& village_effects,
    ItemId item_id) noexcept
{
    switch (item_id.value)
    {
    case k_item_wormwood_broth:
    case k_item_rich_wormwood_broth:
        return village_effects.tier_two_food_buffs_upgraded
            ? ModifierId {3402U}
            : ModifierId {3401U};
    case k_item_thornberry_cooler:
    case k_item_rich_thornberry_cooler:
        return village_effects.tier_two_food_buffs_upgraded
            ? ModifierId {3404U}
            : ModifierId {3403U};
    case k_item_peashrub_hotpot:
    case k_item_rich_peashrub_hotpot:
        return village_effects.tier_five_food_buffs_upgraded
            ? ModifierId {3406U}
            : ModifierId {3405U};
    case k_item_buckthorn_tonic:
    case k_item_rich_buckthorn_tonic:
        return village_effects.tier_five_food_buffs_upgraded
            ? ModifierId {3408U}
            : ModifierId {3407U};
    case k_item_jadeleaf_stew:
    case k_item_rich_jadeleaf_stew:
        return village_effects.tier_eight_food_buffs_upgraded
            ? ModifierId {3410U}
            : ModifierId {3409U};
    case k_item_desert_revival_draught:
    case k_item_rich_desert_revival_draught:
        return village_effects.tier_eight_food_buffs_upgraded
            ? ModifierId {3412U}
            : ModifierId {3411U};
    default:
        return {};
    }
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

        const auto* modifier_def =
            find_modifier_def(ModifierId {faction_progress.unlocked_assistant_package_id});
        if (modifier_def == nullptr)
        {
            continue;
        }

        (void)apply_modifier(
            modifier_state,
            *modifier_def,
            ItemId {});
    }
}

void accumulate_campaign_technology_modifier_totals(
    const CampaignState& campaign,
    ModifierChannelTotals& totals) noexcept
{
    for (const auto& node_def : all_technology_node_defs())
    {
        if (node_def.entry_kind != TechnologyEntryKind::GlobalModifier ||
            !TechnologySystem::node_purchased(campaign, node_def.tech_node_id))
        {
            continue;
        }

        accumulate_totals(
            totals,
            scale_totals(
                resolve_run_modifier_preset(node_def.linked_modifier_id),
                TechnologySystem::current_effect_parameter(campaign, node_def)));
    }
}

void accumulate_active_site_modifier_totals(
    const ModifierState& modifier_state,
    ModifierChannelTotals& totals,
    ActionCostModifierState& action_cost_modifiers,
    HarvestOutputModifierState& harvest_output_modifiers) noexcept
{
    for (const auto& active_modifier : modifier_state.active_site_modifiers)
    {
        accumulate_totals(totals, active_modifier.totals);
        accumulate_action_cost_modifiers(
            action_cost_modifiers,
            active_modifier.action_cost_modifiers);
        accumulate_harvest_output_modifiers(
            harvest_output_modifiers,
            active_modifier.harvest_output_modifiers);
    }
}

void accumulate_campaign_technology_harvest_output_modifiers(
    const CampaignState& campaign,
    HarvestOutputModifierState& harvest_output_modifiers) noexcept
{
    for (const auto& node_def : all_technology_node_defs())
    {
        if (node_def.entry_kind != TechnologyEntryKind::GlobalModifier ||
            !TechnologySystem::node_purchased(campaign, node_def.tech_node_id))
        {
            continue;
        }

        const auto* modifier_def = find_modifier_def(node_def.linked_modifier_id);
        if (modifier_def == nullptr)
        {
            continue;
        }

        accumulate_harvest_output_modifiers(
            harvest_output_modifiers,
            scale_harvest_output_modifiers(
                modifier_def->harvest_output_modifiers,
                TechnologySystem::current_effect_parameter(campaign, node_def)));
    }
}

struct TimedModifierTickResult final
{
    bool projection_changed {false};
    bool modifier_set_changed {false};
};

TimedModifierTickResult tick_timed_modifiers(
    ModifierState& modifier_state,
    double elapsed_world_minutes) noexcept
{
    if (modifier_state.active_site_modifiers.empty() || elapsed_world_minutes <= 0.0)
    {
        return {};
    }

    TimedModifierTickResult result {};
    for (auto& active_modifier : modifier_state.active_site_modifiers)
    {
        if (!is_timed_modifier(active_modifier))
        {
            continue;
        }

        const auto previous_hours =
            projected_remaining_game_hours(active_modifier.remaining_world_minutes);
        const double next_remaining =
            std::max(0.0, active_modifier.remaining_world_minutes - elapsed_world_minutes);
        const auto next_hours = projected_remaining_game_hours(next_remaining);
        result.projection_changed = result.projection_changed || previous_hours != next_hours;
        active_modifier.remaining_world_minutes = next_remaining;
    }

    const auto erase_begin = std::remove_if(
        modifier_state.active_site_modifiers.begin(),
        modifier_state.active_site_modifiers.end(),
        [](const ActiveSiteModifierState& active_modifier) {
            return is_timed_modifier(active_modifier) &&
                active_modifier.remaining_world_minutes <= 0.0;
        });
    if (erase_begin != modifier_state.active_site_modifiers.end())
    {
        modifier_state.active_site_modifiers.erase(
            erase_begin,
            modifier_state.active_site_modifiers.end());
        result.projection_changed = true;
        result.modifier_set_changed = true;
    }

    return result;
}

bool end_timed_modifier(
    ModifierState& modifier_state,
    ModifierId modifier_id) noexcept
{
    const auto active_it = find_active_modifier(modifier_state, modifier_id);
    if (active_it == modifier_state.active_site_modifiers.end() ||
        !is_timed_modifier(*active_it))
    {
        return false;
    }

    modifier_state.active_site_modifiers.erase(active_it);
    return true;
}

struct ResolvedModifierOutputs final
{
    ModifierChannelTotals totals {};
    ActionCostModifierState action_cost_modifiers {};
    HarvestOutputModifierState harvest_output_modifiers {};
    VillageTechnologyEffectState village_technology_effects {};
};

ResolvedModifierOutputs resolve_owned_modifiers(
    const CampaignState& campaign,
    const ModifierState& modifier_state,
    const CampState& camp) noexcept
{
    ResolvedModifierOutputs resolved {};

    for (const auto modifier_id : modifier_state.active_nearby_aura_modifier_ids)
    {
        accumulate_totals(resolved.totals, resolve_nearby_aura_modifier_preset(modifier_id));
        if (const auto* modifier_def = find_modifier_def(modifier_id))
        {
            accumulate_harvest_output_modifiers(
                resolved.harvest_output_modifiers,
                modifier_def->harvest_output_modifiers);
        }
    }

    accumulate_active_site_modifier_totals(
        modifier_state,
        resolved.totals,
        resolved.action_cost_modifiers,
        resolved.harvest_output_modifiers);
    accumulate_campaign_technology_modifier_totals(campaign, resolved.totals);
    accumulate_campaign_technology_harvest_output_modifiers(
        campaign,
        resolved.harvest_output_modifiers);
    resolved.village_technology_effects = resolve_village_technology_effects(campaign);
    accumulate_totals(resolved.totals, camp_comfort_bias(camp));
    resolved.totals = clamp_totals(resolved.totals);
    return resolved;
}

void resolve_modifier_totals(SiteSystemContext<ModifierSystem>& context)
{
    const auto next_outputs =
        resolve_owned_modifiers(context.campaign, context.world.read_modifier(), context.world.read_camp());
    auto& current_totals = context.world.own_modifier().resolved_channel_totals;
    const auto next_terrain_factors = resolve_terrain_factor_modifiers(next_outputs.totals);
    auto& current_terrain_factors = context.world.own_modifier().resolved_terrain_factor_modifiers;
    auto& current_action_cost_modifiers = context.world.own_modifier().resolved_action_cost_modifiers;
    auto& current_harvest_output_modifiers =
        context.world.own_modifier().resolved_harvest_output_modifiers;
    auto& current_village_effects = context.world.own_modifier().resolved_village_technology_effects;

    if (!totals_match(current_totals, next_outputs.totals) ||
        !terrain_factors_match(current_terrain_factors, next_terrain_factors) ||
        !action_cost_modifiers_match(
            current_action_cost_modifiers,
            next_outputs.action_cost_modifiers) ||
        !harvest_output_modifiers_match(
            current_harvest_output_modifiers,
            next_outputs.harvest_output_modifiers) ||
        !village_technology_effects_match(
            current_village_effects,
            next_outputs.village_technology_effects))
    {
        current_totals = next_outputs.totals;
        current_terrain_factors = next_terrain_factors;
        current_action_cost_modifiers = next_outputs.action_cost_modifiers;
        current_harvest_output_modifiers = next_outputs.harvest_output_modifiers;
        current_village_effects = next_outputs.village_technology_effects;
        context.world.mark_projection_dirty(SITE_PROJECTION_UPDATE_HUD);
    }
}

void handle_site_run_started(
    SiteSystemContext<ModifierSystem>& context,
    const SiteRunStartedMessage& /*payload*/) noexcept
{
    auto& modifier_state = context.world.own_modifier();
    modifier_state.active_nearby_aura_modifier_ids.clear();
    modifier_state.active_site_modifiers.clear();
    modifier_state.resolved_channel_totals = {};
    modifier_state.resolved_terrain_factor_modifiers = {};
    modifier_state.resolved_action_cost_modifiers = {};
    modifier_state.resolved_harvest_output_modifiers = {};
    modifier_state.resolved_village_technology_effects = {};

    const auto& aura_ids = context.campaign.loadout_planner_state.active_nearby_aura_modifier_ids;
    modifier_state.active_nearby_aura_modifier_ids.insert(
        modifier_state.active_nearby_aura_modifier_ids.end(),
        aura_ids.begin(),
        aura_ids.end());
    import_campaign_run_modifiers(context.campaign, modifier_state);

    context.world.mark_projection_dirty(SITE_PROJECTION_UPDATE_MODIFIERS);
    resolve_modifier_totals(context);
}

void handle_inventory_item_use_completed(
    SiteSystemContext<ModifierSystem>& context,
    const InventoryItemUseCompletedMessage& payload) noexcept
{
    const ItemId item_id {payload.item_id};
    const auto* item_def = find_item_def(item_id);
    if (item_def == nullptr)
    {
        return;
    }

    const ModifierId resolved_modifier_id =
        item_def->modifier_id.value != 0U
        ? item_def->modifier_id
        : custom_consumable_modifier_id(
            resolve_village_technology_effects(context.campaign),
            item_id);
    if (resolved_modifier_id.value == 0U)
    {
        return;
    }

    const auto* modifier_def = find_modifier_def(resolved_modifier_id);
    if (modifier_def == nullptr ||
        modifier_def->preset_kind != ModifierPresetKind::RunModifier)
    {
        return;
    }

    const float effect_scale =
        modifier_def->duration_eight_hour_blocks == 0U
        ? 1.0f
        : resolve_village_technology_effects(context.campaign).timed_buff_effect_multiplier;

    if (apply_modifier(
            context.world.own_modifier(),
            *modifier_def,
            item_def->item_id,
            effect_scale))
    {
        context.world.mark_projection_dirty(SITE_PROJECTION_UPDATE_MODIFIERS);
        resolve_modifier_totals(context);
    }
}

void handle_run_modifier_award_requested(
    SiteSystemContext<ModifierSystem>& context,
    const RunModifierAwardRequestedMessage& payload) noexcept
{
    if (payload.modifier_id == 0U)
    {
        return;
    }

    const auto* modifier_def = find_modifier_def(ModifierId {payload.modifier_id});
    if (modifier_def == nullptr ||
        modifier_def->preset_kind != ModifierPresetKind::RunModifier)
    {
        return;
    }

    if (apply_modifier(
            context.world.own_modifier(),
            *modifier_def,
            ItemId {}))
    {
        context.world.mark_projection_dirty(SITE_PROJECTION_UPDATE_MODIFIERS);
        resolve_modifier_totals(context);
    }
}

void handle_site_modifier_end_requested(
    SiteSystemContext<ModifierSystem>& context,
    const SiteModifierEndRequestedMessage& payload) noexcept
{
    if (payload.modifier_id == 0U)
    {
        return;
    }

    if (end_timed_modifier(
            context.world.own_modifier(),
            ModifierId {payload.modifier_id}))
    {
        context.world.mark_projection_dirty(SITE_PROJECTION_UPDATE_MODIFIERS);
        resolve_modifier_totals(context);
    }
}
}  // namespace

bool ModifierSystem::subscribes_to(GameMessageType type) noexcept
{
    return type == GameMessageType::SiteRunStarted ||
        type == GameMessageType::RunModifierAwardRequested ||
        type == GameMessageType::InventoryItemUseCompleted ||
        type == GameMessageType::SiteModifierEndRequested;
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
    else if (message.type == GameMessageType::InventoryItemUseCompleted)
    {
        handle_inventory_item_use_completed(
            context,
            message.payload_as<InventoryItemUseCompletedMessage>());
    }
    else if (message.type == GameMessageType::SiteModifierEndRequested)
    {
        handle_site_modifier_end_requested(
            context,
            message.payload_as<SiteModifierEndRequestedMessage>());
    }

    return GS1_STATUS_OK;
}

void ModifierSystem::run(SiteSystemContext<ModifierSystem>& context)
{
    const auto tick_result = tick_timed_modifiers(
            context.world.own_modifier(),
            runtime_minutes_from_real_seconds(context.fixed_step_seconds));
    if (tick_result.projection_changed)
    {
        context.world.mark_projection_dirty(SITE_PROJECTION_UPDATE_MODIFIERS);
    }
    if (tick_result.modifier_set_changed)
    {
        resolve_modifier_totals(context);
        return;
    }

    resolve_modifier_totals(context);
}
}  // namespace gs1
