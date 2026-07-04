#pragma once

#include "shared_framework/runtime/foundation/time_scale.h"

namespace gs1
{
inline constexpr double k_default_fixed_step_seconds = 0.25;
inline constexpr double k_runtime_minutes_per_day = 1440.0;
inline constexpr double k_runtime_real_seconds_per_day = 30.0 * 60.0;
inline constexpr double k_runtime_minutes_per_real_second =
    k_runtime_minutes_per_day / k_runtime_real_seconds_per_day;
inline constexpr shared_framework::runtime::TimeScale k_runtime_time_scale {
    k_runtime_minutes_per_day,
    k_runtime_real_seconds_per_day};

[[nodiscard]] constexpr double runtime_minutes_from_real_seconds(
    double real_seconds) noexcept
{
    return shared_framework::runtime::in_game_minutes_from_real_seconds(real_seconds, k_runtime_time_scale);
}
}  // namespace gs1
