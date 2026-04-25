#pragma once

#include "support/id_types.h"

#include <cstdint>
#include <vector>

namespace gs1
{
struct TechnologyPurchaseRecord final
{
    TechNodeId tech_node_id {};
    FactionId reputation_faction_id {};
    std::int32_t reputation_spent {0};
};

struct TechnologyState final
{
    std::int32_t total_reputation {0};
    std::vector<TechnologyPurchaseRecord> purchased_nodes {};
};
}  // namespace gs1
