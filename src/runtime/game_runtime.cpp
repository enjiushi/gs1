#include "runtime/game_runtime.h"

#include "campaign/systems/campaign_flow_system.h"
#include "campaign/systems/campaign_progression_system.h"
#include "campaign/systems/campaign_time_system.h"
#include "campaign/systems/faction_reputation_system.h"
#include "campaign/systems/loadout_planner_system.h"
#include "campaign/systems/technology_system.h"
#include "content/content_loader.h"
#include "runtime/game_systems.h"
#include "runtime/runtime_split_state_compat.h"
#include "site/systems/action_execution_system.h"
#include "site/systems/camp_durability_system.h"
#include "site/systems/craft_system.h"
#include "site/systems/device_maintenance_system.h"
#include "site/systems/device_support_system.h"
#include "site/systems/device_weather_contribution_system.h"
#include "site/systems/ecology_system.h"
#include "site/systems/economy_phone_system.h"
#include "site/systems/failure_recovery_system.h"
#include "site/systems/inventory_system.h"
#include "site/systems/local_weather_resolve_system.h"
#include "site/systems/modifier_system.h"
#include "site/systems/plant_weather_contribution_system.h"
#include "site/systems/placement_validation_system.h"
#include "site/systems/site_completion_system.h"
#include "site/systems/site_flow_system.h"
#include "site/systems/site_time_system.h"
#include "site/systems/task_board_system.h"
#include "site/systems/weather_event_system.h"
#include "site/systems/worker_condition_system.h"
#include "site/site_world.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <limits>
#include <string_view>
#include <utility>

namespace gs1
{
class RuntimePrivilegedStateMutation final
{
public:
    explicit RuntimePrivilegedStateMutation(StateManager& state_manager) noexcept
        : state_manager_(&state_manager)
    {
        state_manager_->push_privileged_mutation();
    }

    ~RuntimePrivilegedStateMutation()
    {
        if (state_manager_ != nullptr)
        {
            state_manager_->pop_privileged_mutation();
        }
    }

    RuntimePrivilegedStateMutation(const RuntimePrivilegedStateMutation&) = delete;
    RuntimePrivilegedStateMutation& operator=(const RuntimePrivilegedStateMutation&) = delete;

private:
    StateManager* state_manager_ {nullptr};
};

class RuntimeMutationScope final
{
public:
    RuntimeMutationScope(StateManager& state_manager, IRuntimeSystem& system)
        : state_manager_(&state_manager)
    {
        state_manager_->push_current_mutating_system(&system);
    }

    ~RuntimeMutationScope()
    {
        if (state_manager_ != nullptr)
        {
            state_manager_->pop_current_mutating_system();
        }
    }

    RuntimeMutationScope(const RuntimeMutationScope&) = delete;
    RuntimeMutationScope& operator=(const RuntimeMutationScope&) = delete;

private:
    StateManager* state_manager_ {nullptr};
};

class RuntimeInlineGameMessageScope final
{
public:
    explicit RuntimeInlineGameMessageScope(GameRuntime& runtime) noexcept
        : runtime_(&runtime)
    {
        ++runtime_->inline_game_message_depth_;
    }

    ~RuntimeInlineGameMessageScope()
    {
        if (runtime_ != nullptr && runtime_->inline_game_message_depth_ > 0U)
        {
            --runtime_->inline_game_message_depth_;
        }
    }

