#pragma once

namespace shared_framework::godot {
[[nodiscard]] constexpr bool scene_director_requires_scene_switch(
    bool has_active_scene,
    int active_kind,
    int desired_kind) noexcept
{
    return !has_active_scene || active_kind != desired_kind;
}
}  // namespace shared_framework::godot
