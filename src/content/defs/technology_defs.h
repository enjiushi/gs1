#pragma once

#include "content/defs/faction_defs.h"
#include "support/id_types.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace gs1
{
enum class FactionUnlockableKind : std::uint8_t
{
    Item = 0,
    PlantSeed = 1,
    Device = 2,
    RecipeFood = 3,
    RecipeDevice = 4,
    RecipeTool = 5
};

enum class TechnologyEntryKind : std::uint8_t
{
    GlobalModifier = 0,
    MechanismChange = 1
};

struct TechnologyTierDef final
{
    FactionId faction_id {};
    std::uint8_t tier_index {0};
    std::uint8_t reserved0[3] {};
    std::int32_t reputation_requirement {0};
    std::string_view display_name {};
};

struct FactionUnlockableDef final
{
    std::uint32_t unlockable_id {0};
    FactionId faction_id {};
    std::uint8_t tier_index {0};
    FactionUnlockableKind unlockable_kind {FactionUnlockableKind::Item};
    std::uint16_t reserved0 {0};
    std::uint32_t content_id {0};
    bool is_todo_placeholder {true};
    std::uint8_t reserved1[3] {};
    std::string_view display_name {};
    std::string_view description {};
};

struct TechnologyNodeDef final
{
    TechNodeId tech_node_id {};
    FactionId faction_id {};
    std::uint8_t tier_index {0};
    std::uint8_t slot_index {0};
    TechnologyEntryKind entry_kind {TechnologyEntryKind::GlobalModifier};
    std::uint8_t reserved0[3] {};
    std::int32_t reputation_cost {0};
    ModifierId linked_modifier_id {};
    std::uint32_t mechanism_change_id {0};
    bool is_todo_placeholder {true};
    std::uint8_t reserved1[3] {};
    std::string_view display_name {};
    std::string_view description {};
};

struct FactionUnlockableAuthoring final
{
    FactionUnlockableKind unlockable_kind {FactionUnlockableKind::Item};
    std::string_view display_name {};
    std::string_view description {};
};

struct TechnologyAuthoring final
{
    TechnologyEntryKind entry_kind {TechnologyEntryKind::GlobalModifier};
    std::int32_t reputation_cost {0};
    std::string_view display_name {};
    std::string_view description {};
};

inline constexpr std::uint8_t k_faction_tech_tier_count = 3U;
inline constexpr std::uint8_t k_faction_unlockables_per_tier = 4U;
inline constexpr std::uint8_t k_faction_technologies_per_tier = 2U;
inline constexpr std::array<std::int32_t, k_faction_tech_tier_count>
    k_faction_tech_tier_reputation_requirements {{
        10,
        25,
        45,
    }};

[[nodiscard]] constexpr std::uint32_t technology_family_base(FactionId faction_id) noexcept
{
    return faction_id.value * 1000U;
}

[[nodiscard]] constexpr std::uint32_t technology_node_id(
    FactionId faction_id,
    std::uint8_t tier_index,
    std::uint8_t slot_index) noexcept
{
    return technology_family_base(faction_id) +
        static_cast<std::uint32_t>(tier_index - 1U) * 100U +
        static_cast<std::uint32_t>(slot_index + 1U);
}

[[nodiscard]] constexpr std::uint32_t technology_unlockable_id(
    FactionId faction_id,
    std::uint8_t tier_index,
    std::uint8_t slot_index) noexcept
{
    return technology_family_base(faction_id) +
        500U +
        static_cast<std::uint32_t>(tier_index - 1U) * 10U +
        static_cast<std::uint32_t>(slot_index + 1U);
}

[[nodiscard]] constexpr std::uint32_t technology_modifier_id(
    FactionId faction_id,
    std::uint8_t tier_index,
    std::uint8_t slot_index) noexcept
{
    return technology_family_base(faction_id) +
        800U +
        static_cast<std::uint32_t>(tier_index - 1U) * 10U +
        static_cast<std::uint32_t>(slot_index + 1U);
}

[[nodiscard]] constexpr std::uint32_t technology_mechanism_change_id(
    FactionId faction_id,
    std::uint8_t tier_index,
    std::uint8_t slot_index) noexcept
{
    return technology_family_base(faction_id) +
        900U +
        static_cast<std::uint32_t>(tier_index - 1U) * 10U +
        static_cast<std::uint32_t>(slot_index + 1U);
}

inline constexpr std::uint32_t k_tech_node_village_t1_relief_protocol =
    technology_node_id(FactionId {k_faction_village_committee}, 1U, 0U);
inline constexpr std::uint32_t k_tech_node_village_t1_supply_routing =
    technology_node_id(FactionId {k_faction_village_committee}, 1U, 1U);
inline constexpr std::uint32_t k_tech_node_village_t2_kitchen_standards =
    technology_node_id(FactionId {k_faction_village_committee}, 2U, 0U);

[[nodiscard]] constexpr TechnologyTierDef make_tier(
    FactionId faction_id,
    std::uint8_t tier_index,
    std::int32_t reputation_requirement,
    std::string_view display_name) noexcept
{
    return TechnologyTierDef {
        faction_id,
        tier_index,
        {0U, 0U, 0U},
        reputation_requirement,
        display_name};
}

[[nodiscard]] constexpr FactionUnlockableDef make_unlockable(
    FactionId faction_id,
    std::uint8_t tier_index,
    std::uint8_t slot_index,
    const FactionUnlockableAuthoring& authoring) noexcept
{
    return FactionUnlockableDef {
        technology_unlockable_id(faction_id, tier_index, slot_index),
        faction_id,
        tier_index,
        authoring.unlockable_kind,
        0U,
        technology_unlockable_id(faction_id, tier_index, slot_index),
        true,
        {0U, 0U, 0U},
        authoring.display_name,
        authoring.description};
}

[[nodiscard]] constexpr TechnologyNodeDef make_technology_node(
    FactionId faction_id,
    std::uint8_t tier_index,
    std::uint8_t slot_index,
    const TechnologyAuthoring& authoring) noexcept
{
    const auto tech_node_id = technology_node_id(faction_id, tier_index, slot_index);
    return TechnologyNodeDef {
        TechNodeId {tech_node_id},
        faction_id,
        tier_index,
        slot_index,
        authoring.entry_kind,
        {0U, 0U, 0U},
        authoring.reputation_cost,
        authoring.entry_kind == TechnologyEntryKind::GlobalModifier
            ? ModifierId {technology_modifier_id(faction_id, tier_index, slot_index)}
            : ModifierId {},
        authoring.entry_kind == TechnologyEntryKind::MechanismChange
            ? technology_mechanism_change_id(faction_id, tier_index, slot_index)
            : 0U,
        true,
        {0U, 0U, 0U},
        authoring.display_name,
        authoring.description};
}

inline constexpr std::array<std::array<std::string_view, k_faction_tech_tier_count>, 3>
    k_faction_tier_display_names {{
        {{"Village Tier I", "Village Tier II", "Village Tier III"}},
        {{"Forestry Tier I", "Forestry Tier II", "Forestry Tier III"}},
        {{"University Tier I", "University Tier II", "University Tier III"}},
    }};

inline constexpr std::array<
    std::array<std::array<FactionUnlockableAuthoring, k_faction_unlockables_per_tier>, k_faction_tech_tier_count>,
    3>
    k_faction_unlockable_authoring {{
        {{
            {{
                {FactionUnlockableKind::Item, "TODO Relief Bundle",
                 "TODO item unlock placeholder for the Village Committee tier-I reward track."},
                {FactionUnlockableKind::PlantSeed, "TODO Drought Seed",
                 "TODO plant-seed unlock placeholder that should become available immediately at tier I."},
                {FactionUnlockableKind::Device, "TODO Cook Station",
                 "TODO device unlock placeholder for early village logistics support."},
                {FactionUnlockableKind::RecipeFood, "TODO Shared Meal",
                 "TODO food-recipe unlock placeholder for the first village tier."},
            }},
            {{
                {FactionUnlockableKind::Item, "TODO Route Crate",
                 "TODO item unlock placeholder for the Village Committee tier-II track."},
                {FactionUnlockableKind::PlantSeed, "TODO Shade Seed",
                 "TODO plant-seed unlock placeholder for the middle village tier."},
                {FactionUnlockableKind::Device, "TODO Prep Table",
                 "TODO device unlock placeholder for upgraded village operations."},
                {FactionUnlockableKind::RecipeDevice, "TODO Utility Rig",
                 "TODO device-recipe unlock placeholder for the second village tier."},
            }},
            {{
                {FactionUnlockableKind::Item, "TODO Relief Cache",
                 "TODO item unlock placeholder for the late village tier."},
                {FactionUnlockableKind::PlantSeed, "TODO Canopy Seed",
                 "TODO plant-seed unlock placeholder for the late village tier."},
                {FactionUnlockableKind::Device, "TODO Mobile Kitchen",
                 "TODO device unlock placeholder for the late village tier."},
                {FactionUnlockableKind::RecipeTool, "TODO Repair Kit",
                 "TODO tool-recipe unlock placeholder for the late village tier."},
            }},
        }},
        {{
            {{
                {FactionUnlockableKind::Item, "TODO Nursery Pack",
                 "TODO item unlock placeholder for the Forestry Bureau tier-I track."},
                {FactionUnlockableKind::PlantSeed, "TODO Windbreak Seed",
                 "TODO plant-seed unlock placeholder for forestry tier I."},
                {FactionUnlockableKind::Device, "TODO Mist Cart",
                 "TODO device unlock placeholder for forestry field support."},
                {FactionUnlockableKind::RecipeFood, "TODO Herb Tea",
                 "TODO food-recipe unlock placeholder for forestry tier I."},
            }},
            {{
                {FactionUnlockableKind::Item, "TODO Soil Kit",
                 "TODO item unlock placeholder for the Forestry Bureau tier-II track."},
                {FactionUnlockableKind::PlantSeed, "TODO Salt Trap Seed",
                 "TODO plant-seed unlock placeholder for forestry tier II."},
                {FactionUnlockableKind::Device, "TODO Shade Frame",
                 "TODO device unlock placeholder for forestry tier II."},
                {FactionUnlockableKind::RecipeDevice, "TODO Nursery Pump",
                 "TODO device-recipe unlock placeholder for forestry tier II."},
            }},
            {{
                {FactionUnlockableKind::Item, "TODO Habitat Stock",
                 "TODO item unlock placeholder for the Forestry Bureau tier-III track."},
                {FactionUnlockableKind::PlantSeed, "TODO Refuge Seed",
                 "TODO plant-seed unlock placeholder for forestry tier III."},
                {FactionUnlockableKind::Device, "TODO Canopy Lattice",
                 "TODO device unlock placeholder for forestry tier III."},
                {FactionUnlockableKind::RecipeTool, "TODO Survey Tool",
                 "TODO tool-recipe unlock placeholder for forestry tier III."},
            }},
        }},
        {{
            {{
                {FactionUnlockableKind::Item, "TODO Field Kit",
                 "TODO item unlock placeholder for Agricultural University tier I."},
                {FactionUnlockableKind::PlantSeed, "TODO Trial Seed",
                 "TODO plant-seed unlock placeholder for university tier I."},
                {FactionUnlockableKind::Device, "TODO Sensor Rack",
                 "TODO device unlock placeholder for university tier I."},
                {FactionUnlockableKind::RecipeFood, "TODO Focus Tea",
                 "TODO food-recipe unlock placeholder for university tier I."},
            }},
            {{
                {FactionUnlockableKind::Item, "TODO Lab Stock",
                 "TODO item unlock placeholder for Agricultural University tier II."},
                {FactionUnlockableKind::PlantSeed, "TODO Climate Seed",
                 "TODO plant-seed unlock placeholder for university tier II."},
                {FactionUnlockableKind::Device, "TODO Relay Mast",
                 "TODO device unlock placeholder for university tier II."},
                {FactionUnlockableKind::RecipeDevice, "TODO Calibration Jig",
                 "TODO device-recipe unlock placeholder for university tier II."},
            }},
            {{
                {FactionUnlockableKind::Item, "TODO Control Cache",
                 "TODO item unlock placeholder for Agricultural University tier III."},
                {FactionUnlockableKind::PlantSeed, "TODO Research Seed",
                 "TODO plant-seed unlock placeholder for university tier III."},
                {FactionUnlockableKind::Device, "TODO Control Hub",
                 "TODO device unlock placeholder for university tier III."},
                {FactionUnlockableKind::RecipeTool, "TODO Analysis Tool",
                 "TODO tool-recipe unlock placeholder for university tier III."},
            }},
        }},
    }};

inline constexpr std::array<
    std::array<std::array<TechnologyAuthoring, k_faction_technologies_per_tier>, k_faction_tech_tier_count>,
    3>
    k_faction_technology_authoring {{
        {{
            {{
                {TechnologyEntryKind::GlobalModifier, 4, "TODO Relief Doctrine",
                 "TODO global-modifier technology placeholder for Village Committee tier I."},
                {TechnologyEntryKind::MechanismChange, 6, "TODO Queue Standard",
                 "TODO mechanism-change technology placeholder for Village Committee tier I."},
            }},
            {{
                {TechnologyEntryKind::GlobalModifier, 8, "TODO Kitchen Tempo",
                 "TODO global-modifier technology placeholder for Village Committee tier II."},
                {TechnologyEntryKind::MechanismChange, 10, "TODO Dispatch Rule",
                 "TODO mechanism-change technology placeholder for Village Committee tier II."},
            }},
            {{
                {TechnologyEntryKind::GlobalModifier, 12, "TODO Relief Network",
                 "TODO global-modifier technology placeholder for Village Committee tier III."},
                {TechnologyEntryKind::MechanismChange, 14, "TODO Convoy Logic",
                 "TODO mechanism-change technology placeholder for Village Committee tier III."},
            }},
        }},
        {{
            {{
                {TechnologyEntryKind::GlobalModifier, 4, "TODO Seed Vigor",
                 "TODO global-modifier technology placeholder for Forestry Bureau tier I."},
                {TechnologyEntryKind::MechanismChange, 6, "TODO Nursery Rule",
                 "TODO mechanism-change technology placeholder for Forestry Bureau tier I."},
            }},
            {{
                {TechnologyEntryKind::GlobalModifier, 8, "TODO Shelter Growth",
                 "TODO global-modifier technology placeholder for Forestry Bureau tier II."},
                {TechnologyEntryKind::MechanismChange, 10, "TODO Root Cycle",
                 "TODO mechanism-change technology placeholder for Forestry Bureau tier II."},
            }},
            {{
                {TechnologyEntryKind::GlobalModifier, 12, "TODO Habitat Buffer",
                 "TODO global-modifier technology placeholder for Forestry Bureau tier III."},
                {TechnologyEntryKind::MechanismChange, 14, "TODO Recovery Rule",
                 "TODO mechanism-change technology placeholder for Forestry Bureau tier III."},
            }},
        }},
        {{
            {{
                {TechnologyEntryKind::GlobalModifier, 4, "TODO Survey Bias",
                 "TODO global-modifier technology placeholder for Agricultural University tier I."},
                {TechnologyEntryKind::MechanismChange, 6, "TODO Trial Method",
                 "TODO mechanism-change technology placeholder for Agricultural University tier I."},
            }},
            {{
                {TechnologyEntryKind::GlobalModifier, 8, "TODO Field Precision",
                 "TODO global-modifier technology placeholder for Agricultural University tier II."},
                {TechnologyEntryKind::MechanismChange, 10, "TODO Relay Policy",
                 "TODO mechanism-change technology placeholder for Agricultural University tier II."},
            }},
            {{
                {TechnologyEntryKind::GlobalModifier, 12, "TODO Control Envelope",
                 "TODO global-modifier technology placeholder for Agricultural University tier III."},
                {TechnologyEntryKind::MechanismChange, 14, "TODO Systems Method",
                 "TODO mechanism-change technology placeholder for Agricultural University tier III."},
            }},
        }},
    }};

[[nodiscard]] constexpr std::size_t faction_authoring_index(FactionId faction_id) noexcept
{
    return faction_id.value <= 1U ? 0U : static_cast<std::size_t>(faction_id.value - 1U);
}

[[nodiscard]] constexpr std::string_view faction_unlockable_kind_display_name(
    FactionUnlockableKind kind) noexcept
{
    switch (kind)
    {
    case FactionUnlockableKind::Item:
        return "Item";
    case FactionUnlockableKind::PlantSeed:
        return "Seed";
    case FactionUnlockableKind::Device:
        return "Device";
    case FactionUnlockableKind::RecipeFood:
        return "Food";
    case FactionUnlockableKind::RecipeDevice:
        return "Device Recipe";
    case FactionUnlockableKind::RecipeTool:
        return "Tool Recipe";
    default:
        return {};
    }
}

[[nodiscard]] constexpr std::string_view technology_entry_kind_display_name(
    TechnologyEntryKind kind) noexcept
{
    switch (kind)
    {
    case TechnologyEntryKind::GlobalModifier:
        return "Modifier";
    case TechnologyEntryKind::MechanismChange:
        return "Mechanism";
    default:
        return {};
    }
}

inline constexpr std::size_t k_prototype_technology_tier_count =
    k_prototype_faction_defs.size() * static_cast<std::size_t>(k_faction_tech_tier_count);
inline constexpr std::size_t k_prototype_faction_unlockable_count =
    k_prototype_faction_defs.size() *
    static_cast<std::size_t>(k_faction_tech_tier_count) *
    static_cast<std::size_t>(k_faction_unlockables_per_tier);
inline constexpr std::size_t k_prototype_technology_node_count =
    k_prototype_faction_defs.size() *
    static_cast<std::size_t>(k_faction_tech_tier_count) *
    static_cast<std::size_t>(k_faction_technologies_per_tier);

inline const auto k_prototype_technology_tier_defs = []()
{
    std::array<TechnologyTierDef, k_prototype_technology_tier_count> tier_defs {};
    std::size_t index = 0U;
    for (const auto& faction_def : k_prototype_faction_defs)
    {
        for (std::uint8_t tier_index = 1U; tier_index <= k_faction_tech_tier_count; ++tier_index)
        {
            tier_defs[index++] = make_tier(
                faction_def.faction_id,
                tier_index,
                k_faction_tech_tier_reputation_requirements[tier_index - 1U],
                k_faction_tier_display_names[faction_authoring_index(faction_def.faction_id)][tier_index - 1U]);
        }
    }
    return tier_defs;
}();

inline const auto k_prototype_faction_unlockable_defs = []()
{
    std::array<FactionUnlockableDef, k_prototype_faction_unlockable_count> unlockable_defs {};
    std::size_t index = 0U;
    for (const auto& faction_def : k_prototype_faction_defs)
    {
        for (std::uint8_t tier_index = 1U; tier_index <= k_faction_tech_tier_count; ++tier_index)
        {
            for (std::uint8_t slot_index = 0U; slot_index < k_faction_unlockables_per_tier; ++slot_index)
            {
                unlockable_defs[index++] = make_unlockable(
                    faction_def.faction_id,
                    tier_index,
                    slot_index,
                    k_faction_unlockable_authoring[faction_authoring_index(faction_def.faction_id)][tier_index - 1U]
                                                 [slot_index]);
            }
        }
    }
    return unlockable_defs;
}();

inline const auto k_prototype_technology_node_defs = []()
{
    std::array<TechnologyNodeDef, k_prototype_technology_node_count> node_defs {};
    std::size_t index = 0U;
    for (const auto& faction_def : k_prototype_faction_defs)
    {
        for (std::uint8_t tier_index = 1U; tier_index <= k_faction_tech_tier_count; ++tier_index)
        {
            for (std::uint8_t slot_index = 0U; slot_index < k_faction_technologies_per_tier; ++slot_index)
            {
                node_defs[index++] = make_technology_node(
                    faction_def.faction_id,
                    tier_index,
                    slot_index,
                    k_faction_technology_authoring[faction_authoring_index(faction_def.faction_id)][tier_index - 1U]
                                                  [slot_index]);
            }
        }
    }
    return node_defs;
}();

[[nodiscard]] inline constexpr const TechnologyTierDef* find_technology_tier_def(
    FactionId faction_id,
    std::uint8_t tier_index) noexcept
{
    for (const auto& tier_def : k_prototype_technology_tier_defs)
    {
        if (tier_def.faction_id == faction_id && tier_def.tier_index == tier_index)
        {
            return &tier_def;
        }
    }

    return nullptr;
}

[[nodiscard]] inline constexpr const FactionUnlockableDef* find_faction_unlockable_def(
    std::uint32_t unlockable_id) noexcept
{
    for (const auto& unlockable_def : k_prototype_faction_unlockable_defs)
    {
        if (unlockable_def.unlockable_id == unlockable_id)
        {
            return &unlockable_def;
        }
    }

    return nullptr;
}

[[nodiscard]] inline constexpr const TechnologyNodeDef* find_technology_node_def(
    TechNodeId tech_node_id) noexcept
{
    for (const auto& node_def : k_prototype_technology_node_defs)
    {
        if (node_def.tech_node_id == tech_node_id)
        {
            return &node_def;
        }
    }

    return nullptr;
}
}  // namespace gs1
