#pragma once

#include "runtime/system_interface.h"
#include "site/systems/site_system_context.h"
#include "gs1/status.h"

namespace gs1
{
class ActionExecutionSystem final : public IRuntimeSystem
{
public:
    using subscribed_messages = type_list<
        StartSiteActionMessage,
        CancelSiteActionMessage,
        PlacementModeCursorMovedMessage,
        PlacementReservationAcceptedMessage,
        PlacementReservationRejectedMessage>;
    using emitted_runtime_messages = type_list<
        runtime_message_type_constant<GS1_ENGINE_MESSAGE_SITE_ACTION_UPDATE>,
        runtime_message_type_constant<GS1_ENGINE_MESSAGE_SITE_PLACEMENT_FAILURE>,
        runtime_message_type_constant<GS1_ENGINE_MESSAGE_PLAY_ONE_SHOT_CUE>>;
    static constexpr std::optional<Gs1RuntimeProfileSystemId> profile_id =
        GS1_RUNTIME_PROFILE_SYSTEM_ACTION_EXECUTION;
    static constexpr std::optional<std::uint32_t> fixed_step_order_value = 5U;

    [[nodiscard]] std::span<const StateSetId> owned_state_sets() const noexcept override
    {
        return site_access_owned_state_sets<ActionExecutionSystem>();
    }

    [[nodiscard]] const char* name() const noexcept override;
    [[nodiscard]] std::optional<Gs1RuntimeProfileSystemId> profile_system_id() const noexcept override;
    [[nodiscard]] std::optional<std::uint32_t> fixed_step_order() const noexcept override;
    [[nodiscard]] Gs1Status handle(
        RuntimeInvocation& invocation,
        const StartSiteActionMessage& message);
    [[nodiscard]] Gs1Status handle(
        RuntimeInvocation& invocation,
        const CancelSiteActionMessage& message);
    [[nodiscard]] Gs1Status handle(
        RuntimeInvocation& invocation,
        const PlacementModeCursorMovedMessage& message);
    [[nodiscard]] Gs1Status handle(
        RuntimeInvocation& invocation,
        const PlacementReservationAcceptedMessage& message);
    [[nodiscard]] Gs1Status handle(
        RuntimeInvocation& invocation,
        const PlacementReservationRejectedMessage& message);
    void run(RuntimeInvocation& invocation) override;

    [[nodiscard]] static constexpr SiteSystemAccess access() noexcept
    {
        return SiteSystemAccess {
            "ActionExecutionSystem",
            site_component_mask_of(
                SiteComponent::Time,
                SiteComponent::TileLayout,
                SiteComponent::TileExcavation,
                SiteComponent::TileWeather,
                SiteComponent::DeviceCondition,
                SiteComponent::WorkerMotion,
                SiteComponent::WorkerNeeds,
                SiteComponent::Inventory,
                SiteComponent::Modifier,
                SiteComponent::Craft,
                SiteComponent::Action),
            site_component_mask_of(
                SiteComponent::TileExcavation,
                SiteComponent::Action)};
    }
};
}  // namespace gs1
