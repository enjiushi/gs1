#pragma once

#include <cstdint>

enum Gs1GodotDirectorScreenKind : std::uint8_t
{
    GS1_GODOT_SCREEN_KIND_NONE = 0,
    GS1_GODOT_SCREEN_KIND_LOADING = 1,
    GS1_GODOT_SCREEN_KIND_MAIN_MENU = 2,
    GS1_GODOT_SCREEN_KIND_REGIONAL_MAP = 3,
    GS1_GODOT_SCREEN_KIND_SITE_SESSION = 4
};

[[nodiscard]] constexpr bool gs1_godot_director_requires_scene_switch(
    bool has_active_scene,
    Gs1GodotDirectorScreenKind active_kind,
    Gs1GodotDirectorScreenKind desired_kind) noexcept
{
    return !has_active_scene || active_kind != desired_kind;
}
