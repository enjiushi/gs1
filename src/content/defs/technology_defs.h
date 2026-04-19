#pragma once

#include "content/defs/faction_defs.h"
#include "support/id_types.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace gs1
{
enum class TechnologyEntryKind : std::uint8_t
{
    BaseTech = 0,
    Amplification = 1
};

struct TechnologyTierDef final
{
    FactionId faction_id {};
    std::uint8_t tier_index {0};
    std::uint8_t reserved0[3] {};
    std::int32_t base_reputation_cost {0};
    std::string_view display_name {};
};

struct TechnologyNodeDef final
{
    TechNodeId tech_node_id {};
    FactionId faction_id {};
    std::uint8_t tier_index {0};
    std::uint8_t base_slot_index {0};
    TechnologyEntryKind entry_kind {TechnologyEntryKind::BaseTech};
    std::uint8_t amplification_choice_index {0};
    TechNodeId parent_base_tech_id {};
    std::string_view display_name {};
    std::string_view description {};
};

inline constexpr std::uint8_t k_faction_tech_tier_count = 4U;
inline constexpr std::uint8_t k_faction_tech_base_choices_per_tier = 3U;
inline constexpr std::uint8_t k_faction_tech_amplification_choices_per_base = 2U;
inline constexpr float k_tier_additional_purchase_cost_multiplier = 1.2f;
inline constexpr std::array<std::int32_t, k_faction_tech_tier_count> k_faction_tech_tier_base_costs {{
    10,
    18,
    28,
    42,
}};

[[nodiscard]] constexpr std::uint32_t technology_family_base(FactionId faction_id) noexcept
{
    return faction_id.value * 1000U;
}

[[nodiscard]] constexpr std::uint32_t technology_base_node_id(
    FactionId faction_id,
    std::uint8_t tier_index,
    std::uint8_t base_slot_index) noexcept
{
    return technology_family_base(faction_id) +
        static_cast<std::uint32_t>(tier_index - 1U) * 100U +
        static_cast<std::uint32_t>(base_slot_index + 1U);
}

[[nodiscard]] constexpr std::uint32_t technology_amplification_node_id(
    FactionId faction_id,
    std::uint8_t tier_index,
    std::uint8_t base_slot_index,
    std::uint8_t amplification_choice_index) noexcept
{
    return technology_family_base(faction_id) +
        static_cast<std::uint32_t>(tier_index - 1U) * 100U +
        static_cast<std::uint32_t>(base_slot_index + 1U) * 10U +
        static_cast<std::uint32_t>(amplification_choice_index + 1U);
}

inline constexpr std::uint32_t k_tech_node_village_t1_field_rations =
    technology_base_node_id(FactionId {k_faction_village_committee}, 1U, 0U);
inline constexpr std::uint32_t k_tech_node_village_t1_shift_meals =
    technology_base_node_id(FactionId {k_faction_village_committee}, 1U, 1U);
inline constexpr std::uint32_t k_tech_node_village_t1_supply_runners =
    technology_base_node_id(FactionId {k_faction_village_committee}, 1U, 2U);
inline constexpr std::uint32_t k_tech_node_village_t1_field_rations_amp_recovery =
    technology_amplification_node_id(FactionId {k_faction_village_committee}, 1U, 0U, 0U);
inline constexpr std::uint32_t k_tech_node_village_t1_field_rations_amp_comfort =
    technology_amplification_node_id(FactionId {k_faction_village_committee}, 1U, 0U, 1U);

inline constexpr std::uint32_t k_tech_node_village_t2_stove_network =
    technology_base_node_id(FactionId {k_faction_village_committee}, 2U, 0U);

[[nodiscard]] constexpr TechnologyTierDef make_tier(
    FactionId faction_id,
    std::uint8_t tier_index,
    std::int32_t base_reputation_cost,
    std::string_view display_name) noexcept
{
    return TechnologyTierDef {
        faction_id,
        tier_index,
        {0U, 0U, 0U},
        base_reputation_cost,
        display_name};
}

[[nodiscard]] constexpr TechnologyNodeDef make_base_tech(
    std::uint32_t tech_node_id,
    FactionId faction_id,
    std::uint8_t tier_index,
    std::uint8_t base_slot_index,
    std::string_view display_name,
    std::string_view description) noexcept
{
    return TechnologyNodeDef {
        TechNodeId {tech_node_id},
        faction_id,
        tier_index,
        base_slot_index,
        TechnologyEntryKind::BaseTech,
        0U,
        TechNodeId {},
        display_name,
        description};
}

[[nodiscard]] constexpr TechnologyNodeDef make_amplification(
    std::uint32_t tech_node_id,
    FactionId faction_id,
    std::uint8_t tier_index,
    std::uint8_t base_slot_index,
    std::uint8_t amplification_choice_index,
    std::uint32_t parent_base_tech_id,
    std::string_view display_name,
    std::string_view description) noexcept
{
    return TechnologyNodeDef {
        TechNodeId {tech_node_id},
        faction_id,
        tier_index,
        base_slot_index,
        TechnologyEntryKind::Amplification,
        amplification_choice_index,
        TechNodeId {parent_base_tech_id},
        display_name,
        description};
}

inline constexpr std::string_view k_base_modifier_description =
    "Temporary modifier tech for the prototype faction branch pass.";
inline constexpr std::string_view k_amplification_modifier_description =
    "Temporary amplification modifier for the prototype faction branch pass.";

inline constexpr std::array<std::array<std::array<std::string_view, 3>, k_faction_tech_tier_count>, 3>
    k_faction_tier_base_names {{
        {{
            {{"Hydration Reserve", "Recovery Cadence", "Morale Buffer"}},
            {{"Endurance Rhythm", "Supply Surge", "Camp Cushion"}},
            {{"Field Tempo", "Route Stability", "Condition Guard"}},
            {{"Program Surge", "Relief Grid", "Frontline Resilience"}},
        }},
        {{
            {{"Wind Break", "Root Hold", "Dust Filter"}},
            {{"Shade Net", "Soil Lift", "Moisture Weave"}},
            {{"Shelter Belt", "Salt Draw", "Growth Lattice"}},
            {{"Canopy Wall", "Fertility Bloom", "Recovery Mesh"}},
        }},
        {{
            {{"Sensor Sweep", "Output Tune", "Field Shield"}},
            {{"Signal Net", "Servo Trim", "Stress Scan"}},
            {{"Pressure Map", "Adaptive Tuning", "Envelope Mesh"}},
            {{"Response Matrix", "Control Loop", "Hazard Null"}},
        }},
    }};

inline constexpr std::array<
    std::array<std::array<std::array<std::string_view, 2>, 3>, k_faction_tech_tier_count>,
    3>
    k_faction_tier_amplification_names {{
        {{
            {{{"Cool Draft", "Deep Reserve"}, {"Quick Patch", "Long Restore"}, {"Warm Pulse", "Steady Nerve"}}},
            {{{"Shift Burst", "Shift Hold"}, {"Supply Loop", "Supply Guard"}, {"Camp Ease", "Camp Shelter"}}},
            {{{"Fast Step", "Measured Step"}, {"Route Grip", "Route Cushion"}, {"Guard Rise", "Guard Hold"}}},
            {{{"Program Flow", "Program Guard"}, {"Relief Pulse", "Relief Hold"}, {"Frontline Surge", "Frontline Wall"}}},
        }},
        {{
            {{{"Wind Reach", "Wind Wall"}, {"Root Spread", "Root Depth"}, {"Dust Cut", "Dust Sink"}}},
            {{{"Shade Reach", "Shade Hold"}, {"Soil Spark", "Soil Hold"}, {"Moisture Catch", "Moisture Seal"}}},
            {{{"Belt Reach", "Belt Power"}, {"Salt Pull", "Salt Bleed"}, {"Growth Lift", "Growth Lock"}}},
            {{{"Canopy Drift", "Canopy Cover"}, {"Bloom Rise", "Bloom Hold"}, {"Mesh Spread", "Mesh Anchor"}}},
        }},
        {{
            {{{"Wide Sweep", "Dense Sweep"}, {"High Output", "Low Strain"}, {"Soft Shield", "Hard Shield"}}},
            {{{"Signal Reach", "Signal Purity"}, {"Servo Flow", "Servo Hold"}, {"Stress Read", "Stress Guard"}}},
            {{{"Pressure Read", "Pressure Guard"}, {"Tune Burst", "Tune Hold"}, {"Mesh Reach", "Mesh Depth"}}},
            {{{"Fast Response", "Deep Response"}, {"Loop Speed", "Loop Stability"}, {"Null Gate", "Null Shell"}}},
        }},
    }};

[[nodiscard]] constexpr std::size_t faction_name_index(FactionId faction_id) noexcept
{
    return faction_id.value <= 1U ? 0U : static_cast<std::size_t>(faction_id.value - 1U);
}

[[nodiscard]] constexpr std::string_view technology_base_name(
    FactionId faction_id,
    std::uint8_t tier_index,
    std::uint8_t base_slot_index) noexcept
{
    return k_faction_tier_base_names[faction_name_index(faction_id)][tier_index - 1U][base_slot_index];
}

[[nodiscard]] constexpr std::string_view technology_amplification_name(
    FactionId faction_id,
    std::uint8_t tier_index,
    std::uint8_t base_slot_index,
    std::uint8_t amplification_choice_index) noexcept
{
    return k_faction_tier_amplification_names[faction_name_index(faction_id)][tier_index - 1U][base_slot_index]
        [amplification_choice_index];
}

inline constexpr std::size_t k_prototype_technology_tier_count =
    k_prototype_faction_defs.size() * static_cast<std::size_t>(k_faction_tech_tier_count);
inline constexpr std::size_t k_prototype_technology_node_count =
    k_prototype_faction_defs.size() *
    static_cast<std::size_t>(k_faction_tech_tier_count) *
    static_cast<std::size_t>(k_faction_tech_base_choices_per_tier) *
    (1U + static_cast<std::size_t>(k_faction_tech_amplification_choices_per_base));

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
                k_faction_tech_tier_base_costs[tier_index - 1U],
                tier_index == 1U
                    ? "Tier 1"
                    : (tier_index == 2U
                            ? "Tier 2"
                            : (tier_index == 3U ? "Tier 3" : "Tier 4")));
        }
    }
    return tier_defs;
}();

