#pragma once

#include "gs1_godot_main_menu_ui_controller.h"
#include "gs1_godot_regional_map_scene_controller.h"
#include "gs1_godot_regional_map_ui_controller.h"
#include "gs1_godot_runtime_node.h"
#include "gs1_godot_site_scene_controller.h"
#include "gs1_godot_site_session_ui_controller.h"
#include "gs1_godot_site_view_node.h"

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/core/binder_common.hpp>
#include <godot_cpp/core/object_id.hpp>
#include <godot_cpp/variant/node_path.hpp>
#include <godot_cpp/variant/string.hpp>

class Gs1GodotDirectorControl final : public godot::Control, public IGs1GodotEngineMessageSubscriber
{
    GDCLASS(Gs1GodotDirectorControl, godot::Control)

public:
    Gs1GodotDirectorControl() = default;
    ~Gs1GodotDirectorControl() override = default;

    void _ready() override;
    void _process(double delta) override;

    void set_runtime_node_path(const godot::NodePath& path);
    [[nodiscard]] godot::NodePath get_runtime_node_path() const;

    void set_scene_host_path(const godot::NodePath& path);
    [[nodiscard]] godot::NodePath get_scene_host_path() const;

    void set_main_menu_scene_path(const godot::String& path);
    [[nodiscard]] godot::String get_main_menu_scene_path() const;

    void set_regional_map_scene_path(const godot::String& path);
    [[nodiscard]] godot::String get_regional_map_scene_path() const;

    void set_site_session_scene_path(const godot::String& path);
    [[nodiscard]] godot::String get_site_session_scene_path() const;

    [[nodiscard]] bool handles_engine_message(Gs1EngineMessageType type) const noexcept override;
    void handle_engine_message(const Gs1EngineMessage& message) override;
    void handle_runtime_message_reset() override;

protected:
    static void _bind_methods();

private:
    enum ScreenKind : std::uint8_t
    {
        SCREEN_KIND_NONE = 0,
        SCREEN_KIND_MAIN_MENU = 1,
        SCREEN_KIND_REGIONAL_MAP = 2,
        SCREEN_KIND_SITE_SESSION = 3
    };

    void cache_nodes();
    void ensure_active_scene();
    [[nodiscard]] ScreenKind desired_screen_kind(int app_state) const noexcept;
    [[nodiscard]] godot::String scene_path_for(ScreenKind kind) const;
    void switch_to_scene(ScreenKind kind);
    [[nodiscard]] godot::Node* instantiate_scene(const godot::String& scene_path) const;
    void configure_scene_instance(godot::Node* instance) const;

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

    godot::NodePath runtime_node_path_ {"Runtime"};
    godot::NodePath scene_host_path_ {"SceneHost"};
    godot::String main_menu_scene_path_ {"res://scenes/main_menu.tscn"};
    godot::String regional_map_scene_path_ {"res://scenes/regional_map.tscn"};
    godot::String site_session_scene_path_ {"res://scenes/site_session.tscn"};

    Gs1RuntimeNode* runtime_node_ {nullptr};
    godot::Control* scene_host_ {nullptr};
    godot::ObjectID active_scene_id_ {};
    ScreenKind active_screen_kind_ {SCREEN_KIND_NONE};
    int last_app_state_ {-1};
};