    RuntimeInlineGameMessageScope(const RuntimeInlineGameMessageScope&) = delete;
    RuntimeInlineGameMessageScope& operator=(const RuntimeInlineGameMessageScope&) = delete;

private:
    GameRuntime* runtime_ {nullptr};
};

namespace
{
inline constexpr std::uint32_t k_inline_game_message_warn_depth = 4U;
static_assert(GameSystems::size == 25U, "Update GameSystems when the runtime system list changes.");

[[nodiscard]] std::string unescape_json_string(std::string_view value)
{
    std::string result;
    result.reserve(value.size());

    for (std::size_t index = 0; index < value.size(); ++index)
    {
        const char ch = value[index];
        if (ch != '\\' || (index + 1U) >= value.size())
        {
            result.push_back(ch);
            continue;
        }

        const char escaped = value[++index];
        switch (escaped)
        {
        case '\\':
            result.push_back('\\');
            break;
        case '"':
            result.push_back('"');
            break;
        case 'n':
            result.push_back('\n');
            break;
        case 'r':
            result.push_back('\r');
            break;
        case 't':
            result.push_back('\t');
            break;
        default:
            result.push_back(escaped);
            break;
        }
    }

    return result;
}

[[nodiscard]] std::size_t skip_json_whitespace(std::string_view text, std::size_t index) noexcept
{
    while (index < text.size())
    {
        const char ch = text[index];
        if (ch != ' ' && ch != '\t' && ch != '\r' && ch != '\n')
        {
            break;
        }
        ++index;
    }
    return index;
}

[[nodiscard]] std::size_t find_json_string_end(std::string_view text, std::size_t start_index) noexcept
{
    bool escaping = false;
    for (std::size_t index = start_index; index < text.size(); ++index)
    {
        const char ch = text[index];
        if (escaping)
        {
            escaping = false;
            continue;
        }
        if (ch == '\\')
        {
            escaping = true;
            continue;
        }
        if (ch == '"')
        {
            return index;
        }
    }

    return std::string_view::npos;
}

[[nodiscard]] std::filesystem::path extract_project_config_root_from_adapter_config_json(
    std::string_view adapter_config_json_utf8)
{
    constexpr std::string_view key = "\"project_config_root\"";
    const std::size_t key_pos = adapter_config_json_utf8.find(key);
    if (key_pos == std::string_view::npos)
    {
        return {};
    }

    std::size_t cursor = skip_json_whitespace(adapter_config_json_utf8, key_pos + key.size());
    if (cursor >= adapter_config_json_utf8.size() || adapter_config_json_utf8[cursor] != ':')
    {
        return {};
    }

    cursor = skip_json_whitespace(adapter_config_json_utf8, cursor + 1U);
    if (cursor >= adapter_config_json_utf8.size() || adapter_config_json_utf8[cursor] != '"')
    {
        return {};
    }

    const std::size_t value_start = cursor + 1U;
    const std::size_t value_end = find_json_string_end(adapter_config_json_utf8, value_start);
    if (value_end == std::string_view::npos)
    {
        return {};
    }

    return std::filesystem::path {
        unescape_json_string(adapter_config_json_utf8.substr(value_start, value_end - value_start))};
}


std::size_t message_type_index(GameMessageType type) noexcept
{
    return static_cast<std::size_t>(type);
}

std::size_t host_message_type_index(Gs1HostMessageType type) noexcept
{
    return static_cast<std::size_t>(type);
}

bool is_valid_host_message_type(Gs1HostMessageType type) noexcept
{
    return host_message_type_index(type) < (static_cast<std::size_t>(GS1_HOST_EVENT_SITE_SCENE_READY) + 1U);
}

bool is_valid_message_type(GameMessageType type) noexcept
{
    return message_type_index(type) < k_game_message_type_count;
}

template <typename EnumType, typename SubscriberArray>
void append_runtime_subscribers(
    SubscriberArray& subscribers_by_type,
    std::span<const EnumType> subscribed_types,
    IRuntimeSystem& system)
{
    for (const EnumType type : subscribed_types)
    {
        const auto index = static_cast<std::size_t>(type);
        if (index < subscribers_by_type.size())
        {
            subscribers_by_type[index].push_back(&system);
        }
    }
}

bool is_valid_profile_system_id(Gs1RuntimeProfileSystemId system_id) noexcept
{
    return static_cast<std::size_t>(system_id) <
        static_cast<std::size_t>(GS1_RUNTIME_PROFILE_SYSTEM_COUNT);
}


double elapsed_milliseconds_since(
    std::chrono::steady_clock::time_point started_at) noexcept
{
    return std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - started_at)
        .count();
}

constexpr auto record_timing_sample = [](auto& accumulator, double elapsed_ms) noexcept
{
    accumulator.sample_count += 1U;
    accumulator.last_elapsed_ms = elapsed_ms;
    accumulator.total_elapsed_ms += elapsed_ms;
    accumulator.max_elapsed_ms = std::max(accumulator.max_elapsed_ms, elapsed_ms);
};

[[nodiscard]] const char* debug_game_message_type_name(GameMessageType type) noexcept
{
    switch (type)
    {
    case GameMessageType::OpenMainMenu:
        return "OpenMainMenuMessage";
    case GameMessageType::StartNewCampaign:
        return "StartNewCampaignMessage";
    case GameMessageType::SelectDeploymentSite:
        return "SelectDeploymentSiteMessage";
    case GameMessageType::ClearDeploymentSiteSelection:
        return "ClearDeploymentSiteSelectionMessage";
    case GameMessageType::DeploymentSiteSelectionChanged:
        return "DeploymentSiteSelectionChangedMessage";
    case GameMessageType::ProgressionEventOccurred:
        return "ProgressionEventOccurredMessage";
    case GameMessageType::PurchaseEntrySelected:
        return "PurchaseEntrySelectedMessage";
    case GameMessageType::TargetGranted:
        return "TargetGrantedMessage";
    case GameMessageType::CampaignCashDeltaRequested:
        return "CampaignCashDeltaRequestedMessage";
    case GameMessageType::CampaignReputationAwardRequested:
        return "CampaignReputationAwardRequestedMessage";
    case GameMessageType::FactionReputationAwardRequested:
        return "FactionReputationAwardRequestedMessage";
    case GameMessageType::TechnologyNodeClaimRequested:
        return "TechnologyNodeClaimRequestedMessage";
    case GameMessageType::TechnologyNodeRefundRequested:
        return "TechnologyNodeRefundRequestedMessage";
    case GameMessageType::StartSiteAttempt:
        return "StartSiteAttemptMessage";
    case GameMessageType::ReturnToRegionalMap:
        return "ReturnToRegionalMapMessage";
    case GameMessageType::SiteAttemptEnded:
        return "SiteAttemptEndedMessage";
    case GameMessageType::PresentLog:
        return "PresentLogMessage";
    case GameMessageType::SiteRunStarted:
        return "SiteRunStartedMessage";
    case GameMessageType::SiteSceneActivated:
        return "SiteSceneActivatedMessage";
    case GameMessageType::StartSiteAction:
        return "StartSiteActionMessage";
    case GameMessageType::CancelSiteAction:
        return "CancelSiteActionMessage";
    case GameMessageType::SiteActionStarted:
        return "SiteActionStartedMessage";
    case GameMessageType::SiteActionCompleted:
        return "SiteActionCompletedMessage";
    case GameMessageType::SiteActionFailed:
        return "SiteActionFailedMessage";
    case GameMessageType::PlacementReservationRequested:
        return "PlacementReservationRequestedMessage";
    case GameMessageType::PlacementReservationAccepted:
        return "PlacementReservationAcceptedMessage";
    case GameMessageType::PlacementReservationRejected:
        return "PlacementReservationRejectedMessage";
    case GameMessageType::PlacementReservationReleased:
        return "PlacementReservationReleasedMessage";
    case GameMessageType::SiteGroundCoverPlaced:
        return "SiteGroundCoverPlacedMessage";
    case GameMessageType::SiteTilePlantingCompleted:
        return "SiteTilePlantingCompletedMessage";
    case GameMessageType::SiteTileWatered:
        return "SiteTileWateredMessage";
    case GameMessageType::SiteTileBurialCleared:
        return "SiteTileBurialClearedMessage";
    case GameMessageType::SiteTileHarvested:
        return "SiteTileHarvestedMessage";
    case GameMessageType::SiteDevicePlaced:
        return "SiteDevicePlacedMessage";
    case GameMessageType::SiteDeviceBroken:
        return "SiteDeviceBrokenMessage";
    case GameMessageType::SiteDeviceRepaired:
        return "SiteDeviceRepairedMessage";
    case GameMessageType::SiteDeviceConditionChanged:
        return "SiteDeviceConditionChangedMessage";
    case GameMessageType::WorkerMeterDeltaRequested:
        return "WorkerMeterDeltaRequestedMessage";
    case GameMessageType::WorkerMetersChanged:
        return "WorkerMetersChangedMessage";
    case GameMessageType::TileEcologyChanged:
        return "TileEcologyChangedMessage";
    case GameMessageType::TileEcologyBatchChanged:
        return "TileEcologyBatchChangedMessage";
    case GameMessageType::LivingPlantStabilityChanged:
        return "LivingPlantStabilityChangedMessage";
    case GameMessageType::SiteTileStateChanged:
        return "SiteTileStateChangedMessage";
    case GameMessageType::RestorationProgressChanged:
        return "RestorationProgressChangedMessage";
    case GameMessageType::SiteRefreshTick:
        return "SiteRefreshTickMessage";
    case GameMessageType::TaskAcceptRequested:
        return "TaskAcceptRequestedMessage";
    case GameMessageType::TaskRewardClaimRequested:
        return "TaskRewardClaimRequestedMessage";
    case GameMessageType::PhoneListingPurchased:
        return "PhoneListingPurchasedMessage";
    case GameMessageType::PhoneListingSold:
        return "PhoneListingSoldMessage";
    case GameMessageType::InventoryTransferCompleted:
        return "InventoryTransferCompletedMessage";
    case GameMessageType::InventoryItemSubmitted:
        return "InventoryItemSubmittedMessage";
    case GameMessageType::InventoryItemUseCompleted:
        return "InventoryItemUseCompletedMessage";
    case GameMessageType::InventoryCraftCompleted:
        return "InventoryCraftCompletedMessage";
    case GameMessageType::EconomyMoneyAwardRequested:
        return "EconomyMoneyAwardRequestedMessage";
    case GameMessageType::SiteUnlockableRevealRequested:
        return "SiteUnlockableRevealRequestedMessage";
    case GameMessageType::RunModifierAwardRequested:
        return "RunModifierAwardRequestedMessage";
    case GameMessageType::SiteModifierEndRequested:
        return "SiteModifierEndRequestedMessage";
    case GameMessageType::PhoneListingPurchaseRequested:
        return "PhoneListingPurchaseRequestedMessage";
    case GameMessageType::PhoneListingSaleRequested:
        return "PhoneListingSaleRequestedMessage";
    case GameMessageType::InventoryDeliveryRequested:
        return "InventoryDeliveryRequestedMessage";
    case GameMessageType::InventoryDeliveryBatchRequested:
        return "InventoryDeliveryBatchRequestedMessage";
    case GameMessageType::InventoryWorkerPackInsertRequested:
        return "InventoryWorkerPackInsertRequestedMessage";
    case GameMessageType::InventoryItemUseRequested:
        return "InventoryItemUseRequestedMessage";
    case GameMessageType::InventoryItemConsumeRequested:
        return "InventoryItemConsumeRequestedMessage";
    case GameMessageType::InventoryGlobalItemConsumeRequested:
        return "InventoryGlobalItemConsumeRequestedMessage";
    case GameMessageType::InventoryTransferRequested:
        return "InventoryTransferRequestedMessage";
    case GameMessageType::InventoryItemSubmitRequested:
        return "InventoryItemSubmitRequestedMessage";
    case GameMessageType::InventorySlotTapped:
        return "InventorySlotTappedMessage";
    case GameMessageType::InventoryCraftContextRequested:
        return "CraftContextRequestedMessage";
    case GameMessageType::PlacementModeCursorMoved:
        return "PlacementModeCursorMovedMessage";
    case GameMessageType::PlacementModeCommitRejected:
        return "PlacementModeCommitRejectedMessage";
    case GameMessageType::InventoryCraftCommitRequested:
        return "InventoryCraftCommitRequestedMessage";
    case GameMessageType::ContractorHireRequested:
        return "ContractorHireRequestedMessage";
    case GameMessageType::SiteUnlockablePurchaseRequested:
        return "SiteUnlockablePurchaseRequestedMessage";
    case GameMessageType::Count:
    default:
        return "UnknownGameMessage";
    }
}

template <typename System>
void append_runtime_game_message_subscribers(
    RuntimeGameMessageSubscriberEntryArray& subscribers_by_type,
    std::span<const GameMessageType> subscribed_types,
    IRuntimeSystem& system)
{
    const auto profile_id = runtime_profile_system_id_for<System>(system);
    for (const GameMessageType type : subscribed_types)
    {
        const auto index = static_cast<std::size_t>(type);
        if (index < subscribers_by_type.size())
        {
            subscribers_by_type[index].push_back(RuntimeGameMessageSubscriberEntry {
                .system = &system,
                .profile_id = profile_id});
        }
    }
}

template <GameMessageType Type>
inline constexpr bool legacy_game_message_type_uses_typed_dispatch_v =
    false;

template <>
inline constexpr bool legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::OpenMainMenu> =
    typed_gameplay_dispatch_traits<OpenMainMenuMessage>::enabled;
template <>
inline constexpr bool legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::StartNewCampaign> =
    typed_gameplay_dispatch_traits<StartNewCampaignMessage>::enabled;
template <>
inline constexpr bool legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::SelectDeploymentSite> =
    typed_gameplay_dispatch_traits<SelectDeploymentSiteMessage>::enabled;
template <>
inline constexpr bool legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::ClearDeploymentSiteSelection> =
    typed_gameplay_dispatch_traits<ClearDeploymentSiteSelectionMessage>::enabled;
template <>
inline constexpr bool legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::DeploymentSiteSelectionChanged> =
    typed_gameplay_dispatch_traits<DeploymentSiteSelectionChangedMessage>::enabled;
template <>
inline constexpr bool legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::ProgressionEventOccurred> =
    typed_gameplay_dispatch_traits<ProgressionEventOccurredMessage>::enabled;
template <>
inline constexpr bool legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::PurchaseEntrySelected> =
    typed_gameplay_dispatch_traits<PurchaseEntrySelectedMessage>::enabled;
template <>
inline constexpr bool legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::TargetGranted> =
    typed_gameplay_dispatch_traits<TargetGrantedMessage>::enabled;
template <>
inline constexpr bool legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::CampaignReputationAwardRequested> =
    typed_gameplay_dispatch_traits<CampaignReputationAwardRequestedMessage>::enabled;
template <>
inline constexpr bool legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::FactionReputationAwardRequested> =
    typed_gameplay_dispatch_traits<FactionReputationAwardRequestedMessage>::enabled;
template <>
inline constexpr bool legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::TechnologyNodeClaimRequested> =
    typed_gameplay_dispatch_traits<TechnologyNodeClaimRequestedMessage>::enabled;
template <>
inline constexpr bool legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::TechnologyNodeRefundRequested> =
    typed_gameplay_dispatch_traits<TechnologyNodeRefundRequestedMessage>::enabled;
template <>
inline constexpr bool legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::StartSiteAttempt> =
    typed_gameplay_dispatch_traits<StartSiteAttemptMessage>::enabled;
template <>
inline constexpr bool legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::ReturnToRegionalMap> =
    typed_gameplay_dispatch_traits<ReturnToRegionalMapMessage>::enabled;
template <>
inline constexpr bool legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::SiteAttemptEnded> =
    typed_gameplay_dispatch_traits<SiteAttemptEndedMessage>::enabled;
template <>
inline constexpr bool legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::SiteRunStarted> =
    typed_gameplay_dispatch_traits<SiteRunStartedMessage>::enabled;
template <>
inline constexpr bool legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::SiteSceneActivated> =
    typed_gameplay_dispatch_traits<SiteSceneActivatedMessage>::enabled;
template <>
inline constexpr bool legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::StartSiteAction> =
    typed_gameplay_dispatch_traits<StartSiteActionMessage>::enabled;
template <>
inline constexpr bool legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::PhoneListingPurchased> =
    typed_gameplay_dispatch_traits<PhoneListingPurchasedMessage>::enabled;
template <>
inline constexpr bool legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::PhoneListingSold> =
    typed_gameplay_dispatch_traits<PhoneListingSoldMessage>::enabled;
template <>
inline constexpr bool legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::InventoryTransferCompleted> =
    typed_gameplay_dispatch_traits<InventoryTransferCompletedMessage>::enabled;
template <>
inline constexpr bool legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::InventoryItemSubmitted> =
    typed_gameplay_dispatch_traits<InventoryItemSubmittedMessage>::enabled;
template <>
inline constexpr bool legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::InventoryItemUseCompleted> =
    typed_gameplay_dispatch_traits<InventoryItemUseCompletedMessage>::enabled;
template <>
inline constexpr bool legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::InventoryCraftCompleted> =
    typed_gameplay_dispatch_traits<InventoryCraftCompletedMessage>::enabled;
template <>
inline constexpr bool legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::EconomyMoneyAwardRequested> =
    typed_gameplay_dispatch_traits<EconomyMoneyAwardRequestedMessage>::enabled;
template <>
inline constexpr bool legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::SiteUnlockableRevealRequested> =
    typed_gameplay_dispatch_traits<SiteUnlockableRevealRequestedMessage>::enabled;
template <>
inline constexpr bool legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::RunModifierAwardRequested> =
    typed_gameplay_dispatch_traits<RunModifierAwardRequestedMessage>::enabled;
template <>
inline constexpr bool legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::SiteRefreshTick> =
    typed_gameplay_dispatch_traits<SiteRefreshTickMessage>::enabled;
template <>
inline constexpr bool legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::InventoryDeliveryBatchRequested> =
    typed_gameplay_dispatch_traits<InventoryDeliveryBatchRequestedMessage>::enabled;
template <>
inline constexpr bool legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::InventoryDeliveryRequested> =
    typed_gameplay_dispatch_traits<InventoryDeliveryRequestedMessage>::enabled;
template <>
inline constexpr bool legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::InventoryWorkerPackInsertRequested> =
    typed_gameplay_dispatch_traits<InventoryWorkerPackInsertRequestedMessage>::enabled;
template <>
inline constexpr bool legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::InventoryItemUseRequested> =
    typed_gameplay_dispatch_traits<InventoryItemUseRequestedMessage>::enabled;
template <>
inline constexpr bool legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::InventoryItemConsumeRequested> =
    typed_gameplay_dispatch_traits<InventoryItemConsumeRequestedMessage>::enabled;
template <>
inline constexpr bool legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::InventoryGlobalItemConsumeRequested> =
    typed_gameplay_dispatch_traits<InventoryGlobalItemConsumeRequestedMessage>::enabled;
template <>
inline constexpr bool legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::InventoryTransferRequested> =
    typed_gameplay_dispatch_traits<InventoryTransferRequestedMessage>::enabled;
template <>
inline constexpr bool legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::InventoryItemSubmitRequested> =
    typed_gameplay_dispatch_traits<InventoryItemSubmitRequestedMessage>::enabled;
template <>
inline constexpr bool legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::InventoryCraftContextRequested> =
    typed_gameplay_dispatch_traits<CraftContextRequestedMessage>::enabled;
template <>
inline constexpr bool legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::SiteGroundCoverPlaced> =
    typed_gameplay_dispatch_traits<SiteGroundCoverPlacedMessage>::enabled;
template <>
inline constexpr bool legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::SiteTilePlantingCompleted> =
    typed_gameplay_dispatch_traits<SiteTilePlantingCompletedMessage>::enabled;
template <>
inline constexpr bool legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::SiteTileWatered> =
    typed_gameplay_dispatch_traits<SiteTileWateredMessage>::enabled;
template <>
inline constexpr bool legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::SiteTileBurialCleared> =
    typed_gameplay_dispatch_traits<SiteTileBurialClearedMessage>::enabled;
template <>
inline constexpr bool legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::SiteTileHarvested> =
    typed_gameplay_dispatch_traits<SiteTileHarvestedMessage>::enabled;
template <>
inline constexpr bool legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::SiteDevicePlaced> =
    typed_gameplay_dispatch_traits<SiteDevicePlacedMessage>::enabled;
template <>
inline constexpr bool legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::SiteDeviceBroken> =
    typed_gameplay_dispatch_traits<SiteDeviceBrokenMessage>::enabled;
template <>
inline constexpr bool legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::SiteDeviceRepaired> =
    typed_gameplay_dispatch_traits<SiteDeviceRepairedMessage>::enabled;
template <>
inline constexpr bool legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::SiteDeviceConditionChanged> =
    typed_gameplay_dispatch_traits<SiteDeviceConditionChangedMessage>::enabled;
template <>
inline constexpr bool legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::WorkerMeterDeltaRequested> =
    typed_gameplay_dispatch_traits<WorkerMeterDeltaRequestedMessage>::enabled;
template <>
inline constexpr bool legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::WorkerMetersChanged> =
    typed_gameplay_dispatch_traits<WorkerMetersChangedMessage>::enabled;
template <>
inline constexpr bool legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::TileEcologyChanged> =
    typed_gameplay_dispatch_traits<TileEcologyChangedMessage>::enabled;
template <>
inline constexpr bool legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::TileEcologyBatchChanged> =
    typed_gameplay_dispatch_traits<TileEcologyBatchChangedMessage>::enabled;
template <>
inline constexpr bool legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::LivingPlantStabilityChanged> =
    typed_gameplay_dispatch_traits<LivingPlantStabilityChangedMessage>::enabled;
template <>
inline constexpr bool legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::SiteTileStateChanged> =
    typed_gameplay_dispatch_traits<SiteTileStateChangedMessage>::enabled;
template <>
inline constexpr bool legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::RestorationProgressChanged> =
    typed_gameplay_dispatch_traits<RestorationProgressChangedMessage>::enabled;
template <>
inline constexpr bool legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::SiteActionCompleted> =
    typed_gameplay_dispatch_traits<SiteActionCompletedMessage>::enabled;
template <>
inline constexpr bool legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::PlacementReservationRequested> =
    typed_gameplay_dispatch_traits<PlacementReservationRequestedMessage>::enabled;
template <>
inline constexpr bool legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::PlacementReservationAccepted> =
    typed_gameplay_dispatch_traits<PlacementReservationAcceptedMessage>::enabled;
template <>
inline constexpr bool legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::PlacementReservationRejected> =
    typed_gameplay_dispatch_traits<PlacementReservationRejectedMessage>::enabled;
template <>
inline constexpr bool legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::PlacementReservationReleased> =
    typed_gameplay_dispatch_traits<PlacementReservationReleasedMessage>::enabled;
template <>
inline constexpr bool legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::InventoryCraftCommitRequested> =
    typed_gameplay_dispatch_traits<InventoryCraftCommitRequestedMessage>::enabled;

template <typename System>
void append_runtime_game_message_subscribers_if_legacy(
    RuntimeGameMessageSubscriberEntryArray& subscribers_by_type,
    std::span<const GameMessageType> subscribed_types,
    IRuntimeSystem& system)
{
    const auto profile_id = runtime_profile_system_id_for<System>(system);
    for (const GameMessageType type : subscribed_types)
    {
        if (type == GameMessageType::Count)
        {
            continue;
        }

        switch (type)
        {
        case GameMessageType::OpenMainMenu:
            if constexpr (legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::OpenMainMenu>) { continue; }
            break;
        case GameMessageType::StartNewCampaign:
            if constexpr (legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::StartNewCampaign>) { continue; }
            break;
        case GameMessageType::SelectDeploymentSite:
            if constexpr (legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::SelectDeploymentSite>) { continue; }
            break;
        case GameMessageType::ClearDeploymentSiteSelection:
            if constexpr (legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::ClearDeploymentSiteSelection>) { continue; }
            break;
        case GameMessageType::DeploymentSiteSelectionChanged:
            if constexpr (legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::DeploymentSiteSelectionChanged>) { continue; }
            break;
        case GameMessageType::ProgressionEventOccurred:
            if constexpr (legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::ProgressionEventOccurred>) { continue; }
            break;
        case GameMessageType::PurchaseEntrySelected:
            if constexpr (legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::PurchaseEntrySelected>) { continue; }
            break;
        case GameMessageType::TargetGranted:
            if constexpr (legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::TargetGranted>) { continue; }
            break;
        case GameMessageType::CampaignReputationAwardRequested:
            if constexpr (legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::CampaignReputationAwardRequested>) { continue; }
            break;
        case GameMessageType::FactionReputationAwardRequested:
            if constexpr (legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::FactionReputationAwardRequested>) { continue; }
            break;
        case GameMessageType::TechnologyNodeClaimRequested:
            if constexpr (legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::TechnologyNodeClaimRequested>) { continue; }
            break;
        case GameMessageType::TechnologyNodeRefundRequested:
            if constexpr (legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::TechnologyNodeRefundRequested>) { continue; }
            break;
        case GameMessageType::StartSiteAttempt:
            if constexpr (legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::StartSiteAttempt>) { continue; }
            break;
        case GameMessageType::ReturnToRegionalMap:
            if constexpr (legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::ReturnToRegionalMap>) { continue; }
            break;
        case GameMessageType::SiteAttemptEnded:
            if constexpr (legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::SiteAttemptEnded>) { continue; }
            break;
        case GameMessageType::SiteRunStarted:
            if constexpr (legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::SiteRunStarted>) { continue; }
            break;
        case GameMessageType::SiteSceneActivated:
            if constexpr (legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::SiteSceneActivated>) { continue; }
            break;
        case GameMessageType::StartSiteAction:
            if constexpr (legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::StartSiteAction>) { continue; }
            break;
        case GameMessageType::PhoneListingPurchased:
            if constexpr (legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::PhoneListingPurchased>) { continue; }
            break;
        case GameMessageType::PhoneListingSold:
            if constexpr (legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::PhoneListingSold>) { continue; }
            break;
        case GameMessageType::InventoryTransferCompleted:
            if constexpr (legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::InventoryTransferCompleted>) { continue; }
            break;
        case GameMessageType::InventoryItemSubmitted:
            if constexpr (legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::InventoryItemSubmitted>) { continue; }
            break;
        case GameMessageType::InventoryItemUseCompleted:
            if constexpr (legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::InventoryItemUseCompleted>) { continue; }
            break;
        case GameMessageType::InventoryCraftCompleted:
            if constexpr (legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::InventoryCraftCompleted>) { continue; }
            break;
        case GameMessageType::EconomyMoneyAwardRequested:
            if constexpr (legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::EconomyMoneyAwardRequested>) { continue; }
            break;
        case GameMessageType::SiteUnlockableRevealRequested:
            if constexpr (legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::SiteUnlockableRevealRequested>) { continue; }
            break;
        case GameMessageType::RunModifierAwardRequested:
            if constexpr (legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::RunModifierAwardRequested>) { continue; }
            break;
        case GameMessageType::SiteRefreshTick:
            if constexpr (legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::SiteRefreshTick>) { continue; }
            break;
        case GameMessageType::InventoryDeliveryBatchRequested:
            if constexpr (legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::InventoryDeliveryBatchRequested>) { continue; }
            break;
        case GameMessageType::InventoryDeliveryRequested:
            if constexpr (legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::InventoryDeliveryRequested>) { continue; }
            break;
        case GameMessageType::InventoryWorkerPackInsertRequested:
            if constexpr (legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::InventoryWorkerPackInsertRequested>) { continue; }
            break;
        case GameMessageType::InventoryItemUseRequested:
            if constexpr (legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::InventoryItemUseRequested>) { continue; }
            break;
        case GameMessageType::InventoryItemConsumeRequested:
            if constexpr (legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::InventoryItemConsumeRequested>) { continue; }
            break;
        case GameMessageType::InventoryGlobalItemConsumeRequested:
            if constexpr (legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::InventoryGlobalItemConsumeRequested>) { continue; }
            break;
        case GameMessageType::InventoryTransferRequested:
            if constexpr (legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::InventoryTransferRequested>) { continue; }
            break;
        case GameMessageType::InventoryItemSubmitRequested:
            if constexpr (legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::InventoryItemSubmitRequested>) { continue; }
            break;
        case GameMessageType::InventoryCraftContextRequested:
            if constexpr (legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::InventoryCraftContextRequested>) { continue; }
            break;
        case GameMessageType::SiteGroundCoverPlaced:
            if constexpr (legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::SiteGroundCoverPlaced>) { continue; }
            break;
        case GameMessageType::SiteTilePlantingCompleted:
            if constexpr (legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::SiteTilePlantingCompleted>) { continue; }
            break;
        case GameMessageType::SiteTileWatered:
            if constexpr (legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::SiteTileWatered>) { continue; }
            break;
        case GameMessageType::SiteTileBurialCleared:
            if constexpr (legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::SiteTileBurialCleared>) { continue; }
            break;
        case GameMessageType::SiteTileHarvested:
            if constexpr (legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::SiteTileHarvested>) { continue; }
            break;
        case GameMessageType::SiteDevicePlaced:
            if constexpr (legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::SiteDevicePlaced>) { continue; }
            break;
        case GameMessageType::SiteDeviceBroken:
            if constexpr (legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::SiteDeviceBroken>) { continue; }
            break;
        case GameMessageType::SiteDeviceRepaired:
            if constexpr (legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::SiteDeviceRepaired>) { continue; }
            break;
        case GameMessageType::SiteDeviceConditionChanged:
            if constexpr (legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::SiteDeviceConditionChanged>) { continue; }
            break;
        case GameMessageType::WorkerMeterDeltaRequested:
            if constexpr (legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::WorkerMeterDeltaRequested>) { continue; }
            break;
        case GameMessageType::WorkerMetersChanged:
            if constexpr (legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::WorkerMetersChanged>) { continue; }
            break;
        case GameMessageType::TileEcologyChanged:
            if constexpr (legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::TileEcologyChanged>) { continue; }
            break;
        case GameMessageType::TileEcologyBatchChanged:
            if constexpr (legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::TileEcologyBatchChanged>) { continue; }
            break;
        case GameMessageType::LivingPlantStabilityChanged:
            if constexpr (legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::LivingPlantStabilityChanged>) { continue; }
            break;
        case GameMessageType::SiteTileStateChanged:
            if constexpr (legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::SiteTileStateChanged>) { continue; }
            break;
        case GameMessageType::RestorationProgressChanged:
            if constexpr (legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::RestorationProgressChanged>) { continue; }
            break;
        case GameMessageType::SiteActionCompleted:
            if constexpr (legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::SiteActionCompleted>) { continue; }
            break;
        case GameMessageType::PlacementReservationRequested:
            if constexpr (legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::PlacementReservationRequested>) { continue; }
            break;
        case GameMessageType::PlacementReservationAccepted:
            if constexpr (legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::PlacementReservationAccepted>) { continue; }
            break;
        case GameMessageType::PlacementReservationRejected:
            if constexpr (legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::PlacementReservationRejected>) { continue; }
            break;
        case GameMessageType::PlacementReservationReleased:
            if constexpr (legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::PlacementReservationReleased>) { continue; }
            break;
        case GameMessageType::InventoryCraftCommitRequested:
            if constexpr (legacy_game_message_type_uses_typed_dispatch_v<GameMessageType::InventoryCraftCommitRequested>) { continue; }
            break;
        default:
            break;
        }

        const auto index = static_cast<std::size_t>(type);
        if (index < subscribers_by_type.size())
        {
            subscribers_by_type[index].push_back(RuntimeGameMessageSubscriberEntry {
                .system = &system,
                .profile_id = profile_id});
        }
    }
}

}  // namespace

