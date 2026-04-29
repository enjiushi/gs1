#pragma once

#include "content/defs/item_defs.h"
#include "content/defs/structure_defs.h"

#include <array>
#include <cstdint>
#include <span>

namespace gs1
{
struct CraftRecipeIngredientDef final
{
    ItemId item_id {};
    std::uint16_t quantity {0U};
};

struct CraftRecipeDef final
{
    RecipeId recipe_id {};
    StructureId station_structure_id {};
    ItemId output_item_id {};
    std::uint16_t output_quantity {1U};
    float craft_minutes {1.0f};
    float energy_cost {0.0f};
    float hydration_cost {0.0f};
    float nourishment_cost {0.0f};
    std::array<CraftRecipeIngredientDef, 4> ingredients {};
    std::uint8_t ingredient_count {0U};
};

inline constexpr std::uint32_t k_recipe_cook_food_pack = 1U;
inline constexpr std::uint32_t k_recipe_craft_camp_stove = 2U;
inline constexpr std::uint32_t k_recipe_craft_workbench = 3U;
inline constexpr std::uint32_t k_recipe_craft_storage_crate = 4U;
inline constexpr std::uint32_t k_recipe_craft_hammer = 5U;
inline constexpr std::uint32_t k_recipe_cook_field_tea = 6U;
inline constexpr std::uint32_t k_recipe_cook_spiced_stew = 7U;
inline constexpr std::uint32_t k_recipe_craft_shovel = 8U;
inline constexpr std::uint32_t k_recipe_cook_wormwood_broth = 9U;
inline constexpr std::uint32_t k_recipe_cook_thornberry_cooler = 10U;
inline constexpr std::uint32_t k_recipe_cook_rich_wormwood_broth = 11U;
inline constexpr std::uint32_t k_recipe_cook_rich_thornberry_cooler = 12U;
inline constexpr std::uint32_t k_recipe_cook_peashrub_hotpot = 13U;
inline constexpr std::uint32_t k_recipe_cook_buckthorn_tonic = 14U;
inline constexpr std::uint32_t k_recipe_cook_rich_peashrub_hotpot = 15U;
inline constexpr std::uint32_t k_recipe_cook_rich_buckthorn_tonic = 16U;
inline constexpr std::uint32_t k_recipe_cook_jadeleaf_stew = 17U;
inline constexpr std::uint32_t k_recipe_cook_desert_revival_draught = 18U;
inline constexpr std::uint32_t k_recipe_cook_rich_jadeleaf_stew = 19U;
inline constexpr std::uint32_t k_recipe_cook_rich_desert_revival_draught = 20U;
inline constexpr std::uint32_t k_recipe_cook_ephedra_stew = 21U;

[[nodiscard]] std::span<const CraftRecipeDef> all_craft_recipe_defs() noexcept;

[[nodiscard]] const CraftRecipeDef* find_craft_recipe_def(
    StructureId station_structure_id,
    ItemId output_item_id) noexcept;
[[nodiscard]] const CraftRecipeDef* find_craft_recipe_def(RecipeId recipe_id) noexcept;
[[nodiscard]] bool item_has_craft_recipe_output(ItemId item_id) noexcept;
}  // namespace gs1
