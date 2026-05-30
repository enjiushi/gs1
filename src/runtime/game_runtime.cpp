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

template <typename... Systems>
consteval bool runtime_fixed_step_orders_within_game_system_pack(
    system_pack<Systems...>) noexcept
{
    return (((!runtime_fixed_step_order_v<Systems>.has_value()) ||
             (runtime_fixed_step_order_v<Systems>.value() < GameSystems::size)) &&
            ...);
}

static_assert(
    runtime_fixed_step_orders_within_game_system_pack(GameSystems {}),
    "Fixed-step system order values must fit inside the active GameSystems pack size.");

template <std::uint32_t Order, typename AppendFn>
void append_fixed_step_systems_for_declared_order(AppendFn&& append_fn)
{
    for_each_type<typename GameSystems::list>(
        [&]<typename System>()
        {
            if constexpr (runtime_fixed_step_order_v<System>.has_value() &&
                          runtime_fixed_step_order_v<System>.value() == Order)
            {
                append_fn.template operator()<System>();
            }
        });
}

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

bool is_valid_message_type(GameMessageType type) noexcept
{
    return message_type_index(type) < k_game_message_type_count;
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

}  // namespace

RuntimeInvocation::RuntimeInvocation(GameRuntime& runtime) noexcept
    : runtime_(&runtime)
    , owned_state_(&runtime.state())
    , state_manager_(&runtime.state_manager_)
    , app_state_(&runtime.state().app_state.get())
    , fixed_step_seconds_(&runtime.state().fixed_step_seconds.get())
    , site_world_(runtime.site_world_)
{
}

RuntimeInvocation::RuntimeInvocation(
    GameState& state,
    GameMessageQueue* emitted_game_messages) noexcept
    : owned_state_(&state)
    , app_state_(&state.app_state.get())
    , fixed_step_seconds_(&state.fixed_step_seconds.get())
    , emitted_game_messages_(emitted_game_messages)
{
}

RuntimeInvocation::RuntimeInvocation(
    GameState& state,
    StateManager& state_manager,
    SiteWorldHandle site_world,
    GameMessageQueue* emitted_game_messages) noexcept
    : owned_state_(&state)
    , state_manager_(&state_manager)
    , app_state_(&state.app_state.get())
    , fixed_step_seconds_(&state.fixed_step_seconds.get())
    , site_world_(std::move(site_world))
    , emitted_game_messages_(emitted_game_messages)
{
}

RuntimeInvocation::RuntimeInvocation(
    GameState& state,
    StateManager& state_manager,
    float move_direction_x,
    float move_direction_y,
    float move_direction_z,
    bool move_direction_present,
    SiteWorldHandle site_world,
    GameMessageQueue* emitted_game_messages) noexcept
    : owned_state_(&state)
    , state_manager_(&state_manager)
    , app_state_(&state.app_state.get())
    , fixed_step_seconds_(&state.fixed_step_seconds.get())
    , site_world_(std::move(site_world))
    , emitted_game_messages_(emitted_game_messages)
{
    move_direction_ = RuntimeMoveDirectionSnapshot {
        move_direction_x,
        move_direction_y,
        move_direction_z,
        move_direction_present};
    state_manager_->state<StateSetId::MoveDirection>(state) = move_direction_;
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

    fixed_step_systems_.clear();

    const auto append_fixed_step_system = [this]<typename System>()
    {
        auto& system = std::get<System>(systems_);
        fixed_step_systems_.push_back(GameRuntime::FixedStepSystemEntry {
            .system = &system,
            .profile_id = runtime_profile_system_id_for<System>(system),
            .order = runtime_fixed_step_order_v<System>.value()});
    };

    const auto register_subscribers = [this]<typename System>()
    {
        auto& system = std::get<System>(systems_);
        state_manager_.register_resolver(system);
    };

    [&]<std::size_t... Orders>(std::index_sequence<Orders...>)
    {
        (append_fixed_step_systems_for_declared_order<static_cast<std::uint32_t>(Orders)>(
             append_fixed_step_system),
         ...);
    }(std::make_index_sequence<GameSystems::size> {});

    for_each_type<typename GameSystems::list>(register_subscribers);
}

Gs1Status GameRuntime::submit_gameplay_action(const Gs1GameplayAction& action)
{
    PendingHostCommand command {};
    command.kind = PendingHostCommand::Kind::GameplayAction;
    command.gameplay_action = action;
    pending_host_commands_.push_back(command);
    return GS1_STATUS_OK;
}

