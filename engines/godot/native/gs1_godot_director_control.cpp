#include "gs1_godot_director_control.h"

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/packed_scene.hpp>
#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/object.hpp>
#include <godot_cpp/core/property_info.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

namespace
{
template <typename T>
T* find_controller(Node* scene_root, const char* node_name)
{
    if (scene_root == nullptr)
    {
        return nullptr;
    }
    if (auto* direct = Object::cast_to<T>(scene_root))
    {
        return direct;
    }
    return Object::cast_to<T>(scene_root->find_child(node_name, true, false));
}
}

void Gs1GodotDirectorControl::_bind_methods()
{
    ClassDB::bind_method(D_METHOD("set_scene_host_path", "path"), &Gs1GodotDirectorControl::set_scene_host_path);
    ClassDB::bind_method(D_METHOD("get_scene_host_path"), &Gs1GodotDirectorControl::get_scene_host_path);
    ClassDB::bind_method(D_METHOD("set_loading_scene_path", "path"), &Gs1GodotDirectorControl::set_loading_scene_path);
    ClassDB::bind_method(D_METHOD("get_loading_scene_path"), &Gs1GodotDirectorControl::get_loading_scene_path);
    ClassDB::bind_method(D_METHOD("set_main_menu_scene_path", "path"), &Gs1GodotDirectorControl::set_main_menu_scene_path);
    ClassDB::bind_method(D_METHOD("get_main_menu_scene_path"), &Gs1GodotDirectorControl::get_main_menu_scene_path);
    ClassDB::bind_method(D_METHOD("set_regional_map_scene_path", "path"), &Gs1GodotDirectorControl::set_regional_map_scene_path);
    ClassDB::bind_method(D_METHOD("get_regional_map_scene_path"), &Gs1GodotDirectorControl::get_regional_map_scene_path);
    ClassDB::bind_method(D_METHOD("set_site_session_scene_path", "path"), &Gs1GodotDirectorControl::set_site_session_scene_path);
    ClassDB::bind_method(D_METHOD("get_site_session_scene_path"), &Gs1GodotDirectorControl::get_site_session_scene_path);

    ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "scene_host_path"), "set_scene_host_path", "get_scene_host_path");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "loading_scene_path"), "set_loading_scene_path", "get_loading_scene_path");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "main_menu_scene_path"), "set_main_menu_scene_path", "get_main_menu_scene_path");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "regional_map_scene_path"), "set_regional_map_scene_path", "get_regional_map_scene_path");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "site_session_scene_path"), "set_site_session_scene_path", "get_site_session_scene_path");
}

void Gs1GodotDirectorControl::_ready()
{
    set_process(true);
    adapter_service_.subscribe(GS1_ENGINE_MESSAGE_SET_APP_STATE, *this);
    cache_nodes();
    ensure_active_scene();
}

void Gs1GodotDirectorControl::_process(double delta)
{
    adapter_service_.process_frame(delta);
    cache_nodes();
    poll_async_scene_switch();
    ensure_active_scene();
}

void Gs1GodotDirectorControl::cache_nodes()
{
    if (scene_host_ == nullptr)
    {
        scene_host_ = Object::cast_to<Control>(get_node_or_null(scene_host_path_));
    }
}

void Gs1GodotDirectorControl::ensure_active_scene()
{
    if (scene_host_ == nullptr)
    {
        return;
    }

    const int app_state = last_app_state_ >= 0 ? last_app_state_ : APP_STATE_BOOT;
    const ScreenKind desired_kind = desired_screen_kind(app_state);
    if (pending_async_target_kind_ != SCREEN_KIND_NONE)
    {
        return;
    }

    Node* active_scene = Object::cast_to<Node>(ObjectDB::get_instance(active_scene_id_));
    if (active_scene != nullptr && desired_kind == active_screen_kind_ && app_state == last_app_state_)
    {
        return;
    }

    if (should_async_load_transition(desired_kind))
    {
        begin_async_scene_switch(desired_kind);
        last_app_state_ = app_state;
        return;
    }

    switch_to_scene(desired_kind);
    last_app_state_ = app_state;
}

void Gs1GodotDirectorControl::begin_async_scene_switch(ScreenKind kind)
{
    ResourceLoader* resource_loader = ResourceLoader::get_singleton();
    const String scene_path = scene_path_for(kind);
    if (resource_loader == nullptr || scene_path.is_empty())
    {
        switch_to_scene(kind);
        return;
    }

    const Error status = resource_loader->load_threaded_request(
        scene_path,
        "PackedScene",
        true,
        ResourceLoader::CACHE_MODE_REUSE);
    if (status != OK)
    {
        switch_to_scene(kind);
        return;
    }

    pending_async_target_kind_ = kind;
    pending_async_scene_path_ = scene_path;
    switch_to_scene(SCREEN_KIND_LOADING);
}

