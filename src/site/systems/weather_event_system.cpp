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

void apply_site_weather(SiteRunState& site_run, float heat, float wind, float dust)
{
    const bool changed =
        std::fabs(site_run.weather.weather_heat - heat) > k_weather_epsilon ||
        std::fabs(site_run.weather.weather_wind - wind) > k_weather_epsilon ||
        std::fabs(site_run.weather.weather_dust - dust) > k_weather_epsilon;
    if (!changed)
    {
        return;
    }

    site_run.weather.weather_heat = heat;
    site_run.weather.weather_wind = wind;
    site_run.weather.weather_dust = dust;
    site_run.pending_projection_update_flags |= SITE_PROJECTION_UPDATE_WEATHER;
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
    SiteSystemContext& context,
    const GameCommand& command)
{
    if (command.type != GameCommandType::SiteRunStarted)
    {
        return GS1_STATUS_OK;
    }

    auto& site_run = context.site_run;
    if (site_run.site_id.value != 1U || site_run.event.event_phase != EventPhase::None)
    {
        return GS1_STATUS_OK;
    }

    site_run.weather.forecast_profile_state.forecast_profile_id = 1U;
    site_run.weather.site_weather_bias = 0.0f;
    site_run.event.active_event_template_id = EventTemplateId{1U};
    site_run.event.event_phase = EventPhase::Warning;
    site_run.event.phase_minutes_remaining = 5.0;
    site_run.event.event_heat_pressure = k_mild_weather_heat * resolve_phase_pressure(EventPhase::Warning);
    site_run.event.event_wind_pressure = k_mild_weather_wind * resolve_phase_pressure(EventPhase::Warning);
    site_run.event.event_dust_pressure = k_mild_weather_dust * resolve_phase_pressure(EventPhase::Warning);
    site_run.event.aftermath_relief_resolved = 0.0f;

    site_run.pending_projection_update_flags |= SITE_PROJECTION_UPDATE_WEATHER;
    apply_site_weather(site_run, k_mild_weather_heat, k_mild_weather_wind, k_mild_weather_dust);

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

void WeatherEventSystem::run(SiteSystemContext& context)
{
    auto& site_run = context.site_run;
    if (site_run.event.event_phase == EventPhase::None ||
        !site_run.event.active_event_template_id.has_value())
    {
        return;
    }

    const auto previous_phase = site_run.event.event_phase;
    const auto previous_template_id = site_run.event.active_event_template_id;

    site_run.event.phase_minutes_remaining -= k_phase_step_minutes;
    if (site_run.event.phase_minutes_remaining <= 0.0)
    {
        if (site_run.event.event_phase == EventPhase::Aftermath)
        {
            site_run.event.event_phase = EventPhase::None;
            site_run.event.phase_minutes_remaining = 0.0;
            site_run.event.active_event_template_id.reset();
            site_run.event.event_heat_pressure = 0.0f;
            site_run.event.event_wind_pressure = 0.0f;
            site_run.event.event_dust_pressure = 0.0f;
            site_run.event.aftermath_relief_resolved = 1.0f;
            site_run.pending_projection_update_flags |= SITE_PROJECTION_UPDATE_WEATHER;
            apply_site_weather(site_run, 0.0f, 0.0f, 0.0f);
            return;
        }

        site_run.event.event_phase = advance_phase(site_run.event.event_phase);
        site_run.event.phase_minutes_remaining = 5.0;
    }

    const float pressure_scale = resolve_phase_pressure(site_run.event.event_phase);
    site_run.event.event_heat_pressure = k_mild_weather_heat * pressure_scale;
    site_run.event.event_wind_pressure = k_mild_weather_wind * pressure_scale;
    site_run.event.event_dust_pressure = k_mild_weather_dust * pressure_scale;
    if (site_run.event.event_phase != previous_phase ||
        site_run.event.active_event_template_id != previous_template_id)
    {
        site_run.pending_projection_update_flags |= SITE_PROJECTION_UPDATE_WEATHER;
    }
    apply_site_weather(
        site_run,
        k_mild_weather_heat * pressure_scale,
        k_mild_weather_wind * pressure_scale,
        k_mild_weather_dust * pressure_scale);
}
}  // namespace gs1
