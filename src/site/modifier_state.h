#pragma once

#include "support/id_types.h"

#include <vector>

namespace gs1
{
struct ModifierChannelTotals final
{
    float heat {0.0f};
    float wind {0.0f};
    float dust {0.0f};
    float moisture {0.0f};
    float fertility {0.0f};
    float salinity {0.0f};
    float growth_pressure {0.0f};
    float salinity_density_cap {0.0f};
    float plant_density {0.0f};
    float health {0.0f};
    float hydration {0.0f};
    float nourishment {0.0f};
    float energy_cap {0.0f};
    float energy {0.0f};
    float morale {0.0f};
    float work_efficiency {0.0f};
    float timed_buff_cap_bias {0.0f};
};

struct TerrainFactorModifierState final
{
    float fertility_to_moisture_cap_weight {1.0f};
    float fertility_to_moisture_cap_bias {0.0f};
    float moisture_weight {1.0f};
    float moisture_bias {0.0f};
    float heat_to_moisture_weight {1.0f};
    float heat_to_moisture_bias {0.0f};
    float wind_to_moisture_weight {1.0f};
    float wind_to_moisture_bias {0.0f};

    float salinity_to_fertility_cap_weight {1.0f};
    float salinity_to_fertility_cap_bias {0.0f};
    float fertility_weight {1.0f};
    float fertility_bias {0.0f};
    float wind_to_fertility_weight {1.0f};
    float wind_to_fertility_bias {0.0f};
    float dust_to_fertility_weight {1.0f};
    float dust_to_fertility_bias {0.0f};

    float salinity_source_weight {1.0f};
    float salinity_source_bias {0.0f};
    float salinity_reduction_weight {1.0f};
    float salinity_reduction_bias {0.0f};
};

struct ActionCostModifierState final
{
    float hydration_weight_delta {0.0f};
    float hydration_bias {0.0f};
    float nourishment_weight_delta {0.0f};
    float nourishment_bias {0.0f};
    float energy_weight_delta {0.0f};
    float energy_bias {0.0f};
    float morale_weight_delta {0.0f};
    float morale_bias {0.0f};
};

struct HarvestOutputModifierState final
{
    float cash_point_multiplier_delta {0.0f};
};

enum class HarvestBonusTier : std::uint8_t
{
    None = 0,
    Tier1 = 1,
    Tier2 = 2,
    Tier3 = 3
};

struct VillageTechnologyEffectState final
{
    float shovel_meter_cost_reduction {0.0f};
    float shovel_plant_duration_reduction {0.0f};
    float shovel_excavate_duration_reduction {0.0f};
    float careful_excavation_meter_cost_reduction {0.0f};
    float careful_excavation_duration_reduction {0.0f};
    float thorough_excavation_meter_cost_reduction {0.0f};
    float thorough_excavation_duration_reduction {0.0f};
    float weather_nourishment_hydration_loss_reduction {0.0f};
    float weather_health_morale_loss_reduction {0.0f};
    float timed_buff_effect_multiplier {1.0f};
    bool careful_excavation_loot_rebalance {false};
    bool thorough_excavation_loot_rebalance {false};
    bool tier_two_food_buffs_upgraded {false};
    bool tier_five_food_buffs_upgraded {false};
    bool tier_eight_food_buffs_upgraded {false};
};

struct BureauTechnologyEffectState final
{
    HarvestBonusTier unlocked_harvest_bonus_tier {HarvestBonusTier::None};
    std::uint8_t reserved0[3] {};
    float harvest_bonus_proc_chance_percent {0.0f};
    float harvest_bonus_higher_tier_bias_percent {0.0f};
};

struct ActiveSiteModifierState final
{
    ModifierId modifier_id {};
    ItemId source_item_id {};
    double duration_world_minutes {0.0};
    double remaining_world_minutes {0.0};
    ModifierChannelTotals totals {};
    ActionCostModifierState action_cost_modifiers {};
    HarvestOutputModifierState harvest_output_modifiers {};
};

struct ModifierMetaState final
{
    ModifierChannelTotals resolved_channel_totals {};
    TerrainFactorModifierState resolved_terrain_factor_modifiers {};
    ActionCostModifierState resolved_action_cost_modifiers {};
    HarvestOutputModifierState resolved_harvest_output_modifiers {};
    VillageTechnologyEffectState resolved_village_technology_effects {};
    BureauTechnologyEffectState resolved_bureau_technology_effects {};
};

struct ModifierState final
{
    std::vector<ModifierId> active_nearby_aura_modifier_ids {};
    std::vector<ActiveSiteModifierState> active_site_modifiers {};
    ModifierChannelTotals resolved_channel_totals {};
    TerrainFactorModifierState resolved_terrain_factor_modifiers {};
    ActionCostModifierState resolved_action_cost_modifiers {};
    HarvestOutputModifierState resolved_harvest_output_modifiers {};
    VillageTechnologyEffectState resolved_village_technology_effects {};
    BureauTechnologyEffectState resolved_bureau_technology_effects {};
};
}  // namespace gs1
