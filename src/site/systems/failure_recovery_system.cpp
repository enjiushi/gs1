#include "site/systems/failure_recovery_system.h"

#include "runtime/game_runtime.h"
#include "site/site_run_state.h"

namespace gs1
{
const char* FailureRecoverySystem::name() const noexcept
{
    return access().system_name.data();
}

GameMessageSubscriptionSpan FailureRecoverySystem::subscribed_game_messages() const noexcept
{
    static constexpr GameMessageType subscriptions[] = {
        GameMessageType::SiteAttemptEnded};
    return subscriptions;
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
    if (message.type == GameMessageType::SiteAttemptEnded)
    {
        return handle(invocation, message.payload_as<SiteAttemptEndedMessage>());
    }

    return GS1_STATUS_OK;
}

Gs1Status FailureRecoverySystem::process_host_message(
    RuntimeInvocation& invocation,
    const Gs1HostMessage& message)
{
    (void)message;
    (void)invocation;
    return GS1_STATUS_OK;
}

Gs1Status FailureRecoverySystem::handle(
    RuntimeInvocation& invocation,
    const SiteAttemptEndedMessage& message)
{
    (void)invocation;
    (void)message;
    return GS1_STATUS_OK;
}

void FailureRecoverySystem::run(RuntimeInvocation& invocation)
{
    SiteWorldAccess<FailureRecoverySystem> world {invocation};
    if (!world.has_world())
    {
        return;
    }

    const auto worker = world.read_worker();
    const float worker_health = worker.conditions.health;
    if (world.run_status() != SiteRunStatus::Active ||
        worker_health > 0.0f)
    {
        return;
    }

    invocation.emit_game_message(SiteAttemptEndedMessage {
        world.site_id_value(),
        GS1_SITE_ATTEMPT_RESULT_FAILED});
}
}  // namespace gs1

