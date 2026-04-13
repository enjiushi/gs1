#include "smoke_engine_host.h"
#include "smoke_log.h"

#include <algorithm>
#include <cassert>
#include <utility>

namespace
{
const char* command_type_name(Gs1EngineCommandType type)
{
    switch (type)
    {
    case GS1_ENGINE_COMMAND_LOG_TEXT:
        return "LOG_TEXT";
    case GS1_ENGINE_COMMAND_SET_APP_STATE:
        return "SET_APP_STATE";
    case GS1_ENGINE_COMMAND_PRESENTATION_DIRTY:
        return "PRESENTATION_DIRTY";
    case GS1_ENGINE_COMMAND_BEGIN_REGIONAL_MAP_SNAPSHOT:
        return "BEGIN_REGIONAL_MAP_SNAPSHOT";
    case GS1_ENGINE_COMMAND_REGIONAL_MAP_SITE_UPSERT:
        return "REGIONAL_MAP_SITE_UPSERT";
    case GS1_ENGINE_COMMAND_REGIONAL_MAP_SITE_REMOVE:
        return "REGIONAL_MAP_SITE_REMOVE";
    case GS1_ENGINE_COMMAND_REGIONAL_MAP_LINK_UPSERT:
        return "REGIONAL_MAP_LINK_UPSERT";
    case GS1_ENGINE_COMMAND_REGIONAL_MAP_LINK_REMOVE:
        return "REGIONAL_MAP_LINK_REMOVE";
    case GS1_ENGINE_COMMAND_END_REGIONAL_MAP_SNAPSHOT:
        return "END_REGIONAL_MAP_SNAPSHOT";
    case GS1_ENGINE_COMMAND_BEGIN_UI_SETUP:
        return "BEGIN_UI_SETUP";
    case GS1_ENGINE_COMMAND_UI_ELEMENT_UPSERT:
        return "UI_ELEMENT_UPSERT";
    case GS1_ENGINE_COMMAND_END_UI_SETUP:
        return "END_UI_SETUP";
    case GS1_ENGINE_COMMAND_CLOSE_UI_SETUP:
        return "CLOSE_UI_SETUP";
    case GS1_ENGINE_COMMAND_BEGIN_SITE_SNAPSHOT:
        return "BEGIN_SITE_SNAPSHOT";
    case GS1_ENGINE_COMMAND_SITE_TILE_UPSERT:
        return "SITE_TILE_UPSERT";
    case GS1_ENGINE_COMMAND_SITE_WORKER_UPDATE:
        return "SITE_WORKER_UPDATE";
    case GS1_ENGINE_COMMAND_SITE_CAMP_UPDATE:
        return "SITE_CAMP_UPDATE";
    case GS1_ENGINE_COMMAND_SITE_WEATHER_UPDATE:
        return "SITE_WEATHER_UPDATE";
    case GS1_ENGINE_COMMAND_SITE_INVENTORY_SLOT_UPSERT:
        return "SITE_INVENTORY_SLOT_UPSERT";
    case GS1_ENGINE_COMMAND_SITE_TASK_UPSERT:
        return "SITE_TASK_UPSERT";
    case GS1_ENGINE_COMMAND_SITE_TASK_REMOVE:
        return "SITE_TASK_REMOVE";
    case GS1_ENGINE_COMMAND_SITE_PHONE_LISTING_UPSERT:
        return "SITE_PHONE_LISTING_UPSERT";
    case GS1_ENGINE_COMMAND_SITE_PHONE_LISTING_REMOVE:
        return "SITE_PHONE_LISTING_REMOVE";
    case GS1_ENGINE_COMMAND_END_SITE_SNAPSHOT:
        return "END_SITE_SNAPSHOT";
    case GS1_ENGINE_COMMAND_HUD_STATE:
        return "HUD_STATE";
    case GS1_ENGINE_COMMAND_NOTIFICATION_PUSH:
        return "NOTIFICATION_PUSH";
    case GS1_ENGINE_COMMAND_SITE_RESULT_READY:
        return "SITE_RESULT_READY";
    case GS1_ENGINE_COMMAND_PLAY_ONE_SHOT_CUE:
        return "PLAY_ONE_SHOT_CUE";
    default:
        return "NONE";
    }
}

const char* app_state_name(Gs1AppState app_state)
{
    switch (app_state)
    {
    case GS1_APP_STATE_BOOT:
        return "BOOT";
    case GS1_APP_STATE_MAIN_MENU:
        return "MAIN_MENU";
    case GS1_APP_STATE_CAMPAIGN_SETUP:
        return "CAMPAIGN_SETUP";
    case GS1_APP_STATE_REGIONAL_MAP:
        return "REGIONAL_MAP";
    case GS1_APP_STATE_SITE_LOADING:
        return "SITE_LOADING";
    case GS1_APP_STATE_SITE_ACTIVE:
        return "SITE_ACTIVE";
    case GS1_APP_STATE_SITE_PAUSED:
        return "SITE_PAUSED";
    case GS1_APP_STATE_SITE_RESULT:
        return "SITE_RESULT";
    case GS1_APP_STATE_CAMPAIGN_END:
        return "CAMPAIGN_END";
    default:
        return "NONE";
    }
}

const char* ui_setup_name(Gs1UiSetupId setup_id)
{
    switch (setup_id)
    {
    case GS1_UI_SETUP_MAIN_MENU:
        return "MAIN_MENU";
    case GS1_UI_SETUP_REGIONAL_MAP_SELECTION:
        return "REGIONAL_MAP_SELECTION";
    case GS1_UI_SETUP_SITE_RESULT:
        return "SITE_RESULT";
    default:
        return "NONE";
    }
}

const char* ui_setup_presentation_type_name(Gs1UiSetupPresentationType presentation_type)
{
    switch (presentation_type)
    {
    case GS1_UI_SETUP_PRESENTATION_NORMAL:
        return "NORMAL";
    case GS1_UI_SETUP_PRESENTATION_OVERLAY:
        return "OVERLAY";
    case GS1_UI_SETUP_PRESENTATION_NONE:
    default:
        return "NONE";
    }
}

const char* ui_action_name(Gs1UiActionType action_type)
{
    switch (action_type)
    {
    case GS1_UI_ACTION_START_NEW_CAMPAIGN:
        return "START_NEW_CAMPAIGN";
    case GS1_UI_ACTION_SELECT_DEPLOYMENT_SITE:
        return "SELECT_DEPLOYMENT_SITE";
    case GS1_UI_ACTION_START_SITE_ATTEMPT:
        return "START_SITE_ATTEMPT";
    case GS1_UI_ACTION_RETURN_TO_REGIONAL_MAP:
        return "RETURN_TO_REGIONAL_MAP";
    case GS1_UI_ACTION_CLEAR_DEPLOYMENT_SITE_SELECTION:
        return "CLEAR_DEPLOYMENT_SITE_SELECTION";
    default:
        return "NONE";
    }
}

const char* projection_mode_name(Gs1ProjectionMode mode)
{
    switch (mode)
    {
    case GS1_PROJECTION_MODE_SNAPSHOT:
        return "SNAPSHOT";
    case GS1_PROJECTION_MODE_DELTA:
        return "DELTA";
    default:
        return "UNKNOWN";
    }
}

const char* site_state_name(Gs1SiteState site_state)
{
    switch (site_state)
    {
    case GS1_SITE_STATE_LOCKED:
        return "LOCKED";
    case GS1_SITE_STATE_AVAILABLE:
        return "AVAILABLE";
    case GS1_SITE_STATE_COMPLETED:
        return "COMPLETED";
    default:
        return "UNKNOWN";
    }
}

const char* weather_phase_name(Gs1WeatherEventPhase phase)
{
    switch (phase)
    {
    case GS1_WEATHER_EVENT_PHASE_NONE:
        return "NONE";
    case GS1_WEATHER_EVENT_PHASE_WARNING:
        return "WARNING";
    case GS1_WEATHER_EVENT_PHASE_BUILD:
        return "BUILD";
    case GS1_WEATHER_EVENT_PHASE_PEAK:
        return "PEAK";
    case GS1_WEATHER_EVENT_PHASE_DECAY:
        return "DECAY";
    case GS1_WEATHER_EVENT_PHASE_AFTERMATH:
        return "AFTERMATH";
    default:
        return "UNKNOWN";
    }
}

const char* site_attempt_result_name(Gs1SiteAttemptResult result)
{
    switch (result)
    {
    case GS1_SITE_ATTEMPT_RESULT_COMPLETED:
        return "COMPLETED";
    case GS1_SITE_ATTEMPT_RESULT_FAILED:
        return "FAILED";
    case GS1_SITE_ATTEMPT_RESULT_NONE:
    default:
        return "NONE";
    }
}

bool ui_action_matches(const Gs1UiAction& requested, const Gs1UiAction& available)
{
    if (requested.type != available.type)
    {
        return false;
    }

    if (requested.target_id != 0U && requested.target_id != available.target_id)
    {
        return false;
    }

    if (requested.arg0 != 0ULL && requested.arg0 != available.arg0)
    {
        return false;
    }

    if (requested.arg1 != 0ULL && requested.arg1 != available.arg1)
    {
        return false;
    }

    return true;
}
}  // namespace

