#pragma once

#include "content/defs/craft_recipe_defs.h"
#include "content/defs/item_defs.h"
#include "support/id_types.h"

#include <cstdint>
#include <span>

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
inline constexpr std::uint32_t k_task_template_site1_plant_ordos_wormwood = 5U;
inline constexpr std::uint32_t k_task_template_site1_craft_hammer = 6U;
inline constexpr std::uint32_t k_task_template_site1_consume_supply = 7U;

[[nodiscard]] std::span<const TaskTemplateDef> all_task_template_defs() noexcept;
[[nodiscard]] const TaskTemplateDef* find_task_template_def(TaskTemplateId task_template_id) noexcept;
}  // namespace gs1
