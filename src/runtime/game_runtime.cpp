#include "runtime/game_runtime.h"

#include "campaign/systems/campaign_flow_system.h"
#include "campaign/systems/campaign_system_context.h"
#include "campaign/systems/loadout_planner_system.h"
#include "commands/command_dispatcher.h"
#include "content/defs/item_defs.h"
#include "site/inventory_storage.h"
#include "site/site_world_access.h"
#include "site/systems/action_execution_system.h"
#include "site/systems/camp_durability_system.h"
#include "site/systems/craft_system.h"
#include "site/systems/device_maintenance_system.h"
#include "site/systems/device_support_system.h"
#include "site/systems/ecology_system.h"
#include "site/systems/economy_phone_system.h"
#include "site/site_projection_update_flags.h"
#include "site/systems/inventory_system.h"
#include "site/systems/local_weather_resolve_system.h"
#include "site/systems/modifier_system.h"
#include "site/systems/placement_validation_system.h"
#include "site/systems/failure_recovery_system.h"
#include "site/systems/site_completion_system.h"
#include "site/systems/site_flow_system.h"
#include "site/systems/task_board_system.h"
#include "site/systems/weather_event_system.h"
#include "site/systems/worker_condition_system.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <limits>
#include <set>

namespace gs1
{
namespace
{
[[nodiscard]] std::uint32_t visible_loadout_slot_count(const LoadoutPlannerState& planner) noexcept
{
    std::uint32_t count = 0U;
    for (const auto& slot : planner.selected_loadout_slots)
    {
        if (slot.occupied && slot.item_id.value != 0U && slot.quantity > 0U)
        {
            count += 1U;
        }
    }

    return count;
}

std::size_t command_type_index(GameCommandType type) noexcept
{
    return static_cast<std::size_t>(type);
}

bool is_valid_command_type(GameCommandType type) noexcept
{
    return command_type_index(type) < k_game_command_type_count;
}

std::size_t feedback_event_type_index(Gs1FeedbackEventType type) noexcept
{
    return static_cast<std::size_t>(type);
}

bool is_valid_feedback_event_type(Gs1FeedbackEventType type) noexcept
{
    return feedback_event_type_index(type) < k_feedback_event_type_count;
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

std::uint64_t pack_inventory_use_arg(
    Gs1InventoryContainerKind container_kind,
    std::uint32_t slot_index,
    std::uint32_t quantity) noexcept
{
    return static_cast<std::uint64_t>(container_kind) |
        (static_cast<std::uint64_t>(slot_index & 0xffU) << 8U) |
        (static_cast<std::uint64_t>(quantity & 0xffffU) << 16U);
}

Gs1InventoryContainerKind unpack_inventory_container(std::uint64_t packed) noexcept
{
    return static_cast<Gs1InventoryContainerKind>(packed & 0xffU);
}

std::uint32_t unpack_inventory_slot_index(std::uint64_t packed) noexcept
{
    return static_cast<std::uint32_t>((packed >> 8U) & 0xffU);
}

std::uint32_t unpack_inventory_quantity(std::uint64_t packed) noexcept
{
    return static_cast<std::uint32_t>((packed >> 16U) & 0xffffU);
}

std::uint64_t pack_inventory_transfer_arg(
    Gs1InventoryContainerKind source_container_kind,
    std::uint32_t source_slot_index,
    Gs1InventoryContainerKind destination_container_kind,
    std::uint32_t destination_slot_index,
    std::uint32_t quantity) noexcept
{
    return static_cast<std::uint64_t>(source_container_kind) |
        (static_cast<std::uint64_t>(source_slot_index & 0xffU) << 8U) |
        (static_cast<std::uint64_t>(destination_container_kind) << 16U) |
        (static_cast<std::uint64_t>(destination_slot_index & 0xffU) << 24U) |
        (static_cast<std::uint64_t>(quantity & 0xffffU) << 32U);
}

Gs1InventoryContainerKind unpack_transfer_source_container(std::uint64_t packed) noexcept
{
    return static_cast<Gs1InventoryContainerKind>(packed & 0xffU);
}

std::uint32_t unpack_transfer_source_slot(std::uint64_t packed) noexcept
{
    return static_cast<std::uint32_t>((packed >> 8U) & 0xffU);
}

Gs1InventoryContainerKind unpack_transfer_destination_container(std::uint64_t packed) noexcept
{
    return static_cast<Gs1InventoryContainerKind>((packed >> 16U) & 0xffU);
}

std::uint32_t unpack_transfer_destination_slot(std::uint64_t packed) noexcept
{
    return static_cast<std::uint32_t>((packed >> 24U) & 0xffU);
}

std::uint32_t unpack_transfer_quantity(std::uint64_t packed) noexcept
{
    return static_cast<std::uint32_t>((packed >> 32U) & 0xffffU);
}

std::uint32_t unpack_transfer_source_owner(std::uint64_t packed) noexcept
{
    return static_cast<std::uint32_t>(packed & 0xffffffffULL);
}

std::uint32_t unpack_transfer_destination_owner(std::uint64_t packed) noexcept
{
    return static_cast<std::uint32_t>((packed >> 32U) & 0xffffffffULL);
}

bool action_has_started(const ActionState& action_state) noexcept
{
    return action_state.current_action_id.has_value() &&
        action_state.action_kind != ActionKind::None &&
        action_state.started_at_world_minute.has_value() &&
        action_state.total_action_minutes > 0.0;
}

float action_progress_normalized(const ActionState& action_state) noexcept
{
    if (!action_has_started(action_state))
    {
        return 0.0f;
    }

    const double elapsed_minutes =
        std::max(0.0, action_state.total_action_minutes - action_state.remaining_action_minutes);
    const double normalized = elapsed_minutes / std::max(action_state.total_action_minutes, 0.0001);
    return static_cast<float>(std::clamp(normalized, 0.0, 1.0));
}

Gs1EngineCommand make_engine_command(
    Gs1EngineCommandType type)
{
    Gs1EngineCommand command {};
    command.type = type;
    return command;
}

Gs1TaskPresentationListKind to_task_presentation_list_kind(TaskRuntimeListKind kind) noexcept
{
    switch (kind)
    {
    case TaskRuntimeListKind::Accepted:
        return GS1_TASK_PRESENTATION_LIST_ACCEPTED;
    case TaskRuntimeListKind::Completed:
        return GS1_TASK_PRESENTATION_LIST_COMPLETED;
    case TaskRuntimeListKind::Visible:
    default:
        return GS1_TASK_PRESENTATION_LIST_VISIBLE;
    }
}

Gs1PhoneListingPresentationKind to_phone_listing_presentation_kind(PhoneListingKind kind) noexcept
{
    switch (kind)
    {
    case PhoneListingKind::SellItem:
        return GS1_PHONE_LISTING_PRESENTATION_SELL_ITEM;
    case PhoneListingKind::HireContractor:
        return GS1_PHONE_LISTING_PRESENTATION_HIRE_CONTRACTOR;
    case PhoneListingKind::PurchaseUnlockable:
        return GS1_PHONE_LISTING_PRESENTATION_PURCHASE_UNLOCKABLE;
    case PhoneListingKind::BuyItem:
    default:
        return GS1_PHONE_LISTING_PRESENTATION_BUY_ITEM;
    }
}

template <typename SiteSystemTag, typename ProcessCommandFn>
Gs1Status dispatch_site_system_command(
    ProcessCommandFn process_command_fn,
    const GameCommand& command,
    const std::optional<CampaignState>& campaign,
    std::optional<SiteRunState>& active_site_run,
    GameCommandQueue& command_queue,
    double fixed_step_seconds)
{
    if (!campaign.has_value() || !active_site_run.has_value())
    {
        return GS1_STATUS_INVALID_STATE;
    }

    auto context = make_site_system_context<SiteSystemTag>(
        *campaign,
        *active_site_run,
        command_queue,
        fixed_step_seconds,
        SiteMoveDirectionInput {});
    return process_command_fn(context, command);
}
}  // namespace

GameRuntime::GameRuntime(Gs1RuntimeCreateDesc create_desc)
    : create_desc_(create_desc)
{
    if (create_desc_.fixed_step_seconds > 0.0)
    {
        fixed_step_seconds_ = create_desc_.fixed_step_seconds;
    }

    initialize_subscription_tables();
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
        copy_timing_snapshot(source.command_timing, destination.command_timing);
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
        system_state.command_timing = {};
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

void GameRuntime::initialize_subscription_tables()
{
#ifndef NDEBUG
    const std::array site_system_access_registry {
        ActionExecutionSystem::access(),
        WeatherEventSystem::access(),
        WorkerConditionSystem::access(),
        EcologySystem::access(),
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

    for (std::size_t index = 0; index < k_game_command_type_count; ++index)
    {
        const auto type = static_cast<GameCommandType>(index);
        auto& subscribers = command_subscribers_[index];

        if (CampaignFlowSystem::subscribes_to(type))
        {
            subscribers.push_back(CommandSubscriberId::CampaignFlow);
        }

        if (LoadoutPlannerSystem::subscribes_to(type))
        {
            subscribers.push_back(CommandSubscriberId::LoadoutPlanner);
        }

        if (ActionExecutionSystem::subscribes_to(type))
        {
            subscribers.push_back(CommandSubscriberId::ActionExecution);
        }

        if (WeatherEventSystem::subscribes_to(type))
        {
            subscribers.push_back(CommandSubscriberId::WeatherEvent);
        }

        if (WorkerConditionSystem::subscribes_to(type))
        {
            subscribers.push_back(CommandSubscriberId::WorkerCondition);
        }

        if (EcologySystem::subscribes_to(type))
        {
            subscribers.push_back(CommandSubscriberId::Ecology);
        }

        if (TaskBoardSystem::subscribes_to(type))
        {
            subscribers.push_back(CommandSubscriberId::TaskBoard);
        }

        if (PlacementValidationSystem::subscribes_to(type))
        {
            subscribers.push_back(CommandSubscriberId::PlacementValidation);
        }

        if (LocalWeatherResolveSystem::subscribes_to(type))
        {
            subscribers.push_back(CommandSubscriberId::LocalWeatherResolve);
        }

        if (DeviceMaintenanceSystem::subscribes_to(type))
        {
            subscribers.push_back(CommandSubscriberId::DeviceMaintenance);
        }

        if (InventorySystem::subscribes_to(type))
        {
            subscribers.push_back(CommandSubscriberId::Inventory);
        }

        if (CraftSystem::subscribes_to(type))
        {
            subscribers.push_back(CommandSubscriberId::Craft);
        }

        if (EconomyPhoneSystem::subscribes_to(type))
        {
            subscribers.push_back(CommandSubscriberId::EconomyPhone);
        }

        if (CampDurabilitySystem::subscribes_to(type))
        {
            subscribers.push_back(CommandSubscriberId::CampDurability);
        }

        if (DeviceSupportSystem::subscribes_to(type))
        {
            subscribers.push_back(CommandSubscriberId::DeviceSupport);
        }

        if (ModifierSystem::subscribes_to(type))
        {
            subscribers.push_back(CommandSubscriberId::Modifier);
        }
    }
}

Gs1Status GameRuntime::submit_host_events(
    const Gs1HostEvent* events,
    std::uint32_t event_count)
{
    if (event_count > 0U && events == nullptr)
    {
        return GS1_STATUS_INVALID_ARGUMENT;
    }

    for (std::uint32_t i = 0; i < event_count; ++i)
    {
        host_events_.push_back(events[i]);
    }

    return GS1_STATUS_OK;
}

Gs1Status GameRuntime::submit_feedback_events(
    const Gs1FeedbackEvent* events,
    std::uint32_t event_count)
{
    if (event_count > 0U && events == nullptr)
    {
        return GS1_STATUS_INVALID_ARGUMENT;
    }

    for (std::uint32_t i = 0; i < event_count; ++i)
    {
        feedback_events_.push_back(events[i]);
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
    phase1_site_move_direction_ = {};

    auto status = GS1_STATUS_OK;
    if (!boot_initialized_)
    {
        GameCommand boot_command {};
        boot_command.type = GameCommandType::OpenMainMenu;
        boot_command.set_payload(OpenMainMenuCommand {});
        command_queue_.push_back(boot_command);

        status = dispatch_queued_commands();
        if (status != GS1_STATUS_OK)
        {
            finish_phase();
            return status;
        }

        boot_initialized_ = true;
    }

    status = dispatch_host_events(HostEventDispatchStage::Phase1, out_result.processed_host_event_count);
    if (status != GS1_STATUS_OK)
    {
        phase1_site_move_direction_ = {};
        finish_phase();
        return status;
    }

    if (!active_site_run_.has_value())
    {
        phase1_site_move_direction_ = {};
        out_result.engine_commands_queued = static_cast<std::uint32_t>(engine_commands_.size());
        finish_phase();
        return GS1_STATUS_OK;
    }

    active_site_run_->clock.accumulator_real_seconds += request.real_delta_seconds;

    while (active_site_run_->clock.accumulator_real_seconds >= fixed_step_seconds_)
    {
        active_site_run_->clock.accumulator_real_seconds -= fixed_step_seconds_;
        run_fixed_step();
        out_result.fixed_steps_executed += 1U;
    }

    flush_site_presentation_if_dirty();

    status = dispatch_queued_commands();
    phase1_site_move_direction_ = {};
    out_result.engine_commands_queued = static_cast<std::uint32_t>(engine_commands_.size());
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

    auto status = dispatch_host_events(HostEventDispatchStage::Phase2, out_result.processed_host_event_count);
    if (status != GS1_STATUS_OK)
    {
        finish_phase();
        return status;
    }

    status = dispatch_feedback_events(out_result.processed_feedback_event_count);
    if (status != GS1_STATUS_OK)
    {
        finish_phase();
        return status;
    }

    status = dispatch_queued_commands();
    out_result.engine_commands_queued = static_cast<std::uint32_t>(engine_commands_.size());
    finish_phase();
    return status;
}

Gs1Status GameRuntime::pop_engine_command(Gs1EngineCommand& out_command)
{
    if (engine_commands_.empty())
    {
        return GS1_STATUS_BUFFER_EMPTY;
    }

    out_command = engine_commands_.front();
    engine_commands_.pop_front();
    return GS1_STATUS_OK;
}

Gs1Status GameRuntime::handle_command(const GameCommand& command)
{
    command_queue_.push_back(command);
    return dispatch_queued_commands();
}

Gs1Status GameRuntime::dispatch_subscribed_command(const GameCommand& command)
{
    if (!is_valid_command_type(command.type) || command.type == GameCommandType::Count)
    {
        return GS1_STATUS_INVALID_ARGUMENT;
    }

    const auto dispatch_profiled_command = [this](Gs1RuntimeProfileSystemId system_id, auto&& dispatch_fn)
        -> Gs1Status
    {
        if (!profiled_system_enabled(system_id))
        {
            return GS1_STATUS_OK;
        }

        const auto started_at = std::chrono::steady_clock::now();
        const auto status = dispatch_fn();
        record_timing_sample(
            profiled_systems_[static_cast<std::size_t>(system_id)].command_timing,
            elapsed_milliseconds_since(started_at));
        return status;
    };

    const auto& subscribers = command_subscribers_[command_type_index(command.type)];
    for (const auto subscriber : subscribers)
    {
        switch (subscriber)
        {
        case CommandSubscriberId::CampaignFlow:
        {
            const auto status = dispatch_profiled_command(
                GS1_RUNTIME_PROFILE_SYSTEM_CAMPAIGN_FLOW,
                [&]() -> Gs1Status {
                    CampaignFlowCommandContext context {
                        campaign_,
                        active_site_run_,
                        app_state_,
                        command_queue_};
                    return CampaignFlowSystem::process_command(context, command);
                });
            if (status != GS1_STATUS_OK)
            {
                return status;
            }
            break;
        }

        case CommandSubscriberId::LoadoutPlanner:
        {
            if (!campaign_.has_value())
            {
                break;
            }

            const auto status = dispatch_profiled_command(
                GS1_RUNTIME_PROFILE_SYSTEM_LOADOUT_PLANNER,
                [&]() -> Gs1Status {
                    CampaignSystemContext context {*campaign_};
                    return LoadoutPlannerSystem::process_command(context, command);
                });
            if (status != GS1_STATUS_OK)
            {
                return status;
            }
            break;
        }

        case CommandSubscriberId::ActionExecution:
        {
            const auto status = dispatch_profiled_command(
                GS1_RUNTIME_PROFILE_SYSTEM_ACTION_EXECUTION,
                [&]() -> Gs1Status {
                    return dispatch_site_system_command<ActionExecutionSystem>(
                        ActionExecutionSystem::process_command,
                        command,
                        campaign_,
                        active_site_run_,
                        command_queue_,
                        fixed_step_seconds_);
                });
            if (status != GS1_STATUS_OK)
            {
                return status;
            }
            break;
        }

        case CommandSubscriberId::WeatherEvent:
        {
            const auto status = dispatch_profiled_command(
                GS1_RUNTIME_PROFILE_SYSTEM_WEATHER_EVENT,
                [&]() -> Gs1Status {
                    return dispatch_site_system_command<WeatherEventSystem>(
                        WeatherEventSystem::process_command,
                        command,
                        campaign_,
                        active_site_run_,
                        command_queue_,
                        fixed_step_seconds_);
                });
            if (status != GS1_STATUS_OK)
            {
                return status;
            }
            break;
        }

        case CommandSubscriberId::WorkerCondition:
        {
            const auto status = dispatch_profiled_command(
                GS1_RUNTIME_PROFILE_SYSTEM_WORKER_CONDITION,
                [&]() -> Gs1Status {
                    return dispatch_site_system_command<WorkerConditionSystem>(
                        WorkerConditionSystem::process_command,
                        command,
                        campaign_,
                        active_site_run_,
                        command_queue_,
                        fixed_step_seconds_);
                });
            if (status != GS1_STATUS_OK)
            {
                return status;
            }
            break;
        }

        case CommandSubscriberId::Ecology:
        {
            const auto status = dispatch_profiled_command(
                GS1_RUNTIME_PROFILE_SYSTEM_ECOLOGY,
                [&]() -> Gs1Status {
                    return dispatch_site_system_command<EcologySystem>(
                        EcologySystem::process_command,
                        command,
                        campaign_,
                        active_site_run_,
                        command_queue_,
                        fixed_step_seconds_);
                });
            if (status != GS1_STATUS_OK)
            {
                return status;
            }
            break;
        }

        case CommandSubscriberId::TaskBoard:
        {
            const auto status = dispatch_profiled_command(
                GS1_RUNTIME_PROFILE_SYSTEM_TASK_BOARD,
                [&]() -> Gs1Status {
                    return dispatch_site_system_command<TaskBoardSystem>(
                        TaskBoardSystem::process_command,
                        command,
                        campaign_,
                        active_site_run_,
                        command_queue_,
                        fixed_step_seconds_);
                });
            if (status != GS1_STATUS_OK)
            {
                return status;
            }
            break;
        }

        case CommandSubscriberId::PlacementValidation:
        {
            const auto status = dispatch_profiled_command(
                GS1_RUNTIME_PROFILE_SYSTEM_PLACEMENT_VALIDATION,
                [&]() -> Gs1Status {
                    return dispatch_site_system_command<PlacementValidationSystem>(
                        PlacementValidationSystem::process_command,
                        command,
                        campaign_,
                        active_site_run_,
                        command_queue_,
                        fixed_step_seconds_);
                });
            if (status != GS1_STATUS_OK)
            {
                return status;
            }
            break;
        }

        case CommandSubscriberId::LocalWeatherResolve:
        {
            const auto status = dispatch_profiled_command(
                GS1_RUNTIME_PROFILE_SYSTEM_LOCAL_WEATHER_RESOLVE,
                [&]() -> Gs1Status {
                    return dispatch_site_system_command<LocalWeatherResolveSystem>(
                        LocalWeatherResolveSystem::process_command,
                        command,
                        campaign_,
                        active_site_run_,
                        command_queue_,
                        fixed_step_seconds_);
                });
            if (status != GS1_STATUS_OK)
            {
                return status;
            }
            break;
        }

        case CommandSubscriberId::Inventory:
        {
            const auto status = dispatch_profiled_command(
                GS1_RUNTIME_PROFILE_SYSTEM_INVENTORY,
                [&]() -> Gs1Status {
                    return dispatch_site_system_command<InventorySystem>(
                        InventorySystem::process_command,
                        command,
                        campaign_,
                        active_site_run_,
                        command_queue_,
                        fixed_step_seconds_);
                });
            if (status != GS1_STATUS_OK)
            {
                return status;
            }
            break;
        }

        case CommandSubscriberId::Craft:
        {
            const auto status = dispatch_profiled_command(
                GS1_RUNTIME_PROFILE_SYSTEM_CRAFT,
                [&]() -> Gs1Status {
                    return dispatch_site_system_command<CraftSystem>(
                        CraftSystem::process_command,
                        command,
                        campaign_,
                        active_site_run_,
                        command_queue_,
                        fixed_step_seconds_);
                });
            if (status != GS1_STATUS_OK)
            {
                return status;
            }
            break;
        }

        case CommandSubscriberId::EconomyPhone:
        {
            const auto status = dispatch_profiled_command(
                GS1_RUNTIME_PROFILE_SYSTEM_ECONOMY_PHONE,
                [&]() -> Gs1Status {
                    return dispatch_site_system_command<EconomyPhoneSystem>(
                        EconomyPhoneSystem::process_command,
                        command,
                        campaign_,
                        active_site_run_,
                        command_queue_,
                        fixed_step_seconds_);
                });
            if (status != GS1_STATUS_OK)
            {
                return status;
            }
            break;
        }

        case CommandSubscriberId::CampDurability:
        {
            const auto status = dispatch_profiled_command(
                GS1_RUNTIME_PROFILE_SYSTEM_CAMP_DURABILITY,
                [&]() -> Gs1Status {
                    return dispatch_site_system_command<CampDurabilitySystem>(
                        CampDurabilitySystem::process_command,
                        command,
                        campaign_,
                        active_site_run_,
                        command_queue_,
                        fixed_step_seconds_);
                });
            if (status != GS1_STATUS_OK)
            {
                return status;
            }
            break;
        }

        case CommandSubscriberId::DeviceSupport:
        {
            const auto status = dispatch_profiled_command(
                GS1_RUNTIME_PROFILE_SYSTEM_DEVICE_SUPPORT,
                [&]() -> Gs1Status {
                    return dispatch_site_system_command<DeviceSupportSystem>(
                        DeviceSupportSystem::process_command,
                        command,
                        campaign_,
                        active_site_run_,
                        command_queue_,
                        fixed_step_seconds_);
                });
            if (status != GS1_STATUS_OK)
            {
                return status;
            }
            break;
        }

        case CommandSubscriberId::DeviceMaintenance:
        {
            const auto status = dispatch_profiled_command(
                GS1_RUNTIME_PROFILE_SYSTEM_DEVICE_MAINTENANCE,
                [&]() -> Gs1Status {
                    return dispatch_site_system_command<DeviceMaintenanceSystem>(
                        DeviceMaintenanceSystem::process_command,
                        command,
                        campaign_,
                        active_site_run_,
                        command_queue_,
                        fixed_step_seconds_);
                });
            if (status != GS1_STATUS_OK)
            {
                return status;
            }
            break;
        }

        case CommandSubscriberId::Modifier:
        {
            const auto status = dispatch_profiled_command(
                GS1_RUNTIME_PROFILE_SYSTEM_MODIFIER,
                [&]() -> Gs1Status {
                    return dispatch_site_system_command<ModifierSystem>(
                        ModifierSystem::process_command,
                        command,
                        campaign_,
                        active_site_run_,
                        command_queue_,
                        fixed_step_seconds_);
                });
            if (status != GS1_STATUS_OK)
            {
                return status;
            }
            break;
        }

        default:
            return GS1_STATUS_INVALID_ARGUMENT;
        }
    }

    return GS1_STATUS_OK;
}

Gs1Status GameRuntime::dispatch_subscribed_feedback_event(const Gs1FeedbackEvent& event)
{
    if (!is_valid_feedback_event_type(event.type))
    {
        return GS1_STATUS_INVALID_ARGUMENT;
    }

    const auto& subscribers = feedback_event_subscribers_[feedback_event_type_index(event.type)];
    for (const auto subscriber : subscribers)
    {
        (void)subscriber;
    }

    return GS1_STATUS_OK;
}

void GameRuntime::sync_after_processed_command(const GameCommand& command)
{
    switch (command.type)
    {
    case GameCommandType::OpenMainMenu:
        queue_close_active_normal_ui_if_open();
        queue_app_state_command(app_state_);
        queue_main_menu_ui_commands();
        queue_log_command("Entered main menu.");
        break;

    case GameCommandType::StartNewCampaign:
        queue_close_ui_setup_if_open(GS1_UI_SETUP_MAIN_MENU);
        queue_app_state_command(app_state_);
        queue_regional_map_snapshot_commands();
        queue_regional_map_selection_ui_commands();
        queue_log_command("Started new GS1 campaign.");
        break;

    case GameCommandType::SelectDeploymentSite:
        queue_regional_map_snapshot_commands();
        queue_regional_map_selection_ui_commands();
        queue_log_command("Selected deployment site.");
        break;

    case GameCommandType::ClearDeploymentSiteSelection:
        queue_regional_map_snapshot_commands();
        queue_close_ui_setup_if_open(GS1_UI_SETUP_REGIONAL_MAP_SELECTION);
        queue_log_command("Cleared deployment site selection.");
        break;

    case GameCommandType::StartSiteAttempt:
        queue_close_ui_setup_if_open(GS1_UI_SETUP_REGIONAL_MAP_SELECTION);
        queue_app_state_command(app_state_);
        break;

    case GameCommandType::SiteRunStarted:
        queue_site_bootstrap_commands();
        queue_site_action_update_command();
        queue_hud_state_command();
        queue_log_command("Started site attempt.");
        break;

    case GameCommandType::ReturnToRegionalMap:
        queue_app_state_command(app_state_);
        queue_close_ui_setup_if_open(GS1_UI_SETUP_SITE_RESULT);
        queue_regional_map_snapshot_commands();
        queue_regional_map_selection_ui_commands();
        break;

    case GameCommandType::SiteAttemptEnded:
    {
        const auto& payload = command.payload_as<SiteAttemptEndedCommand>();
        const auto newly_revealed_site_count =
            active_site_run_.has_value() ? active_site_run_->result_newly_revealed_site_count : 0U;
        queue_app_state_command(app_state_);
        queue_site_result_ui_commands(payload.site_id, payload.result);
        queue_site_result_ready_command(
            payload.site_id,
            payload.result,
            newly_revealed_site_count);
        break;
    }

    case GameCommandType::PresentLog:
    {
        const auto& payload = command.payload_as<PresentLogCommand>();
        queue_log_command(payload.text);
        break;
    }

    case GameCommandType::SiteActionStarted:
    case GameCommandType::SiteActionCompleted:
    case GameCommandType::SiteActionFailed:
        queue_site_action_update_command();
        break;

    case GameCommandType::DeploymentSiteSelectionChanged:
    case GameCommandType::StartSiteAction:
    case GameCommandType::CancelSiteAction:
    case GameCommandType::SiteGroundCoverPlaced:
    case GameCommandType::SiteTilePlantingCompleted:
    case GameCommandType::SiteTileWatered:
    case GameCommandType::SiteTileBurialCleared:
    case GameCommandType::WorkerMeterDeltaRequested:
    case GameCommandType::WorkerMetersChanged:
    case GameCommandType::TileEcologyChanged:
    case GameCommandType::RestorationProgressChanged:
    case GameCommandType::TaskAcceptRequested:
    case GameCommandType::TaskRewardClaimRequested:
    case GameCommandType::PhoneListingPurchaseRequested:
    case GameCommandType::PhoneListingSaleRequested:
    case GameCommandType::InventoryDeliveryRequested:
    case GameCommandType::InventoryItemUseRequested:
    case GameCommandType::InventoryItemConsumeRequested:
    case GameCommandType::InventoryTransferRequested:
    case GameCommandType::ContractorHireRequested:
    case GameCommandType::SiteUnlockablePurchaseRequested:
    default:
        break;
    }
}

void GameRuntime::queue_log_command(const char* message)
{
    auto command = make_engine_command(GS1_ENGINE_COMMAND_LOG_TEXT);
    auto& payload = command.emplace_payload<Gs1EngineCommandLogTextData>();
    payload.level = GS1_LOG_LEVEL_INFO;
    strncpy_s(
        payload.text,
        sizeof(payload.text),
        message,
        _TRUNCATE);
    engine_commands_.push_back(command);
}

void GameRuntime::queue_app_state_command(Gs1AppState app_state)
{
    if (last_emitted_app_state_.has_value() && last_emitted_app_state_.value() == app_state)
    {
        return;
    }

    auto command = make_engine_command(GS1_ENGINE_COMMAND_SET_APP_STATE);
    auto& payload = command.emplace_payload<Gs1EngineCommandSetAppStateData>();
    payload.app_state = app_state;
    engine_commands_.push_back(command);
    last_emitted_app_state_ = app_state;
}

void GameRuntime::queue_ui_setup_begin_command(
    Gs1UiSetupId setup_id,
    Gs1UiSetupPresentationType presentation_type,
    std::uint32_t element_count,
    std::uint32_t context_id)
{
    auto command = make_engine_command(GS1_ENGINE_COMMAND_BEGIN_UI_SETUP);
    auto& payload = command.emplace_payload<Gs1EngineCommandUiSetupData>();
    payload.setup_id = setup_id;
    payload.mode = GS1_PROJECTION_MODE_SNAPSHOT;
    payload.presentation_type = presentation_type;
    payload.element_count = static_cast<std::uint16_t>(element_count);
    payload.context_id = context_id;
    engine_commands_.push_back(command);

    active_ui_setups_[setup_id] = presentation_type;
    if (presentation_type == GS1_UI_SETUP_PRESENTATION_NORMAL)
    {
        active_normal_ui_setup_ = setup_id;
    }
}

void GameRuntime::queue_ui_setup_close_command(
    Gs1UiSetupId setup_id,
    Gs1UiSetupPresentationType presentation_type)
{
    auto command = make_engine_command(GS1_ENGINE_COMMAND_CLOSE_UI_SETUP);
    auto& payload = command.emplace_payload<Gs1EngineCommandCloseUiSetupData>();
    payload.setup_id = setup_id;
    payload.presentation_type = presentation_type;
    engine_commands_.push_back(command);

    active_ui_setups_.erase(setup_id);
    if (active_normal_ui_setup_.has_value() && active_normal_ui_setup_.value() == setup_id)
    {
        active_normal_ui_setup_.reset();
    }
}

void GameRuntime::queue_close_ui_setup_if_open(Gs1UiSetupId setup_id)
{
    const auto it = active_ui_setups_.find(setup_id);
    if (it == active_ui_setups_.end())
    {
        return;
    }

    queue_ui_setup_close_command(setup_id, it->second);
}

void GameRuntime::queue_close_active_normal_ui_if_open()
{
    if (!active_normal_ui_setup_.has_value())
    {
        return;
    }

    queue_close_ui_setup_if_open(active_normal_ui_setup_.value());
}

void GameRuntime::queue_ui_element_command(
    std::uint32_t element_id,
    Gs1UiElementType element_type,
    std::uint32_t flags,
    const Gs1UiAction& action,
    const char* text)
{
    auto command = make_engine_command(GS1_ENGINE_COMMAND_UI_ELEMENT_UPSERT);
    auto& payload = command.emplace_payload<Gs1EngineCommandUiElementData>();
    payload.element_id = element_id;
    payload.element_type = element_type;
    payload.flags = static_cast<std::uint8_t>(flags);
    payload.action = action;
    strncpy_s(
        payload.text,
        sizeof(payload.text),
        text,
        _TRUNCATE);
    engine_commands_.push_back(command);
}

void GameRuntime::queue_ui_setup_end_command()
{
    engine_commands_.push_back(make_engine_command(GS1_ENGINE_COMMAND_END_UI_SETUP));
}

void GameRuntime::queue_clear_ui_setup_commands(Gs1UiSetupId setup_id)
{
    queue_close_ui_setup_if_open(setup_id);
}

void GameRuntime::queue_main_menu_ui_commands()
{
    queue_ui_setup_begin_command(
        GS1_UI_SETUP_MAIN_MENU,
        GS1_UI_SETUP_PRESENTATION_NORMAL,
        1U,
        0U);

    Gs1UiAction action {};
    action.type = GS1_UI_ACTION_START_NEW_CAMPAIGN;
    action.arg0 = 42ULL;
    action.arg1 = 30ULL;
    queue_ui_element_command(
        1U,
        GS1_UI_ELEMENT_BUTTON,
        GS1_UI_ELEMENT_FLAG_PRIMARY,
        action,
        "Start New Campaign");

    queue_ui_setup_end_command();
}

void GameRuntime::queue_regional_map_selection_ui_commands()
{
    if (!campaign_.has_value() || !campaign_->regional_map_state.selected_site_id.has_value())
    {
        return;
    }

    const auto site_id = campaign_->regional_map_state.selected_site_id->value;
    const auto loadout_label_count = visible_loadout_slot_count(campaign_->loadout_planner_state);

    queue_ui_setup_begin_command(
        GS1_UI_SETUP_REGIONAL_MAP_SELECTION,
        GS1_UI_SETUP_PRESENTATION_OVERLAY,
        static_cast<std::uint32_t>(3U + loadout_label_count),
        site_id);

    Gs1UiAction no_action {};
    char label_text[64] {};
    std::snprintf(label_text, sizeof(label_text), "Selected Site %u", static_cast<unsigned>(site_id));
    queue_ui_element_command(
        1U,
        GS1_UI_ELEMENT_LABEL,
        GS1_UI_ELEMENT_FLAG_NONE,
        no_action,
        label_text);

    std::uint32_t next_element_id = 2U;
    for (const auto& slot : campaign_->loadout_planner_state.selected_loadout_slots)
    {
        if (!slot.occupied || slot.item_id.value == 0U || slot.quantity == 0U)
        {
            continue;
        }

        char loadout_text[64] {};
        if (const auto* item_def = find_item_def(slot.item_id); item_def != nullptr)
        {
            std::snprintf(
                loadout_text,
                sizeof(loadout_text),
                "%.*s x%u",
                static_cast<int>(item_def->display_name.size()),
                item_def->display_name.data(),
                static_cast<unsigned>(slot.quantity));
        }
        else
        {
            std::snprintf(
                loadout_text,
                sizeof(loadout_text),
                "Item %u x%u",
                static_cast<unsigned>(slot.item_id.value),
                static_cast<unsigned>(slot.quantity));
        }

        queue_ui_element_command(
            next_element_id++,
            GS1_UI_ELEMENT_LABEL,
            GS1_UI_ELEMENT_FLAG_NONE,
            no_action,
            loadout_text);
    }

    Gs1UiAction deploy_action {};
    deploy_action.type = GS1_UI_ACTION_START_SITE_ATTEMPT;
    deploy_action.target_id = site_id;
    char button_text[64] {};
    std::snprintf(button_text, sizeof(button_text), "Start Site %u", static_cast<unsigned>(site_id));
    queue_ui_element_command(
        next_element_id++,
        GS1_UI_ELEMENT_BUTTON,
        GS1_UI_ELEMENT_FLAG_PRIMARY,
        deploy_action,
        button_text);

    Gs1UiAction clear_selection_action {};
    clear_selection_action.type = GS1_UI_ACTION_CLEAR_DEPLOYMENT_SITE_SELECTION;
    queue_ui_element_command(
        next_element_id,
        GS1_UI_ELEMENT_BUTTON,
        GS1_UI_ELEMENT_FLAG_BACKGROUND_CLICK,
        clear_selection_action,
        "");

    queue_ui_setup_end_command();
}

void GameRuntime::queue_site_result_ui_commands(std::uint32_t site_id, Gs1SiteAttemptResult result)
{
    queue_ui_setup_begin_command(
        GS1_UI_SETUP_SITE_RESULT,
        GS1_UI_SETUP_PRESENTATION_OVERLAY,
        2U,
        site_id);

    Gs1UiAction no_action {};
    char label_text[64] {};
    const char* result_text = result == GS1_SITE_ATTEMPT_RESULT_COMPLETED ? "completed" : "failed";
    std::snprintf(
        label_text,
        sizeof(label_text),
        "Site %u %s",
        static_cast<unsigned>(site_id),
        result_text);
    queue_ui_element_command(
        1U,
        GS1_UI_ELEMENT_LABEL,
        GS1_UI_ELEMENT_FLAG_NONE,
        no_action,
        label_text);

    Gs1UiAction return_action {};
    return_action.type = GS1_UI_ACTION_RETURN_TO_REGIONAL_MAP;
    queue_ui_element_command(
        2U,
        GS1_UI_ELEMENT_BUTTON,
        GS1_UI_ELEMENT_FLAG_PRIMARY,
        return_action,
        "Return To Regional Map");

    queue_ui_setup_end_command();
}

void GameRuntime::queue_regional_map_snapshot_commands()
{
    if (!campaign_.has_value())
    {
        return;
    }

    auto begin = make_engine_command(GS1_ENGINE_COMMAND_BEGIN_REGIONAL_MAP_SNAPSHOT);
    auto& begin_payload = begin.emplace_payload<Gs1EngineCommandRegionalMapSnapshotData>();
    begin_payload.mode = GS1_PROJECTION_MODE_SNAPSHOT;
    begin_payload.site_count = static_cast<std::uint32_t>(campaign_->sites.size());

    std::set<std::pair<std::uint32_t, std::uint32_t>> unique_links;
    for (const auto& site : campaign_->sites)
    {
        for (const auto& adjacent_site_id : site.adjacent_site_ids)
        {
            const auto low = (site.site_id.value < adjacent_site_id.value) ? site.site_id.value : adjacent_site_id.value;
            const auto high = (site.site_id.value < adjacent_site_id.value) ? adjacent_site_id.value : site.site_id.value;
            unique_links.insert({low, high});
        }
    }

    begin_payload.link_count = static_cast<std::uint32_t>(unique_links.size());
    begin_payload.selected_site_id =
        campaign_->regional_map_state.selected_site_id.has_value()
            ? campaign_->regional_map_state.selected_site_id->value
            : 0U;
    engine_commands_.push_back(begin);

    for (const auto& site : campaign_->sites)
    {
        queue_regional_map_site_upsert_command(site);
    }

    for (const auto& link : unique_links)
    {
        auto link_command = make_engine_command(GS1_ENGINE_COMMAND_REGIONAL_MAP_LINK_UPSERT);
        auto& payload = link_command.emplace_payload<Gs1EngineCommandRegionalMapLinkData>();
        payload.from_site_id = link.first;
        payload.to_site_id = link.second;
        engine_commands_.push_back(link_command);
    }

    engine_commands_.push_back(make_engine_command(GS1_ENGINE_COMMAND_END_REGIONAL_MAP_SNAPSHOT));
}

void GameRuntime::queue_regional_map_site_upsert_command(const SiteMetaState& site)
{
    if (!campaign_.has_value())
    {
        return;
    }

    auto site_command = make_engine_command(GS1_ENGINE_COMMAND_REGIONAL_MAP_SITE_UPSERT);
    auto& payload = site_command.emplace_payload<Gs1EngineCommandRegionalMapSiteData>();
    payload.site_id = site.site_id.value;
    payload.site_state = site.site_state;
    payload.site_archetype_id = site.site_archetype_id;
    payload.flags =
        (campaign_->regional_map_state.selected_site_id.has_value() &&
            campaign_->regional_map_state.selected_site_id->value == site.site_id.value)
        ? 1U
        : 0U;

    std::size_t map_index = 0;
    for (; map_index < campaign_->sites.size(); ++map_index)
    {
        if (campaign_->sites[map_index].site_id == site.site_id)
        {
            break;
        }
    }

    payload.map_x = static_cast<std::int32_t>(map_index * 160);
    payload.map_y = 0;
    payload.support_package_id =
        site.has_support_package_id ? site.support_package_id : 0U;
    payload.support_preview_mask = 0U;
    engine_commands_.push_back(site_command);
}

void GameRuntime::queue_site_snapshot_begin_command(Gs1ProjectionMode mode)
{
    if (!active_site_run_.has_value())
    {
        return;
    }

    auto begin = make_engine_command(GS1_ENGINE_COMMAND_BEGIN_SITE_SNAPSHOT);
    auto& begin_payload = begin.emplace_payload<Gs1EngineCommandSiteSnapshotData>();
    begin_payload.mode = mode;
    begin_payload.site_id = active_site_run_->site_id.value;
    begin_payload.site_archetype_id = active_site_run_->site_archetype_id;
    begin_payload.width = static_cast<std::uint16_t>(site_world_access::width(active_site_run_.value()));
    begin_payload.height = static_cast<std::uint16_t>(site_world_access::height(active_site_run_.value()));
    engine_commands_.push_back(begin);
}

void GameRuntime::queue_site_snapshot_end_command()
{
    engine_commands_.push_back(make_engine_command(GS1_ENGINE_COMMAND_END_SITE_SNAPSHOT));
}

void GameRuntime::queue_site_tile_upsert_command(std::uint32_t x, std::uint32_t y)
{
    if (!active_site_run_.has_value())
    {
        return;
    }

    auto& site_run = active_site_run_.value();
    const TileCoord coord {
        static_cast<std::int32_t>(x),
        static_cast<std::int32_t>(y)};

    if (!site_world_access::tile_coord_in_bounds(site_run, coord))
    {
        return;
    }

    const auto tile = site_run.site_world->tile_at(coord);

    auto tile_command = make_engine_command(GS1_ENGINE_COMMAND_SITE_TILE_UPSERT);
    auto& payload = tile_command.emplace_payload<Gs1EngineCommandSiteTileData>();
    payload.x = x;
    payload.y = y;
    payload.terrain_type_id = tile.static_data.terrain_type_id;
    payload.plant_type_id = tile.ecology.plant_id.value;
    payload.structure_type_id = tile.device.structure_id.value;
    payload.ground_cover_type_id = tile.ecology.ground_cover_type_id;
    payload.plant_density = tile.ecology.plant_density;
    payload.sand_burial = tile.ecology.sand_burial;
    engine_commands_.push_back(tile_command);
}

void GameRuntime::queue_all_site_tile_upsert_commands()
{
    if (!active_site_run_.has_value())
    {
        return;
    }

    const auto width = site_world_access::width(active_site_run_.value());
    const auto height = site_world_access::height(active_site_run_.value());
    for (std::uint32_t y = 0; y < height; ++y)
    {
        for (std::uint32_t x = 0; x < width; ++x)
        {
            queue_site_tile_upsert_command(x, y);
        }
    }
}

void GameRuntime::queue_pending_site_tile_upsert_commands()
{
    if (!active_site_run_.has_value())
    {
        return;
    }

    auto& site_run = active_site_run_.value();
    if (site_run.pending_full_tile_projection_update || site_run.pending_tile_projection_updates.empty())
    {
        queue_all_site_tile_upsert_commands();
        return;
    }

    std::sort(
        site_run.pending_tile_projection_updates.begin(),
        site_run.pending_tile_projection_updates.end(),
        [](const TileCoord& lhs, const TileCoord& rhs) {
            if (lhs.y != rhs.y)
            {
                return lhs.y < rhs.y;
            }
            return lhs.x < rhs.x;
        });

    for (const auto& coord : site_run.pending_tile_projection_updates)
    {
        queue_site_tile_upsert_command(
            static_cast<std::uint32_t>(coord.x),
            static_cast<std::uint32_t>(coord.y));
    }
}

void GameRuntime::queue_site_worker_update_command()
{
    if (!active_site_run_.has_value())
    {
        return;
    }

    const auto worker_position = site_world_access::worker_position(active_site_run_.value());
    const auto worker_conditions = site_world_access::worker_conditions(active_site_run_.value());

    auto worker_command = make_engine_command(GS1_ENGINE_COMMAND_SITE_WORKER_UPDATE);
    auto& worker_payload = worker_command.emplace_payload<Gs1EngineCommandWorkerData>();
    worker_payload.tile_x = worker_position.tile_x;
    worker_payload.tile_y = worker_position.tile_y;
    worker_payload.facing_degrees = worker_position.facing_degrees;
    worker_payload.health_normalized = worker_conditions.health / 100.0f;
    worker_payload.hydration_normalized = worker_conditions.hydration / 100.0f;
    worker_payload.energy_normalized =
        worker_conditions.energy_cap > 0.0f
            ? (worker_conditions.energy / worker_conditions.energy_cap)
            : 0.0f;
    worker_payload.current_action_kind =
        static_cast<Gs1SiteActionKind>(active_site_run_->site_action.action_kind);
    engine_commands_.push_back(worker_command);
}

void GameRuntime::queue_site_camp_update_command()
{
    if (!active_site_run_.has_value())
    {
        return;
    }

    auto camp_command = make_engine_command(GS1_ENGINE_COMMAND_SITE_CAMP_UPDATE);
    auto& camp_payload = camp_command.emplace_payload<Gs1EngineCommandCampData>();
    camp_payload.tile_x = active_site_run_->camp.camp_anchor_tile.x;
    camp_payload.tile_y = active_site_run_->camp.camp_anchor_tile.y;
    camp_payload.durability_normalized = active_site_run_->camp.camp_durability / 100.0f;
    camp_payload.flags =
        (active_site_run_->camp.delivery_point_operational ? 1U : 0U) |
        (active_site_run_->camp.shared_camp_storage_access_enabled ? 2U : 0U);
    engine_commands_.push_back(camp_command);
}

void GameRuntime::queue_site_weather_update_command()
{
    if (!active_site_run_.has_value())
    {
        return;
    }

    auto weather_command = make_engine_command(GS1_ENGINE_COMMAND_SITE_WEATHER_UPDATE);
    auto& weather_payload = weather_command.emplace_payload<Gs1EngineCommandWeatherData>();
    weather_payload.heat = active_site_run_->weather.weather_heat;
    weather_payload.wind = active_site_run_->weather.weather_wind;
    weather_payload.dust = active_site_run_->weather.weather_dust;
    weather_payload.event_template_id =
        active_site_run_->event.active_event_template_id.has_value()
            ? active_site_run_->event.active_event_template_id->value
            : 0U;
    weather_payload.event_phase =
        static_cast<Gs1WeatherEventPhase>(active_site_run_->event.event_phase);
    weather_payload.phase_minutes_remaining =
        static_cast<float>(active_site_run_->event.phase_minutes_remaining);
    engine_commands_.push_back(weather_command);
}

void GameRuntime::queue_site_inventory_slot_upsert_command(
    Gs1InventoryContainerKind container_kind,
    std::uint32_t slot_index,
    std::uint32_t container_owner_id,
    TileCoord container_tile)
{
    if (!active_site_run_.has_value())
    {
        return;
    }

    InventorySlot slot {};
    if (container_kind == GS1_INVENTORY_CONTAINER_WORKER_PACK)
    {
        if (slot_index >= active_site_run_->inventory.worker_pack_slots.size())
        {
            return;
        }

        slot = active_site_run_->inventory.worker_pack_slots[slot_index];
    }
    else if (container_kind == GS1_INVENTORY_CONTAINER_CAMP_STORAGE)
    {
        if (slot_index >= active_site_run_->inventory.camp_storage_slots.size())
        {
            return;
        }

        slot = active_site_run_->inventory.camp_storage_slots[slot_index];
        container_tile = inventory_storage::container_tile_coord(
            active_site_run_.value(),
            inventory_storage::camp_storage_container(active_site_run_.value()));
    }
    else if (container_kind == GS1_INVENTORY_CONTAINER_DEVICE_STORAGE)
    {
        auto container = inventory_storage::container_for_kind(
            active_site_run_.value(),
            container_kind,
            container_owner_id);
        if (!container.is_valid())
        {
            return;
        }

        container_tile = inventory_storage::container_tile_coord(active_site_run_.value(), container);
        const auto slot_entity = inventory_storage::slot_entity(active_site_run_.value(), container, slot_index);
        const auto item_entity = inventory_storage::item_entity_for_slot(active_site_run_.value(), slot_entity);
        inventory_storage::fill_projection_slot_from_entities(slot, item_entity);
    }
    else
    {
        return;
    }

    auto command = make_engine_command(GS1_ENGINE_COMMAND_SITE_INVENTORY_SLOT_UPSERT);
    auto& payload = command.emplace_payload<Gs1EngineCommandInventorySlotData>();
    payload.item_id = slot.item_id.value;
    payload.condition = slot.item_condition;
    payload.freshness = slot.item_freshness;
    payload.container_owner_id = container_owner_id;
    payload.quantity = static_cast<std::uint16_t>(std::min<std::uint32_t>(slot.item_quantity, 65535U));
    payload.slot_index = static_cast<std::uint16_t>(slot_index);
    payload.container_tile_x = static_cast<std::int16_t>(container_tile.x);
    payload.container_tile_y = static_cast<std::int16_t>(container_tile.y);
    payload.container_kind = container_kind;
    payload.flags = slot.occupied ? 1U : 0U;
    engine_commands_.push_back(command);
}

void GameRuntime::queue_all_site_inventory_slot_upsert_commands()
{
    if (!active_site_run_.has_value())
    {
        return;
    }

    for (std::uint32_t index = 0; index < active_site_run_->inventory.worker_pack_slots.size(); ++index)
    {
        queue_site_inventory_slot_upsert_command(GS1_INVENTORY_CONTAINER_WORKER_PACK, index);
    }

    for (std::uint32_t index = 0; index < active_site_run_->inventory.camp_storage_slots.size(); ++index)
    {
        queue_site_inventory_slot_upsert_command(GS1_INVENTORY_CONTAINER_CAMP_STORAGE, index);
    }

    using namespace site_ecs;
    const auto containers = inventory_storage::collect_all_storage_containers(active_site_run_.value(), true);
    for (const auto& container : containers)
    {
        if (!container.is_valid() || !container.has<StorageContainerKindComponent>())
        {
            continue;
        }

        if (container.get<StorageContainerKindComponent>().kind != StorageContainerKind::DeviceStorage)
        {
            continue;
        }

        const auto owner_id =
            inventory_storage::owner_device_entity_id_for_container(active_site_run_.value(), container);
        const auto container_tile =
            inventory_storage::container_tile_coord(active_site_run_.value(), container);
        const auto slot_count = inventory_storage::slot_count_in_container(active_site_run_.value(), container);
        for (std::uint32_t slot_index = 0U; slot_index < slot_count; ++slot_index)
        {
            queue_site_inventory_slot_upsert_command(
                GS1_INVENTORY_CONTAINER_DEVICE_STORAGE,
                slot_index,
                owner_id,
                container_tile);
        }
    }
}

void GameRuntime::queue_pending_site_inventory_slot_upsert_commands()
{
    if (!active_site_run_.has_value())
    {
        return;
    }

    auto& site_run = active_site_run_.value();
    if (site_run.pending_full_inventory_projection_update ||
        (site_run.pending_worker_pack_inventory_projection_updates.empty() &&
            site_run.pending_camp_storage_inventory_projection_updates.empty()))
    {
        queue_all_site_inventory_slot_upsert_commands();
        return;
    }

    std::sort(
        site_run.pending_worker_pack_inventory_projection_updates.begin(),
        site_run.pending_worker_pack_inventory_projection_updates.end());
    for (const auto slot_index : site_run.pending_worker_pack_inventory_projection_updates)
    {
        queue_site_inventory_slot_upsert_command(GS1_INVENTORY_CONTAINER_WORKER_PACK, slot_index);
    }

    std::sort(
        site_run.pending_camp_storage_inventory_projection_updates.begin(),
        site_run.pending_camp_storage_inventory_projection_updates.end());
    for (const auto slot_index : site_run.pending_camp_storage_inventory_projection_updates)
    {
        queue_site_inventory_slot_upsert_command(GS1_INVENTORY_CONTAINER_CAMP_STORAGE, slot_index);
    }
}

void GameRuntime::queue_site_task_upsert_command(std::size_t task_index)
{
    if (!active_site_run_.has_value())
    {
        return;
    }

    const auto& tasks = active_site_run_->task_board.visible_tasks;
    if (task_index >= tasks.size())
    {
        return;
    }

    const auto& task = tasks[task_index];
    auto command = make_engine_command(GS1_ENGINE_COMMAND_SITE_TASK_UPSERT);
    auto& payload = command.emplace_payload<Gs1EngineCommandTaskData>();
    payload.task_instance_id = task.task_instance_id.value;
    payload.task_template_id = task.task_template_id.value;
    payload.publisher_faction_id = task.publisher_faction_id.value;
    payload.current_progress = static_cast<std::uint16_t>(std::min<std::uint32_t>(task.current_progress_amount, 65535U));
    payload.target_progress = static_cast<std::uint16_t>(std::min<std::uint32_t>(task.target_amount, 65535U));
    payload.list_kind = to_task_presentation_list_kind(task.runtime_list_kind);
    payload.flags = 1U;
    engine_commands_.push_back(command);
}

void GameRuntime::queue_all_site_task_upsert_commands()
{
    if (!active_site_run_.has_value())
    {
        return;
    }

    for (std::size_t index = 0; index < active_site_run_->task_board.visible_tasks.size(); ++index)
    {
        queue_site_task_upsert_command(index);
    }
}

void GameRuntime::queue_site_phone_listing_upsert_command(std::size_t listing_index)
{
    if (!active_site_run_.has_value())
    {
        return;
    }

    const auto& listings = active_site_run_->economy.available_phone_listings;
    if (listing_index >= listings.size())
    {
        return;
    }

    const auto& listing = listings[listing_index];
    auto command = make_engine_command(GS1_ENGINE_COMMAND_SITE_PHONE_LISTING_UPSERT);
    auto& payload = command.emplace_payload<Gs1EngineCommandPhoneListingData>();
    payload.listing_id = listing.listing_id;
    payload.item_or_unlockable_id = listing.item_id.value;
    payload.price = listing.price;
    payload.related_site_id = active_site_run_->site_id.value;
    payload.quantity = static_cast<std::uint16_t>(std::min<std::uint32_t>(listing.quantity, 65535U));
    payload.listing_kind = to_phone_listing_presentation_kind(listing.kind);
    payload.flags = listing.occupied ? 1U : 0U;
    engine_commands_.push_back(command);
}

void GameRuntime::queue_all_site_phone_listing_upsert_commands()
{
    if (!active_site_run_.has_value())
    {
        return;
    }

    for (std::size_t index = 0; index < active_site_run_->economy.available_phone_listings.size(); ++index)
    {
        queue_site_phone_listing_upsert_command(index);
    }
}

void GameRuntime::queue_site_bootstrap_commands()
{
    if (!active_site_run_.has_value())
    {
        return;
    }

    queue_site_snapshot_begin_command(GS1_PROJECTION_MODE_SNAPSHOT);
    queue_all_site_tile_upsert_commands();
    queue_site_worker_update_command();
    queue_site_camp_update_command();
    queue_site_weather_update_command();
    queue_all_site_inventory_slot_upsert_commands();
    queue_all_site_task_upsert_commands();
    queue_all_site_phone_listing_upsert_commands();
    queue_site_snapshot_end_command();

    active_site_run_->pending_projection_update_flags = 0U;
    clear_pending_site_tile_projection_updates();
    clear_pending_site_inventory_projection_updates();
}

void GameRuntime::queue_site_delta_commands(std::uint64_t dirty_flags)
{
    if (!active_site_run_.has_value())
    {
        return;
    }

    const auto site_dirty_flags =
        dirty_flags & (SITE_PROJECTION_UPDATE_TILES |
            SITE_PROJECTION_UPDATE_WORKER |
            SITE_PROJECTION_UPDATE_CAMP |
            SITE_PROJECTION_UPDATE_WEATHER |
            SITE_PROJECTION_UPDATE_INVENTORY |
            SITE_PROJECTION_UPDATE_TASKS |
            SITE_PROJECTION_UPDATE_PHONE);
    if (site_dirty_flags == 0U)
    {
        return;
    }

    queue_site_snapshot_begin_command(GS1_PROJECTION_MODE_DELTA);

    if ((site_dirty_flags & SITE_PROJECTION_UPDATE_TILES) != 0U)
    {
        queue_pending_site_tile_upsert_commands();
    }

    if ((site_dirty_flags & SITE_PROJECTION_UPDATE_WORKER) != 0U)
    {
        queue_site_worker_update_command();
    }

    if ((site_dirty_flags & SITE_PROJECTION_UPDATE_CAMP) != 0U)
    {
        queue_site_camp_update_command();
    }

    if ((site_dirty_flags & SITE_PROJECTION_UPDATE_WEATHER) != 0U)
    {
        queue_site_weather_update_command();
    }

    if ((site_dirty_flags & SITE_PROJECTION_UPDATE_INVENTORY) != 0U)
    {
        queue_pending_site_inventory_slot_upsert_commands();
    }

    if ((site_dirty_flags & SITE_PROJECTION_UPDATE_TASKS) != 0U)
    {
        queue_all_site_task_upsert_commands();
    }

    if ((site_dirty_flags & SITE_PROJECTION_UPDATE_PHONE) != 0U)
    {
        queue_all_site_phone_listing_upsert_commands();
    }

    queue_site_snapshot_end_command();
}

void GameRuntime::queue_site_action_update_command()
{
    if (!active_site_run_.has_value())
    {
        return;
    }

    const auto& action_state = active_site_run_->site_action;
    auto command = make_engine_command(GS1_ENGINE_COMMAND_SITE_ACTION_UPDATE);
    auto& payload = command.emplace_payload<Gs1EngineCommandSiteActionData>();

    if (action_has_started(action_state))
    {
        const TileCoord target_tile = action_state.target_tile.value_or(TileCoord {});
        payload.action_id = action_state.current_action_id->value;
        payload.target_tile_x = target_tile.x;
        payload.target_tile_y = target_tile.y;
        payload.action_kind = static_cast<Gs1SiteActionKind>(action_state.action_kind);
        payload.flags = GS1_SITE_ACTION_PRESENTATION_FLAG_ACTIVE;
        payload.progress_normalized = action_progress_normalized(action_state);
        payload.duration_minutes = static_cast<float>(action_state.total_action_minutes);
    }
    else
    {
        payload.action_id = 0U;
        payload.target_tile_x = 0;
        payload.target_tile_y = 0;
        payload.action_kind = GS1_SITE_ACTION_NONE;
        payload.flags = GS1_SITE_ACTION_PRESENTATION_FLAG_CLEAR;
        payload.progress_normalized = 0.0f;
        payload.duration_minutes = 0.0f;
    }

    engine_commands_.push_back(command);
}

void GameRuntime::queue_hud_state_command()
{
    if (!active_site_run_.has_value())
    {
        return;
    }

    const auto worker_conditions = site_world_access::worker_conditions(active_site_run_.value());

    auto hud_command = make_engine_command(GS1_ENGINE_COMMAND_HUD_STATE);
    auto& payload = hud_command.emplace_payload<Gs1EngineCommandHudStateData>();
    payload.player_health = worker_conditions.health;
    payload.player_hydration = worker_conditions.hydration;
    payload.player_energy = worker_conditions.energy;
    payload.current_money = active_site_run_->economy.money;
    payload.active_task_count =
        static_cast<std::uint16_t>(active_site_run_->task_board.accepted_task_ids.size());
    payload.current_action_kind =
        static_cast<Gs1SiteActionKind>(active_site_run_->site_action.action_kind);
    payload.site_completion_normalized =
        active_site_run_->counters.site_completion_tile_threshold > 0U
            ? static_cast<float>(active_site_run_->counters.fully_grown_tile_count) /
                static_cast<float>(active_site_run_->counters.site_completion_tile_threshold)
            : 0.0f;
    engine_commands_.push_back(hud_command);
}

void GameRuntime::queue_site_result_ready_command(
    std::uint32_t site_id,
    Gs1SiteAttemptResult result,
    std::uint32_t newly_revealed_site_count)
{
    auto command = make_engine_command(GS1_ENGINE_COMMAND_SITE_RESULT_READY);
    auto& payload = command.emplace_payload<Gs1EngineCommandSiteResultData>();
    payload.site_id = site_id;
    payload.result = result;
    payload.newly_revealed_site_count = static_cast<std::uint16_t>(newly_revealed_site_count);
    engine_commands_.push_back(command);
}

void GameRuntime::mark_site_projection_update_dirty(std::uint64_t dirty_flags) noexcept
{
    if (!active_site_run_.has_value())
    {
        return;
    }

    if ((dirty_flags & SITE_PROJECTION_UPDATE_TILES) != 0U)
    {
        active_site_run_->pending_full_tile_projection_update = true;
    }
    if ((dirty_flags & SITE_PROJECTION_UPDATE_INVENTORY) != 0U)
    {
        active_site_run_->pending_full_inventory_projection_update = true;
    }

    active_site_run_->pending_projection_update_flags |= dirty_flags;
}

void GameRuntime::mark_site_tile_projection_dirty(TileCoord coord) noexcept
{
    if (!active_site_run_.has_value())
    {
        return;
    }

    auto& site_run = active_site_run_.value();
    if (!site_world_access::tile_coord_in_bounds(site_run, coord))
    {
        return;
    }

    const auto tile_count = site_world_access::tile_count(site_run);
    if (site_run.pending_tile_projection_update_mask.size() != tile_count)
    {
        site_run.pending_tile_projection_update_mask.assign(tile_count, 0U);
        site_run.pending_tile_projection_updates.clear();
    }

    const auto index = site_world_access::tile_index(site_run, coord);
    if (site_run.pending_tile_projection_update_mask[index] == 0U)
    {
        site_run.pending_tile_projection_update_mask[index] = 1U;
        site_run.pending_tile_projection_updates.push_back(coord);
    }

    site_run.pending_projection_update_flags |= SITE_PROJECTION_UPDATE_TILES;
}

void GameRuntime::clear_pending_site_tile_projection_updates() noexcept
{
    if (!active_site_run_.has_value())
    {
        return;
    }

    auto& site_run = active_site_run_.value();
    for (const auto& coord : site_run.pending_tile_projection_updates)
    {
        if (!site_world_access::tile_coord_in_bounds(site_run, coord))
        {
            continue;
        }

        const auto index = site_world_access::tile_index(site_run, coord);
        if (index < site_run.pending_tile_projection_update_mask.size())
        {
            site_run.pending_tile_projection_update_mask[index] = 0U;
        }
    }

    site_run.pending_tile_projection_updates.clear();
    site_run.pending_full_tile_projection_update = false;
}

void GameRuntime::clear_pending_site_inventory_projection_updates() noexcept
{
    if (!active_site_run_.has_value())
    {
        return;
    }

    auto& site_run = active_site_run_.value();

    for (const auto slot_index : site_run.pending_worker_pack_inventory_projection_updates)
    {
        if (slot_index < site_run.pending_worker_pack_inventory_projection_update_mask.size())
        {
            site_run.pending_worker_pack_inventory_projection_update_mask[slot_index] = 0U;
        }
    }
    site_run.pending_worker_pack_inventory_projection_updates.clear();

    for (const auto slot_index : site_run.pending_camp_storage_inventory_projection_updates)
    {
        if (slot_index < site_run.pending_camp_storage_inventory_projection_update_mask.size())
        {
            site_run.pending_camp_storage_inventory_projection_update_mask[slot_index] = 0U;
        }
    }
    site_run.pending_camp_storage_inventory_projection_updates.clear();

    site_run.pending_full_inventory_projection_update = false;
}

void GameRuntime::flush_site_presentation_if_dirty()
{
    if (!active_site_run_.has_value())
    {
        return;
    }

    const auto dirty_flags = active_site_run_->pending_projection_update_flags;
    if (dirty_flags == 0U)
    {
        return;
    }

    queue_site_delta_commands(dirty_flags);

    if ((dirty_flags & SITE_PROJECTION_UPDATE_HUD) != 0U)
    {
        queue_hud_state_command();
    }

    active_site_run_->pending_projection_update_flags = 0U;
    clear_pending_site_tile_projection_updates();
    clear_pending_site_inventory_projection_updates();
}

Gs1Status GameRuntime::translate_ui_action_to_command(const Gs1UiAction& action, GameCommand& out_command) const
{
    switch (action.type)
    {
    case GS1_UI_ACTION_START_NEW_CAMPAIGN:
        if (action.arg1 > static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max()))
        {
            return GS1_STATUS_INVALID_ARGUMENT;
        }
        out_command.type = GameCommandType::StartNewCampaign;
        out_command.set_payload(StartNewCampaignCommand {
            action.arg0,
            static_cast<std::uint32_t>(action.arg1)});
        return GS1_STATUS_OK;

    case GS1_UI_ACTION_SELECT_DEPLOYMENT_SITE:
        if (action.target_id == 0U)
        {
            return GS1_STATUS_INVALID_ARGUMENT;
        }
        out_command.type = GameCommandType::SelectDeploymentSite;
        out_command.set_payload(SelectDeploymentSiteCommand {action.target_id});
        return GS1_STATUS_OK;

    case GS1_UI_ACTION_CLEAR_DEPLOYMENT_SITE_SELECTION:
        out_command.type = GameCommandType::ClearDeploymentSiteSelection;
        out_command.set_payload(ClearDeploymentSiteSelectionCommand {});
        return GS1_STATUS_OK;

    case GS1_UI_ACTION_START_SITE_ATTEMPT:
        if (action.target_id == 0U)
        {
            return GS1_STATUS_INVALID_ARGUMENT;
        }
        out_command.type = GameCommandType::StartSiteAttempt;
        out_command.set_payload(StartSiteAttemptCommand {action.target_id});
        return GS1_STATUS_OK;

    case GS1_UI_ACTION_RETURN_TO_REGIONAL_MAP:
        out_command.type = GameCommandType::ReturnToRegionalMap;
        out_command.set_payload(ReturnToRegionalMapCommand {});
        return GS1_STATUS_OK;

    case GS1_UI_ACTION_ACCEPT_TASK:
        if (action.target_id == 0U)
        {
            return GS1_STATUS_INVALID_ARGUMENT;
        }
        out_command.type = GameCommandType::TaskAcceptRequested;
        out_command.set_payload(TaskAcceptRequestedCommand {action.target_id});
        return GS1_STATUS_OK;

    case GS1_UI_ACTION_CLAIM_TASK_REWARD:
        if (action.target_id == 0U ||
            action.arg0 > static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max()))
        {
            return GS1_STATUS_INVALID_ARGUMENT;
        }
        out_command.type = GameCommandType::TaskRewardClaimRequested;
        out_command.set_payload(TaskRewardClaimRequestedCommand {
            action.target_id,
            static_cast<std::uint32_t>(action.arg0)});
        return GS1_STATUS_OK;

    case GS1_UI_ACTION_BUY_PHONE_LISTING:
        if (action.target_id == 0U ||
            action.arg0 > static_cast<std::uint64_t>(std::numeric_limits<std::uint16_t>::max()))
        {
            return GS1_STATUS_INVALID_ARGUMENT;
        }
        out_command.type = GameCommandType::PhoneListingPurchaseRequested;
        out_command.set_payload(PhoneListingPurchaseRequestedCommand {
            action.target_id,
            static_cast<std::uint16_t>(action.arg0 == 0ULL ? 1ULL : action.arg0),
            0U});
        return GS1_STATUS_OK;

    case GS1_UI_ACTION_SELL_PHONE_LISTING:
        if (action.target_id == 0U ||
            action.arg0 > static_cast<std::uint64_t>(std::numeric_limits<std::uint16_t>::max()))
        {
            return GS1_STATUS_INVALID_ARGUMENT;
        }
        out_command.type = GameCommandType::PhoneListingSaleRequested;
        out_command.set_payload(PhoneListingSaleRequestedCommand {
            action.target_id,
            static_cast<std::uint16_t>(action.arg0 == 0ULL ? 1ULL : action.arg0),
            0U});
        return GS1_STATUS_OK;

    case GS1_UI_ACTION_HIRE_CONTRACTOR:
        if (action.target_id == 0U ||
            action.arg0 > static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max()))
        {
            return GS1_STATUS_INVALID_ARGUMENT;
        }
        out_command.type = GameCommandType::ContractorHireRequested;
        out_command.set_payload(ContractorHireRequestedCommand {
            action.target_id,
            static_cast<std::uint32_t>(action.arg0)});
        return GS1_STATUS_OK;

    case GS1_UI_ACTION_PURCHASE_SITE_UNLOCKABLE:
        if (action.target_id == 0U)
        {
            return GS1_STATUS_INVALID_ARGUMENT;
        }
        out_command.type = GameCommandType::SiteUnlockablePurchaseRequested;
        out_command.set_payload(SiteUnlockablePurchaseRequestedCommand {action.target_id});
        return GS1_STATUS_OK;

    case GS1_UI_ACTION_USE_INVENTORY_ITEM:
        if (action.target_id == 0U)
        {
            return GS1_STATUS_INVALID_ARGUMENT;
        }
        out_command.type = GameCommandType::InventoryItemUseRequested;
        out_command.set_payload(InventoryItemUseRequestedCommand {
            action.target_id,
            static_cast<std::uint16_t>(unpack_inventory_quantity(action.arg0) == 0U
                    ? 1U
                    : unpack_inventory_quantity(action.arg0)),
            unpack_inventory_container(action.arg0),
            static_cast<std::uint8_t>(unpack_inventory_slot_index(action.arg0))});
        return GS1_STATUS_OK;

    case GS1_UI_ACTION_TRANSFER_INVENTORY_ITEM:
        out_command.type = GameCommandType::InventoryTransferRequested;
        out_command.set_payload(InventoryTransferRequestedCommand {
            static_cast<std::uint16_t>(unpack_transfer_source_slot(action.arg0)),
            static_cast<std::uint16_t>(unpack_transfer_destination_slot(action.arg0)),
            static_cast<std::uint16_t>(unpack_transfer_quantity(action.arg0) == 0U
                    ? 1U
                    : unpack_transfer_quantity(action.arg0)),
            unpack_transfer_source_container(action.arg0),
            unpack_transfer_destination_container(action.arg0),
            0U,
            0U,
            unpack_transfer_source_owner(action.arg1),
            unpack_transfer_destination_owner(action.arg1)});
        return GS1_STATUS_OK;

    case GS1_UI_ACTION_NONE:
    default:
        return GS1_STATUS_INVALID_ARGUMENT;
    }
}

Gs1Status GameRuntime::translate_site_action_request_to_command(
    const Gs1HostEventSiteActionRequestData& action,
    GameCommand& out_command) const
{
    if (action.action_kind == GS1_SITE_ACTION_NONE)
    {
        return GS1_STATUS_INVALID_ARGUMENT;
    }

    out_command.type = GameCommandType::StartSiteAction;
    out_command.set_payload(StartSiteActionCommand {
        action.action_kind,
        action.flags,
        action.quantity == 0U ? 1U : action.quantity,
        action.target_tile_x,
        action.target_tile_y,
        action.primary_subject_id,
        action.secondary_subject_id,
        action.item_id});
    return GS1_STATUS_OK;
}

Gs1Status GameRuntime::translate_site_action_cancel_to_command(
    const Gs1HostEventSiteActionCancelData& action,
    GameCommand& out_command) const
{
    if (action.action_id == 0U &&
        (action.flags & GS1_SITE_ACTION_CANCEL_FLAG_CURRENT_ACTION) == 0U)
    {
        return GS1_STATUS_INVALID_ARGUMENT;
    }

    out_command.type = GameCommandType::CancelSiteAction;
    out_command.set_payload(CancelSiteActionCommand {
        action.action_id,
        action.flags});
    return GS1_STATUS_OK;
}

Gs1Status GameRuntime::dispatch_host_events(
    HostEventDispatchStage stage,
    std::uint32_t& out_processed_count)
{
    out_processed_count = 0U;

    while (!host_events_.empty())
    {
        const auto event = host_events_.front();
        host_events_.pop_front();
        out_processed_count += 1U;

        switch (event.type)
        {
        case GS1_HOST_EVENT_UI_ACTION:
        {
            GameCommand command {};
            const auto status = translate_ui_action_to_command(event.payload.ui_action.action, command);
            if (status != GS1_STATUS_OK)
            {
                return status;
            }

            command_queue_.push_back(command);
            const auto dispatch_status = dispatch_queued_commands();
            if (dispatch_status != GS1_STATUS_OK)
            {
                return dispatch_status;
            }
            break;
        }

        case GS1_HOST_EVENT_SITE_MOVE_DIRECTION:
        {
            assert(stage == HostEventDispatchStage::Phase1);
            if (stage != HostEventDispatchStage::Phase1)
            {
                return GS1_STATUS_INVALID_STATE;
            }

            assert(!phase1_site_move_direction_.present);
            if (phase1_site_move_direction_.present)
            {
                return GS1_STATUS_INVALID_STATE;
            }

            if (!active_site_run_.has_value() || app_state_ != GS1_APP_STATE_SITE_ACTIVE)
            {
                break;
            }

            const auto& payload = event.payload.site_move_direction;
            phase1_site_move_direction_.world_move_x = payload.world_move_x;
            phase1_site_move_direction_.world_move_y = payload.world_move_y;
            phase1_site_move_direction_.world_move_z = payload.world_move_z;
            phase1_site_move_direction_.present = true;
            break;
        }

        case GS1_HOST_EVENT_SITE_ACTION_REQUEST:
        {
            GameCommand command {};
            const auto status =
                translate_site_action_request_to_command(event.payload.site_action_request, command);
            if (status != GS1_STATUS_OK)
            {
                return status;
            }

            command_queue_.push_back(command);
            const auto dispatch_status = dispatch_queued_commands();
            if (dispatch_status != GS1_STATUS_OK)
            {
                return dispatch_status;
            }
            break;
        }

        case GS1_HOST_EVENT_SITE_ACTION_CANCEL:
        {
            GameCommand command {};
            const auto status =
                translate_site_action_cancel_to_command(event.payload.site_action_cancel, command);
            if (status != GS1_STATUS_OK)
            {
                return status;
            }

            command_queue_.push_back(command);
            const auto dispatch_status = dispatch_queued_commands();
            if (dispatch_status != GS1_STATUS_OK)
            {
                return dispatch_status;
            }
            break;
        }

        default:
            return GS1_STATUS_INVALID_ARGUMENT;
        }
    }

    return GS1_STATUS_OK;
}

Gs1Status GameRuntime::dispatch_feedback_events(std::uint32_t& out_processed_count)
{
    out_processed_count = 0U;

    while (!feedback_events_.empty())
    {
        const auto event = feedback_events_.front();
        feedback_events_.pop_front();
        out_processed_count += 1U;

        const auto status = dispatch_subscribed_feedback_event(event);
        if (status != GS1_STATUS_OK)
        {
            return status;
        }
    }

    return GS1_STATUS_OK;
}

Gs1Status GameRuntime::dispatch_queued_commands()
{
    return CommandDispatcher::dispatch_all(*this);
}

void GameRuntime::run_fixed_step()
{
    if (!campaign_.has_value() || !active_site_run_.has_value())
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

    run_profiled_system(GS1_RUNTIME_PROFILE_SYSTEM_CAMPAIGN_FLOW, [&]()
    {
        CampaignFixedStepContext campaign_context {*campaign_};
        CampaignFlowSystem::run(campaign_context);
    });

    const SiteMoveDirectionInput move_direction {
        phase1_site_move_direction_.world_move_x,
        phase1_site_move_direction_.world_move_y,
        phase1_site_move_direction_.world_move_z,
        phase1_site_move_direction_.present};

    run_profiled_system(GS1_RUNTIME_PROFILE_SYSTEM_SITE_FLOW, [&]()
    {
        auto site_flow_context = make_site_system_context<SiteFlowSystem>(
            *campaign_,
            *active_site_run_,
            command_queue_,
            fixed_step_seconds_,
            move_direction);
        SiteFlowSystem::run(site_flow_context);
    });

    run_profiled_system(GS1_RUNTIME_PROFILE_SYSTEM_MODIFIER, [&]()
    {
        auto modifier_context = make_site_system_context<ModifierSystem>(
            *campaign_,
            *active_site_run_,
            command_queue_,
            fixed_step_seconds_,
            move_direction);
        ModifierSystem::run(modifier_context);
    });

    run_profiled_system(GS1_RUNTIME_PROFILE_SYSTEM_WEATHER_EVENT, [&]()
    {
        auto weather_event_context = make_site_system_context<WeatherEventSystem>(
            *campaign_,
            *active_site_run_,
            command_queue_,
            fixed_step_seconds_,
            move_direction);
        WeatherEventSystem::run(weather_event_context);
    });

    run_profiled_system(GS1_RUNTIME_PROFILE_SYSTEM_ACTION_EXECUTION, [&]()
    {
        auto action_execution_context = make_site_system_context<ActionExecutionSystem>(
            *campaign_,
            *active_site_run_,
            command_queue_,
            fixed_step_seconds_,
            move_direction);
        ActionExecutionSystem::run(action_execution_context);
    });

    run_profiled_system(GS1_RUNTIME_PROFILE_SYSTEM_LOCAL_WEATHER_RESOLVE, [&]()
    {
        auto local_weather_context = make_site_system_context<LocalWeatherResolveSystem>(
            *campaign_,
            *active_site_run_,
            command_queue_,
            fixed_step_seconds_,
            move_direction);
        LocalWeatherResolveSystem::run(local_weather_context);
    });

    run_profiled_system(GS1_RUNTIME_PROFILE_SYSTEM_WORKER_CONDITION, [&]()
    {
        auto worker_condition_context = make_site_system_context<WorkerConditionSystem>(
            *campaign_,
            *active_site_run_,
            command_queue_,
            fixed_step_seconds_,
            move_direction);
        WorkerConditionSystem::run(worker_condition_context);
    });

    run_profiled_system(GS1_RUNTIME_PROFILE_SYSTEM_CAMP_DURABILITY, [&]()
    {
        auto camp_durability_context = make_site_system_context<CampDurabilitySystem>(
            *campaign_,
            *active_site_run_,
            command_queue_,
            fixed_step_seconds_,
            move_direction);
        CampDurabilitySystem::run(camp_durability_context);
    });

    run_profiled_system(GS1_RUNTIME_PROFILE_SYSTEM_DEVICE_MAINTENANCE, [&]()
    {
        auto device_maintenance_context = make_site_system_context<DeviceMaintenanceSystem>(
            *campaign_,
            *active_site_run_,
            command_queue_,
            fixed_step_seconds_,
            move_direction);
        DeviceMaintenanceSystem::run(device_maintenance_context);
    });

    run_profiled_system(GS1_RUNTIME_PROFILE_SYSTEM_DEVICE_SUPPORT, [&]()
    {
        auto device_support_context = make_site_system_context<DeviceSupportSystem>(
            *campaign_,
            *active_site_run_,
            command_queue_,
            fixed_step_seconds_,
            move_direction);
        DeviceSupportSystem::run(device_support_context);
    });

    run_profiled_system(GS1_RUNTIME_PROFILE_SYSTEM_ECOLOGY, [&]()
    {
        auto ecology_context = make_site_system_context<EcologySystem>(
            *campaign_,
            *active_site_run_,
            command_queue_,
            fixed_step_seconds_,
            move_direction);
        EcologySystem::run(ecology_context);
    });

    run_profiled_system(GS1_RUNTIME_PROFILE_SYSTEM_INVENTORY, [&]()
    {
        auto inventory_context = make_site_system_context<InventorySystem>(
            *campaign_,
            *active_site_run_,
            command_queue_,
            fixed_step_seconds_,
            move_direction);
        InventorySystem::run(inventory_context);
    });

    run_profiled_system(GS1_RUNTIME_PROFILE_SYSTEM_CRAFT, [&]()
    {
        auto craft_context = make_site_system_context<CraftSystem>(
            *campaign_,
            *active_site_run_,
            command_queue_,
            fixed_step_seconds_,
            move_direction);
        CraftSystem::run(craft_context);
    });

    run_profiled_system(GS1_RUNTIME_PROFILE_SYSTEM_TASK_BOARD, [&]()
    {
        auto task_board_context = make_site_system_context<TaskBoardSystem>(
            *campaign_,
            *active_site_run_,
            command_queue_,
            fixed_step_seconds_,
            move_direction);
        TaskBoardSystem::run(task_board_context);
    });

    run_profiled_system(GS1_RUNTIME_PROFILE_SYSTEM_ECONOMY_PHONE, [&]()
    {
        auto economy_context = make_site_system_context<EconomyPhoneSystem>(
            *campaign_,
            *active_site_run_,
            command_queue_,
            fixed_step_seconds_,
            move_direction);
        EconomyPhoneSystem::run(economy_context);
    });

    run_profiled_system(GS1_RUNTIME_PROFILE_SYSTEM_FAILURE_RECOVERY, [&]()
    {
        auto failure_context = make_site_system_context<FailureRecoverySystem>(
            *campaign_,
            *active_site_run_,
            command_queue_,
            fixed_step_seconds_,
            move_direction);
        FailureRecoverySystem::run(failure_context);
    });

    run_profiled_system(GS1_RUNTIME_PROFILE_SYSTEM_SITE_COMPLETION, [&]()
    {
        auto completion_context = make_site_system_context<SiteCompletionSystem>(
            *campaign_,
            *active_site_run_,
            command_queue_,
            fixed_step_seconds_,
            move_direction);
        SiteCompletionSystem::run(completion_context);
    });

    record_timing_sample(fixed_step_timing_, elapsed_milliseconds_since(fixed_step_started_at));
}
}  // namespace gs1
