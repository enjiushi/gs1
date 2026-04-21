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
    std::array<CraftRecipeIngredientDef, 4> ingredients {};
    std::uint8_t ingredient_count {0U};
};

inline constexpr std::uint32_t k_recipe_cook_food_pack = 1U;
inline constexpr std::uint32_t k_recipe_craft_camp_stove = 2U;
inline constexpr std::uint32_t k_recipe_craft_workbench = 3U;
inline constexpr std::uint32_t k_recipe_craft_storage_crate = 4U;
inline constexpr std::uint32_t k_recipe_craft_hammer = 5U;

[[nodiscard]] std::span<const CraftRecipeDef> all_craft_recipe_defs() noexcept;

[[nodiscard]] const CraftRecipeDef* find_craft_recipe_def(
    StructureId station_structure_id,
    ItemId output_item_id) noexcept;
[[nodiscard]] const CraftRecipeDef* find_craft_recipe_def(RecipeId recipe_id) noexcept;
}  // namespace gs1
