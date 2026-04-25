#pragma once

#include "content/defs/faction_defs.h"
#include "content/defs/plant_defs.h"
#include "support/id_types.h"

#include <cstdint>
#include <span>
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
    std::uint8_t tier_index {0};
    std::uint8_t reserved0[3] {};
    std::string_view display_name {};
};

struct ReputationUnlockDef final
{
    std::uint32_t unlock_id {0};
    ReputationUnlockKind unlock_kind {ReputationUnlockKind::Plant};
    std::uint8_t reserved0[3] {};
    std::int32_t reputation_requirement {0};
    std::uint32_t content_id {0};
    std::string_view display_name {};
    std::string_view description {};
};

struct TechnologyNodeDef final
{
    TechNodeId tech_node_id {};
    FactionId faction_id {};
    std::uint8_t tier_index {0};
    std::uint8_t reserved0[3] {};
    TechnologyEntryKind entry_kind {TechnologyEntryKind::GlobalModifier};
    std::uint8_t reserved1[3] {};
    std::int32_t reputation_cost {0};
    std::int32_t cash_cost {0};
    ModifierId linked_modifier_id {};
    std::uint32_t mechanism_change_id {0};
    bool is_todo_placeholder {true};
    std::uint8_t reserved2[3] {};
    std::string_view display_name {};
    std::string_view description {};
};

inline constexpr std::uint8_t k_faction_tech_tier_count = 3U;
inline constexpr std::uint8_t k_initial_unlocked_plant_count = 2U;

[[nodiscard]] constexpr std::uint32_t technology_modifier_id(TechNodeId tech_node_id) noexcept
{
    return 20000U + tech_node_id.value;
}

[[nodiscard]] constexpr std::uint32_t technology_mechanism_change_id(TechNodeId tech_node_id) noexcept
{
    return 40000U + tech_node_id.value;
}

[[nodiscard]] constexpr std::uint32_t technology_node_id(
    FactionId faction_id,
    std::uint8_t tier_index) noexcept
{
    return 1000U +
        static_cast<std::uint32_t>(faction_id.value) * 100U +
        static_cast<std::uint32_t>(tier_index);
}

[[nodiscard]] constexpr TechNodeId previous_faction_technology_node_id(
    FactionId faction_id,
    std::uint8_t tier_index) noexcept
{
    return TechNodeId {technology_node_id(faction_id, static_cast<std::uint8_t>(tier_index - 1U))};
}

inline constexpr std::uint32_t k_tech_node_village_t1_field_briefing =
    technology_node_id(FactionId {k_faction_village_committee}, 1U);
inline constexpr std::uint32_t k_tech_node_village_t2_field_logistics =
    technology_node_id(FactionId {k_faction_village_committee}, 2U);
inline constexpr std::uint32_t k_tech_node_village_t3_regional_coordination =
    technology_node_id(FactionId {k_faction_village_committee}, 3U);
inline constexpr std::uint32_t k_tech_node_forestry_t1_seed_vigor =
    technology_node_id(FactionId {k_faction_forestry_bureau}, 1U);
inline constexpr std::uint32_t k_tech_node_forestry_t2_shelter_growth =
    technology_node_id(FactionId {k_faction_forestry_bureau}, 2U);
inline constexpr std::uint32_t k_tech_node_forestry_t3_habitat_buffer =
    technology_node_id(FactionId {k_faction_forestry_bureau}, 3U);
inline constexpr std::uint32_t k_tech_node_university_t1_survey_bias =
    technology_node_id(FactionId {k_faction_agricultural_university}, 1U);
inline constexpr std::uint32_t k_tech_node_university_t2_field_precision =
    technology_node_id(FactionId {k_faction_agricultural_university}, 2U);
inline constexpr std::uint32_t k_tech_node_university_t3_systems_method =
    technology_node_id(FactionId {k_faction_agricultural_university}, 3U);
inline constexpr std::uint32_t k_tech_node_t1_field_briefing =
    k_tech_node_village_t1_field_briefing;
inline constexpr std::uint32_t k_tech_node_t2_meal_rotation =
    k_tech_node_village_t2_field_logistics;

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

[[nodiscard]] std::span<const TechnologyTierDef> all_technology_tier_defs() noexcept;
[[nodiscard]] std::span<const ReputationUnlockDef> all_reputation_unlock_defs() noexcept;
[[nodiscard]] std::span<const TechnologyNodeDef> all_technology_node_defs() noexcept;
[[nodiscard]] std::span<const PlantId> all_initial_unlocked_plant_ids() noexcept;

[[nodiscard]] const TechnologyTierDef* find_technology_tier_def(std::uint8_t tier_index) noexcept;
[[nodiscard]] const ReputationUnlockDef* find_reputation_unlock_def(
    std::uint32_t unlock_id) noexcept;
[[nodiscard]] const TechnologyNodeDef* find_technology_node_def(
    TechNodeId tech_node_id) noexcept;
[[nodiscard]] const TechnologyNodeDef* find_faction_technology_node_def(
    FactionId faction_id,
    std::uint8_t tier_index) noexcept;
}  // namespace gs1
