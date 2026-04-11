#pragma once

#include <cstdint>

namespace gs1
{
struct FixedStepRunResult final
{
    std::uint32_t fixed_steps_executed {0};
    double remaining_accumulator_seconds {0.0};
};

class FixedStepRunner final
{
public:
    [[nodiscard]] static FixedStepRunResult consume_steps(
        double accumulator_seconds,
        double real_delta_seconds,
        double fixed_step_seconds) noexcept;
};
}  // namespace gs1
