#pragma once

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/progress_bar.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/core/object_id.hpp>
#include <godot_cpp/variant/node_path.hpp>
#include <godot_cpp/variant/string.hpp>

#include <functional>
#include <vector>

namespace shared_framework::godot {
struct SceneDirectorRoute final
{
    int scene_kind {-1};
    ::godot::String scene_path {};
    bool async_load {false};
    std::vector<::godot::String> async_preload_paths {};
};

class SceneDirectorFramework final
{
public:
    using SceneInstantiator = std::function<::godot::Node*(const ::godot::String& scene_path)>;

    void set_scene_host_path(const ::godot::NodePath& path);
    [[nodiscard]] const ::godot::NodePath& scene_host_path() const noexcept { return scene_host_path_; }

    void set_loading_scene_kind(int scene_kind) noexcept { loading_scene_kind_ = scene_kind; }
    [[nodiscard]] int loading_scene_kind() const noexcept { return loading_scene_kind_; }

    void set_loading_scene_path(const ::godot::String& path);
    [[nodiscard]] const ::godot::String& loading_scene_path() const noexcept { return loading_scene_path_; }

    void set_loading_progress_bar_path(const ::godot::NodePath& path);
    [[nodiscard]] const ::godot::NodePath& loading_progress_bar_path() const noexcept { return loading_progress_bar_path_; }

    void set_loading_progress_label_path(const ::godot::NodePath& path);
    [[nodiscard]] const ::godot::NodePath& loading_progress_label_path() const noexcept { return loading_progress_label_path_; }

    void set_loading_reveal_delay_frames(int frames) noexcept;
    [[nodiscard]] int loading_reveal_delay_frames() const noexcept { return loading_reveal_delay_frames_; }

    void refresh(
        ::godot::Control& owner,
        const SceneDirectorRoute& desired_route,
        const SceneInstantiator& instantiator = SceneInstantiator {});
    void reset(::godot::Control& owner) noexcept;

    [[nodiscard]] int active_scene_kind() const noexcept { return active_scene_kind_; }
    [[nodiscard]] bool has_pending_async_switch() const noexcept { return pending_async_target_kind_ >= 0; }

private:
    void cache_scene_host(::godot::Control& owner);
    void begin_async_scene_switch(
        ::godot::Control& owner,
        const SceneDirectorRoute& desired_route,
        const SceneInstantiator& instantiator);
    void poll_async_scene_switch(::godot::Control& owner, const SceneInstantiator& instantiator);
    [[nodiscard]] bool stage_async_scene_switch(
        const SceneInstantiator& instantiator,
        int scene_kind,
        const ::godot::String& scene_path);
    void reveal_staged_async_scene() noexcept;
    void cache_loading_scene_nodes();
    void refresh_loading_scene_progress(::godot::ResourceLoader& resource_loader);
    [[nodiscard]] double threaded_load_progress(
        ::godot::ResourceLoader& resource_loader,
        const ::godot::String& path,
        ::godot::ResourceLoader::ThreadLoadStatus* out_status = nullptr) const;
    [[nodiscard]] bool async_resource_preloads_complete(::godot::ResourceLoader& resource_loader);
    [[nodiscard]] ::godot::Node* instantiate_scene(
        const SceneInstantiator& instantiator,
        const ::godot::String& scene_path) const;
    void switch_to_scene(
        ::godot::Control& owner,
        int scene_kind,
        const ::godot::String& scene_path,
        const SceneInstantiator& instantiator);
    void clear_async_scene_switch_state() noexcept;

private:
    ::godot::NodePath scene_host_path_ {"SceneHost"};
    ::godot::String loading_scene_path_ {};
    ::godot::NodePath loading_progress_bar_path_ {"ProgressBar"};
    ::godot::NodePath loading_progress_label_path_ {"ProgressLabel"};
    int loading_scene_kind_ {-1};
    int loading_reveal_delay_frames_ {0};

    ::godot::Control* scene_host_ {nullptr};
    ::godot::ProgressBar* loading_progress_bar_ {nullptr};
    ::godot::Label* loading_progress_label_ {nullptr};
    ::godot::ObjectID active_scene_id_ {};
    int active_scene_kind_ {-1};
    int pending_async_target_kind_ {-1};
    ::godot::String pending_async_scene_path_ {};
    std::vector<::godot::String> pending_async_resource_paths_ {};
    ::godot::ObjectID staged_async_scene_id_ {};
    int pending_loading_reveal_delay_frames_ {0};
};
}  // namespace shared_framework::godot
