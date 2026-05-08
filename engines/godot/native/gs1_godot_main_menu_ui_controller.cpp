#include "gs1_godot_main_menu_ui_controller.h"

#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/property_info.hpp>

using namespace godot;

namespace
{
constexpr std::int64_t k_ui_action_start_new_campaign = 1;
}

void Gs1GodotMainMenuUiController::_bind_methods()
{
    ClassDB::bind_method(D_METHOD("set_runtime_node_path", "path"), &Gs1GodotMainMenuUiController::set_runtime_node_path);
    ClassDB::bind_method(D_METHOD("get_runtime_node_path"), &Gs1GodotMainMenuUiController::get_runtime_node_path);
    ClassDB::bind_method(D_METHOD("on_start_campaign_pressed"), &Gs1GodotMainMenuUiController::on_start_campaign_pressed);
    ClassDB::bind_method(D_METHOD("on_menu_settings_pressed"), &Gs1GodotMainMenuUiController::on_menu_settings_pressed);
    ClassDB::bind_method(D_METHOD("on_quit_pressed"), &Gs1GodotMainMenuUiController::on_quit_pressed);

    ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "runtime_node_path"), "set_runtime_node_path", "get_runtime_node_path");
}

void Gs1GodotMainMenuUiController::_ready()
{
    cache_runtime_reference();
    cache_ui_references();
    action_panel_controller_.set_submit_ui_action_callback([this](std::int64_t action_type, std::int64_t target_id, std::int64_t arg0, std::int64_t arg1) {
        submit_ui_action(action_type, target_id, arg0, arg1);
    });
    wire_static_buttons();
    set_process(true);
}

void Gs1GodotMainMenuUiController::_process(double delta)
{
    (void)delta;
    cache_runtime_reference();
    cache_ui_references();
    action_panel_controller_.refresh_fixed_slot_actions_if_needed();
}

void Gs1GodotMainMenuUiController::cache_runtime_reference()
{
    if (runtime_node_ != nullptr || runtime_node_path_.is_empty())
    {
        return;
    }

    runtime_node_ = Object::cast_to<Gs1RuntimeNode>(get_node_or_null(runtime_node_path_));
    if (runtime_node_ != nullptr)
    {
        runtime_node_->subscribe_engine_messages(action_panel_controller_);
    }
}

void Gs1GodotMainMenuUiController::cache_ui_references()
{
    action_panel_controller_.cache_ui_references(*this);
}

void Gs1GodotMainMenuUiController::wire_static_buttons()
{
    if (buttons_wired_)
    {
        return;
    }

    bind_button(
        Object::cast_to<BaseButton>(find_child("StartCampaignButton", true, false)),
        callable_mp(this, &Gs1GodotMainMenuUiController::on_start_campaign_pressed));
    bind_button(
        Object::cast_to<BaseButton>(find_child("ContinueCampaignButton", true, false)),
        callable_mp(this, &Gs1GodotMainMenuUiController::on_start_campaign_pressed));
    bind_button(
        Object::cast_to<BaseButton>(find_child("MenuSettingsButton", true, false)),
        callable_mp(this, &Gs1GodotMainMenuUiController::on_menu_settings_pressed));
    bind_button(
        Object::cast_to<BaseButton>(find_child("QuitButton", true, false)),
        callable_mp(this, &Gs1GodotMainMenuUiController::on_quit_pressed));

    buttons_wired_ = true;
}

void Gs1GodotMainMenuUiController::bind_button(BaseButton* button, const Callable& callback)
{
    if (button == nullptr || button->is_connected("pressed", callback))
    {
        return;
    }
    button->connect("pressed", callback);
}

void Gs1GodotMainMenuUiController::submit_ui_action(std::int64_t action_type, std::int64_t target_id, std::int64_t arg0, std::int64_t arg1)
{
    if (runtime_node_ != nullptr)
    {
        (void)runtime_node_->submit_ui_action(action_type, target_id, arg0, arg1);
    }
}

void Gs1GodotMainMenuUiController::on_start_campaign_pressed()
{
    submit_ui_action(k_ui_action_start_new_campaign, 0, 0, 0);
}

void Gs1GodotMainMenuUiController::on_menu_settings_pressed()
{
}

void Gs1GodotMainMenuUiController::on_quit_pressed()
{
    if (SceneTree* tree = get_tree())
    {
        tree->quit();
    }
}

void Gs1GodotMainMenuUiController::set_runtime_node_path(const NodePath& path)
{
    disconnect_runtime_subscriptions();
    runtime_node_path_ = path;
}

NodePath Gs1GodotMainMenuUiController::get_runtime_node_path() const
{
    return runtime_node_path_;
}

void Gs1GodotMainMenuUiController::disconnect_runtime_subscriptions()
{
    if (runtime_node_ != nullptr)
    {
        runtime_node_->unsubscribe_engine_messages(action_panel_controller_);
        runtime_node_ = nullptr;
    }
}

void Gs1GodotMainMenuUiController::apply_bootstrap_app_state(int app_state)
{
    Gs1EngineMessage message {};
    message.type = GS1_ENGINE_MESSAGE_SET_APP_STATE;
    auto& payload = message.emplace_payload<Gs1EngineMessageSetAppStateData>();
    payload.app_state = static_cast<Gs1AppState>(app_state);
    action_panel_controller_.handle_engine_message(message);
}
