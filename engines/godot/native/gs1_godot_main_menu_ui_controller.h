#pragma once

#include "gs1_godot_action_panel_controller.h"
#include "gs1_godot_runtime_node.h"

#include <godot_cpp/classes/base_button.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/core/binder_common.hpp>
#include <godot_cpp/variant/callable.hpp>
#include <godot_cpp/variant/node_path.hpp>

#include <cstdint>

class Gs1GodotMainMenuUiController final : public godot::Control
{
    GDCLASS(Gs1GodotMainMenuUiController, godot::Control)

public:
    Gs1GodotMainMenuUiController() = default;
    ~Gs1GodotMainMenuUiController() override = default;

    void _ready() override;
    void _process(double delta) override;

    void set_runtime_node_path(const godot::NodePath& path);
    [[nodiscard]] godot::NodePath get_runtime_node_path() const;

    void disconnect_runtime_subscriptions();
    void apply_bootstrap_app_state(int app_state);

protected:
    static void _bind_methods();

private:
    void cache_runtime_reference();
    void cache_ui_references();
    void wire_static_buttons();
    void bind_button(godot::BaseButton* button, const godot::Callable& callback);
    void submit_ui_action(std::int64_t action_type, std::int64_t target_id = 0, std::int64_t arg0 = 0, std::int64_t arg1 = 0);
    void on_start_campaign_pressed();
    void on_menu_settings_pressed();
    void on_quit_pressed();

private:
    godot::NodePath runtime_node_path_ {};
    Gs1RuntimeNode* runtime_node_ {nullptr};
    Gs1GodotActionPanelController action_panel_controller_ {};
    bool buttons_wired_ {false};
};
