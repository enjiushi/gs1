#pragma once

#include "runtime/system_interface.h"
#include "site/systems/site_system_context.h"
#include "gs1/status.h"

namespace gs1
{
class EconomyPhoneSystem final : public IRuntimeSystem
{
public:
    using subscribed_messages = type_list<
        SiteRunStartedMessage,
        SiteRefreshTickMessage,
        EconomyMoneyAwardRequestedMessage,
        PhoneListingPurchaseRequestedMessage,
        PhoneListingSaleRequestedMessage,
        ContractorHireRequestedMessage,
        SiteUnlockableRevealRequestedMessage,
        SiteUnlockablePurchaseRequestedMessage>;
    using emitted_runtime_messages = type_list<>;
    static constexpr std::optional<Gs1RuntimeProfileSystemId> profile_id =
        GS1_RUNTIME_PROFILE_SYSTEM_ECONOMY_PHONE;
    static constexpr std::optional<std::uint32_t> fixed_step_order_value = 17U;

    [[nodiscard]] std::span<const StateSetId> owned_state_sets() const noexcept override
    {
        return site_access_owned_state_sets<EconomyPhoneSystem>();
    }

    [[nodiscard]] const char* name() const noexcept override;
    [[nodiscard]] std::optional<Gs1RuntimeProfileSystemId> profile_system_id() const noexcept override;
    [[nodiscard]] std::optional<std::uint32_t> fixed_step_order() const noexcept override;
    [[nodiscard]] Gs1Status handle(
        RuntimeInvocation& invocation,
        const SiteRunStartedMessage& message);
    [[nodiscard]] Gs1Status handle(
        RuntimeInvocation& invocation,
        const SiteRefreshTickMessage& message);
    [[nodiscard]] Gs1Status handle(
        RuntimeInvocation& invocation,
        const EconomyMoneyAwardRequestedMessage& message);
    [[nodiscard]] Gs1Status handle(
        RuntimeInvocation& invocation,
        const PhoneListingPurchaseRequestedMessage& message);
    [[nodiscard]] Gs1Status handle(
        RuntimeInvocation& invocation,
        const PhoneListingSaleRequestedMessage& message);
    [[nodiscard]] Gs1Status handle(
        RuntimeInvocation& invocation,
        const ContractorHireRequestedMessage& message);
    [[nodiscard]] Gs1Status handle(
        RuntimeInvocation& invocation,
        const SiteUnlockableRevealRequestedMessage& message);
    [[nodiscard]] Gs1Status handle(
        RuntimeInvocation& invocation,
        const SiteUnlockablePurchaseRequestedMessage& message);
    void run(RuntimeInvocation& invocation) override;

    [[nodiscard]] static constexpr SiteSystemAccess access() noexcept
    {
        return SiteSystemAccess {
            "EconomyPhoneSystem",
            site_component_mask_of(
                SiteComponent::Inventory,
                SiteComponent::Craft,
                SiteComponent::Action,
                SiteComponent::TaskBoard,
                SiteComponent::Economy),
            site_component_mask_of(SiteComponent::Economy)};
    }
};
}  // namespace gs1
