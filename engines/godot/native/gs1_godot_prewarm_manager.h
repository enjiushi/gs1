#pragma once

#include "gs1_godot_prewarm_config.h"

#include <godot_cpp/classes/packed_scene.hpp>
#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/variant/string.hpp>

#include <string>
#include <unordered_map>

class Gs1GodotPrewarmManager final
{
public:
    void set_current_screen(Gs1GodotDirectorScreenKind screen_kind);
    void poll();
    void clear() noexcept;

    [[nodiscard]] godot::Ref<godot::PackedScene> find_retained_packed_scene(const godot::String& path) const;

private:
    struct CachedTarget final
    {
        Gs1GodotPrewarmTargetKind kind {Gs1GodotPrewarmTargetKind::Resource};
        Gs1GodotPrewarmRetainPolicy retain_policy {Gs1GodotPrewarmRetainPolicy::UntilScreenExit};
        godot::String path {};
        godot::String resource_type {};
        godot::Ref<godot::Resource> retained_resource {};
        bool threaded_request_started {false};
        bool load_failed {false};
        bool manual_retained {false};
    };

    void rebuild_active_requests();
    void start_threaded_request(CachedTarget& target);

    Gs1GodotDirectorScreenKind current_screen_kind_ {GS1_GODOT_SCREEN_KIND_NONE};
    std::unordered_map<std::string, CachedTarget> cached_targets_ {};
};
