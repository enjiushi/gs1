#pragma once

#include "runtime/system_interface.h"
#include "site/systems/site_system_context.h"
#include "gs1/status.h"

namespace gs1
{
class TaskBoardSystem final : public IRuntimeSystem
{
public:
    using subscribed_messages = type_list<
        SiteRunStartedMessage,
        SiteRefreshTickMessage,
        TaskAcceptRequestedMessage,
        TaskRewardClaimRequestedMessage,
        RestorationProgressChangedMessage,
        TileEcologyChangedMessage,
        TileEcologyBatchChangedMessage,
        LivingPlantStabilityChangedMessage,
        SiteTileStateChangedMessage,
        WorkerMetersChangedMessage,
        PhoneListingPurchasedMessage,
        PhoneListingSoldMessage,
        InventoryTransferCompletedMessage,
        InventoryItemSubmittedMessage,
        InventoryItemUseCompletedMessage,
        InventoryCraftCompletedMessage,
        SiteTilePlantingCompletedMessage,
        SiteActionCompletedMessage,
        SiteDevicePlacedMessage,
        SiteDeviceConditionChangedMessage,
        EconomyMoneyAwardRequestedMessage>;
    static constexpr std::optional<Gs1RuntimeProfileSystemId> profile_id =
        GS1_RUNTIME_PROFILE_SYSTEM_TASK_BOARD;
    static constexpr std::optional<std::uint32_t> fixed_step_order_value = 16U;

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
        const TaskAcceptRequestedMessage& message);
    [[nodiscard]] Gs1Status handle(
        RuntimeInvocation& invocation,
        const TaskRewardClaimRequestedMessage& message);
    [[nodiscard]] Gs1Status handle(
        RuntimeInvocation& invocation,
        const RestorationProgressChangedMessage& message);
    [[nodiscard]] Gs1Status handle(
        RuntimeInvocation& invocation,
        const TileEcologyChangedMessage& message);
    [[nodiscard]] Gs1Status handle(
        RuntimeInvocation& invocation,
        const TileEcologyBatchChangedMessage& message);
    [[nodiscard]] Gs1Status handle(
        RuntimeInvocation& invocation,
        const LivingPlantStabilityChangedMessage& message);
    [[nodiscard]] Gs1Status handle(
        RuntimeInvocation& invocation,
        const SiteTileStateChangedMessage& message);
    [[nodiscard]] Gs1Status handle(
        RuntimeInvocation& invocation,
        const WorkerMetersChangedMessage& message);
    [[nodiscard]] Gs1Status handle(
        RuntimeInvocation& invocation,
        const PhoneListingPurchasedMessage& message);
    [[nodiscard]] Gs1Status handle(
        RuntimeInvocation& invocation,
        const PhoneListingSoldMessage& message);
    [[nodiscard]] Gs1Status handle(
        RuntimeInvocation& invocation,
        const InventoryTransferCompletedMessage& message);
    [[nodiscard]] Gs1Status handle(
        RuntimeInvocation& invocation,
        const InventoryItemSubmittedMessage& message);
    [[nodiscard]] Gs1Status handle(
        RuntimeInvocation& invocation,
        const InventoryItemUseCompletedMessage& message);
    [[nodiscard]] Gs1Status handle(
        RuntimeInvocation& invocation,
        const InventoryCraftCompletedMessage& message);
    [[nodiscard]] Gs1Status handle(
        RuntimeInvocation& invocation,
        const SiteTilePlantingCompletedMessage& message);
    [[nodiscard]] Gs1Status handle(
        RuntimeInvocation& invocation,
        const SiteActionCompletedMessage& message);
    [[nodiscard]] Gs1Status handle(
        RuntimeInvocation& invocation,
        const SiteDevicePlacedMessage& message);
    [[nodiscard]] Gs1Status handle(
        RuntimeInvocation& invocation,
        const SiteDeviceConditionChangedMessage& message);
    [[nodiscard]] Gs1Status handle(
        RuntimeInvocation& invocation,
        const EconomyMoneyAwardRequestedMessage& message);
    void run(RuntimeInvocation& invocation) override;

    [[nodiscard]] static constexpr SiteSystemAccess access() noexcept
    {
        return SiteSystemAccess {
            "TaskBoardSystem",
            site_component_mask_of(
                SiteComponent::TaskBoard,
                SiteComponent::TileLayout,
                SiteComponent::Counters,
                SiteComponent::Objective),
            site_component_mask_of(SiteComponent::TaskBoard)};
    }
};

template <>
struct system_state_tags<TaskBoardSystem>
{
    using type = type_list<
        RuntimeCampaignFactionProgressTag,
        RuntimeCampaignTechnologyTag,
        RuntimeFixedStepSecondsTag>;
};
}  // namespace gs1
