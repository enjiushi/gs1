#include "gs1_godot_director_control.h"

#include "godot_progression_resources.h"

#include "content/defs/technology_defs.h"

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/packed_scene.hpp>
#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/object.hpp>
#include <godot_cpp/core/property_info.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <algorithm>
#include <string>
#include <unordered_set>

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

std::uint8_t unlockable_content_kind_from_def(const gs1::ReputationUnlockDef& unlock_def) noexcept
{
    switch (unlock_def.unlock_kind)
    {
    case gs1::ReputationUnlockKind::Plant:
        return 0U;
    case gs1::ReputationUnlockKind::Item:
        return 1U;
    case gs1::ReputationUnlockKind::StructureRecipe:
        return 2U;
    case gs1::ReputationUnlockKind::Recipe:
        return 3U;
    default:
        return 1U;
    }
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
    adapter_service_.begin_frame(delta);
    cache_nodes();
    poll_async_scene_switch();
    ensure_active_scene();
    adapter_service_.finish_frame();
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
    if (pending_async_target_kind_ != GS1_GODOT_SCREEN_KIND_NONE)
    {
        return;
    }

    Node* active_scene = Object::cast_to<Node>(ObjectDB::get_instance(active_scene_id_));
    if (!gs1_godot_director_requires_scene_switch(active_scene != nullptr, active_screen_kind_, desired_kind))
    {
        last_app_state_ = app_state;
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
    const bool buffer_engine_messages =
        kind == GS1_GODOT_SCREEN_KIND_REGIONAL_MAP &&
        active_screen_kind_ == GS1_GODOT_SCREEN_KIND_MAIN_MENU;
    const bool submit_site_scene_ready =
        kind == GS1_GODOT_SCREEN_KIND_SITE_SESSION &&
        active_screen_kind_ == GS1_GODOT_SCREEN_KIND_REGIONAL_MAP;
    if (resource_loader == nullptr || scene_path.is_empty())
    {
        switch_to_scene(kind);
        if (submit_site_scene_ready && !adapter_service_.submit_site_scene_ready())
        {
            UtilityFunctions::push_warning("GS1 Godot director could not acknowledge site scene readiness.");
        }
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
        if (submit_site_scene_ready && !adapter_service_.submit_site_scene_ready())
        {
            UtilityFunctions::push_warning("GS1 Godot director could not acknowledge site scene readiness.");
        }
        return;
    }

    pending_async_target_kind_ = kind;
    pending_async_scene_path_ = scene_path;
    begin_async_resource_preloads(kind);
    pending_async_transition_buffers_engine_messages_ = buffer_engine_messages;
    pending_async_transition_submits_site_scene_ready_ = submit_site_scene_ready;
    if (pending_async_transition_buffers_engine_messages_)
    {
        adapter_service_.begin_engine_message_buffering();
    }
    switch_to_scene(GS1_GODOT_SCREEN_KIND_LOADING);
}

void Gs1GodotDirectorControl::poll_async_scene_switch()
{
    if (pending_async_target_kind_ == GS1_GODOT_SCREEN_KIND_NONE || pending_async_scene_path_.is_empty())
    {
        return;
    }

    ResourceLoader* resource_loader = ResourceLoader::get_singleton();
    if (resource_loader == nullptr)
    {
        const ScreenKind target_kind = pending_async_target_kind_;
        const bool buffer_engine_messages = pending_async_transition_buffers_engine_messages_;
        const bool submit_site_scene_ready = pending_async_transition_submits_site_scene_ready_;
        clear_async_scene_switch_state();
        switch_to_scene(target_kind);
        if (buffer_engine_messages)
        {
            adapter_service_.flush_buffered_engine_messages();
        }
        if (submit_site_scene_ready && !adapter_service_.submit_site_scene_ready())
        {
            UtilityFunctions::push_warning("GS1 Godot director could not acknowledge site scene readiness.");
        }
        return;
    }

    refresh_loading_scene_progress(*resource_loader);

    if (!async_resource_preloads_complete(*resource_loader))
    {
        return;
    }

    const ResourceLoader::ThreadLoadStatus status = resource_loader->load_threaded_get_status(pending_async_scene_path_);
    if (status == ResourceLoader::THREAD_LOAD_IN_PROGRESS)
    {
        return;
    }

    const ScreenKind target_kind = pending_async_target_kind_;
    const String target_path = pending_async_scene_path_;
    const bool buffer_engine_messages = pending_async_transition_buffers_engine_messages_;
    const bool submit_site_scene_ready = pending_async_transition_submits_site_scene_ready_;
    clear_async_scene_switch_state();

    if (status != ResourceLoader::THREAD_LOAD_LOADED)
    {
        switch_to_scene(target_kind);
        if (buffer_engine_messages)
        {
            adapter_service_.flush_buffered_engine_messages();
        }
        if (submit_site_scene_ready && !adapter_service_.submit_site_scene_ready())
        {
            UtilityFunctions::push_warning("GS1 Godot director could not acknowledge site scene readiness.");
        }
        return;
    }

    Ref<Resource> resource = resource_loader->load_threaded_get(target_path);
    Ref<PackedScene> packed_scene = resource;
    if (packed_scene.is_null())
    {
        switch_to_scene(target_kind);
        if (buffer_engine_messages)
        {
            adapter_service_.flush_buffered_engine_messages();
        }
        if (submit_site_scene_ready && !adapter_service_.submit_site_scene_ready())
        {
            UtilityFunctions::push_warning("GS1 Godot director could not acknowledge site scene readiness.");
        }
        return;
    }

    switch_to_scene(target_kind, packed_scene);
    if (buffer_engine_messages)
    {
        adapter_service_.flush_buffered_engine_messages();
    }
    if (submit_site_scene_ready && !adapter_service_.submit_site_scene_ready())
    {
        UtilityFunctions::push_warning("GS1 Godot director could not acknowledge site scene readiness.");
    }
}

void Gs1GodotDirectorControl::cache_loading_scene_nodes()
{
    if (active_screen_kind_ != GS1_GODOT_SCREEN_KIND_LOADING)
    {
        loading_progress_bar_ = nullptr;
        loading_progress_label_ = nullptr;
        return;
    }

    Node* active_scene = Object::cast_to<Node>(ObjectDB::get_instance(active_scene_id_));
    if (active_scene == nullptr)
    {
        return;
    }

    if (loading_progress_bar_ == nullptr)
    {
        loading_progress_bar_ = Object::cast_to<ProgressBar>(active_scene->find_child("ProgressBar", true, false));
    }
    if (loading_progress_label_ == nullptr)
    {
        loading_progress_label_ = Object::cast_to<Label>(active_scene->find_child("ProgressLabel", true, false));
    }
}

void Gs1GodotDirectorControl::refresh_loading_scene_progress(ResourceLoader& resource_loader)
{
    if (pending_async_target_kind_ == GS1_GODOT_SCREEN_KIND_NONE || active_screen_kind_ != GS1_GODOT_SCREEN_KIND_LOADING)
    {
        return;
    }

    cache_loading_scene_nodes();
    if (loading_progress_bar_ == nullptr && loading_progress_label_ == nullptr)
    {
        return;
    }

    // Combine the threaded target-scene load with each queued preload request so the loading screen
    // reflects the actual Godot work this transition is waiting on rather than a fake timer.
    double completed_work = threaded_load_progress(resource_loader, pending_async_scene_path_);
    double total_work = 1.0;
    for (const String& path : pending_async_resource_paths_)
    {
        completed_work += threaded_load_progress(resource_loader, path);
        total_work += 1.0;
    }

    const double clamped_progress = std::clamp(total_work > 0.0 ? (completed_work / total_work) : 0.0, 0.0, 1.0);
    if (loading_progress_bar_ != nullptr)
    {
        loading_progress_bar_->set_value(clamped_progress * 100.0);
    }
    if (loading_progress_label_ != nullptr)
    {
        loading_progress_label_->set_text(vformat("%.0f%% complete", clamped_progress * 100.0));
    }
}

void Gs1GodotDirectorControl::begin_async_resource_preloads(ScreenKind kind)
{
    pending_async_resource_paths_ = build_async_preload_paths(kind);
    if (pending_async_resource_paths_.empty())
    {
        return;
    }

    ResourceLoader* resource_loader = ResourceLoader::get_singleton();
    if (resource_loader == nullptr)
    {
        pending_async_resource_paths_.clear();
        return;
    }

    auto it = pending_async_resource_paths_.begin();
    while (it != pending_async_resource_paths_.end())
    {
        const Error status = resource_loader->load_threaded_request(
            *it,
            "Texture2D",
            false,
            ResourceLoader::CACHE_MODE_REUSE);
        if (status == OK || status == ERR_BUSY)
        {
            ++it;
            continue;
        }

        UtilityFunctions::push_warning(vformat(
            "GS1 Godot director could not queue async preload for %s",
            *it));
        it = pending_async_resource_paths_.erase(it);
    }
}

std::vector<String> Gs1GodotDirectorControl::build_async_preload_paths(ScreenKind kind) const
{
    std::vector<String> paths {};
    if (kind != GS1_GODOT_SCREEN_KIND_REGIONAL_MAP)
    {
        return paths;
    }

    std::unordered_set<std::string> dedupe {};
    const auto add_path = [&](const String& path) {
        if (path.is_empty())
        {
            return;
        }

        const std::string key = path.utf8().get_data();
        if (dedupe.insert(key).second)
        {
            paths.push_back(path);
        }
    };

    const auto& resource_database = GodotProgressionResourceDatabase::instance();
    for (const auto& node_def : gs1::all_technology_node_defs())
    {
        add_path(resource_database.technology_icon_path(node_def.tech_node_id.value));
    }

    for (const auto& unlock_def : gs1::all_reputation_unlock_defs())
    {
        add_path(resource_database.unlockable_icon_path(
            unlockable_content_kind_from_def(unlock_def),
            unlock_def.content_id));
    }

    return paths;
}

double Gs1GodotDirectorControl::threaded_load_progress(
    ResourceLoader& resource_loader,
    const String& path,
    ResourceLoader::ThreadLoadStatus* out_status) const
{
    Array progress {};
    const ResourceLoader::ThreadLoadStatus status = resource_loader.load_threaded_get_status(path, progress);
    if (out_status != nullptr)
    {
        *out_status = status;
    }

    switch (status)
    {
    case ResourceLoader::THREAD_LOAD_LOADED:
    case ResourceLoader::THREAD_LOAD_FAILED:
    case ResourceLoader::THREAD_LOAD_INVALID_RESOURCE:
        return 1.0;
    case ResourceLoader::THREAD_LOAD_IN_PROGRESS:
        if (progress.size() > 0)
        {
            return std::clamp(static_cast<double>(progress[0]), 0.0, 1.0);
        }
        return 0.0;
    default:
        return 0.0;
    }
}

bool Gs1GodotDirectorControl::async_resource_preloads_complete(ResourceLoader& resource_loader)
{
    for (const String& path : pending_async_resource_paths_)
    {
        const ResourceLoader::ThreadLoadStatus status = resource_loader.load_threaded_get_status(path);
        if (status == ResourceLoader::THREAD_LOAD_IN_PROGRESS)
        {
            return false;
        }
    }

    for (const String& path : pending_async_resource_paths_)
    {
        const ResourceLoader::ThreadLoadStatus status = resource_loader.load_threaded_get_status(path);
        if (status == ResourceLoader::THREAD_LOAD_LOADED)
        {
            resource_loader.load_threaded_get(path);
            continue;
        }

        if (status != ResourceLoader::THREAD_LOAD_FAILED &&
            status != ResourceLoader::THREAD_LOAD_INVALID_RESOURCE)
        {
            UtilityFunctions::push_warning(vformat(
                "GS1 Godot director saw unexpected async preload status %d for %s",
                static_cast<int>(status),
                path));
        }
    }

    pending_async_resource_paths_.clear();
    return true;
}

void Gs1GodotDirectorControl::clear_async_scene_switch_state() noexcept
{
    pending_async_target_kind_ = GS1_GODOT_SCREEN_KIND_NONE;
    pending_async_scene_path_ = String();
    pending_async_resource_paths_.clear();
    pending_async_transition_buffers_engine_messages_ = false;
    pending_async_transition_submits_site_scene_ready_ = false;
}

bool Gs1GodotDirectorControl::should_async_load_transition(ScreenKind kind) const noexcept
{
    return !loading_scene_path_.is_empty() &&
        ((kind == GS1_GODOT_SCREEN_KIND_REGIONAL_MAP &&
                active_screen_kind_ == GS1_GODOT_SCREEN_KIND_MAIN_MENU) ||
            (kind == GS1_GODOT_SCREEN_KIND_SITE_SESSION &&
                active_screen_kind_ == GS1_GODOT_SCREEN_KIND_REGIONAL_MAP));
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
    clear_async_scene_switch_state();
    last_app_state_ = APP_STATE_BOOT;
}

Gs1GodotDirectorControl::ScreenKind Gs1GodotDirectorControl::desired_screen_kind(int app_state) const noexcept
{
    switch (app_state)
    {
    case APP_STATE_SITE_ACTIVE:
    case APP_STATE_SITE_PAUSED:
    case APP_STATE_SITE_RESULT:
    case APP_STATE_SITE_LOADING:
        return GS1_GODOT_SCREEN_KIND_SITE_SESSION;
    case APP_STATE_CAMPAIGN_SETUP:
    case APP_STATE_REGIONAL_MAP:
        return GS1_GODOT_SCREEN_KIND_REGIONAL_MAP;
    case APP_STATE_BOOT:
    case APP_STATE_MAIN_MENU:
    case APP_STATE_CAMPAIGN_END:
    default:
        return GS1_GODOT_SCREEN_KIND_MAIN_MENU;
    }
}

String Gs1GodotDirectorControl::scene_path_for(ScreenKind kind) const
{
    switch (kind)
    {
    case GS1_GODOT_SCREEN_KIND_LOADING:
        return loading_scene_path_;
    case GS1_GODOT_SCREEN_KIND_MAIN_MENU:
        return main_menu_scene_path_;
    case GS1_GODOT_SCREEN_KIND_REGIONAL_MAP:
        return regional_map_scene_path_;
    case GS1_GODOT_SCREEN_KIND_SITE_SESSION:
        return site_session_scene_path_;
    case GS1_GODOT_SCREEN_KIND_NONE:
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
    active_screen_kind_ = GS1_GODOT_SCREEN_KIND_NONE;
    loading_progress_bar_ = nullptr;
    loading_progress_label_ = nullptr;

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
