#pragma once

#include "runtime/system_interface.h"
#include "site/systems/site_system_context.h"
#include "gs1/status.h"

namespace gs1
{
class SiteFlowSystem final : public IRuntimeSystem
{
public:
    [[nodiscard]] std::span<const StateSetId> owned_state_sets() const noexcept override
    {
        return site_access_owned_state_sets<SiteFlowSystem>();
    }

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
            "SiteFlowSystem",
            site_component_mask_of(
                SiteComponent::TileLayout,
                SiteComponent::WorkerMotion,
                SiteComponent::Action,
                SiteComponent::Inventory),
            site_component_mask_of(
                SiteComponent::WorkerMotion)};
    }

};

template <>
struct system_state_tags<SiteFlowSystem>
{
    using type = type_list<RuntimeFixedStepSecondsTag, RuntimeMoveDirectionTag>;
};
}  // namespace gs1
