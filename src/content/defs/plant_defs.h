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
    bool growable;
    std::uint8_t aura_size;
    std::uint8_t footprint_width;
    std::uint8_t footprint_height;
    std::uint8_t wind_protection_range;
    float plant_action_duration_minutes;
    float constant_wither_rate;
    float salt_tolerance;
    float heat_tolerance;
    float wind_resistance;
    float dust_tolerance;
    float fertility_improve_power;
    float salinity_reduction_power;
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

inline constexpr PlantDef k_generic_living_plant_def {
    PlantId {},
    "Generic Plant",
    PlantHeightClass::Low,
    true,
    0U,
    1U,
    1U,
    0U,
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
    false,
    0U,
    1U,
    1U,
    0U,
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
