#include "gs1_godot_regional_map_ui_controller.h"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/object.hpp>
#include <godot_cpp/core/property_info.hpp>

using namespace godot;

void Gs1GodotRegionalMapUiController::_bind_methods()
{
    ClassDB::bind_method(D_METHOD("set_runtime_node_path", "path"), &Gs1GodotRegionalMapUiController::set_runtime_node_path);
    ClassDB::bind_method(D_METHOD("get_runtime_node_path"), &Gs1GodotRegionalMapUiController::get_runtime_node_path);
    ClassDB::bind_method(D_METHOD("set_ui_root_path", "path"), &Gs1GodotRegionalMapUiController::set_ui_root_path);
    ClassDB::bind_method(D_METHOD("get_ui_root_path"), &Gs1GodotRegionalMapUiController::get_ui_root_path);
    ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "runtime_node_path"), "set_runtime_node_path", "get_runtime_node_path");
    ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "ui_root_path"), "set_ui_root_path", "get_ui_root_path");
}

void Gs1GodotRegionalMapUiController::_ready()
{
    cache_runtime_reference();
    cache_ui_references();
    regional_map_hud_controller_.set_submit_ui_action_callback([this](std::int64_t action_type, std::int64_t target_id, std::int64_t arg0, std::int64_t arg1) {
        submit_ui_action(action_type, target_id, arg0, arg1);
    });
    regional_selection_panel_controller_.set_submit_ui_action_callback([this](std::int64_t action_type, std::int64_t target_id, std::int64_t arg0, std::int64_t arg1) {
        submit_ui_action(action_type, target_id, arg0, arg1);
    });
    regional_tech_tree_panel_controller_.set_submit_ui_action_callback([this](std::int64_t action_type, std::int64_t target_id, std::int64_t arg0, std::int64_t arg1) {
        submit_ui_action(action_type, target_id, arg0, arg1);
    });
    set_process(true);
}

void Gs1GodotRegionalMapUiController::_process(double delta)
{
    (void)delta;
    cache_runtime_reference();
    cache_ui_references();
    regional_map_hud_controller_.refresh_if_needed();
    regional_selection_panel_controller_.refresh_if_needed();
    regional_summary_panel_controller_.refresh_if_needed();
    regional_tech_tree_panel_controller_.refresh_if_needed();
    refresh_visibility();
}

void Gs1GodotRegionalMapUiController::cache_runtime_reference()
{
    if (runtime_node_ != nullptr || runtime_node_path_.is_empty())
    {
        return;
    }

    runtime_node_ = Object::cast_to<Gs1RuntimeNode>(get_node_or_null(runtime_node_path_));
    if (runtime_node_ == nullptr)
    {
        return;
    }

    runtime_node_->subscribe_engine_messages(*this);
    runtime_node_->subscribe_engine_messages(regional_map_hud_controller_);
    runtime_node_->subscribe_engine_messages(regional_selection_panel_controller_);
    runtime_node_->subscribe_engine_messages(regional_summary_panel_controller_);
    runtime_node_->subscribe_engine_messages(regional_tech_tree_panel_controller_);
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

void Gs1GodotRegionalMapUiController::cache_ui_references()
{
    Control* ui_root = resolve_ui_root();
    if (ui_root == nullptr)
    {
        return;
    }

    regional_map_hud_controller_.cache_ui_references(*ui_root);
    regional_selection_panel_controller_.cache_ui_references(*ui_root);
    regional_summary_panel_controller_.cache_ui_references(*ui_root);
    regional_tech_tree_panel_controller_.cache_ui_references(*ui_root);
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

void Gs1GodotRegionalMapUiController::submit_ui_action(std::int64_t action_type, std::int64_t target_id, std::int64_t arg0, std::int64_t arg1)
{
    if (runtime_node_ != nullptr)
    {
        runtime_node_->submit_ui_action(action_type, target_id, arg0, arg1);
    }
}

void Gs1GodotRegionalMapUiController::set_runtime_node_path(const NodePath& path)
{
    disconnect_runtime_subscriptions();
    runtime_node_path_ = path;
}

NodePath Gs1GodotRegionalMapUiController::get_runtime_node_path() const
{
    return runtime_node_path_;
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

void Gs1GodotRegionalMapUiController::disconnect_runtime_subscriptions()
{
    if (runtime_node_ != nullptr)
    {
        runtime_node_->unsubscribe_engine_messages(*this);
        runtime_node_->unsubscribe_engine_messages(regional_map_hud_controller_);
        runtime_node_->unsubscribe_engine_messages(regional_selection_panel_controller_);
        runtime_node_->unsubscribe_engine_messages(regional_summary_panel_controller_);
        runtime_node_->unsubscribe_engine_messages(regional_tech_tree_panel_controller_);
        runtime_node_ = nullptr;
    }
}

void Gs1GodotRegionalMapUiController::apply_bootstrap_app_state(int app_state)
{
    Gs1EngineMessage message {};
    message.type = GS1_ENGINE_MESSAGE_SET_APP_STATE;
    auto& payload = message.emplace_payload<Gs1EngineMessageSetAppStateData>();
    payload.app_state = static_cast<Gs1AppState>(app_state);

    handle_engine_message(message);
    regional_map_hud_controller_.handle_engine_message(message);
    regional_selection_panel_controller_.handle_engine_message(message);
    regional_summary_panel_controller_.handle_engine_message(message);
    regional_tech_tree_panel_controller_.handle_engine_message(message);
}
