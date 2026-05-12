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
#include <utility>

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


}  // namespace

RuntimeInvocation::RuntimeInvocation(GameRuntime& runtime) noexcept
    : runtime_(&runtime)
    , app_state_(&runtime.app_state_)
    , campaign_(&runtime.campaign_)
    , active_site_run_(&runtime.active_site_run_)
    , runtime_messages_(&runtime.runtime_messages_)
    , game_messages_(&runtime.message_queue_)
    , fixed_step_seconds_(&runtime.fixed_step_seconds_)
{
}

RuntimeInvocation::RuntimeInvocation(
    Gs1AppState& app_state,
    std::optional<CampaignState>& campaign,
    std::optional<SiteRunState>& active_site_run,
    std::deque<Gs1RuntimeMessage>& runtime_messages,
    GameMessageQueue& game_messages,
    double fixed_step_seconds,
    float move_direction_x,
    float move_direction_y,
    float move_direction_z,
    bool move_direction_present) noexcept
    : app_state_(&app_state)
    , campaign_(&campaign)
    , active_site_run_(&active_site_run)
    , runtime_messages_(&runtime_messages)
    , game_messages_(&game_messages)
    , fixed_step_seconds_(&fixed_step_seconds)
    , move_direction_ {
          move_direction_x,
          move_direction_y,
          move_direction_z,
          move_direction_present}
{
}

void RuntimeInvocation::push_game_message(const GameMessage& message)
{
    game_messages_->push_back(message);
}

void RuntimeInvocation::push_runtime_message(const Gs1RuntimeMessage& message)
{
    runtime_messages_->push_back(message);
}

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

    systems_.clear();
    fixed_step_systems_.clear();
    for (auto& subscribers : host_message_subscribers_)
    {
        subscribers.clear();
    }
    for (auto& subscribers : message_subscribers_)
    {
        subscribers.clear();
    }

    systems_.push_back(std::make_unique<CampaignFlowSystem>());
    systems_.push_back(std::make_unique<LoadoutPlannerSystem>());
    systems_.push_back(std::make_unique<FactionReputationSystem>());
    systems_.push_back(std::make_unique<TechnologySystem>());
    systems_.push_back(std::make_unique<ActionExecutionSystem>());
    systems_.push_back(std::make_unique<WeatherEventSystem>());
    systems_.push_back(std::make_unique<WorkerConditionSystem>());
    systems_.push_back(std::make_unique<EcologySystem>());
    systems_.push_back(std::make_unique<PlantWeatherContributionSystem>());
    systems_.push_back(std::make_unique<DeviceWeatherContributionSystem>());
    systems_.push_back(std::make_unique<TaskBoardSystem>());
    systems_.push_back(std::make_unique<PlacementValidationSystem>());
    systems_.push_back(std::make_unique<LocalWeatherResolveSystem>());
    systems_.push_back(std::make_unique<DeviceMaintenanceSystem>());
    systems_.push_back(std::make_unique<InventorySystem>());
    systems_.push_back(std::make_unique<CraftSystem>());
    systems_.push_back(std::make_unique<EconomyPhoneSystem>());
    systems_.push_back(std::make_unique<PhonePanelSystem>());
    systems_.push_back(std::make_unique<CampDurabilitySystem>());
    systems_.push_back(std::make_unique<DeviceSupportSystem>());
    systems_.push_back(std::make_unique<ModifierSystem>());
    systems_.push_back(std::make_unique<CampaignTimeSystem>());
    systems_.push_back(std::make_unique<SiteTimeSystem>());
    systems_.push_back(std::make_unique<SiteFlowSystem>());
    systems_.push_back(std::make_unique<FailureRecoverySystem>());
    systems_.push_back(std::make_unique<SiteCompletionSystem>());

    for (const auto& system : systems_)
    {
        append_runtime_subscribers(message_subscribers_, system->subscribed_game_messages(), *system);
        append_runtime_subscribers(host_message_subscribers_, system->subscribed_host_messages(), *system);
        if (system->fixed_step_order().has_value())
        {
            fixed_step_systems_.push_back(system.get());
        }
    }

    std::sort(
        fixed_step_systems_.begin(),
        fixed_step_systems_.end(),
        [](const IRuntimeSystem* lhs, const IRuntimeSystem* rhs)
        {
            return lhs->fixed_step_order().value_or(std::numeric_limits<std::uint32_t>::max()) <
                rhs->fixed_step_order().value_or(std::numeric_limits<std::uint32_t>::max());
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

    status = dispatch_host_messages(out_result.processed_host_message_count);
    if (status != GS1_STATUS_OK)
    {
        finish_phase();
        return status;
    }

    if (!active_site_run_.has_value())
    {
        out_result.runtime_messages_queued = static_cast<std::uint32_t>(runtime_messages_.size());
        finish_phase();
        return GS1_STATUS_OK;
    }

    if (app_state_ == GS1_APP_STATE_SITE_LOADING)
    {
        out_result.runtime_messages_queued = static_cast<std::uint32_t>(runtime_messages_.size());
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
    out_result.runtime_messages_queued = static_cast<std::uint32_t>(runtime_messages_.size());
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
    out_result.runtime_messages_queued = static_cast<std::uint32_t>(runtime_messages_.size());
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

    RuntimeInvocation invocation {*this};
    const auto& subscribers = message_subscribers_[message_type_index(message.type)];
    for (IRuntimeSystem* system : subscribers)
    {
        if (system == nullptr)
        {
            continue;
        }

        const auto profile_id = system->profile_system_id();
        const auto status = profile_id.has_value()
            ? dispatch_profiled_message(
                *profile_id,
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

    RuntimeInvocation invocation {*this};
    const auto& subscribers = host_message_subscribers_[host_message_type_index(message.type)];
    for (IRuntimeSystem* system : subscribers)
    {
        if (system == nullptr)
        {
            continue;
        }

        const auto status = system->process_host_message(invocation, message);
        if (status != GS1_STATUS_OK)
        {
            return status;
        }
    }

    if (GamePresentationCoordinator::subscribes_to_host_message(message.type))
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

    RuntimeInvocation invocation {*this};
    for (IRuntimeSystem* system : fixed_step_systems_)
    {
        if (system == nullptr)
        {
            continue;
        }

        const auto profile_id = system->profile_system_id();
        if (!profile_id.has_value())
        {
            system->run(invocation);
            continue;
        }

        run_profiled_system(
            *profile_id,
            [&]()
            {
                system->run(invocation);
            });
    }

    record_timing_sample(fixed_step_timing_, elapsed_milliseconds_since(fixed_step_started_at));
}
}  // namespace gs1

