#pragma once

#include "site/modifier_state.h"
#include "support/id_types.h"

#include <cstdint>
#include <span>

namespace gs1
{
enum class ModifierPresetKind : std::uint8_t
{
    NearbyAura = 0,
    RunModifier = 1
};

struct ModifierPresetDef final
{
    ModifierPresetKind preset_kind {ModifierPresetKind::NearbyAura};
    std::uint32_t preset_index {0};
    ModifierChannelTotals totals {};
};

[[nodiscard]] std::span<const ModifierPresetDef> all_nearby_aura_modifier_presets() noexcept;
[[nodiscard]] std::span<const ModifierPresetDef> all_run_modifier_presets() noexcept;
[[nodiscard]] ModifierChannelTotals resolve_nearby_aura_modifier_preset(ModifierId id) noexcept;
[[nodiscard]] ModifierChannelTotals resolve_run_modifier_preset(ModifierId id) noexcept;
}  // namespace gs1
