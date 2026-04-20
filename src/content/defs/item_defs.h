#pragma once

#include "content/defs/plant_defs.h"
#include "content/defs/structure_defs.h"

#include <array>
#include <cstdint>
#include <string_view>

namespace gs1
{
enum ItemCapabilityFlags : std::uint32_t
{
    ITEM_CAPABILITY_NONE = 0,
    ITEM_CAPABILITY_DRINK = 1u << 0,
    ITEM_CAPABILITY_EAT = 1u << 1,
    ITEM_CAPABILITY_USE_MEDICINE = 1u << 2,
    ITEM_CAPABILITY_PLANT = 1u << 3,
    ITEM_CAPABILITY_SELL = 1u << 4,
    ITEM_CAPABILITY_DEPLOY = 1u << 5
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
    std::string_view display_name {};
    std::uint16_t stack_size {1};
    ItemSourceRule source_rule {ItemSourceRule::None};
    bool consumable {false};
    std::int32_t buy_price {0};
    std::int32_t sell_price {0};
    std::uint32_t capability_flags {ITEM_CAPABILITY_NONE};
    PlantId linked_plant_id {};
    StructureId linked_structure_id {};
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
inline constexpr std::uint32_t k_item_wood_bundle = 8U;
inline constexpr std::uint32_t k_item_iron_bundle = 9U;
inline constexpr std::uint32_t k_item_wind_reed_fiber = 10U;
inline constexpr std::uint32_t k_item_camp_stove_kit = 11U;
inline constexpr std::uint32_t k_item_workbench_kit = 12U;
inline constexpr std::uint32_t k_item_storage_crate_kit = 13U;
inline constexpr std::uint32_t k_item_hammer = 14U;
inline constexpr std::uint32_t k_item_basic_straw_checkerboard = 15U;

inline constexpr std::array<ItemDef, 15> k_prototype_item_defs {{
    ItemDef {
        ItemId {k_item_water_container},
        "Water",
        5U,
        ItemSourceRule::BuyOrCraft,
        true,
        5,
        3,
        ITEM_CAPABILITY_DRINK | ITEM_CAPABILITY_SELL,
        PlantId {},
        StructureId {},
        0.0f,
        12.0f,
        0.0f,
        0.0f},
    ItemDef {
        ItemId {k_item_food_pack},
        "Food",
        5U,
        ItemSourceRule::BuyOrCraft,
        true,
        4,
        2,
        ITEM_CAPABILITY_EAT | ITEM_CAPABILITY_SELL,
        PlantId {},
        StructureId {},
        0.0f,
        0.0f,
        10.0f,
        8.0f},
    ItemDef {
        ItemId {k_item_medicine_pack},
        "Medicine",
        3U,
        ItemSourceRule::BuyOnly,
        true,
        8,
        5,
        ITEM_CAPABILITY_USE_MEDICINE | ITEM_CAPABILITY_SELL,
        PlantId {},
        StructureId {},
        18.0f,
        0.0f,
        0.0f,
        0.0f},
    ItemDef {
        ItemId {k_item_wind_reed_seed_bundle},
        "Wind Reed Seeds",
        10U,
        ItemSourceRule::BuyOnly,
        false,
        6,
        3,
        ITEM_CAPABILITY_PLANT | ITEM_CAPABILITY_SELL,
        PlantId {k_plant_wind_reed},
        StructureId {},
        0.0f,
        0.0f,
        0.0f,
        0.0f},
    ItemDef {
        ItemId {k_item_saltbush_seed_bundle},
        "Saltbush Seeds",
        10U,
        ItemSourceRule::BuyOnly,
        false,
        7,
        4,
        ITEM_CAPABILITY_PLANT | ITEM_CAPABILITY_SELL,
        PlantId {k_plant_saltbush},
        StructureId {},
        0.0f,
        0.0f,
        0.0f,
        0.0f},
    ItemDef {
        ItemId {k_item_shade_cactus_seed_bundle},
        "Shade Cactus Seeds",
        10U,
        ItemSourceRule::BuyOnly,
        false,
        8,
        4,
        ITEM_CAPABILITY_PLANT | ITEM_CAPABILITY_SELL,
        PlantId {k_plant_shade_cactus},
        StructureId {},
        0.0f,
        0.0f,
        0.0f,
        0.0f},
    ItemDef {
        ItemId {k_item_sunfruit_vine_seed_bundle},
        "Sunfruit Vine Seeds",
        10U,
        ItemSourceRule::BuyOnly,
        false,
        10,
        5,
        ITEM_CAPABILITY_PLANT | ITEM_CAPABILITY_SELL,
        PlantId {k_plant_sunfruit_vine},
        StructureId {},
        0.0f,
        0.0f,
        0.0f,
        0.0f},
    ItemDef {
        ItemId {k_item_wood_bundle},
        "Wood",
        20U,
        ItemSourceRule::BuyOnly,
        false,
        2,
        1,
        ITEM_CAPABILITY_SELL,
        PlantId {},
        StructureId {},
        0.0f,
        0.0f,
        0.0f,
        0.0f},
    ItemDef {
        ItemId {k_item_iron_bundle},
        "Iron",
        20U,
        ItemSourceRule::BuyOnly,
        false,
        3,
        1,
        ITEM_CAPABILITY_SELL,
        PlantId {},
        StructureId {},
        0.0f,
        0.0f,
        0.0f,
        0.0f},
    ItemDef {
        ItemId {k_item_wind_reed_fiber},
        "Wind Reed Fiber",
        20U,
        ItemSourceRule::HarvestOnly,
        false,
        1,
        1,
        ITEM_CAPABILITY_SELL,
        PlantId {},
        StructureId {},
        0.0f,
        0.0f,
        0.0f,
        0.0f},
    ItemDef {
        ItemId {k_item_camp_stove_kit},
        "Camp Stove Kit",
        1U,
        ItemSourceRule::CraftOnly,
        false,
        0,
        6,
        ITEM_CAPABILITY_DEPLOY | ITEM_CAPABILITY_SELL,
        PlantId {},
        StructureId {k_structure_camp_stove},
        0.0f,
        0.0f,
        0.0f,
        0.0f},
    ItemDef {
        ItemId {k_item_workbench_kit},
        "Workbench Kit",
        1U,
        ItemSourceRule::CraftOnly,
        false,
        0,
        8,
        ITEM_CAPABILITY_DEPLOY | ITEM_CAPABILITY_SELL,
        PlantId {},
        StructureId {k_structure_workbench},
        0.0f,
        0.0f,
        0.0f,
        0.0f},
    ItemDef {
        ItemId {k_item_storage_crate_kit},
        "Storage Crate Kit",
        1U,
        ItemSourceRule::CraftOnly,
        false,
        0,
        5,
        ITEM_CAPABILITY_DEPLOY | ITEM_CAPABILITY_SELL,
        PlantId {},
        StructureId {k_structure_storage_crate},
        0.0f,
        0.0f,
        0.0f,
        0.0f},
    ItemDef {
        ItemId {k_item_hammer},
        "Hammer",
        1U,
        ItemSourceRule::CraftOnly,
        false,
        0,
        4,
        ITEM_CAPABILITY_SELL,
        PlantId {},
        StructureId {},
        0.0f,
        0.0f,
        0.0f,
        0.0f},
    ItemDef {
        ItemId {k_item_basic_straw_checkerboard},
        "Basic Straw Checkerboard",
        10U,
        ItemSourceRule::BuyOnly,
        false,
        6,
        3,
        ITEM_CAPABILITY_PLANT | ITEM_CAPABILITY_SELL,
        PlantId {k_plant_straw_checkerboard},
        StructureId {},
        0.0f,
        0.0f,
        0.0f,
        0.0f},
}};

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
