#pragma once

namespace shared_framework::runtime
{
struct TimeScale final
{
    double in_game_minutes_per_day {0.0};
    double real_seconds_per_day {0.0};
};

[[nodiscard]] constexpr double in_game_minutes_from_real_seconds(
    double real_seconds,
    const TimeScale& time_scale) noexcept
{
    if (real_seconds <= 0.0 || time_scale.real_seconds_per_day <= 0.0)
    {
        return 0.0;
    }

    return real_seconds * (time_scale.in_game_minutes_per_day / time_scale.real_seconds_per_day);
}
}  // namespace shared_framework::runtime
