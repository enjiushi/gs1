#include "gs1_godot_director_control.h"

#include "godot_progression_resources.h"
#include "content/defs/technology_defs.h"

#include <godot_cpp/classes/packed_scene.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/property_info.hpp>

#include <string>
#include <unordered_set>

using namespace godot;

namespace
{
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
    ClassDB::bind_method(
        D_METHOD("set_loading_reveal_delay_frames", "frames"),
        &Gs1GodotDirectorControl::set_loading_reveal_delay_frames);
    ClassDB::bind_method(
        D_METHOD("get_loading_reveal_delay_frames"),
        &Gs1GodotDirectorControl::get_loading_reveal_delay_frames);
    ClassDB::bind_method(D_METHOD("set_main_menu_scene_path", "path"), &Gs1GodotDirectorControl::set_main_menu_scene_path);
    ClassDB::bind_method(D_METHOD("get_main_menu_scene_path"), &Gs1GodotDirectorControl::get_main_menu_scene_path);
    ClassDB::bind_method(D_METHOD("set_regional_map_scene_path", "path"), &Gs1GodotDirectorControl::set_regional_map_scene_path);
    ClassDB::bind_method(D_METHOD("get_regional_map_scene_path"), &Gs1GodotDirectorControl::get_regional_map_scene_path);
    ClassDB::bind_method(D_METHOD("set_site_session_scene_path", "path"), &Gs1GodotDirectorControl::set_site_session_scene_path);
    ClassDB::bind_method(D_METHOD("get_site_session_scene_path"), &Gs1GodotDirectorControl::get_site_session_scene_path);

    ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "scene_host_path"), "set_scene_host_path", "get_scene_host_path");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "loading_scene_path"), "set_loading_scene_path", "get_loading_scene_path");
    ADD_PROPERTY(
        PropertyInfo(Variant::INT, "loading_reveal_delay_frames"),
        "set_loading_reveal_delay_frames",
        "get_loading_reveal_delay_frames");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "main_menu_scene_path"), "set_main_menu_scene_path", "get_main_menu_scene_path");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "regional_map_scene_path"), "set_regional_map_scene_path", "get_regional_map_scene_path");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "site_session_scene_path"), "set_site_session_scene_path", "get_site_session_scene_path");
}

void Gs1GodotDirectorControl::_ready()
{
    set_process(true);
    scene_director_.set_loading_scene_kind(GS1_GODOT_SCREEN_KIND_LOADING);
    scene_director_.set_loading_scene_path(loading_scene_path_);
    scene_director_.set_loading_reveal_delay_frames(get_loading_reveal_delay_frames());
    adapter_service_.subscribe(GS1_GODOT_NOTIFICATION_SET_APP_STATE, *this);
    refresh_scene_route();
}

void Gs1GodotDirectorControl::_process(double delta)
{
    adapter_service_.begin_frame(delta);
    prewarm_manager_.poll();
    refresh_scene_route();
    adapter_service_.finish_frame();
}

void Gs1GodotDirectorControl::refresh_scene_route()
{
    const int app_state = last_app_state_ >= 0 ? last_app_state_ : APP_STATE_BOOT;
    const ScreenKind desired_kind = desired_screen_kind(app_state);
    shared_framework::godot::SceneDirectorRoute desired_route {};
    desired_route.scene_kind = static_cast<int>(desired_kind);
    desired_route.scene_path = scene_path_for(desired_kind);
    desired_route.async_load = should_async_load_transition(desired_kind);
    if (desired_route.async_load)
    {
        desired_route.async_preload_paths = build_async_preload_paths(desired_kind);
    }

    scene_director_.refresh(
        *this,
        desired_route,
        [this](const String& scene_path) -> Node*
        {
            return instantiate_scene_for_framework(scene_path);
        });
    sync_prewarm_current_screen();
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

void Gs1GodotDirectorControl::sync_prewarm_current_screen()
{
    const int active_kind = scene_director_.active_scene_kind();
    if (active_kind == last_prewarm_screen_kind_)
    {
        return;
    }

    last_prewarm_screen_kind_ = active_kind;
    if (active_kind == GS1_GODOT_SCREEN_KIND_NONE ||
        active_kind == GS1_GODOT_SCREEN_KIND_LOADING)
    {
        return;
    }

    prewarm_manager_.set_current_screen(static_cast<ScreenKind>(active_kind));
}

bool Gs1GodotDirectorControl::should_async_load_transition(ScreenKind kind) const noexcept
{
    return !loading_scene_path_.is_empty() &&
        ((kind == GS1_GODOT_SCREEN_KIND_REGIONAL_MAP &&
                scene_director_.active_scene_kind() == GS1_GODOT_SCREEN_KIND_MAIN_MENU) ||
            (kind == GS1_GODOT_SCREEN_KIND_SITE_SESSION &&
                scene_director_.active_scene_kind() == GS1_GODOT_SCREEN_KIND_REGIONAL_MAP));
}

bool Gs1GodotDirectorControl::handles_notification(Gs1GodotNotificationType type) const noexcept
{
    return type == GS1_GODOT_NOTIFICATION_SET_APP_STATE;
}

void Gs1GodotDirectorControl::handle_notification(const Gs1GodotNotification& message)
{
    if (message.type != GS1_GODOT_NOTIFICATION_SET_APP_STATE)
    {
        return;
    }
    last_app_state_ = static_cast<int>(message.payload_as<Gs1GodotAppStateNotification>().app_state);
    refresh_scene_route();
}

void Gs1GodotDirectorControl::handle_runtime_message_reset()
{
    scene_director_.reset(*this);
    prewarm_manager_.clear();
    last_app_state_ = APP_STATE_BOOT;
    last_prewarm_screen_kind_ = GS1_GODOT_SCREEN_KIND_NONE;
    refresh_scene_route();
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

Node* Gs1GodotDirectorControl::instantiate_scene_for_framework(const String& scene_path) const
{
    if (Ref<PackedScene> prefetched_scene = prewarm_manager_.find_retained_packed_scene(scene_path);
        !prefetched_scene.is_null())
    {
        return prefetched_scene->instantiate();
    }

    return nullptr;
}

void Gs1GodotDirectorControl::set_scene_host_path(const NodePath& path)
{
    scene_director_.set_scene_host_path(path);
}

NodePath Gs1GodotDirectorControl::get_scene_host_path() const
{
    return scene_director_.scene_host_path();
}

void Gs1GodotDirectorControl::set_loading_scene_path(const String& path)
{
    loading_scene_path_ = path;
    scene_director_.set_loading_scene_path(path);
}

String Gs1GodotDirectorControl::get_loading_scene_path() const
{
    return loading_scene_path_;
}

void Gs1GodotDirectorControl::set_loading_reveal_delay_frames(int frames)
{
    scene_director_.set_loading_reveal_delay_frames(frames);
}

int Gs1GodotDirectorControl::get_loading_reveal_delay_frames() const noexcept
{
    return scene_director_.loading_reveal_delay_frames();
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
