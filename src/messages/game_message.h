#pragma once

#include "support/id_types.h"
#include "gs1/types.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <new>
#include <type_traits>

namespace gs1
{
inline constexpr std::size_t k_message_cache_line_size = 64U;
inline constexpr std::size_t k_message_payload_byte_count = k_message_cache_line_size - sizeof(std::uint8_t);

#define GS1_ASSERT_TRIVIAL_MESSAGE_TYPE(Type) \
    static_assert(std::is_standard_layout_v<Type>, #Type " must remain standard layout."); \
    static_assert(std::is_trivial_v<Type>, #Type " must remain trivial."); \
    static_assert(std::is_trivially_copyable_v<Type>, #Type " must remain trivially copyable.")

#define GS1_ASSERT_MESSAGE_PAYLOAD_LAYOUT(Type, ExpectedSize) \
    GS1_ASSERT_TRIVIAL_MESSAGE_TYPE(Type); \
    static_assert(sizeof(Type) == (ExpectedSize), #Type " size changed; revisit message payload packing."); \
    static_assert(sizeof(Type) <= k_message_payload_byte_count, #Type " exceeds the game message payload budget.")

enum class GameMessageType : std::uint8_t
{
    OpenMainMenu,
    StartNewCampaign,
    SelectDeploymentSite,
    ClearDeploymentSiteSelection,
    OpenRegionalMapTechTree,
    CloseRegionalMapTechTree,
    SelectRegionalMapTechTreeFaction,
    DeploymentSiteSelectionChanged,
    CampaignCashDeltaRequested,
    CampaignReputationAwardRequested,
    FactionReputationAwardRequested,
    TechnologyNodeClaimRequested,
    TechnologyNodeRefundRequested,
    StartSiteAttempt,
    ReturnToRegionalMap,
    SiteAttemptEnded,
    PresentLog,
    SiteRunStarted,
    StartSiteAction,
    CancelSiteAction,
    SiteActionStarted,
    SiteActionCompleted,
    SiteActionFailed,
    PlacementReservationRequested,
    PlacementReservationAccepted,
    PlacementReservationRejected,
    PlacementReservationReleased,
    SiteGroundCoverPlaced,
    SiteTilePlantingCompleted,
    SiteTileWatered,
    SiteTileBurialCleared,
    SiteTileHarvested,
    SiteDevicePlaced,
    SiteDeviceBroken,
    SiteDeviceRepaired,
    SiteDeviceConditionChanged,
    WorkerMeterDeltaRequested,
    WorkerMetersChanged,
    TileEcologyChanged,
    TileEcologyBatchChanged,
    LivingPlantStabilityChanged,
    SiteTileStateChanged,
    RestorationProgressChanged,
    SiteRefreshTick,
    TaskAcceptRequested,
    TaskRewardClaimRequested,
    TaskRewardClaimResolved,
    PhoneListingPurchased,
    PhoneListingSold,
    InventoryTransferCompleted,
    InventoryItemSubmitted,
    InventoryItemUseCompleted,
    InventoryCraftCompleted,
    EconomyMoneyAwardRequested,
    SiteUnlockableRevealRequested,
    RunModifierAwardRequested,
    SiteModifierEndRequested,
    PhoneListingPurchaseRequested,
    PhoneListingSaleRequested,
    PhoneListingCartAddRequested,
    PhoneListingCartRemoveRequested,
    PhoneCartCheckoutRequested,
    PhonePanelSectionRequested,
    ClosePhonePanel,
    OpenSiteProtectionSelector,
    CloseSiteProtectionUi,
    SetSiteProtectionOverlayMode,
    InventoryDeliveryRequested,
    InventoryDeliveryBatchRequested,
    InventoryWorkerPackInsertRequested,
    InventoryItemUseRequested,
    InventoryItemConsumeRequested,
    InventoryGlobalItemConsumeRequested,
    InventoryTransferRequested,
    InventoryItemSubmitRequested,
    InventoryStorageViewRequest,
    InventorySlotTapped,
    InventoryCraftContextRequested,
    PlacementModeCursorMoved,
    PlacementModeCommitRejected,
    InventoryCraftCommitRequested,
    ContractorHireRequested,
    SiteUnlockablePurchaseRequested,
    Count
};

inline constexpr std::size_t k_game_message_type_count =
    static_cast<std::size_t>(GameMessageType::Count);

enum class SiteActionFailureReason : std::uint8_t
{
    None = 0,
    InvalidState = 1,
    InvalidTarget = 2,
    InsufficientResources = 3,
    Busy = 4,
    Cancelled = 5
};

enum class PlacementOccupancyLayer : std::uint8_t
{
    None = 0,
    GroundCover = 1,
    Structure = 2
};

enum class PlacementReservationRejectionReason : std::uint8_t
{
    None = 0,
    OutOfBounds = 1,
    TerrainBlocked = 2,
    Occupied = 3,
    Reserved = 4,
    Misaligned = 5
};

enum class PlacementReservationSubjectKind : std::uint8_t
{
    None = 0,
    GroundCoverType = 1,
    PlantType = 2,
    StructureType = 3
};

enum WorkerMeterChangedFlags : std::uint32_t
{
    WORKER_METER_CHANGED_NONE = 0,
    WORKER_METER_CHANGED_HEALTH = 1u << 0,
    WORKER_METER_CHANGED_HYDRATION = 1u << 1,
    WORKER_METER_CHANGED_NOURISHMENT = 1u << 2,
    WORKER_METER_CHANGED_ENERGY_CAP = 1u << 3,
    WORKER_METER_CHANGED_ENERGY = 1u << 4,
    WORKER_METER_CHANGED_MORALE = 1u << 5,
    WORKER_METER_CHANGED_WORK_EFFICIENCY = 1u << 6,
    WORKER_METER_CHANGED_SHELTER = 1u << 7
};

enum TileEcologyChangedFlags : std::uint32_t
{
    TILE_ECOLOGY_CHANGED_NONE = 0,
    TILE_ECOLOGY_CHANGED_OCCUPANCY = 1u << 0,
    TILE_ECOLOGY_CHANGED_DENSITY = 1u << 1,
    TILE_ECOLOGY_CHANGED_SAND_BURIAL = 1u << 2,
    TILE_ECOLOGY_CHANGED_MOISTURE = 1u << 3,
    TILE_ECOLOGY_CHANGED_FERTILITY = 1u << 4,
    TILE_ECOLOGY_CHANGED_SALINITY = 1u << 5,
    TILE_ECOLOGY_CHANGED_GROWTH_PRESSURE = 1u << 6
};

enum SiteRefreshTickFlags : std::uint32_t
{
    SITE_REFRESH_TICK_NONE = 0,
    SITE_REFRESH_TICK_TASK_BOARD = 1u << 0,
    SITE_REFRESH_TICK_PHONE_BUY_STOCK = 1u << 1
};

struct StartNewCampaignMessage final
{
    std::uint64_t campaign_seed;
    std::uint32_t campaign_days;
};

struct OpenMainMenuMessage final
{
};

struct SelectDeploymentSiteMessage final
{
    std::uint32_t site_id;
};

struct ClearDeploymentSiteSelectionMessage final
{
};

struct OpenRegionalMapTechTreeMessage final
{
};

struct CloseRegionalMapTechTreeMessage final
{
};

struct SelectRegionalMapTechTreeFactionMessage final
{
    std::uint32_t faction_id;
};

struct DeploymentSiteSelectionChangedMessage final
{
    std::uint32_t selected_site_id;
};

struct CampaignCashDeltaRequestedMessage final
{
    std::int32_t delta;
};

struct CampaignReputationAwardRequestedMessage final
{
    std::int32_t delta;
};

struct FactionReputationAwardRequestedMessage final
{
    std::uint32_t faction_id;
    std::int32_t delta;
};

struct TechnologyNodeClaimRequestedMessage final
{
    std::uint32_t tech_node_id;
    std::uint32_t reputation_faction_id;
};

struct TechnologyNodeRefundRequestedMessage final
{
    std::uint32_t tech_node_id;
};

struct StartSiteAttemptMessage final
{
    std::uint32_t site_id;
};

struct ReturnToRegionalMapMessage final
{
};

struct SiteAttemptEndedMessage final
{
    std::uint32_t site_id;
    Gs1SiteAttemptResult result;
};

struct PresentLogMessage final
{
    Gs1LogLevel level;
    char text[62];
};

struct SiteRunStartedMessage final
{
    std::uint32_t site_id;
    std::uint32_t site_run_id;
    std::uint32_t site_archetype_id;
    std::uint32_t attempt_index;
    std::uint64_t attempt_seed;
};

struct StartSiteActionMessage final
{
    Gs1SiteActionKind action_kind;
    std::uint8_t flags;
    std::uint16_t quantity;
    std::int32_t target_tile_x;
    std::int32_t target_tile_y;
    std::uint32_t primary_subject_id;
    std::uint32_t secondary_subject_id;
    std::uint32_t item_id;
};

struct CancelSiteActionMessage final
{
    std::uint32_t action_id;
    std::uint32_t flags;
};

struct SiteActionStartedMessage final
{
    std::uint32_t action_id;
    Gs1SiteActionKind action_kind;
    std::uint8_t flags;
    std::uint16_t reserved0;
    std::int32_t target_tile_x;
    std::int32_t target_tile_y;
    std::uint32_t primary_subject_id;
    float duration_minutes;
};

struct SiteActionCompletedMessage final
{
    std::uint32_t action_id;
    Gs1SiteActionKind action_kind;
    std::uint8_t flags;
    std::uint16_t reserved0;
    std::int32_t target_tile_x;
    std::int32_t target_tile_y;
    std::uint32_t primary_subject_id;
    std::uint32_t secondary_subject_id;
};

struct SiteActionFailedMessage final
{
    std::uint32_t action_id;
    Gs1SiteActionKind action_kind;
    SiteActionFailureReason reason;
    std::uint16_t flags;
    std::int32_t target_tile_x;
    std::int32_t target_tile_y;
    std::uint32_t primary_subject_id;
    std::uint32_t secondary_subject_id;
};

struct PlacementReservationRequestedMessage final
{
    std::uint32_t action_id;
    std::int32_t target_tile_x;
    std::int32_t target_tile_y;
    PlacementOccupancyLayer occupancy_layer;
    PlacementReservationSubjectKind subject_kind;
    std::uint8_t reserved0[2];
    std::uint32_t subject_id;
};

struct PlacementReservationAcceptedMessage final
{
    std::uint32_t action_id;
    std::int32_t target_tile_x;
    std::int32_t target_tile_y;
    std::uint32_t reservation_token;
};

struct PlacementReservationRejectedMessage final
{
    std::uint32_t action_id;
    std::int32_t target_tile_x;
    std::int32_t target_tile_y;
    PlacementReservationRejectionReason reason_code;
    std::uint8_t reserved0[3];
};

struct PlacementReservationReleasedMessage final
{
    std::uint32_t action_id;
    std::uint32_t reservation_token;
};

struct SiteGroundCoverPlacedMessage final
{
    std::uint32_t action_id;
    std::int32_t target_tile_x;
    std::int32_t target_tile_y;
    std::uint32_t ground_cover_type_id;
    float initial_density;
    std::uint32_t flags;
};

struct SiteTilePlantingCompletedMessage final
{
    std::uint32_t action_id;
    std::int32_t target_tile_x;
    std::int32_t target_tile_y;
    std::uint32_t plant_type_id;
    float initial_density;
    std::uint32_t flags;
};

struct SiteTileWateredMessage final
{
    std::uint32_t source_id;
    std::int32_t target_tile_x;
    std::int32_t target_tile_y;
    float water_amount;
    std::uint32_t flags;
};

struct SiteTileBurialClearedMessage final
{
    std::uint32_t source_id;
    std::int32_t target_tile_x;
    std::int32_t target_tile_y;
    float cleared_amount;
    std::uint32_t flags;
};

struct SiteTileHarvestedMessage final
{
    std::uint32_t action_id;
    std::int32_t target_tile_x;
    std::int32_t target_tile_y;
    std::uint32_t plant_type_id;
    std::uint32_t item_id;
    std::uint16_t item_quantity;
    std::uint16_t reserved0;
    float density_removed;
};

struct SiteDevicePlacedMessage final
{
    std::uint32_t action_id;
    std::int32_t target_tile_x;
    std::int32_t target_tile_y;
    std::uint32_t structure_id;
    std::uint32_t flags;
};

struct SiteDeviceBrokenMessage final
{
    std::uint64_t device_entity_id;
    std::int32_t target_tile_x;
    std::int32_t target_tile_y;
    std::uint32_t structure_id;
};

struct SiteDeviceRepairedMessage final
{
    std::uint32_t action_id;
    std::int32_t target_tile_x;
    std::int32_t target_tile_y;
    std::uint32_t flags;
};

struct SiteDeviceConditionChangedMessage final
{
    std::int32_t target_tile_x;
    std::int32_t target_tile_y;
    std::uint32_t structure_id;
    float device_integrity;
    std::uint32_t flags;
};

struct WorkerMeterDeltaRequestedMessage final
{
    std::uint32_t source_id;
    std::uint32_t flags;
    float health_delta;
    float hydration_delta;
    float nourishment_delta;
    float energy_cap_delta;
    float energy_delta;
    float morale_delta;
    float work_efficiency_delta;
};

struct WorkerMetersChangedMessage final
{
    std::uint32_t changed_mask;
    float player_health;
    float player_hydration;
    float player_nourishment;
    float player_energy_cap;
    float player_energy;
    float player_morale;
    float player_work_efficiency;
};

struct TileEcologyChangedMessage final
{
    std::int32_t target_tile_x;
    std::int32_t target_tile_y;
    std::uint32_t changed_mask;
    std::uint32_t plant_type_id;
    std::uint32_t ground_cover_type_id;
    float plant_density;
    float sand_burial;
};

struct TileEcologyBatchEntry final
{
    std::int16_t target_tile_x;
    std::int16_t target_tile_y;
    std::uint16_t changed_mask;
    std::uint16_t plant_density_centi;
    std::uint32_t plant_type_id;
    std::uint32_t ground_cover_type_id;
};

inline constexpr std::size_t k_tile_ecology_batch_entry_count = 3U;

struct TileEcologyBatchChangedMessage final
{
    std::uint8_t entry_count;
    std::uint8_t reserved0[3];
    TileEcologyBatchEntry entries[k_tile_ecology_batch_entry_count];
};

enum LivingPlantStabilityFlags : std::uint32_t
{
    LIVING_PLANT_STABILITY_NONE = 0,
    LIVING_PLANT_STABILITY_ALL_TRACKED_PLANTS_STABLE = 1u << 0,
    LIVING_PLANT_STABILITY_ANY_TRACKED_PLANT_WITHERING = 1u << 1
};

struct LivingPlantStabilityChangedMessage final
{
    std::uint32_t tracked_living_plant_count;
    std::uint32_t status_flags;
};

struct SiteTileStateChangedMessage final
{
    std::int32_t target_tile_x;
    std::int32_t target_tile_y;
    std::uint32_t changed_mask;
    std::uint32_t plant_type_id;
    float plant_density;
    float moisture;
    float heat;
    float dust;
    float wind_protection;
    float heat_protection;
    float dust_protection;
};

struct RestorationProgressChangedMessage final
{
    std::uint32_t fully_grown_tile_count;
    std::uint32_t site_completion_tile_threshold;
    float normalized_progress;
};

struct SiteRefreshTickMessage final
{
    std::uint32_t refresh_mask;
};

struct TaskAcceptRequestedMessage final
{
    std::uint32_t task_instance_id;
};

struct TaskRewardClaimRequestedMessage final
{
    std::uint32_t task_instance_id;
    std::uint32_t reward_candidate_id;
};

struct TaskRewardClaimResolvedMessage final
{
    std::uint32_t task_instance_id;
    std::uint32_t task_template_id;
    std::uint32_t reward_candidate_count;
};

struct PhoneListingPurchasedMessage final
{
    std::uint32_t listing_id;
    std::uint32_t item_id;
    std::uint16_t quantity;
    std::uint16_t flags;
};

struct PhoneListingSoldMessage final
{
    std::uint32_t listing_id;
    std::uint32_t item_id;
    std::uint16_t quantity;
    std::uint16_t flags;
};

struct InventoryTransferCompletedMessage final
{
    std::uint32_t source_storage_id;
    std::uint32_t destination_storage_id;
    std::uint32_t item_id;
    std::uint16_t quantity;
    std::uint16_t flags;
};

struct InventoryItemSubmittedMessage final
{
    std::uint32_t item_id;
    std::uint16_t quantity;
    std::uint16_t flags;
};

struct InventoryItemUseCompletedMessage final
{
    std::uint32_t item_id;
    std::uint16_t quantity;
    std::uint16_t flags;
};

struct InventoryCraftCompletedMessage final
{
    std::uint32_t recipe_id;
    std::uint32_t output_item_id;
    std::uint16_t output_quantity;
    std::uint16_t reserved;
    std::uint32_t output_storage_id;
};

struct EconomyMoneyAwardRequestedMessage final
{
    std::int32_t delta;
};

struct SiteUnlockableRevealRequestedMessage final
{
    std::uint32_t unlockable_id;
};

struct RunModifierAwardRequestedMessage final
{
    std::uint32_t modifier_id;
};

struct SiteModifierEndRequestedMessage final
{
    std::uint32_t modifier_id;
};

struct PhoneListingPurchaseRequestedMessage final
{
    std::uint32_t listing_id;
    std::uint16_t quantity;
    std::uint16_t flags;
};

struct PhoneListingSaleRequestedMessage final
{
    std::uint32_t listing_id_or_item_id;
    std::uint16_t quantity;
    std::uint16_t flags;
};

struct PhoneListingCartAddRequestedMessage final
{
    std::uint32_t listing_id;
    std::uint16_t quantity;
    std::uint16_t flags;
};

struct PhoneListingCartRemoveRequestedMessage final
{
    std::uint32_t listing_id;
    std::uint16_t quantity;
    std::uint16_t flags;
};

struct PhoneCartCheckoutRequestedMessage final
{
};

struct PhonePanelSectionRequestedMessage final
{
    Gs1PhonePanelSection section;
    std::uint8_t reserved0[3];
};

struct ClosePhonePanelMessage final
{
};

struct OpenSiteProtectionSelectorMessage final
{
};

struct CloseSiteProtectionUiMessage final
{
};

struct SetSiteProtectionOverlayModeMessage final
{
    Gs1SiteProtectionOverlayMode mode;
    std::uint8_t reserved0[3];
};

struct InventoryDeliveryRequestedMessage final
{
    std::uint32_t item_id;
    std::uint16_t quantity;
    std::uint16_t minutes_until_arrival;
};

struct InventoryDeliveryBatchEntry final
{
    std::uint16_t item_id;
    std::uint16_t quantity;
};

inline constexpr std::size_t k_inventory_delivery_batch_entry_count = 10U;

struct InventoryDeliveryBatchRequestedMessage final
{
    std::uint16_t minutes_until_arrival;
    std::uint8_t entry_count;
    std::uint8_t reserved0;
    InventoryDeliveryBatchEntry entries[k_inventory_delivery_batch_entry_count];
};

struct InventoryWorkerPackInsertRequestedMessage final
{
    std::uint32_t item_id;
    std::uint16_t quantity;
    std::uint16_t reserved0;
};

struct InventoryItemUseRequestedMessage final
{
    std::uint32_t item_id;
    std::uint32_t storage_id;
    std::uint16_t quantity;
    std::uint16_t slot_index;
};

struct InventoryItemConsumeRequestedMessage final
{
    std::uint32_t item_id;
    std::uint16_t quantity;
    Gs1InventoryContainerKind container_kind;
    std::uint8_t flags;
};

inline constexpr std::uint8_t k_inventory_item_consume_flag_ignore_action_reservations = 1U << 0U;

struct InventoryGlobalItemConsumeRequestedMessage final
{
    std::uint32_t item_id;
    std::uint16_t quantity;
    std::uint16_t flags;
};

inline constexpr std::uint8_t k_inventory_transfer_flag_resolve_destination_in_dll = 1U << 0U;

struct InventoryTransferRequestedMessage final
{
    std::uint32_t source_storage_id;
    std::uint32_t destination_storage_id;
    std::uint16_t source_slot_index;
    std::uint16_t destination_slot_index;
    std::uint16_t quantity;
    std::uint8_t flags;
    std::uint8_t reserved0;
};

struct InventoryItemSubmitRequestedMessage final
{
    std::uint32_t source_storage_id;
    std::uint16_t source_slot_index;
    std::uint16_t quantity;
};

struct InventoryStorageViewRequestMessage final
{
    std::uint32_t storage_id;
    Gs1InventoryViewEventKind event_kind;
    std::uint8_t reserved0[3];
};

struct InventorySlotTappedMessage final
{
    std::uint32_t storage_id;
    std::uint32_t item_instance_id;
    std::uint16_t slot_index;
    Gs1InventoryContainerKind container_kind;
    std::uint8_t reserved0;
};

struct CraftContextRequestedMessage final
{
    std::int32_t tile_x;
    std::int32_t tile_y;
    std::uint32_t flags;
};

struct PlacementModeCursorMovedMessage final
{
    std::int32_t tile_x;
    std::int32_t tile_y;
    std::uint32_t flags;
};

struct PlacementModeCommitRejectedMessage final
{
    std::int32_t tile_x;
    std::int32_t tile_y;
    std::uint64_t blocked_mask;
    Gs1SiteActionKind action_kind;
    std::uint32_t item_id;
};

struct InventoryCraftCommitRequestedMessage final
{
    std::uint32_t recipe_id;
    std::int32_t target_tile_x;
    std::int32_t target_tile_y;
    std::uint32_t flags;
};

struct ContractorHireRequestedMessage final
{
    std::uint32_t listing_or_offer_id;
    std::uint32_t requested_work_units;
};

struct SiteUnlockablePurchaseRequestedMessage final
{
    std::uint32_t unlockable_id;
};

GS1_ASSERT_MESSAGE_PAYLOAD_LAYOUT(OpenMainMenuMessage, 1U);
GS1_ASSERT_MESSAGE_PAYLOAD_LAYOUT(StartNewCampaignMessage, 16U);
GS1_ASSERT_MESSAGE_PAYLOAD_LAYOUT(SelectDeploymentSiteMessage, 4U);
GS1_ASSERT_MESSAGE_PAYLOAD_LAYOUT(ClearDeploymentSiteSelectionMessage, 1U);
GS1_ASSERT_MESSAGE_PAYLOAD_LAYOUT(OpenRegionalMapTechTreeMessage, 1U);
GS1_ASSERT_MESSAGE_PAYLOAD_LAYOUT(CloseRegionalMapTechTreeMessage, 1U);
GS1_ASSERT_MESSAGE_PAYLOAD_LAYOUT(SelectRegionalMapTechTreeFactionMessage, 4U);
GS1_ASSERT_MESSAGE_PAYLOAD_LAYOUT(DeploymentSiteSelectionChangedMessage, 4U);
GS1_ASSERT_MESSAGE_PAYLOAD_LAYOUT(CampaignCashDeltaRequestedMessage, 4U);
GS1_ASSERT_MESSAGE_PAYLOAD_LAYOUT(CampaignReputationAwardRequestedMessage, 4U);
GS1_ASSERT_MESSAGE_PAYLOAD_LAYOUT(FactionReputationAwardRequestedMessage, 8U);
GS1_ASSERT_MESSAGE_PAYLOAD_LAYOUT(TechnologyNodeClaimRequestedMessage, 8U);
GS1_ASSERT_MESSAGE_PAYLOAD_LAYOUT(TechnologyNodeRefundRequestedMessage, 4U);
GS1_ASSERT_MESSAGE_PAYLOAD_LAYOUT(StartSiteAttemptMessage, 4U);
GS1_ASSERT_MESSAGE_PAYLOAD_LAYOUT(ReturnToRegionalMapMessage, 1U);
GS1_ASSERT_MESSAGE_PAYLOAD_LAYOUT(SiteAttemptEndedMessage, 8U);
GS1_ASSERT_MESSAGE_PAYLOAD_LAYOUT(PresentLogMessage, 63U);
GS1_ASSERT_MESSAGE_PAYLOAD_LAYOUT(SiteRunStartedMessage, 24U);
GS1_ASSERT_MESSAGE_PAYLOAD_LAYOUT(StartSiteActionMessage, 24U);
GS1_ASSERT_MESSAGE_PAYLOAD_LAYOUT(CancelSiteActionMessage, 8U);
GS1_ASSERT_MESSAGE_PAYLOAD_LAYOUT(SiteActionStartedMessage, 24U);
GS1_ASSERT_MESSAGE_PAYLOAD_LAYOUT(SiteActionCompletedMessage, 24U);
GS1_ASSERT_MESSAGE_PAYLOAD_LAYOUT(SiteActionFailedMessage, 24U);
GS1_ASSERT_MESSAGE_PAYLOAD_LAYOUT(PlacementReservationRequestedMessage, 20U);
GS1_ASSERT_MESSAGE_PAYLOAD_LAYOUT(PlacementReservationAcceptedMessage, 16U);
GS1_ASSERT_MESSAGE_PAYLOAD_LAYOUT(PlacementReservationRejectedMessage, 16U);
GS1_ASSERT_MESSAGE_PAYLOAD_LAYOUT(PlacementReservationReleasedMessage, 8U);
GS1_ASSERT_MESSAGE_PAYLOAD_LAYOUT(SiteGroundCoverPlacedMessage, 24U);
GS1_ASSERT_MESSAGE_PAYLOAD_LAYOUT(SiteTilePlantingCompletedMessage, 24U);
GS1_ASSERT_MESSAGE_PAYLOAD_LAYOUT(SiteTileWateredMessage, 20U);
GS1_ASSERT_MESSAGE_PAYLOAD_LAYOUT(SiteTileBurialClearedMessage, 20U);
GS1_ASSERT_MESSAGE_PAYLOAD_LAYOUT(SiteTileHarvestedMessage, 28U);
GS1_ASSERT_MESSAGE_PAYLOAD_LAYOUT(SiteDevicePlacedMessage, 20U);
GS1_ASSERT_MESSAGE_PAYLOAD_LAYOUT(SiteDeviceBrokenMessage, 24U);
GS1_ASSERT_MESSAGE_PAYLOAD_LAYOUT(SiteDeviceRepairedMessage, 16U);
GS1_ASSERT_MESSAGE_PAYLOAD_LAYOUT(SiteDeviceConditionChangedMessage, 20U);
GS1_ASSERT_MESSAGE_PAYLOAD_LAYOUT(WorkerMeterDeltaRequestedMessage, 36U);
GS1_ASSERT_MESSAGE_PAYLOAD_LAYOUT(WorkerMetersChangedMessage, 32U);
GS1_ASSERT_MESSAGE_PAYLOAD_LAYOUT(TileEcologyChangedMessage, 28U);
GS1_ASSERT_MESSAGE_PAYLOAD_LAYOUT(TileEcologyBatchEntry, 16U);
GS1_ASSERT_MESSAGE_PAYLOAD_LAYOUT(TileEcologyBatchChangedMessage, 52U);
GS1_ASSERT_MESSAGE_PAYLOAD_LAYOUT(LivingPlantStabilityChangedMessage, 8U);
GS1_ASSERT_MESSAGE_PAYLOAD_LAYOUT(SiteTileStateChangedMessage, 44U);
GS1_ASSERT_MESSAGE_PAYLOAD_LAYOUT(RestorationProgressChangedMessage, 12U);
GS1_ASSERT_MESSAGE_PAYLOAD_LAYOUT(SiteRefreshTickMessage, 4U);
GS1_ASSERT_MESSAGE_PAYLOAD_LAYOUT(TaskAcceptRequestedMessage, 4U);
GS1_ASSERT_MESSAGE_PAYLOAD_LAYOUT(TaskRewardClaimRequestedMessage, 8U);
GS1_ASSERT_MESSAGE_PAYLOAD_LAYOUT(TaskRewardClaimResolvedMessage, 12U);
GS1_ASSERT_MESSAGE_PAYLOAD_LAYOUT(PhoneListingPurchasedMessage, 12U);
GS1_ASSERT_MESSAGE_PAYLOAD_LAYOUT(PhoneListingSoldMessage, 12U);
GS1_ASSERT_MESSAGE_PAYLOAD_LAYOUT(InventoryTransferCompletedMessage, 16U);
GS1_ASSERT_MESSAGE_PAYLOAD_LAYOUT(InventoryItemSubmittedMessage, 8U);
GS1_ASSERT_MESSAGE_PAYLOAD_LAYOUT(InventoryItemUseCompletedMessage, 8U);
GS1_ASSERT_MESSAGE_PAYLOAD_LAYOUT(InventoryCraftCompletedMessage, 16U);
GS1_ASSERT_MESSAGE_PAYLOAD_LAYOUT(EconomyMoneyAwardRequestedMessage, 4U);
GS1_ASSERT_MESSAGE_PAYLOAD_LAYOUT(SiteUnlockableRevealRequestedMessage, 4U);
GS1_ASSERT_MESSAGE_PAYLOAD_LAYOUT(RunModifierAwardRequestedMessage, 4U);
GS1_ASSERT_MESSAGE_PAYLOAD_LAYOUT(SiteModifierEndRequestedMessage, 4U);
GS1_ASSERT_MESSAGE_PAYLOAD_LAYOUT(PhoneListingPurchaseRequestedMessage, 8U);
GS1_ASSERT_MESSAGE_PAYLOAD_LAYOUT(PhoneListingSaleRequestedMessage, 8U);
GS1_ASSERT_MESSAGE_PAYLOAD_LAYOUT(PhoneListingCartAddRequestedMessage, 8U);
GS1_ASSERT_MESSAGE_PAYLOAD_LAYOUT(PhoneListingCartRemoveRequestedMessage, 8U);
GS1_ASSERT_MESSAGE_PAYLOAD_LAYOUT(PhoneCartCheckoutRequestedMessage, 1U);
GS1_ASSERT_MESSAGE_PAYLOAD_LAYOUT(PhonePanelSectionRequestedMessage, 4U);
GS1_ASSERT_MESSAGE_PAYLOAD_LAYOUT(ClosePhonePanelMessage, 1U);
GS1_ASSERT_MESSAGE_PAYLOAD_LAYOUT(OpenSiteProtectionSelectorMessage, 1U);
GS1_ASSERT_MESSAGE_PAYLOAD_LAYOUT(CloseSiteProtectionUiMessage, 1U);
GS1_ASSERT_MESSAGE_PAYLOAD_LAYOUT(SetSiteProtectionOverlayModeMessage, 4U);
GS1_ASSERT_MESSAGE_PAYLOAD_LAYOUT(InventoryDeliveryRequestedMessage, 8U);
GS1_ASSERT_MESSAGE_PAYLOAD_LAYOUT(InventoryDeliveryBatchEntry, 4U);
GS1_ASSERT_MESSAGE_PAYLOAD_LAYOUT(InventoryDeliveryBatchRequestedMessage, 44U);
GS1_ASSERT_MESSAGE_PAYLOAD_LAYOUT(InventoryWorkerPackInsertRequestedMessage, 8U);
GS1_ASSERT_MESSAGE_PAYLOAD_LAYOUT(InventoryItemUseRequestedMessage, 12U);
GS1_ASSERT_MESSAGE_PAYLOAD_LAYOUT(InventoryItemConsumeRequestedMessage, 8U);
GS1_ASSERT_MESSAGE_PAYLOAD_LAYOUT(InventoryGlobalItemConsumeRequestedMessage, 8U);
GS1_ASSERT_MESSAGE_PAYLOAD_LAYOUT(InventoryTransferRequestedMessage, 16U);
GS1_ASSERT_MESSAGE_PAYLOAD_LAYOUT(InventoryItemSubmitRequestedMessage, 8U);
GS1_ASSERT_MESSAGE_PAYLOAD_LAYOUT(InventoryStorageViewRequestMessage, 8U);
GS1_ASSERT_MESSAGE_PAYLOAD_LAYOUT(InventorySlotTappedMessage, 12U);
GS1_ASSERT_MESSAGE_PAYLOAD_LAYOUT(CraftContextRequestedMessage, 12U);
GS1_ASSERT_MESSAGE_PAYLOAD_LAYOUT(PlacementModeCursorMovedMessage, 12U);
GS1_ASSERT_MESSAGE_PAYLOAD_LAYOUT(PlacementModeCommitRejectedMessage, 24U);
GS1_ASSERT_MESSAGE_PAYLOAD_LAYOUT(InventoryCraftCommitRequestedMessage, 16U);
GS1_ASSERT_MESSAGE_PAYLOAD_LAYOUT(ContractorHireRequestedMessage, 8U);
GS1_ASSERT_MESSAGE_PAYLOAD_LAYOUT(SiteUnlockablePurchaseRequestedMessage, 4U);

struct alignas(k_message_cache_line_size) GameMessage final
{
    unsigned char payload[k_message_payload_byte_count];
    GameMessageType type;

    template <typename PayloadData>
    [[nodiscard]] PayloadData& emplace_payload() noexcept
    {
        validate_payload_type<PayloadData>();
        auto* ptr = std::construct_at(reinterpret_cast<PayloadData*>(payload), PayloadData {});
        return *std::launder(ptr);
    }

    template <typename PayloadData>
    [[nodiscard]] PayloadData& emplace_payload(const PayloadData& value) noexcept
    {
        validate_payload_type<PayloadData>();
        auto* ptr = std::construct_at(reinterpret_cast<PayloadData*>(payload), value);
        return *std::launder(ptr);
    }

    template <typename PayloadData>
    void set_payload(const PayloadData& value) noexcept
    {
        (void)emplace_payload(value);
    }

    template <typename PayloadData>
    [[nodiscard]] PayloadData& payload_as() noexcept
    {
        validate_payload_type<PayloadData>();
        return *std::launder(reinterpret_cast<PayloadData*>(payload));
    }

    template <typename PayloadData>
    [[nodiscard]] const PayloadData& payload_as() const noexcept
    {
        validate_payload_type<PayloadData>();
        return *std::launder(reinterpret_cast<const PayloadData*>(payload));
    }

private:
    template <typename PayloadData>
    static constexpr void validate_payload_type() noexcept
    {
        GS1_ASSERT_TRIVIAL_MESSAGE_TYPE(PayloadData);
        static_assert(sizeof(PayloadData) <= k_message_payload_byte_count, "Game message payload data exceeds message payload storage.");
        static_assert(alignof(PayloadData) <= alignof(GameMessage), "Game message payload data requires stronger alignment than GameMessage.");
    }
};

GS1_ASSERT_TRIVIAL_MESSAGE_TYPE(GameMessage);
static_assert(sizeof(GameMessage) == k_message_cache_line_size, "GameMessage must fit exactly one cache line.");
static_assert(alignof(GameMessage) == k_message_cache_line_size, "GameMessage must be cache-line aligned.");
static_assert(offsetof(GameMessage, payload) == 0U, "GameMessage payload must start at byte zero.");
static_assert(offsetof(GameMessage, type) == k_message_payload_byte_count, "GameMessage type must sit at the tail byte.");

using GameMessageQueue = std::deque<GameMessage>;

#undef GS1_ASSERT_MESSAGE_PAYLOAD_LAYOUT
#undef GS1_ASSERT_TRIVIAL_MESSAGE_TYPE
}  // namespace gs1
