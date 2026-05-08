#include "gs1_godot_site_scene_controller.h"

#include "gs1_godot_site_session_ui_controller.h"
#include "gs1_godot_site_view_node.h"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/object.hpp>

using namespace godot;

void Gs1GodotSiteSceneController::_bind_methods()
{
}

void Gs1GodotSiteSceneController::_ready()
{
    cache_nodes();
    set_process(true);
    set_process_input(true);
}

void Gs1GodotSiteSceneController::_process(double delta)
{
    (void)delta;
    cache_nodes();
}

void Gs1GodotSiteSceneController::_input(const Ref<InputEvent>& event)
{
    input_dispatcher_.dispatch(event);
}

void Gs1GodotSiteSceneController::cache_nodes()
{
    if (ui_controller_ == nullptr)
    {
        ui_controller_ = Object::cast_to<Gs1GodotSiteSessionUiController>(get_node_or_null(ui_controller_path_));
        if (ui_controller_ != nullptr)
        {
            ui_controller_->attach_input_dispatcher(input_dispatcher_);
        }
    }
    if (site_view_ == nullptr)
    {
        site_view_ = Object::cast_to<Gs1SiteViewNode>(get_node_or_null(site_view_path_));
    }
}
