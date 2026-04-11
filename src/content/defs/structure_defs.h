#pragma once

#include "support/id_types.h"

namespace gs1
{
struct StructureDef final
{
    StructureId structure_id {};
    float durability {0.0f};
    float support_value {0.0f};
};
}  // namespace gs1
