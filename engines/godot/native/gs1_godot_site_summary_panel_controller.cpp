#include "gs1_godot_site_summary_panel_controller.h"

#include "gs1_godot_controller_context.h"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>

#include <algorithm>

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
    refresh_from_game_state_view();
    rebuild_summary();
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
    return type == GS1_ENGINE_MESSAGE_PRESENTATION_DIRTY ||
        type == GS1_ENGINE_MESSAGE_SET_APP_STATE;
}

void Gs1GodotSiteSummaryPanelController::refresh_from_game_state_view()
{
    state_.reset();
    if (adapter_service_ == nullptr)
    {
        return;
    }

    Gs1GameStateView view {};
    if (!adapter_service_->get_game_state_view(view) ||
        view.has_active_site == 0U ||
        view.active_site == nullptr)
    {
        return;
    }

    const Gs1SiteStateView& site = *view.active_site;
    State state {};
    state.site_id = site.site_id;
    state.width = site.tile_width;
    state.height = site.tile_height;
    state.worker_tile_x = site.worker.tile_x;
    state.worker_tile_y = site.worker.tile_y;
    state.weather_heat = site.weather.heat;
    state.weather_wind = site.weather.wind;
    state.weather_dust = site.weather.dust;
    state.health = site.worker.health;
    state.hydration = site.worker.hydration;
    state.nourishment = site.worker.nourishment;
    state.energy = site.worker.energy;
    state.morale = site.worker.morale;
    state.has_worker = site.worker.worker_entity_id != 0U;
    state.has_action = site.action.has_current_action != 0U;
    state.action_kind = site.action.action_kind;
    if (state.has_action && site.action.total_action_minutes > 0.0)
    {
        const double completed = site.action.total_action_minutes - site.action.remaining_action_minutes;
        state.action_progress = static_cast<float>(std::clamp(
            completed / site.action.total_action_minutes,
            0.0,
            1.0));
    }
    state_ = state;
}

void Gs1GodotSiteSummaryPanelController::handle_engine_message(const Gs1EngineMessage& message)
{
    if (message.type == GS1_ENGINE_MESSAGE_PRESENTATION_DIRTY ||
        message.type == GS1_ENGINE_MESSAGE_SET_APP_STATE)
    {
        refresh_from_game_state_view();
        rebuild_summary();
    }
}

void Gs1GodotSiteSummaryPanelController::handle_runtime_message_reset()
{
    state_.reset();
    rebuild_summary();
}

void Gs1GodotSiteSummaryPanelController::rebuild_summary()
{
    if (site_title_ == nullptr && site_summary_ == nullptr)
    {
        return;
    }

    if (!state_.has_value())
    {
        if (site_title_ != nullptr)
        {
            site_title_->set_text("Site");
        }
        if (site_summary_ != nullptr)
        {
            site_summary_->set_text(String());
        }
        return;
    }

    if (site_title_ != nullptr)
    {
        site_title_->set_text(vformat("Site %d", static_cast<int>(state_->site_id)));
    }

    PackedStringArray site_lines;
    site_lines.push_back(vformat("Size: %d x %d", static_cast<int>(state_->width), static_cast<int>(state_->height)));

    if (state_->has_worker)
    {
        site_lines.push_back(vformat(
            "Worker: (%.1f, %.1f)  Action %d",
            static_cast<double>(state_->worker_tile_x),
            static_cast<double>(state_->worker_tile_y),
            static_cast<int>(state_->action_kind)));
    }

    site_lines.push_back(vformat(
        "Weather H/W/D: %.1f / %.1f / %.1f",
        state_->weather_heat,
        state_->weather_wind,
        state_->weather_dust));

        site_lines.push_back(vformat(
            "Meters HP/HY/NO/EN/MO: %.1f / %.1f / %.1f / %.1f / %.1f",
            state_->health,
            state_->hydration,
            state_->nourishment,
            state_->energy,
            state_->morale));

    if (state_->has_action)
    {
        site_lines.push_back(vformat(
            "Current Action: kind %d  progress %.2f",
            static_cast<int>(state_->action_kind),
            state_->action_progress));
    }

    if (site_summary_ != nullptr)
    {
        site_summary_->set_text(String("\n").join(site_lines));
    }
}
