#include "runtime/game_runtime.h"

#include "campaign/systems/campaign_flow_system.h"
#include "campaign/systems/campaign_time_system.h"
#include "campaign/systems/campaign_system_context.h"
#include "campaign/systems/faction_reputation_system.h"
#include "campaign/systems/loadout_planner_system.h"
#include "campaign/systems/technology_system.h"
#include "content/content_loader.h"
#include "messages/message_dispatcher.h"
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
#include "site/systems/phone_panel_system.h"
#include "site/systems/plant_weather_contribution_system.h"
#include "site/systems/placement_validation_system.h"
#include "site/systems/site_completion_system.h"
#include "site/systems/site_flow_system.h"
#include "site/systems/site_time_system.h"
#include "site/systems/task_board_system.h"
#include "site/systems/weather_event_system.h"
#include "site/systems/worker_condition_system.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <filesystem>
#include <limits>
#include <string_view>

namespace gs1
{
namespace
{
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


template <typename SiteSystemTag, typename ProcessMessageFn>
Gs1Status dispatch_site_system_message(
    ProcessMessageFn process_message_fn,
    const GameMessage& message,
    const std::optional<CampaignState>& campaign,
    std::optional<SiteRunState>& active_site_run,
    GameMessageQueue& message_queue,
    double fixed_step_seconds)
{
    if (!campaign.has_value() || !active_site_run.has_value())
    {
        return GS1_STATUS_INVALID_STATE;
    }

    auto context = make_site_system_context<SiteSystemTag>(
        *campaign,
        *active_site_run,
        message_queue,
        fixed_step_seconds,
        SiteMoveDirectionInput {});
    return process_message_fn(context, message);
}

template <typename SiteSystemTag, typename ProcessHostMessageFn>
Gs1Status dispatch_site_system_host_message(
    ProcessHostMessageFn process_message_fn,
    const Gs1HostMessage& message,
    const std::optional<CampaignState>& campaign,
    std::optional<SiteRunState>& active_site_run,
    GameMessageQueue& message_queue,
    double fixed_step_seconds)
{
    if (!campaign.has_value() || !active_site_run.has_value())
    {
        return GS1_STATUS_INVALID_STATE;
    }

    auto context = make_site_system_context<SiteSystemTag>(
        *campaign,
        *active_site_run,
        message_queue,
        fixed_step_seconds,
        SiteMoveDirectionInput {});
    return process_message_fn(context, message);
}
}  // namespace

GameRuntime::GameRuntime(Gs1RuntimeCreateDesc create_desc)
    : create_desc_(create_desc)
{
    if (create_desc_.fixed_step_seconds > 0.0)
    {
        fixed_step_seconds_ = create_desc_.fixed_step_seconds;
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

void GameRuntime::initialize_subscription_tables()
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
        PhonePanelSystem::access(),
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

    for (std::size_t index = 0; index < host_message_subscribers_.size(); ++index)
    {
        const auto type = static_cast<Gs1HostMessageType>(index);
        auto& subscribers = host_message_subscribers_[index];

        if (CampaignFlowSystem::subscribes_to_host_message(type))
        {
            subscribers.push_back(HostMessageSubscriberId::CampaignFlow);
        }

        if (ActionExecutionSystem::subscribes_to_host_message(type))
        {
            subscribers.push_back(HostMessageSubscriberId::ActionExecution);
        }

        if (InventorySystem::subscribes_to_host_message(type))
        {
            subscribers.push_back(HostMessageSubscriberId::Inventory);
        }

        if (CraftSystem::subscribes_to_host_message(type))
        {
            subscribers.push_back(HostMessageSubscriberId::Craft);
        }

        if (TaskBoardSystem::subscribes_to_host_message(type))
        {
            subscribers.push_back(HostMessageSubscriberId::TaskBoard);
        }

        if (EconomyPhoneSystem::subscribes_to_host_message(type))
        {
            subscribers.push_back(HostMessageSubscriberId::EconomyPhone);
        }

        if (PhonePanelSystem::subscribes_to_host_message(type))
        {
            subscribers.push_back(HostMessageSubscriberId::PhonePanel);
        }

        if (ModifierSystem::subscribes_to_host_message(type))
        {
            subscribers.push_back(HostMessageSubscriberId::Modifier);
        }

        if (SiteFlowSystem::subscribes_to_host_message(type))
        {
            subscribers.push_back(HostMessageSubscriberId::SiteFlow);
        }

        if (GamePresentationCoordinator::subscribes_to_host_message(type))
        {
            subscribers.push_back(HostMessageSubscriberId::Presentation);
        }
    }

    for (std::size_t index = 0; index < k_game_message_type_count; ++index)
    {
        const auto type = static_cast<GameMessageType>(index);
        auto& subscribers = message_subscribers_[index];

        if (CampaignFlowSystem::subscribes_to(type))
        {
            subscribers.push_back(MessageSubscriberId::CampaignFlow);
        }

        if (LoadoutPlannerSystem::subscribes_to(type))
        {
            subscribers.push_back(MessageSubscriberId::LoadoutPlanner);
        }

        if (FactionReputationSystem::subscribes_to(type))
        {
            subscribers.push_back(MessageSubscriberId::FactionReputation);
        }

        if (TechnologySystem::subscribes_to(type))
        {
            subscribers.push_back(MessageSubscriberId::Technology);
        }

        if (ActionExecutionSystem::subscribes_to(type))
        {
            subscribers.push_back(MessageSubscriberId::ActionExecution);
        }

        if (WeatherEventSystem::subscribes_to(type))
        {
            subscribers.push_back(MessageSubscriberId::WeatherEvent);
        }

        if (WorkerConditionSystem::subscribes_to(type))
        {
            subscribers.push_back(MessageSubscriberId::WorkerCondition);
        }

        if (EcologySystem::subscribes_to(type))
        {
            subscribers.push_back(MessageSubscriberId::Ecology);
        }

        if (PlantWeatherContributionSystem::subscribes_to(type))
        {
            subscribers.push_back(MessageSubscriberId::PlantWeatherContribution);
        }

        if (DeviceWeatherContributionSystem::subscribes_to(type))
        {
            subscribers.push_back(MessageSubscriberId::DeviceWeatherContribution);
        }

        if (TaskBoardSystem::subscribes_to(type))
        {
            subscribers.push_back(MessageSubscriberId::TaskBoard);
        }

        if (PlacementValidationSystem::subscribes_to(type))
        {
            subscribers.push_back(MessageSubscriberId::PlacementValidation);
        }

        if (LocalWeatherResolveSystem::subscribes_to(type))
        {
            subscribers.push_back(MessageSubscriberId::LocalWeatherResolve);
        }

        if (DeviceMaintenanceSystem::subscribes_to(type))
        {
            subscribers.push_back(MessageSubscriberId::DeviceMaintenance);
        }

        if (InventorySystem::subscribes_to(type))
        {
            subscribers.push_back(MessageSubscriberId::Inventory);
        }

        if (CraftSystem::subscribes_to(type))
        {
            subscribers.push_back(MessageSubscriberId::Craft);
        }

        if (EconomyPhoneSystem::subscribes_to(type))
        {
            subscribers.push_back(MessageSubscriberId::EconomyPhone);
        }

        if (PhonePanelSystem::subscribes_to(type))
        {
            subscribers.push_back(MessageSubscriberId::PhonePanel);
        }

        if (CampDurabilitySystem::subscribes_to(type))
        {
            subscribers.push_back(MessageSubscriberId::CampDurability);
        }

        if (DeviceSupportSystem::subscribes_to(type))
        {
            subscribers.push_back(MessageSubscriberId::DeviceSupport);
        }

        if (ModifierSystem::subscribes_to(type))
        {
            subscribers.push_back(MessageSubscriberId::Modifier);
        }
    }
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

Gs1Status GameRuntime::submit_host_events(
    const Gs1HostEvent* events,
    std::uint32_t event_count)
{
    return submit_host_messages(events, event_count);
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
    auto status = GS1_STATUS_OK;
    if (!boot_initialized_)
    {
        if (!campaign_.has_value() && !active_site_run_.has_value())
        {
            GameMessage boot_message {};
            boot_message.type = GameMessageType::OpenMainMenu;
            boot_message.set_payload(OpenMainMenuMessage {});
            message_queue_.push_back(boot_message);

            status = dispatch_queued_messages();
            if (status != GS1_STATUS_OK)
            {
                finish_phase();
                return status;
            }
        }

        boot_initialized_ = true;
    }

    status = dispatch_host_messages(out_result.processed_host_event_count);
    if (status != GS1_STATUS_OK)
    {
        finish_phase();
        return status;
    }

    if (!active_site_run_.has_value())
    {
        out_result.engine_messages_queued = static_cast<std::uint32_t>(runtime_messages_.size());
        finish_phase();
        return GS1_STATUS_OK;
    }

    if (app_state_ == GS1_APP_STATE_SITE_LOADING)
    {
        out_result.engine_messages_queued = static_cast<std::uint32_t>(runtime_messages_.size());
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

    GamePresentationRuntimeContext presentation_context {
        app_state_,
        campaign_,
        active_site_run_,
        message_queue_,
        runtime_messages_,
        fixed_step_seconds_};
    presentation_.flush_site_presentation_if_dirty(presentation_context);

    status = dispatch_queued_messages();
    active_site_run_->host_move_direction = {};
    out_result.engine_messages_queued = static_cast<std::uint32_t>(runtime_messages_.size());
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

    auto status = dispatch_host_messages(out_result.processed_host_event_count);
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

    status = dispatch_queued_messages();
    out_result.engine_messages_queued = static_cast<std::uint32_t>(runtime_messages_.size());
    finish_phase();
    return status;
}

Gs1Status GameRuntime::pop_runtime_message(Gs1RuntimeMessage& out_message)
{
    if (runtime_messages_.empty())
    {
        return GS1_STATUS_BUFFER_EMPTY;
    }

    out_message = runtime_messages_.front();
    runtime_messages_.pop_front();
    return GS1_STATUS_OK;
}

Gs1Status GameRuntime::pop_engine_message(Gs1EngineMessage& out_message)
{
    return pop_runtime_message(out_message);
}

Gs1Status GameRuntime::handle_message(const GameMessage& message)
{
    message_queue_.push_back(message);
    return dispatch_queued_messages();
}

Gs1Status GameRuntime::dispatch_subscribed_message(const GameMessage& message)
{
    if (!is_valid_message_type(message.type) || message.type == GameMessageType::Count)
    {
        return GS1_STATUS_INVALID_ARGUMENT;
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

    const auto& subscribers = message_subscribers_[message_type_index(message.type)];
    for (const auto subscriber : subscribers)
    {
        switch (subscriber)
        {
        case MessageSubscriberId::CampaignFlow:
        {
            CampaignFlowMessageContext context {
                campaign_,
                active_site_run_,
                app_state_,
                message_queue_};
            const auto status = dispatch_profiled_message(
                GS1_RUNTIME_PROFILE_SYSTEM_CAMPAIGN_FLOW,
                [&]() -> Gs1Status {
                    return CampaignFlowSystem::process_message(context, message);
                });
            if (status != GS1_STATUS_OK)
            {
                return status;
            }
            app_state_ = context.app_state;
            break;
        }

        case MessageSubscriberId::LoadoutPlanner:
        {
            if (!campaign_.has_value())
            {
                break;
            }

            const auto status = dispatch_profiled_message(
                GS1_RUNTIME_PROFILE_SYSTEM_LOADOUT_PLANNER,
                [&]() -> Gs1Status {
                    CampaignSystemContext context {*campaign_};
                    return LoadoutPlannerSystem::process_message(context, message);
                });
            if (status != GS1_STATUS_OK)
            {
                return status;
            }
            break;
        }

        case MessageSubscriberId::FactionReputation:
        {
            if (!campaign_.has_value())
            {
                break;
            }

            const auto status = dispatch_profiled_message(
                GS1_RUNTIME_PROFILE_SYSTEM_FACTION_REPUTATION,
                [&]() -> Gs1Status {
                    CampaignSystemContext context {*campaign_};
                    return FactionReputationSystem::process_message(context, message);
                });
            if (status != GS1_STATUS_OK)
            {
                return status;
            }
            break;
        }

        case MessageSubscriberId::Technology:
        {
            if (!campaign_.has_value())
            {
                break;
            }

            const auto status = dispatch_profiled_message(
                GS1_RUNTIME_PROFILE_SYSTEM_TECHNOLOGY,
                [&]() -> Gs1Status {
                    CampaignSystemContext context {*campaign_};
                    return TechnologySystem::process_message(context, message);
                });
            if (status != GS1_STATUS_OK)
            {
                return status;
            }
            break;
        }

        case MessageSubscriberId::ActionExecution:
        {
            const auto status = dispatch_profiled_message(
                GS1_RUNTIME_PROFILE_SYSTEM_ACTION_EXECUTION,
                [&]() -> Gs1Status {
                    return dispatch_site_system_message<ActionExecutionSystem>(
                        ActionExecutionSystem::process_message,
                        message,
                        campaign_,
                        active_site_run_,
                        message_queue_,
                        fixed_step_seconds_);
                });
            if (status != GS1_STATUS_OK)
            {
                return status;
            }
            break;
        }

        case MessageSubscriberId::WeatherEvent:
        {
            const auto status = dispatch_profiled_message(
                GS1_RUNTIME_PROFILE_SYSTEM_WEATHER_EVENT,
                [&]() -> Gs1Status {
                    return dispatch_site_system_message<WeatherEventSystem>(
                        WeatherEventSystem::process_message,
                        message,
                        campaign_,
                        active_site_run_,
                        message_queue_,
                        fixed_step_seconds_);
                });
            if (status != GS1_STATUS_OK)
            {
                return status;
            }
            break;
        }

        case MessageSubscriberId::WorkerCondition:
        {
            const auto status = dispatch_profiled_message(
                GS1_RUNTIME_PROFILE_SYSTEM_WORKER_CONDITION,
                [&]() -> Gs1Status {
                    return dispatch_site_system_message<WorkerConditionSystem>(
                        WorkerConditionSystem::process_message,
                        message,
                        campaign_,
                        active_site_run_,
                        message_queue_,
                        fixed_step_seconds_);
                });
            if (status != GS1_STATUS_OK)
            {
                return status;
            }
            break;
        }

        case MessageSubscriberId::Ecology:
        {
            const auto status = dispatch_profiled_message(
                GS1_RUNTIME_PROFILE_SYSTEM_ECOLOGY,
                [&]() -> Gs1Status {
                    return dispatch_site_system_message<EcologySystem>(
                        EcologySystem::process_message,
                        message,
                        campaign_,
                        active_site_run_,
                        message_queue_,
                        fixed_step_seconds_);
                });
            if (status != GS1_STATUS_OK)
            {
                return status;
            }
            break;
        }

        case MessageSubscriberId::TaskBoard:
        {
            const auto status = dispatch_profiled_message(
                GS1_RUNTIME_PROFILE_SYSTEM_TASK_BOARD,
                [&]() -> Gs1Status {
                    return dispatch_site_system_message<TaskBoardSystem>(
                        TaskBoardSystem::process_message,
                        message,
                        campaign_,
                        active_site_run_,
                        message_queue_,
                        fixed_step_seconds_);
                });
            if (status != GS1_STATUS_OK)
            {
                return status;
            }
            break;
        }

        case MessageSubscriberId::PlantWeatherContribution:
        {
            const auto status = dispatch_profiled_message(
                GS1_RUNTIME_PROFILE_SYSTEM_LOCAL_WEATHER_RESOLVE,
                [&]() -> Gs1Status {
                    return dispatch_site_system_message<PlantWeatherContributionSystem>(
                        PlantWeatherContributionSystem::process_message,
                        message,
                        campaign_,
                        active_site_run_,
                        message_queue_,
                        fixed_step_seconds_);
                });
            if (status != GS1_STATUS_OK)
            {
                return status;
            }
            break;
        }

        case MessageSubscriberId::DeviceWeatherContribution:
        {
            const auto status = dispatch_profiled_message(
                GS1_RUNTIME_PROFILE_SYSTEM_LOCAL_WEATHER_RESOLVE,
                [&]() -> Gs1Status {
                    return dispatch_site_system_message<DeviceWeatherContributionSystem>(
                        DeviceWeatherContributionSystem::process_message,
                        message,
                        campaign_,
                        active_site_run_,
                        message_queue_,
                        fixed_step_seconds_);
                });
            if (status != GS1_STATUS_OK)
            {
                return status;
            }
            break;
        }

        case MessageSubscriberId::PlacementValidation:
        {
            const auto status = dispatch_profiled_message(
                GS1_RUNTIME_PROFILE_SYSTEM_PLACEMENT_VALIDATION,
                [&]() -> Gs1Status {
                    return dispatch_site_system_message<PlacementValidationSystem>(
                        PlacementValidationSystem::process_message,
                        message,
                        campaign_,
                        active_site_run_,
                        message_queue_,
                        fixed_step_seconds_);
                });
            if (status != GS1_STATUS_OK)
            {
                return status;
            }
            break;
        }

        case MessageSubscriberId::LocalWeatherResolve:
        {
            const auto status = dispatch_profiled_message(
                GS1_RUNTIME_PROFILE_SYSTEM_LOCAL_WEATHER_RESOLVE,
                [&]() -> Gs1Status {
                    return dispatch_site_system_message<LocalWeatherResolveSystem>(
                        LocalWeatherResolveSystem::process_message,
                        message,
                        campaign_,
                        active_site_run_,
                        message_queue_,
                        fixed_step_seconds_);
                });
            if (status != GS1_STATUS_OK)
            {
                return status;
            }
            break;
        }

        case MessageSubscriberId::Inventory:
        {
            const auto status = dispatch_profiled_message(
                GS1_RUNTIME_PROFILE_SYSTEM_INVENTORY,
                [&]() -> Gs1Status {
                    return dispatch_site_system_message<InventorySystem>(
                        InventorySystem::process_message,
                        message,
                        campaign_,
                        active_site_run_,
                        message_queue_,
                        fixed_step_seconds_);
                });
            if (status != GS1_STATUS_OK)
            {
                return status;
            }
            break;
        }

        case MessageSubscriberId::Craft:
        {
            const auto status = dispatch_profiled_message(
                GS1_RUNTIME_PROFILE_SYSTEM_CRAFT,
                [&]() -> Gs1Status {
                    return dispatch_site_system_message<CraftSystem>(
                        CraftSystem::process_message,
                        message,
                        campaign_,
                        active_site_run_,
                        message_queue_,
                        fixed_step_seconds_);
                });
            if (status != GS1_STATUS_OK)
            {
                return status;
            }
            break;
        }

        case MessageSubscriberId::EconomyPhone:
        {
            const auto status = dispatch_profiled_message(
                GS1_RUNTIME_PROFILE_SYSTEM_ECONOMY_PHONE,
                [&]() -> Gs1Status {
                    return dispatch_site_system_message<EconomyPhoneSystem>(
                        EconomyPhoneSystem::process_message,
                        message,
                        campaign_,
                        active_site_run_,
                        message_queue_,
                        fixed_step_seconds_);
                });
            if (status != GS1_STATUS_OK)
            {
                return status;
            }
            break;
        }

        case MessageSubscriberId::PhonePanel:
        {
            const auto status = dispatch_profiled_message(
                GS1_RUNTIME_PROFILE_SYSTEM_PHONE_PANEL,
                [&]() -> Gs1Status {
                    return dispatch_site_system_message<PhonePanelSystem>(
                        PhonePanelSystem::process_message,
                        message,
                        campaign_,
                        active_site_run_,
                        message_queue_,
                        fixed_step_seconds_);
                });
            if (status != GS1_STATUS_OK)
            {
                return status;
            }
            break;
        }

        case MessageSubscriberId::CampDurability:
        {
            const auto status = dispatch_profiled_message(
                GS1_RUNTIME_PROFILE_SYSTEM_CAMP_DURABILITY,
                [&]() -> Gs1Status {
                    return dispatch_site_system_message<CampDurabilitySystem>(
                        CampDurabilitySystem::process_message,
                        message,
                        campaign_,
                        active_site_run_,
                        message_queue_,
                        fixed_step_seconds_);
                });
            if (status != GS1_STATUS_OK)
            {
                return status;
            }
            break;
        }

        case MessageSubscriberId::DeviceSupport:
        {
            const auto status = dispatch_profiled_message(
                GS1_RUNTIME_PROFILE_SYSTEM_DEVICE_SUPPORT,
                [&]() -> Gs1Status {
                    return dispatch_site_system_message<DeviceSupportSystem>(
                        DeviceSupportSystem::process_message,
                        message,
                        campaign_,
                        active_site_run_,
                        message_queue_,
                        fixed_step_seconds_);
                });
            if (status != GS1_STATUS_OK)
            {
                return status;
            }
            break;
        }

        case MessageSubscriberId::DeviceMaintenance:
        {
            const auto status = dispatch_profiled_message(
                GS1_RUNTIME_PROFILE_SYSTEM_DEVICE_MAINTENANCE,
                [&]() -> Gs1Status {
                    return dispatch_site_system_message<DeviceMaintenanceSystem>(
                        DeviceMaintenanceSystem::process_message,
                        message,
                        campaign_,
                        active_site_run_,
                        message_queue_,
                        fixed_step_seconds_);
                });
            if (status != GS1_STATUS_OK)
            {
                return status;
            }
            break;
        }

        case MessageSubscriberId::Modifier:
        {
            const auto status = dispatch_profiled_message(
                GS1_RUNTIME_PROFILE_SYSTEM_MODIFIER,
                [&]() -> Gs1Status {
                    return dispatch_site_system_message<ModifierSystem>(
                        ModifierSystem::process_message,
                        message,
                        campaign_,
                        active_site_run_,
                        message_queue_,
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

    const auto& subscribers = host_message_subscribers_[host_message_type_index(message.type)];
    for (const auto subscriber : subscribers)
    {
        switch (subscriber)
        {
        case HostMessageSubscriberId::CampaignFlow:
        {
            CampaignFlowMessageContext context {
                campaign_,
                active_site_run_,
                app_state_,
                message_queue_};
            const auto status = CampaignFlowSystem::process_host_message(context, message);
            if (status != GS1_STATUS_OK)
            {
                return status;
            }
            app_state_ = context.app_state;
            break;
        }

        case HostMessageSubscriberId::ActionExecution:
        {
            const auto status = dispatch_site_system_host_message<ActionExecutionSystem>(
                ActionExecutionSystem::process_host_message,
                message,
                campaign_,
                active_site_run_,
                message_queue_,
                fixed_step_seconds_);
            if (status != GS1_STATUS_OK && status != GS1_STATUS_INVALID_STATE)
            {
                return status;
            }
            break;
        }

        case HostMessageSubscriberId::Inventory:
        {
            const auto status = dispatch_site_system_host_message<InventorySystem>(
                InventorySystem::process_host_message,
                message,
                campaign_,
                active_site_run_,
                message_queue_,
                fixed_step_seconds_);
            if (status != GS1_STATUS_OK && status != GS1_STATUS_INVALID_STATE)
            {
                return status;
            }
            break;
        }

        case HostMessageSubscriberId::Craft:
        {
            const auto status = dispatch_site_system_host_message<CraftSystem>(
                CraftSystem::process_host_message,
                message,
                campaign_,
                active_site_run_,
                message_queue_,
                fixed_step_seconds_);
            if (status != GS1_STATUS_OK && status != GS1_STATUS_INVALID_STATE)
            {
                return status;
            }
            break;
        }

        case HostMessageSubscriberId::TaskBoard:
        {
            const auto status = dispatch_site_system_host_message<TaskBoardSystem>(
                TaskBoardSystem::process_host_message,
                message,
                campaign_,
                active_site_run_,
                message_queue_,
                fixed_step_seconds_);
            if (status != GS1_STATUS_OK && status != GS1_STATUS_INVALID_STATE)
            {
                return status;
            }
            break;
        }

        case HostMessageSubscriberId::EconomyPhone:
        {
            const auto status = dispatch_site_system_host_message<EconomyPhoneSystem>(
                EconomyPhoneSystem::process_host_message,
                message,
                campaign_,
                active_site_run_,
                message_queue_,
                fixed_step_seconds_);
            if (status != GS1_STATUS_OK && status != GS1_STATUS_INVALID_STATE)
            {
                return status;
            }
            break;
        }

        case HostMessageSubscriberId::PhonePanel:
        {
            const auto status = dispatch_site_system_host_message<PhonePanelSystem>(
                PhonePanelSystem::process_host_message,
                message,
                campaign_,
                active_site_run_,
                message_queue_,
                fixed_step_seconds_);
            if (status != GS1_STATUS_OK && status != GS1_STATUS_INVALID_STATE)
            {
                return status;
            }
            break;
        }

        case HostMessageSubscriberId::Modifier:
        {
            const auto status = dispatch_site_system_host_message<ModifierSystem>(
                ModifierSystem::process_host_message,
                message,
                campaign_,
                active_site_run_,
                message_queue_,
                fixed_step_seconds_);
            if (status != GS1_STATUS_OK && status != GS1_STATUS_INVALID_STATE)
            {
                return status;
            }
            break;
        }

        case HostMessageSubscriberId::SiteFlow:
        {
            const auto status = dispatch_site_system_host_message<SiteFlowSystem>(
                SiteFlowSystem::process_host_message,
                message,
                campaign_,
                active_site_run_,
                message_queue_,
                fixed_step_seconds_);
            if (status != GS1_STATUS_OK && status != GS1_STATUS_INVALID_STATE)
            {
                return status;
            }
            break;
        }

        case HostMessageSubscriberId::Presentation:
        {
            GamePresentationRuntimeContext context {
                app_state_,
                campaign_,
                active_site_run_,
                message_queue_,
                runtime_messages_,
                fixed_step_seconds_};
            const auto status = presentation_.process_host_message(context, message);
            if (status != GS1_STATUS_OK)
            {
                return status;
            }
            break;
        }

        default:
            break;
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

Gs1Status GameRuntime::dispatch_queued_messages()
{
    return MessageDispatcher::dispatch_all(*this);
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

    const SiteMoveDirectionInput move_direction {
        active_site_run_->host_move_direction.world_move_x,
        active_site_run_->host_move_direction.world_move_y,
        active_site_run_->host_move_direction.world_move_z,
        active_site_run_->host_move_direction.present};

    run_profiled_system(GS1_RUNTIME_PROFILE_SYSTEM_CAMPAIGN_TIME, [&]()
    {
        CampaignFixedStepContext campaign_context {*campaign_, fixed_step_seconds_};
        CampaignTimeSystem::run(campaign_context);
    });

    run_profiled_system(GS1_RUNTIME_PROFILE_SYSTEM_SITE_TIME, [&]()
    {
        auto site_time_context = make_site_system_context<SiteTimeSystem>(
            *campaign_,
            *active_site_run_,
            message_queue_,
            fixed_step_seconds_,
            move_direction);
        SiteTimeSystem::run(site_time_context);
    });

    run_profiled_system(GS1_RUNTIME_PROFILE_SYSTEM_SITE_FLOW, [&]()
    {
        auto site_flow_context = make_site_system_context<SiteFlowSystem>(
            *campaign_,
            *active_site_run_,
            message_queue_,
            fixed_step_seconds_,
            move_direction);
        SiteFlowSystem::run(site_flow_context);
    });

    run_profiled_system(GS1_RUNTIME_PROFILE_SYSTEM_MODIFIER, [&]()
    {
        auto modifier_context = make_site_system_context<ModifierSystem>(
            *campaign_,
            *active_site_run_,
            message_queue_,
            fixed_step_seconds_,
            move_direction);
        ModifierSystem::run(modifier_context);
    });

    run_profiled_system(GS1_RUNTIME_PROFILE_SYSTEM_WEATHER_EVENT, [&]()
    {
        auto weather_event_context = make_site_system_context<WeatherEventSystem>(
            *campaign_,
            *active_site_run_,
            message_queue_,
            fixed_step_seconds_,
            move_direction);
        WeatherEventSystem::run(weather_event_context);
    });

    run_profiled_system(GS1_RUNTIME_PROFILE_SYSTEM_ACTION_EXECUTION, [&]()
    {
        auto action_execution_context = make_site_system_context<ActionExecutionSystem>(
            *campaign_,
            *active_site_run_,
            message_queue_,
            fixed_step_seconds_,
            move_direction);
        ActionExecutionSystem::run(action_execution_context);
    });

    run_profiled_system(GS1_RUNTIME_PROFILE_SYSTEM_LOCAL_WEATHER_RESOLVE, [&]()
    {
        auto plant_weather_contribution_context =
            make_site_system_context<PlantWeatherContributionSystem>(
                *campaign_,
                *active_site_run_,
                message_queue_,
                fixed_step_seconds_,
                move_direction);
        PlantWeatherContributionSystem::run(plant_weather_contribution_context);

        auto device_weather_contribution_context =
            make_site_system_context<DeviceWeatherContributionSystem>(
                *campaign_,
                *active_site_run_,
                message_queue_,
                fixed_step_seconds_,
                move_direction);
        DeviceWeatherContributionSystem::run(device_weather_contribution_context);

        auto local_weather_context = make_site_system_context<LocalWeatherResolveSystem>(
            *campaign_,
            *active_site_run_,
            message_queue_,
            fixed_step_seconds_,
            move_direction);
        LocalWeatherResolveSystem::run(local_weather_context);
    });

    run_profiled_system(GS1_RUNTIME_PROFILE_SYSTEM_WORKER_CONDITION, [&]()
    {
        auto worker_condition_context = make_site_system_context<WorkerConditionSystem>(
            *campaign_,
            *active_site_run_,
            message_queue_,
            fixed_step_seconds_,
            move_direction);
        WorkerConditionSystem::run(worker_condition_context);
    });

    run_profiled_system(GS1_RUNTIME_PROFILE_SYSTEM_CAMP_DURABILITY, [&]()
    {
        auto camp_durability_context = make_site_system_context<CampDurabilitySystem>(
            *campaign_,
            *active_site_run_,
            message_queue_,
            fixed_step_seconds_,
            move_direction);
        CampDurabilitySystem::run(camp_durability_context);
    });

    run_profiled_system(GS1_RUNTIME_PROFILE_SYSTEM_DEVICE_MAINTENANCE, [&]()
    {
        auto device_maintenance_context = make_site_system_context<DeviceMaintenanceSystem>(
            *campaign_,
            *active_site_run_,
            message_queue_,
            fixed_step_seconds_,
            move_direction);
        DeviceMaintenanceSystem::run(device_maintenance_context);
    });

    run_profiled_system(GS1_RUNTIME_PROFILE_SYSTEM_DEVICE_SUPPORT, [&]()
    {
        auto device_support_context = make_site_system_context<DeviceSupportSystem>(
            *campaign_,
            *active_site_run_,
            message_queue_,
            fixed_step_seconds_,
            move_direction);
        DeviceSupportSystem::run(device_support_context);
    });

    run_profiled_system(GS1_RUNTIME_PROFILE_SYSTEM_ECOLOGY, [&]()
    {
        auto ecology_context = make_site_system_context<EcologySystem>(
            *campaign_,
            *active_site_run_,
            message_queue_,
            fixed_step_seconds_,
            move_direction);
        EcologySystem::run(ecology_context);
    });

    run_profiled_system(GS1_RUNTIME_PROFILE_SYSTEM_INVENTORY, [&]()
    {
        auto inventory_context = make_site_system_context<InventorySystem>(
            *campaign_,
            *active_site_run_,
            message_queue_,
            fixed_step_seconds_,
            move_direction);
        InventorySystem::run(inventory_context);
    });

    run_profiled_system(GS1_RUNTIME_PROFILE_SYSTEM_CRAFT, [&]()
    {
        auto craft_context = make_site_system_context<CraftSystem>(
            *campaign_,
            *active_site_run_,
            message_queue_,
            fixed_step_seconds_,
            move_direction);
        CraftSystem::run(craft_context);
    });

    run_profiled_system(GS1_RUNTIME_PROFILE_SYSTEM_TASK_BOARD, [&]()
    {
        auto task_board_context = make_site_system_context<TaskBoardSystem>(
            *campaign_,
            *active_site_run_,
            message_queue_,
            fixed_step_seconds_,
            move_direction);
        TaskBoardSystem::run(task_board_context);
    });

    run_profiled_system(GS1_RUNTIME_PROFILE_SYSTEM_ECONOMY_PHONE, [&]()
    {
        auto economy_context = make_site_system_context<EconomyPhoneSystem>(
            *campaign_,
            *active_site_run_,
            message_queue_,
            fixed_step_seconds_,
            move_direction);
        EconomyPhoneSystem::run(economy_context);
    });

    run_profiled_system(GS1_RUNTIME_PROFILE_SYSTEM_PHONE_PANEL, [&]()
    {
        auto phone_panel_context = make_site_system_context<PhonePanelSystem>(
            *campaign_,
            *active_site_run_,
            message_queue_,
            fixed_step_seconds_,
            move_direction);
        PhonePanelSystem::run(phone_panel_context);
    });

    run_profiled_system(GS1_RUNTIME_PROFILE_SYSTEM_FAILURE_RECOVERY, [&]()
    {
        auto failure_context = make_site_system_context<FailureRecoverySystem>(
            *campaign_,
            *active_site_run_,
            message_queue_,
            fixed_step_seconds_,
            move_direction);
        FailureRecoverySystem::run(failure_context);
    });

    run_profiled_system(GS1_RUNTIME_PROFILE_SYSTEM_SITE_COMPLETION, [&]()
    {
        auto completion_context = make_site_system_context<SiteCompletionSystem>(
            *campaign_,
            *active_site_run_,
            message_queue_,
            fixed_step_seconds_,
            move_direction);
        SiteCompletionSystem::run(completion_context);
    });

    record_timing_sample(fixed_step_timing_, elapsed_milliseconds_since(fixed_step_started_at));
}
}  // namespace gs1
