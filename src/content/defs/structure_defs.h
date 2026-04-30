#pragma once

#include "support/id_types.h"

#include <cstdint>
#include <span>
#include <string_view>

namespace gs1
{
enum class CraftingStationKind : std::uint8_t
{
    None = 0,
    Cooking = 1,
    Fabrication = 2,
    Chemistry = 3
};

struct StructureDef final
{
    StructureId structure_id {};
    std::string_view display_name {};
    float durability {0.0f};
    float integrity_loss_per_second_at_max_weather {0.0f};
    std::uint8_t aura_size {0U};
    float wind_protection_value {0.0f};
    std::uint8_t wind_protection_range {0U};
    float heat_protection_value {0.0f};
    float dust_protection_value {0.0f};
    float fertility_improve_value {0.0f};
    float salinity_reduction_value {0.0f};
    float irrigation_value {0.0f};
    std::uint16_t storage_slot_count {0U};
    CraftingStationKind crafting_station_kind {CraftingStationKind::None};
    bool grants_storage {false};
    std::uint8_t footprint_width {1U};
    std::uint8_t footprint_height {1U};
};

inline constexpr std::uint32_t k_structure_camp_stove = 201U;
inline constexpr std::uint32_t k_structure_workbench = 202U;
inline constexpr std::uint32_t k_structure_storage_crate = 203U;
inline constexpr std::uint32_t k_structure_wind_fence = 204U;
inline constexpr std::uint32_t k_structure_chemistry_station = 205U;

[[nodiscard]] std::span<const StructureDef> all_structure_defs() noexcept;
[[nodiscard]] const StructureDef* find_structure_def(StructureId structure_id) noexcept;
[[nodiscard]] bool structure_has_storage(StructureId structure_id) noexcept;
[[nodiscard]] bool structure_is_crafting_station(StructureId structure_id) noexcept;
}  // namespace gs1
