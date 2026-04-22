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

inline constexpr std::uint8_t k_faction_tech_tier_count = 3U;
inline constexpr std::uint8_t k_faction_technologies_per_tier = 2U;
inline constexpr std::uint8_t k_total_reputation_tier_count = 3U;
inline constexpr std::uint8_t k_reputation_unlocks_per_tier = 2U;

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
[[nodiscard]] std::span<const TotalReputationTierDef> all_total_reputation_tier_defs() noexcept;
[[nodiscard]] std::span<const ReputationUnlockDef> all_reputation_unlock_defs() noexcept;
[[nodiscard]] std::span<const TechnologyNodeDef> all_technology_node_defs() noexcept;
[[nodiscard]] std::span<const PlantId> all_initial_unlocked_plant_ids() noexcept;

[[nodiscard]] const TechnologyTierDef* find_technology_tier_def(
    FactionId faction_id,
    std::uint8_t tier_index) noexcept;
[[nodiscard]] const TotalReputationTierDef* find_total_reputation_tier_def(
    std::uint8_t tier_index) noexcept;
[[nodiscard]] const ReputationUnlockDef* find_reputation_unlock_def(
    std::uint32_t unlock_id) noexcept;
[[nodiscard]] const TechnologyNodeDef* find_technology_node_def(
    TechNodeId tech_node_id) noexcept;
}  // namespace gs1
