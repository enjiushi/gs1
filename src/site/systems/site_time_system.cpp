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

const char* SiteTimeSystem::name() const noexcept
{
    return access().system_name.data();
}

GameMessageSubscriptionSpan SiteTimeSystem::subscribed_game_messages() const noexcept
{
    return {};
}

HostMessageSubscriptionSpan SiteTimeSystem::subscribed_host_messages() const noexcept
{
    return {};
}

std::optional<Gs1RuntimeProfileSystemId> SiteTimeSystem::profile_system_id() const noexcept
{
    return GS1_RUNTIME_PROFILE_SYSTEM_SITE_TIME;
}

std::optional<std::uint32_t> SiteTimeSystem::fixed_step_order() const noexcept
{
    return 1U;
}

Gs1Status SiteTimeSystem::process_game_message(
    RuntimeInvocation& invocation,
    const GameMessage& message)
{
    (void)invocation;
    (void)message;
    return GS1_STATUS_OK;
}

Gs1Status SiteTimeSystem::process_host_message(
    RuntimeInvocation& invocation,
    const Gs1HostMessage& message)
{
    (void)invocation;
    (void)message;
    return GS1_STATUS_OK;
}

void SiteTimeSystem::run(RuntimeInvocation& invocation)
{
    auto access = make_game_state_access<SiteTimeSystem>(invocation);
    SiteWorldAccess<SiteTimeSystem> world {invocation};
    if (!world.has_world())
    {
        return;
    }

    auto& message_queue = invocation.game_message_queue();
    const double fixed_step_seconds = access.template read<RuntimeFixedStepSecondsTag>();
    auto& clock = world.own_time();
    const double step_minutes =
        runtime_minutes_from_real_seconds(fixed_step_seconds);
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
        message_queue.push_back(refresh_message);
    }
}
}  // namespace gs1