Gs1Status GameRuntime::submit_site_move_direction(const Gs1SiteMoveDirectionCommand& command)
{
    auto pending = PendingHostCommand {};
    pending.kind = PendingHostCommand::Kind::SiteMoveDirection;
    pending.site_move_direction = command;
    pending_host_commands_.push_back(pending);
    return GS1_STATUS_OK;
}

Gs1Status GameRuntime::submit_site_action_request(const Gs1SiteActionRequestCommand& command)
{
    auto pending = PendingHostCommand {};
    pending.kind = PendingHostCommand::Kind::SiteActionRequest;
    pending.site_action_request = command;
    pending_host_commands_.push_back(pending);
    return GS1_STATUS_OK;
}

Gs1Status GameRuntime::submit_site_action_cancel(const Gs1SiteActionCancelCommand& command)
{
    auto pending = PendingHostCommand {};
    pending.kind = PendingHostCommand::Kind::SiteActionCancel;
    pending.site_action_cancel = command;
    pending_host_commands_.push_back(pending);
    return GS1_STATUS_OK;
}

Gs1Status GameRuntime::submit_site_context_request(const Gs1SiteContextRequestCommand& command)
{
    auto pending = PendingHostCommand {};
    pending.kind = PendingHostCommand::Kind::SiteContextRequest;
    pending.site_context_request = command;
    pending_host_commands_.push_back(pending);
    return GS1_STATUS_OK;
}

Gs1Status GameRuntime::submit_site_inventory_slot_tap(const Gs1SiteInventorySlotTapCommand& command)
{
    auto pending = PendingHostCommand {};
    pending.kind = PendingHostCommand::Kind::SiteInventorySlotTap;
    pending.site_inventory_slot_tap = command;
    pending_host_commands_.push_back(pending);
    return GS1_STATUS_OK;
}

