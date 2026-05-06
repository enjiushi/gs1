#include "gs1_godot_scene_router_control.h"

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/packed_scene.hpp>
#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/object.hpp>
#include <godot_cpp/core/property_info.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

void Gs1GodotSceneRouterControl::_bind_methods()
{
    ClassDB::bind_method(D_METHOD("set_runtime_node_path", "path"), &Gs1GodotSceneRouterControl::set_runtime_node_path);
    ClassDB::bind_method(D_METHOD("get_runtime_node_path"), &Gs1GodotSceneRouterControl::get_runtime_node_path);
    ClassDB::bind_method(D_METHOD("set_scene_host_path", "path"), &Gs1GodotSceneRouterControl::set_scene_host_path);
    ClassDB::bind_method(D_METHOD("get_scene_host_path"), &Gs1GodotSceneRouterControl::get_scene_host_path);
    ClassDB::bind_method(D_METHOD("set_main_menu_scene_path", "path"), &Gs1GodotSceneRouterControl::set_main_menu_scene_path);
    ClassDB::bind_method(D_METHOD("get_main_menu_scene_path"), &Gs1GodotSceneRouterControl::get_main_menu_scene_path);
    ClassDB::bind_method(D_METHOD("set_regional_map_scene_path", "path"), &Gs1GodotSceneRouterControl::set_regional_map_scene_path);
    ClassDB::bind_method(D_METHOD("get_regional_map_scene_path"), &Gs1GodotSceneRouterControl::get_regional_map_scene_path);
    ClassDB::bind_method(D_METHOD("set_site_session_scene_path", "path"), &Gs1GodotSceneRouterControl::set_site_session_scene_path);
    ClassDB::bind_method(D_METHOD("get_site_session_scene_path"), &Gs1GodotSceneRouterControl::get_site_session_scene_path);

    ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "runtime_node_path"), "set_runtime_node_path", "get_runtime_node_path");
    ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "scene_host_path"), "set_scene_host_path", "get_scene_host_path");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "main_menu_scene_path"), "set_main_menu_scene_path", "get_main_menu_scene_path");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "regional_map_scene_path"), "set_regional_map_scene_path", "get_regional_map_scene_path");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "site_session_scene_path"), "set_site_session_scene_path", "get_site_session_scene_path");
}

void Gs1GodotSceneRouterControl::_ready()
{
    set_process(true);
    cache_nodes();
    ensure_active_scene();
}

void Gs1GodotSceneRouterControl::_process(double delta)
{
    (void)delta;
    cache_nodes();
    ensure_active_scene();
}

void Gs1GodotSceneRouterControl::cache_nodes()
{
    if (runtime_node_ == nullptr)
    {
        runtime_node_ = Object::cast_to<Gs1RuntimeNode>(get_node_or_null(runtime_node_path_));
        if (runtime_node_ != nullptr)
        {
            runtime_node_->subscribe_engine_messages(*this);
        }
    }
    if (scene_host_ == nullptr)
    {
        scene_host_ = Object::cast_to<Control>(get_node_or_null(scene_host_path_));
    }
}

void Gs1GodotSceneRouterControl::ensure_active_scene()
{
    if (runtime_node_ == nullptr || scene_host_ == nullptr)
    {
        return;
    }

    int app_state = last_app_state_ >= 0 ? last_app_state_ : APP_STATE_BOOT;
    const ScreenKind desired_kind = desired_screen_kind(app_state);
    Node* active_scene = Object::cast_to<Node>(ObjectDB::get_instance(active_scene_id_));
    if (active_scene != nullptr && desired_kind == active_screen_kind_ && app_state == last_app_state_)
    {
        return;
    }

    switch_to_scene(desired_kind);
    last_app_state_ = app_state;
}

bool Gs1GodotSceneRouterControl::handles_engine_message(Gs1EngineMessageType type) const noexcept
{
    return type == GS1_ENGINE_MESSAGE_SET_APP_STATE;
}

void Gs1GodotSceneRouterControl::handle_engine_message(const Gs1EngineMessage& message)
{
    if (message.type != GS1_ENGINE_MESSAGE_SET_APP_STATE)
    {
        return;
    }
    last_app_state_ = static_cast<int>(message.payload_as<Gs1EngineMessageSetAppStateData>().app_state);
    ensure_active_scene();
}

void Gs1GodotSceneRouterControl::handle_runtime_message_reset()
{
    last_app_state_ = APP_STATE_BOOT;
}

Gs1GodotSceneRouterControl::ScreenKind Gs1GodotSceneRouterControl::desired_screen_kind(int app_state) const noexcept
{
    switch (app_state)
    {
    case APP_STATE_SITE_ACTIVE:
    case APP_STATE_SITE_PAUSED:
    case APP_STATE_SITE_RESULT:
        return SCREEN_KIND_SITE_SESSION;
    case APP_STATE_CAMPAIGN_SETUP:
    case APP_STATE_REGIONAL_MAP:
    case APP_STATE_SITE_LOADING:
        return SCREEN_KIND_REGIONAL_MAP;
    case APP_STATE_BOOT:
    case APP_STATE_MAIN_MENU:
    case APP_STATE_CAMPAIGN_END:
    default:
        return SCREEN_KIND_MAIN_MENU;
    }
}

