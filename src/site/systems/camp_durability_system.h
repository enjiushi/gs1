#pragma once

#include "runtime/system_interface.h"
#include "site/systems/site_system_context.h"
#include "gs1/status.h"

namespace gs1
{
class CampDurabilitySystem final : public IRuntimeSystem
{
public:
    [[nodiscard]] std::span<const StateSetId> owned_state_sets() const noexcept override
    {
        return site_access_owned_state_sets<CampDurabilitySystem>();
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
            "CampDurabilitySystem",
            site_component_mask_of(
                SiteComponent::RunMeta,
                SiteComponent::Camp,
                SiteComponent::Weather,
                SiteComponent::Event),
            site_component_mask_of(SiteComponent::Camp)};
    }

};

template <>
struct system_state_tags<CampDurabilitySystem>
{
    using type = type_list<RuntimeFixedStepSecondsTag>;
};
}  // namespace gs1