RuntimeInvocation::RuntimeInvocation(GameRuntime& runtime) noexcept
    : runtime_(&runtime)
    , owned_state_(&runtime.state())
    , state_manager_(&runtime.state_manager_)
    , app_state_(&runtime.state().app_state.get())
    , fixed_step_seconds_(&runtime.state().fixed_step_seconds.get())
    , site_world_(runtime.site_world_)
    , runtime_messages_(&runtime.state().runtime_messages)
    , game_messages_(&runtime.state().message_queue)
{
}

RuntimeInvocation::RuntimeInvocation(GameState& state) noexcept
    : owned_state_(&state)
    , app_state_(&state.app_state.get())
    , fixed_step_seconds_(&state.fixed_step_seconds.get())
    , runtime_messages_(&state.runtime_messages)
    , game_messages_(&state.message_queue)
{
}

RuntimeInvocation::RuntimeInvocation(
    GameState& state,
    StateManager& state_manager,
    SiteWorldHandle site_world) noexcept
    : owned_state_(&state)
    , state_manager_(&state_manager)
    , app_state_(&state.app_state.get())
    , fixed_step_seconds_(&state.fixed_step_seconds.get())
    , site_world_(std::move(site_world))
    , runtime_messages_(&state.runtime_messages)
    , game_messages_(&state.message_queue)
{
}

