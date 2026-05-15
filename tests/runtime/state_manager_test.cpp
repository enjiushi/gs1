#include "runtime/state_manager.h"

#include "messages/game_message.h"
#include "runtime/runtime_state_access.h"
#include "runtime/system_interface.h"

#include <array>
#include <cassert>
#include <deque>
#include <optional>
#include <stdexcept>

namespace
{
using gs1::GameMessage;
using gs1::GameMessageSubscriptionSpan;
using gs1::HostMessageSubscriptionSpan;
using gs1::IRuntimeSystem;
using gs1::RuntimeInvocation;
using gs1::StateManager;
using gs1::StateSetId;

struct StubSystemBase : IRuntimeSystem
{
    [[nodiscard]] const char* name() const noexcept override { return "StubSystem"; }
    [[nodiscard]] GameMessageSubscriptionSpan subscribed_game_messages() const noexcept override
    {
        return {};
    }
    [[nodiscard]] HostMessageSubscriptionSpan subscribed_host_messages() const noexcept override
    {
        return {};
    }
    [[nodiscard]] std::optional<Gs1RuntimeProfileSystemId> profile_system_id() const noexcept override
    {
        return std::nullopt;
    }
    [[nodiscard]] std::optional<std::uint32_t> fixed_step_order() const noexcept override
    {
        return std::nullopt;
    }
    [[nodiscard]] Gs1Status process_game_message(RuntimeInvocation&, const GameMessage&) override
    {
        return GS1_STATUS_OK;
    }
    [[nodiscard]] Gs1Status process_host_message(RuntimeInvocation&, const Gs1HostMessage&) override
    {
        return GS1_STATUS_OK;
    }
    void run(RuntimeInvocation&) override {}
};

struct CampaignCoreOwnerSystem final : StubSystemBase
{
    static constexpr std::array<StateSetId, 1> k_owned_state_sets {StateSetId::CampaignCore};

    [[nodiscard]] std::span<const StateSetId> owned_state_sets() const noexcept override
    {
        return std::span<const StateSetId> {k_owned_state_sets};
    }
};

struct CampaignTechnologyOwnerSystem final : StubSystemBase
{
    static constexpr std::array<StateSetId, 1> k_owned_state_sets {StateSetId::CampaignTechnology};

    [[nodiscard]] std::span<const StateSetId> owned_state_sets() const noexcept override
    {
        return std::span<const StateSetId> {k_owned_state_sets};
    }
};

struct DuplicateCampaignCoreOwnerSystem final : StubSystemBase
{
    static constexpr std::array<StateSetId, 1> k_owned_state_sets {StateSetId::CampaignCore};

    [[nodiscard]] std::span<const StateSetId> owned_state_sets() const noexcept override
    {
        return std::span<const StateSetId> {k_owned_state_sets};
    }
};
}  // namespace

int main()
{
    StateManager state_manager {};

    CampaignCoreOwnerSystem campaign_core_owner {};
    state_manager.register_resolver(campaign_core_owner);
    assert(state_manager.default_resolver(StateSetId::CampaignCore) == &campaign_core_owner);
    assert(state_manager.active_resolver(StateSetId::CampaignCore) == &campaign_core_owner);

    CampaignTechnologyOwnerSystem campaign_technology_owner {};
    state_manager.register_resolver(campaign_technology_owner);
    assert(state_manager.default_resolver(StateSetId::CampaignTechnology) == &campaign_technology_owner);
    assert(state_manager.active_resolver(StateSetId::CampaignTechnology) == &campaign_technology_owner);

    state_manager.register_resolver(campaign_core_owner);
    assert(state_manager.default_resolver(StateSetId::CampaignCore) == &campaign_core_owner);
    assert(state_manager.active_resolver(StateSetId::CampaignCore) == &campaign_core_owner);

    DuplicateCampaignCoreOwnerSystem duplicate_campaign_core_owner {};
    bool rejected_duplicate_owner = false;
    try
    {
        state_manager.register_resolver(duplicate_campaign_core_owner);
    }
    catch (const std::logic_error&)
    {
        rejected_duplicate_owner = true;
    }

    assert(rejected_duplicate_owner);
    assert(state_manager.default_resolver(StateSetId::CampaignCore) == &campaign_core_owner);
    assert(state_manager.active_resolver(StateSetId::CampaignCore) == &campaign_core_owner);
    assert(state_manager.default_resolver(StateSetId::SiteRunMeta) == nullptr);
    assert(state_manager.active_resolver(StateSetId::SiteRunMeta) == nullptr);

    return 0;
}
