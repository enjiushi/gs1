#include "gs1_godot_director_scene_policy.h"
#include "gs1_godot_engine_message_policy.h"

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

    assert(gs1_godot_engine_message_requires_exclusive_subscriber(
        GS1_ENGINE_MESSAGE_BEGIN_REGIONAL_MAP_SNAPSHOT));
    assert(!gs1_godot_engine_message_requires_exclusive_subscriber(
        GS1_ENGINE_MESSAGE_REGIONAL_MAP_SITE_UPSERT));
    assert(!gs1_godot_engine_message_requires_exclusive_subscriber(
        GS1_ENGINE_MESSAGE_SITE_WORKER_UPDATE));
    assert(!gs1_godot_engine_message_requires_exclusive_subscriber(
        GS1_ENGINE_MESSAGE_BEGIN_UI_SETUP));
    assert(!gs1_godot_engine_message_requires_exclusive_subscriber(
        GS1_ENGINE_MESSAGE_UI_ELEMENT_UPSERT));

    return 0;
}
