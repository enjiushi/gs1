#include "site/systems/failure_recovery_system.h"

#include "runtime/game_runtime.h"
#include "site/site_run_state.h"

namespace gs1
{
namespace
{
struct FailureRecoveryContext final
{
    SiteRunState& site_run;
    SiteWorldAccess<FailureRecoverySystem> world;
    GameMessageQueue& message_queue;
};

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
    (void)message;
    (void)invocation;
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

void FailureRecoverySystem::run(RuntimeInvocation& invocation)
{
    auto access = make_game_state_access<FailureRecoverySystem>(invocation);
    auto& site_run = access.template read<RuntimeActiveSiteRunTag>();
    if (!site_run.has_value())
    {
        return;
    }

    FailureRecoveryContext context {
        *site_run,
        SiteWorldAccess<FailureRecoverySystem> {*site_run},
        invocation.game_message_queue()};
    if (!context.world.has_world())
    {
        return;
    }

    const auto worker = context.world.read_worker();
    const float worker_health = worker.conditions.health;
    const auto& site_run_state = context.site_run;
    if (site_run_state.run_status != SiteRunStatus::Active ||
        worker_health > 0.0f ||
        has_pending_site_transition_message(context.message_queue, site_run_state.site_id.value))
    {
        return;
    }

    GameMessage message {};
    message.type = GameMessageType::SiteAttemptEnded;
    message.set_payload(SiteAttemptEndedMessage {
        context.world.site_id_value(),
        GS1_SITE_ATTEMPT_RESULT_FAILED});
    context.message_queue.push_back(message);
}
}  // namespace gs1

