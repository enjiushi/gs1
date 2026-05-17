#pragma once

#include "gs1_godot_director_scene_policy.h"

#include <cstdint>
#include <string>
#include <vector>

enum class Gs1GodotPrewarmTargetKind : std::uint8_t
{
    Scene = 0,
    Resource = 1
};

enum class Gs1GodotPrewarmRetainPolicy : std::uint8_t
{
    Inherit = 0,
    UntilTransitionConsumed = 1,
    UntilScreenExit = 2,
    Manual = 3
};

struct Gs1GodotPrewarmTargetDef final
{
    Gs1GodotPrewarmTargetKind kind {Gs1GodotPrewarmTargetKind::Resource};
    Gs1GodotPrewarmRetainPolicy retain_policy {Gs1GodotPrewarmRetainPolicy::UntilScreenExit};
    std::string path {};
    std::string resource_type {};
};

struct Gs1GodotPrewarmTransitionDef final
{
    Gs1GodotDirectorScreenKind from {GS1_GODOT_SCREEN_KIND_NONE};
    Gs1GodotDirectorScreenKind to {GS1_GODOT_SCREEN_KIND_NONE};
    std::vector<Gs1GodotPrewarmTargetDef> targets {};
};

class Gs1GodotPrewarmConfig final
{
public:
    static const Gs1GodotPrewarmConfig& instance();

    [[nodiscard]] const std::vector<Gs1GodotPrewarmTransitionDef>& transitions_from(
        Gs1GodotDirectorScreenKind screen_kind) const noexcept;

private:
    Gs1GodotPrewarmConfig();

    std::vector<Gs1GodotPrewarmTransitionDef> transitions_by_from_[5] {};
};
