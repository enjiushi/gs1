#pragma once

#include <cstdint>

namespace shared_framework::runtime
{
struct FixedStepRunResult final
{
    std::uint32_t fixed_steps_executed {0};
    double remaining_accumulator_seconds {0.0};
};

class FixedStepRunner final
{
public:
    [[nodiscard]] static constexpr FixedStepRunResult consume_steps(
        double accumulator_seconds,
        double real_delta_seconds,
        double fixed_step_seconds) noexcept
    {
        const double safe_accumulator = accumulator_seconds > 0.0 ? accumulator_seconds : 0.0;
        const double safe_delta = real_delta_seconds > 0.0 ? real_delta_seconds : 0.0;
        if (fixed_step_seconds <= 0.0)
        {
            return {0U, safe_accumulator + safe_delta};
        }

        double remaining = safe_accumulator + safe_delta;
        std::uint32_t steps = 0U;
        while (remaining >= fixed_step_seconds)
        {
            remaining -= fixed_step_seconds;
            ++steps;
        }

        return {steps, remaining};
    }
};
}  // namespace shared_framework::runtime
