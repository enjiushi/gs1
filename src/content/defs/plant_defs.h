#pragma once

#include "support/id_types.h"

namespace gs1
{
struct PlantDef final
{
    PlantId plant_id {};
    float growth_rate {0.0f};
    float salinity_tolerance {0.0f};
};
}  // namespace gs1
