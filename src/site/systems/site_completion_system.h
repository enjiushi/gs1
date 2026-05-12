#pragma once

#include "runtime/system_interface.h"
#include "site/systems/site_system_context.h"

namespace gs1
{
class SiteCompletionSystem final : public IRuntimeSystem
{
public:
    [[nodiscard]] const char* name() const noexcept override;
    [[nodiscard]] GameMessageSubscriptionSpan subscribed_game_messages() const noexcept override;
    [[nodiscard]] HostMessageSubscriptionSpan subscribed_host_messages() const noexcept override;
    [[nodiscard]] std::optional<Gs1RuntimeProfileSystemId> profile_system_id() const noexcept override;
    [[nodiscard]] std::optional<std::uint32_t> fixed_step_order() const noexcept override;
    [[nodiscard]] Gs1Status process_game_message(
        RuntimeInvocation& invocation,
        const GameMessage& message) override;
    [[nodiscard]] Gs1Status process_host_message(
        RuntimeInvocation& invocation,
        const Gs1HostMessage& message) override;
    void run(RuntimeInvocation& invocation) override;

    [[nodiscard]] static constexpr SiteSystemAccess access() noexcept
    {
        return SiteSystemAccess {
            "SiteCompletionSystem",
            site_component_mask_of(
                SiteComponent::RunMeta,
                SiteComponent::Time,
                SiteComponent::Counters,
                SiteComponent::TileLayout,
                SiteComponent::TileEcology,
                SiteComponent::Objective),
            0U};
    }

};

template <>
struct system_state_tags<SiteCompletionSystem>
{
    using type = type_list<
        RuntimeCampaignTag,
        RuntimeActiveSiteRunTag,
        RuntimeFixedStepSecondsTag,
        RuntimeMoveDirectionTag>;
};
}  // namespace gs1
