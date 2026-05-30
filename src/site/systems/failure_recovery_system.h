#pragma once

#include "runtime/system_interface.h"
#include "site/systems/site_system_context.h"

namespace gs1
{
class FailureRecoverySystem final : public IRuntimeSystem
{
public:
    using subscribed_messages = type_list<SiteAttemptEndedMessage>;
    using emitted_runtime_messages = type_list<>;
    static constexpr std::optional<Gs1RuntimeProfileSystemId> profile_id =
        GS1_RUNTIME_PROFILE_SYSTEM_FAILURE_RECOVERY;
    static constexpr std::optional<std::uint32_t> fixed_step_order_value = 19U;

    [[nodiscard]] std::span<const StateSetId> owned_state_sets() const noexcept override
    {
        return site_access_owned_state_sets<FailureRecoverySystem>();
    }

    [[nodiscard]] const char* name() const noexcept override;
    [[nodiscard]] std::optional<Gs1RuntimeProfileSystemId> profile_system_id() const noexcept override;
    [[nodiscard]] std::optional<std::uint32_t> fixed_step_order() const noexcept override;
    [[nodiscard]] Gs1Status handle(
        RuntimeInvocation& invocation,
        const SiteAttemptEndedMessage& message);
    void run(RuntimeInvocation& invocation) override;

    [[nodiscard]] static constexpr SiteSystemAccess access() noexcept
    {
        return SiteSystemAccess {
            "FailureRecoverySystem",
            site_component_mask_of(
                SiteComponent::RunMeta,
                SiteComponent::WorkerNeeds),
            0U};
    }

};

template <>
struct system_state_tags<FailureRecoverySystem>
{
    using type = type_list<RuntimeFixedStepSecondsTag>;
};
}  // namespace gs1
