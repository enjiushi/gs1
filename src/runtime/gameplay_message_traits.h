#pragma once

#include "messages/game_message.h"

namespace gs1
{
template <typename Message>
struct gameplay_message_traits;

template <typename Message>
[[nodiscard]] inline GameMessage make_game_message(const Message& payload) noexcept
{
    GameMessage message {};
    message.type = gameplay_message_traits<Message>::type;
    message.set_payload(payload);
    return message;
}

template <>
struct gameplay_message_traits<OpenMainMenuMessage>
{
    static constexpr GameMessageType type = GameMessageType::OpenMainMenu;
};

template <>
struct gameplay_message_traits<StartNewCampaignMessage>
{
    static constexpr GameMessageType type = GameMessageType::StartNewCampaign;
};

template <>
struct gameplay_message_traits<SelectDeploymentSiteMessage>
{
    static constexpr GameMessageType type = GameMessageType::SelectDeploymentSite;
};

template <>
struct gameplay_message_traits<ClearDeploymentSiteSelectionMessage>
{
    static constexpr GameMessageType type = GameMessageType::ClearDeploymentSiteSelection;
};

template <>
struct gameplay_message_traits<StartSiteAttemptMessage>
{
    static constexpr GameMessageType type = GameMessageType::StartSiteAttempt;
};

template <>
struct gameplay_message_traits<DeploymentSiteSelectionChangedMessage>
{
    static constexpr GameMessageType type = GameMessageType::DeploymentSiteSelectionChanged;
};

template <>
struct gameplay_message_traits<ReturnToRegionalMapMessage>
{
    static constexpr GameMessageType type = GameMessageType::ReturnToRegionalMap;
};

template <>
struct gameplay_message_traits<ProgressionEventOccurredMessage>
{
    static constexpr GameMessageType type = GameMessageType::ProgressionEventOccurred;
};

template <>
struct gameplay_message_traits<PurchaseEntrySelectedMessage>
{
    static constexpr GameMessageType type = GameMessageType::PurchaseEntrySelected;
};

template <>
struct gameplay_message_traits<TargetGrantedMessage>
{
    static constexpr GameMessageType type = GameMessageType::TargetGranted;
};

template <>
struct gameplay_message_traits<SiteAttemptEndedMessage>
{
    static constexpr GameMessageType type = GameMessageType::SiteAttemptEnded;
};

template <>
struct gameplay_message_traits<CampaignReputationAwardRequestedMessage>
{
    static constexpr GameMessageType type = GameMessageType::CampaignReputationAwardRequested;
};

template <>
struct gameplay_message_traits<FactionReputationAwardRequestedMessage>
{
    static constexpr GameMessageType type = GameMessageType::FactionReputationAwardRequested;
};

template <>
struct gameplay_message_traits<TechnologyNodeClaimRequestedMessage>
{
    static constexpr GameMessageType type = GameMessageType::TechnologyNodeClaimRequested;
};

template <>
struct gameplay_message_traits<TechnologyNodeRefundRequestedMessage>
{
    static constexpr GameMessageType type = GameMessageType::TechnologyNodeRefundRequested;
};

template <>
struct gameplay_message_traits<SiteRunStartedMessage>
{
    static constexpr GameMessageType type = GameMessageType::SiteRunStarted;
};

template <>
struct gameplay_message_traits<SiteSceneActivatedMessage>
{
    static constexpr GameMessageType type = GameMessageType::SiteSceneActivated;
};

template <>
struct gameplay_message_traits<SiteRefreshTickMessage>
{
    static constexpr GameMessageType type = GameMessageType::SiteRefreshTick;
};

template <>
struct gameplay_message_traits<StartSiteActionMessage>
{
    static constexpr GameMessageType type = GameMessageType::StartSiteAction;
};

template <>
struct gameplay_message_traits<PhoneListingPurchasedMessage>
{
    static constexpr GameMessageType type = GameMessageType::PhoneListingPurchased;
};

template <>
struct gameplay_message_traits<PhoneListingSoldMessage>
{
    static constexpr GameMessageType type = GameMessageType::PhoneListingSold;
};

template <>
struct gameplay_message_traits<InventoryTransferCompletedMessage>
{
    static constexpr GameMessageType type = GameMessageType::InventoryTransferCompleted;
};

template <>
struct gameplay_message_traits<InventoryItemSubmittedMessage>
{
    static constexpr GameMessageType type = GameMessageType::InventoryItemSubmitted;
};

template <>
struct gameplay_message_traits<InventoryItemUseCompletedMessage>
{
    static constexpr GameMessageType type = GameMessageType::InventoryItemUseCompleted;
};

template <>
struct gameplay_message_traits<InventoryCraftCompletedMessage>
{
    static constexpr GameMessageType type = GameMessageType::InventoryCraftCompleted;
};

template <>
struct gameplay_message_traits<InventoryDeliveryBatchRequestedMessage>
{
    static constexpr GameMessageType type = GameMessageType::InventoryDeliveryBatchRequested;
};

template <>
struct gameplay_message_traits<InventoryDeliveryRequestedMessage>
{
    static constexpr GameMessageType type = GameMessageType::InventoryDeliveryRequested;
};

template <>
struct gameplay_message_traits<InventoryWorkerPackInsertRequestedMessage>
{
    static constexpr GameMessageType type = GameMessageType::InventoryWorkerPackInsertRequested;
};

template <>
struct gameplay_message_traits<InventoryItemUseRequestedMessage>
{
    static constexpr GameMessageType type = GameMessageType::InventoryItemUseRequested;
};

template <>
struct gameplay_message_traits<InventoryItemConsumeRequestedMessage>
{
    static constexpr GameMessageType type = GameMessageType::InventoryItemConsumeRequested;
};

template <>
struct gameplay_message_traits<InventoryGlobalItemConsumeRequestedMessage>
{
    static constexpr GameMessageType type = GameMessageType::InventoryGlobalItemConsumeRequested;
};

template <>
struct gameplay_message_traits<InventoryTransferRequestedMessage>
{
    static constexpr GameMessageType type = GameMessageType::InventoryTransferRequested;
};

template <>
struct gameplay_message_traits<InventoryItemSubmitRequestedMessage>
{
    static constexpr GameMessageType type = GameMessageType::InventoryItemSubmitRequested;
};

template <>
struct gameplay_message_traits<InventoryCraftCommitRequestedMessage>
{
    static constexpr GameMessageType type = GameMessageType::InventoryCraftCommitRequested;
};

template <>
struct gameplay_message_traits<CraftContextRequestedMessage>
{
    static constexpr GameMessageType type = GameMessageType::InventoryCraftContextRequested;
};

template <>
struct gameplay_message_traits<SiteGroundCoverPlacedMessage>
{
    static constexpr GameMessageType type = GameMessageType::SiteGroundCoverPlaced;
};

template <>
struct gameplay_message_traits<SiteTilePlantingCompletedMessage>
{
    static constexpr GameMessageType type = GameMessageType::SiteTilePlantingCompleted;
};

template <>
struct gameplay_message_traits<SiteTileWateredMessage>
{
    static constexpr GameMessageType type = GameMessageType::SiteTileWatered;
};

template <>
struct gameplay_message_traits<SiteTileBurialClearedMessage>
{
    static constexpr GameMessageType type = GameMessageType::SiteTileBurialCleared;
};

template <>
struct gameplay_message_traits<SiteTileHarvestedMessage>
{
    static constexpr GameMessageType type = GameMessageType::SiteTileHarvested;
};

template <>
struct gameplay_message_traits<SiteDevicePlacedMessage>
{
    static constexpr GameMessageType type = GameMessageType::SiteDevicePlaced;
};

template <>
struct gameplay_message_traits<SiteDeviceBrokenMessage>
{
    static constexpr GameMessageType type = GameMessageType::SiteDeviceBroken;
};

template <>
struct gameplay_message_traits<SiteDeviceRepairedMessage>
{
    static constexpr GameMessageType type = GameMessageType::SiteDeviceRepaired;
};

template <>
struct gameplay_message_traits<SiteDeviceConditionChangedMessage>
{
    static constexpr GameMessageType type = GameMessageType::SiteDeviceConditionChanged;
};

template <>
struct gameplay_message_traits<TileEcologyChangedMessage>
{
    static constexpr GameMessageType type = GameMessageType::TileEcologyChanged;
};

template <>
struct gameplay_message_traits<TileEcologyBatchChangedMessage>
{
    static constexpr GameMessageType type = GameMessageType::TileEcologyBatchChanged;
};

template <>
struct gameplay_message_traits<LivingPlantStabilityChangedMessage>
{
    static constexpr GameMessageType type = GameMessageType::LivingPlantStabilityChanged;
};

template <>
struct gameplay_message_traits<RestorationProgressChangedMessage>
{
    static constexpr GameMessageType type = GameMessageType::RestorationProgressChanged;
};

template <>
struct gameplay_message_traits<SiteTileStateChangedMessage>
{
    static constexpr GameMessageType type = GameMessageType::SiteTileStateChanged;
};

template <>
struct gameplay_message_traits<WorkerMetersChangedMessage>
{
    static constexpr GameMessageType type = GameMessageType::WorkerMetersChanged;
};

template <>
struct gameplay_message_traits<WorkerMeterDeltaRequestedMessage>
{
    static constexpr GameMessageType type = GameMessageType::WorkerMeterDeltaRequested;
};

template <>
struct gameplay_message_traits<SiteActionCompletedMessage>
{
    static constexpr GameMessageType type = GameMessageType::SiteActionCompleted;
};

template <>
struct gameplay_message_traits<PlacementReservationRequestedMessage>
{
    static constexpr GameMessageType type = GameMessageType::PlacementReservationRequested;
};

template <>
struct gameplay_message_traits<EconomyMoneyAwardRequestedMessage>
{
    static constexpr GameMessageType type = GameMessageType::EconomyMoneyAwardRequested;
};

template <>
struct gameplay_message_traits<SiteUnlockableRevealRequestedMessage>
{
    static constexpr GameMessageType type = GameMessageType::SiteUnlockableRevealRequested;
};

template <>
struct gameplay_message_traits<RunModifierAwardRequestedMessage>
{
    static constexpr GameMessageType type = GameMessageType::RunModifierAwardRequested;
};

template <>
struct gameplay_message_traits<PlacementReservationAcceptedMessage>
{
    static constexpr GameMessageType type = GameMessageType::PlacementReservationAccepted;
};

template <>
struct gameplay_message_traits<PlacementReservationRejectedMessage>
{
    static constexpr GameMessageType type = GameMessageType::PlacementReservationRejected;
};

template <>
struct gameplay_message_traits<PlacementReservationReleasedMessage>
{
    static constexpr GameMessageType type = GameMessageType::PlacementReservationReleased;
};
}  // namespace gs1
