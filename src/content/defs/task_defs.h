#pragma once

#include "content/defs/craft_recipe_defs.h"
#include "content/defs/item_defs.h"
#include "support/id_types.h"

#include <array>
#include <cstdint>

namespace gs1
{
enum class TaskProgressKind : std::uint32_t
{
    None = 0,
    RestorationTiles = 1,
    BuyItem = 2,
    SellItem = 3,
    TransferItem = 4,
    PlantItem = 5,
    CraftRecipe = 6,
    ConsumeItem = 7
};

struct TaskTemplateDef final
{
    TaskTemplateId task_template_id {};
    FactionId publisher_faction_id {};
    std::uint32_t task_tier_id {0};
    TaskProgressKind progress_kind {TaskProgressKind::None};
    std::uint32_t target_amount {0};
    ItemId item_id {};
    RecipeId recipe_id {};
    std::int32_t completion_faction_reputation_delta {0};
};

inline constexpr std::uint32_t k_task_template_site1_restore_patch = 1U;
inline constexpr std::uint32_t k_task_template_site1_buy_water = 2U;
inline constexpr std::uint32_t k_task_template_site1_sell_water = 3U;
inline constexpr std::uint32_t k_task_template_site1_transfer_seeds = 4U;
inline constexpr std::uint32_t k_task_template_site1_plant_wind_reed = 5U;
inline constexpr std::uint32_t k_task_template_site1_craft_hammer = 6U;
inline constexpr std::uint32_t k_task_template_site1_consume_supply = 7U;

inline constexpr std::array<TaskTemplateDef, 7> k_prototype_task_template_defs {{
    TaskTemplateDef {
        TaskTemplateId {k_task_template_site1_restore_patch},
        FactionId {1U},
        1U,
        TaskProgressKind::RestorationTiles,
        0U,
        ItemId {},
        RecipeId {},
        5},
    TaskTemplateDef {
        TaskTemplateId {k_task_template_site1_buy_water},
        FactionId {1U},
        1U,
        TaskProgressKind::BuyItem,
        1U,
        ItemId {k_item_water_container},
        RecipeId {},
        2},
    TaskTemplateDef {
        TaskTemplateId {k_task_template_site1_sell_water},
        FactionId {1U},
        1U,
        TaskProgressKind::SellItem,
        1U,
        ItemId {k_item_water_container},
        RecipeId {},
        2},
    TaskTemplateDef {
        TaskTemplateId {k_task_template_site1_transfer_seeds},
        FactionId {1U},
        1U,
        TaskProgressKind::TransferItem,
        1U,
        ItemId {k_item_wind_reed_seed_bundle},
        RecipeId {},
        2},
    TaskTemplateDef {
        TaskTemplateId {k_task_template_site1_plant_wind_reed},
        FactionId {1U},
        1U,
        TaskProgressKind::PlantItem,
        1U,
        ItemId {k_item_wind_reed_seed_bundle},
        RecipeId {},
        3},
    TaskTemplateDef {
        TaskTemplateId {k_task_template_site1_craft_hammer},
        FactionId {1U},
        1U,
        TaskProgressKind::CraftRecipe,
        1U,
        ItemId {},
        RecipeId {k_recipe_craft_hammer},
        3},
    TaskTemplateDef {
        TaskTemplateId {k_task_template_site1_consume_supply},
        FactionId {1U},
        1U,
        TaskProgressKind::ConsumeItem,
        1U,
        ItemId {},
        RecipeId {},
        2},
}};

[[nodiscard]] inline constexpr const TaskTemplateDef* find_task_template_def(
    TaskTemplateId task_template_id) noexcept
{
    for (const auto& task_template_def : k_prototype_task_template_defs)
    {
        if (task_template_def.task_template_id == task_template_id)
        {
            return &task_template_def;
        }
    }

    return nullptr;
}
}  // namespace gs1
