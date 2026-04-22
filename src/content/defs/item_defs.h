#pragma once

#include "content/defs/plant_defs.h"
#include "content/defs/structure_defs.h"

#include <cstdint>
#include <span>
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
inline constexpr std::uint32_t k_item_salt_bean_seed_bundle = 5U;
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
inline constexpr std::uint32_t k_item_root_binder_seed_bundle = 16U;
inline constexpr std::uint32_t k_item_dew_grass_seed_bundle = 17U;
inline constexpr std::uint32_t k_item_thorn_shrub_seed_bundle = 18U;
inline constexpr std::uint32_t k_item_medicinal_sage_seed_bundle = 19U;
inline constexpr std::uint32_t k_item_sand_willow_seed_bundle = 20U;

[[nodiscard]] std::span<const ItemDef> all_item_defs() noexcept;

[[nodiscard]] inline constexpr bool item_has_capability(
    const ItemDef& item_def,
    ItemCapabilityFlags capability) noexcept
{
    return (item_def.capability_flags & capability) != 0U;
}

[[nodiscard]] const ItemDef* find_item_def(ItemId item_id) noexcept;
[[nodiscard]] std::uint32_t item_stack_size(ItemId item_id) noexcept;
}  // namespace gs1
