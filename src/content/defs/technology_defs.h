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

struct TechnologyTierDef final
{
    std::uint8_t tier_index {0};
    std::uint8_t reserved0[3] {};
    std::int32_t reputation_requirement {0};
    std::string_view display_name {};
};

struct FactionTechnologyTierDef final
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
    TechnologyNodeKind node_kind {TechnologyNodeKind::BaseTech};
    FactionId faction_id {};
    std::uint8_t tier_index {0};
    std::uint8_t slot_index {0};
    std::uint8_t base_tech_slot_index {0};
    std::uint8_t reserved0 {0};
    TechnologyEntryKind entry_kind {TechnologyEntryKind::GlobalModifier};
    std::uint8_t reserved1[3] {};
    std::int32_t cash_cost {0};
    ModifierId linked_modifier_id {};
    std::uint32_t mechanism_change_id {0};
    bool is_todo_placeholder {true};
    std::uint8_t reserved2[3] {};
    std::string_view display_name {};
    std::string_view description {};
};

inline constexpr std::uint8_t k_faction_tech_tier_count = 3U;
inline constexpr std::uint8_t k_base_technologies_per_tier = 2U;
inline constexpr std::uint8_t k_faction_enhancements_per_base_tech = 1U;
inline constexpr std::uint8_t k_total_reputation_tier_count = 3U;
inline constexpr std::uint8_t k_reputation_unlocks_per_tier = 2U;

[[nodiscard]] constexpr std::uint32_t technology_base_node_id(
    std::uint8_t tier_index,
    std::uint8_t slot_index) noexcept
{
    return 1000U +
        static_cast<std::uint32_t>(tier_index - 1U) * 100U +
        static_cast<std::uint32_t>(slot_index + 1U);
}

[[nodiscard]] constexpr std::uint32_t technology_enhancement_node_id(
    FactionId faction_id,
    std::uint8_t tier_index,
    std::uint8_t base_tech_slot_index,
    std::uint8_t enhancement_index) noexcept
{
    return faction_id.value * 10000U +
        5000U +
        static_cast<std::uint32_t>(tier_index - 1U) * 100U +
        static_cast<std::uint32_t>(base_tech_slot_index) * 10U +
        static_cast<std::uint32_t>(enhancement_index + 1U);
}

[[nodiscard]] constexpr TechNodeId technology_parent_base_node_id(
    std::uint8_t tier_index,
    std::uint8_t base_tech_slot_index) noexcept
{
    return TechNodeId {technology_base_node_id(tier_index, base_tech_slot_index)};
}

[[nodiscard]] constexpr std::uint32_t technology_modifier_id(TechNodeId tech_node_id) noexcept
{
    return 20000U + tech_node_id.value;
}

[[nodiscard]] constexpr std::uint32_t technology_mechanism_change_id(TechNodeId tech_node_id) noexcept
{
    return 40000U + tech_node_id.value;
}

[[nodiscard]] constexpr std::uint32_t reputation_unlock_id(
    std::uint8_t tier_index,
    std::uint8_t slot_index) noexcept
{
    return 5000U +
        static_cast<std::uint32_t>(tier_index - 1U) * 10U +
        static_cast<std::uint32_t>(slot_index + 1U);
}

inline constexpr std::uint32_t k_tech_node_t1_field_briefing =
    technology_base_node_id(1U, 0U);
inline constexpr std::uint32_t k_tech_node_t1_supply_standards =
    technology_base_node_id(1U, 1U);
inline constexpr std::uint32_t k_tech_node_t2_meal_rotation =
    technology_base_node_id(2U, 0U);
inline constexpr std::uint32_t k_tech_node_village_t1_field_briefing_enhancement =
    technology_enhancement_node_id(FactionId {k_faction_village_committee}, 1U, 0U, 0U);
inline constexpr std::uint32_t k_tech_node_forestry_t1_field_briefing_enhancement =
    technology_enhancement_node_id(FactionId {k_faction_forestry_bureau}, 1U, 0U, 0U);
inline constexpr std::uint32_t k_tech_node_university_t1_field_briefing_enhancement =
    technology_enhancement_node_id(FactionId {k_faction_agricultural_university}, 1U, 0U, 0U);

[[nodiscard]] constexpr std::string_view technology_node_kind_display_name(
    TechnologyNodeKind kind) noexcept
{
    switch (kind)
    {
    case TechnologyNodeKind::BaseTech:
        return "Base";
    case TechnologyNodeKind::Enhancement:
        return "Enhancement";
    default:
        return {};
    }
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

[[nodiscard]] std::span<const TechnologyTierDef> all_technology_tier_defs() noexcept;
[[nodiscard]] std::span<const FactionTechnologyTierDef> all_faction_technology_tier_defs() noexcept;
[[nodiscard]] std::span<const TotalReputationTierDef> all_total_reputation_tier_defs() noexcept;
[[nodiscard]] std::span<const ReputationUnlockDef> all_reputation_unlock_defs() noexcept;
[[nodiscard]] std::span<const TechnologyNodeDef> all_technology_node_defs() noexcept;
[[nodiscard]] std::span<const PlantId> all_initial_unlocked_plant_ids() noexcept;

[[nodiscard]] const TechnologyTierDef* find_technology_tier_def(std::uint8_t tier_index) noexcept;
[[nodiscard]] const FactionTechnologyTierDef* find_faction_technology_tier_def(
    FactionId faction_id,
    std::uint8_t tier_index) noexcept;
[[nodiscard]] const TotalReputationTierDef* find_total_reputation_tier_def(
    std::uint8_t tier_index) noexcept;
[[nodiscard]] const ReputationUnlockDef* find_reputation_unlock_def(
    std::uint32_t unlock_id) noexcept;
[[nodiscard]] const TechnologyNodeDef* find_technology_node_def(
    TechNodeId tech_node_id) noexcept;
}  // namespace gs1
