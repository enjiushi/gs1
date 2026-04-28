#pragma once

#include "content/defs/item_defs.h"
#include "support/excavation_types.h"

#include <span>
#include <type_traits>

namespace gs1
{
struct ExcavationDepthDef final
{
    ExcavationDepth depth;
    float duration_minutes;
    float energy_cost_multiplier;
    float find_chance_percent;
    float common_tier_percent;
    float uncommon_tier_percent;
    float rare_tier_percent;
    float very_rare_tier_percent;
    float jackpot_tier_percent;
};

struct ExcavationLootEntryDef final
{
    ExcavationDepth depth;
    ItemId item_id;
    ExcavationLootTier tier;
    float percent_within_tier;
};

[[nodiscard]] std::span<const ExcavationDepthDef> all_excavation_depth_defs() noexcept;
[[nodiscard]] const ExcavationDepthDef* find_excavation_depth_def(ExcavationDepth depth) noexcept;
[[nodiscard]] std::span<const ExcavationLootEntryDef> all_excavation_loot_entry_defs() noexcept;

static_assert(std::is_standard_layout_v<ExcavationDepthDef>, "ExcavationDepthDef must remain standard layout.");
static_assert(std::is_trivial_v<ExcavationDepthDef>, "ExcavationDepthDef must remain trivial.");
static_assert(std::is_trivially_copyable_v<ExcavationDepthDef>, "ExcavationDepthDef must remain trivially copyable.");
static_assert(std::is_standard_layout_v<ExcavationLootEntryDef>, "ExcavationLootEntryDef must remain standard layout.");
static_assert(std::is_trivial_v<ExcavationLootEntryDef>, "ExcavationLootEntryDef must remain trivial.");
static_assert(std::is_trivially_copyable_v<ExcavationLootEntryDef>, "ExcavationLootEntryDef must remain trivially copyable.");
}  // namespace gs1
