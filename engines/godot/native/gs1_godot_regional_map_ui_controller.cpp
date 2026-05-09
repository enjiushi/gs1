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
    refresh_visibility();
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
    return type == GS1_ENGINE_MESSAGE_SET_APP_STATE ||
        type == GS1_ENGINE_MESSAGE_SET_UI_SURFACE_VISIBILITY;
}

void Gs1GodotRegionalMapUiController::handle_engine_message(const Gs1EngineMessage& message)
{
    if (message.type == GS1_ENGINE_MESSAGE_SET_APP_STATE)
    {
        last_app_state_ = static_cast<int>(message.payload_as<Gs1EngineMessageSetAppStateData>().app_state);
    }
    else if (message.type == GS1_ENGINE_MESSAGE_SET_UI_SURFACE_VISIBILITY)
    {
        const auto& payload = message.payload_as<Gs1EngineMessageUiSurfaceVisibilityData>();
        const bool visible = payload.visible != 0U;
        if (payload.surface_id == GS1_UI_SURFACE_REGIONAL_SELECTION_PANEL)
        {
            selection_visible_ = visible;
        }
        else if (payload.surface_id == GS1_UI_SURFACE_REGIONAL_TECH_TREE_OVERLAY)
        {
            tech_tree_visible_ = visible;
        }
    }

    refresh_visibility();
}

void Gs1GodotRegionalMapUiController::handle_runtime_message_reset()
{
    last_app_state_ = GS1_APP_STATE_BOOT;
    selection_visible_ = false;
    tech_tree_visible_ = false;
    refresh_visibility();
}

void Gs1GodotRegionalMapUiController::refresh_visibility()
{
    Control* ui_root = resolve_ui_root();
    if (ui_root == nullptr)
    {
        return;
    }

    const bool regional_visible =
        last_app_state_ == GS1_APP_STATE_REGIONAL_MAP ||
        last_app_state_ == GS1_APP_STATE_SITE_LOADING;

    if (Control* hud = Object::cast_to<Control>(ui_root->find_child("RegionalMapHud", true, false)))
    {
        hud->set_visible(regional_visible);
    }
    if (Control* panel = Object::cast_to<Control>(ui_root->find_child("RegionalSelectionPanel", true, false)))
    {
        if (!regional_visible || !selection_visible_)
        {
            panel->set_visible(false);
        }
    }
    if (Control* overlay = Object::cast_to<Control>(ui_root->find_child("TechOverlay", true, false)))
    {
        if (!regional_visible || !tech_tree_visible_)
        {
            overlay->set_visible(false);
        }
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