void Gs1GodotDirectorControl::poll_async_scene_switch()
{
    if (pending_async_target_kind_ == SCREEN_KIND_NONE || pending_async_scene_path_.is_empty())
    {
        return;
    }

    ResourceLoader* resource_loader = ResourceLoader::get_singleton();
    if (resource_loader == nullptr)
    {
        const ScreenKind target_kind = pending_async_target_kind_;
        pending_async_target_kind_ = SCREEN_KIND_NONE;
        pending_async_scene_path_ = String();
        switch_to_scene(target_kind);
        return;
    }

    const ResourceLoader::ThreadLoadStatus status = resource_loader->load_threaded_get_status(pending_async_scene_path_);
    if (status == ResourceLoader::THREAD_LOAD_IN_PROGRESS)
    {
        return;
    }

    const ScreenKind target_kind = pending_async_target_kind_;
    const String target_path = pending_async_scene_path_;
    pending_async_target_kind_ = SCREEN_KIND_NONE;
    pending_async_scene_path_ = String();

    if (status != ResourceLoader::THREAD_LOAD_LOADED)
    {
        switch_to_scene(target_kind);
        return;
    }

    Ref<Resource> resource = resource_loader->load_threaded_get(target_path);
    Ref<PackedScene> packed_scene = resource;
    if (packed_scene.is_null())
    {
        switch_to_scene(target_kind);
        return;
    }

    switch_to_scene(target_kind, packed_scene);
}

bool Gs1GodotDirectorControl::should_async_load_transition(ScreenKind kind) const noexcept
{
    return kind == SCREEN_KIND_REGIONAL_MAP &&
        active_screen_kind_ == SCREEN_KIND_MAIN_MENU &&
        !loading_scene_path_.is_empty();
}

bool Gs1GodotDirectorControl::handles_engine_message(Gs1EngineMessageType type) const noexcept
{
    return type == GS1_ENGINE_MESSAGE_SET_APP_STATE;
}

void Gs1GodotDirectorControl::handle_engine_message(const Gs1EngineMessage& message)
{
    if (message.type != GS1_ENGINE_MESSAGE_SET_APP_STATE)
    {
        return;
    }
    last_app_state_ = static_cast<int>(message.payload_as<Gs1EngineMessageSetAppStateData>().app_state);
    ensure_active_scene();
}

void Gs1GodotDirectorControl::handle_runtime_message_reset()
{
    last_app_state_ = APP_STATE_BOOT;
}

Gs1GodotDirectorControl::ScreenKind Gs1GodotDirectorControl::desired_screen_kind(int app_state) const noexcept
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

String Gs1GodotDirectorControl::scene_path_for(ScreenKind kind) const
{
    switch (kind)
    {
    case SCREEN_KIND_LOADING:
        return loading_scene_path_;
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

void Gs1GodotDirectorControl::switch_to_scene(ScreenKind kind, const Ref<PackedScene>& prefetched_scene)
{
    if (scene_host_ == nullptr)
    {
        return;
    }

    if (Node* active_scene = Object::cast_to<Node>(ObjectDB::get_instance(active_scene_id_)))
    {
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

    Node* instance = prefetched_scene.is_null()
        ? instantiate_scene(scene_path)
        : instantiate_scene(prefetched_scene);
    if (instance == nullptr)
    {
        UtilityFunctions::push_warning(vformat("GS1 Godot director could not load scene: %s", scene_path));
        return;
    }

    scene_host_->add_child(instance);
    if (auto* control = Object::cast_to<Control>(instance))
    {
        control->set_anchors_preset(Control::PRESET_FULL_RECT);
    }

    active_scene_id_ = instance->get_instance_id();
    active_screen_kind_ = kind;
}

Node* Gs1GodotDirectorControl::instantiate_scene(const String& scene_path) const
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

    return instantiate_scene(packed_scene);
}

Node* Gs1GodotDirectorControl::instantiate_scene(const Ref<PackedScene>& packed_scene) const
{
    if (packed_scene.is_null())
    {
        return nullptr;
    }

    return packed_scene->instantiate();
}

void Gs1GodotDirectorControl::set_scene_host_path(const NodePath& path)
{
    scene_host_path_ = path;
    scene_host_ = nullptr;
}

NodePath Gs1GodotDirectorControl::get_scene_host_path() const
{
    return scene_host_path_;
}

void Gs1GodotDirectorControl::set_loading_scene_path(const String& path)
{
    loading_scene_path_ = path;
}

String Gs1GodotDirectorControl::get_loading_scene_path() const
{
    return loading_scene_path_;
}

void Gs1GodotDirectorControl::set_main_menu_scene_path(const String& path)
{
    main_menu_scene_path_ = path;
}

String Gs1GodotDirectorControl::get_main_menu_scene_path() const
{
    return main_menu_scene_path_;
}

void Gs1GodotDirectorControl::set_regional_map_scene_path(const String& path)
{
    regional_map_scene_path_ = path;
}

String Gs1GodotDirectorControl::get_regional_map_scene_path() const
{
    return regional_map_scene_path_;
}

void Gs1GodotDirectorControl::set_site_session_scene_path(const String& path)
{
    site_session_scene_path_ = path;
}

String Gs1GodotDirectorControl::get_site_session_scene_path() const
{
    return site_session_scene_path_;
}
