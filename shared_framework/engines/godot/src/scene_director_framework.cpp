#include "shared_framework/godot/scene_director_framework.h"

#include "shared_framework/godot/scene_director_policy.h"

#include <godot_cpp/classes/canvas_item.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/packed_scene.hpp>
#include <godot_cpp/classes/progress_bar.hpp>
#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/core/object.hpp>
#include <godot_cpp/variant/array.hpp>

#include <algorithm>

using namespace godot;

namespace
{
void set_node_visible(Node* node, const bool visible)
{
    if (node == nullptr)
    {
        return;
    }

    if (auto* canvas_item = Object::cast_to<CanvasItem>(node))
    {
        canvas_item->set_visible(visible);
        return;
    }

    if (auto* node3d = Object::cast_to<Node3D>(node))
    {
        node3d->set_visible(visible);
    }
}
}  // namespace

namespace shared_framework::godot {

void SceneDirectorFramework::set_scene_host_path(const NodePath& path)
{
    scene_host_path_ = path;
    scene_host_ = nullptr;
}

void SceneDirectorFramework::set_loading_scene_path(const String& path)
{
    loading_scene_path_ = path;
}

void SceneDirectorFramework::set_loading_progress_bar_path(const NodePath& path)
{
    loading_progress_bar_path_ = path;
    loading_progress_bar_ = nullptr;
}

void SceneDirectorFramework::set_loading_progress_label_path(const NodePath& path)
{
    loading_progress_label_path_ = path;
    loading_progress_label_ = nullptr;
}

void SceneDirectorFramework::set_loading_reveal_delay_frames(const int frames) noexcept
{
    loading_reveal_delay_frames_ = std::max(0, frames);
}

void SceneDirectorFramework::refresh(
    Control& owner,
    const SceneDirectorRoute& desired_route,
    const SceneInstantiator& instantiator)
{
    cache_scene_host(owner);
    poll_async_scene_switch(owner, instantiator);

    if (scene_host_ == nullptr || pending_async_target_kind_ >= 0)
    {
        return;
    }

    Node* active_scene = Object::cast_to<Node>(ObjectDB::get_instance(active_scene_id_));
    if (!scene_director_requires_scene_switch(
            active_scene != nullptr,
            active_scene_kind_,
            desired_route.scene_kind))
    {
        return;
    }

    if (desired_route.async_load &&
        !loading_scene_path_.is_empty() &&
        loading_scene_kind_ >= 0)
    {
        begin_async_scene_switch(owner, desired_route, instantiator);
        return;
    }

    switch_to_scene(owner, desired_route.scene_kind, desired_route.scene_path, instantiator);
}

void SceneDirectorFramework::reset(Control& owner) noexcept
{
    if (Node* staged_scene = Object::cast_to<Node>(ObjectDB::get_instance(staged_async_scene_id_)))
    {
        staged_scene->queue_free();
    }

    if (Node* active_scene = Object::cast_to<Node>(ObjectDB::get_instance(active_scene_id_)))
    {
        if (scene_host_ == nullptr)
        {
            cache_scene_host(owner);
        }
        if (scene_host_ != nullptr && active_scene->get_parent() == scene_host_)
        {
            scene_host_->remove_child(active_scene);
        }
        active_scene->queue_free();
    }

    active_scene_id_ = ObjectID();
    active_scene_kind_ = -1;
    loading_progress_bar_ = nullptr;
    loading_progress_label_ = nullptr;
    clear_async_scene_switch_state();
}

void SceneDirectorFramework::cache_scene_host(Control& owner)
{
    if (scene_host_ == nullptr)
    {
        scene_host_ = Object::cast_to<Control>(owner.get_node_or_null(scene_host_path_));
    }
}

void SceneDirectorFramework::begin_async_scene_switch(
    Control& owner,
    const SceneDirectorRoute& desired_route,
    const SceneInstantiator& instantiator)
{
    ResourceLoader* resource_loader = ResourceLoader::get_singleton();
    if (resource_loader == nullptr || desired_route.scene_path.is_empty())
    {
        switch_to_scene(owner, desired_route.scene_kind, desired_route.scene_path, instantiator);
        return;
    }

    const Error status = resource_loader->load_threaded_request(
        desired_route.scene_path,
        "PackedScene",
        true,
        ResourceLoader::CACHE_MODE_REUSE);
    if (status != OK && status != ERR_BUSY)
    {
        switch_to_scene(owner, desired_route.scene_kind, desired_route.scene_path, instantiator);
        return;
    }

    pending_async_target_kind_ = desired_route.scene_kind;
    pending_async_scene_path_ = desired_route.scene_path;
    pending_async_resource_paths_ = desired_route.async_preload_paths;
    pending_loading_reveal_delay_frames_ = 0;

    for (auto it = pending_async_resource_paths_.begin(); it != pending_async_resource_paths_.end();)
    {
        const Error preload_status = resource_loader->load_threaded_request(
            *it,
            String(),
            false,
            ResourceLoader::CACHE_MODE_REUSE);
        if (preload_status == OK || preload_status == ERR_BUSY)
        {
            ++it;
            continue;
        }

        it = pending_async_resource_paths_.erase(it);
    }

    switch_to_scene(owner, loading_scene_kind_, loading_scene_path_, instantiator);
}

void SceneDirectorFramework::poll_async_scene_switch(
    Control& owner,
    const SceneInstantiator& instantiator)
{
    if (pending_async_target_kind_ < 0 || pending_async_scene_path_.is_empty())
    {
        return;
    }

    ResourceLoader* resource_loader = ResourceLoader::get_singleton();
    if (resource_loader == nullptr)
    {
        const int target_kind = pending_async_target_kind_;
        const String target_path = pending_async_scene_path_;
        clear_async_scene_switch_state();
        switch_to_scene(owner, target_kind, target_path, instantiator);
        return;
    }

    refresh_loading_scene_progress(*resource_loader);
    if (!async_resource_preloads_complete(*resource_loader))
    {
        return;
    }

    const ResourceLoader::ThreadLoadStatus status =
        resource_loader->load_threaded_get_status(pending_async_scene_path_);
    if (status == ResourceLoader::THREAD_LOAD_IN_PROGRESS)
    {
        return;
    }

    if (status != ResourceLoader::THREAD_LOAD_LOADED)
    {
        const int target_kind = pending_async_target_kind_;
        const String target_path = pending_async_scene_path_;
        clear_async_scene_switch_state();
        switch_to_scene(owner, target_kind, target_path, instantiator);
        return;
    }

    if (!stage_async_scene_switch(
            instantiator,
            pending_async_target_kind_,
            pending_async_scene_path_))
    {
        const int target_kind = pending_async_target_kind_;
        const String target_path = pending_async_scene_path_;
        clear_async_scene_switch_state();
        switch_to_scene(owner, target_kind, target_path, instantiator);
        return;
    }

    if (pending_loading_reveal_delay_frames_ > 0)
    {
        --pending_loading_reveal_delay_frames_;
        return;
    }

    reveal_staged_async_scene();
}

bool SceneDirectorFramework::stage_async_scene_switch(
    const SceneInstantiator& instantiator,
    const int scene_kind,
    const String& scene_path)
{
    if (scene_host_ == nullptr || active_scene_kind_ != loading_scene_kind_)
    {
        return false;
    }

    if (Object::cast_to<Node>(ObjectDB::get_instance(staged_async_scene_id_)) != nullptr)
    {
        return true;
    }

    Node* instance = instantiate_scene(instantiator, scene_path);
    if (instance == nullptr)
    {
        return false;
    }

    instance->set_process_mode(Node::PROCESS_MODE_INHERIT);
    scene_host_->add_child(instance);
    if (auto* control = Object::cast_to<Control>(instance))
    {
        control->set_anchors_preset(Control::PRESET_FULL_RECT);
    }

    set_node_visible(instance, false);
    scene_host_->move_child(instance, 0);
    staged_async_scene_id_ = instance->get_instance_id();
    pending_async_target_kind_ = scene_kind;
    pending_async_scene_path_ = scene_path;
    pending_loading_reveal_delay_frames_ = std::max(0, loading_reveal_delay_frames_);
    return true;
}

void SceneDirectorFramework::reveal_staged_async_scene() noexcept
{
    Node* staged_scene = Object::cast_to<Node>(ObjectDB::get_instance(staged_async_scene_id_));
    if (scene_host_ == nullptr || staged_scene == nullptr)
    {
        clear_async_scene_switch_state();
        return;
    }

    if (Node* loading_scene = Object::cast_to<Node>(ObjectDB::get_instance(active_scene_id_)))
    {
        if (loading_scene->get_parent() == scene_host_)
        {
            scene_host_->remove_child(loading_scene);
        }
        loading_scene->queue_free();
    }

    set_node_visible(staged_scene, true);
    active_scene_id_ = staged_scene->get_instance_id();
    active_scene_kind_ = pending_async_target_kind_;
    loading_progress_bar_ = nullptr;
    loading_progress_label_ = nullptr;
    clear_async_scene_switch_state();
}

void SceneDirectorFramework::cache_loading_scene_nodes()
{
    if (active_scene_kind_ != loading_scene_kind_)
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
        loading_progress_bar_ = Object::cast_to<ProgressBar>(
            active_scene->get_node_or_null(loading_progress_bar_path_));
    }
    if (loading_progress_label_ == nullptr)
    {
        loading_progress_label_ = Object::cast_to<Label>(
            active_scene->get_node_or_null(loading_progress_label_path_));
    }
}

