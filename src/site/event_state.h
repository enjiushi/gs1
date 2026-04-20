#pragma once

#include "support/id_types.h"

namespace gs1
{
struct EventState final
{
    std::optional<EventTemplateId> active_event_template_id {};
    double start_time_minutes {0.0};
    double peak_time_minutes {0.0};
    double peak_duration_minutes {0.0};
    double end_time_minutes {0.0};
    double minutes_until_next_wave {0.0};
    float event_heat_pressure {0.0f};
    float event_wind_pressure {0.0f};
    float event_dust_pressure {0.0f};
    float aftermath_relief_resolved {0.0f};
    std::uint32_t wave_sequence_index {0U};
};
}  // namespace gs1
