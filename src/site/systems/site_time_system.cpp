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
struct SiteTimeContext final
{
    SiteRunState& site_run;
    SiteWorldAccess<SiteTimeSystem> world;
    GameMessageQueue& message_queue;
    double fixed_step_seconds {0.0};
};

template <typename Fn>
Gs1Status with_site_time_context(
    RuntimeInvocation& invocation,
    Fn&& fn,
    bool missing_context_is_ok = false)
{
    auto access = make_game_state_access<SiteTimeSystem>(invocation);
    auto& site_run = access.template read<RuntimeActiveSiteRunTag>();
    const double fixed_step_seconds = access.template read<RuntimeFixedStepSecondsTag>();
    if (!site_run.has_value())
    {
        return missing_context_is_ok ? GS1_STATUS_OK : GS1_STATUS_INVALID_STATE;
    }

    SiteTimeContext context {
        *site_run,
        SiteWorldAccess<SiteTimeSystem> {*site_run},
        invocation.game_message_queue(),
        fixed_step_seconds};
    return fn(context);
}

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
    auto access = make_game_state_access<SiteTimeSystem>(invocation);
    (void)access;
    (void)message;
    return GS1_STATUS_OK;
}

Gs1Status SiteTimeSystem::process_host_message(
    RuntimeInvocation& invocation,
    const Gs1HostMessage& message)
{
    auto access = make_game_state_access<SiteTimeSystem>(invocation);
    (void)access;
    (void)message;
    return GS1_STATUS_OK;
}

void SiteTimeSystem::run(RuntimeInvocation& invocation)
{
    auto access = make_game_state_access<SiteTimeSystem>(invocation);
    (void)access;
    (void)with_site_time_context(
        invocation,
        [&](SiteTimeContext& context) -> Gs1Status
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
                return GS1_STATUS_OK;
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
            return GS1_STATUS_OK;
        });
}
}  // namespace gs1