RuntimeInvocation::RuntimeInvocation(
    GameState& state,
    StateManager& state_manager,
    float move_direction_x,
    float move_direction_y,
    float move_direction_z,
    bool move_direction_present,
    SiteWorldHandle site_world) noexcept
    : owned_state_(&state)
    , state_manager_(&state_manager)
    , app_state_(&state.app_state.get())
    , fixed_step_seconds_(&state.fixed_step_seconds.get())
    , site_world_(std::move(site_world))
    , runtime_messages_(&state.runtime_messages)
    , game_messages_(&state.message_queue)
{
    move_direction_ = RuntimeMoveDirectionSnapshot {
        move_direction_x,
        move_direction_y,
        move_direction_z,
        move_direction_present};
    state_manager_->state<StateSetId::MoveDirection>(state) = move_direction_;
}

void RuntimeInvocation::push_game_message(const GameMessage& message)
{
    if (runtime_ != nullptr)
    {
        const auto status = runtime_->dispatch_game_message_inline(message);
        assert(status == GS1_STATUS_OK);
        (void)status;
        return;
    }

    game_messages_->push_back(message);
}

void RuntimeInvocation::install_campaign_state(const CampaignState& campaign)
{
    if (runtime_ != nullptr)
    {
        runtime_->install_campaign_state(campaign);
        return;
    }

    RuntimePrivilegedStateMutation privileged {*state_manager_};
    write_campaign_state_to_state_sets(campaign, *owned_state_, *state_manager_);
}

