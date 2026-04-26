#pragma once

#include "support/id_types.h"

#include <cstdint>
#include <span>
#include <type_traits>

namespace gs1
{
enum class PlantHeightClass : std::uint8_t
{
    None = 0,
    Low = 1,
    Medium = 2,
    Tall = 3
};

enum class PlantFocus : std::uint8_t
{
    Setup = 0,
    Protection = 1,
    OutputWorkerSupport = 2,
    SalinityOutput = 3,
    SoilSupport = 4,
    ProtectionOutput = 5,
    SoilSupportSalinity = 6,
    ProtectionWorkerSupport = 7,
    Output = 8,
    ProtectionSoilSupport = 9
};

inline constexpr std::uint32_t k_plant_ordos_wormwood = 1U;
inline constexpr std::uint32_t k_plant_white_thorn = 2U;
inline constexpr std::uint32_t k_plant_red_tamarisk = 3U;
inline constexpr std::uint32_t k_plant_ningxia_wolfberry = 4U;
inline constexpr std::uint32_t k_plant_straw_checkerboard = 5U;
inline constexpr std::uint32_t k_plant_korshinsk_peashrub = 6U;
inline constexpr std::uint32_t k_plant_jiji_grass = 7U;
inline constexpr std::uint32_t k_plant_sea_buckthorn = 8U;
inline constexpr std::uint32_t k_plant_desert_ephedra = 9U;
inline constexpr std::uint32_t k_plant_saxaul = 10U;

struct PlantDef final
{
    PlantId plant_id;
    const char* display_name;
    PlantHeightClass height_class;
    PlantFocus focus;
    bool growable;
    std::uint8_t aura_size;
    std::uint8_t footprint_width;
    std::uint8_t footprint_height;
    std::uint8_t wind_protection_range;
    float protection_ratio;
    float plant_action_duration_minutes;
    float constant_wither_rate;
    float salt_tolerance;
    float heat_tolerance;
    float wind_resistance;
    float dust_tolerance;
    float fertility_improve_power;
    float output_power;
    float spread_readiness;
    float spread_chance;
    float output_dependency;
    ItemId harvest_item_id;
    std::uint16_t harvest_quantity;
    std::uint16_t reserved0;
    float harvest_action_duration_minutes;
    float harvest_density_required;
    float harvest_density_removed;
};

[[nodiscard]] inline constexpr bool plant_focus_has_aura(PlantFocus focus) noexcept
{
    switch (focus)
    {
    case PlantFocus::Setup:
    case PlantFocus::Protection:
    case PlantFocus::SalinityOutput:
    case PlantFocus::SoilSupport:
    case PlantFocus::ProtectionOutput:
    case PlantFocus::SoilSupportSalinity:
    case PlantFocus::ProtectionWorkerSupport:
    case PlantFocus::ProtectionSoilSupport:
        return true;
    case PlantFocus::OutputWorkerSupport:
    case PlantFocus::Output:
    default:
        return false;
    }
}

[[nodiscard]] inline constexpr bool plant_focus_has_wind_projection(PlantFocus focus) noexcept
{
    switch (focus)
    {
    case PlantFocus::Setup:
    case PlantFocus::Protection:
    case PlantFocus::ProtectionOutput:
    case PlantFocus::ProtectionWorkerSupport:
    case PlantFocus::ProtectionSoilSupport:
        return true;
    case PlantFocus::OutputWorkerSupport:
    case PlantFocus::SalinityOutput:
    case PlantFocus::SoilSupport:
    case PlantFocus::SoilSupportSalinity:
    case PlantFocus::Output:
    default:
        return false;
    }
}

[[nodiscard]] inline constexpr float plant_focus_salinity_ratio(PlantFocus focus) noexcept
{
    switch (focus)
    {
    case PlantFocus::SalinityOutput:
        return 1.0f;
    case PlantFocus::SoilSupportSalinity:
        return 0.75f;
    case PlantFocus::Setup:
    case PlantFocus::Protection:
    case PlantFocus::OutputWorkerSupport:
    case PlantFocus::SoilSupport:
    case PlantFocus::Output:
    case PlantFocus::ProtectionOutput:
    case PlantFocus::ProtectionWorkerSupport:
    case PlantFocus::ProtectionSoilSupport:
    default:
        return 0.0f;
    }
}

[[nodiscard]] inline constexpr float derived_plant_salinity_reduction_power(
    const PlantDef& plant_def) noexcept
{
    return plant_def.salt_tolerance * plant_focus_salinity_ratio(plant_def.focus);
}

[[nodiscard]] inline constexpr float plant_total_meter_pool(
    const PlantDef& plant_def) noexcept
{
    return plant_def.salt_tolerance +
        plant_def.heat_tolerance +
        plant_def.wind_resistance +
        plant_def.dust_tolerance +
        plant_def.fertility_improve_power +
        plant_def.output_power;
}

inline constexpr PlantDef k_generic_living_plant_def {
    PlantId {},
    "Generic Plant",
    PlantHeightClass::Low,
    PlantFocus::Protection,
    true,
    0U,
    1U,
    1U,
    0U,
    1.0f,
    1.0f,
    0.0f,
    40.0f,
    40.0f,
    40.0f,
    40.0f,
    12.0f,
    8.0f,
    35.0f,
    12.0f,
    16.0f,
    ItemId {},
    0U,
    0U,
    0.0f,
    0.0f,
    0.0f};

inline constexpr PlantDef k_generic_ground_cover_def {
    PlantId {},
    "Generic Ground Cover",
    PlantHeightClass::None,
    PlantFocus::Setup,
    false,
    0U,
    1U,
    1U,
    0U,
    1.0f,
    1.0f,
    0.0f,
    100.0f,
    100.0f,
    100.0f,
    100.0f,
    20.0f,
    0.0f,
    0.0f,
    0.0f,
    0.0f,
    ItemId {},
    0U,
    0U,
    0.0f,
    0.0f,
    0.0f};

[[nodiscard]] std::span<const PlantDef> all_plant_defs() noexcept;
[[nodiscard]] const PlantDef* find_plant_def(PlantId plant_id) noexcept;

static_assert(std::is_standard_layout_v<PlantDef>, "PlantDef must remain standard layout.");
static_assert(std::is_trivial_v<PlantDef>, "PlantDef must remain trivial.");
static_assert(std::is_trivially_copyable_v<PlantDef>, "PlantDef must remain trivially copyable.");
}  // namespace gs1
