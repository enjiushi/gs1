#pragma once

#include "support/id_types.h"

#include <array>
#include <cstdint>

namespace gs1
{
enum ItemCapabilityFlags : std::uint32_t
{
    ITEM_CAPABILITY_NONE = 0,
    ITEM_CAPABILITY_DRINK = 1u << 0,
    ITEM_CAPABILITY_EAT = 1u << 1,
    ITEM_CAPABILITY_USE_MEDICINE = 1u << 2,
    ITEM_CAPABILITY_PLANT = 1u << 3,
    ITEM_CAPABILITY_SELL = 1u << 4
};

enum class ItemSourceRule : std::uint8_t
{
    None = 0,
    BuyOnly = 1,
    CraftOnly = 2,
    BuyOrCraft = 3,
    HarvestOnly = 4
};

struct ItemDef final
{
    ItemId item_id {};
    std::uint16_t stack_size {1};
    ItemSourceRule source_rule {ItemSourceRule::None};
    std::uint8_t reserved0 {0};
    std::int32_t buy_price {0};
    std::int32_t sell_price {0};
    std::uint32_t capability_flags {ITEM_CAPABILITY_NONE};
    PlantId linked_plant_id {};
    float health_delta {0.0f};
    float hydration_delta {0.0f};
    float nourishment_delta {0.0f};
    float energy_delta {0.0f};
};

inline constexpr std::uint32_t k_item_water_container = 1U;
inline constexpr std::uint32_t k_item_food_pack = 2U;
inline constexpr std::uint32_t k_item_medicine_pack = 3U;
inline constexpr std::uint32_t k_item_wind_reed_seed_bundle = 4U;
inline constexpr std::uint32_t k_item_saltbush_seed_bundle = 5U;
inline constexpr std::uint32_t k_item_shade_cactus_seed_bundle = 6U;
inline constexpr std::uint32_t k_item_sunfruit_vine_seed_bundle = 7U;

inline constexpr std::uint32_t k_plant_wind_reed = 1U;
inline constexpr std::uint32_t k_plant_saltbush = 2U;
inline constexpr std::uint32_t k_plant_shade_cactus = 3U;
inline constexpr std::uint32_t k_plant_sunfruit_vine = 4U;

inline constexpr std::array<ItemDef, 7> k_prototype_item_defs {
    ItemDef {
        ItemId {k_item_water_container},
        5U,
        ItemSourceRule::BuyOnly,
        0U,
        5,
        3,
        ITEM_CAPABILITY_DRINK | ITEM_CAPABILITY_SELL,
        PlantId {},
        0.0f,
        12.0f,
        0.0f,
        0.0f},
    ItemDef {
        ItemId {k_item_food_pack},
        5U,
        ItemSourceRule::BuyOnly,
        0U,
        4,
        2,
        ITEM_CAPABILITY_EAT | ITEM_CAPABILITY_SELL,
        PlantId {},
        0.0f,
        0.0f,
        10.0f,
        8.0f},
    ItemDef {
        ItemId {k_item_medicine_pack},
        3U,
        ItemSourceRule::BuyOnly,
        0U,
        8,
        5,
        ITEM_CAPABILITY_USE_MEDICINE | ITEM_CAPABILITY_SELL,
        PlantId {},
        18.0f,
        0.0f,
        0.0f,
        0.0f},
    ItemDef {
        ItemId {k_item_wind_reed_seed_bundle},
        10U,
        ItemSourceRule::BuyOnly,
        0U,
        6,
        3,
        ITEM_CAPABILITY_PLANT | ITEM_CAPABILITY_SELL,
        PlantId {k_plant_wind_reed},
        0.0f,
        0.0f,
        0.0f,
        0.0f},
    ItemDef {
        ItemId {k_item_saltbush_seed_bundle},
        10U,
        ItemSourceRule::BuyOnly,
        0U,
        7,
        4,
        ITEM_CAPABILITY_PLANT | ITEM_CAPABILITY_SELL,
        PlantId {k_plant_saltbush},
        0.0f,
        0.0f,
        0.0f,
        0.0f},
    ItemDef {
        ItemId {k_item_shade_cactus_seed_bundle},
        10U,
        ItemSourceRule::BuyOnly,
        0U,
        8,
        4,
        ITEM_CAPABILITY_PLANT | ITEM_CAPABILITY_SELL,
        PlantId {k_plant_shade_cactus},
        0.0f,
        0.0f,
        0.0f,
        0.0f},
    ItemDef {
        ItemId {k_item_sunfruit_vine_seed_bundle},
        10U,
        ItemSourceRule::BuyOnly,
        0U,
        10,
        5,
        ITEM_CAPABILITY_PLANT | ITEM_CAPABILITY_SELL,
        PlantId {k_plant_sunfruit_vine},
        0.0f,
        0.0f,
        0.0f,
        0.0f}
};

[[nodiscard]] inline constexpr bool item_has_capability(
    const ItemDef& item_def,
    ItemCapabilityFlags capability) noexcept
{
    return (item_def.capability_flags & capability) != 0U;
}

[[nodiscard]] inline constexpr const ItemDef* find_item_def(ItemId item_id) noexcept
{
    for (const auto& item_def : k_prototype_item_defs)
    {
        if (item_def.item_id == item_id)
        {
            return &item_def;
        }
    }

    return nullptr;
}

[[nodiscard]] inline constexpr std::uint32_t item_stack_size(ItemId item_id) noexcept
{
    const auto* item_def = find_item_def(item_id);
    return item_def == nullptr ? 1U : static_cast<std::uint32_t>(item_def->stack_size);
}
}  // namespace gs1