void RuntimeInvocation::install_site_run_state(const SiteRunState& site_run)
{
    if (runtime_ != nullptr)
    {
        runtime_->install_site_run_state(site_run);
        site_world_ = runtime_->site_world_;
        return;
    }

    RuntimePrivilegedStateMutation privileged {*state_manager_};
    write_site_run_state_to_state_sets(site_run, *owned_state_, *state_manager_);
    site_world_ = site_run.site_world;
}

void RuntimeInvocation::clear_site_run_state()
{
    if (runtime_ != nullptr)
    {
        runtime_->clear_site_run_state();
        site_world_ = runtime_->site_world_;
        return;
    }

    RuntimePrivilegedStateMutation privileged {*state_manager_};
    write_site_run_state_to_state_sets(std::optional<SiteRunState> {}, *owned_state_, *state_manager_);
    site_world_ = nullptr;
}

void RuntimeInvocation::push_runtime_message(const Gs1RuntimeMessage& message)
{
    runtime_messages_->push_back(message);
}

void RuntimeInvocation::push_log_message(Gs1LogLevel level, const char* text)
{
    if (text == nullptr)
    {
        return;
    }

    Gs1RuntimeMessage message {};
    message.type = GS1_ENGINE_MESSAGE_LOG_TEXT;
    auto& payload = message.emplace_payload<Gs1EngineMessageLogTextData>();
    payload.level = level;
    std::memset(payload.text, 0, sizeof(payload.text));
    const auto copy_length = std::min<std::size_t>(
        std::strlen(text),
        sizeof(payload.text) - 1U);
    std::memcpy(payload.text, text, copy_length);
    push_runtime_message(message);
}

GameRuntime::GameRuntime(Gs1RuntimeCreateDesc create_desc)
    : create_desc_(create_desc)
    , state_(state_manager_.game_state())
{
    if (create_desc_.fixed_step_seconds > 0.0)
    {
        state().fixed_step_seconds = create_desc_.fixed_step_seconds;
    }

    if (create_desc_.adapter_config_json_utf8 != nullptr)
    {
        adapter_config_json_utf8_ = create_desc_.adapter_config_json_utf8;
    }

    std::filesystem::path resolved_project_config_root {};
    if (!adapter_config_json_utf8_.empty())
    {
        resolved_project_config_root = extract_project_config_root_from_adapter_config_json(adapter_config_json_utf8_);
    }

    if (create_desc_.project_config_root_utf8 != nullptr &&
        create_desc_.project_config_root_utf8[0] != '\0' &&
        resolved_project_config_root.empty())
    {
        resolved_project_config_root = std::filesystem::path {create_desc_.project_config_root_utf8};
    }

    if (!resolved_project_config_root.empty())
    {
        set_prototype_content_root(resolved_project_config_root / "content");
    }

    initialize_system_registry();
}

Gs1Status GameRuntime::get_profiling_snapshot(Gs1RuntimeProfilingSnapshot& out_snapshot) const noexcept
{
    if (out_snapshot.struct_size != sizeof(Gs1RuntimeProfilingSnapshot))
    {
        return GS1_STATUS_INVALID_ARGUMENT;
    }

    out_snapshot.system_count = static_cast<std::uint32_t>(GS1_RUNTIME_PROFILE_SYSTEM_COUNT);
    copy_timing_snapshot(phase1_timing_, out_snapshot.phase1_timing);
    copy_timing_snapshot(phase2_timing_, out_snapshot.phase2_timing);
    copy_timing_snapshot(fixed_step_timing_, out_snapshot.fixed_step_timing);

    for (std::size_t index = 0; index < profiled_systems_.size(); ++index)
    {
        auto& destination = out_snapshot.systems[index];
        const auto& source = profiled_systems_[index];
        destination.system_id = static_cast<Gs1RuntimeProfileSystemId>(index);
        destination.enabled = source.enabled ? 1U : 0U;
        destination.reserved0 = 0U;
        destination.reserved1 = 0U;
        copy_timing_snapshot(source.run_timing, destination.run_timing);
        copy_timing_snapshot(source.message_timing, destination.message_timing);
    }

    return GS1_STATUS_OK;
}

void GameRuntime::reset_profiling() noexcept
{
    phase1_timing_ = {};
    phase2_timing_ = {};
    fixed_step_timing_ = {};

    for (auto& system_state : profiled_systems_)
    {
        system_state.run_timing = {};
        system_state.message_timing = {};
    }
}

Gs1Status GameRuntime::set_profiled_system_enabled(
    Gs1RuntimeProfileSystemId system_id,
    bool enabled) noexcept
{
    if (!is_valid_profile_system_id(system_id))
    {
        return GS1_STATUS_INVALID_ARGUMENT;
    }

    profiled_systems_[static_cast<std::size_t>(system_id)].enabled = enabled;
    return GS1_STATUS_OK;
}

bool GameRuntime::profiled_system_enabled(Gs1RuntimeProfileSystemId system_id) const noexcept
{
    return is_valid_profile_system_id(system_id) &&
        profiled_systems_[static_cast<std::size_t>(system_id)].enabled;
}

void GameRuntime::copy_timing_snapshot(
    const TimingAccumulator& source,
    Gs1RuntimeTimingStats& destination) const noexcept
{
    destination.sample_count = source.sample_count;
    destination.last_elapsed_ms = source.last_elapsed_ms;
    destination.total_elapsed_ms = source.total_elapsed_ms;
    destination.max_elapsed_ms = source.max_elapsed_ms;
}

void GameRuntime::install_campaign_state(const CampaignState& campaign)
{
    RuntimePrivilegedStateMutation privileged {state_manager_};
    write_campaign_state_to_state_sets(campaign, state(), state_manager_);
}

void GameRuntime::install_site_run_state(const SiteRunState& site_run)
{
    RuntimePrivilegedStateMutation privileged {state_manager_};
    write_site_run_state_to_state_sets(site_run, state(), state_manager_);
    site_world_ = site_run.site_world;
}

void GameRuntime::clear_site_run_state()
{
    RuntimePrivilegedStateMutation privileged {state_manager_};
    write_site_run_state_to_state_sets(std::optional<SiteRunState> {}, state(), state_manager_);
    site_world_ = nullptr;
}

