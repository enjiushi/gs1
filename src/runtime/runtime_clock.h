#pragma once

namespace gs1
{
struct RuntimeClock final
{
    double fixed_step_seconds {0.25};
    double step_minutes {0.2};
};
}  // namespace gs1
