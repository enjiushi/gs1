#include "site/systems/site_time_system.h"

#include "content/defs/gameplay_tuning_defs.h"
#include "messages/game_message.h"
#include "runtime/game_runtime.h"
#include "runtime/runtime_clock.h"

#include <algorithm>
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
    const double step_minutes =
        runtime_minutes_from_real_seconds(context.fixed_step_seconds);
    clock.world_time_minutes += step_minutes;
    clock.day_index =
        static_cast<std::uint32_t>(clock.world_time_minutes / k_runtime_minutes_per_day);
    clock.day_phase = resolve_day_phase(clock.world_time_minutes);

    const double refresh_interval_minutes =
        static_cast<double>(gameplay_tuning_def().task_board.refresh_interval_minutes);
    if (refresh_interval_minutes <= 0.0)
    {
        return;
    }

    clock.task_refresh_accumulator += step_minutes;
    while (clock.task_refresh_accumulator + 0.0001 >= refresh_interval_minutes)
    {
        clock.task_refresh_accumulator =
            std::max(0.0, clock.task_refresh_accumulator - refresh_interval_minutes);
        GameMessage refresh_message {};
        refresh_message.type = GameMessageType::SiteRefreshTick;
        refresh_message.set_payload(SiteRefreshTickMessage {
            SITE_REFRESH_TICK_TASK_BOARD | SITE_REFRESH_TICK_PHONE_BUY_STOCK});
        context.message_queue.push_back(refresh_message);
    }
}
GS1_IMPLEMENT_RUNTIME_SITE_RUN_ONLY_SYSTEM(
    SiteTimeSystem,
    GS1_RUNTIME_PROFILE_SYSTEM_SITE_TIME,
    1U)
}  // namespace gs1
