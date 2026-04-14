#include "site/systems/weather_event_system.h"

#include "site/site_projection_update_flags.h"
#include "site/site_run_state.h"

#include <cmath>

namespace gs1
{
namespace
{
constexpr float k_mild_weather_heat = 15.0f;
constexpr float k_mild_weather_wind = 10.0f;
constexpr float k_mild_weather_dust = 5.0f;

constexpr double k_phase_step_minutes = 0.5;
constexpr float k_weather_epsilon = 0.0001f;

void apply_site_weather(
    SiteSystemContext<WeatherEventSystem>& context,
    float heat,
    float wind,
    float dust)
{
    auto& weather = context.world.own_weather();
    const bool changed =
        std::fabs(weather.weather_heat - heat) > k_weather_epsilon ||
        std::fabs(weather.weather_wind - wind) > k_weather_epsilon ||
        std::fabs(weather.weather_dust - dust) > k_weather_epsilon;
    if (!changed)
    {
        return;
    }

    weather.weather_heat = heat;
    weather.weather_wind = wind;
    weather.weather_dust = dust;
    context.world.mark_projection_dirty(SITE_PROJECTION_UPDATE_WEATHER);
}

float resolve_phase_pressure(EventPhase phase)
{
    switch (phase)
    {
    case EventPhase::Warning:
    case EventPhase::Build:
        return 0.5f;
    case EventPhase::Peak:
        return 1.0f;
    case EventPhase::Decay:
    case EventPhase::Aftermath:
        return 0.3f;
    default:
        return 0.0f;
    }
}
}  // namespace

bool WeatherEventSystem::subscribes_to(GameCommandType type) noexcept
{
    return type == GameCommandType::SiteRunStarted;
}

Gs1Status WeatherEventSystem::process_command(
    SiteSystemContext<WeatherEventSystem>& context,
    const GameCommand& command)
{
    if (command.type != GameCommandType::SiteRunStarted)
    {
        return GS1_STATUS_OK;
    }

    auto& weather = context.world.own_weather();
    auto& event = context.world.own_event();
    if (context.world.site_id_value() != 1U || event.event_phase != EventPhase::None)
    {
        return GS1_STATUS_OK;
    }

    weather.forecast_profile_state.forecast_profile_id = 1U;
    weather.site_weather_bias = 0.0f;
    event.active_event_template_id = EventTemplateId{1U};
    event.event_phase = EventPhase::Warning;
    event.phase_minutes_remaining = 5.0;
    event.event_heat_pressure = k_mild_weather_heat * resolve_phase_pressure(EventPhase::Warning);
    event.event_wind_pressure = k_mild_weather_wind * resolve_phase_pressure(EventPhase::Warning);
    event.event_dust_pressure = k_mild_weather_dust * resolve_phase_pressure(EventPhase::Warning);
    event.aftermath_relief_resolved = 0.0f;

    context.world.mark_projection_dirty(SITE_PROJECTION_UPDATE_WEATHER);
    apply_site_weather(context, k_mild_weather_heat, k_mild_weather_wind, k_mild_weather_dust);

    return GS1_STATUS_OK;
}

namespace
{
EventPhase advance_phase(EventPhase current) noexcept
{
    switch (current)
    {
    case EventPhase::Warning:
        return EventPhase::Build;
    case EventPhase::Build:
        return EventPhase::Peak;
    case EventPhase::Peak:
        return EventPhase::Decay;
    case EventPhase::Decay:
        return EventPhase::Aftermath;
    default:
        return EventPhase::Aftermath;
    }
}
}  // namespace

void WeatherEventSystem::run(SiteSystemContext<WeatherEventSystem>& context)
{
    auto& event = context.world.own_event();
    if (event.event_phase == EventPhase::None ||
        !event.active_event_template_id.has_value())
    {
        return;
    }

    const auto previous_phase = event.event_phase;
    const auto previous_template_id = event.active_event_template_id;

    event.phase_minutes_remaining -= k_phase_step_minutes;
    if (event.phase_minutes_remaining <= 0.0)
    {
        if (event.event_phase == EventPhase::Aftermath)
        {
            event.event_phase = EventPhase::None;
            event.phase_minutes_remaining = 0.0;
            event.active_event_template_id.reset();
            event.event_heat_pressure = 0.0f;
            event.event_wind_pressure = 0.0f;
            event.event_dust_pressure = 0.0f;
            event.aftermath_relief_resolved = 1.0f;
            context.world.mark_projection_dirty(SITE_PROJECTION_UPDATE_WEATHER);
            apply_site_weather(context, 0.0f, 0.0f, 0.0f);
            return;
        }

        event.event_phase = advance_phase(event.event_phase);
        event.phase_minutes_remaining = 5.0;
    }

    const float pressure_scale = resolve_phase_pressure(event.event_phase);
    event.event_heat_pressure = k_mild_weather_heat * pressure_scale;
    event.event_wind_pressure = k_mild_weather_wind * pressure_scale;
    event.event_dust_pressure = k_mild_weather_dust * pressure_scale;
    if (event.event_phase != previous_phase ||
        event.active_event_template_id != previous_template_id)
    {
        context.world.mark_projection_dirty(SITE_PROJECTION_UPDATE_WEATHER);
    }
    apply_site_weather(
        context,
        k_mild_weather_heat * pressure_scale,
        k_mild_weather_wind * pressure_scale,
        k_mild_weather_dust * pressure_scale);
}
}  // namespace gs1
