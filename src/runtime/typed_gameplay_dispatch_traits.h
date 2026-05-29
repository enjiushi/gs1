#pragma once

#include "runtime/game_systems.h"
#include "runtime/system_interface.h"
#include "runtime/type_list.h"

namespace gs1
{
template <typename Message, typename SystemPack>
struct typed_game_message_subscribers;

template <typename Message, typename... Systems>
struct typed_game_message_subscribers<Message, system_pack<Systems...>>
{
    template <typename System>
    using predicate = std::bool_constant<
        type_list_contains_v<Message, runtime_subscribed_messages_t<System>>>;

    using type = type_list_filter_t<predicate, type_list<Systems...>>;
};

template <typename Message>
struct typed_gameplay_dispatch_traits
{
    using subscribers = type_list<>;
    static constexpr bool enabled = false;
};

template <>
struct typed_gameplay_dispatch_traits<SiteRefreshTickMessage>
{
    using subscribers = typename typed_game_message_subscribers<
        SiteRefreshTickMessage,
        GameSystems>::type;
    static constexpr bool enabled = true;
};

template <>
struct typed_gameplay_dispatch_traits<TaskAcceptRequestedMessage>
{
    using subscribers = typename typed_game_message_subscribers<
        TaskAcceptRequestedMessage,
        GameSystems>::type;
    static constexpr bool enabled = true;
};

template <>
struct typed_gameplay_dispatch_traits<TaskRewardClaimRequestedMessage>
{
    using subscribers = typename typed_game_message_subscribers<
        TaskRewardClaimRequestedMessage,
        GameSystems>::type;
    static constexpr bool enabled = true;
};

template <>
struct typed_gameplay_dispatch_traits<SiteModifierEndRequestedMessage>
{
    using subscribers = typename typed_game_message_subscribers<
        SiteModifierEndRequestedMessage,
        GameSystems>::type;
    static constexpr bool enabled = true;
};

template <>
struct typed_gameplay_dispatch_traits<PhoneListingPurchaseRequestedMessage>
{
    using subscribers = typename typed_game_message_subscribers<
        PhoneListingPurchaseRequestedMessage,
        GameSystems>::type;
    static constexpr bool enabled = true;
};

template <>
struct typed_gameplay_dispatch_traits<PhoneListingSaleRequestedMessage>
{
    using subscribers = typename typed_game_message_subscribers<
        PhoneListingSaleRequestedMessage,
        GameSystems>::type;
    static constexpr bool enabled = true;
};

template <>
struct typed_gameplay_dispatch_traits<OpenMainMenuMessage>
{
    using subscribers = typename typed_game_message_subscribers<
        OpenMainMenuMessage,
        GameSystems>::type;
    static constexpr bool enabled = true;
};

template <>
struct typed_gameplay_dispatch_traits<StartNewCampaignMessage>
{
    using subscribers = typename typed_game_message_subscribers<
        StartNewCampaignMessage,
        GameSystems>::type;
    static constexpr bool enabled = true;
};

template <>
struct typed_gameplay_dispatch_traits<SelectDeploymentSiteMessage>
{
    using subscribers = typename typed_game_message_subscribers<
        SelectDeploymentSiteMessage,
        GameSystems>::type;
    static constexpr bool enabled = true;
};

template <>
struct typed_gameplay_dispatch_traits<ClearDeploymentSiteSelectionMessage>
{
    using subscribers = typename typed_game_message_subscribers<
        ClearDeploymentSiteSelectionMessage,
        GameSystems>::type;
    static constexpr bool enabled = true;
};

template <>
struct typed_gameplay_dispatch_traits<StartSiteAttemptMessage>
{
    using subscribers = typename typed_game_message_subscribers<
        StartSiteAttemptMessage,
        GameSystems>::type;
    static constexpr bool enabled = true;
};

template <>
struct typed_gameplay_dispatch_traits<DeploymentSiteSelectionChangedMessage>
{
    using subscribers = typename typed_game_message_subscribers<
        DeploymentSiteSelectionChangedMessage,
        GameSystems>::type;
    static constexpr bool enabled = true;
};

template <>
struct typed_gameplay_dispatch_traits<ReturnToRegionalMapMessage>
{
    using subscribers = typename typed_game_message_subscribers<
        ReturnToRegionalMapMessage,
        GameSystems>::type;
    static constexpr bool enabled = true;
};

template <>
struct typed_gameplay_dispatch_traits<TargetGrantedMessage>
{
    using subscribers = typename typed_game_message_subscribers<
        TargetGrantedMessage,
        GameSystems>::type;
    static constexpr bool enabled = true;
};

template <>
struct typed_gameplay_dispatch_traits<SiteAttemptEndedMessage>
{
    using subscribers = typename typed_game_message_subscribers<
        SiteAttemptEndedMessage,
        GameSystems>::type;
    static constexpr bool enabled = true;
};

template <>
struct typed_gameplay_dispatch_traits<ProgressionEventOccurredMessage>
{
    using subscribers = typename typed_game_message_subscribers<
        ProgressionEventOccurredMessage,
        GameSystems>::type;
    static constexpr bool enabled = true;
};

template <>
struct typed_gameplay_dispatch_traits<PurchaseEntrySelectedMessage>
{
    using subscribers = typename typed_game_message_subscribers<
        PurchaseEntrySelectedMessage,
        GameSystems>::type;
    static constexpr bool enabled = true;
};

template <>
struct typed_gameplay_dispatch_traits<CampaignReputationAwardRequestedMessage>
{
    using subscribers = typename typed_game_message_subscribers<
        CampaignReputationAwardRequestedMessage,
        GameSystems>::type;
    static constexpr bool enabled = true;
};

template <>
struct typed_gameplay_dispatch_traits<FactionReputationAwardRequestedMessage>
{
    using subscribers = typename typed_game_message_subscribers<
        FactionReputationAwardRequestedMessage,
        GameSystems>::type;
    static constexpr bool enabled = true;
};

template <>
struct typed_gameplay_dispatch_traits<TechnologyNodeClaimRequestedMessage>
{
    using subscribers = typename typed_game_message_subscribers<
        TechnologyNodeClaimRequestedMessage,
        GameSystems>::type;
    static constexpr bool enabled = true;
};

template <>
struct typed_gameplay_dispatch_traits<TechnologyNodeRefundRequestedMessage>
{
    using subscribers = typename typed_game_message_subscribers<
        TechnologyNodeRefundRequestedMessage,
        GameSystems>::type;
    static constexpr bool enabled = true;
};

template <>
struct typed_gameplay_dispatch_traits<SiteRunStartedMessage>
{
    using subscribers = typename typed_game_message_subscribers<
        SiteRunStartedMessage,
        GameSystems>::type;
    static constexpr bool enabled = true;
};

template <>
struct typed_gameplay_dispatch_traits<SiteSceneActivatedMessage>
{
    using subscribers = typename typed_game_message_subscribers<
        SiteSceneActivatedMessage,
        GameSystems>::type;
    static constexpr bool enabled = true;
};

template <>
struct typed_gameplay_dispatch_traits<StartSiteActionMessage>
{
    using subscribers = typename typed_game_message_subscribers<
        StartSiteActionMessage,
        GameSystems>::type;
    static constexpr bool enabled = true;
};

template <>
struct typed_gameplay_dispatch_traits<CancelSiteActionMessage>
{
    using subscribers = typename typed_game_message_subscribers<
        CancelSiteActionMessage,
        GameSystems>::type;
    static constexpr bool enabled = true;
};

template <>
struct typed_gameplay_dispatch_traits<PhoneListingPurchasedMessage>
{
    using subscribers = typename typed_game_message_subscribers<
        PhoneListingPurchasedMessage,
        GameSystems>::type;
    static constexpr bool enabled = true;
};

template <>
struct typed_gameplay_dispatch_traits<PhoneListingSoldMessage>
{
    using subscribers = typename typed_game_message_subscribers<
        PhoneListingSoldMessage,
        GameSystems>::type;
    static constexpr bool enabled = true;
};

template <>
struct typed_gameplay_dispatch_traits<InventoryTransferCompletedMessage>
{
    using subscribers = typename typed_game_message_subscribers<
        InventoryTransferCompletedMessage,
        GameSystems>::type;
    static constexpr bool enabled = true;
};

template <>
struct typed_gameplay_dispatch_traits<InventoryItemSubmittedMessage>
{
    using subscribers = typename typed_game_message_subscribers<
        InventoryItemSubmittedMessage,
        GameSystems>::type;
    static constexpr bool enabled = true;
};

template <>
struct typed_gameplay_dispatch_traits<InventoryItemUseCompletedMessage>
{
    using subscribers = typename typed_game_message_subscribers<
        InventoryItemUseCompletedMessage,
        GameSystems>::type;
    static constexpr bool enabled = true;
};

template <>
struct typed_gameplay_dispatch_traits<InventoryCraftCompletedMessage>
{
    using subscribers = typename typed_game_message_subscribers<
        InventoryCraftCompletedMessage,
        GameSystems>::type;
    static constexpr bool enabled = true;
};

template <>
struct typed_gameplay_dispatch_traits<InventoryDeliveryBatchRequestedMessage>
{
    using subscribers = typename typed_game_message_subscribers<
        InventoryDeliveryBatchRequestedMessage,
        GameSystems>::type;
    static constexpr bool enabled = true;
};

template <>
struct typed_gameplay_dispatch_traits<InventoryDeliveryRequestedMessage>
{
    using subscribers = typename typed_game_message_subscribers<
        InventoryDeliveryRequestedMessage,
        GameSystems>::type;
    static constexpr bool enabled = true;
};

template <>
struct typed_gameplay_dispatch_traits<InventoryWorkerPackInsertRequestedMessage>
{
    using subscribers = typename typed_game_message_subscribers<
        InventoryWorkerPackInsertRequestedMessage,
        GameSystems>::type;
    static constexpr bool enabled = true;
};

template <>
struct typed_gameplay_dispatch_traits<InventoryItemUseRequestedMessage>
{
    using subscribers = typename typed_game_message_subscribers<
        InventoryItemUseRequestedMessage,
        GameSystems>::type;
    static constexpr bool enabled = true;
};

template <>
struct typed_gameplay_dispatch_traits<InventoryItemConsumeRequestedMessage>
{
    using subscribers = typename typed_game_message_subscribers<
        InventoryItemConsumeRequestedMessage,
        GameSystems>::type;
    static constexpr bool enabled = true;
};

template <>
struct typed_gameplay_dispatch_traits<InventoryGlobalItemConsumeRequestedMessage>
{
    using subscribers = typename typed_game_message_subscribers<
        InventoryGlobalItemConsumeRequestedMessage,
        GameSystems>::type;
    static constexpr bool enabled = true;
};

template <>
struct typed_gameplay_dispatch_traits<InventoryTransferRequestedMessage>
{
    using subscribers = typename typed_game_message_subscribers<
        InventoryTransferRequestedMessage,
        GameSystems>::type;
    static constexpr bool enabled = true;
};

template <>
struct typed_gameplay_dispatch_traits<InventoryItemSubmitRequestedMessage>
{
    using subscribers = typename typed_game_message_subscribers<
        InventoryItemSubmitRequestedMessage,
        GameSystems>::type;
    static constexpr bool enabled = true;
};

template <>
struct typed_gameplay_dispatch_traits<InventorySlotTappedMessage>
{
    using subscribers = typename typed_game_message_subscribers<
        InventorySlotTappedMessage,
        GameSystems>::type;
    static constexpr bool enabled = true;
};

template <>
struct typed_gameplay_dispatch_traits<InventoryCraftCommitRequestedMessage>
{
    using subscribers = typename typed_game_message_subscribers<
        InventoryCraftCommitRequestedMessage,
        GameSystems>::type;
    static constexpr bool enabled = true;
};

template <>
struct typed_gameplay_dispatch_traits<ContractorHireRequestedMessage>
{
    using subscribers = typename typed_game_message_subscribers<
        ContractorHireRequestedMessage,
        GameSystems>::type;
    static constexpr bool enabled = true;
};

template <>
struct typed_gameplay_dispatch_traits<SiteUnlockablePurchaseRequestedMessage>
{
    using subscribers = typename typed_game_message_subscribers<
        SiteUnlockablePurchaseRequestedMessage,
        GameSystems>::type;
    static constexpr bool enabled = true;
};

template <>
struct typed_gameplay_dispatch_traits<CraftContextRequestedMessage>
{
    using subscribers = typename typed_game_message_subscribers<
        CraftContextRequestedMessage,
        GameSystems>::type;
    static constexpr bool enabled = true;
};

template <>
struct typed_gameplay_dispatch_traits<SiteGroundCoverPlacedMessage>
{
    using subscribers = typename typed_game_message_subscribers<
        SiteGroundCoverPlacedMessage,
        GameSystems>::type;
    static constexpr bool enabled = true;
};

template <>
struct typed_gameplay_dispatch_traits<SiteTilePlantingCompletedMessage>
{
    using subscribers = typename typed_game_message_subscribers<
        SiteTilePlantingCompletedMessage,
        GameSystems>::type;
    static constexpr bool enabled = true;
};

template <>
struct typed_gameplay_dispatch_traits<SiteTileWateredMessage>
{
    using subscribers = typename typed_game_message_subscribers<
        SiteTileWateredMessage,
        GameSystems>::type;
    static constexpr bool enabled = true;
};

template <>
struct typed_gameplay_dispatch_traits<SiteTileBurialClearedMessage>
{
    using subscribers = typename typed_game_message_subscribers<
        SiteTileBurialClearedMessage,
        GameSystems>::type;
    static constexpr bool enabled = true;
};

template <>
struct typed_gameplay_dispatch_traits<SiteTileHarvestedMessage>
{
    using subscribers = typename typed_game_message_subscribers<
        SiteTileHarvestedMessage,
        GameSystems>::type;
    static constexpr bool enabled = true;
};

template <>
struct typed_gameplay_dispatch_traits<SiteDevicePlacedMessage>
{
    using subscribers = typename typed_game_message_subscribers<
        SiteDevicePlacedMessage,
        GameSystems>::type;
    static constexpr bool enabled = true;
};

template <>
struct typed_gameplay_dispatch_traits<SiteDeviceBrokenMessage>
{
    using subscribers = typename typed_game_message_subscribers<
        SiteDeviceBrokenMessage,
        GameSystems>::type;
    static constexpr bool enabled = true;
};

template <>
struct typed_gameplay_dispatch_traits<SiteDeviceRepairedMessage>
{
    using subscribers = typename typed_game_message_subscribers<
        SiteDeviceRepairedMessage,
        GameSystems>::type;
    static constexpr bool enabled = true;
};

template <>
struct typed_gameplay_dispatch_traits<SiteDeviceConditionChangedMessage>
{
    using subscribers = typename typed_game_message_subscribers<
        SiteDeviceConditionChangedMessage,
        GameSystems>::type;
    static constexpr bool enabled = true;
};

template <>
struct typed_gameplay_dispatch_traits<TileEcologyChangedMessage>
{
    using subscribers = typename typed_game_message_subscribers<
        TileEcologyChangedMessage,
        GameSystems>::type;
    static constexpr bool enabled = true;
};

template <>
struct typed_gameplay_dispatch_traits<TileEcologyBatchChangedMessage>
{
    using subscribers = typename typed_game_message_subscribers<
        TileEcologyBatchChangedMessage,
        GameSystems>::type;
    static constexpr bool enabled = true;
};

template <>
struct typed_gameplay_dispatch_traits<LivingPlantStabilityChangedMessage>
{
    using subscribers = typename typed_game_message_subscribers<
        LivingPlantStabilityChangedMessage,
        GameSystems>::type;
    static constexpr bool enabled = true;
};

template <>
struct typed_gameplay_dispatch_traits<RestorationProgressChangedMessage>
{
    using subscribers = typename typed_game_message_subscribers<
        RestorationProgressChangedMessage,
        GameSystems>::type;
    static constexpr bool enabled = true;
};

template <>
struct typed_gameplay_dispatch_traits<SiteTileStateChangedMessage>
{
    using subscribers = typename typed_game_message_subscribers<
        SiteTileStateChangedMessage,
        GameSystems>::type;
    static constexpr bool enabled = true;
};

template <>
struct typed_gameplay_dispatch_traits<WorkerMetersChangedMessage>
{
    using subscribers = typename typed_game_message_subscribers<
        WorkerMetersChangedMessage,
        GameSystems>::type;
    static constexpr bool enabled = true;
};

template <>
struct typed_gameplay_dispatch_traits<WorkerMeterDeltaRequestedMessage>
{
    using subscribers = typename typed_game_message_subscribers<
        WorkerMeterDeltaRequestedMessage,
        GameSystems>::type;
    static constexpr bool enabled = true;
};

template <>
struct typed_gameplay_dispatch_traits<SiteActionCompletedMessage>
{
    using subscribers = typename typed_game_message_subscribers<
        SiteActionCompletedMessage,
        GameSystems>::type;
    static constexpr bool enabled = true;
};

template <>
struct typed_gameplay_dispatch_traits<PlacementReservationRequestedMessage>
{
    using subscribers = typename typed_game_message_subscribers<
        PlacementReservationRequestedMessage,
        GameSystems>::type;
    static constexpr bool enabled = true;
};

template <>
struct typed_gameplay_dispatch_traits<EconomyMoneyAwardRequestedMessage>
{
    using subscribers = typename typed_game_message_subscribers<
        EconomyMoneyAwardRequestedMessage,
        GameSystems>::type;
    static constexpr bool enabled = true;
};

template <>
struct typed_gameplay_dispatch_traits<SiteUnlockableRevealRequestedMessage>
{
    using subscribers = typename typed_game_message_subscribers<
        SiteUnlockableRevealRequestedMessage,
        GameSystems>::type;
    static constexpr bool enabled = true;
};

template <>
struct typed_gameplay_dispatch_traits<RunModifierAwardRequestedMessage>
{
    using subscribers = typename typed_game_message_subscribers<
        RunModifierAwardRequestedMessage,
        GameSystems>::type;
    static constexpr bool enabled = true;
};

template <>
struct typed_gameplay_dispatch_traits<PlacementReservationReleasedMessage>
{
    using subscribers = typename typed_game_message_subscribers<
        PlacementReservationReleasedMessage,
        GameSystems>::type;
    static constexpr bool enabled = true;
};

template <>
struct typed_gameplay_dispatch_traits<PlacementReservationAcceptedMessage>
{
    using subscribers = typename typed_game_message_subscribers<
        PlacementReservationAcceptedMessage,
        GameSystems>::type;
    static constexpr bool enabled = true;
};

template <>
struct typed_gameplay_dispatch_traits<PlacementReservationRejectedMessage>
{
    using subscribers = typename typed_game_message_subscribers<
        PlacementReservationRejectedMessage,
        GameSystems>::type;
    static constexpr bool enabled = true;
};
}  // namespace gs1
