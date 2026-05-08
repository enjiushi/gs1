#pragma once

#include "gs1_godot_input_dispatcher.h"

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/core/binder_common.hpp>
#include <godot_cpp/variant/node_path.hpp>

class Gs1GodotSiteSessionUiController;
class Gs1SiteViewNode;

class Gs1GodotSiteSceneController final : public godot::Control
{
    GDCLASS(Gs1GodotSiteSceneController, godot::Control)

public:
    void _ready() override;
    void _process(double delta) override;
    void _input(const godot::Ref<godot::InputEvent>& event) override;

protected:
    static void _bind_methods();

private:
    void cache_nodes();

    godot::NodePath ui_controller_path_ {"SiteUiController"};
    godot::NodePath site_view_path_ {"SiteView"};
    Gs1GodotSiteSessionUiController* ui_controller_ {nullptr};
    Gs1SiteViewNode* site_view_ {nullptr};
    Gs1GodotInputDispatcher input_dispatcher_ {};
};