void SceneDirectorFramework::refresh_loading_scene_progress(ResourceLoader& resource_loader)
{
    if (pending_async_target_kind_ < 0 || active_scene_kind_ != loading_scene_kind_)
    {
        return;
    }

    cache_loading_scene_nodes();
    if (loading_progress_bar_ == nullptr && loading_progress_label_ == nullptr)
    {
        return;
    }

    double completed_work = threaded_load_progress(resource_loader, pending_async_scene_path_);
    double total_work = 1.0;
    for (const String& path : pending_async_resource_paths_)
    {
        completed_work += threaded_load_progress(resource_loader, path);
        total_work += 1.0;
    }

    const double clamped_progress = std::clamp(
        total_work > 0.0 ? (completed_work / total_work) : 0.0,
        0.0,
        1.0);
    if (loading_progress_bar_ != nullptr)
    {
        loading_progress_bar_->set_value(clamped_progress * 100.0);
    }
    if (loading_progress_label_ != nullptr)
    {
        loading_progress_label_->set_text(vformat("%.0f%% complete", clamped_progress * 100.0));
    }
}

double SceneDirectorFramework::threaded_load_progress(
    ResourceLoader& resource_loader,
    const String& path,
    ResourceLoader::ThreadLoadStatus* out_status) const
{
    Array progress {};
    const ResourceLoader::ThreadLoadStatus status =
        resource_loader.load_threaded_get_status(path, progress);
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

bool SceneDirectorFramework::async_resource_preloads_complete(ResourceLoader& resource_loader)
{
    for (const String& path : pending_async_resource_paths_)
    {
        if (resource_loader.load_threaded_get_status(path) ==
            ResourceLoader::THREAD_LOAD_IN_PROGRESS)
        {
            return false;
        }
    }

    for (const String& path : pending_async_resource_paths_)
    {
        if (resource_loader.load_threaded_get_status(path) ==
            ResourceLoader::THREAD_LOAD_LOADED)
        {
            resource_loader.load_threaded_get(path);
        }
    }

    pending_async_resource_paths_.clear();
    return true;
}

Node* SceneDirectorFramework::instantiate_scene(
    const SceneInstantiator& instantiator,
    const String& scene_path) const
{
    if (instantiator)
    {
        if (Node* instance = instantiator(scene_path))
        {
            return instance;
        }
    }

    ResourceLoader* resource_loader = ResourceLoader::get_singleton();
    if (resource_loader == nullptr)
    {
        return nullptr;
    }

    Ref<Resource> resource = resource_loader->load(scene_path);
    Ref<PackedScene> packed_scene = resource;
    return packed_scene.is_null() ? nullptr : packed_scene->instantiate();
}

void SceneDirectorFramework::switch_to_scene(
    Control& owner,
    const int scene_kind,
    const String& scene_path,
    const SceneInstantiator& instantiator)
{
    cache_scene_host(owner);
    if (scene_host_ == nullptr)
    {
        return;
    }

    if (Node* staged_scene = Object::cast_to<Node>(ObjectDB::get_instance(staged_async_scene_id_)))
    {
        if (staged_scene->get_parent() == scene_host_)
        {
            scene_host_->remove_child(staged_scene);
        }
        staged_scene->queue_free();
    }

    if (Node* active_scene = Object::cast_to<Node>(ObjectDB::get_instance(active_scene_id_)))
    {
        if (active_scene->get_parent() == scene_host_)
        {
            scene_host_->remove_child(active_scene);
        }
        active_scene->queue_free();
    }

    active_scene_id_ = ObjectID();
    active_scene_kind_ = -1;
    loading_progress_bar_ = nullptr;
    loading_progress_label_ = nullptr;
    staged_async_scene_id_ = ObjectID();
    pending_loading_reveal_delay_frames_ = 0;

    if (scene_path.is_empty())
    {
        return;
    }

    Node* instance = instantiate_scene(instantiator, scene_path);
    if (instance == nullptr)
    {
        return;
    }

    scene_host_->add_child(instance);
    if (auto* control = Object::cast_to<Control>(instance))
    {
        control->set_anchors_preset(Control::PRESET_FULL_RECT);
    }

    active_scene_id_ = instance->get_instance_id();
    active_scene_kind_ = scene_kind;
}

void SceneDirectorFramework::clear_async_scene_switch_state() noexcept
{
    pending_async_target_kind_ = -1;
    pending_async_scene_path_ = String();
    pending_async_resource_paths_.clear();
    staged_async_scene_id_ = ObjectID();
    pending_loading_reveal_delay_frames_ = 0;
}

}  // namespace shared_framework::godot
