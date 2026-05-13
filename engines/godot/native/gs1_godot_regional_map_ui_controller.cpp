#include "gs1_godot_regional_map_ui_controller.h"

#include "gs1_godot_controller_context.h"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/object.hpp>
#include <godot_cpp/core/property_info.hpp>

using namespace godot;

void Gs1GodotRegionalMapUiController::_bind_methods()
{
    ClassDB::bind_method(D_METHOD("set_ui_root_path", "path"), &Gs1GodotRegionalMapUiController::set_ui_root_path);
    ClassDB::bind_method(D_METHOD("get_ui_root_path"), &Gs1GodotRegionalMapUiController::get_ui_root_path);
    ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "ui_root_path"), "set_ui_root_path", "get_ui_root_path");
}

void Gs1GodotRegionalMapUiController::_ready()
{
    cache_adapter_service();
    refresh_from_game_state_view();
    apply_surface_visibility();
}

void Gs1GodotRegionalMapUiController::_exit_tree()
{
    if (adapter_service_ != nullptr)
    {
        adapter_service_->unsubscribe_all(*this);
        adapter_service_ = nullptr;
    }
}

void Gs1GodotRegionalMapUiController::cache_adapter_service()
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

Control* Gs1GodotRegionalMapUiController::resolve_ui_root()
{
    if (ui_root_ != nullptr)
    {
        return ui_root_;
    }

    if (ui_root_path_.is_empty() || ui_root_path_ == NodePath("."))
    {
        ui_root_ = this;
        return ui_root_;
    }

    ui_root_ = Object::cast_to<Control>(get_node_or_null(ui_root_path_));
    if (ui_root_ == nullptr)
    {
        ui_root_ = this;
    }
    return ui_root_;
}

bool Gs1GodotRegionalMapUiController::handles_engine_message(Gs1EngineMessageType type) const noexcept
{
    return type == GS1_ENGINE_MESSAGE_PRESENTATION_DIRTY ||
        type == GS1_ENGINE_MESSAGE_SET_APP_STATE;
}

void Gs1GodotRegionalMapUiController::handle_engine_message(const Gs1EngineMessage& message)
{
    if (message.type == GS1_ENGINE_MESSAGE_PRESENTATION_DIRTY ||
        message.type == GS1_ENGINE_MESSAGE_SET_APP_STATE)
    {
        refresh_from_game_state_view();
    }

    apply_surface_visibility();
}

void Gs1GodotRegionalMapUiController::handle_runtime_message_reset()
{
    selection_visible_ = false;
    apply_surface_visibility();
}

void Gs1GodotRegionalMapUiController::refresh_from_game_state_view()
{
    if (adapter_service_ == nullptr)
    {
        selection_visible_ = false;
        return;
    }

    Gs1GameStateView view {};
    if (!adapter_service_->get_game_state_view(view) ||
        view.has_campaign == 0U ||
        view.campaign == nullptr)
    {
        selection_visible_ = false;
        return;
    }

    selection_visible_ = view.campaign->selected_site_id > 0;
}

void Gs1GodotRegionalMapUiController::apply_surface_visibility()
{
    Control* ui_root = resolve_ui_root();
    if (ui_root == nullptr)
    {
        return;
    }

    if (Control* hud = Object::cast_to<Control>(ui_root->find_child("RegionalMapHud", true, false)))
    {
        hud->set_visible(true);
    }
    if (Control* panel = Object::cast_to<Control>(ui_root->find_child("RegionalSelectionPanel", true, false)))
    {
        panel->set_visible(selection_visible_);
    }
    if (Control* overlay = Object::cast_to<Control>(ui_root->find_child("TechOverlay", true, false)))
    {
        const bool tech_tree_visible =
            adapter_service_ != nullptr &&
            adapter_service_->ui_session_state().regional_tech.open;
        overlay->set_visible(tech_tree_visible);
    }
}

void Gs1GodotRegionalMapUiController::set_ui_root_path(const NodePath& path)
{
    ui_root_path_ = path;
    ui_root_ = nullptr;
}

NodePath Gs1GodotRegionalMapUiController::get_ui_root_path() const
{
    return ui_root_path_;
}

