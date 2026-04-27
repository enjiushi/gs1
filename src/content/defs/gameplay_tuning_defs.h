#pragma once

#include <cstdint>

namespace gs1
{
struct WorkerConditionTuning final
{
    float hydration_base_loss_per_game_minute {0.0100f};
    float heat_to_hydration_factor {0.40666667f};
    float wind_to_hydration_factor {0.05083333f};
    float dust_to_hydration_factor {0.05083333f};
    float nourishment_base_loss_per_game_minute {0.00250f};
    float wind_to_nourishment_factor {0.00700f};
    float heat_to_nourishment_factor {0.00200f};
    float dust_to_nourishment_factor {0.00100f};
    float energy_background_increase_real_minutes {0.5f};
    float energy_background_min_speed_factor {0.2f};
    float morale_background_increase_real_minutes {3.0f};
    float morale_background_decrease_real_minutes {3.0f};
    float morale_support_real_minutes {2.0f};
    float heat_to_health_factor {0.20833333f};
    float wind_to_health_factor {0.20833333f};
    float dust_to_health_factor {0.10416667f};
    float health_energy_cap_factor {1.0f};
    float hydration_energy_cap_factor {1.0f};
    float nourishment_energy_cap_factor {1.0f};
    float energy_cap_factor {1.0f};
    float health_efficiency_factor {1.0f};
    float hydration_efficiency_factor {1.0f};
    float nourishment_efficiency_factor {1.0f};
    float morale_efficiency_factor {1.0f};
    float work_efficiency_factor {1.0f};
    float factor_weight_default {1.0f};
    float factor_bias_default {0.0f};
    float factor_min {0.0f};
    float factor_max {4.0f};
    float sheltered_exposure_scale {0.35f};
};

struct PlayerMeterCashPointTuning final
{
    float health_per_point {50.0f};
    float hydration_per_point {10.0f};
    float nourishment_per_point {25.0f};
    float energy_per_point {1.0f};
    float morale_per_point {35.0f};
    float buy_price_multiplier {1.1f};
    float sell_price_multiplier {0.9f};
};

struct EcologyTuning final
{
    float moisture_gain_per_water_unit {22.0f};
    float fertility_to_moisture_cap_factor {1.0f};
    float salinity_to_fertility_cap_factor {1.0f};
    float moisture_factor {0.02f};
    float heat_to_moisture_factor {0.00025f};
    float wind_to_moisture_factor {0.00018f};
    float fertility_factor {0.015f};
    float wind_to_fertility_factor {0.00008f};
    float dust_to_fertility_factor {0.00006f};
    float salinity_factor {0.012f};
    float salinity_source {0.0f};
    float growth_relief_from_moisture {0.55f};
    float growth_relief_from_fertility {0.35f};
    float growth_pressure_heat_scale {0.01f};
    float growth_pressure_wind_scale {0.01f};
    float growth_pressure_dust_scale {0.01f};
    float growth_pressure_base {0.2f};
    float growth_pressure_heat_weight {0.9f};
    float growth_pressure_wind_weight {0.75f};
    float growth_pressure_dust_weight {0.65f};
    float growth_pressure_burial_weight {0.55f};
    float growth_pressure_salinity_weight {0.45f};
    float growth_pressure_dust_burial_scale {12.0f};
    float growth_pressure_modifier_influence {0.35f};
    float density_full_range_real_minutes {5.0f};
    float salinity_cap_softening {0.75f};
    float salinity_cap_min_unit {0.15f};
    float salinity_density_cap_modifier_influence {0.35f};
    float resistance_density_influence {0.35f};
    float tolerance_percent_scale {0.01f};
    float density_salinity_overcap_loss_scale {1.8f};
    float density_modifier_influence {0.35f};
    float constant_wither_rate_scale {0.08f};
    float highway_cover_gain_wind_scale {0.00028f};
    float highway_cover_gain_dust_scale {0.00045f};
};

struct ModifierSystemTuning final
{
    std::uint8_t active_timed_buff_cap {3U};
    std::uint8_t reserved0[3] {};
    float modifier_channel_limit {1.0f};
    float factor_weight_limit {2.5f};
    float factor_bias_limit {2.0f};
    float camp_comfort_morale_scale {0.12f};
    float camp_comfort_work_efficiency_scale {0.06f};
};

struct DeviceSupportTuning final
{
    float water_evaporation_base {0.0035f};
    float heat_evaporation_multiplier {0.0075f};
};

struct CampDurabilityTuning final
{
    float durability_max {100.0f};
    float base_wear_per_second {0.01f};
    float weather_wear_scale {0.00075f};
    float event_timeline_wear_per_second {0.04f};
    float peak_event_pressure_total {30.0f};
    float protection_threshold {70.0f};
    float delivery_threshold {50.0f};
    float shared_storage_threshold {30.0f};
};

struct GameplayTuningDef final
{
    WorkerConditionTuning worker_condition {};
    PlayerMeterCashPointTuning player_meter_cash_points {};
    EcologyTuning ecology {};
    ModifierSystemTuning modifier_system {};
    DeviceSupportTuning device_support {};
    CampDurabilityTuning camp_durability {};
};

[[nodiscard]] const GameplayTuningDef& gameplay_tuning_def() noexcept;
[[nodiscard]] double player_meter_cash_point_value(
    float health_delta,
    float hydration_delta,
    float nourishment_delta,
    float energy_delta,
    float morale_delta) noexcept;
[[nodiscard]] std::uint32_t player_meter_gain_internal_cash_points(
    float health_delta,
    float hydration_delta,
    float nourishment_delta,
    float energy_delta,
    float morale_delta) noexcept;
[[nodiscard]] std::uint32_t player_meter_cost_internal_cash_points(
    float health_cost,
    float hydration_cost,
    float nourishment_cost,
    float energy_cost,
    float morale_cost) noexcept;
}  // namespace gs1
