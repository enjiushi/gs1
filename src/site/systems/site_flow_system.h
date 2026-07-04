#pragma once

#include "runtime/system_interface.h"
#include "site/systems/site_system_context.h"
#include "gs1/status.h"

namespace gs1
{
class SiteFlowSystem final : public IRuntimeSystem
{
public:
    using subscribed_messages = type_list<>;
    using emitted_runtime_messages = type_list<>;
    static constexpr std::optional<Gs1RuntimeProfileSystemId> profile_id =
        GS1_RUNTIME_PROFILE_SYSTEM_SITE_FLOW;
    static constexpr std::optional<std::uint32_t> fixed_step_order_value = 2U;

    static constexpr std::array<StateSetId, 1> k_owned_state_sets {
        StateSetId::MoveDirection};
    [[nodiscard]] const char* name() const noexcept override;
    [[nodiscard]] std::optional<Gs1RuntimeProfileSystemId> profile_system_id() const noexcept override;
    [[nodiscard]] std::optional<std::uint32_t> fixed_step_order() const noexcept override;
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
