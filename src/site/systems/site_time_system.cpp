#include "site/systems/site_time_system.h"

#include "runtime/runtime_clock.h"

#include <cmath>

namespace gs1
{
namespace
{
DayPhase resolve_day_phase(double world_time_minutes)
{
    const double minute_in_day = std::fmod(world_time_minutes, k_runtime_minutes_per_day);

    if (minute_in_day < 360.0)
    {
        return DayPhase::Dawn;
    }
    if (minute_in_day < 900.0)
    {
        return DayPhase::Day;
    }
    if (minute_in_day < 1140.0)
    {
        return DayPhase::Dusk;
    }
    return DayPhase::Night;
}
}  // namespace

void SiteTimeSystem::run(SiteSystemContext<SiteTimeSystem>& context)
{
    auto& clock = context.world.own_time();
    clock.world_time_minutes +=
        runtime_minutes_from_real_seconds(context.fixed_step_seconds);
    clock.day_index =
        static_cast<std::uint32_t>(clock.world_time_minutes / k_runtime_minutes_per_day);
    clock.day_phase = resolve_day_phase(clock.world_time_minutes);
}
}  // namespace gs1
