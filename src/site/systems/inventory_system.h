#pragma once

#include "runtime/system_interface.h"
#include "site/systems/site_system_context.h"
#include "gs1/status.h"

namespace gs1
{
class InventorySystem final : public IRuntimeSystem
{
public:
    using subscribed_messages = type_list<
        SiteRunStartedMessage,
        SiteDevicePlacedMessage,
        SiteDeviceBrokenMessage,
        InventoryDeliveryRequestedMessage,
        InventoryDeliveryBatchRequestedMessage,
        InventoryWorkerPackInsertRequestedMessage,
        InventoryItemUseRequestedMessage,
        InventoryItemConsumeRequestedMessage,
        InventoryGlobalItemConsumeRequestedMessage,
        InventoryTransferRequestedMessage,
        InventoryItemSubmitRequestedMessage,
        InventorySlotTappedMessage,
        InventoryCraftCommitRequestedMessage>;
    using emitted_runtime_messages = type_list<>;
    static constexpr std::optional<Gs1RuntimeProfileSystemId> profile_id =
        GS1_RUNTIME_PROFILE_SYSTEM_INVENTORY;
    static constexpr std::optional<std::uint32_t> fixed_step_order_value = 14U;

    [[nodiscard]] const char* name() const noexcept override;
    [[nodiscard]] std::optional<Gs1RuntimeProfileSystemId> profile_system_id() const noexcept override;
    [[nodiscard]] std::optional<std::uint32_t> fixed_step_order() const noexcept override;
    [[nodiscard]] Gs1Status handle(
        RuntimeInvocation& invocation,
        const SiteRunStartedMessage& message);
    [[nodiscard]] Gs1Status handle(
        RuntimeInvocation& invocation,
        const SiteDevicePlacedMessage& message);
    [[nodiscard]] Gs1Status handle(
        RuntimeInvocation& invocation,
        const SiteDeviceBrokenMessage& message);
    [[nodiscard]] Gs1Status handle(
        RuntimeInvocation& invocation,
        const InventoryDeliveryRequestedMessage& message);
    [[nodiscard]] Gs1Status handle(
        RuntimeInvocation& invocation,
        const InventoryDeliveryBatchRequestedMessage& message);
    [[nodiscard]] Gs1Status handle(
        RuntimeInvocation& invocation,
        const InventoryWorkerPackInsertRequestedMessage& message);
    [[nodiscard]] Gs1Status handle(
        RuntimeInvocation& invocation,
        const InventoryItemUseRequestedMessage& message);
    [[nodiscard]] Gs1Status handle(
        RuntimeInvocation& invocation,
        const InventoryItemConsumeRequestedMessage& message);
    [[nodiscard]] Gs1Status handle(
        RuntimeInvocation& invocation,
        const InventoryGlobalItemConsumeRequestedMessage& message);
    [[nodiscard]] Gs1Status handle(
        RuntimeInvocation& invocation,
        const InventoryTransferRequestedMessage& message);
    [[nodiscard]] Gs1Status handle(
        RuntimeInvocation& invocation,
        const InventoryItemSubmitRequestedMessage& message);
    [[nodiscard]] Gs1Status handle(
        RuntimeInvocation& invocation,
        const InventorySlotTappedMessage& message);
    [[nodiscard]] Gs1Status handle(
        RuntimeInvocation& invocation,
        const InventoryCraftCommitRequestedMessage& message);
    void run(RuntimeInvocation& invocation) override;

    [[nodiscard]] static constexpr SiteSystemAccess access() noexcept
    {
        return SiteSystemAccess {
            "InventorySystem",
            site_component_mask_of(
                SiteComponent::RunMeta,
                SiteComponent::TileLayout,
                SiteComponent::WorkerMotion,
                SiteComponent::Inventory,
                SiteComponent::Action),
            site_component_mask_of(SiteComponent::Inventory)};
    }
};
}  // namespace gs1
