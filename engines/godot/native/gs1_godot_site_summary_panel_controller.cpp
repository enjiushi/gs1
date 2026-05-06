#include "gs1_godot_site_summary_panel_controller.h"

#include <godot_cpp/variant/packed_string_array.hpp>

using namespace godot;

void Gs1GodotSiteSummaryPanelController::cache_ui_references(Control& owner)
{
    if (site_title_ == nullptr)
    {
        site_title_ = Object::cast_to<Label>(owner.find_child("SiteTitle", true, false));
    }
    if (site_summary_ == nullptr)
    {
        site_summary_ = Object::cast_to<RichTextLabel>(owner.find_child("SiteSummary", true, false));
    }
}

bool Gs1GodotSiteSummaryPanelController::handles_engine_message(Gs1EngineMessageType type) const noexcept
{
    switch (type)
    {
    case GS1_ENGINE_MESSAGE_SET_APP_STATE:
    case GS1_ENGINE_MESSAGE_BEGIN_SITE_SNAPSHOT:
    case GS1_ENGINE_MESSAGE_SITE_WORKER_UPDATE:
    case GS1_ENGINE_MESSAGE_SITE_WEATHER_UPDATE:
    case GS1_ENGINE_MESSAGE_HUD_STATE:
    case GS1_ENGINE_MESSAGE_SITE_ACTION_UPDATE:
    case GS1_ENGINE_MESSAGE_END_SITE_SNAPSHOT:
        return true;
    default:
        return false;
    }
}

void Gs1GodotSiteSummaryPanelController::handle_engine_message(const Gs1EngineMessage& message)
{
    switch (message.type)
    {
    case GS1_ENGINE_MESSAGE_SET_APP_STATE:
    {
        const auto& payload = message.payload_as<Gs1EngineMessageSetAppStateData>();
        current_app_state_ = payload.app_state;
        if (payload.app_state == GS1_APP_STATE_MAIN_MENU ||
            payload.app_state == GS1_APP_STATE_REGIONAL_MAP ||
            payload.app_state == GS1_APP_STATE_CAMPAIGN_END)
        {
            state_.reset();
        }
        break;
    }
    case GS1_ENGINE_MESSAGE_BEGIN_SITE_SNAPSHOT:
    {
        const auto& payload = message.payload_as<Gs1EngineMessageSiteSnapshotData>();
        if (!state_.has_value())
        {
            state_ = State {};
        }
        state_->site_id = payload.site_id;
        state_->width = payload.width;
        state_->height = payload.height;
        if (payload.mode == GS1_PROJECTION_MODE_SNAPSHOT)
        {
            state_->worker.reset();
            state_->weather.reset();
        }
        break;
    }
    case GS1_ENGINE_MESSAGE_SITE_WORKER_UPDATE:
        if (state_.has_value()) state_->worker = message.payload_as<Gs1EngineMessageWorkerData>();
        break;
    case GS1_ENGINE_MESSAGE_SITE_WEATHER_UPDATE:
        if (state_.has_value()) state_->weather = message.payload_as<Gs1EngineMessageWeatherData>();
        break;
    case GS1_ENGINE_MESSAGE_HUD_STATE:
        if (state_.has_value()) state_->hud = message.payload_as<Gs1EngineMessageHudStateData>();
        break;
    case GS1_ENGINE_MESSAGE_SITE_ACTION_UPDATE:
    {
        if (!state_.has_value())
        {
            break;
        }
        const auto& payload = message.payload_as<Gs1EngineMessageSiteActionData>();
        if ((payload.flags & GS1_SITE_ACTION_PRESENTATION_FLAG_CLEAR) != 0U)
        {
            state_->site_action.reset();
        }
        else
        {
            state_->site_action = payload;
        }
        break;
    }
    case GS1_ENGINE_MESSAGE_END_SITE_SNAPSHOT:
        break;
    default:
        break;
    }
    dirty_ = true;
}

void Gs1GodotSiteSummaryPanelController::handle_runtime_message_reset()
{
    current_app_state_.reset();
    state_.reset();
    dirty_ = true;
}

void Gs1GodotSiteSummaryPanelController::refresh_if_needed()
{
    if (!dirty_)
    {
        return;
    }

    const int app_state = current_app_state_.has_value() ? static_cast<int>(current_app_state_.value()) : 0;
    if ((app_state < 4 || app_state > 7) || !state_.has_value())
    {
        dirty_ = false;
        return;
    }

    if (site_title_ != nullptr)
    {
        site_title_->set_text(vformat("Site %d", static_cast<int>(state_->site_id)));
    }

    PackedStringArray site_lines;
    site_lines.push_back(vformat("Size: %d x %d", static_cast<int>(state_->width), static_cast<int>(state_->height)));

    if (state_->worker.has_value())
    {
        const auto& worker = state_->worker.value();
        site_lines.push_back(vformat(
            "Worker: (%.1f, %.1f)  Action %d",
            worker.tile_x,
            worker.tile_y,
            static_cast<int>(worker.current_action_kind)));
    }

    if (state_->weather.has_value())
    {
        const auto& weather = state_->weather.value();
        site_lines.push_back(vformat(
            "Weather H/W/D: %.1f / %.1f / %.1f",
            weather.heat,
            weather.wind,
            weather.dust));
    }

    if (state_->hud.has_value())
    {
        const auto& hud = state_->hud.value();
        site_lines.push_back(vformat(
            "Meters HP/HY/NO/EN/MO: %.1f / %.1f / %.1f / %.1f / %.1f",
            hud.player_health,
            hud.player_hydration,
            hud.player_nourishment,
            hud.player_energy,
            hud.player_morale));
    }

    if (state_->site_action.has_value())
    {
        const auto& site_action = state_->site_action.value();
        site_lines.push_back(vformat(
            "Current Action: kind %d  progress %.2f",
            static_cast<int>(site_action.action_kind),
            site_action.progress_normalized));
    }

    if (site_summary_ != nullptr)
    {
        site_summary_->set_text(String("\n").join(site_lines));
    }

    dirty_ = false;
}
