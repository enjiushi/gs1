#include "campaign/systems/faction_reputation_system.h"

#include "campaign/campaign_state.h"
#include "content/defs/faction_defs.h"

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

bool FactionReputationSystem::subscribes_to(GameMessageType type) noexcept
{
    return type == GameMessageType::FactionReputationAwardRequested;
}

Gs1Status FactionReputationSystem::process_message(
    CampaignSystemContext& context,
    const GameMessage& message)
{
    if (message.type != GameMessageType::FactionReputationAwardRequested)
    {
        return GS1_STATUS_OK;
    }

    const auto& payload = message.payload_as<FactionReputationAwardRequestedMessage>();
    auto* faction_progress = find_faction_progress_mut(context.campaign, FactionId {payload.faction_id});
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
