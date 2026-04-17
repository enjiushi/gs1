#pragma once

#include "content/defs/item_defs.h"
#include "content/defs/structure_defs.h"

#include <array>
#include <cstdint>

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

inline constexpr std::array<CraftRecipeDef, 5> k_prototype_craft_recipe_defs {{
    CraftRecipeDef {
        RecipeId {k_recipe_cook_food_pack},
        StructureId {k_structure_camp_stove},
        ItemId {k_item_food_pack},
        1U,
        1.0f,
        {CraftRecipeIngredientDef {ItemId {k_item_water_container}, 1U},
         CraftRecipeIngredientDef {ItemId {k_item_wind_reed_fiber}, 2U},
         CraftRecipeIngredientDef {},
         CraftRecipeIngredientDef {}},
        2U},
    CraftRecipeDef {
        RecipeId {k_recipe_craft_camp_stove},
        StructureId {k_structure_workbench},
        ItemId {k_item_camp_stove_kit},
        1U,
        1.5f,
        {CraftRecipeIngredientDef {ItemId {k_item_wood_bundle}, 4U},
         CraftRecipeIngredientDef {ItemId {k_item_iron_bundle}, 2U},
         CraftRecipeIngredientDef {},
         CraftRecipeIngredientDef {}},
        2U},
    CraftRecipeDef {
        RecipeId {k_recipe_craft_workbench},
        StructureId {k_structure_workbench},
        ItemId {k_item_workbench_kit},
        1U,
        2.0f,
        {CraftRecipeIngredientDef {ItemId {k_item_wood_bundle}, 5U},
         CraftRecipeIngredientDef {ItemId {k_item_iron_bundle}, 3U},
         CraftRecipeIngredientDef {},
         CraftRecipeIngredientDef {}},
        2U},
    CraftRecipeDef {
        RecipeId {k_recipe_craft_storage_crate},
        StructureId {k_structure_workbench},
        ItemId {k_item_storage_crate_kit},
        1U,
        1.0f,
        {CraftRecipeIngredientDef {ItemId {k_item_wood_bundle}, 3U},
         CraftRecipeIngredientDef {ItemId {k_item_iron_bundle}, 2U},
         CraftRecipeIngredientDef {},
         CraftRecipeIngredientDef {}},
        2U},
    CraftRecipeDef {
        RecipeId {k_recipe_craft_hammer},
        StructureId {k_structure_workbench},
        ItemId {k_item_hammer},
        1U,
        1.0f,
        {CraftRecipeIngredientDef {ItemId {k_item_wood_bundle}, 2U},
         CraftRecipeIngredientDef {ItemId {k_item_iron_bundle}, 1U},
         CraftRecipeIngredientDef {},
         CraftRecipeIngredientDef {}},
        2U},
}};

[[nodiscard]] inline constexpr const CraftRecipeDef* find_craft_recipe_def(
    StructureId station_structure_id,
    ItemId output_item_id) noexcept
{
    for (const auto& recipe_def : k_prototype_craft_recipe_defs)
    {
        if (recipe_def.station_structure_id == station_structure_id &&
            recipe_def.output_item_id == output_item_id)
        {
            return &recipe_def;
        }
    }

    return nullptr;
}

[[nodiscard]] inline constexpr const CraftRecipeDef* find_craft_recipe_def(RecipeId recipe_id) noexcept
{
    for (const auto& recipe_def : k_prototype_craft_recipe_defs)
    {
        if (recipe_def.recipe_id == recipe_id)
        {
            return &recipe_def;
        }
    }

    return nullptr;
}
}  // namespace gs1
