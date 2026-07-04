#pragma once

namespace shared_framework::runtime
{
struct TimeScale final
{
    double in_game_minutes_per_day {1440.0};
    double real_seconds_per_day {1800.0};

    [[nodiscard]] constexpr double in_game_minutes_per_real_second() const noexcept
    {
        return real_seconds_per_day > 0.0
            ? in_game_minutes_per_day / real_seconds_per_day
            : 0.0;
    }
};

[[nodiscard]] constexpr double in_game_minutes_from_real_seconds(
    double real_seconds,
    const TimeScale& scale) noexcept
{
    return real_seconds > 0.0
        ? real_seconds * scale.in_game_minutes_per_real_second()
        : 0.0;
}
}  // namespace shared_framework::runtime