inline const auto k_prototype_technology_node_defs = []()
{
    std::array<TechnologyNodeDef, k_prototype_technology_node_count> node_defs {};
    std::size_t index = 0U;
    for (const auto& faction_def : k_prototype_faction_defs)
    {
        for (std::uint8_t tier_index = 1U; tier_index <= k_faction_tech_tier_count; ++tier_index)
        {
            for (std::uint8_t base_slot_index = 0U;
                 base_slot_index < k_faction_tech_base_choices_per_tier;
                 ++base_slot_index)
            {
                const auto base_node_id =
                    technology_base_node_id(faction_def.faction_id, tier_index, base_slot_index);
                node_defs[index++] = make_base_tech(
                    base_node_id,
                    faction_def.faction_id,
                    tier_index,
                    base_slot_index,
                    technology_base_name(faction_def.faction_id, tier_index, base_slot_index),
                    k_base_modifier_description);

                for (std::uint8_t amplification_choice_index = 0U;
                     amplification_choice_index < k_faction_tech_amplification_choices_per_base;
                     ++amplification_choice_index)
                {
                    node_defs[index++] = make_amplification(
                        technology_amplification_node_id(
                            faction_def.faction_id,
                            tier_index,
                            base_slot_index,
                            amplification_choice_index),
                        faction_def.faction_id,
                        tier_index,
                        base_slot_index,
                        amplification_choice_index,
                        base_node_id,
                        technology_amplification_name(
                            faction_def.faction_id,
                            tier_index,
                            base_slot_index,
                            amplification_choice_index),
                        k_amplification_modifier_description);
                }
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
