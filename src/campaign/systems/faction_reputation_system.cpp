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
    return {};
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

void FactionReputationSystem::run(RuntimeInvocation& invocation)
{
    (void)invocation;
}

}  // namespace gs1
