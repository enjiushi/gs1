#pragma once

#include "gs1_godot_adapter_service.h"
#include "gs1_godot_director_scene_policy.h"
#include "gs1_godot_prewarm_manager.h"
#include "shared_framework/godot/scene_director_framework.h"

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/core/binder_common.hpp>
#include <godot_cpp/variant/node_path.hpp>
#include <godot_cpp/variant/string.hpp>

#include <vector>

class Gs1GodotDirectorControl final : public godot::Control, public IGs1GodotNotificationSubscriber
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
    void set_loading_reveal_delay_frames(int frames);
    [[nodiscard]] int get_loading_reveal_delay_frames() const noexcept;

    [[nodiscard]] Gs1GodotAdapterService& adapter_service() noexcept { return adapter_service_; }
    [[nodiscard]] const Gs1GodotAdapterService& adapter_service() const noexcept { return adapter_service_; }

    [[nodiscard]] bool handles_notification(Gs1GodotNotificationType type) const noexcept override;
    void handle_notification(const Gs1GodotNotification& message) override;
    void handle_runtime_message_reset() override;

protected:
    static void _bind_methods();

private:
    using ScreenKind = Gs1GodotDirectorScreenKind;

    void refresh_scene_route();
    void sync_prewarm_current_screen();
    [[nodiscard]] std::vector<godot::String> build_async_preload_paths(ScreenKind kind) const;
    [[nodiscard]] bool should_async_load_transition(ScreenKind kind) const noexcept;
    [[nodiscard]] ScreenKind desired_screen_kind(int app_state) const noexcept;
    [[nodiscard]] godot::String scene_path_for(ScreenKind kind) const;
    [[nodiscard]] godot::Node* instantiate_scene_for_framework(const godot::String& scene_path) const;

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

    godot::String loading_scene_path_ {"res://scenes/loading.tscn"};
    godot::String main_menu_scene_path_ {"res://scenes/main_menu.tscn"};
    godot::String regional_map_scene_path_ {"res://scenes/regional_map.tscn"};
    godot::String site_session_scene_path_ {"res://scenes/site_session.tscn"};
    Gs1GodotAdapterService adapter_service_ {};
    Gs1GodotPrewarmManager prewarm_manager_ {};
    shared_framework::godot::SceneDirectorFramework scene_director_ {};
    int last_app_state_ {-1};
    int last_prewarm_screen_kind_ {GS1_GODOT_SCREEN_KIND_NONE};
};
