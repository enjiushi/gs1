#pragma once

#include "support/id_types.h"

namespace gs1
{
struct TechnologyNodeDef final
{
    TechNodeId tech_node_id {};
    std::int32_t reputation_requirement {0};
};
}  // namespace gs1
