#pragma once

#include "site/modifier_state.h"
#include "support/id_types.h"

#include <cstdint>
#include <span>
#include <string_view>

namespace gs1
{
enum class ModifierPresetKind : std::uint8_t
{
    NearbyAura = 0,
    RunModifier = 1
};

struct ModifierDef final
{
    ModifierPresetKind preset_kind {ModifierPresetKind::NearbyAura};
    ModifierId modifier_id {};
    std::string_view display_name {};
    std::string_view description {};
    std::uint16_t duration_eight_hour_blocks {0U};
    std::uint16_t reserved0 {0U};
    ModifierChannelTotals totals {};
    ActionCostModifierState action_cost_modifiers {};
};

[[nodiscard]] std::span<const ModifierDef> all_modifier_defs() noexcept;
[[nodiscard]] const ModifierDef* find_modifier_def(ModifierId id) noexcept;
[[nodiscard]] ModifierChannelTotals resolve_nearby_aura_modifier_preset(ModifierId id) noexcept;
[[nodiscard]] ModifierChannelTotals resolve_run_modifier_preset(ModifierId id) noexcept;
}  // namespace gs1
