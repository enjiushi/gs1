#include "campaign/systems/faction_reputation_system.h"

#include "content/defs/faction_defs.h"
#include "runtime/game_runtime.h"

#include <algorithm>

namespace gs1
{
namespace
{
FactionProgressState* find_faction_progress_mut(
    std::vector<FactionProgressState>& faction_progress,
    FactionId faction_id) noexcept
{
    for (auto& faction_progress_state : faction_progress)
    {
        if (faction_progress_state.faction_id == faction_id)
        {
            return &faction_progress_state;
        }
    }

    return nullptr;
}

}  // namespace

Gs1Status process_faction_reputation_message(
    RuntimeInvocation& invocation,
    const GameMessage& message);

template <>
struct system_state_tags<FactionReputationSystem>
{
    using type = type_list<RuntimeCampaignFactionProgressTag>;
};

const char* FactionReputationSystem::name() const noexcept
{
    return "FactionReputationSystem";
}

GameMessageSubscriptionSpan FactionReputationSystem::subscribed_game_messages() const noexcept
{
    static constexpr GameMessageType subscriptions[] = {GameMessageType::FactionReputationAwardRequested};
    return subscriptions;
}

HostMessageSubscriptionSpan FactionReputationSystem::subscribed_host_messages() const noexcept
{
    return {};
}

std::optional<Gs1RuntimeProfileSystemId> FactionReputationSystem::profile_system_id() const noexcept
{
    return GS1_RUNTIME_PROFILE_SYSTEM_FACTION_REPUTATION;
}

std::optional<std::uint32_t> FactionReputationSystem::fixed_step_order() const noexcept
{
    return std::nullopt;
}

Gs1Status FactionReputationSystem::process_game_message(
    RuntimeInvocation& invocation,
    const GameMessage& message)
{
    if (!runtime_invocation_has_campaign(invocation))
    {
        return GS1_STATUS_INVALID_STATE;
    }

    return process_faction_reputation_message(invocation, message);
}

Gs1Status FactionReputationSystem::process_host_message(
    RuntimeInvocation& invocation,
    const Gs1HostMessage& message)
{
    (void)invocation;
    (void)message;
    return GS1_STATUS_OK;
}

void FactionReputationSystem::run(RuntimeInvocation& invocation)
{
    (void)invocation;
}

Gs1Status process_faction_reputation_message(
    RuntimeInvocation& invocation,
    const GameMessage& message)
{
    if (message.type != GameMessageType::FactionReputationAwardRequested)
    {
        return GS1_STATUS_OK;
    }

    if (!runtime_invocation_has_campaign(invocation))
    {
        return GS1_STATUS_INVALID_STATE;
    }

    const auto& payload = message.payload_as<FactionReputationAwardRequestedMessage>();
    auto* faction_progress =
        find_faction_progress_mut(
            runtime_invocation_state_ref<RuntimeCampaignFactionProgressTag>(invocation),
            FactionId {payload.faction_id});
    if (faction_progress == nullptr)
    {
        return GS1_STATUS_NOT_FOUND;
    }

    if (payload.delta <= 0)
    {
        return GS1_STATUS_OK;
    }

    faction_progress->faction_reputation += payload.delta;

    if (const auto* faction_def = find_faction_def(faction_progress->faction_id);
        faction_def != nullptr &&
        !faction_progress->has_unlocked_assistant_package &&
        faction_progress->faction_reputation >= faction_def->assistant_unlock_reputation)
    {
        faction_progress->unlocked_assistant_package_id = faction_def->assistant_package_id;
        faction_progress->has_unlocked_assistant_package = faction_def->assistant_package_id != 0U;
    }

    return GS1_STATUS_OK;
}
}  // namespace gs1