SmokeEngineHost::SmokeEngineHost(
    const Gs1RuntimeApi& api,
    Gs1RuntimeHandle* runtime,
    LogMode log_mode) noexcept
    : api_(&api)
    , runtime_(runtime)
    , log_mode_(log_mode)
{
    smoke_log::infof("[ENGINE][BOOT] smoke host ready\n");
}

void SmokeEngineHost::queue_feedback_event(const Gs1FeedbackEvent& event)
{
    pending_feedback_events_.push_back(event);
}

void SmokeEngineHost::update(double delta_seconds)
{
    update(delta_seconds, nullptr);
}

void SmokeEngineHost::update(double delta_seconds, const Gs1InputSnapshot* input_override)
{
    frame_number_ += 1U;
    current_frame_command_entries_.clear();

    apply_inflight_script_directive();
    submit_host_events(pending_pre_phase1_host_events_, "pre_phase1");

    Gs1InputSnapshot input_snapshot {};
    input_snapshot.struct_size = sizeof(Gs1InputSnapshot);
    input_snapshot.frame_number = frame_number_;
    if (input_override != nullptr)
    {
        input_snapshot = *input_override;
        input_snapshot.struct_size = sizeof(Gs1InputSnapshot);
        input_snapshot.frame_number = frame_number_;
    }

    Gs1Phase1Request phase1_request {};
    phase1_request.struct_size = sizeof(Gs1Phase1Request);
    phase1_request.real_delta_seconds = delta_seconds;
    phase1_request.input = &input_snapshot;

    Gs1Phase1Result phase1_result {};
    const auto phase1_status = api_->run_phase1(runtime_, &phase1_request, &phase1_result);
    assert(phase1_status == GS1_STATUS_OK);
    const bool log_phase1_summary =
        log_mode_ == LogMode::Verbose ||
        phase1_result.processed_host_event_count > 0U ||
        phase1_result.engine_commands_queued > 0U;
    if (log_phase1_summary)
    {
        smoke_log::infof("[ENGINE][FRAME %llu] phase1 status=%u host_events=%u fixed_steps=%u queued=%u\n",
            static_cast<unsigned long long>(frame_number_),
            static_cast<unsigned>(phase1_status),
            static_cast<unsigned>(phase1_result.processed_host_event_count),
            static_cast<unsigned>(phase1_result.fixed_steps_executed),
            static_cast<unsigned>(phase1_result.engine_commands_queued));
    }
    phase1_fixed_steps_executed_ = phase1_result.fixed_steps_executed;
    phase1_processed_host_event_count_ = phase1_result.processed_host_event_count;

    flush_engine_commands("phase1");

    queue_between_phase_ui_action_if_ready();
    submit_host_events(pending_between_phase_host_events_, "between_phases");

    if (!pending_feedback_events_.empty())
    {
        const auto status = api_->submit_feedback_events(
            runtime_,
            pending_feedback_events_.data(),
            static_cast<std::uint32_t>(pending_feedback_events_.size()));
        assert(status == GS1_STATUS_OK);
        smoke_log::infof("[ENGINE][FRAME %llu] submit_feedback_events status=%u count=%u\n",
            static_cast<unsigned long long>(frame_number_),
            static_cast<unsigned>(status),
            static_cast<unsigned>(pending_feedback_events_.size()));
        pending_feedback_events_.clear();
    }

    Gs1Phase2Request phase2_request {};
    phase2_request.struct_size = sizeof(Gs1Phase2Request);

    Gs1Phase2Result phase2_result {};
    const auto phase2_status = api_->run_phase2(runtime_, &phase2_request, &phase2_result);
    assert(phase2_status == GS1_STATUS_OK);
    const bool log_phase2_summary =
        log_mode_ == LogMode::Verbose ||
        phase2_result.processed_host_event_count > 0U ||
        phase2_result.processed_feedback_event_count > 0U ||
        phase2_result.engine_commands_queued > 0U;
    if (log_phase2_summary)
    {
        smoke_log::infof("[ENGINE][FRAME %llu] phase2 status=%u host_events=%u feedback_events=%u queued=%u\n",
            static_cast<unsigned long long>(frame_number_),
            static_cast<unsigned>(phase2_status),
            static_cast<unsigned>(phase2_result.processed_host_event_count),
            static_cast<unsigned>(phase2_result.processed_feedback_event_count),
            static_cast<unsigned>(phase2_result.engine_commands_queued));
    }
    phase2_processed_host_event_count_ = phase2_result.processed_host_event_count;

    flush_engine_commands("phase2");
    resolve_inflight_script_directive();
}

