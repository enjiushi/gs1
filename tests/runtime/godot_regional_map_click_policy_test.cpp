#include "gs1_godot_regional_map_click_policy.h"

#include <cassert>

namespace
{
void blank_world_click_without_selection_is_consumed()
{
    assert(
        gs1_godot_resolve_regional_map_world_click(false, false) ==
        GS1_GODOT_REGIONAL_MAP_WORLD_CLICK_CONSUME_BLANK);
}

void blank_world_click_with_selection_requests_clear()
{
    assert(
        gs1_godot_resolve_regional_map_world_click(false, true) ==
        GS1_GODOT_REGIONAL_MAP_WORLD_CLICK_CLEAR_SELECTION);
}

void picked_site_keeps_selection_path()
{
    assert(
        gs1_godot_resolve_regional_map_world_click(true, false) ==
        GS1_GODOT_REGIONAL_MAP_WORLD_CLICK_SITE_PICKED);
    assert(
        gs1_godot_resolve_regional_map_world_click(true, true) ==
        GS1_GODOT_REGIONAL_MAP_WORLD_CLICK_SITE_PICKED);
}
}

int main()
{
    blank_world_click_without_selection_is_consumed();
    blank_world_click_with_selection_requests_clear();
    picked_site_keeps_selection_path();
    return 0;
}
