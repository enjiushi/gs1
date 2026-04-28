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

enum class TechnologyNodeKind : std::uint8_t
{
    BaseTech = 0,
    Enhancement = 1
};

enum class ReputationUnlockKind : std::uint8_t
{
    Plant = 0,
    Device = 1,
    Recipe = 2
};

enum class TechnologyGrantedContentKind : std::uint8_t
{
    None = 0,
    Item = 1,
    Plant = 2,
    Structure = 3,
    Recipe = 4
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
    TechnologyNodeKind node_kind {TechnologyNodeKind::BaseTech};
    std::uint8_t enhancement_choice_index {0};
    std::uint8_t reserved0[1] {};
    TechnologyEntryKind entry_kind {TechnologyEntryKind::GlobalModifier};
    std::uint8_t reserved1[3] {};
    std::int32_t reputation_requirement {0};
    std::uint32_t internal_cost_cash_points {0U};
    float unlock_effect_parameter {1.0f};
    float effect_parameter_per_bonus_reputation {0.0f};
    ModifierId linked_modifier_id {};
    std::uint32_t mechanism_change_id {0};
    TechnologyGrantedContentKind granted_content_kind {TechnologyGrantedContentKind::None};
    std::uint8_t reserved2[3] {};
    std::uint32_t granted_content_id {0U};
    bool is_todo_placeholder {true};
    std::uint8_t reserved3[3] {};
    std::string_view display_name {};
    std::string_view description {};
};

inline constexpr std::uint8_t k_faction_tech_tier_count = 8U;
inline constexpr std::uint8_t k_faction_base_tech_reputation_max_requirement =
    k_faction_tech_tier_count;
inline constexpr std::uint8_t k_faction_enhancement_reputation_min_requirement =
    k_faction_base_tech_reputation_max_requirement;
inline constexpr std::uint8_t k_faction_enhancement_reputation_max_requirement =
    k_faction_enhancement_reputation_min_requirement + k_faction_tech_tier_count - 1U;
inline constexpr std::uint8_t k_technology_enhancement_choice_count = 2U;
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
    std::uint8_t tier_index,
    std::uint8_t enhancement_choice_index = 0U) noexcept
{
    return 1000U +
        static_cast<std::uint32_t>(faction_id.value) * 1000U +
        static_cast<std::uint32_t>(tier_index) * 10U +
        static_cast<std::uint32_t>(enhancement_choice_index);
}

[[nodiscard]] constexpr std::uint32_t base_technology_node_id(
    FactionId faction_id,
    std::uint8_t tier_index) noexcept
{
    return technology_node_id(faction_id, tier_index, 0U);
}

[[nodiscard]] constexpr std::uint32_t enhancement_technology_node_id(
    FactionId faction_id,
    std::uint8_t tier_index,
    std::uint8_t enhancement_choice_index) noexcept
{
    return technology_node_id(faction_id, tier_index, enhancement_choice_index);
}

inline constexpr std::uint32_t k_tech_node_village_t1_field_briefing =
    base_technology_node_id(FactionId {k_faction_village_committee}, 1U);
inline constexpr std::uint32_t k_tech_node_village_t2_field_logistics =
    base_technology_node_id(FactionId {k_faction_village_committee}, 2U);
inline constexpr std::uint32_t k_tech_node_village_t3_regional_coordination =
    base_technology_node_id(FactionId {k_faction_village_committee}, 3U);
inline constexpr std::uint32_t k_tech_node_forestry_t1_seed_vigor =
    base_technology_node_id(FactionId {k_faction_forestry_bureau}, 1U);
inline constexpr std::uint32_t k_tech_node_forestry_t2_shelter_growth =
    base_technology_node_id(FactionId {k_faction_forestry_bureau}, 2U);
inline constexpr std::uint32_t k_tech_node_forestry_t3_habitat_buffer =
    base_technology_node_id(FactionId {k_faction_forestry_bureau}, 3U);
inline constexpr std::uint32_t k_tech_node_university_t1_survey_bias =
    base_technology_node_id(FactionId {k_faction_agricultural_university}, 1U);
inline constexpr std::uint32_t k_tech_node_university_t2_field_precision =
    base_technology_node_id(FactionId {k_faction_agricultural_university}, 2U);
inline constexpr std::uint32_t k_tech_node_university_t3_systems_method =
    base_technology_node_id(FactionId {k_faction_agricultural_university}, 3U);
inline constexpr std::uint32_t k_tech_node_village_t1_field_briefing_choice_a =
    enhancement_technology_node_id(FactionId {k_faction_village_committee}, 1U, 1U);
inline constexpr std::uint32_t k_tech_node_village_t1_field_briefing_choice_b =
    enhancement_technology_node_id(FactionId {k_faction_village_committee}, 1U, 2U);
inline constexpr std::uint32_t k_tech_node_forestry_t1_seed_vigor_choice_a =
    enhancement_technology_node_id(FactionId {k_faction_forestry_bureau}, 1U, 1U);
inline constexpr std::uint32_t k_tech_node_forestry_t1_seed_vigor_choice_b =
    enhancement_technology_node_id(FactionId {k_faction_forestry_bureau}, 1U, 2U);
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

[[nodiscard]] constexpr std::string_view technology_node_kind_display_name(
    TechnologyNodeKind kind) noexcept
{
    switch (kind)
    {
    case TechnologyNodeKind::BaseTech:
        return "Base Tech";
    case TechnologyNodeKind::Enhancement:
        return "Enhancement";
    default:
        return {};
    }
}

[[nodiscard]] constexpr std::string_view technology_granted_content_kind_display_name(
    TechnologyGrantedContentKind kind) noexcept
{
    switch (kind)
    {
    case TechnologyGrantedContentKind::Item:
        return "Item";
    case TechnologyGrantedContentKind::Plant:
        return "Plant";
    case TechnologyGrantedContentKind::Structure:
        return "Structure";
    case TechnologyGrantedContentKind::Recipe:
        return "Recipe";
    case TechnologyGrantedContentKind::None:
    default:
        return "None";
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
