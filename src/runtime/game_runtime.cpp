#include "runtime/game_runtime.h"

#include "campaign/systems/campaign_flow_system.h"
#include "campaign/systems/campaign_system_context.h"
#include "campaign/systems/loadout_planner_system.h"
#include "commands/command_dispatcher.h"
#include "site/systems/action_execution_system.h"
#include "site/systems/camp_durability_system.h"
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
#include <cstdio>
#include <cstring>
#include <limits>
#include <set>

namespace gs1
{
namespace
{
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

Gs1EngineCommand make_engine_command(
    Gs1EngineCommandType type)
{
    Gs1EngineCommand command {};
    command.type = type;
    return command;
}

bool tile_coord_in_bounds(const TileGridState& tile_grid, TileCoord coord) noexcept
{
    return coord.x >= 0 &&
        coord.y >= 0 &&
        static_cast<std::uint32_t>(coord.x) < tile_grid.width &&
        static_cast<std::uint32_t>(coord.y) < tile_grid.height;
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

void GameRuntime::initialize_subscription_tables()
{
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

        if (InventorySystem::subscribes_to(type))
        {
            subscribers.push_back(CommandSubscriberId::Inventory);
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

        if (DeviceMaintenanceSystem::subscribes_to(type))
        {
            subscribers.push_back(CommandSubscriberId::DeviceMaintenance);
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
            return status;
        }

        boot_initialized_ = true;
    }

    status = dispatch_host_events(HostEventDispatchStage::Phase1, out_result.processed_host_event_count);
    if (status != GS1_STATUS_OK)
    {
        phase1_site_move_direction_ = {};
        return status;
    }

    if (!active_site_run_.has_value())
    {
        phase1_site_move_direction_ = {};
        out_result.engine_commands_queued = static_cast<std::uint32_t>(engine_commands_.size());
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
    return status;
}

Gs1Status GameRuntime::run_phase2(const Gs1Phase2Request& request, Gs1Phase2Result& out_result)
{
    if (request.struct_size != sizeof(Gs1Phase2Request))
    {
        return GS1_STATUS_INVALID_ARGUMENT;
    }

    out_result = {};
    out_result.struct_size = sizeof(Gs1Phase2Result);

    auto status = dispatch_host_events(HostEventDispatchStage::Phase2, out_result.processed_host_event_count);
    if (status != GS1_STATUS_OK)
    {
        return status;
    }

    status = dispatch_feedback_events(out_result.processed_feedback_event_count);
    if (status != GS1_STATUS_OK)
    {
        return status;
    }

    status = dispatch_queued_commands();
    out_result.engine_commands_queued = static_cast<std::uint32_t>(engine_commands_.size());
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

    auto dispatch_to_site_system = [&](auto process_command_fn) -> Gs1Status {
        if (!campaign_.has_value() || !active_site_run_.has_value())
        {
            return GS1_STATUS_INVALID_STATE;
        }

        SiteSystemContext context {
            *campaign_,
            *active_site_run_,
            command_queue_,
            fixed_step_seconds_,
            SiteMoveDirectionInput {}};
        return process_command_fn(context, command);
    };

    const auto& subscribers = command_subscribers_[command_type_index(command.type)];
    for (const auto subscriber : subscribers)
    {
        switch (subscriber)
        {
        case CommandSubscriberId::CampaignFlow:
        {
            CampaignFlowCommandContext context {
                campaign_,
                active_site_run_,
                app_state_,
                command_queue_};
            const auto status = CampaignFlowSystem::process_command(context, command);
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

            CampaignSystemContext context {*campaign_};
            const auto status = LoadoutPlannerSystem::process_command(context, command);
            if (status != GS1_STATUS_OK)
            {
                return status;
            }
            break;
        }

        case CommandSubscriberId::ActionExecution:
        {
            const auto status = dispatch_to_site_system(ActionExecutionSystem::process_command);
            if (status != GS1_STATUS_OK)
            {
                return status;
            }
            break;
        }

        case CommandSubscriberId::WeatherEvent:
        {
            const auto status = dispatch_to_site_system(WeatherEventSystem::process_command);
            if (status != GS1_STATUS_OK)
            {
                return status;
            }
            break;
        }

        case CommandSubscriberId::WorkerCondition:
        {
            const auto status = dispatch_to_site_system(WorkerConditionSystem::process_command);
            if (status != GS1_STATUS_OK)
            {
                return status;
            }
            break;
        }

        case CommandSubscriberId::Ecology:
        {
            const auto status = dispatch_to_site_system(EcologySystem::process_command);
            if (status != GS1_STATUS_OK)
            {
                return status;
            }
            break;
        }

        case CommandSubscriberId::TaskBoard:
        {
            const auto status = dispatch_to_site_system(TaskBoardSystem::process_command);
            if (status != GS1_STATUS_OK)
            {
                return status;
            }
            break;
        }

        case CommandSubscriberId::PlacementValidation:
        {
            const auto status = dispatch_to_site_system(PlacementValidationSystem::process_command);
            if (status != GS1_STATUS_OK)
            {
                return status;
            }
            break;
        }

        case CommandSubscriberId::LocalWeatherResolve:
        {
            const auto status = dispatch_to_site_system(LocalWeatherResolveSystem::process_command);
            if (status != GS1_STATUS_OK)
            {
                return status;
            }
            break;
        }

        case CommandSubscriberId::Inventory:
        {
            const auto status = dispatch_to_site_system(InventorySystem::process_command);
            if (status != GS1_STATUS_OK)
            {
                return status;
            }
            break;
        }

        case CommandSubscriberId::EconomyPhone:
        {
            const auto status = dispatch_to_site_system(EconomyPhoneSystem::process_command);
            if (status != GS1_STATUS_OK)
            {
                return status;
            }
            break;
        }

        case CommandSubscriberId::CampDurability:
        {
            const auto status = dispatch_to_site_system(CampDurabilitySystem::process_command);
            if (status != GS1_STATUS_OK)
            {
                return status;
            }
            break;
        }

        case CommandSubscriberId::DeviceSupport:
        {
            const auto status = dispatch_to_site_system(DeviceSupportSystem::process_command);
            if (status != GS1_STATUS_OK)
            {
                return status;
            }
            break;
        }

        case CommandSubscriberId::DeviceMaintenance:
        {
            const auto status = dispatch_to_site_system(DeviceMaintenanceSystem::process_command);
            if (status != GS1_STATUS_OK)
            {
                return status;
            }
            break;
        }

        case CommandSubscriberId::Modifier:
        {
            const auto status = dispatch_to_site_system(ModifierSystem::process_command);
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

    case GameCommandType::DeploymentSiteSelectionChanged:
    case GameCommandType::StartSiteAction:
    case GameCommandType::CancelSiteAction:
    case GameCommandType::SiteActionStarted:
    case GameCommandType::SiteActionCompleted:
    case GameCommandType::SiteActionFailed:
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
    case GameCommandType::InventoryItemUseRequested:
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

    queue_ui_setup_begin_command(
        GS1_UI_SETUP_REGIONAL_MAP_SELECTION,
        GS1_UI_SETUP_PRESENTATION_OVERLAY,
        2U,
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

    Gs1UiAction deploy_action {};
    deploy_action.type = GS1_UI_ACTION_START_SITE_ATTEMPT;
    deploy_action.target_id = site_id;
    char button_text[64] {};
    std::snprintf(button_text, sizeof(button_text), "Start Site %u", static_cast<unsigned>(site_id));
    queue_ui_element_command(
        2U,
        GS1_UI_ELEMENT_BUTTON,
        GS1_UI_ELEMENT_FLAG_PRIMARY,
        deploy_action,
        button_text);

    Gs1UiAction clear_selection_action {};
    clear_selection_action.type = GS1_UI_ACTION_CLEAR_DEPLOYMENT_SITE_SELECTION;
    queue_ui_element_command(
        3U,
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
    begin_payload.width = static_cast<std::uint16_t>(active_site_run_->tile_grid.width);
    begin_payload.height = static_cast<std::uint16_t>(active_site_run_->tile_grid.height);
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

    if (x >= active_site_run_->tile_grid.width || y >= active_site_run_->tile_grid.height)
    {
        return;
    }

    const auto index =
        static_cast<std::size_t>(y) * static_cast<std::size_t>(active_site_run_->tile_grid.width) +
        static_cast<std::size_t>(x);

    auto tile_command = make_engine_command(GS1_ENGINE_COMMAND_SITE_TILE_UPSERT);
    auto& payload = tile_command.emplace_payload<Gs1EngineCommandSiteTileData>();
    payload.x = x;
    payload.y = y;
    payload.terrain_type_id = active_site_run_->tile_grid.terrain_type_ids[index];
    payload.plant_type_id = active_site_run_->tile_grid.plant_type_ids[index].value;
    payload.structure_type_id = active_site_run_->tile_grid.structure_type_ids[index].value;
    payload.ground_cover_type_id = active_site_run_->tile_grid.ground_cover_type_ids[index];
    payload.plant_density = active_site_run_->tile_grid.tile_plant_density[index];
    payload.sand_burial = active_site_run_->tile_grid.tile_sand_burial[index];
    engine_commands_.push_back(tile_command);
}

void GameRuntime::queue_all_site_tile_upsert_commands()
{
    if (!active_site_run_.has_value())
    {
        return;
    }

    for (std::uint32_t y = 0; y < active_site_run_->tile_grid.height; ++y)
    {
        for (std::uint32_t x = 0; x < active_site_run_->tile_grid.width; ++x)
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

    auto worker_command = make_engine_command(GS1_ENGINE_COMMAND_SITE_WORKER_UPDATE);
    auto& worker_payload = worker_command.emplace_payload<Gs1EngineCommandWorkerData>();
    worker_payload.tile_x = active_site_run_->worker.tile_position_x;
    worker_payload.tile_y = active_site_run_->worker.tile_position_y;
    worker_payload.facing_degrees = active_site_run_->worker.facing_degrees;
    worker_payload.health_normalized = active_site_run_->worker.player_health / 100.0f;
    worker_payload.hydration_normalized = active_site_run_->worker.player_hydration / 100.0f;
    worker_payload.energy_normalized =
        active_site_run_->worker.player_energy_cap > 0.0f
            ? (active_site_run_->worker.player_energy / active_site_run_->worker.player_energy_cap)
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
    std::uint32_t slot_index)
{
    if (!active_site_run_.has_value())
    {
        return;
    }

    const auto& slots =
        container_kind == GS1_INVENTORY_CONTAINER_WORKER_PACK
            ? active_site_run_->inventory.worker_pack_slots
            : active_site_run_->inventory.camp_storage_slots;
    if (slot_index >= slots.size())
    {
        return;
    }

    const auto& slot = slots[slot_index];
    auto command = make_engine_command(GS1_ENGINE_COMMAND_SITE_INVENTORY_SLOT_UPSERT);
    auto& payload = command.emplace_payload<Gs1EngineCommandInventorySlotData>();
    payload.item_id = slot.item_id.value;
    payload.condition = slot.item_condition;
    payload.freshness = slot.item_freshness;
    payload.quantity = static_cast<std::uint16_t>(std::min<std::uint32_t>(slot.item_quantity, 65535U));
    payload.slot_index = static_cast<std::uint16_t>(slot_index);
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
        queue_all_site_inventory_slot_upsert_commands();
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

void GameRuntime::queue_hud_state_command()
{
    if (!active_site_run_.has_value())
    {
        return;
    }

    auto hud_command = make_engine_command(GS1_ENGINE_COMMAND_HUD_STATE);
    auto& payload = hud_command.emplace_payload<Gs1EngineCommandHudStateData>();
    payload.player_health = active_site_run_->worker.player_health;
    payload.player_hydration = active_site_run_->worker.player_hydration;
    payload.player_energy = active_site_run_->worker.player_energy;
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

    active_site_run_->pending_projection_update_flags |= dirty_flags;
}

void GameRuntime::mark_site_tile_projection_dirty(TileCoord coord) noexcept
{
    if (!active_site_run_.has_value())
    {
        return;
    }

    auto& site_run = active_site_run_.value();
    if (!tile_coord_in_bounds(site_run.tile_grid, coord))
    {
        return;
    }

    const auto tile_count = site_run.tile_grid.tile_count();
    if (site_run.pending_tile_projection_update_mask.size() != tile_count)
    {
        site_run.pending_tile_projection_update_mask.assign(tile_count, 0U);
        site_run.pending_tile_projection_updates.clear();
    }

    const auto index = site_run.tile_grid.to_index(coord);
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
        if (!tile_coord_in_bounds(site_run.tile_grid, coord))
        {
            continue;
        }

        const auto index = site_run.tile_grid.to_index(coord);
        if (index < site_run.pending_tile_projection_update_mask.size())
        {
            site_run.pending_tile_projection_update_mask[index] = 0U;
        }
    }

    site_run.pending_tile_projection_updates.clear();
    site_run.pending_full_tile_projection_update = false;
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
    case GS1_UI_ACTION_TRANSFER_INVENTORY_ITEM:
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

    CampaignFixedStepContext campaign_context {*campaign_};
    CampaignFlowSystem::run(campaign_context);

    SiteSystemContext context {
        *campaign_,
        *active_site_run_,
        command_queue_,
        fixed_step_seconds_,
        SiteMoveDirectionInput {
            phase1_site_move_direction_.world_move_x,
            phase1_site_move_direction_.world_move_y,
            phase1_site_move_direction_.world_move_z,
            phase1_site_move_direction_.present}};

    SiteFlowSystem::run(context);
    ModifierSystem::run(context);
    WeatherEventSystem::run(context);
    ActionExecutionSystem::run(context);
    LocalWeatherResolveSystem::run(context);
    WorkerConditionSystem::run(context);
    CampDurabilitySystem::run(context);
    DeviceMaintenanceSystem::run(context);
    DeviceSupportSystem::run(context);
    EcologySystem::run(context);
    InventorySystem::run(context);
    TaskBoardSystem::run(context);
    EconomyPhoneSystem::run(context);
    FailureRecoverySystem::run(context);
    SiteCompletionSystem::run(context);
}
}  // namespace gs1
