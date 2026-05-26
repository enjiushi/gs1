#include "campaign/systems/campaign_time_system.h"

#include "runtime/game_runtime.h"

namespace gs1
{
template <>
struct system_state_tags<CampaignTimeSystem>
{
    using type = type_list<RuntimeFixedStepSecondsTag>;
};

const char* CampaignTimeSystem::name() const noexcept
{
    return "CampaignTimeSystem";
}

GameMessageSubscriptionSpan CampaignTimeSystem::subscribed_game_messages() const noexcept
{
    return {};
}

HostMessageSubscriptionSpan CampaignTimeSystem::subscribed_host_messages() const noexcept
{
    return {};
}

std::optional<Gs1RuntimeProfileSystemId> CampaignTimeSystem::profile_system_id() const noexcept
{
    return GS1_RUNTIME_PROFILE_SYSTEM_CAMPAIGN_TIME;
}

std::optional<std::uint32_t> CampaignTimeSystem::fixed_step_order() const noexcept
{
    return 0U;
}

Gs1Status CampaignTimeSystem::process_game_message(
    RuntimeInvocation& invocation,
    const GameMessage& message)
{
    (void)invocation;
    (void)message;
    return GS1_STATUS_OK;
}

Gs1Status CampaignTimeSystem::process_host_message(
    RuntimeInvocation& invocation,
    const Gs1HostMessage& message)
{
    (void)invocation;
    (void)message;
    return GS1_STATUS_OK;
}

void CampaignTimeSystem::run(RuntimeInvocation& invocation)
{
    if (!runtime_invocation_has_campaign(invocation))
    {
        return;
    }

    const double fixed_step_seconds =
        make_game_state_access<CampaignTimeSystem>(invocation).template read<RuntimeFixedStepSecondsTag>();
    GameMessage message {};
    message.type = GameMessageType::CampaignTimeDeltaRequested;
    message.set_payload(CampaignTimeDeltaRequestedMessage {
        runtime_minutes_from_real_seconds(fixed_step_seconds)});
    invocation.push_game_message(message);
}
}  // namespace gs1
