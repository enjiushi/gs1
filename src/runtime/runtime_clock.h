#pragma once

namespace gs1
{
inline constexpr double k_default_fixed_step_seconds = 0.25;
inline constexpr double k_runtime_minutes_per_day = 1440.0;
inline constexpr double k_runtime_real_seconds_per_day = 30.0 * 60.0;
inline constexpr double k_runtime_minutes_per_real_second =
    k_runtime_minutes_per_day / k_runtime_real_seconds_per_day;

[[nodiscard]] constexpr double runtime_minutes_from_real_seconds(
    double real_seconds) noexcept
{
    return real_seconds > 0.0
        ? real_seconds * k_runtime_minutes_per_real_second
        : 0.0;
}
}  // namespace gs1
