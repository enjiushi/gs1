#pragma once

#include "support/id_types.h"

#include <cstdint>

namespace gs1
{
enum class TechnologyGrantState : std::uint8_t
{
    Locked = 0,
    Available = 1,
    Effective = 2
};

struct TechnologyNodeGrantState final
{
    TechNodeId tech_node_id {};
    TechnologyGrantState grant_state {TechnologyGrantState::Locked};
    std::uint8_t reserved0[3] {};
};

inline constexpr std::uint16_t k_max_technology_node_grant_count = 96U;

struct TechnologyState final
{
    std::int32_t total_reputation {0};
    std::uint16_t technology_node_grant_count {0};
    std::uint16_t reserved0 {0};
    TechnologyNodeGrantState technology_node_grants[k_max_technology_node_grant_count] {};
};
}  // namespace gs1