void GameRuntime::initialize_system_registry()
{
#ifndef NDEBUG
    const std::array site_system_access_registry {
        ActionExecutionSystem::access(),
        WeatherEventSystem::access(),
        WorkerConditionSystem::access(),
        EcologySystem::access(),
        PlantWeatherContributionSystem::access(),
        DeviceWeatherContributionSystem::access(),
        TaskBoardSystem::access(),
        PlacementValidationSystem::access(),
        LocalWeatherResolveSystem::access(),
        InventorySystem::access(),
        CraftSystem::access(),
        EconomyPhoneSystem::access(),
        CampDurabilitySystem::access(),
        DeviceSupportSystem::access(),
        DeviceMaintenanceSystem::access(),
        ModifierSystem::access(),
        FailureRecoverySystem::access(),
        SiteCompletionSystem::access(),
        SiteTimeSystem::access(),
        SiteFlowSystem::access()};
    const auto validation = validate_site_system_access_registry(site_system_access_registry);
    if (!validation.ok)
    {
        std::fprintf(
            stderr,
            "Invalid site system access registry: %.*s system=%.*s component=%.*s other=%.*s\n",
            static_cast<int>(validation.message.size()),
            validation.message.data(),
            static_cast<int>(validation.system_name.size()),
            validation.system_name.data(),
            static_cast<int>(site_component_name(validation.component).size()),
            site_component_name(validation.component).data(),
            static_cast<int>(validation.other_system_name.size()),
            validation.other_system_name.data());
        assert(false && "Invalid site system access registry");
    }
#endif

    systems_.clear();
    fixed_step_systems_.clear();
    systems_by_pack_index_.fill(nullptr);
    for (auto& subscribers : host_message_subscribers_)
    {
        subscribers.clear();
    }
    for (auto& subscribers : message_subscribers_)
    {
        subscribers.clear();
    }

    const auto append_system = [this]<typename System>()
    {
        auto system = std::make_unique<System>();
        constexpr std::size_t system_index = system_pack_index_v<System, GameSystems>;
        static_assert(system_index < GameSystems::size, "Registered runtime system must exist in GameSystems.");
        systems_by_pack_index_[system_index] = system.get();
        const auto fixed_step_order = runtime_fixed_step_order_for<System>(*system);
        if (fixed_step_order.has_value())
        {
            fixed_step_systems_.push_back(GameRuntime::FixedStepSystemEntry {
                .system = system.get(),
                .profile_id = runtime_profile_system_id_for<System>(*system),
                .order = *fixed_step_order});
        }
        systems_.push_back(std::move(system));
    };

    append_system.template operator()<CampaignFlowSystem>();
    append_system.template operator()<LoadoutPlannerSystem>();
    append_system.template operator()<CampaignProgressionSystem>();
    append_system.template operator()<TechnologySystem>();
    append_system.template operator()<ActionExecutionSystem>();
    append_system.template operator()<WeatherEventSystem>();
    append_system.template operator()<WorkerConditionSystem>();
    append_system.template operator()<EcologySystem>();
    append_system.template operator()<PlantWeatherContributionSystem>();
    append_system.template operator()<DeviceWeatherContributionSystem>();
    append_system.template operator()<TaskBoardSystem>();
    append_system.template operator()<PlacementValidationSystem>();
    append_system.template operator()<LocalWeatherResolveSystem>();
    append_system.template operator()<DeviceMaintenanceSystem>();
    append_system.template operator()<InventorySystem>();
    append_system.template operator()<CraftSystem>();
    append_system.template operator()<EconomyPhoneSystem>();
    append_system.template operator()<CampDurabilitySystem>();
    append_system.template operator()<DeviceSupportSystem>();
    append_system.template operator()<ModifierSystem>();
    append_system.template operator()<CampaignTimeSystem>();
    append_system.template operator()<SiteTimeSystem>();
    append_system.template operator()<SiteFlowSystem>();
    append_system.template operator()<FailureRecoverySystem>();
    append_system.template operator()<SiteCompletionSystem>();
    for (const auto& system : systems_)
    {
        state_manager_.register_resolver(*system);
        append_runtime_subscribers(host_message_subscribers_, system->subscribed_host_messages(), *system);
    }

    const auto register_message_subscribers = [this]<typename System>()
    {
        auto* system = find_system<System>();
        if (system == nullptr)
        {
            return;
        }

        append_runtime_game_message_subscribers_if_legacy<System>(
            message_subscribers_,
            system->subscribed_game_messages(),
            *system);
    };

    register_message_subscribers.template operator()<CampaignFlowSystem>();
    register_message_subscribers.template operator()<LoadoutPlannerSystem>();
    register_message_subscribers.template operator()<CampaignProgressionSystem>();
    register_message_subscribers.template operator()<TechnologySystem>();
    register_message_subscribers.template operator()<ActionExecutionSystem>();
    register_message_subscribers.template operator()<WeatherEventSystem>();
    register_message_subscribers.template operator()<WorkerConditionSystem>();
    register_message_subscribers.template operator()<EcologySystem>();
    register_message_subscribers.template operator()<PlantWeatherContributionSystem>();
    register_message_subscribers.template operator()<DeviceWeatherContributionSystem>();
    register_message_subscribers.template operator()<TaskBoardSystem>();
    register_message_subscribers.template operator()<PlacementValidationSystem>();
    register_message_subscribers.template operator()<LocalWeatherResolveSystem>();
    register_message_subscribers.template operator()<DeviceMaintenanceSystem>();
    register_message_subscribers.template operator()<InventorySystem>();
    register_message_subscribers.template operator()<CraftSystem>();
    register_message_subscribers.template operator()<EconomyPhoneSystem>();
    register_message_subscribers.template operator()<CampDurabilitySystem>();
    register_message_subscribers.template operator()<DeviceSupportSystem>();
    register_message_subscribers.template operator()<ModifierSystem>();
    register_message_subscribers.template operator()<CampaignTimeSystem>();
    register_message_subscribers.template operator()<SiteTimeSystem>();
    register_message_subscribers.template operator()<SiteFlowSystem>();
    register_message_subscribers.template operator()<FailureRecoverySystem>();
    register_message_subscribers.template operator()<SiteCompletionSystem>();

    std::sort(
        fixed_step_systems_.begin(),
        fixed_step_systems_.end(),
        [](const FixedStepSystemEntry& lhs, const FixedStepSystemEntry& rhs)
        {
            return lhs.order < rhs.order;
        });
}

Gs1Status GameRuntime::submit_host_messages(
    const Gs1HostMessage* messages,
    std::uint32_t message_count)
{
    if (message_count > 0U && messages == nullptr)
    {
        return GS1_STATUS_INVALID_ARGUMENT;
    }

    for (std::uint32_t i = 0; i < message_count; ++i)
    {
        host_messages_.push_back(messages[i]);
    }

    return GS1_STATUS_OK;
}

Gs1Status GameRuntime::run_phase1(const Gs1Phase1Request& request, Gs1Phase1Result& out_result)
{
    if (request.struct_size != sizeof(Gs1Phase1Request))
    {
        return GS1_STATUS_INVALID_ARGUMENT;
    }

    const auto phase_started_at = std::chrono::steady_clock::now();
    const auto finish_phase = [this, phase_started_at]() noexcept
    {
        record_timing_sample(phase1_timing_, elapsed_milliseconds_since(phase_started_at));
    };

    out_result = {};
    out_result.struct_size = sizeof(Gs1Phase1Result);
    auto status = GS1_STATUS_OK;
    if (!boot_initialized_)
    {
        if (!state().campaign_core.has_value() && !state().site_run_meta.has_value())
        {
            status = dispatch_game_message_inline(OpenMainMenuMessage {});
            if (status != GS1_STATUS_OK)
            {
                finish_phase();
                return status;
            }
        }

        boot_initialized_ = true;
    }

    status = dispatch_host_messages(out_result.processed_host_message_count);
    if (status != GS1_STATUS_OK)
    {
        finish_phase();
        return status;
    }

    if (!state().site_run_meta.has_value())
    {
        out_result.runtime_messages_queued = static_cast<std::uint32_t>(state().runtime_messages.size());
        finish_phase();
        return GS1_STATUS_OK;
    }

    if (state().app_state.get() == GS1_APP_STATE_SITE_LOADING)
    {
        out_result.runtime_messages_queued = static_cast<std::uint32_t>(state().runtime_messages.size());
        finish_phase();
        return GS1_STATUS_OK;
    }

    if (!state().site_clock.has_value())
    {
        out_result.runtime_messages_queued = static_cast<std::uint32_t>(state().runtime_messages.size());
        finish_phase();
        return GS1_STATUS_OK;
    }

    state().site_clock->accumulator_real_seconds += request.real_delta_seconds;

    while (state().site_clock->accumulator_real_seconds >= state().fixed_step_seconds)
    {
        state().site_clock->accumulator_real_seconds -= state().fixed_step_seconds;
        run_fixed_step();
        out_result.fixed_steps_executed += 1U;
    }

    status = dispatch_queued_messages();
    state().move_direction = RuntimeMoveDirectionSnapshot {};
    out_result.runtime_messages_queued = static_cast<std::uint32_t>(state().runtime_messages.size());
    finish_phase();
    return status;
}

Gs1Status GameRuntime::run_phase2(const Gs1Phase2Request& request, Gs1Phase2Result& out_result)
{
    if (request.struct_size != sizeof(Gs1Phase2Request))
    {
        return GS1_STATUS_INVALID_ARGUMENT;
    }

    const auto phase_started_at = std::chrono::steady_clock::now();
    const auto finish_phase = [this, phase_started_at]() noexcept
    {
        record_timing_sample(phase2_timing_, elapsed_milliseconds_since(phase_started_at));
    };

    out_result = {};
    out_result.struct_size = sizeof(Gs1Phase2Result);
    auto status = dispatch_host_messages(out_result.processed_host_message_count);
    if (status != GS1_STATUS_OK)
    {
        finish_phase();
        return status;
    }

    status = dispatch_queued_messages();
    if (status != GS1_STATUS_OK)
    {
        finish_phase();
        return status;
    }

    out_result.runtime_messages_queued = static_cast<std::uint32_t>(state().runtime_messages.size());
    finish_phase();
    return status;
}

Gs1Status GameRuntime::pop_runtime_message(Gs1RuntimeMessage& out_message)
{
    if (state().runtime_messages.empty())
    {
        return GS1_STATUS_BUFFER_EMPTY;
    }

    out_message = state().runtime_messages.front();
    state().runtime_messages.pop_front();
    return GS1_STATUS_OK;
}

Gs1Status GameRuntime::get_game_state_view(Gs1GameStateView& out_view)
{
    if (out_view.struct_size != sizeof(Gs1GameStateView))
    {
        return GS1_STATUS_INVALID_ARGUMENT;
    }

    rebuild_game_state_view_cache(*this, state_view_cache_);
    out_view = state_view_cache_.root;
    return GS1_STATUS_OK;
}

Gs1Status GameRuntime::query_site_tile_view(std::uint32_t tile_index, Gs1SiteTileView& out_tile) const
{
    if (!state().site_run_meta.has_value())
    {
        return GS1_STATUS_INVALID_STATE;
    }

    return build_site_tile_view(site_world(), tile_index, out_tile)
        ? GS1_STATUS_OK
        : GS1_STATUS_INVALID_ARGUMENT;
}

Gs1Status GameRuntime::handle_message(const GameMessage& message)
{
    return dispatch_game_message_inline(message);
}

#ifndef NDEBUG
void GameRuntime::push_debug_semantic_game_message(std::string_view message_name)
{
    debug_semantic_game_message_stack_.push_back(message_name);
    if (debug_semantic_game_message_stack_.size() >= debug_last_semantic_game_message_stack_.size())
    {
        debug_last_semantic_game_message_stack_ = debug_semantic_game_message_stack_;
    }
}

