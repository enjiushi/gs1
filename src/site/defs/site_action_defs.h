#pragma once

#include "messages/game_message.h"
#include "site/action_state.h"

#include <cstdint>
#include <span>
#include <type_traits>

namespace gs1
{
struct SiteActionDef final
{
    ActionKind action_kind;
    float duration_minutes_per_unit;
    float energy_cost_per_unit;
    float hydration_cost_per_unit;
    float nourishment_cost_per_unit;
    float morale_cost_per_unit;
    float heat_to_energy_cost;
    float wind_to_energy_cost;
    float dust_to_energy_cost;
    float heat_to_hydration_cost;
    float wind_to_hydration_cost;
    float dust_to_hydration_cost;
    float heat_to_nourishment_cost;
    float wind_to_nourishment_cost;
    float dust_to_nourishment_cost;
    float heat_to_morale_cost;
    float wind_to_morale_cost;
    float dust_to_morale_cost;
    PlacementOccupancyLayer placement_occupancy_layer;
    bool requests_placement_reservation;
    bool requires_worker_approach;
    bool impacts_worker_movement;
};

[[nodiscard]] inline constexpr ActionKind action_kind_from_gs1(
    Gs1SiteActionKind action_kind) noexcept
{
    switch (action_kind)
    {
    case GS1_SITE_ACTION_PLANT:
        return ActionKind::Plant;
    case GS1_SITE_ACTION_BUILD:
        return ActionKind::Build;
    case GS1_SITE_ACTION_REPAIR:
        return ActionKind::Repair;
    case GS1_SITE_ACTION_WATER:
        return ActionKind::Water;
    case GS1_SITE_ACTION_CLEAR_BURIAL:
        return ActionKind::ClearBurial;
    case GS1_SITE_ACTION_CRAFT:
        return ActionKind::Craft;
    case GS1_SITE_ACTION_DRINK:
        return ActionKind::Drink;
    case GS1_SITE_ACTION_EAT:
        return ActionKind::Eat;
    case GS1_SITE_ACTION_HARVEST:
        return ActionKind::Harvest;
    default:
        return ActionKind::None;
    }
}

[[nodiscard]] std::span<const SiteActionDef> all_site_action_defs() noexcept;
[[nodiscard]] const SiteActionDef* find_site_action_def(ActionKind action_kind) noexcept;

[[nodiscard]] inline bool action_impacts_worker_movement(ActionKind action_kind) noexcept
{
    const auto* action_def = find_site_action_def(action_kind);
    return action_def != nullptr && action_def->impacts_worker_movement;
}

[[nodiscard]] inline bool gs1_action_impacts_worker_movement(
    Gs1SiteActionKind action_kind) noexcept
{
    return action_impacts_worker_movement(action_kind_from_gs1(action_kind));
}

static_assert(std::is_standard_layout_v<SiteActionDef>, "SiteActionDef must remain standard layout.");
static_assert(std::is_trivial_v<SiteActionDef>, "SiteActionDef must remain trivial.");
static_assert(std::is_trivially_copyable_v<SiteActionDef>, "SiteActionDef must remain trivially copyable.");
}  // namespace gs1
