#include "runtime/game_runtime.h"

#include "app/campaign_factory.h"
#include "app/site_run_factory.h"
#include "commands/command_dispatcher.h"

#include <cstdio>
#include <cmath>
#include <cstring>
#include <limits>
#include <set>

namespace gs1
{
namespace
{
Gs1EngineCommand make_engine_command(
    Gs1EngineCommandType type)
{
    Gs1EngineCommand command {};
    command.struct_size = sizeof(Gs1EngineCommand);
    command.type = type;
    return command;
}

DayPhase resolve_day_phase(double world_time_minutes)
{
    constexpr double k_minutes_per_day = 1440.0;
    const double minute_in_day = std::fmod(world_time_minutes, k_minutes_per_day);

    if (minute_in_day < 360.0)
    {
        return DayPhase::Dawn;
    }
    if (minute_in_day < 900.0)
    {
        return DayPhase::Day;
    }
    if (minute_in_day < 1140.0)
    {
        return DayPhase::Dusk;
    }
    return DayPhase::Night;
}
}  // namespace

GameRuntime::GameRuntime(Gs1RuntimeCreateDesc create_desc)
    : create_desc_(create_desc)
{
    if (create_desc_.fixed_step_seconds > 0.0)
    {
        fixed_step_seconds_ = create_desc_.fixed_step_seconds;
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
        if (events[i].struct_size != sizeof(Gs1HostEvent))
        {
            return GS1_STATUS_INVALID_ARGUMENT;
        }

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
        if (events[i].struct_size != sizeof(Gs1FeedbackEvent))
        {
            return GS1_STATUS_INVALID_ARGUMENT;
        }

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

    if (request.input != nullptr && request.input->struct_size != sizeof(Gs1InputSnapshot))
    {
        return GS1_STATUS_INVALID_ARGUMENT;
    }

    out_result = {};
    out_result.struct_size = sizeof(Gs1Phase1Result);

    consume_input_snapshot(request.input);

    auto status = GS1_STATUS_OK;
    if (!boot_initialized_)
    {
        GameCommand boot_command {};
        boot_command.type = GameCommandType::OpenMainMenu;
        boot_command.payload = OpenMainMenuCommand {};
        command_queue_.push_back(boot_command);

        status = dispatch_queued_commands();
        if (status != GS1_STATUS_OK)
        {
            return status;
        }

        boot_initialized_ = true;
    }

    status = dispatch_host_events(out_result.processed_host_event_count);
    if (status != GS1_STATUS_OK)
    {
        return status;
    }

    if (!active_site_run_.has_value())
    {
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

    status = dispatch_queued_commands();
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

    auto status = dispatch_host_events(out_result.processed_host_event_count);
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
    switch (command.type)
    {
    case GameCommandType::OpenMainMenu:
    {
        queue_close_active_normal_ui_if_open();
        app_state_ = GS1_APP_STATE_MAIN_MENU;
        queue_app_state_command(app_state_);
        queue_main_menu_ui_commands();
        queue_log_command("Entered main menu.");
        return GS1_STATUS_OK;
    }

    case GameCommandType::StartNewCampaign:
    {
        const auto& payload = std::get<StartNewCampaignCommand>(command.payload);
        queue_close_ui_setup_if_open(GS1_UI_SETUP_MAIN_MENU);
        campaign_ = CampaignFactory::create_prototype_campaign(payload.campaign_seed, payload.campaign_days);
        app_state_ = GS1_APP_STATE_REGIONAL_MAP;
        active_site_run_.reset();
        rebuild_regional_map_caches();
        campaign_->app_state = app_state_;
        queue_app_state_command(app_state_);
        queue_regional_map_snapshot_commands();
        queue_regional_map_selection_ui_commands();
        queue_log_command("Started new GS1 campaign.");
        return GS1_STATUS_OK;
    }

    case GameCommandType::SelectDeploymentSite:
    {
        if (!campaign_.has_value())
        {
            return GS1_STATUS_INVALID_STATE;
        }

        const auto& payload = std::get<SelectDeploymentSiteCommand>(command.payload);
        auto site = find_site_mut(payload.site_id.value);
        if (!site.has_value())
        {
            return GS1_STATUS_NOT_FOUND;
        }

        if (site.value()->site_state != GS1_SITE_STATE_AVAILABLE)
        {
            return GS1_STATUS_INVALID_STATE;
        }

        const auto previous_selected_site_id = campaign_->regional_map_state.selected_site_id;
        if (campaign_->regional_map_state.selected_site_id.has_value() &&
            campaign_->regional_map_state.selected_site_id->value == payload.site_id.value)
        {
            return GS1_STATUS_OK;
        }

        campaign_->regional_map_state.selected_site_id = payload.site_id;
        campaign_->loadout_planner_state.selected_target_site_id = payload.site_id;
        if (previous_selected_site_id.has_value())
        {
            auto previous_site = find_site_mut(previous_selected_site_id->value);
            if (previous_site.has_value())
            {
                queue_regional_map_site_upsert_command(*previous_site.value());
            }
        }
        else
        {
            queue_close_ui_setup_if_open(GS1_UI_SETUP_REGIONAL_MAP_SELECTION);
        }
        queue_regional_map_site_upsert_command(*site.value());
        queue_regional_map_selection_ui_commands();
        queue_log_command("Selected deployment site.");
        return GS1_STATUS_OK;
    }

    case GameCommandType::ClearDeploymentSiteSelection:
    {
        if (!campaign_.has_value())
        {
            return GS1_STATUS_INVALID_STATE;
        }

        if (!campaign_->regional_map_state.selected_site_id.has_value())
        {
            return GS1_STATUS_OK;
        }

        const auto selected_site_id = campaign_->regional_map_state.selected_site_id.value();
        campaign_->regional_map_state.selected_site_id.reset();
        campaign_->loadout_planner_state.selected_target_site_id.reset();

        auto site = find_site_mut(selected_site_id.value);
        if (site.has_value())
        {
            queue_regional_map_site_upsert_command(*site.value());
        }

        queue_close_ui_setup_if_open(GS1_UI_SETUP_REGIONAL_MAP_SELECTION);
        queue_log_command("Cleared deployment site selection.");
        return GS1_STATUS_OK;
    }

    case GameCommandType::StartSiteAttempt:
    {
        if (!campaign_.has_value())
        {
            return GS1_STATUS_INVALID_STATE;
        }

        const auto& payload = std::get<StartSiteAttemptCommand>(command.payload);
        auto site = find_site_mut(payload.site_id.value);
        if (!site.has_value())
        {
            return GS1_STATUS_NOT_FOUND;
        }

        if (site.value()->site_state != GS1_SITE_STATE_AVAILABLE)
        {
            return GS1_STATUS_INVALID_STATE;
        }

        site.value()->attempt_count += 1U;
        queue_close_ui_setup_if_open(GS1_UI_SETUP_REGIONAL_MAP_SELECTION);
        active_site_run_ = SiteRunFactory::create_site_run(*campaign_, *site.value());
        campaign_->active_site_id = payload.site_id;
        app_state_ = GS1_APP_STATE_SITE_ACTIVE;
        campaign_->app_state = app_state_;
        queue_app_state_command(app_state_);
        queue_site_snapshot_commands();
        queue_hud_state_command();
        queue_log_command("Started site attempt.");
        return GS1_STATUS_OK;
    }

    case GameCommandType::ReturnToRegionalMap:
    {
        if (!campaign_.has_value())
        {
            return GS1_STATUS_INVALID_STATE;
        }

        if (!active_site_run_.has_value() && campaign_->app_state == GS1_APP_STATE_REGIONAL_MAP)
        {
            return GS1_STATUS_OK;
        }

        active_site_run_.reset();
        campaign_->active_site_id.reset();
        app_state_ = GS1_APP_STATE_REGIONAL_MAP;
        campaign_->app_state = app_state_;
        rebuild_regional_map_caches();
        queue_app_state_command(app_state_);
        queue_close_ui_setup_if_open(GS1_UI_SETUP_SITE_RESULT);
        queue_regional_map_snapshot_commands();
        queue_regional_map_selection_ui_commands();
        return GS1_STATUS_OK;
    }

    case GameCommandType::MarkSiteCompleted:
    {
        if (!campaign_.has_value() || !active_site_run_.has_value())
        {
            return GS1_STATUS_INVALID_STATE;
        }

        const auto& payload = std::get<MarkSiteCompletedCommand>(command.payload);
        auto site = find_site_mut(payload.site_id.value);
        if (!site.has_value())
        {
            return GS1_STATUS_NOT_FOUND;
        }

        active_site_run_->run_status = SiteRunStatus::Completed;
        site.value()->site_state = GS1_SITE_STATE_COMPLETED;
        std::uint32_t newly_revealed_site_count = 0U;

        for (const auto adjacent_site_id : site.value()->adjacent_site_ids)
        {
            auto adjacent_site = find_site_mut(adjacent_site_id.value);
            if (adjacent_site.has_value() && adjacent_site.value()->site_state == GS1_SITE_STATE_LOCKED)
            {
                adjacent_site.value()->site_state = GS1_SITE_STATE_AVAILABLE;
                campaign_->regional_map_state.revealed_site_ids.push_back(adjacent_site_id);
                newly_revealed_site_count += 1U;
            }
        }

        app_state_ = GS1_APP_STATE_SITE_RESULT;
        campaign_->app_state = app_state_;
        rebuild_regional_map_caches();
        queue_app_state_command(app_state_);
        queue_site_result_ui_commands(payload.site_id.value, GS1_SITE_ATTEMPT_RESULT_COMPLETED);
        queue_site_result_ready_command(
            payload.site_id.value,
            GS1_SITE_ATTEMPT_RESULT_COMPLETED,
            newly_revealed_site_count);
        return GS1_STATUS_OK;
    }

    case GameCommandType::MarkSiteFailed:
    {
        if (!campaign_.has_value() || !active_site_run_.has_value())
        {
            return GS1_STATUS_INVALID_STATE;
        }

        const auto& payload = std::get<MarkSiteFailedCommand>(command.payload);
        active_site_run_->run_status = SiteRunStatus::Failed;
        app_state_ = GS1_APP_STATE_SITE_RESULT;
        campaign_->app_state = app_state_;
        queue_app_state_command(app_state_);
        queue_site_result_ui_commands(payload.site_id.value, GS1_SITE_ATTEMPT_RESULT_FAILED);
        queue_site_result_ready_command(
            payload.site_id.value,
            GS1_SITE_ATTEMPT_RESULT_FAILED,
            0U);
        return GS1_STATUS_OK;
    }

    case GameCommandType::PresentLog:
    {
        const auto& payload = std::get<PresentLogCommand>(command.payload);
        queue_log_command(payload.text);
        return GS1_STATUS_OK;
    }
    }

    return GS1_STATUS_INVALID_ARGUMENT;
}

void GameRuntime::queue_log_command(const char* message)
{
    auto command = make_engine_command(GS1_ENGINE_COMMAND_LOG_TEXT);
    command.payload.log_text.level = GS1_LOG_LEVEL_INFO;
    strncpy_s(
        command.payload.log_text.text,
        sizeof(command.payload.log_text.text),
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
    command.payload.set_app_state.app_state = app_state;
    command.payload.set_app_state.frame_ordinal = 0U;
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
    command.payload.ui_setup.setup_id = setup_id;
    command.payload.ui_setup.mode = GS1_PROJECTION_MODE_SNAPSHOT;
    command.payload.ui_setup.presentation_type = presentation_type;
    command.payload.ui_setup.element_count = element_count;
    command.payload.ui_setup.context_id = context_id;
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
    command.payload.close_ui_setup.setup_id = setup_id;
    command.payload.close_ui_setup.presentation_type = presentation_type;
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
    command.payload.ui_element.element_id = element_id;
    command.payload.ui_element.element_type = element_type;
    command.payload.ui_element.flags = flags;
    command.payload.ui_element.action = action;
    strncpy_s(
        command.payload.ui_element.text,
        sizeof(command.payload.ui_element.text),
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
    begin.payload.regional_map_snapshot.mode = GS1_PROJECTION_MODE_SNAPSHOT;
    begin.payload.regional_map_snapshot.site_count = static_cast<std::uint32_t>(campaign_->sites.size());

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

    begin.payload.regional_map_snapshot.link_count = static_cast<std::uint32_t>(unique_links.size());
    begin.payload.regional_map_snapshot.selected_site_id =
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
        link_command.payload.regional_map_link.from_site_id = link.first;
        link_command.payload.regional_map_link.to_site_id = link.second;
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
    site_command.payload.regional_map_site.site_id = site.site_id.value;
    site_command.payload.regional_map_site.site_state = site.site_state;
    site_command.payload.regional_map_site.site_archetype_id = site.site_archetype_id;
    site_command.payload.regional_map_site.flags =
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

    site_command.payload.regional_map_site.map_x = static_cast<std::int32_t>(map_index * 160);
    site_command.payload.regional_map_site.map_y = 0;
    site_command.payload.regional_map_site.support_package_id =
        site.has_support_package_id ? site.support_package_id : 0U;
    site_command.payload.regional_map_site.support_preview_mask = 0U;
    engine_commands_.push_back(site_command);
}

void GameRuntime::queue_site_snapshot_commands()
{
    if (!active_site_run_.has_value())
    {
        return;
    }

    auto begin = make_engine_command(GS1_ENGINE_COMMAND_BEGIN_SITE_SNAPSHOT);
    begin.payload.site_snapshot.mode = GS1_PROJECTION_MODE_SNAPSHOT;
    begin.payload.site_snapshot.site_id = active_site_run_->site_id.value;
    begin.payload.site_snapshot.site_archetype_id = active_site_run_->site_archetype_id;
    begin.payload.site_snapshot.width = active_site_run_->tile_grid.width;
    begin.payload.site_snapshot.height = active_site_run_->tile_grid.height;
    engine_commands_.push_back(begin);

    for (std::uint32_t y = 0; y < active_site_run_->tile_grid.height; ++y)
    {
        for (std::uint32_t x = 0; x < active_site_run_->tile_grid.width; ++x)
        {
            const auto index =
                static_cast<std::size_t>(y) * static_cast<std::size_t>(active_site_run_->tile_grid.width) +
                static_cast<std::size_t>(x);

            auto tile_command = make_engine_command(GS1_ENGINE_COMMAND_SITE_TILE_UPSERT);
            tile_command.payload.site_tile.x = x;
            tile_command.payload.site_tile.y = y;
            tile_command.payload.site_tile.terrain_type_id = active_site_run_->tile_grid.terrain_type_ids[index];
            tile_command.payload.site_tile.plant_type_id = active_site_run_->tile_grid.plant_type_ids[index].value;
            tile_command.payload.site_tile.structure_type_id = active_site_run_->tile_grid.structure_type_ids[index].value;
            tile_command.payload.site_tile.ground_cover_type_id = active_site_run_->tile_grid.ground_cover_type_ids[index];
            tile_command.payload.site_tile.plant_density = active_site_run_->tile_grid.tile_plant_density[index];
            tile_command.payload.site_tile.sand_burial = active_site_run_->tile_grid.tile_sand_burial[index];
            engine_commands_.push_back(tile_command);
        }
    }

    auto worker_command = make_engine_command(GS1_ENGINE_COMMAND_SITE_WORKER_UPDATE);
    worker_command.payload.site_worker.tile_x = static_cast<float>(active_site_run_->worker.tile_coord.x);
    worker_command.payload.site_worker.tile_y = static_cast<float>(active_site_run_->worker.tile_coord.y);
    worker_command.payload.site_worker.facing_degrees = active_site_run_->worker.facing_degrees;
    worker_command.payload.site_worker.health_normalized = active_site_run_->worker.player_health / 100.0f;
    worker_command.payload.site_worker.hydration_normalized = active_site_run_->worker.player_hydration / 100.0f;
    worker_command.payload.site_worker.energy_normalized =
        active_site_run_->worker.player_energy_cap > 0.0f
            ? (active_site_run_->worker.player_energy / active_site_run_->worker.player_energy_cap)
            : 0.0f;
    worker_command.payload.site_worker.current_action_kind = static_cast<std::uint32_t>(active_site_run_->site_action.action_kind);
    engine_commands_.push_back(worker_command);

    auto camp_command = make_engine_command(GS1_ENGINE_COMMAND_SITE_CAMP_UPDATE);
    camp_command.payload.site_camp.tile_x = active_site_run_->camp.camp_anchor_tile.x;
    camp_command.payload.site_camp.tile_y = active_site_run_->camp.camp_anchor_tile.y;
    camp_command.payload.site_camp.durability_normalized = active_site_run_->camp.camp_durability / 100.0f;
    camp_command.payload.site_camp.flags =
        (active_site_run_->camp.delivery_point_operational ? 1U : 0U) |
        (active_site_run_->camp.shared_camp_storage_access_enabled ? 2U : 0U);
    engine_commands_.push_back(camp_command);

    auto weather_command = make_engine_command(GS1_ENGINE_COMMAND_SITE_WEATHER_UPDATE);
    weather_command.payload.site_weather.heat = active_site_run_->weather.weather_heat;
    weather_command.payload.site_weather.wind = active_site_run_->weather.weather_wind;
    weather_command.payload.site_weather.dust = active_site_run_->weather.weather_dust;
    weather_command.payload.site_weather.event_template_id =
        active_site_run_->event.active_event_template_id.has_value()
            ? active_site_run_->event.active_event_template_id->value
            : 0U;
    weather_command.payload.site_weather.event_phase =
        static_cast<Gs1WeatherEventPhase>(active_site_run_->event.event_phase);
    weather_command.payload.site_weather.phase_minutes_remaining =
        static_cast<float>(active_site_run_->event.phase_minutes_remaining);
    engine_commands_.push_back(weather_command);

    engine_commands_.push_back(make_engine_command(GS1_ENGINE_COMMAND_END_SITE_SNAPSHOT));
}

void GameRuntime::queue_hud_state_command()
{
    if (!active_site_run_.has_value())
    {
        return;
    }

    auto hud_command = make_engine_command(GS1_ENGINE_COMMAND_HUD_STATE);
    hud_command.payload.hud_state.player_health = active_site_run_->worker.player_health;
    hud_command.payload.hud_state.player_hydration = active_site_run_->worker.player_hydration;
    hud_command.payload.hud_state.player_energy = active_site_run_->worker.player_energy;
    hud_command.payload.hud_state.current_money = active_site_run_->economy.money;
    hud_command.payload.hud_state.active_task_count =
        static_cast<std::uint32_t>(active_site_run_->task_board.accepted_task_ids.size());
    hud_command.payload.hud_state.current_action_kind =
        static_cast<std::uint32_t>(active_site_run_->site_action.action_kind);
    hud_command.payload.hud_state.site_completion_normalized =
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
    command.payload.site_result.site_id = site_id;
    command.payload.site_result.result = result;
    command.payload.site_result.newly_revealed_site_count = newly_revealed_site_count;
    engine_commands_.push_back(command);
}

void GameRuntime::consume_input_snapshot(const Gs1InputSnapshot* input)
{
    if (input_snapshot_.current.struct_size == sizeof(Gs1InputSnapshot))
    {
        input_snapshot_.previous = input_snapshot_.current;
        input_snapshot_.has_previous = true;
    }

    input_snapshot_.current = {};
    input_snapshot_.current.struct_size = sizeof(Gs1InputSnapshot);

    if (input != nullptr)
    {
        input_snapshot_.current = *input;
    }
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
        out_command.payload = StartNewCampaignCommand {
            action.arg0,
            static_cast<std::uint32_t>(action.arg1)};
        return GS1_STATUS_OK;

    case GS1_UI_ACTION_SELECT_DEPLOYMENT_SITE:
        if (action.target_id == 0U)
        {
            return GS1_STATUS_INVALID_ARGUMENT;
        }
        out_command.type = GameCommandType::SelectDeploymentSite;
        out_command.payload = SelectDeploymentSiteCommand {SiteId {action.target_id}};
        return GS1_STATUS_OK;

    case GS1_UI_ACTION_CLEAR_DEPLOYMENT_SITE_SELECTION:
        out_command.type = GameCommandType::ClearDeploymentSiteSelection;
        out_command.payload = ClearDeploymentSiteSelectionCommand {};
        return GS1_STATUS_OK;

    case GS1_UI_ACTION_START_SITE_ATTEMPT:
        if (action.target_id == 0U)
        {
            return GS1_STATUS_INVALID_ARGUMENT;
        }
        out_command.type = GameCommandType::StartSiteAttempt;
        out_command.payload = StartSiteAttemptCommand {SiteId {action.target_id}};
        return GS1_STATUS_OK;

    case GS1_UI_ACTION_RETURN_TO_REGIONAL_MAP:
        out_command.type = GameCommandType::ReturnToRegionalMap;
        out_command.payload = ReturnToRegionalMapCommand {};
        return GS1_STATUS_OK;

    case GS1_UI_ACTION_NONE:
    default:
        return GS1_STATUS_INVALID_ARGUMENT;
    }
}

Gs1Status GameRuntime::dispatch_host_events(std::uint32_t& out_processed_count)
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

        switch (event.type)
        {
        case GS1_FEEDBACK_EVENT_NONE:
        case GS1_FEEDBACK_EVENT_PHYSICS_CONTACT:
        case GS1_FEEDBACK_EVENT_TRACE_HIT:
        case GS1_FEEDBACK_EVENT_ANIMATION_NOTIFY:
            break;

        default:
            return GS1_STATUS_INVALID_ARGUMENT;
        }
    }

    return GS1_STATUS_OK;
}

Gs1Status GameRuntime::dispatch_queued_commands()
{
    return CommandDispatcher::dispatch_all(*this);
}

void GameRuntime::rebuild_regional_map_caches()
{
    if (!campaign_.has_value())
    {
        return;
    }

    auto& map = campaign_->regional_map_state;
    map.available_site_ids.clear();
    map.completed_site_ids.clear();

    for (const auto& site : campaign_->sites)
    {
        if (site.site_state == GS1_SITE_STATE_AVAILABLE)
        {
            map.available_site_ids.push_back(site.site_id);
        }
        else if (site.site_state == GS1_SITE_STATE_COMPLETED)
        {
            map.completed_site_ids.push_back(site.site_id);
        }
    }

    if (map.selected_site_id.has_value())
    {
        bool selected_site_still_valid = false;
        for (const auto available_site_id : map.available_site_ids)
        {
            if (available_site_id == map.selected_site_id.value())
            {
                selected_site_still_valid = true;
                break;
            }
        }

        if (!selected_site_still_valid)
        {
            map.selected_site_id.reset();
        }
    }
}

void GameRuntime::run_fixed_step()
{
    if (!campaign_.has_value() || !active_site_run_.has_value())
    {
        return;
    }

    constexpr double k_step_minutes = 0.2;
    constexpr double k_minutes_per_day = 1440.0;

    campaign_->campaign_clock_minutes_elapsed += k_step_minutes;
    active_site_run_->clock.world_time_minutes += k_step_minutes;
    active_site_run_->clock.day_index =
        static_cast<std::uint32_t>(active_site_run_->clock.world_time_minutes / k_minutes_per_day);
    active_site_run_->clock.day_phase = resolve_day_phase(active_site_run_->clock.world_time_minutes);

    const auto elapsed_days = static_cast<std::uint32_t>(campaign_->campaign_clock_minutes_elapsed / k_minutes_per_day);
    campaign_->campaign_days_remaining =
        (elapsed_days >= campaign_->campaign_days_total) ? 0U : (campaign_->campaign_days_total - elapsed_days);

    if (active_site_run_->worker.player_health <= 0.0f)
    {
        GameCommand command {};
        command.type = GameCommandType::MarkSiteFailed;
        command.payload = MarkSiteFailedCommand{active_site_run_->site_id};
        command_queue_.push_back(command);
    }
    else if (active_site_run_->counters.fully_grown_tile_count >=
        active_site_run_->counters.site_completion_tile_threshold)
    {
        GameCommand command {};
        command.type = GameCommandType::MarkSiteCompleted;
        command.payload = MarkSiteCompletedCommand{active_site_run_->site_id};
        command_queue_.push_back(command);
    }
}

std::optional<SiteMetaState*> GameRuntime::find_site_mut(std::uint32_t site_id)
{
    if (!campaign_.has_value())
    {
        return std::nullopt;
    }

    for (auto& site : campaign_->sites)
    {
        if (site.site_id.value == site_id)
        {
            return &site;
        }
    }

    return std::nullopt;
}
}  // namespace gs1
