#pragma once

#include "gs1_godot_adapter_service.h"
#include "gs1_godot_main_menu_ui_controller.h"
#include "gs1_godot_regional_map_scene_controller.h"
#include "gs1_godot_regional_map_ui_controller.h"
#include "gs1_godot_site_scene_controller.h"
#include "gs1_godot_site_session_ui_controller.h"

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/packed_scene.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/core/binder_common.hpp>
#include <godot_cpp/core/object_id.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/variant/node_path.hpp>
#include <godot_cpp/variant/string.hpp>

#include <vector>

class Gs1GodotDirectorControl final : public godot::Control, public IGs1GodotEngineMessageSubscriber
{
    GDCLASS(Gs1GodotDirectorControl, godot::Control)

public:
    Gs1GodotDirectorControl() = default;
    ~Gs1GodotDirectorControl() override = default;

    void _ready() override;
    void _process(double delta) override;

    void set_scene_host_path(const godot::NodePath& path);
    [[nodiscard]] godot::NodePath get_scene_host_path() const;

    void set_main_menu_scene_path(const godot::String& path);
    [[nodiscard]] godot::String get_main_menu_scene_path() const;

    void set_regional_map_scene_path(const godot::String& path);
    [[nodiscard]] godot::String get_regional_map_scene_path() const;

    void set_site_session_scene_path(const godot::String& path);
    [[nodiscard]] godot::String get_site_session_scene_path() const;

    void set_loading_scene_path(const godot::String& path);
    [[nodiscard]] godot::String get_loading_scene_path() const;

    [[nodiscard]] Gs1GodotAdapterService& adapter_service() noexcept { return adapter_service_; }
    [[nodiscard]] const Gs1GodotAdapterService& adapter_service() const noexcept { return adapter_service_; }

    [[nodiscard]] bool handles_engine_message(Gs1EngineMessageType type) const noexcept override;
    void handle_engine_message(const Gs1EngineMessage& message) override;
    void handle_runtime_message_reset() override;

protected:
    static void _bind_methods();

private:
    enum ScreenKind : std::uint8_t
    {
        SCREEN_KIND_NONE = 0,
        SCREEN_KIND_LOADING = 1,
        SCREEN_KIND_MAIN_MENU = 2,
        SCREEN_KIND_REGIONAL_MAP = 3,
        SCREEN_KIND_SITE_SESSION = 4
    };

    void cache_nodes();
    void ensure_active_scene();
    void begin_async_scene_switch(ScreenKind kind);
    void poll_async_scene_switch();
    void begin_async_resource_preloads(ScreenKind kind);
    [[nodiscard]] std::vector<godot::String> build_async_preload_paths(ScreenKind kind) const;
    [[nodiscard]] bool async_resource_preloads_complete(godot::ResourceLoader& resource_loader);
    void clear_async_scene_switch_state() noexcept;
    [[nodiscard]] bool should_async_load_transition(ScreenKind kind) const noexcept;
    [[nodiscard]] ScreenKind desired_screen_kind(int app_state) const noexcept;
    [[nodiscard]] godot::String scene_path_for(ScreenKind kind) const;
    void switch_to_scene(
        ScreenKind kind,
        const godot::Ref<godot::PackedScene>& prefetched_scene = godot::Ref<godot::PackedScene> {});
    [[nodiscard]] godot::Node* instantiate_scene(const godot::String& scene_path) const;
    [[nodiscard]] godot::Node* instantiate_scene(const godot::Ref<godot::PackedScene>& packed_scene) const;

private:
    static constexpr int APP_STATE_BOOT = 0;
    static constexpr int APP_STATE_MAIN_MENU = 1;
    static constexpr int APP_STATE_CAMPAIGN_SETUP = 2;
    static constexpr int APP_STATE_REGIONAL_MAP = 3;
    static constexpr int APP_STATE_SITE_LOADING = 4;
    static constexpr int APP_STATE_SITE_ACTIVE = 5;
    static constexpr int APP_STATE_SITE_PAUSED = 6;
    static constexpr int APP_STATE_SITE_RESULT = 7;
    static constexpr int APP_STATE_CAMPAIGN_END = 8;

    godot::NodePath scene_host_path_ {"SceneHost"};
    godot::String loading_scene_path_ {"res://scenes/loading.tscn"};
    godot::String main_menu_scene_path_ {"res://scenes/main_menu.tscn"};
    godot::String regional_map_scene_path_ {"res://scenes/regional_map.tscn"};
    godot::String site_session_scene_path_ {"res://scenes/site_session.tscn"};

    Gs1GodotAdapterService adapter_service_ {};
    godot::Control* scene_host_ {nullptr};
    godot::ObjectID active_scene_id_ {};
    ScreenKind active_screen_kind_ {SCREEN_KIND_NONE};
    ScreenKind pending_async_target_kind_ {SCREEN_KIND_NONE};
    godot::String pending_async_scene_path_ {};
    std::vector<godot::String> pending_async_resource_paths_ {};
    int last_app_state_ {-1};
};