void GameRuntime::pop_debug_semantic_game_message() noexcept
{
    if (!debug_semantic_game_message_stack_.empty())
    {
        debug_semantic_game_message_stack_.pop_back();
    }
}

void GameRuntime::print_debug_semantic_game_message_stack() const
{
    if (debug_semantic_game_message_stack_.empty())
    {
        std::fprintf(stderr, "Semantic gameplay message stack: <empty>\n");
        return;
    }

    std::fprintf(stderr, "Semantic gameplay message stack:");
    for (std::size_t index = 0; index < debug_semantic_game_message_stack_.size(); ++index)
    {
        std::fprintf(
            stderr,
            "%s%.*s",
            index == 0U ? " " : " -> ",
            static_cast<int>(debug_semantic_game_message_stack_[index].size()),
            debug_semantic_game_message_stack_[index].data());
    }
    std::fprintf(stderr, "\n");
}
#endif

Gs1Status GameRuntime::dispatch_queued_messages()
{
    while (!state().message_queue.empty())
    {
        const auto message = state().message_queue.front();
        state().message_queue.pop_front();

        const auto status = dispatch_subscribed_message(message);
        if (status != GS1_STATUS_OK)
        {
            return status;
        }

    }

    return GS1_STATUS_OK;
}

Gs1Status GameRuntime::dispatch_game_message_inline(const GameMessage& message)
{
    RuntimeInlineGameMessageScope depth_scope {*this};
#ifndef NDEBUG
    RuntimeSemanticGameMessageScope semantic_scope {*this, debug_game_message_type_name(message.type)};
#endif

    if (inline_game_message_depth_ == k_inline_game_message_warn_depth)
    {
        std::fprintf(
            stderr,
            "Warning: internal GameMessage inline dispatch depth reached %u for type=%u.\n",
            inline_game_message_depth_,
            static_cast<unsigned>(message.type));
#ifndef NDEBUG
        print_debug_semantic_game_message_stack();
#endif
    }

    assert(
        inline_game_message_depth_ <= k_inline_game_message_warn_depth &&
        "Internal GameMessage inline dispatch depth exceeded limit 4.");

    return dispatch_subscribed_message(message);
}

Gs1Status GameRuntime::dispatch_subscribed_message(const GameMessage& message)
{
    if (!is_valid_message_type(message.type) || message.type == GameMessageType::Count)
    {
        return GS1_STATUS_INVALID_ARGUMENT;
    }

    switch (message.type)
    {
    case GameMessageType::OpenMainMenu:
        return dispatch_typed_game_message_to_subscribers(message.payload_as<OpenMainMenuMessage>());
    case GameMessageType::StartNewCampaign:
        return dispatch_typed_game_message_to_subscribers(message.payload_as<StartNewCampaignMessage>());
    case GameMessageType::SelectDeploymentSite:
        return dispatch_typed_game_message_to_subscribers(message.payload_as<SelectDeploymentSiteMessage>());
    case GameMessageType::ClearDeploymentSiteSelection:
        return dispatch_typed_game_message_to_subscribers(message.payload_as<ClearDeploymentSiteSelectionMessage>());
    case GameMessageType::DeploymentSiteSelectionChanged:
        return dispatch_typed_game_message_to_subscribers(message.payload_as<DeploymentSiteSelectionChangedMessage>());
    case GameMessageType::ProgressionEventOccurred:
        return dispatch_typed_game_message_to_subscribers(message.payload_as<ProgressionEventOccurredMessage>());
    case GameMessageType::PurchaseEntrySelected:
        return dispatch_typed_game_message_to_subscribers(message.payload_as<PurchaseEntrySelectedMessage>());
    case GameMessageType::TargetGranted:
        return dispatch_typed_game_message_to_subscribers(message.payload_as<TargetGrantedMessage>());
    case GameMessageType::CampaignReputationAwardRequested:
        return dispatch_typed_game_message_to_subscribers(
            message.payload_as<CampaignReputationAwardRequestedMessage>());
    case GameMessageType::FactionReputationAwardRequested:
        return dispatch_typed_game_message_to_subscribers(
            message.payload_as<FactionReputationAwardRequestedMessage>());
    case GameMessageType::TechnologyNodeClaimRequested:
        return dispatch_typed_game_message_to_subscribers(
            message.payload_as<TechnologyNodeClaimRequestedMessage>());
    case GameMessageType::TechnologyNodeRefundRequested:
        return dispatch_typed_game_message_to_subscribers(
            message.payload_as<TechnologyNodeRefundRequestedMessage>());
    case GameMessageType::StartSiteAttempt:
        return dispatch_typed_game_message_to_subscribers(message.payload_as<StartSiteAttemptMessage>());
    case GameMessageType::ReturnToRegionalMap:
        return dispatch_typed_game_message_to_subscribers(message.payload_as<ReturnToRegionalMapMessage>());
    case GameMessageType::SiteAttemptEnded:
        return dispatch_typed_game_message_to_subscribers(message.payload_as<SiteAttemptEndedMessage>());
    case GameMessageType::SiteRunStarted:
        return dispatch_typed_game_message_to_subscribers(message.payload_as<SiteRunStartedMessage>());
    case GameMessageType::SiteSceneActivated:
        return dispatch_typed_game_message_to_subscribers(message.payload_as<SiteSceneActivatedMessage>());
    case GameMessageType::StartSiteAction:
        return dispatch_typed_game_message_to_subscribers(message.payload_as<StartSiteActionMessage>());
    case GameMessageType::PhoneListingPurchased:
        return dispatch_typed_game_message_to_subscribers(message.payload_as<PhoneListingPurchasedMessage>());
    case GameMessageType::PhoneListingSold:
        return dispatch_typed_game_message_to_subscribers(message.payload_as<PhoneListingSoldMessage>());
    case GameMessageType::InventoryTransferCompleted:
        return dispatch_typed_game_message_to_subscribers(message.payload_as<InventoryTransferCompletedMessage>());
    case GameMessageType::InventoryItemSubmitted:
        return dispatch_typed_game_message_to_subscribers(message.payload_as<InventoryItemSubmittedMessage>());
    case GameMessageType::InventoryItemUseCompleted:
        return dispatch_typed_game_message_to_subscribers(message.payload_as<InventoryItemUseCompletedMessage>());
    case GameMessageType::InventoryCraftCompleted:
        return dispatch_typed_game_message_to_subscribers(message.payload_as<InventoryCraftCompletedMessage>());
    case GameMessageType::EconomyMoneyAwardRequested:
        return dispatch_typed_game_message_to_subscribers(message.payload_as<EconomyMoneyAwardRequestedMessage>());
    case GameMessageType::SiteUnlockableRevealRequested:
        return dispatch_typed_game_message_to_subscribers(message.payload_as<SiteUnlockableRevealRequestedMessage>());
    case GameMessageType::RunModifierAwardRequested:
        return dispatch_typed_game_message_to_subscribers(message.payload_as<RunModifierAwardRequestedMessage>());
    case GameMessageType::SiteRefreshTick:
        return dispatch_typed_game_message_to_subscribers(message.payload_as<SiteRefreshTickMessage>());
    case GameMessageType::InventoryDeliveryBatchRequested:
        return dispatch_typed_game_message_to_subscribers(
            message.payload_as<InventoryDeliveryBatchRequestedMessage>());
    case GameMessageType::InventoryDeliveryRequested:
        return dispatch_typed_game_message_to_subscribers(message.payload_as<InventoryDeliveryRequestedMessage>());
    case GameMessageType::InventoryWorkerPackInsertRequested:
        return dispatch_typed_game_message_to_subscribers(
            message.payload_as<InventoryWorkerPackInsertRequestedMessage>());
    case GameMessageType::InventoryItemUseRequested:
        return dispatch_typed_game_message_to_subscribers(message.payload_as<InventoryItemUseRequestedMessage>());
    case GameMessageType::InventoryItemConsumeRequested:
        return dispatch_typed_game_message_to_subscribers(
            message.payload_as<InventoryItemConsumeRequestedMessage>());
    case GameMessageType::InventoryGlobalItemConsumeRequested:
        return dispatch_typed_game_message_to_subscribers(
            message.payload_as<InventoryGlobalItemConsumeRequestedMessage>());
    case GameMessageType::InventoryTransferRequested:
        return dispatch_typed_game_message_to_subscribers(message.payload_as<InventoryTransferRequestedMessage>());
    case GameMessageType::InventoryItemSubmitRequested:
        return dispatch_typed_game_message_to_subscribers(message.payload_as<InventoryItemSubmitRequestedMessage>());
    case GameMessageType::InventoryCraftContextRequested:
        return dispatch_typed_game_message_to_subscribers(message.payload_as<CraftContextRequestedMessage>());
    case GameMessageType::SiteGroundCoverPlaced:
        return dispatch_typed_game_message_to_subscribers(message.payload_as<SiteGroundCoverPlacedMessage>());
    case GameMessageType::SiteTilePlantingCompleted:
        return dispatch_typed_game_message_to_subscribers(
            message.payload_as<SiteTilePlantingCompletedMessage>());
    case GameMessageType::SiteTileWatered:
        return dispatch_typed_game_message_to_subscribers(message.payload_as<SiteTileWateredMessage>());
    case GameMessageType::SiteTileBurialCleared:
        return dispatch_typed_game_message_to_subscribers(message.payload_as<SiteTileBurialClearedMessage>());
    case GameMessageType::SiteTileHarvested:
        return dispatch_typed_game_message_to_subscribers(message.payload_as<SiteTileHarvestedMessage>());
    case GameMessageType::SiteDevicePlaced:
        return dispatch_typed_game_message_to_subscribers(message.payload_as<SiteDevicePlacedMessage>());
    case GameMessageType::SiteDeviceBroken:
        return dispatch_typed_game_message_to_subscribers(message.payload_as<SiteDeviceBrokenMessage>());
    case GameMessageType::SiteDeviceRepaired:
        return dispatch_typed_game_message_to_subscribers(message.payload_as<SiteDeviceRepairedMessage>());
    case GameMessageType::SiteDeviceConditionChanged:
        return dispatch_typed_game_message_to_subscribers(
            message.payload_as<SiteDeviceConditionChangedMessage>());
    case GameMessageType::WorkerMeterDeltaRequested:
        return dispatch_typed_game_message_to_subscribers(message.payload_as<WorkerMeterDeltaRequestedMessage>());
    case GameMessageType::WorkerMetersChanged:
        return dispatch_typed_game_message_to_subscribers(message.payload_as<WorkerMetersChangedMessage>());
    case GameMessageType::TileEcologyChanged:
        return dispatch_typed_game_message_to_subscribers(message.payload_as<TileEcologyChangedMessage>());
    case GameMessageType::TileEcologyBatchChanged:
        return dispatch_typed_game_message_to_subscribers(message.payload_as<TileEcologyBatchChangedMessage>());
    case GameMessageType::LivingPlantStabilityChanged:
        return dispatch_typed_game_message_to_subscribers(
            message.payload_as<LivingPlantStabilityChangedMessage>());
    case GameMessageType::SiteTileStateChanged:
        return dispatch_typed_game_message_to_subscribers(message.payload_as<SiteTileStateChangedMessage>());
    case GameMessageType::RestorationProgressChanged:
        return dispatch_typed_game_message_to_subscribers(
            message.payload_as<RestorationProgressChangedMessage>());
    case GameMessageType::SiteActionCompleted:
        return dispatch_typed_game_message_to_subscribers(message.payload_as<SiteActionCompletedMessage>());
    case GameMessageType::PlacementReservationRequested:
        return dispatch_typed_game_message_to_subscribers(
            message.payload_as<PlacementReservationRequestedMessage>());
    case GameMessageType::PlacementReservationAccepted:
        return dispatch_typed_game_message_to_subscribers(
            message.payload_as<PlacementReservationAcceptedMessage>());
    case GameMessageType::PlacementReservationRejected:
        return dispatch_typed_game_message_to_subscribers(
            message.payload_as<PlacementReservationRejectedMessage>());
    case GameMessageType::PlacementReservationReleased:
        return dispatch_typed_game_message_to_subscribers(
            message.payload_as<PlacementReservationReleasedMessage>());
    case GameMessageType::InventoryCraftCommitRequested:
        return dispatch_typed_game_message_to_subscribers(
            message.payload_as<InventoryCraftCommitRequestedMessage>());
    default:
        break;
    }

    const auto dispatch_profiled_message = [this](Gs1RuntimeProfileSystemId system_id, auto&& dispatch_fn)
        -> Gs1Status
    {
        if (!profiled_system_enabled(system_id))
        {
            return GS1_STATUS_OK;
        }

        const auto started_at = std::chrono::steady_clock::now();
        const auto status = dispatch_fn();
        record_timing_sample(
            profiled_systems_[static_cast<std::size_t>(system_id)].message_timing,
            elapsed_milliseconds_since(started_at));
        return status;
    };

    RuntimeInvocation invocation {*this};
    const auto& subscribers = message_subscribers_[message_type_index(message.type)];
    for (const RuntimeGameMessageSubscriberEntry& entry : subscribers)
    {
        IRuntimeSystem* system = entry.system;
        if (system == nullptr)
        {
            continue;
        }

        RuntimeMutationScope mutation_scope {state_manager_, *system};
        const auto status = entry.profile_id.has_value()
            ? dispatch_profiled_message(
                *entry.profile_id,
                [&]() -> Gs1Status
                {
                    return system->process_game_message(invocation, message);
                })
            : system->process_game_message(invocation, message);
        if (status != GS1_STATUS_OK)
        {
            return status;
        }
    }

    return GS1_STATUS_OK;
}

