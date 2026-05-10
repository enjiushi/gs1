#include "gs1_godot_regional_map_click_policy.h"

#include <cassert>

namespace
{
void non_left_mouse_input_is_ignored()
{
    assert(
        gs1_godot_resolve_regional_map_input(false, false, false, false) ==
        GS1_GODOT_REGIONAL_MAP_INPUT_IGNORE);
}

void ui_hits_are_consumed_before_world_click_logic()
{
    assert(
        gs1_godot_resolve_regional_map_input(true, true, false, false) ==
        GS1_GODOT_REGIONAL_MAP_INPUT_CONSUME_UI);
    assert(
        gs1_godot_resolve_regional_map_input(true, true, true, true) ==
        GS1_GODOT_REGIONAL_MAP_INPUT_CONSUME_UI);
}

void blank_world_click_without_selection_is_consumed()
{
    assert(
        gs1_godot_resolve_regional_map_world_click(false, false) ==
        GS1_GODOT_REGIONAL_MAP_WORLD_CLICK_CONSUME_BLANK);
    assert(
        gs1_godot_resolve_regional_map_input(true, false, false, false) ==
        GS1_GODOT_REGIONAL_MAP_INPUT_CONSUME_BLANK);
}

void blank_world_click_with_selection_requests_clear()
{
    assert(
        gs1_godot_resolve_regional_map_world_click(false, true) ==
        GS1_GODOT_REGIONAL_MAP_WORLD_CLICK_CLEAR_SELECTION);
    assert(
        gs1_godot_resolve_regional_map_input(true, false, false, true) ==
        GS1_GODOT_REGIONAL_MAP_INPUT_CLEAR_SELECTION);
}

void picked_site_keeps_selection_path()
{
    assert(
        gs1_godot_resolve_regional_map_world_click(true, false) ==
        GS1_GODOT_REGIONAL_MAP_WORLD_CLICK_SITE_PICKED);
    assert(
        gs1_godot_resolve_regional_map_world_click(true, true) ==
        GS1_GODOT_REGIONAL_MAP_WORLD_CLICK_SITE_PICKED);
    assert(
        gs1_godot_resolve_regional_map_input(true, false, true, false) ==
        GS1_GODOT_REGIONAL_MAP_INPUT_SITE_PICKED);
    assert(
        gs1_godot_resolve_regional_map_input(true, false, true, true) ==
        GS1_GODOT_REGIONAL_MAP_INPUT_SITE_PICKED);
}
}

int main()
{
    non_left_mouse_input_is_ignored();
    ui_hits_are_consumed_before_world_click_logic();
    blank_world_click_without_selection_is_consumed();
    blank_world_click_with_selection_requests_clear();
    picked_site_keeps_selection_path();
    return 0;
}
