#pragma once

#include "runtime/system_interface.h"
#include "site/systems/site_system_context.h"
#include "gs1/status.h"

namespace gs1
{
class ModifierSystem final : public IRuntimeSystem
{
public:
    using subscribed_messages = type_list<
        SiteRunStartedMessage,
        RunModifierAwardRequestedMessage,
        InventoryItemUseCompletedMessage,
        SiteModifierEndRequestedMessage>;
    using emitted_runtime_messages = type_list<>;
    static constexpr std::optional<Gs1RuntimeProfileSystemId> profile_id =
        GS1_RUNTIME_PROFILE_SYSTEM_MODIFIER;
    static constexpr std::optional<std::uint32_t> fixed_step_order_value = 3U;

    [[nodiscard]] std::span<const StateSetId> owned_state_sets() const noexcept override
    {
        return site_access_owned_state_sets<ModifierSystem>();
    }

    [[nodiscard]] const char* name() const noexcept override;
    [[nodiscard]] GameMessageSubscriptionSpan subscribed_game_messages() const noexcept override;
    [[nodiscard]] HostMessageSubscriptionSpan subscribed_host_messages() const noexcept override;
    [[nodiscard]] std::optional<Gs1RuntimeProfileSystemId> profile_system_id() const noexcept override;
    [[nodiscard]] std::optional<std::uint32_t> fixed_step_order() const noexcept override;
    [[nodiscard]] Gs1Status handle(
        RuntimeInvocation& invocation,
        const SiteRunStartedMessage& message);
    [[nodiscard]] Gs1Status handle(
        RuntimeInvocation& invocation,
        const RunModifierAwardRequestedMessage& message);
    [[nodiscard]] Gs1Status handle(
        RuntimeInvocation& invocation,
        const InventoryItemUseCompletedMessage& message);
    [[nodiscard]] Gs1Status handle(
        RuntimeInvocation& invocation,
        const SiteModifierEndRequestedMessage& message);
    void run(RuntimeInvocation& invocation) override;

    [[nodiscard]] static constexpr SiteSystemAccess access() noexcept
    {
        return SiteSystemAccess {
            "ModifierSystem",
            site_component_mask_of(
                SiteComponent::Camp,
                SiteComponent::Modifier),
            site_component_mask_of(SiteComponent::Modifier)};
    }
};

template <>
struct system_state_tags<ModifierSystem>
{
    using type = type_list<
        RuntimeCampaignFactionProgressTag,
        RuntimeCampaignTechnologyTag,
        RuntimeFixedStepSecondsTag>;
};
}  // namespace gs1
