#pragma once

#include "runtime/system_interface.h"
#include "site/systems/site_system_context.h"
#include "gs1/status.h"

namespace gs1
{
class WorkerConditionSystem final : public IRuntimeSystem
{
public:
    using subscribed_messages = type_list<
        SiteRunStartedMessage,
        WorkerMeterDeltaRequestedMessage,
        InventoryItemUseCompletedMessage>;
    using emitted_runtime_messages = type_list<>;
    static constexpr std::optional<Gs1RuntimeProfileSystemId> profile_id =
        GS1_RUNTIME_PROFILE_SYSTEM_WORKER_CONDITION;
    static constexpr std::optional<std::uint32_t> fixed_step_order_value = 9U;

    [[nodiscard]] const char* name() const noexcept override;
    [[nodiscard]] std::optional<Gs1RuntimeProfileSystemId> profile_system_id() const noexcept override;
    [[nodiscard]] std::optional<std::uint32_t> fixed_step_order() const noexcept override;
    [[nodiscard]] Gs1Status handle(
        RuntimeInvocation& invocation,
        const SiteRunStartedMessage& message);
    [[nodiscard]] Gs1Status handle(
        RuntimeInvocation& invocation,
        const WorkerMeterDeltaRequestedMessage& message);
    [[nodiscard]] Gs1Status handle(
        RuntimeInvocation& invocation,
        const InventoryItemUseCompletedMessage& message);
    void run(RuntimeInvocation& invocation) override;

    [[nodiscard]] static constexpr SiteSystemAccess access() noexcept
    {
        return SiteSystemAccess {
            "WorkerConditionSystem",
            site_component_mask_of(
                SiteComponent::RunMeta,
                SiteComponent::TileWeather,
                SiteComponent::WorkerMotion,
                SiteComponent::WorkerNeeds,
                SiteComponent::Action),
            site_component_mask_of(SiteComponent::WorkerNeeds)};
    }
};

template <>
struct system_state_tags<WorkerConditionSystem>
{
    using type = type_list<RuntimeFixedStepSecondsTag>;
};
}  // namespace gs1
