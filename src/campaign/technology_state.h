#pragma once

#include "support/id_types.h"

#include <cstdint>
#include <vector>

namespace gs1
{
struct TechnologyState final
{
    std::int32_t reputation {0};
    std::vector<TechNodeId> purchased_node_ids {};
};
}  // namespace gs1
