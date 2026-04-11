#pragma once

#include "support/id_types.h"

namespace gs1
{
struct EventDef final
{
    EventTemplateId event_template_id {};
    float heat_pressure {0.0f};
    float wind_pressure {0.0f};
    float dust_pressure {0.0f};
};
}  // namespace gs1
