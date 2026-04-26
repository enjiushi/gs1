#pragma once

#include "support/id_types.h"

#include <cstdint>
#include <vector>

namespace gs1
{
struct TechnologyPurchaseRecord final
{
    TechNodeId tech_node_id {};
};

struct TechnologyState final
{
    std::int32_t total_reputation {0};
    std::vector<TechnologyPurchaseRecord> purchased_nodes {};
};
}  // namespace gs1
