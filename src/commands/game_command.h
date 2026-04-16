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
inline constexpr std::size_t k_command_cache_line_size = 64U;
inline constexpr std::size_t k_command_payload_byte_count = k_command_cache_line_size - sizeof(std::uint8_t);

#define GS1_ASSERT_TRIVIAL_COMMAND_TYPE(Type) \
    static_assert(std::is_standard_layout_v<Type>, #Type " must remain standard layout."); \
    static_assert(std::is_trivial_v<Type>, #Type " must remain trivial."); \
    static_assert(std::is_trivially_copyable_v<Type>, #Type " must remain trivially copyable.")

#define GS1_ASSERT_COMMAND_PAYLOAD_LAYOUT(Type, ExpectedSize) \
    GS1_ASSERT_TRIVIAL_COMMAND_TYPE(Type); \
    static_assert(sizeof(Type) == (ExpectedSize), #Type " size changed; revisit command payload packing."); \
    static_assert(sizeof(Type) <= k_command_payload_byte_count, #Type " exceeds the game command payload budget.")

enum class GameCommandType : std::uint8_t
{
    OpenMainMenu,
    StartNewCampaign,
    SelectDeploymentSite,
    ClearDeploymentSiteSelection,
    DeploymentSiteSelectionChanged,
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
    SiteDevicePlaced,
    WorkerMeterDeltaRequested,
    WorkerMetersChanged,
    TileEcologyChanged,
    RestorationProgressChanged,
    TaskAcceptRequested,
    TaskRewardClaimRequested,
    PhoneListingPurchaseRequested,
    PhoneListingSaleRequested,
    InventoryDeliveryRequested,
    InventoryItemUseRequested,
    InventoryItemConsumeRequested,
    InventoryGlobalItemConsumeRequested,
    InventoryTransferRequested,
    InventoryCraftCommitRequested,
    ContractorHireRequested,
    SiteUnlockablePurchaseRequested,
    Count
};

inline constexpr std::size_t k_game_command_type_count =
    static_cast<std::size_t>(GameCommandType::Count);

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
    Reserved = 4
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

struct StartNewCampaignCommand final
{
    std::uint64_t campaign_seed;
    std::uint32_t campaign_days;
};

struct OpenMainMenuCommand final
{
};

struct SelectDeploymentSiteCommand final
{
    std::uint32_t site_id;
};

struct ClearDeploymentSiteSelectionCommand final
{
};

struct DeploymentSiteSelectionChangedCommand final
{
    std::uint32_t selected_site_id;
};

struct StartSiteAttemptCommand final
{
    std::uint32_t site_id;
};

struct ReturnToRegionalMapCommand final
{
};

struct SiteAttemptEndedCommand final
{
    std::uint32_t site_id;
    Gs1SiteAttemptResult result;
};

struct PresentLogCommand final
{
    char text[63];
};

struct SiteRunStartedCommand final
{
    std::uint32_t site_id;
    std::uint32_t site_run_id;
    std::uint32_t site_archetype_id;
    std::uint32_t attempt_index;
    std::uint64_t attempt_seed;
};

struct StartSiteActionCommand final
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

struct CancelSiteActionCommand final
{
    std::uint32_t action_id;
    std::uint32_t flags;
};

struct SiteActionStartedCommand final
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

struct SiteActionCompletedCommand final
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

struct SiteActionFailedCommand final
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

struct PlacementReservationRequestedCommand final
{
    std::uint32_t action_id;
    std::int32_t target_tile_x;
    std::int32_t target_tile_y;
    PlacementOccupancyLayer occupancy_layer;
    std::uint8_t reserved0[3];
    std::uint32_t subject_id;
};

struct PlacementReservationAcceptedCommand final
{
    std::uint32_t action_id;
    std::int32_t target_tile_x;
    std::int32_t target_tile_y;
    std::uint32_t reservation_token;
};

struct PlacementReservationRejectedCommand final
{
    std::uint32_t action_id;
    std::int32_t target_tile_x;
    std::int32_t target_tile_y;
    PlacementReservationRejectionReason reason_code;
    std::uint8_t reserved0[3];
};

struct PlacementReservationReleasedCommand final
{
    std::uint32_t action_id;
    std::uint32_t reservation_token;
};

struct SiteGroundCoverPlacedCommand final
{
    std::uint32_t action_id;
    std::int32_t target_tile_x;
    std::int32_t target_tile_y;
    std::uint32_t ground_cover_type_id;
    float initial_density;
    std::uint32_t flags;
};

struct SiteTilePlantingCompletedCommand final
{
    std::uint32_t action_id;
    std::int32_t target_tile_x;
    std::int32_t target_tile_y;
    std::uint32_t plant_type_id;
    float initial_density;
    std::uint32_t flags;
};

struct SiteTileWateredCommand final
{
    std::uint32_t source_id;
    std::int32_t target_tile_x;
    std::int32_t target_tile_y;
    float water_amount;
    std::uint32_t flags;
};

struct SiteTileBurialClearedCommand final
{
    std::uint32_t source_id;
    std::int32_t target_tile_x;
    std::int32_t target_tile_y;
    float cleared_amount;
    std::uint32_t flags;
};

struct SiteDevicePlacedCommand final
{
    std::uint32_t action_id;
    std::int32_t target_tile_x;
    std::int32_t target_tile_y;
    std::uint32_t structure_id;
    std::uint32_t flags;
};

struct WorkerMeterDeltaRequestedCommand final
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

struct WorkerMetersChangedCommand final
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

struct TileEcologyChangedCommand final
{
    std::int32_t target_tile_x;
    std::int32_t target_tile_y;
    std::uint32_t changed_mask;
    std::uint32_t plant_type_id;
    std::uint32_t ground_cover_type_id;
    float plant_density;
    float sand_burial;
};

struct RestorationProgressChangedCommand final
{
    std::uint32_t fully_grown_tile_count;
    std::uint32_t site_completion_tile_threshold;
    float normalized_progress;
};

struct TaskAcceptRequestedCommand final
{
    std::uint32_t task_instance_id;
};

struct TaskRewardClaimRequestedCommand final
{
    std::uint32_t task_instance_id;
    std::uint32_t reward_candidate_id;
};

struct PhoneListingPurchaseRequestedCommand final
{
    std::uint32_t listing_id;
    std::uint16_t quantity;
    std::uint16_t flags;
};

struct PhoneListingSaleRequestedCommand final
{
    std::uint32_t listing_id_or_item_id;
    std::uint16_t quantity;
    std::uint16_t flags;
};

struct InventoryDeliveryRequestedCommand final
{
    std::uint32_t item_id;
    std::uint16_t quantity;
    std::uint16_t minutes_until_arrival;
};

struct InventoryItemUseRequestedCommand final
{
    std::uint32_t item_id;
    std::uint16_t quantity;
    Gs1InventoryContainerKind container_kind;
    std::uint8_t slot_index;
};

struct InventoryItemConsumeRequestedCommand final
{
    std::uint32_t item_id;
    std::uint16_t quantity;
    Gs1InventoryContainerKind container_kind;
    std::uint8_t flags;
};

struct InventoryGlobalItemConsumeRequestedCommand final
{
    std::uint32_t item_id;
    std::uint16_t quantity;
    std::uint16_t flags;
};

struct InventoryTransferRequestedCommand final
{
    std::uint16_t source_slot_index;
    std::uint16_t destination_slot_index;
    std::uint16_t quantity;
    Gs1InventoryContainerKind source_container_kind;
    Gs1InventoryContainerKind destination_container_kind;
    std::uint8_t flags;
    std::uint8_t reserved0;
    std::uint32_t source_container_owner_id;
    std::uint32_t destination_container_owner_id;
};

struct InventoryCraftCommitRequestedCommand final
{
    std::uint32_t recipe_id;
    std::int32_t target_tile_x;
    std::int32_t target_tile_y;
    std::uint32_t flags;
};

struct ContractorHireRequestedCommand final
{
    std::uint32_t listing_or_offer_id;
    std::uint32_t requested_work_units;
};

struct SiteUnlockablePurchaseRequestedCommand final
{
    std::uint32_t unlockable_id;
};

GS1_ASSERT_COMMAND_PAYLOAD_LAYOUT(OpenMainMenuCommand, 1U);
GS1_ASSERT_COMMAND_PAYLOAD_LAYOUT(StartNewCampaignCommand, 16U);
GS1_ASSERT_COMMAND_PAYLOAD_LAYOUT(SelectDeploymentSiteCommand, 4U);
GS1_ASSERT_COMMAND_PAYLOAD_LAYOUT(ClearDeploymentSiteSelectionCommand, 1U);
GS1_ASSERT_COMMAND_PAYLOAD_LAYOUT(DeploymentSiteSelectionChangedCommand, 4U);
GS1_ASSERT_COMMAND_PAYLOAD_LAYOUT(StartSiteAttemptCommand, 4U);
GS1_ASSERT_COMMAND_PAYLOAD_LAYOUT(ReturnToRegionalMapCommand, 1U);
GS1_ASSERT_COMMAND_PAYLOAD_LAYOUT(SiteAttemptEndedCommand, 8U);
GS1_ASSERT_COMMAND_PAYLOAD_LAYOUT(PresentLogCommand, 63U);
GS1_ASSERT_COMMAND_PAYLOAD_LAYOUT(SiteRunStartedCommand, 24U);
GS1_ASSERT_COMMAND_PAYLOAD_LAYOUT(StartSiteActionCommand, 24U);
GS1_ASSERT_COMMAND_PAYLOAD_LAYOUT(CancelSiteActionCommand, 8U);
GS1_ASSERT_COMMAND_PAYLOAD_LAYOUT(SiteActionStartedCommand, 24U);
GS1_ASSERT_COMMAND_PAYLOAD_LAYOUT(SiteActionCompletedCommand, 24U);
GS1_ASSERT_COMMAND_PAYLOAD_LAYOUT(SiteActionFailedCommand, 24U);
GS1_ASSERT_COMMAND_PAYLOAD_LAYOUT(PlacementReservationRequestedCommand, 20U);
GS1_ASSERT_COMMAND_PAYLOAD_LAYOUT(PlacementReservationAcceptedCommand, 16U);
GS1_ASSERT_COMMAND_PAYLOAD_LAYOUT(PlacementReservationRejectedCommand, 16U);
GS1_ASSERT_COMMAND_PAYLOAD_LAYOUT(PlacementReservationReleasedCommand, 8U);
GS1_ASSERT_COMMAND_PAYLOAD_LAYOUT(SiteGroundCoverPlacedCommand, 24U);
GS1_ASSERT_COMMAND_PAYLOAD_LAYOUT(SiteTilePlantingCompletedCommand, 24U);
GS1_ASSERT_COMMAND_PAYLOAD_LAYOUT(SiteTileWateredCommand, 20U);
GS1_ASSERT_COMMAND_PAYLOAD_LAYOUT(SiteTileBurialClearedCommand, 20U);
GS1_ASSERT_COMMAND_PAYLOAD_LAYOUT(SiteDevicePlacedCommand, 20U);
GS1_ASSERT_COMMAND_PAYLOAD_LAYOUT(WorkerMeterDeltaRequestedCommand, 36U);
GS1_ASSERT_COMMAND_PAYLOAD_LAYOUT(WorkerMetersChangedCommand, 32U);
GS1_ASSERT_COMMAND_PAYLOAD_LAYOUT(TileEcologyChangedCommand, 28U);
GS1_ASSERT_COMMAND_PAYLOAD_LAYOUT(RestorationProgressChangedCommand, 12U);
GS1_ASSERT_COMMAND_PAYLOAD_LAYOUT(TaskAcceptRequestedCommand, 4U);
GS1_ASSERT_COMMAND_PAYLOAD_LAYOUT(TaskRewardClaimRequestedCommand, 8U);
GS1_ASSERT_COMMAND_PAYLOAD_LAYOUT(PhoneListingPurchaseRequestedCommand, 8U);
GS1_ASSERT_COMMAND_PAYLOAD_LAYOUT(PhoneListingSaleRequestedCommand, 8U);
GS1_ASSERT_COMMAND_PAYLOAD_LAYOUT(InventoryDeliveryRequestedCommand, 8U);
GS1_ASSERT_COMMAND_PAYLOAD_LAYOUT(InventoryItemUseRequestedCommand, 8U);
GS1_ASSERT_COMMAND_PAYLOAD_LAYOUT(InventoryItemConsumeRequestedCommand, 8U);
GS1_ASSERT_COMMAND_PAYLOAD_LAYOUT(InventoryGlobalItemConsumeRequestedCommand, 8U);
GS1_ASSERT_COMMAND_PAYLOAD_LAYOUT(InventoryTransferRequestedCommand, 20U);
GS1_ASSERT_COMMAND_PAYLOAD_LAYOUT(InventoryCraftCommitRequestedCommand, 16U);
GS1_ASSERT_COMMAND_PAYLOAD_LAYOUT(ContractorHireRequestedCommand, 8U);
GS1_ASSERT_COMMAND_PAYLOAD_LAYOUT(SiteUnlockablePurchaseRequestedCommand, 4U);

struct alignas(k_command_cache_line_size) GameCommand final
{
    unsigned char payload[k_command_payload_byte_count];
    GameCommandType type;

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
        GS1_ASSERT_TRIVIAL_COMMAND_TYPE(PayloadData);
        static_assert(sizeof(PayloadData) <= k_command_payload_byte_count, "Game command payload data exceeds command payload storage.");
        static_assert(alignof(PayloadData) <= alignof(GameCommand), "Game command payload data requires stronger alignment than GameCommand.");
    }
};

GS1_ASSERT_TRIVIAL_COMMAND_TYPE(GameCommand);
static_assert(sizeof(GameCommand) == k_command_cache_line_size, "GameCommand must fit exactly one cache line.");
static_assert(alignof(GameCommand) == k_command_cache_line_size, "GameCommand must be cache-line aligned.");
static_assert(offsetof(GameCommand, payload) == 0U, "GameCommand payload must start at byte zero.");
static_assert(offsetof(GameCommand, type) == k_command_payload_byte_count, "GameCommand type must sit at the tail byte.");

using GameCommandQueue = std::deque<GameCommand>;

#undef GS1_ASSERT_COMMAND_PAYLOAD_LAYOUT
#undef GS1_ASSERT_TRIVIAL_COMMAND_TYPE
}  // namespace gs1
