#pragma once

#include "content/defs/faction_defs.h"
#include "content/defs/item_defs.h"
#include "support/id_types.h"

#include <array>
#include <cstdint>
#include <string_view>

namespace gs1
{
enum class TaskTemplateProgressKind : std::uint8_t
{
    None = 0,
    ClearBurialActionCount = 1,
    RepairDeviceCount = 2,
    PlantTileCount = 3,
    WaterTileCount = 4
};

struct TaskTemplateRewardDef final
{
    ItemId item_id {};
    std::uint16_t quantity {0};
    std::uint16_t delivery_minutes {0};
};

struct TaskTemplateDef final
{
    TaskTemplateId task_template_id {};
    FactionId publisher_faction_id {};
    std::uint32_t task_tier_id {0};
    TaskTemplateProgressKind progress_kind {TaskTemplateProgressKind::None};
    std::uint32_t target_amount {0};
    TaskTemplateRewardDef reward_delivery {};
    std::string_view title {};
    std::string_view summary {};
    std::string_view reward_summary {};
};

inline constexpr std::uint32_t k_task_template_site1_clear_path = 1U;
inline constexpr std::uint32_t k_task_template_site1_repair_workbench = 2U;
inline constexpr std::uint32_t k_task_template_site1_plant_wind_reeds = 3U;
inline constexpr std::uint32_t k_task_template_site1_water_seedlings = 4U;

inline constexpr std::array<TaskTemplateDef, 4> k_prototype_task_defs {{
    TaskTemplateDef {
        TaskTemplateId {k_task_template_site1_clear_path},
        FactionId {k_faction_village_committee},
        1U,
        TaskTemplateProgressKind::ClearBurialActionCount,
        1U,
        TaskTemplateRewardDef {ItemId {k_item_wood_bundle}, 2U, 5U},
        "Clear the Camp Path",
        "Use Clear Burial once on the drift packed against the camp approach.",
        "Reward: 2 Wood delivered to the box."},
    TaskTemplateDef {
        TaskTemplateId {k_task_template_site1_repair_workbench},
        FactionId {k_faction_village_committee},
        1U,
        TaskTemplateProgressKind::RepairDeviceCount,
        1U,
        TaskTemplateRewardDef {ItemId {k_item_iron_bundle}, 2U, 5U},
        "Fix the Workbench",
        "Repair the damaged starter workbench so field maintenance becomes familiar.",
        "Reward: 2 Iron delivered to the box."},
    TaskTemplateDef {
        TaskTemplateId {k_task_template_site1_plant_wind_reeds},
        FactionId {k_faction_village_committee},
        1U,
        TaskTemplateProgressKind::PlantTileCount,
        2U,
        TaskTemplateRewardDef {ItemId {k_item_wind_reed_seed_bundle}, 2U, 5U},
        "Plant a Starter Patch",
        "Plant two Wind Reed tiles near camp to learn the core restoration action.",
        "Reward: 2 Wind Reed Seeds delivered to the box."},
    TaskTemplateDef {
        TaskTemplateId {k_task_template_site1_water_seedlings},
        FactionId {k_faction_village_committee},
        1U,
        TaskTemplateProgressKind::WaterTileCount,
        2U,
        TaskTemplateRewardDef {ItemId {k_item_water_container}, 1U, 5U},
        "Water the Seedlings",
        "Use Water twice on planted tiles to teach follow-up care after placement.",
        "Reward: 1 Water delivered to the box."},
}};

[[nodiscard]] inline constexpr const TaskTemplateDef* find_task_template_def(
    TaskTemplateId task_template_id) noexcept
{
    for (const auto& task_def : k_prototype_task_defs)
    {
        if (task_def.task_template_id == task_template_id)
        {
            return &task_def;
        }
    }

    return nullptr;
}
}  // namespace gs1
