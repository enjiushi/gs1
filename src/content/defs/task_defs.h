#pragma once

#include "content/defs/craft_recipe_defs.h"
#include "content/defs/item_defs.h"
#include "content/defs/structure_defs.h"
#include "site/action_state.h"
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
    ConsumeItem = 7,
    PerformAction = 8,
    CraftItem = 9,
    CraftAnyItem = 10,
    BuildStructure = 11,
    BuildAnyStructure = 12,
    BuildStructureSet = 13,
    PlantAny = 14,
    EarnMoney = 15,
    SellCraftedItem = 16,
    KeepAllWorkerMetersAboveForDuration = 17,
    KeepTileMoistureAtLeastForDuration = 18,
    KeepTileHeatAtMostForDuration = 19,
    KeepTileDustAtMostForDuration = 20,
    PlantCountAtDensity = 21,
    AnyPlantDensityAtLeast = 22,
    PlantWindProtectionAtLeast = 23,
    PlantHeatProtectionAtLeast = 24,
    PlantDustProtectionAtLeast = 25,
    KeepAllDevicesIntegrityAboveForDuration = 26,
    NearbyPopulatedPlantTilesAtLeast = 27,
    KeepAllLivingPlantsNotWitheringForDuration = 28,
    SubmitItem = 29
};

struct TaskTemplateDef final
{
    TaskTemplateId task_template_id {};
    FactionId publisher_faction_id {};
    std::uint32_t task_tier_id {0};
    TaskProgressKind progress_kind {TaskProgressKind::None};
    std::uint32_t target_amount {0};
    std::uint32_t required_count {0};
    ItemId item_id {};
    PlantId plant_id {};
    RecipeId recipe_id {};
    StructureId structure_id {};
    StructureId secondary_structure_id {};
    StructureId tertiary_structure_id {};
    ActionKind action_kind {ActionKind::None};
    float threshold_value {0.0f};
    std::int32_t completion_faction_reputation_delta {0};
    float expected_task_hours_in_game {0.0f};
    float risk_multiplier {0.0f};
};

struct SiteOnboardingTaskSeedDef final
{
    SiteId site_id {};
    TaskTemplateId task_template_id {};
    std::uint32_t target_amount {0};
    std::uint32_t required_count {0};
    ItemId item_id {};
    PlantId plant_id {};
    RecipeId recipe_id {};
    StructureId structure_id {};
    StructureId secondary_structure_id {};
    StructureId tertiary_structure_id {};
    ActionKind action_kind {ActionKind::None};
    float threshold_value {0.0f};
    RewardCandidateId reward_candidate_id {};
};

inline constexpr std::uint32_t k_task_template_site1_restore_patch = 1U;
inline constexpr std::uint32_t k_task_template_site1_buy_water = 2U;
inline constexpr std::uint32_t k_task_template_site1_sell_water = 3U;
inline constexpr std::uint32_t k_task_template_site1_transfer_seeds = 4U;
inline constexpr std::uint32_t k_task_template_site1_plant_ordos_wormwood = 5U;
inline constexpr std::uint32_t k_task_template_site1_craft_hammer = 6U;
inline constexpr std::uint32_t k_task_template_site1_consume_supply = 7U;
inline constexpr std::uint32_t k_task_template_site1_build_camp_stove = 34U;
inline constexpr std::uint32_t k_task_template_site1_keep_worker_meters_high = 22U;
inline constexpr std::uint32_t k_task_template_site1_keep_devices_healthy = 38U;
inline constexpr std::uint32_t k_task_template_site1_keep_living_plants_stable = 39U;
inline constexpr std::uint32_t k_task_template_site1_submit_water = 40U;

[[nodiscard]] std::span<const TaskTemplateDef> all_task_template_defs() noexcept;
[[nodiscard]] const TaskTemplateDef* find_task_template_def(TaskTemplateId task_template_id) noexcept;
[[nodiscard]] std::span<const SiteOnboardingTaskSeedDef> all_site_onboarding_task_seed_defs() noexcept;
}  // namespace gs1
