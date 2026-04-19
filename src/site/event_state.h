#pragma once

#include "support/id_types.h"

namespace gs1
{
enum class EventPhase : std::uint32_t
{
    None = 0,
    Warning = 1,
    Build = 2,
    Peak = 3,
    Decay = 4,
    Aftermath = 5
};

struct EventState final
{
    std::optional<EventTemplateId> active_event_template_id {};
    EventPhase event_phase {EventPhase::None};
    double phase_minutes_remaining {0.0};
    double minutes_until_next_wave {0.0};
    float event_heat_pressure {0.0f};
    float event_wind_pressure {0.0f};
    float event_dust_pressure {0.0f};
    float aftermath_relief_resolved {0.0f};
    std::uint32_t wave_sequence_index {0U};
};
}  // namespace gs1
