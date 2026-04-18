#pragma once

#include "support/id_types.h"

#include <array>
#include <cstdint>
#include <string_view>

namespace gs1
{
enum class CraftingStationKind : std::uint8_t
{
    None = 0,
    Cooking = 1,
    Fabrication = 2
};

struct StructureDef final
{
    StructureId structure_id {};
    std::string_view display_name {};
    float durability {0.0f};
    float support_value {0.0f};
    std::uint16_t storage_slot_count {0U};
    CraftingStationKind crafting_station_kind {CraftingStationKind::None};
    bool grants_storage {false};
    std::uint8_t footprint_width {1U};
    std::uint8_t footprint_height {1U};
};

inline constexpr std::uint32_t k_structure_camp_stove = 201U;
inline constexpr std::uint32_t k_structure_workbench = 202U;
inline constexpr std::uint32_t k_structure_storage_crate = 203U;

inline constexpr std::array<StructureDef, 3> k_prototype_structure_defs {{
    StructureDef {
        StructureId {k_structure_camp_stove},
        "Camp Stove",
        1.0f,
        0.1f,
        6U,
        CraftingStationKind::Cooking,
        true,
        1U,
        1U},
    StructureDef {
        StructureId {k_structure_workbench},
        "Workbench",
        1.0f,
        0.2f,
        8U,
        CraftingStationKind::Fabrication,
        true,
        1U,
        1U},
    StructureDef {
        StructureId {k_structure_storage_crate},
        "Storage Crate",
        1.0f,
        0.0f,
        10U,
        CraftingStationKind::None,
        true,
        1U,
        1U},
}};

[[nodiscard]] inline constexpr const StructureDef* find_structure_def(StructureId structure_id) noexcept
{
    for (const auto& structure_def : k_prototype_structure_defs)
    {
        if (structure_def.structure_id == structure_id)
        {
            return &structure_def;
        }
    }

    return nullptr;
}

[[nodiscard]] inline constexpr bool structure_has_storage(StructureId structure_id) noexcept
{
    const auto* structure_def = find_structure_def(structure_id);
    return structure_def != nullptr && structure_def->grants_storage && structure_def->storage_slot_count > 0U;
}

[[nodiscard]] inline constexpr bool structure_is_crafting_station(StructureId structure_id) noexcept
{
    const auto* structure_def = find_structure_def(structure_id);
    return structure_def != nullptr &&
        structure_def->crafting_station_kind != CraftingStationKind::None;
}
}  // namespace gs1