String Gs1GodotSceneRouterControl::scene_path_for(ScreenKind kind) const
{
    switch (kind)
    {
    case SCREEN_KIND_MAIN_MENU:
        return main_menu_scene_path_;
    case SCREEN_KIND_REGIONAL_MAP:
        return regional_map_scene_path_;
    case SCREEN_KIND_SITE_SESSION:
        return site_session_scene_path_;
    case SCREEN_KIND_NONE:
    default:
        return String();
    }
}

void Gs1GodotSceneRouterControl::switch_to_scene(ScreenKind kind)
{
    if (scene_host_ == nullptr)
    {
        return;
    }

    if (Node* active_scene = Object::cast_to<Node>(ObjectDB::get_instance(active_scene_id_)))
    {
        if (auto* screen = Object::cast_to<Gs1GodotMainScreenControl>(active_scene))
        {
            screen->disconnect_runtime_subscriptions();
        }
        if (auto* site_view = Object::cast_to<Gs1SiteViewNode>(active_scene->find_child("SiteView", true, false)))
        {
            site_view->set_runtime_node_path(NodePath());
        }
        scene_host_->remove_child(active_scene);
        active_scene->queue_free();
    }

    active_scene_id_ = ObjectID();
    active_screen_kind_ = SCREEN_KIND_NONE;

    const String scene_path = scene_path_for(kind);
    if (scene_path.is_empty())
    {
        return;
    }

    Node* instance = instantiate_scene(scene_path);
    if (instance == nullptr)
    {
        UtilityFunctions::push_warning(vformat("GS1 Godot router could not load scene: %s", scene_path));
        return;
    }

    configure_scene_instance(instance);
    scene_host_->add_child(instance);
    if (auto* control = Object::cast_to<Control>(instance))
    {
        control->set_anchors_preset(Control::PRESET_FULL_RECT);
    }

    active_scene_id_ = instance->get_instance_id();
    active_screen_kind_ = kind;
}

Node* Gs1GodotSceneRouterControl::instantiate_scene(const String& scene_path) const
{
    ResourceLoader* resource_loader = ResourceLoader::get_singleton();
    if (resource_loader == nullptr)
    {
        return nullptr;
    }

    Ref<Resource> resource = resource_loader->load(scene_path);
    Ref<PackedScene> packed_scene = resource;
    if (packed_scene.is_null())
    {
        return nullptr;
    }

    return packed_scene->instantiate();
}

void Gs1GodotSceneRouterControl::configure_scene_instance(Node* instance) const
{
    if (instance == nullptr)
    {
        return;
    }

    if (auto* screen = Object::cast_to<Gs1GodotMainScreenControl>(instance))
    {
        screen->set_runtime_node_path(NodePath("../../Runtime"));
        screen->apply_bootstrap_app_state(last_app_state_ >= 0 ? last_app_state_ : APP_STATE_BOOT);
    }

    if (auto* site_view = Object::cast_to<Gs1SiteViewNode>(instance->find_child("SiteView", true, false)))
    {
        site_view->set_runtime_node_path(NodePath("../../../Runtime"));
    }
}

void Gs1GodotSceneRouterControl::set_runtime_node_path(const NodePath& path)
{
    if (runtime_node_ != nullptr)
    {
        runtime_node_->unsubscribe_engine_messages(*this);
    }
    runtime_node_path_ = path;
    runtime_node_ = nullptr;
}

NodePath Gs1GodotSceneRouterControl::get_runtime_node_path() const
{
    return runtime_node_path_;
}

void Gs1GodotSceneRouterControl::set_scene_host_path(const NodePath& path)
{
    scene_host_path_ = path;
    scene_host_ = nullptr;
}

NodePath Gs1GodotSceneRouterControl::get_scene_host_path() const
{
    return scene_host_path_;
}

void Gs1GodotSceneRouterControl::set_main_menu_scene_path(const String& path)
{
    main_menu_scene_path_ = path;
}

String Gs1GodotSceneRouterControl::get_main_menu_scene_path() const
{
    return main_menu_scene_path_;
}

void Gs1GodotSceneRouterControl::set_regional_map_scene_path(const String& path)
{
    regional_map_scene_path_ = path;
}

String Gs1GodotSceneRouterControl::get_regional_map_scene_path() const
{
    return regional_map_scene_path_;
}

void Gs1GodotSceneRouterControl::set_site_session_scene_path(const String& path)
{
    site_session_scene_path_ = path;
}

String Gs1GodotSceneRouterControl::get_site_session_scene_path() const
{
    return site_session_scene_path_;
}
