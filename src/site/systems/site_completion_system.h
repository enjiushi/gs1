#pragma once

#include "runtime/system_interface.h"
#include "site/systems/site_system_context.h"

namespace gs1
{
class SiteCompletionSystem final : public IRuntimeSystem
{
public:
    using subscribed_messages = type_list<SiteAttemptEndedMessage>;
    using emitted_runtime_messages = type_list<>;
    static constexpr std::optional<Gs1RuntimeProfileSystemId> profile_id =
        GS1_RUNTIME_PROFILE_SYSTEM_SITE_COMPLETION;
    static constexpr std::optional<std::uint32_t> fixed_step_order_value = 20U;

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
            "SiteCompletionSystem",
            site_component_mask_of(
                SiteComponent::RunMeta,
                SiteComponent::Time,
                SiteComponent::TileLayout,
                SiteComponent::TileEcology,
                SiteComponent::Objective),
            site_component_mask_of(
                SiteComponent::Objective)};
    }

};

template <>
struct system_state_tags<SiteCompletionSystem>
{
    using type = type_list<RuntimeFixedStepSecondsTag>;
};
}  // namespace gs1
