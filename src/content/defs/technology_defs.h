#pragma once

#include "content/defs/faction_defs.h"
#include "content/defs/plant_defs.h"
#include "support/id_types.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace gs1
{
enum class TechnologyEntryKind : std::uint8_t
{
    GlobalModifier = 0,
    MechanismChange = 1
};

enum class ReputationUnlockKind : std::uint8_t
{
    Plant = 0,
    Device = 1,
    Recipe = 2
};

struct TechnologyTierDef final
{
    FactionId faction_id {};
    std::uint8_t tier_index {0};
    std::uint8_t reserved0[3] {};
    std::int32_t reputation_requirement {0};
    std::string_view display_name {};
};

struct TotalReputationTierDef final
{
    std::uint8_t tier_index {0};
    std::uint8_t reserved0[3] {};
    std::int32_t reputation_requirement {0};
    std::string_view display_name {};
};

struct ReputationUnlockDef final
{
    std::uint32_t unlock_id {0};
    ReputationUnlockKind unlock_kind {ReputationUnlockKind::Plant};
    std::uint8_t tier_index {0};
    std::uint8_t reserved0[2] {};
    std::uint32_t content_id {0};
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
    std::int32_t cash_cost {0};
    ModifierId linked_modifier_id {};
    std::uint32_t mechanism_change_id {0};
    bool is_todo_placeholder {true};
    std::uint8_t reserved1[3] {};
    std::string_view display_name {};
    std::string_view description {};
};

struct ReputationUnlockAuthoring final
{
    ReputationUnlockKind unlock_kind {ReputationUnlockKind::Plant};
    std::uint32_t content_id {0};
    std::string_view display_name {};
    std::string_view description {};
};

struct TechnologyAuthoring final
{
    TechnologyEntryKind entry_kind {TechnologyEntryKind::GlobalModifier};
    std::int32_t cash_cost {0};
    std::string_view display_name {};
    std::string_view description {};
};

inline constexpr std::uint8_t k_faction_tech_tier_count = 3U;
inline constexpr std::uint8_t k_faction_technologies_per_tier = 2U;
inline constexpr std::uint8_t k_total_reputation_tier_count = 3U;
inline constexpr std::uint8_t k_reputation_unlocks_per_tier = 2U;
inline constexpr std::array<std::int32_t, k_faction_tech_tier_count>
    k_faction_tech_tier_reputation_requirements {{
        10,
        25,
        45,
    }};
inline constexpr std::array<std::int32_t, k_total_reputation_tier_count>
    k_total_reputation_tier_requirements {{
        10,
        25,
        45,
    }};

inline constexpr std::array<PlantId, 4> k_initial_unlocked_plant_ids {{
    PlantId {k_plant_straw_checkerboard},
    PlantId {k_plant_wind_reed},
    PlantId {k_plant_root_binder},
    PlantId {k_plant_salt_bean},
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

[[nodiscard]] constexpr std::uint32_t reputation_unlock_id(
    std::uint8_t tier_index,
    std::uint8_t slot_index) noexcept
{
    return 5000U +
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

[[nodiscard]] constexpr TotalReputationTierDef make_total_reputation_tier(
    std::uint8_t tier_index,
    std::int32_t reputation_requirement,
    std::string_view display_name) noexcept
{
    return TotalReputationTierDef {
        tier_index,
        {0U, 0U, 0U},
        reputation_requirement,
        display_name};
}

[[nodiscard]] constexpr ReputationUnlockDef make_reputation_unlock(
    std::uint8_t tier_index,
    std::uint8_t slot_index,
    const ReputationUnlockAuthoring& authoring) noexcept
{
    return ReputationUnlockDef {
        reputation_unlock_id(tier_index, slot_index),
        authoring.unlock_kind,
        tier_index,
        {0U, 0U},
        authoring.content_id,
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
        authoring.cash_cost,
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

inline constexpr std::array<std::string_view, k_total_reputation_tier_count>
    k_total_reputation_tier_display_names {{
        "Reputation Tier I",
        "Reputation Tier II",
        "Reputation Tier III",
    }};

inline constexpr std::array<
    std::array<ReputationUnlockAuthoring, k_reputation_unlocks_per_tier>,
    k_total_reputation_tier_count>
    k_total_reputation_unlock_authoring {{
        {{
            {ReputationUnlockKind::Plant,
             k_plant_shade_cactus,
             "Shade Cactus",
             "Unlocks the first heat-buffer specialist for safer camp-edge pockets."},
            {ReputationUnlockKind::Plant,
             k_plant_dew_grass,
             "Dew Grass",
             "Unlocks a dryland bridge plant that adds soft moisture and fertility support."},
        }},
        {{
            {ReputationUnlockKind::Plant,
             k_plant_thorn_shrub,
             "Thorn Shrub",
             "Unlocks a tougher productive perimeter plant for harsher edges."},
            {ReputationUnlockKind::Plant,
             k_plant_medicinal_sage,
             "Medicinal Sage",
             "Unlocks a support-economy plant that rewards stable safe pockets."},
        }},
        {{
            {ReputationUnlockKind::Plant,
             k_plant_sunfruit_vine,
             "Sunfruit Vine",
             "Unlocks a fragile high-payoff crop that depends on established support."},
            {ReputationUnlockKind::Plant,
             k_plant_sand_willow,
             "Sand Willow",
             "Unlocks the late anchor canopy plant for turning a zone into a refuge core."},
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

[[nodiscard]] constexpr std::string_view reputation_unlock_kind_display_name(
    ReputationUnlockKind kind) noexcept
{
    switch (kind)
    {
    case ReputationUnlockKind::Plant:
        return "Plant";
    case ReputationUnlockKind::Device:
        return "Device";
    case ReputationUnlockKind::Recipe:
        return "Recipe";
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
inline constexpr std::size_t k_prototype_total_reputation_unlock_count =
    static_cast<std::size_t>(k_total_reputation_tier_count) *
    static_cast<std::size_t>(k_reputation_unlocks_per_tier);
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

inline constexpr std::array<TotalReputationTierDef, k_total_reputation_tier_count>
    k_total_reputation_tier_defs {{
        make_total_reputation_tier(1U, k_total_reputation_tier_requirements[0], k_total_reputation_tier_display_names[0]),
        make_total_reputation_tier(2U, k_total_reputation_tier_requirements[1], k_total_reputation_tier_display_names[1]),
        make_total_reputation_tier(3U, k_total_reputation_tier_requirements[2], k_total_reputation_tier_display_names[2]),
    }};

inline const auto k_prototype_total_reputation_unlock_defs = []()
{
    std::array<ReputationUnlockDef, k_prototype_total_reputation_unlock_count> unlock_defs {};
    std::size_t index = 0U;
    for (std::uint8_t tier_index = 1U; tier_index <= k_total_reputation_tier_count; ++tier_index)
    {
        for (std::uint8_t slot_index = 0U; slot_index < k_reputation_unlocks_per_tier; ++slot_index)
        {
            unlock_defs[index++] = make_reputation_unlock(
                tier_index,
                slot_index,
                k_total_reputation_unlock_authoring[tier_index - 1U][slot_index]);
        }
    }
    return unlock_defs;
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

[[nodiscard]] inline constexpr const TotalReputationTierDef* find_total_reputation_tier_def(
    std::uint8_t tier_index) noexcept
{
    for (const auto& tier_def : k_total_reputation_tier_defs)
    {
        if (tier_def.tier_index == tier_index)
        {
            return &tier_def;
        }
    }

    return nullptr;
}

[[nodiscard]] inline constexpr const ReputationUnlockDef* find_reputation_unlock_def(
    std::uint32_t unlock_id) noexcept
{
    for (const auto& unlock_def : k_prototype_total_reputation_unlock_defs)
    {
        if (unlock_def.unlock_id == unlock_id)
        {
            return &unlock_def;
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
