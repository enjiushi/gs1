#pragma once

#include "support/id_types.h"

#include <cstdint>
#include <vector>

namespace gs1
{
struct TechnologyState final
{
    std::int32_t reputation {0};
    std::uint32_t available_tech_picks {0};
    std::vector<TechNodeId> unlocked_node_ids {};
    std::vector<TechNodeId> applied_content_update_node_ids {};
};
}  // namespace gs1
