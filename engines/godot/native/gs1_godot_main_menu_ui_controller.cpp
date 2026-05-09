#include "gs1_godot_main_menu_ui_controller.h"

#include "gs1_godot_controller_context.h"

#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/core/class_db.hpp>

using namespace godot;

namespace
{
constexpr std::int64_t k_ui_action_start_new_campaign = 1;
}

void Gs1GodotMainMenuUiController::_bind_methods()
{
    ClassDB::bind_method(D_METHOD("on_start_campaign_pressed"), &Gs1GodotMainMenuUiController::on_start_campaign_pressed);
    ClassDB::bind_method(D_METHOD("on_menu_settings_pressed"), &Gs1GodotMainMenuUiController::on_menu_settings_pressed);
    ClassDB::bind_method(D_METHOD("on_quit_pressed"), &Gs1GodotMainMenuUiController::on_quit_pressed);
}

void Gs1GodotMainMenuUiController::_ready()
{
    cache_adapter_service();
    wire_static_buttons();
    set_process(true);
}

void Gs1GodotMainMenuUiController::_process(double delta)
{
    (void)delta;
    cache_adapter_service();
}

void Gs1GodotMainMenuUiController::cache_adapter_service()
{
    if (adapter_service_ != nullptr)
    {
        return;
    }

    adapter_service_ = gs1_resolve_adapter_service(this);
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
    if (adapter_service_ != nullptr)
    {
        (void)adapter_service_->submit_ui_action(action_type, target_id, arg0, arg1);
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