Gs1Status GameRuntime::dispatch_host_messages(std::uint32_t& out_processed_count)
{
    out_processed_count = 0U;

    while (!host_messages_.empty())
    {
        const auto message = host_messages_.front();
        host_messages_.pop_front();
        out_processed_count += 1U;

        const auto status = dispatch_subscribed_host_message(message);
        if (status != GS1_STATUS_OK)
        {
            return status;
        }

        const auto dispatch_status = dispatch_queued_messages();
        if (dispatch_status != GS1_STATUS_OK)
        {
            return dispatch_status;
        }
    }

    return GS1_STATUS_OK;
}

Gs1Status GameRuntime::dispatch_subscribed_host_message(const Gs1HostMessage& message)
{
    if (!is_valid_host_message_type(message.type))
    {
        return GS1_STATUS_INVALID_ARGUMENT;
    }

    if (const auto translated_status = dispatch_runtime_translated_host_message(message);
        translated_status.has_value())
    {
        return *translated_status;
    }

    RuntimeInvocation invocation {*this};
    const auto& subscribers = host_message_subscribers_[host_message_type_index(message.type)];
    for (IRuntimeSystem* system : subscribers)
    {
        if (system == nullptr)
        {
            continue;
        }

        RuntimeMutationScope mutation_scope {state_manager_, *system};
        const auto status = system->process_host_message(invocation, message);
        if (status != GS1_STATUS_OK)
        {
            return status;
        }
    }

    return GS1_STATUS_OK;
}

std::optional<Gs1Status> GameRuntime::dispatch_runtime_translated_host_message(
    const Gs1HostMessage& message)
{
    if (message.type != GS1_HOST_EVENT_GAMEPLAY_ACTION)
    {
        return std::nullopt;
    }

    const auto& action = message.payload.gameplay_action.action;
    switch (action.type)
    {
    case GS1_GAMEPLAY_ACTION_START_NEW_CAMPAIGN:
        return dispatch_game_message_inline(
            StartNewCampaignMessage {action.arg0, static_cast<std::uint32_t>(action.arg1)});

    case GS1_GAMEPLAY_ACTION_SELECT_DEPLOYMENT_SITE:
        if (action.target_id == 0U)
        {
            return GS1_STATUS_INVALID_ARGUMENT;
        }
        return dispatch_game_message_inline(SelectDeploymentSiteMessage {action.target_id});

    case GS1_GAMEPLAY_ACTION_CLEAR_DEPLOYMENT_SITE_SELECTION:
        return dispatch_game_message_inline(ClearDeploymentSiteSelectionMessage {});

    case GS1_GAMEPLAY_ACTION_START_SITE_ATTEMPT:
        if (action.target_id == 0U)
        {
            return GS1_STATUS_INVALID_ARGUMENT;
        }
        return dispatch_game_message_inline(StartSiteAttemptMessage {action.target_id});

    case GS1_GAMEPLAY_ACTION_RETURN_TO_REGIONAL_MAP:
        return dispatch_game_message_inline(ReturnToRegionalMapMessage {});

    case GS1_GAMEPLAY_ACTION_CLAIM_TECHNOLOGY_NODE:
        if (action.target_id == 0U || action.arg0 == 0U ||
            action.arg0 > static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max()))
        {
            return GS1_STATUS_INVALID_ARGUMENT;
        }
        return dispatch_game_message_inline(TechnologyNodeClaimRequestedMessage {
            action.target_id,
            static_cast<std::uint32_t>(action.arg0)});

    case GS1_GAMEPLAY_ACTION_REFUND_TECHNOLOGY_NODE:
        if (action.target_id == 0U)
        {
            return GS1_STATUS_INVALID_ARGUMENT;
        }
        return dispatch_game_message_inline(TechnologyNodeRefundRequestedMessage {
            action.target_id});

    default:
        return std::nullopt;
    }
}

void GameRuntime::run_fixed_step()
{
    if (!state().campaign_core.has_value() || !state().site_run_meta.has_value())
    {
        return;
    }

    const auto fixed_step_started_at = std::chrono::steady_clock::now();
    const auto run_profiled_system = [this](Gs1RuntimeProfileSystemId system_id, auto&& run_fn)
    {
        if (!profiled_system_enabled(system_id))
        {
            return;
        }

        const auto started_at = std::chrono::steady_clock::now();
        run_fn();
        record_timing_sample(
            profiled_systems_[static_cast<std::size_t>(system_id)].run_timing,
            elapsed_milliseconds_since(started_at));
    };

    RuntimeInvocation invocation {*this};
    for (const FixedStepSystemEntry& entry : fixed_step_systems_)
    {
        IRuntimeSystem* system = entry.system;
        if (system == nullptr)
        {
            continue;
        }

        RuntimeMutationScope mutation_scope {state_manager_, *system};
        if (!entry.profile_id.has_value())
        {
            system->run(invocation);
            continue;
        }

        run_profiled_system(
            *entry.profile_id,
            [&]()
            {
                system->run(invocation);
            });
    }

    record_timing_sample(fixed_step_timing_, elapsed_milliseconds_since(fixed_step_started_at));
}
}  // namespace gs1

