#include "site/systems/weather_event_system.h"

#include "content/defs/task_defs.h"
#include "content/prototype_content.h"
#include "runtime/runtime_clock.h"
#include "site/site_projection_update_flags.h"
#include "site/site_run_state.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace gs1
{
namespace
{
constexpr float k_mild_weather_heat = 15.0f;
constexpr float k_mild_weather_wind = 10.0f;
constexpr float k_mild_weather_dust = 5.0f;
constexpr double k_event_ramp_up_minutes = 5.0;
constexpr double k_event_peak_hold_minutes = 5.0;
constexpr double k_event_ramp_down_minutes = 5.0;
constexpr float k_weather_epsilon = 0.0001f;
constexpr float k_weather_transition_rate_per_second = 2.4f;
constexpr double k_min_wave_delay_minutes = 4.0;
constexpr double k_max_wave_delay_minutes = 8.0;
constexpr float k_weather_meter_max = 100.0f;

bool has_pending_site_transition_message(
    const GameMessageQueue& message_queue,
    std::uint32_t site_id)
{
    for (const auto& message : message_queue)
    {
        if (message.type == GameMessageType::SiteAttemptEnded &&
            message.payload_as<SiteAttemptEndedMessage>().site_id == site_id)
        {
            return true;
        }
    }

    return false;
}

void emit_site_one_weather_probe_log(
    SiteSystemContext<WeatherEventSystem>& context,
    const char* label,
    float heat,
    float wind,
    float dust,
    float wind_direction_degrees)
{
    if (!context.world.has_world() || context.world.site_id_value() != 1U)
    {
        return;
    }

    GameMessage message {};
    PresentLogMessage payload {};
    payload.level = GS1_LOG_LEVEL_DEBUG;
    std::snprintf(
        payload.text,
        sizeof(payload.text),
        "S1 wx %s %.0f/%.0f/%.0f dir%.0f",
        label,
        heat,
        wind,
        dust,
        wind_direction_degrees);
    message.type = GameMessageType::PresentLog;
    message.set_payload(payload);
    context.message_queue.push_back(message);
}

struct SiteBaselineWeather final
{
    float heat {0.0f};
    float wind {0.0f};
    float dust {0.0f};
    float wind_direction_degrees {0.0f};
};

[[nodiscard]] SiteBaselineWeather resolve_site_baseline_weather(
    std::uint32_t site_id_value) noexcept
{
    const auto* site_content = find_prototype_site_content(SiteId {site_id_value});
    if (site_content == nullptr)
    {
        return {};
    }

    return SiteBaselineWeather {
        site_content->default_weather_heat,
        site_content->default_weather_wind,
        site_content->default_weather_dust,
        site_content->default_weather_wind_direction_degrees};
}

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

bool objective_uses_repeating_weather_waves(
    const SiteObjectiveState& objective) noexcept
{
    return objective.type == SiteObjectiveType::HighwayProtection ||
        objective.type == SiteObjectiveType::CashTargetSurvival;
}

bool objective_wave_window_is_active(
    const SiteObjectiveState& objective,
    double world_time_minutes) noexcept
{
    if (!objective_uses_repeating_weather_waves(objective))
    {
        return false;
    }

    if (objective.type == SiteObjectiveType::HighwayProtection)
    {
        return objective.time_limit_minutes > 0.0 &&
            world_time_minutes < objective.time_limit_minutes;
    }

    return true;
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

bool has_active_event(const EventState& event) noexcept
{
    return event.active_event_template_id.has_value();
}

bool onboarding_chain_weather_locked(
    SiteId site_id,
    const TaskBoardState& board) noexcept
{
    for (const auto& task : board.visible_tasks)
    {
        if (task.runtime_list_kind == TaskRuntimeListKind::Claimed)
        {
            continue;
        }

        for (const auto& seed_def : all_site_onboarding_task_seed_defs())
        {
            if (seed_def.site_id == site_id &&
                seed_def.task_template_id == task.task_template_id)
            {
                return true;
            }
        }
    }

    return false;
}

double resolve_peak_end_time_minutes(const EventState& event) noexcept
{
    const double peak_time = std::max(event.peak_time_minutes, event.start_time_minutes);
    return peak_time + std::max(event.peak_duration_minutes, 0.0);
}

double resolve_end_time_minutes(const EventState& event) noexcept
{
    return std::max(event.end_time_minutes, resolve_peak_end_time_minutes(event));
}

float resolve_event_pressure_scale(
    const EventState& event,
    double world_time_minutes) noexcept
{
    if (!has_active_event(event))
    {
        return 0.0f;
    }

    const double start_time = event.start_time_minutes;
    const double peak_time = std::max(event.peak_time_minutes, start_time);
    const double peak_end_time = resolve_peak_end_time_minutes(event);
    const double end_time = resolve_end_time_minutes(event);
    if (world_time_minutes < start_time || end_time <= start_time)
    {
        return 0.0f;
    }

    if (world_time_minutes < peak_time)
    {
        const double rise_duration = std::max(peak_time - start_time, 0.0001);
        return static_cast<float>(std::clamp(
            (world_time_minutes - start_time) / rise_duration,
            0.0,
            1.0));
    }

    if (world_time_minutes < peak_end_time)
    {
        return 1.0f;
    }

    if (world_time_minutes < end_time)
    {
        const double fall_duration = std::max(end_time - peak_end_time, 0.0001);
        return static_cast<float>(std::clamp(
            1.0 - ((world_time_minutes - peak_end_time) / fall_duration),
            0.0,
            1.0));
    }

    return 0.0f;
}

float resolve_event_wind_direction(
    const SiteSystemContext<WeatherEventSystem>& context) noexcept
{
    const auto& objective = context.world.read_objective();
    if (objective_uses_repeating_weather_waves(objective))
    {
        const auto& event = context.world.read_event();
        const float edge_base = resolve_edge_base_wind_direction(objective.target_edge);
        const auto wave_index = std::max(event.wave_sequence_index, 1U);
        const float random_offset =
            (normalized_hash(
                 context.world.site_attempt_seed() +
                 static_cast<std::uint64_t>(wave_index) * 97ULL) *
                70.0f) -
            35.0f;
        return normalize_wind_direction_degrees(edge_base + random_offset);
    }

    return 74.0f;
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

void clear_event_timeline(EventState& event) noexcept
{
    event.active_event_template_id.reset();
    event.start_time_minutes = 0.0;
    event.peak_time_minutes = 0.0;
    event.peak_duration_minutes = 0.0;
    event.end_time_minutes = 0.0;
    event.event_heat_pressure = 0.0f;
    event.event_wind_pressure = 0.0f;
    event.event_dust_pressure = 0.0f;
}

void start_next_wave(SiteSystemContext<WeatherEventSystem>& context)
{
    auto& event = context.world.own_event();
    const double world_time_minutes = context.world.read_time().world_time_minutes;
    event.active_event_template_id = EventTemplateId {1U};
    event.start_time_minutes = world_time_minutes;
    event.peak_time_minutes = world_time_minutes + k_event_ramp_up_minutes;
    event.peak_duration_minutes = k_event_peak_hold_minutes;
    event.end_time_minutes =
        event.peak_time_minutes + event.peak_duration_minutes + k_event_ramp_down_minutes;
    event.minutes_until_next_wave = 0.0;
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
    const auto baseline_weather = resolve_site_baseline_weather(context.world.site_id_value());
    if (has_active_event(event))
    {
        return GS1_STATUS_OK;
    }

    if (objective_uses_repeating_weather_waves(objective))
    {
        weather.forecast_profile_state.forecast_profile_id = 2U;
        weather.site_weather_bias = 0.0f;
        event.minutes_until_next_wave = resolve_next_wave_delay_minutes(context, event.wave_sequence_index);
        apply_site_weather(
            context,
            baseline_weather.heat,
            baseline_weather.wind,
            baseline_weather.dust,
            baseline_weather.wind_direction_degrees);
        emit_site_one_weather_probe_log(
            context,
            "start",
            baseline_weather.heat,
            baseline_weather.wind,
            baseline_weather.dust,
            baseline_weather.wind_direction_degrees);
        return GS1_STATUS_OK;
    }

    if (context.world.site_id_value() == 1U)
    {
        weather.forecast_profile_state.forecast_profile_id = 1U;
    }

    weather.site_weather_bias = 0.0f;
    apply_site_weather(
        context,
        baseline_weather.heat,
        baseline_weather.wind,
        baseline_weather.dust,
        baseline_weather.wind_direction_degrees);
    emit_site_one_weather_probe_log(
        context,
        "start",
        baseline_weather.heat,
        baseline_weather.wind,
        baseline_weather.dust,
        baseline_weather.wind_direction_degrees);

    return GS1_STATUS_OK;
}

void WeatherEventSystem::run(SiteSystemContext<WeatherEventSystem>& context)
{
    const double step_minutes =
        runtime_minutes_from_real_seconds(context.fixed_step_seconds);
    auto& event = context.world.own_event();
    const auto& objective = context.world.read_objective();
    const auto& board = context.world.read_task_board();
    const auto baseline_weather = resolve_site_baseline_weather(context.world.site_id_value());
    const double world_time_minutes = context.world.read_time().world_time_minutes;
    if (onboarding_chain_weather_locked(context.world.site_id(), board))
    {
        clear_event_timeline(event);
        event.minutes_until_next_wave = 0.0;
        smooth_site_weather_toward(
            context,
            baseline_weather.heat,
            baseline_weather.wind,
            baseline_weather.dust,
            baseline_weather.wind_direction_degrees);
        return;
    }

    if (!has_active_event(event))
    {
        clear_event_timeline(event);
        if (objective_wave_window_is_active(objective, world_time_minutes) &&
            !has_pending_site_transition_message(
                context.message_queue,
                context.world.site_id_value()))
        {
            event.minutes_until_next_wave =
                std::max(0.0, event.minutes_until_next_wave - step_minutes);
            if (event.minutes_until_next_wave <= 0.0)
            {
                start_next_wave(context);
            }
        }

        smooth_site_weather_toward(
            context,
            baseline_weather.heat,
            baseline_weather.wind,
            baseline_weather.dust,
            baseline_weather.wind_direction_degrees);
        return;
    }

    const auto previous_template_id = event.active_event_template_id;
    const double end_time_minutes = resolve_end_time_minutes(event);
    if (world_time_minutes >= end_time_minutes)
    {
        clear_event_timeline(event);
        if (objective_wave_window_is_active(objective, world_time_minutes) &&
            !has_pending_site_transition_message(
                context.message_queue,
                context.world.site_id_value()))
        {
            event.minutes_until_next_wave =
                resolve_next_wave_delay_minutes(context, event.wave_sequence_index);
        }
        context.world.mark_projection_dirty(SITE_PROJECTION_UPDATE_WEATHER);
        smooth_site_weather_toward(
            context,
            baseline_weather.heat,
            baseline_weather.wind,
            baseline_weather.dust,
            baseline_weather.wind_direction_degrees);
        return;
    }

    const float pressure_scale = resolve_event_pressure_scale(event, world_time_minutes);
    event.event_heat_pressure = k_mild_weather_heat * pressure_scale;
    event.event_wind_pressure = k_mild_weather_wind * pressure_scale;
    event.event_dust_pressure = k_mild_weather_dust * pressure_scale;
    if (event.active_event_template_id != previous_template_id)
    {
        context.world.mark_projection_dirty(SITE_PROJECTION_UPDATE_WEATHER);
    }
    smooth_site_weather_toward(
        context,
        std::clamp(baseline_weather.heat + event.event_heat_pressure, 0.0f, k_weather_meter_max),
        std::clamp(baseline_weather.wind + event.event_wind_pressure, 0.0f, k_weather_meter_max),
        std::clamp(baseline_weather.dust + event.event_dust_pressure, 0.0f, k_weather_meter_max),
        resolve_event_wind_direction(context));
}
}  // namespace gs1
