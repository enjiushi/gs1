#include "campaign/systems/faction_reputation_system.h"

#include "campaign/campaign_state.h"
#include "content/defs/faction_defs.h"
#include "runtime/game_runtime.h"

#include <algorithm>

namespace gs1
{
namespace
{
FactionProgressState* find_faction_progress_mut(
    CampaignState& campaign,
    FactionId faction_id) noexcept
{
    for (auto& faction_progress : campaign.faction_progress)
    {
        if (faction_progress.faction_id == faction_id)
        {
            return &faction_progress;
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
    using type = type_list<RuntimeCampaignTag>;
};

bool FactionReputationSystem::subscribes_to(GameMessageType type) noexcept
{
    return type == GameMessageType::FactionReputationAwardRequested;
}

const char* FactionReputationSystem::name() const noexcept
{
    return "FactionReputationSystem";
}

GameMessageSubscriptionSpan FactionReputationSystem::subscribed_game_messages() const noexcept
{
    return runtime_subscription_list<
        GameMessageType,
        k_game_message_type_count,
        FactionReputationSystem::subscribes_to>();
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
    auto access = make_game_state_access<FactionReputationSystem>(invocation);
    auto& campaign = access.template read<RuntimeCampaignTag>();
    if (!campaign.has_value())
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

    auto access = make_game_state_access<FactionReputationSystem>(invocation);
    auto& campaign = access.template read<RuntimeCampaignTag>();
    if (!campaign.has_value())
    {
        return GS1_STATUS_INVALID_STATE;
    }

    const auto& payload = message.payload_as<FactionReputationAwardRequestedMessage>();
    auto* faction_progress = find_faction_progress_mut(*campaign, FactionId {payload.faction_id});
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
