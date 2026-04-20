#include "site/systems/weather_event_system.h"

#include "runtime/runtime_clock.h"
#include "site/site_projection_update_flags.h"
#include "site/site_run_state.h"

#include <algorithm>
#include <cmath>

namespace gs1
{
namespace
{
constexpr float k_mild_weather_heat = 15.0f;
constexpr float k_mild_weather_wind = 10.0f;
constexpr float k_mild_weather_dust = 5.0f;
constexpr double k_phase_duration_minutes = 5.0;
constexpr float k_weather_epsilon = 0.0001f;
constexpr float k_weather_transition_rate_per_second = 2.4f;
constexpr double k_min_wave_delay_minutes = 4.0;
constexpr double k_max_wave_delay_minutes = 8.0;

void apply_site_weather(
    SiteSystemContext<WeatherEventSystem>& context,
    float heat,
    float wind,
    float dust,
    float wind_direction_degrees)
{
    auto& weather = context.world.own_weather();
    const bool changed =
        std::fabs(weather.weather_heat - heat) > k_weather_epsilon ||
        std::fabs(weather.weather_wind - wind) > k_weather_epsilon ||
        std::fabs(weather.weather_dust - dust) > k_weather_epsilon ||
        std::fabs(weather.weather_wind_direction_degrees - wind_direction_degrees) > k_weather_epsilon;
    if (!changed)
    {
        return;
    }

    weather.weather_heat = heat;
    weather.weather_wind = wind;
    weather.weather_dust = dust;
    weather.weather_wind_direction_degrees = wind_direction_degrees;
    context.world.mark_projection_dirty(SITE_PROJECTION_UPDATE_WEATHER);
}

float resolve_phase_pressure(EventPhase phase)
{
    switch (phase)
    {
    case EventPhase::Warning:
        return 0.0f;
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

float resolve_weather_lerp_factor(double fixed_step_seconds) noexcept
{
    if (fixed_step_seconds <= 0.0)
    {
        return 1.0f;
    }

    return std::clamp(
        1.0f - std::exp(-k_weather_transition_rate_per_second * static_cast<float>(fixed_step_seconds)),
        0.0f,
        1.0f);
}

float smooth_weather_channel(float current, float target, float lerp_factor) noexcept
{
    return current + (target - current) * lerp_factor;
}

float normalize_wind_direction_degrees(float value) noexcept
{
    const float wrapped = std::fmod(value, 360.0f);
    return wrapped < 0.0f ? wrapped + 360.0f : wrapped;
}

float smooth_wind_direction_degrees(float current, float target, float lerp_factor) noexcept
{
    const float normalized_current = normalize_wind_direction_degrees(current);
    const float normalized_target = normalize_wind_direction_degrees(target);
    float delta = normalized_target - normalized_current;
    if (delta > 180.0f)
    {
        delta -= 360.0f;
    }
    else if (delta < -180.0f)
    {
        delta += 360.0f;
    }

    return normalize_wind_direction_degrees(normalized_current + delta * lerp_factor);
}

void smooth_site_weather_toward(
    SiteSystemContext<WeatherEventSystem>& context,
    float target_heat,
    float target_wind,
    float target_dust,
    float target_wind_direction_degrees)
{
    const auto& current_weather = context.world.read_weather();
    const float lerp_factor = resolve_weather_lerp_factor(context.fixed_step_seconds);
    apply_site_weather(
        context,
        smooth_weather_channel(current_weather.weather_heat, target_heat, lerp_factor),
        smooth_weather_channel(current_weather.weather_wind, target_wind, lerp_factor),
        smooth_weather_channel(current_weather.weather_dust, target_dust, lerp_factor),
        smooth_wind_direction_degrees(
            current_weather.weather_wind_direction_degrees,
            target_wind_direction_degrees,
            lerp_factor));
}

double resolve_phase_duration_minutes(EventPhase phase) noexcept
{
    switch (phase)
    {
    case EventPhase::Warning:
    case EventPhase::Build:
    case EventPhase::Peak:
    case EventPhase::Decay:
    case EventPhase::Aftermath:
        return k_phase_duration_minutes;
    default:
        return 0.0;
    }
}

std::uint64_t hash_u64(std::uint64_t value) noexcept
{
    value ^= value >> 33U;
    value *= 0xff51afd7ed558ccdULL;
    value ^= value >> 33U;
    value *= 0xc4ceb9fe1a85ec53ULL;
    value ^= value >> 33U;
    return value;
}

float normalized_hash(std::uint64_t value) noexcept
{
    constexpr double k_u32_max = 4294967295.0;
    return static_cast<float>(
        static_cast<double>(hash_u64(value) & 0xffffffffULL) / k_u32_max);
}

float resolve_edge_base_wind_direction(SiteObjectiveTargetEdge edge) noexcept
{
    switch (edge)
    {
    case SiteObjectiveTargetEdge::North:
        return 270.0f;
    case SiteObjectiveTargetEdge::East:
        return 0.0f;
    case SiteObjectiveTargetEdge::South:
        return 90.0f;
    case SiteObjectiveTargetEdge::West:
        return 180.0f;
    default:
        return 0.0f;
    }
}

float resolve_phase_wind_direction(
    const SiteSystemContext<WeatherEventSystem>& context,
    EventPhase phase) noexcept
{
    const auto& objective = context.world.read_objective();
    if (objective.type == SiteObjectiveType::HighwayProtection)
    {
        const auto& event = context.world.read_event();
        const float edge_base = resolve_edge_base_wind_direction(objective.target_edge);
        const float random_offset =
            (normalized_hash(
                 context.world.site_attempt_seed() +
                 static_cast<std::uint64_t>(event.wave_sequence_index + 1U) * 97ULL) *
                70.0f) -
            35.0f;
        float phase_offset = 0.0f;
        switch (phase)
        {
        case EventPhase::Warning:
            phase_offset = -12.0f;
            break;
        case EventPhase::Build:
            phase_offset = -4.0f;
            break;
        case EventPhase::Peak:
            phase_offset = 11.0f;
            break;
        case EventPhase::Decay:
            phase_offset = 4.0f;
            break;
        case EventPhase::Aftermath:
            phase_offset = -7.0f;
            break;
        default:
            break;
        }

        return normalize_wind_direction_degrees(edge_base + random_offset + phase_offset);
    }

    switch (phase)
    {
    case EventPhase::Warning:
        return 35.0f;
    case EventPhase::Build:
        return 52.0f;
    case EventPhase::Peak:
        return 74.0f;
    case EventPhase::Decay:
        return 58.0f;
    case EventPhase::Aftermath:
        return 41.0f;
    default:
        return 0.0f;
    }
}

double resolve_next_wave_delay_minutes(
    const SiteSystemContext<WeatherEventSystem>& context,
    std::uint32_t wave_sequence_index) noexcept
{
    const float sample =
        normalized_hash(
            context.world.site_attempt_seed() +
            static_cast<std::uint64_t>(wave_sequence_index + 1U) * 131ULL);
    return k_min_wave_delay_minutes +
        (k_max_wave_delay_minutes - k_min_wave_delay_minutes) * static_cast<double>(sample);
}

void start_next_wave(SiteSystemContext<WeatherEventSystem>& context)
{
    auto& event = context.world.own_event();
    event.active_event_template_id = EventTemplateId {1U};
    event.event_phase = EventPhase::Warning;
    event.phase_minutes_remaining = resolve_phase_duration_minutes(EventPhase::Warning);
    event.minutes_until_next_wave = 0.0;
    event.aftermath_relief_resolved = 0.0f;
    event.wave_sequence_index += 1U;
    context.world.mark_projection_dirty(SITE_PROJECTION_UPDATE_WEATHER);
}
}  // namespace

bool WeatherEventSystem::subscribes_to(GameMessageType type) noexcept
{
    return type == GameMessageType::SiteRunStarted;
}

Gs1Status WeatherEventSystem::process_message(
    SiteSystemContext<WeatherEventSystem>& context,
    const GameMessage& message)
{
    if (message.type != GameMessageType::SiteRunStarted)
    {
        return GS1_STATUS_OK;
    }

    auto& weather = context.world.own_weather();
    auto& event = context.world.own_event();
    const auto& objective = context.world.read_objective();
    if (event.event_phase != EventPhase::None)
    {
        return GS1_STATUS_OK;
    }

    if (objective.type == SiteObjectiveType::HighwayProtection)
    {
        weather.forecast_profile_state.forecast_profile_id = 2U;
        weather.site_weather_bias = 0.0f;
        event.aftermath_relief_resolved = 0.0f;
        event.minutes_until_next_wave = resolve_next_wave_delay_minutes(context, event.wave_sequence_index);
        return GS1_STATUS_OK;
    }

    if (context.world.site_id_value() != 1U)
    {
        return GS1_STATUS_OK;
    }

    weather.forecast_profile_state.forecast_profile_id = 1U;
    weather.site_weather_bias = 0.0f;
    event.aftermath_relief_resolved = 0.0f;

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
    const double step_minutes =
        runtime_minutes_from_real_seconds(context.fixed_step_seconds);
    auto& event = context.world.own_event();
    const auto& objective = context.world.read_objective();
    if (event.event_phase == EventPhase::None ||
        !event.active_event_template_id.has_value())
    {
        if (objective.type == SiteObjectiveType::HighwayProtection &&
            objective.time_limit_minutes > 0.0 &&
            context.world.read_time().world_time_minutes < objective.time_limit_minutes)
        {
            event.minutes_until_next_wave =
                std::max(0.0, event.minutes_until_next_wave - step_minutes);
            if (event.minutes_until_next_wave <= 0.0)
            {
                start_next_wave(context);
            }
        }

        smooth_site_weather_toward(context, 0.0f, 0.0f, 0.0f, 0.0f);
        return;
    }

    const auto previous_phase = event.event_phase;
    const auto previous_template_id = event.active_event_template_id;

    event.phase_minutes_remaining -= step_minutes;
    if (event.phase_minutes_remaining <= 0.0)
    {
        if (event.event_phase == EventPhase::Aftermath)
        {
            event.event_phase = EventPhase::None;
            event.phase_minutes_remaining = 0.0;
            event.active_event_template_id.reset();
            if (objective.type == SiteObjectiveType::HighwayProtection &&
                objective.time_limit_minutes > 0.0 &&
                context.world.read_time().world_time_minutes < objective.time_limit_minutes)
            {
                event.minutes_until_next_wave =
                    resolve_next_wave_delay_minutes(context, event.wave_sequence_index);
            }
            event.event_heat_pressure = 0.0f;
            event.event_wind_pressure = 0.0f;
            event.event_dust_pressure = 0.0f;
            event.aftermath_relief_resolved = 1.0f;
            context.world.mark_projection_dirty(SITE_PROJECTION_UPDATE_WEATHER);
        }
        else
        {
            event.event_phase = advance_phase(event.event_phase);
            event.phase_minutes_remaining = resolve_phase_duration_minutes(event.event_phase);
        }
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
    smooth_site_weather_toward(
        context,
        k_mild_weather_heat * pressure_scale,
        k_mild_weather_wind * pressure_scale,
        k_mild_weather_dust * pressure_scale,
        resolve_phase_wind_direction(context, event.event_phase));
}
}  // namespace gs1
