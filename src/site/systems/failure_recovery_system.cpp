#include "site/systems/failure_recovery_system.h"

#include "runtime/game_runtime.h"
#include "site/site_run_state.h"

namespace gs1
{
namespace
{
bool has_pending_site_transition_message(
    const GameMessageQueue& message_queue,
    std::uint32_t site_id)
{
    for (const auto& message : message_queue)
    {
        if (message.type == GameMessageType::SiteAttemptEnded &&
            message.payload_as<SiteAttemptEndedMessage>().site_id == site_id)
        {
            return true;
        }
    }

    return false;
}
}  // namespace

const char* FailureRecoverySystem::name() const noexcept
{
    return access().system_name.data();
}

GameMessageSubscriptionSpan FailureRecoverySystem::subscribed_game_messages() const noexcept
{
    return {};
}

HostMessageSubscriptionSpan FailureRecoverySystem::subscribed_host_messages() const noexcept
{
    return {};
}

std::optional<Gs1RuntimeProfileSystemId> FailureRecoverySystem::profile_system_id() const noexcept
{
    return GS1_RUNTIME_PROFILE_SYSTEM_FAILURE_RECOVERY;
}

std::optional<std::uint32_t> FailureRecoverySystem::fixed_step_order() const noexcept
{
    return 19U;
}

Gs1Status FailureRecoverySystem::process_game_message(
    RuntimeInvocation& invocation,
    const GameMessage& message)
{
    auto access = make_game_state_access<FailureRecoverySystem>(invocation);
    (void)access;
    (void)message;
    return GS1_STATUS_OK;
}

Gs1Status FailureRecoverySystem::process_host_message(
    RuntimeInvocation& invocation,
    const Gs1HostMessage& message)
{
    auto access = make_game_state_access<FailureRecoverySystem>(invocation);
    (void)access;
    (void)message;
    return GS1_STATUS_OK;
}

void FailureRecoverySystem::run(RuntimeInvocation& invocation)
{
    auto access = make_game_state_access<FailureRecoverySystem>(invocation);
    (void)access;
    (void)with_site_system_context<FailureRecoverySystem>(
        invocation,
        [&](SiteSystemContext<FailureRecoverySystem>& context) -> Gs1Status
        {
            if (!context.world.has_world())
            {
                return GS1_STATUS_OK;
            }

            const auto worker = context.world.read_worker();
            const float worker_health = worker.conditions.health;
            const auto& site_run = context.site_run;
            if (site_run.run_status != SiteRunStatus::Active ||
                worker_health > 0.0f ||
                has_pending_site_transition_message(context.message_queue, site_run.site_id.value))
            {
                return GS1_STATUS_OK;
            }

            GameMessage message {};
            message.type = GameMessageType::SiteAttemptEnded;
            message.set_payload(SiteAttemptEndedMessage {
                context.world.site_id_value(),
                GS1_SITE_ATTEMPT_RESULT_FAILED});
            context.message_queue.push_back(message);
            return GS1_STATUS_OK;
        });
}
}  // namespace gs1