Gs1Status GameRuntime::submit_site_scene_ready()
{
    PendingHostCommand command {};
    command.kind = PendingHostCommand::Kind::SiteSceneReady;
    pending_host_commands_.push_back(command);
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
        finish_phase();
        return GS1_STATUS_OK;
    }

    if (state().app_state.get() == GS1_APP_STATE_SITE_LOADING)
    {
        finish_phase();
        return GS1_STATUS_OK;
    }

    if (!state().site_clock.has_value())
    {
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

    state().move_direction = RuntimeMoveDirectionSnapshot {};
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

    finish_phase();
    return status;
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

Gs1Status GameRuntime::dispatch_host_messages(std::uint32_t& out_processed_count)
{
    out_processed_count = 0U;

    while (!pending_host_commands_.empty())
    {
        const auto command = pending_host_commands_.front();
        pending_host_commands_.pop_front();
        out_processed_count += 1U;

        const auto status = dispatch_pending_host_command(command);
        if (status != GS1_STATUS_OK)
        {
            return status;
        }
    }

    return GS1_STATUS_OK;
}

Gs1Status GameRuntime::dispatch_pending_host_command(const PendingHostCommand& command)
{
    switch (command.kind)
    {
    case PendingHostCommand::Kind::SiteSceneReady:
    {
        if (!state().site_run_meta.has_value())
        {
            return GS1_STATUS_OK;
        }

        auto* campaign_flow = find_system<CampaignFlowSystem>();
        if (campaign_flow == nullptr)
        {
            return GS1_STATUS_INVALID_STATE;
        }

        RuntimeInvocation invocation {*this};
        RuntimeMutationScope mutation_scope {state_manager_, *campaign_flow};
        campaign_flow->run(invocation);
        return GS1_STATUS_OK;
    }

    case PendingHostCommand::Kind::SiteInventorySlotTap:
    {
        const auto& payload = command.site_inventory_slot_tap;
        return dispatch_game_message_inline(InventorySlotTappedMessage {
            payload.storage_id,
            payload.item_instance_id,
            payload.slot_index,
            payload.container_kind,
            0U,
            payload.companion_storage_id});
    }

    case PendingHostCommand::Kind::SiteMoveDirection:
    {
        if (!state().site_run_meta.has_value() || site_world() == nullptr)
        {
            return GS1_STATUS_OK;
        }

        const auto& payload = command.site_move_direction;
        RuntimePrivilegedStateMutation privileged {state_manager_};
        auto& move_direction = state_manager_.state<StateSetId::MoveDirection>(state());
        move_direction.world_move_x = payload.world_move_x;
        move_direction.world_move_y = payload.world_move_y;
        move_direction.world_move_z = payload.world_move_z;
        move_direction.present = true;
        return GS1_STATUS_OK;
    }

    case PendingHostCommand::Kind::SiteContextRequest:
    {
        if (!state().site_run_meta.has_value())
        {
            return GS1_STATUS_OK;
        }

        const auto& payload = command.site_context_request;
        const auto& action_meta =
            state_manager_.query<StateSetId::SiteActionMeta>(state());
        if (action_meta.has_value() && action_meta->placement_mode.active)
        {
            return dispatch_game_message_inline(PlacementModeCursorMovedMessage {
                payload.tile_x,
                payload.tile_y,
                payload.flags});
        }

        return dispatch_game_message_inline(CraftContextRequestedMessage {
            payload.tile_x,
            payload.tile_y,
            payload.flags});
    }

    case PendingHostCommand::Kind::SiteActionRequest:
    {
        const auto& payload = command.site_action_request;
        if (payload.action_kind == GS1_SITE_ACTION_NONE)
        {
            return GS1_STATUS_INVALID_ARGUMENT;
        }

        return dispatch_game_message_inline(StartSiteActionMessage {
            payload.action_kind,
            payload.flags,
            payload.quantity == 0U ? 1U : payload.quantity,
            payload.target_tile_x,
            payload.target_tile_y,
            payload.primary_subject_id,
            payload.secondary_subject_id,
            payload.item_id});
    }

    case PendingHostCommand::Kind::SiteActionCancel:
    {
        const auto& payload = command.site_action_cancel;
        if (payload.action_id == 0U &&
            (payload.flags &
                (GS1_SITE_ACTION_CANCEL_FLAG_CURRENT_ACTION |
                    GS1_SITE_ACTION_CANCEL_FLAG_PLACEMENT_MODE)) == 0U)
        {
            return GS1_STATUS_INVALID_ARGUMENT;
        }

        return dispatch_game_message_inline(CancelSiteActionMessage {
            payload.action_id,
            payload.flags});
    }

    case PendingHostCommand::Kind::GameplayAction:
    {
        const auto& action = command.gameplay_action;
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

        case GS1_GAMEPLAY_ACTION_ACCEPT_TASK:
            if (action.target_id == 0U)
            {
                return GS1_STATUS_INVALID_ARGUMENT;
            }
            return dispatch_game_message_inline(TaskAcceptRequestedMessage {action.target_id});

        case GS1_GAMEPLAY_ACTION_CLAIM_TASK_REWARD:
            if (action.target_id == 0U ||
                action.arg0 > static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max()))
            {
                return GS1_STATUS_INVALID_ARGUMENT;
            }
            return dispatch_game_message_inline(TaskRewardClaimRequestedMessage {
                action.target_id,
                static_cast<std::uint32_t>(action.arg0)});

        case GS1_GAMEPLAY_ACTION_END_SITE_MODIFIER:
            if (action.target_id == 0U)
            {
                return GS1_STATUS_INVALID_ARGUMENT;
            }
            return dispatch_game_message_inline(SiteModifierEndRequestedMessage {action.target_id});

        case GS1_GAMEPLAY_ACTION_BUY_PHONE_LISTING:
            if (action.target_id == 0U)
            {
                return GS1_STATUS_INVALID_ARGUMENT;
            }
            return dispatch_game_message_inline(PhoneListingPurchaseRequestedMessage {
                action.target_id,
                static_cast<std::uint16_t>(action.arg0),
                0U});

        case GS1_GAMEPLAY_ACTION_SELL_PHONE_LISTING:
            if (action.target_id == 0U)
            {
                return GS1_STATUS_INVALID_ARGUMENT;
            }
            return dispatch_game_message_inline(PhoneListingSaleRequestedMessage {
                action.target_id,
                static_cast<std::uint16_t>(action.arg0),
                0U});

        case GS1_GAMEPLAY_ACTION_HIRE_CONTRACTOR:
            if (action.target_id == 0U ||
                action.arg0 > static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max()))
            {
                return GS1_STATUS_INVALID_ARGUMENT;
            }
            return dispatch_game_message_inline(ContractorHireRequestedMessage {
                action.target_id,
                static_cast<std::uint32_t>(action.arg0)});

        case GS1_GAMEPLAY_ACTION_PURCHASE_SITE_UNLOCKABLE:
            if (action.target_id == 0U)
            {
                return GS1_STATUS_INVALID_ARGUMENT;
            }
            return dispatch_game_message_inline(SiteUnlockablePurchaseRequestedMessage {
                action.target_id});

        default:
            return GS1_STATUS_OK;
        }
    }
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