void SmokeEngineHost::queue_ui_action(const Gs1UiAction& action)
{
    pending_pre_phase1_host_events_.push_back(make_ui_action_event(action));
}

SmokeEngineHost::LiveStateSnapshot SmokeEngineHost::capture_live_state_snapshot() const
{
    LiveStateSnapshot snapshot {};
    snapshot.frame_number = frame_number_;
    snapshot.current_app_state = current_app_state_;
    snapshot.selected_site_id = selected_site_id_;
    snapshot.script_failed = script_failed_;
    snapshot.current_frame_command_entries = current_frame_command_entries_;

    const std::size_t log_start = command_logs_.size() > 40U ? (command_logs_.size() - 40U) : 0U;
    snapshot.command_log_tail.reserve(command_logs_.size() - log_start);
    for (std::size_t index = log_start; index < command_logs_.size(); ++index)
    {
        snapshot.command_log_tail.push_back(command_logs_[index]);
    }

    snapshot.active_ui_setups = snapshot_active_ui_setups();
    snapshot.regional_map_sites = snapshot_regional_map_sites();
    snapshot.regional_map_links = snapshot_regional_map_links();
    snapshot.active_site_snapshot = active_site_snapshot_;
    snapshot.hud_state = hud_state_;
    snapshot.site_result = site_result_;
    return snapshot;
}

bool SmokeEngineHost::set_inflight_script_directive(SmokeScriptDirective directive)
{
    if (inflight_script_directive_.has_value())
    {
        return false;
    }

    inflight_script_directive_ = std::move(directive);
    inflight_script_directive_started_ = false;

    smoke_log::infof("[SCRIPT][FRAME %llu] line %zu: %s\n",
        static_cast<unsigned long long>(frame_number_ + 1U),
        inflight_script_directive_->line_number,
        inflight_script_directive_->source_text.c_str());
    return true;
}

bool SmokeEngineHost::saw_command(Gs1EngineCommandType type) const noexcept
{
    for (const auto seen_type : seen_commands_)
    {
        if (seen_type == type)
        {
            return true;
        }
    }

    return false;
}

void SmokeEngineHost::queue_pre_phase1_ui_action_if_ready()
{
    (void)try_queue_ui_action_from_directive(pending_pre_phase1_host_events_);
}

void SmokeEngineHost::queue_between_phase_ui_action_if_ready()
{
    (void)try_queue_ui_action_from_directive(pending_between_phase_host_events_);
}

void SmokeEngineHost::submit_host_events(
    std::vector<Gs1HostEvent>& events,
    const char* stage_label)
{
    if (events.empty())
    {
        return;
    }

    const auto status = api_->submit_host_events(
        runtime_,
        events.data(),
        static_cast<std::uint32_t>(events.size()));
    assert(status == GS1_STATUS_OK);
    smoke_log::infof("[ENGINE][FRAME %llu] submit_host_events stage=%s status=%u count=%u\n",
        static_cast<unsigned long long>(frame_number_),
        stage_label,
        static_cast<unsigned>(status),
        static_cast<unsigned>(events.size()));
    events.clear();
}

void SmokeEngineHost::apply_inflight_script_directive()
{
    queue_pre_phase1_ui_action_if_ready();
}

