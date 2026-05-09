#include "gs1_godot_site_summary_panel_controller.h"

#include "gs1_godot_controller_context.h"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>

using namespace godot;

void Gs1GodotSiteSummaryPanelController::_bind_methods()
{
}

void Gs1GodotSiteSummaryPanelController::_ready()
{
    cache_adapter_service();
    if (Control* owner = resolve_owner_control())
    {
        cache_ui_references(*owner);
    }
    set_process(true);
}

void Gs1GodotSiteSummaryPanelController::_process(double delta)
{
    (void)delta;
}

void Gs1GodotSiteSummaryPanelController::_exit_tree()
{
    if (adapter_service_ != nullptr)
    {
        adapter_service_->unsubscribe_all(*this);
        adapter_service_ = nullptr;
    }
}

void Gs1GodotSiteSummaryPanelController::cache_ui_references(Control& owner)
{
    owner_control_ = &owner;
    if (site_title_ == nullptr)
    {
        site_title_ = Object::cast_to<Label>(owner.find_child("SiteTitle", true, false));
    }
    if (site_summary_ == nullptr)
    {
        site_summary_ = Object::cast_to<RichTextLabel>(owner.find_child("SiteSummary", true, false));
    }
    rebuild_summary();
}

void Gs1GodotSiteSummaryPanelController::cache_adapter_service()
{
    if (adapter_service_ != nullptr)
    {
        return;
    }

    adapter_service_ = gs1_resolve_adapter_service(this);
    if (adapter_service_ != nullptr)
    {
        adapter_service_->subscribe_matching_messages(*this);
    }
}

Control* Gs1GodotSiteSummaryPanelController::resolve_owner_control()
{
    if (owner_control_ != nullptr)
    {
        return owner_control_;
    }
    owner_control_ = Object::cast_to<Control>(get_parent());
    if (owner_control_ == nullptr)
    {
        owner_control_ = this;
    }
    return owner_control_;
}

bool Gs1GodotSiteSummaryPanelController::handles_engine_message(Gs1EngineMessageType type) const noexcept
{
    switch (type)
    {
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
    rebuild_summary();
}

void Gs1GodotSiteSummaryPanelController::handle_runtime_message_reset()
{
    state_.reset();
    rebuild_summary();
}

void Gs1GodotSiteSummaryPanelController::rebuild_summary()
{
    if (!state_.has_value())
    {
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
}
