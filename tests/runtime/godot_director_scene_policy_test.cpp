#include "gs1_godot_director_scene_policy.h"

#include <cassert>

int main()
{
    assert(gs1_godot_director_requires_scene_switch(
        false,
        GS1_GODOT_SCREEN_KIND_NONE,
        GS1_GODOT_SCREEN_KIND_MAIN_MENU));

    assert(!gs1_godot_director_requires_scene_switch(
        true,
        GS1_GODOT_SCREEN_KIND_REGIONAL_MAP,
        GS1_GODOT_SCREEN_KIND_REGIONAL_MAP));

    assert(!gs1_godot_director_requires_scene_switch(
        true,
        GS1_GODOT_SCREEN_KIND_SITE_SESSION,
        GS1_GODOT_SCREEN_KIND_SITE_SESSION));

    assert(gs1_godot_director_requires_scene_switch(
        true,
        GS1_GODOT_SCREEN_KIND_REGIONAL_MAP,
        GS1_GODOT_SCREEN_KIND_SITE_SESSION));

    return 0;
}