void SmokeEngineHost::resolve_inflight_script_directive()
{
    if (!inflight_script_directive_.has_value())
    {
        return;
    }

    auto& directive = inflight_script_directive_.value();

    switch (directive.opcode)
    {
    case SmokeScriptOpcode::WaitFrames:
        if (directive.remaining_frames <= 1U)
        {
            clear_inflight_script_directive();
        }
        else
        {
            directive.remaining_frames -= 1U;
        }
        return;

    case SmokeScriptOpcode::ClickUiAction:
        if (inflight_script_directive_started_)
        {
            clear_inflight_script_directive();
        }
        return;

    case SmokeScriptOpcode::WaitAppState:
        if (current_app_state_ == directive.app_state)
        {
            clear_inflight_script_directive();
            return;
        }
        if (directive.remaining_frames <= 1U)
        {
            fail_inflight_script_directive("timed out waiting for app state");
            return;
        }
        directive.remaining_frames -= 1U;
        return;

    case SmokeScriptOpcode::WaitSelectedSite:
        if (selected_site_id_ == directive.site_id)
        {
            clear_inflight_script_directive();
            return;
        }
        if (directive.remaining_frames <= 1U)
        {
            fail_inflight_script_directive("timed out waiting for selected site");
            return;
        }
        directive.remaining_frames -= 1U;
        return;

    case SmokeScriptOpcode::AssertAppState:
        if (current_app_state_ != directive.app_state)
        {
            fail_inflight_script_directive("app state assertion failed");
            return;
        }
        clear_inflight_script_directive();
        return;

    case SmokeScriptOpcode::AssertSelectedSite:
        if (selected_site_id_ != directive.site_id)
        {
            fail_inflight_script_directive("selected site assertion failed");
            return;
        }
        clear_inflight_script_directive();
        return;

    case SmokeScriptOpcode::AssertSawCommand:
        if (!saw_command(directive.engine_command_type))
        {
            fail_inflight_script_directive("engine command was not observed");
            return;
        }
        clear_inflight_script_directive();
        return;

    case SmokeScriptOpcode::AssertLogContains:
        for (const auto& entry : command_logs_)
        {
            if (entry.find(directive.text) != std::string::npos)
            {
                clear_inflight_script_directive();
                return;
            }
        }
        fail_inflight_script_directive("log text was not found: " + directive.text);
        return;
    }
}

void SmokeEngineHost::clear_inflight_script_directive()
{
    inflight_script_directive_.reset();
    inflight_script_directive_started_ = false;
}

void SmokeEngineHost::fail_inflight_script_directive(const std::string& message)
{
    if (!inflight_script_directive_.has_value())
    {
        return;
    }

    smoke_log::errorf(
        "%s:%zu: %s\n",
        inflight_script_directive_->script_path.c_str(),
        inflight_script_directive_->line_number,
        message.c_str());

    script_failed_ = true;
    clear_inflight_script_directive();
}

void SmokeEngineHost::flush_engine_commands(const char* stage_label)
{
    Gs1EngineCommand command {};

    while (api_->pop_engine_command(runtime_, &command) == GS1_STATUS_OK)
    {
        seen_commands_.push_back(command.type);

        switch (command.type)
        {
        case GS1_ENGINE_COMMAND_SET_APP_STATE:
        {
            const auto& payload = command.payload_as<Gs1EngineCommandSetAppStateData>();
            current_app_state_ = payload.app_state;
            if (current_app_state_ == GS1_APP_STATE_MAIN_MENU ||
                current_app_state_ == GS1_APP_STATE_REGIONAL_MAP ||
                current_app_state_ == GS1_APP_STATE_CAMPAIGN_SETUP ||
                current_app_state_ == GS1_APP_STATE_CAMPAIGN_END)
            {
                active_site_snapshot_.reset();
                hud_state_.reset();
                site_result_.reset();
            }
            else if (current_app_state_ == GS1_APP_STATE_SITE_ACTIVE ||
                current_app_state_ == GS1_APP_STATE_SITE_LOADING ||
                current_app_state_ == GS1_APP_STATE_SITE_PAUSED)
            {
                site_result_.reset();
            }
            break;
        }

        case GS1_ENGINE_COMMAND_BEGIN_REGIONAL_MAP_SNAPSHOT:
            apply_regional_map_snapshot_begin(command);
            break;
        case GS1_ENGINE_COMMAND_REGIONAL_MAP_SITE_UPSERT:
            apply_regional_map_site_upsert(command);
            break;
        case GS1_ENGINE_COMMAND_REGIONAL_MAP_SITE_REMOVE:
            apply_regional_map_site_remove(command);
            break;
        case GS1_ENGINE_COMMAND_REGIONAL_MAP_LINK_UPSERT:
            apply_regional_map_link_upsert(command);
            break;
        case GS1_ENGINE_COMMAND_REGIONAL_MAP_LINK_REMOVE:
            apply_regional_map_link_remove(command);
            break;

        case GS1_ENGINE_COMMAND_BEGIN_UI_SETUP:
            apply_ui_setup_begin(command);
            break;
        case GS1_ENGINE_COMMAND_CLOSE_UI_SETUP:
            apply_ui_setup_close(command);
            break;
        case GS1_ENGINE_COMMAND_UI_ELEMENT_UPSERT:
            apply_ui_element_upsert(command);
            break;
        case GS1_ENGINE_COMMAND_END_UI_SETUP:
            apply_ui_setup_end();
            break;

        case GS1_ENGINE_COMMAND_BEGIN_SITE_SNAPSHOT:
            apply_site_snapshot_begin(command);
            break;
        case GS1_ENGINE_COMMAND_SITE_TILE_UPSERT:
            apply_site_tile_upsert(command);
            break;
        case GS1_ENGINE_COMMAND_SITE_WORKER_UPDATE:
            apply_site_worker_update(command);
            break;
        case GS1_ENGINE_COMMAND_SITE_CAMP_UPDATE:
            apply_site_camp_update(command);
            break;
        case GS1_ENGINE_COMMAND_SITE_WEATHER_UPDATE:
            apply_site_weather_update(command);
            break;
        case GS1_ENGINE_COMMAND_END_SITE_SNAPSHOT:
            apply_site_snapshot_end();
            break;

        case GS1_ENGINE_COMMAND_HUD_STATE:
            apply_hud_state(command);
            break;
        case GS1_ENGINE_COMMAND_SITE_RESULT_READY:
            apply_site_result_ready(command);
            break;

        default:
            break;
        }

        const auto description = describe_command(command);
        std::string log_entry {};

        if (command.type == GS1_ENGINE_COMMAND_LOG_TEXT)
        {
            log_entry = std::string("[GAMEPLAY] ") + description;
            smoke_log::infof("[GAMEPLAY][FRAME %llu] %s\n",
                static_cast<unsigned long long>(frame_number_),
                description.c_str());
        }
        else
        {
            log_entry = std::string("[ENGINE][CMD] ") + stage_label + ": " + description;
            smoke_log::infof("[ENGINE][CMD][FRAME %llu] %s: %s\n",
                static_cast<unsigned long long>(frame_number_),
                stage_label,
                description.c_str());
        }

        command_logs_.push_back(log_entry);
        current_frame_command_entries_.push_back(std::move(log_entry));
    }
}

