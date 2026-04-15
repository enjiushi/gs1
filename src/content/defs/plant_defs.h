#pragma once

#include "support/id_types.h"

#include <array>
#include <cstdint>
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

inline constexpr std::uint32_t k_plant_wind_reed = 1U;
inline constexpr std::uint32_t k_plant_saltbush = 2U;
inline constexpr std::uint32_t k_plant_shade_cactus = 3U;
inline constexpr std::uint32_t k_plant_sunfruit_vine = 4U;

struct PlantDef final
{
    PlantId plant_id;
    const char* display_name;
    PlantHeightClass height_class;
    bool growable;
    std::uint8_t aura_size;
    std::uint16_t reserved0;
    float plant_action_duration_minutes;
    float constant_wither_rate;
    float salt_tolerance;
    float heat_tolerance;
    float wind_resistance;
    float dust_tolerance;
    float wind_protection_power;
    float heat_protection_power;
    float dust_protection_power;
    float fertility_improve_power;
    float salinity_reduction_power;
    float spread_readiness;
    float spread_chance;
    float output_dependency;
};

inline constexpr PlantDef k_generic_living_plant_def {
    PlantId {},
    "Generic Plant",
    PlantHeightClass::Low,
    true,
    0U,
    0U,
    1.0f,
    0.0f,
    40.0f,
    40.0f,
    40.0f,
    40.0f,
    18.0f,
    18.0f,
    18.0f,
    12.0f,
    8.0f,
    35.0f,
    12.0f,
    16.0f};

inline constexpr PlantDef k_generic_ground_cover_def {
    PlantId {},
    "Generic Ground Cover",
    PlantHeightClass::None,
    false,
    0U,
    0U,
    1.0f,
    0.0f,
    100.0f,
    100.0f,
    100.0f,
    100.0f,
    24.0f,
    16.0f,
    26.0f,
    20.0f,
    0.0f,
    0.0f,
    0.0f,
    0.0f};

inline constexpr std::array<PlantDef, 4> k_prototype_plant_defs {
    PlantDef {
        PlantId {k_plant_wind_reed},
        "Wind Reed",
        PlantHeightClass::Low,
        true,
        1U,
        0U,
        0.75f,
        0.0f,
        45.0f,
        38.0f,
        78.0f,
        58.0f,
        68.0f,
        18.0f,
        28.0f,
        14.0f,
        10.0f,
        55.0f,
        35.0f,
        10.0f},
    PlantDef {
        PlantId {k_plant_saltbush},
        "Saltbush",
        PlantHeightClass::Low,
        true,
        1U,
        0U,
        1.0f,
        0.0f,
        82.0f,
        52.0f,
        46.0f,
        44.0f,
        24.0f,
        18.0f,
        30.0f,
        38.0f,
        54.0f,
        45.0f,
        22.0f,
        18.0f},
    PlantDef {
        PlantId {k_plant_shade_cactus},
        "Shade Cactus",
        PlantHeightClass::Medium,
        true,
        1U,
        0U,
        1.5f,
        0.0f,
        48.0f,
        82.0f,
        30.0f,
        34.0f,
        12.0f,
        74.0f,
        18.0f,
        12.0f,
        6.0f,
        42.0f,
        10.0f,
        12.0f},
    PlantDef {
        PlantId {k_plant_sunfruit_vine},
        "Sunfruit Vine",
        PlantHeightClass::Low,
        true,
        1U,
        0U,
        1.25f,
        0.0f,
        18.0f,
        28.0f,
        24.0f,
        20.0f,
        8.0f,
        20.0f,
        6.0f,
        10.0f,
        0.0f,
        70.0f,
        14.0f,
        78.0f}
};

[[nodiscard]] inline constexpr const PlantDef* find_plant_def(PlantId plant_id) noexcept
{
    for (const auto& plant_def : k_prototype_plant_defs)
    {
        if (plant_def.plant_id == plant_id)
        {
            return &plant_def;
        }
    }

    return nullptr;
}

static_assert(std::is_standard_layout_v<PlantDef>, "PlantDef must remain standard layout.");
static_assert(std::is_trivial_v<PlantDef>, "PlantDef must remain trivial.");
static_assert(std::is_trivially_copyable_v<PlantDef>, "PlantDef must remain trivially copyable.");
}  // namespace gs1
