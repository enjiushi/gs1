#pragma once

#include "messages/game_message.h"
#include "site/action_state.h"

#include <array>
#include <cstdint>
#include <type_traits>

namespace gs1
{
struct SiteActionDef final
{
    ActionKind action_kind;
    float duration_minutes_per_unit;
    float energy_cost_per_unit;
    float hydration_cost_per_unit;
    PlacementOccupancyLayer placement_occupancy_layer;
    bool requests_placement_reservation;
    bool requires_worker_approach;
    std::uint8_t reserved0;
};

inline constexpr std::array<SiteActionDef, 6> k_prototype_site_action_defs {{
    SiteActionDef {
        ActionKind::Plant,
        1.0f,
        2.0f,
        0.75f,
        PlacementOccupancyLayer::GroundCover,
        true,
        true,
        0U},
    SiteActionDef {
        ActionKind::Build,
        2.5f,
        2.0f,
        0.75f,
        PlacementOccupancyLayer::Structure,
        true,
        true,
        0U},
    SiteActionDef {
        ActionKind::Repair,
        1.5f,
        0.0f,
        0.0f,
        PlacementOccupancyLayer::None,
        false,
        false,
        0U},
    SiteActionDef {
        ActionKind::Water,
        0.75f,
        1.0f,
        0.35f,
        PlacementOccupancyLayer::None,
        false,
        false,
        0U},
    SiteActionDef {
        ActionKind::ClearBurial,
        1.0f,
        1.5f,
        0.5f,
        PlacementOccupancyLayer::None,
        false,
        false,
        0U},
    SiteActionDef {
        ActionKind::Craft,
        1.0f,
        1.5f,
        0.5f,
        PlacementOccupancyLayer::None,
        false,
        true,
        0U},
}};

[[nodiscard]] inline constexpr const SiteActionDef* find_site_action_def(ActionKind action_kind) noexcept
{
    for (const auto& action_def : k_prototype_site_action_defs)
    {
        if (action_def.action_kind == action_kind)
        {
            return &action_def;
        }
    }

    return nullptr;
}

static_assert(std::is_standard_layout_v<SiteActionDef>, "SiteActionDef must remain standard layout.");
static_assert(std::is_trivial_v<SiteActionDef>, "SiteActionDef must remain trivial.");
static_assert(std::is_trivially_copyable_v<SiteActionDef>, "SiteActionDef must remain trivially copyable.");
}  // namespace gs1