bool SmokeEngineHost::try_queue_ui_action_from_directive(std::vector<Gs1HostEvent>& destination)
{
    if (!inflight_script_directive_.has_value() || inflight_script_directive_started_)
    {
        return false;
    }

    if (inflight_script_directive_->opcode != SmokeScriptOpcode::ClickUiAction)
    {
        return false;
    }

    Gs1UiAction resolved_action {};
    if (!resolve_available_ui_action(inflight_script_directive_->ui_action, resolved_action))
    {
        return false;
    }

    destination.push_back(make_ui_action_event(resolved_action));
    inflight_script_directive_started_ = true;
    smoke_log::infof("[ENGINE][FRAME %llu] queued ui_action=%s target=%u\n",
        static_cast<unsigned long long>(frame_number_),
        ui_action_name(resolved_action.type),
        static_cast<unsigned>(resolved_action.target_id));
    return true;
}

bool SmokeEngineHost::resolve_available_ui_action(
    const Gs1UiAction& requested_action,
    Gs1UiAction& out_action) const
{
    if (requested_action.type == GS1_UI_ACTION_SELECT_DEPLOYMENT_SITE)
    {
        const auto site_it = regional_map_sites_.find(requested_action.target_id);
        if (site_it == regional_map_sites_.end())
        {
            return false;
        }

        if (site_it->second.site_state != GS1_SITE_STATE_AVAILABLE)
        {
            return false;
        }

        out_action = requested_action;
        return true;
    }

    for (const auto& [setup_id, setup] : active_ui_setups_)
    {
        (void)setup_id;
        for (const auto& element : setup.elements)
        {
            if ((element.flags & GS1_UI_ELEMENT_FLAG_DISABLED) != 0U)
            {
                continue;
            }

            if (ui_action_matches(requested_action, element.action))
            {
                out_action = element.action;
                return true;
            }
        }
    }

    return false;
}

void SmokeEngineHost::apply_ui_setup_begin(const Gs1EngineCommand& command)
{
    const auto& payload = command.payload_as<Gs1EngineCommandUiSetupData>();
    pending_ui_setup_ = PendingUiSetup {};
    pending_ui_setup_->setup_id = payload.setup_id;
    pending_ui_setup_->presentation_type = payload.presentation_type;
    pending_ui_setup_->context_id = payload.context_id;
}

void SmokeEngineHost::apply_ui_setup_close(const Gs1EngineCommand& command)
{
    const auto& payload = command.payload_as<Gs1EngineCommandCloseUiSetupData>();
    const auto setup_id = payload.setup_id;
    active_ui_setups_.erase(setup_id);

    if (pending_ui_setup_.has_value() && pending_ui_setup_->setup_id == setup_id)
    {
        pending_ui_setup_.reset();
    }
}

void SmokeEngineHost::apply_ui_element_upsert(const Gs1EngineCommand& command)
{
    if (!pending_ui_setup_.has_value())
    {
        return;
    }

    const auto& payload = command.payload_as<Gs1EngineCommandUiElementData>();
    ActiveUiElement element {};
    element.element_id = payload.element_id;
    element.element_type = payload.element_type;
    element.flags = payload.flags;
    element.action = payload.action;
    element.text = payload.text;
    pending_ui_setup_->elements.push_back(std::move(element));
}

