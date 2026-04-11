#pragma once

#include "support/id_types.h"

#include <vector>

namespace gs1
{
struct ModifierChannelTotals final
{
    float heat {0.0f};
    float wind {0.0f};
    float dust {0.0f};
    float moisture {0.0f};
    float fertility {0.0f};
    float salinity {0.0f};
    float growth_pressure {0.0f};
    float salinity_density_cap {0.0f};
    float plant_density {0.0f};
    float health {0.0f};
    float hydration {0.0f};
    float nourishment {0.0f};
    float energy_cap {0.0f};
    float energy {0.0f};
    float morale {0.0f};
    float work_efficiency {0.0f};
};

struct ModifierState final
{
    std::vector<ModifierId> active_run_modifier_ids {};
    std::vector<ModifierId> active_nearby_aura_modifier_ids {};
    ModifierChannelTotals resolved_channel_totals {};
};
}  // namespace gs1
