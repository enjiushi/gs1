#pragma once

#include "support/id_types.h"

#include <array>
#include <cstdint>
#include <string_view>

namespace gs1
{
struct FactionDef final
{
    FactionId faction_id {};
    std::string_view display_name {};
    std::uint32_t assistant_package_id {0};
    std::string_view assistant_display_name {};
    std::int32_t assistant_unlock_reputation {0};
};

inline constexpr std::uint32_t k_faction_village_committee = 1U;
inline constexpr std::uint32_t k_faction_forestry_bureau = 2U;
inline constexpr std::uint32_t k_faction_agricultural_university = 3U;

inline constexpr std::array<FactionDef, 3> k_prototype_faction_defs {{
    FactionDef {
        FactionId {k_faction_village_committee},
        "Village Committee",
        1001U,
        "Workforce Support",
        10},
    FactionDef {
        FactionId {k_faction_forestry_bureau},
        "Forestry Bureau",
        1002U,
        "Plant-Water Support",
        10},
    FactionDef {
        FactionId {k_faction_agricultural_university},
        "Agricultural University",
        1003U,
        "Device Upgrade Support",
        10},
}};

[[nodiscard]] inline constexpr const FactionDef* find_faction_def(FactionId faction_id) noexcept
{
    for (const auto& faction_def : k_prototype_faction_defs)
    {
        if (faction_def.faction_id == faction_id)
        {
            return &faction_def;
        }
    }

    return nullptr;
}
}  // namespace gs1