void SmokeEngineHost::apply_ui_setup_end()
{
    if (!pending_ui_setup_.has_value())
    {
        return;
    }

    if (pending_ui_setup_->elements.empty())
    {
        active_ui_setups_.erase(pending_ui_setup_->setup_id);
        pending_ui_setup_.reset();
        return;
    }

    ActiveUiSetup setup {};
    setup.setup_id = pending_ui_setup_->setup_id;
    setup.presentation_type = pending_ui_setup_->presentation_type;
    setup.context_id = pending_ui_setup_->context_id;
    setup.elements = std::move(pending_ui_setup_->elements);

    if (setup.presentation_type == GS1_UI_SETUP_PRESENTATION_NORMAL)
    {
        for (auto it = active_ui_setups_.begin(); it != active_ui_setups_.end();)
        {
            if (it->second.presentation_type == GS1_UI_SETUP_PRESENTATION_NORMAL)
            {
                it = active_ui_setups_.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    active_ui_setups_[setup.setup_id] = std::move(setup);
    pending_ui_setup_.reset();
}

void SmokeEngineHost::apply_regional_map_snapshot_begin(const Gs1EngineCommand& command)
{
    const auto& payload = command.payload_as<Gs1EngineCommandRegionalMapSnapshotData>();
    if (payload.mode == GS1_PROJECTION_MODE_SNAPSHOT)
    {
        regional_map_sites_.clear();
        regional_map_links_.clear();
    }

    if (payload.selected_site_id != 0U)
    {
        selected_site_id_ = payload.selected_site_id;
    }
    else
    {
        selected_site_id_.reset();
    }
}

void SmokeEngineHost::apply_regional_map_site_upsert(const Gs1EngineCommand& command)
{
    const auto& payload = command.payload_as<Gs1EngineCommandRegionalMapSiteData>();
    RegionalMapSiteProjection projection {};
    projection.site_id = payload.site_id;
    projection.site_state = payload.site_state;
    projection.site_archetype_id = payload.site_archetype_id;
    projection.flags = payload.flags;
    projection.map_x = payload.map_x;
    projection.map_y = payload.map_y;
    projection.support_package_id = payload.support_package_id;
    projection.support_preview_mask = payload.support_preview_mask;
    regional_map_sites_[projection.site_id] = projection;

    if ((projection.flags & 1U) != 0U)
    {
        selected_site_id_ = projection.site_id;
    }
    else if (selected_site_id_.has_value() && selected_site_id_.value() == projection.site_id)
    {
        selected_site_id_.reset();
    }
}

void SmokeEngineHost::apply_regional_map_site_remove(const Gs1EngineCommand& command)
{
    const auto& payload = command.payload_as<Gs1EngineCommandRegionalMapSiteData>();
    const auto site_id = payload.site_id;
    regional_map_sites_.erase(site_id);
    if (selected_site_id_.has_value() && selected_site_id_.value() == site_id)
    {
        selected_site_id_.reset();
    }
}

void SmokeEngineHost::apply_regional_map_link_upsert(const Gs1EngineCommand& command)
{
    const auto& payload = command.payload_as<Gs1EngineCommandRegionalMapLinkData>();
    RegionalMapLinkProjection projection {};
    projection.from_site_id = payload.from_site_id;
    projection.to_site_id = payload.to_site_id;
    projection.flags = payload.flags;
    regional_map_links_[make_regional_map_link_key(projection.from_site_id, projection.to_site_id)] = projection;
}

void SmokeEngineHost::apply_regional_map_link_remove(const Gs1EngineCommand& command)
{
    const auto& payload = command.payload_as<Gs1EngineCommandRegionalMapLinkData>();
    regional_map_links_.erase(make_regional_map_link_key(
        payload.from_site_id,
        payload.to_site_id));
}

void SmokeEngineHost::apply_site_snapshot_begin(const Gs1EngineCommand& command)
{
    const auto& payload = command.payload_as<Gs1EngineCommandSiteSnapshotData>();
    if (payload.mode == GS1_PROJECTION_MODE_DELTA && active_site_snapshot_.has_value())
    {
        pending_site_snapshot_ = active_site_snapshot_.value();
    }
    else
    {
        pending_site_snapshot_ = SiteSnapshotProjection {};
    }

    pending_site_snapshot_->site_id = payload.site_id;
    pending_site_snapshot_->site_archetype_id = payload.site_archetype_id;
    pending_site_snapshot_->width = payload.width;
    pending_site_snapshot_->height = payload.height;

    if (payload.mode == GS1_PROJECTION_MODE_SNAPSHOT)
    {
        pending_site_snapshot_->tiles.clear();
        pending_site_snapshot_->worker.reset();
        pending_site_snapshot_->camp.reset();
        pending_site_snapshot_->weather.reset();
    }
}

void SmokeEngineHost::apply_site_tile_upsert(const Gs1EngineCommand& command)
{
    if (!pending_site_snapshot_.has_value())
    {
        return;
    }

    const auto& payload = command.payload_as<Gs1EngineCommandSiteTileData>();
    SiteTileProjection projection {};
    projection.x = payload.x;
    projection.y = payload.y;
    projection.terrain_type_id = payload.terrain_type_id;
    projection.plant_type_id = payload.plant_type_id;
    projection.structure_type_id = payload.structure_type_id;
    projection.ground_cover_type_id = payload.ground_cover_type_id;
    projection.plant_density = payload.plant_density;
    projection.sand_burial = payload.sand_burial;

    auto& tiles = pending_site_snapshot_->tiles;
    const auto existing = std::find_if(tiles.begin(), tiles.end(), [&](const SiteTileProjection& tile) {
        return tile.x == projection.x && tile.y == projection.y;
    });
    if (existing != tiles.end())
    {
        *existing = projection;
    }
    else
    {
        tiles.push_back(projection);
    }
}

void SmokeEngineHost::apply_site_worker_update(const Gs1EngineCommand& command)
{
    if (!pending_site_snapshot_.has_value())
    {
        return;
    }

    const auto& payload = command.payload_as<Gs1EngineCommandWorkerData>();
    SiteWorkerProjection projection {};
    projection.tile_x = payload.tile_x;
    projection.tile_y = payload.tile_y;
    projection.facing_degrees = payload.facing_degrees;
    projection.health_normalized = payload.health_normalized;
    projection.hydration_normalized = payload.hydration_normalized;
    projection.energy_normalized = payload.energy_normalized;
    projection.flags = payload.flags;
    projection.current_action_kind = payload.current_action_kind;
    pending_site_snapshot_->worker = projection;
}

void SmokeEngineHost::apply_site_camp_update(const Gs1EngineCommand& command)
{
    if (!pending_site_snapshot_.has_value())
    {
        return;
    }

    const auto& payload = command.payload_as<Gs1EngineCommandCampData>();
    SiteCampProjection projection {};
    projection.tile_x = payload.tile_x;
    projection.tile_y = payload.tile_y;
    projection.durability_normalized = payload.durability_normalized;
    projection.flags = payload.flags;
    pending_site_snapshot_->camp = projection;
}

void SmokeEngineHost::apply_site_weather_update(const Gs1EngineCommand& command)
{
    if (!pending_site_snapshot_.has_value())
    {
        return;
    }

    const auto& payload = command.payload_as<Gs1EngineCommandWeatherData>();
    SiteWeatherProjection projection {};
    projection.heat = payload.heat;
    projection.wind = payload.wind;
    projection.dust = payload.dust;
    projection.event_template_id = payload.event_template_id;
    projection.event_phase = payload.event_phase;
    projection.phase_minutes_remaining = payload.phase_minutes_remaining;
    pending_site_snapshot_->weather = projection;
}

void SmokeEngineHost::apply_site_snapshot_end()
{
    if (!pending_site_snapshot_.has_value())
    {
        return;
    }

    auto& tiles = pending_site_snapshot_->tiles;
    std::sort(tiles.begin(), tiles.end(), [](const SiteTileProjection& lhs, const SiteTileProjection& rhs) {
        if (lhs.y != rhs.y)
        {
            return lhs.y < rhs.y;
        }
        return lhs.x < rhs.x;
    });

    active_site_snapshot_ = std::move(pending_site_snapshot_);
    pending_site_snapshot_.reset();
}

void SmokeEngineHost::apply_hud_state(const Gs1EngineCommand& command)
{
    const auto& payload = command.payload_as<Gs1EngineCommandHudStateData>();
    HudProjection projection {};
    projection.player_health = payload.player_health;
    projection.player_hydration = payload.player_hydration;
    projection.player_energy = payload.player_energy;
    projection.current_money = payload.current_money;
    projection.active_task_count = payload.active_task_count;
    projection.warning_code = payload.warning_code;
    projection.current_action_kind = payload.current_action_kind;
    projection.site_completion_normalized = payload.site_completion_normalized;
    hud_state_ = projection;
}

void SmokeEngineHost::apply_site_result_ready(const Gs1EngineCommand& command)
{
    const auto& payload = command.payload_as<Gs1EngineCommandSiteResultData>();
    SiteResultProjection projection {};
    projection.site_id = payload.site_id;
    projection.result = payload.result;
    projection.newly_revealed_site_count = payload.newly_revealed_site_count;
    site_result_ = projection;
}

std::vector<SmokeEngineHost::ActiveUiSetup> SmokeEngineHost::snapshot_active_ui_setups() const
{
    std::vector<ActiveUiSetup> setups {};
    setups.reserve(active_ui_setups_.size());

    for (const auto& [setup_id, setup] : active_ui_setups_)
    {
        (void)setup_id;
        setups.push_back(setup);
    }

    return setups;
}

std::vector<SmokeEngineHost::RegionalMapSiteProjection> SmokeEngineHost::snapshot_regional_map_sites() const
{
    std::vector<RegionalMapSiteProjection> sites {};
    sites.reserve(regional_map_sites_.size());

    for (const auto& [site_id, site] : regional_map_sites_)
    {
        (void)site_id;
        sites.push_back(site);
    }

    return sites;
}

std::vector<SmokeEngineHost::RegionalMapLinkProjection> SmokeEngineHost::snapshot_regional_map_links() const
{
    std::vector<RegionalMapLinkProjection> links {};
    links.reserve(regional_map_links_.size());

    for (const auto& [link_key, link] : regional_map_links_)
    {
        (void)link_key;
        links.push_back(link);
    }

    return links;
}

std::uint64_t SmokeEngineHost::make_regional_map_link_key(
    std::uint32_t from_site_id,
    std::uint32_t to_site_id) noexcept
{
    const auto low = (from_site_id < to_site_id) ? from_site_id : to_site_id;
    const auto high = (from_site_id < to_site_id) ? to_site_id : from_site_id;
    return (static_cast<std::uint64_t>(low) << 32U) | static_cast<std::uint64_t>(high);
}

std::string SmokeEngineHost::describe_command(const Gs1EngineCommand& command)
{
    std::string description = command_type_name(command.type);

    switch (command.type)
    {
    case GS1_ENGINE_COMMAND_LOG_TEXT:
    {
        const auto& payload = command.payload_as<Gs1EngineCommandLogTextData>();
        description += " text=";
        description += payload.text;
        break;
    }

    case GS1_ENGINE_COMMAND_SET_APP_STATE:
    {
        const auto& payload = command.payload_as<Gs1EngineCommandSetAppStateData>();
        description += " state=";
        description += app_state_name(payload.app_state);
        break;
    }

    case GS1_ENGINE_COMMAND_PRESENTATION_DIRTY:
    {
        const auto& payload = command.payload_as<Gs1EngineCommandPresentationDirtyData>();
        description += " flags=" + std::to_string(payload.dirty_flags);
        description += " revision=" + std::to_string(payload.revision);
        break;
    }

    case GS1_ENGINE_COMMAND_BEGIN_REGIONAL_MAP_SNAPSHOT:
    {
        const auto& payload = command.payload_as<Gs1EngineCommandRegionalMapSnapshotData>();
        description += " mode=";
        description += projection_mode_name(payload.mode);
        description += " sites=" + std::to_string(payload.site_count);
        description += " links=" + std::to_string(payload.link_count);
        description += " selected=" + std::to_string(payload.selected_site_id);
        break;
    }

    case GS1_ENGINE_COMMAND_REGIONAL_MAP_SITE_UPSERT:
    {
        const auto& payload = command.payload_as<Gs1EngineCommandRegionalMapSiteData>();
        description += " site=" + std::to_string(payload.site_id);
        description += " state=";
        description += site_state_name(payload.site_state);
        description += " pos=(" + std::to_string(payload.map_x);
        description += "," + std::to_string(payload.map_y) + ")";
        break;
    }

    case GS1_ENGINE_COMMAND_REGIONAL_MAP_SITE_REMOVE:
    {
        const auto& payload = command.payload_as<Gs1EngineCommandRegionalMapSiteData>();
        description += " site=" + std::to_string(payload.site_id);
        break;
    }

    case GS1_ENGINE_COMMAND_REGIONAL_MAP_LINK_UPSERT:
    {
        const auto& payload = command.payload_as<Gs1EngineCommandRegionalMapLinkData>();
        description += " from=" + std::to_string(payload.from_site_id);
        description += " to=" + std::to_string(payload.to_site_id);
        break;
    }

    case GS1_ENGINE_COMMAND_REGIONAL_MAP_LINK_REMOVE:
    {
        const auto& payload = command.payload_as<Gs1EngineCommandRegionalMapLinkData>();
        description += " from=" + std::to_string(payload.from_site_id);
        description += " to=" + std::to_string(payload.to_site_id);
        break;
    }

    case GS1_ENGINE_COMMAND_BEGIN_UI_SETUP:
    {
        const auto& payload = command.payload_as<Gs1EngineCommandUiSetupData>();
        description += " setup=";
        description += ui_setup_name(payload.setup_id);
        description += " type=";
        description += ui_setup_presentation_type_name(payload.presentation_type);
        description += " elements=" + std::to_string(payload.element_count);
        description += " context=" + std::to_string(payload.context_id);
        break;
    }

    case GS1_ENGINE_COMMAND_CLOSE_UI_SETUP:
    {
        const auto& payload = command.payload_as<Gs1EngineCommandCloseUiSetupData>();
        description += " setup=";
        description += ui_setup_name(payload.setup_id);
        description += " type=";
        description += ui_setup_presentation_type_name(payload.presentation_type);
        break;
    }

    case GS1_ENGINE_COMMAND_UI_ELEMENT_UPSERT:
    {
        const auto& payload = command.payload_as<Gs1EngineCommandUiElementData>();
        description += " text=";
        description += payload.text;
        description += " action=";
        description += ui_action_name(payload.action.type);
        description += " target=" + std::to_string(payload.action.target_id);
        break;
    }

    case GS1_ENGINE_COMMAND_BEGIN_SITE_SNAPSHOT:
    {
        const auto& payload = command.payload_as<Gs1EngineCommandSiteSnapshotData>();
        description += " mode=";
        description += projection_mode_name(payload.mode);
        description += " site=" + std::to_string(payload.site_id);
        description += " size=" + std::to_string(payload.width);
        description += "x" + std::to_string(payload.height);
        break;
    }

    case GS1_ENGINE_COMMAND_SITE_TILE_UPSERT:
    {
        const auto& payload = command.payload_as<Gs1EngineCommandSiteTileData>();
        description += " x=" + std::to_string(payload.x);
        description += " y=" + std::to_string(payload.y);
        break;
    }

    case GS1_ENGINE_COMMAND_SITE_WORKER_UPDATE:
    {
        const auto& payload = command.payload_as<Gs1EngineCommandWorkerData>();
        description += " tile=(" + std::to_string(payload.tile_x);
        description += "," + std::to_string(payload.tile_y) + ")";
        description += " health=" + std::to_string(payload.health_normalized);
        break;
    }

    case GS1_ENGINE_COMMAND_SITE_CAMP_UPDATE:
    {
        const auto& payload = command.payload_as<Gs1EngineCommandCampData>();
        description += " tile=(" + std::to_string(payload.tile_x);
        description += "," + std::to_string(payload.tile_y) + ")";
        description += " durability=" + std::to_string(payload.durability_normalized);
        break;
    }

    case GS1_ENGINE_COMMAND_SITE_WEATHER_UPDATE:
    {
        const auto& payload = command.payload_as<Gs1EngineCommandWeatherData>();
        description += " heat=" + std::to_string(payload.heat);
        description += " wind=" + std::to_string(payload.wind);
        description += " dust=" + std::to_string(payload.dust);
        description += " phase=";
        description += weather_phase_name(payload.event_phase);
        break;
    }

    case GS1_ENGINE_COMMAND_HUD_STATE:
    {
        const auto& payload = command.payload_as<Gs1EngineCommandHudStateData>();
        description += " health=" + std::to_string(payload.player_health);
        description += " hydration=" + std::to_string(payload.player_hydration);
        description += " energy=" + std::to_string(payload.player_energy);
        description += " completion=" + std::to_string(payload.site_completion_normalized);
        break;
    }

    case GS1_ENGINE_COMMAND_SITE_RESULT_READY:
    {
        const auto& payload = command.payload_as<Gs1EngineCommandSiteResultData>();
        description += " site=" + std::to_string(payload.site_id);
        description += " result=";
        description += site_attempt_result_name(payload.result);
        description += " revealed=" + std::to_string(payload.newly_revealed_site_count);
        break;
    }

    default:
        break;
    }

    return description;
}

Gs1HostEvent SmokeEngineHost::make_ui_action_event(const Gs1UiAction& action) noexcept
{
    Gs1HostEvent event {};
    event.type = GS1_HOST_EVENT_UI_ACTION;
    event.payload.ui_action.action = action;
    return event;
}
