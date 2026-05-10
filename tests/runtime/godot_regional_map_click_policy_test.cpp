#include "gs1_godot_regional_map_click_policy.h"

#include <cassert>

namespace
{
constexpr std::uint32_t k_site_state_locked = 0U;
constexpr std::uint32_t k_site_state_available = 1U;
constexpr std::uint32_t k_site_state_completed = 2U;

void non_left_mouse_input_is_ignored()
{
    assert(
        gs1_godot_resolve_regional_map_input(
            false,
            false,
            GS1_GODOT_REGIONAL_MAP_PICK_NONE,
            false) ==
        GS1_GODOT_REGIONAL_MAP_INPUT_IGNORE);
}

void ui_hits_are_consumed_before_world_click_logic()
{
    assert(
        gs1_godot_resolve_regional_map_input(
            true,
            true,
            GS1_GODOT_REGIONAL_MAP_PICK_NONE,
            false) ==
        GS1_GODOT_REGIONAL_MAP_INPUT_CONSUME_UI);
    assert(
        gs1_godot_resolve_regional_map_input(
            true,
            true,
            GS1_GODOT_REGIONAL_MAP_PICK_SELECTABLE_SITE,
            true) ==
        GS1_GODOT_REGIONAL_MAP_INPUT_CONSUME_UI);
}

void blank_world_click_without_selection_is_consumed()
{
    assert(
        gs1_godot_resolve_regional_map_world_click(
            GS1_GODOT_REGIONAL_MAP_PICK_NONE,
            false) ==
        GS1_GODOT_REGIONAL_MAP_WORLD_CLICK_CONSUME_BLANK);
    assert(
        gs1_godot_resolve_regional_map_input(
            true,
            false,
            GS1_GODOT_REGIONAL_MAP_PICK_NONE,
            false) ==
        GS1_GODOT_REGIONAL_MAP_INPUT_CONSUME_BLANK);
}

void blank_world_click_with_selection_requests_clear()
{
    assert(
        gs1_godot_resolve_regional_map_world_click(
            GS1_GODOT_REGIONAL_MAP_PICK_NONE,
            true) ==
        GS1_GODOT_REGIONAL_MAP_WORLD_CLICK_CLEAR_SELECTION);
    assert(
        gs1_godot_resolve_regional_map_input(
            true,
            false,
            GS1_GODOT_REGIONAL_MAP_PICK_NONE,
            true) ==
        GS1_GODOT_REGIONAL_MAP_INPUT_CLEAR_SELECTION);
}

void picked_site_keeps_selection_path()
{
    assert(
        gs1_godot_resolve_regional_map_world_click(
            GS1_GODOT_REGIONAL_MAP_PICK_SELECTABLE_SITE,
            false) ==
        GS1_GODOT_REGIONAL_MAP_WORLD_CLICK_SITE_PICKED);
    assert(
        gs1_godot_resolve_regional_map_world_click(
            GS1_GODOT_REGIONAL_MAP_PICK_SELECTABLE_SITE,
            true) ==
        GS1_GODOT_REGIONAL_MAP_WORLD_CLICK_SITE_PICKED);
    assert(
        gs1_godot_resolve_regional_map_input(
            true,
            false,
            GS1_GODOT_REGIONAL_MAP_PICK_SELECTABLE_SITE,
            false) ==
        GS1_GODOT_REGIONAL_MAP_INPUT_SITE_PICKED);
    assert(
        gs1_godot_resolve_regional_map_input(
            true,
            false,
            GS1_GODOT_REGIONAL_MAP_PICK_SELECTABLE_SITE,
            true) ==
        GS1_GODOT_REGIONAL_MAP_INPUT_SITE_PICKED);
}

void selectable_site_state_matches_gameplay_deploy_rules()
{
    assert(gs1_godot_regional_map_site_is_selectable(k_site_state_available));
    assert(!gs1_godot_regional_map_site_is_selectable(k_site_state_locked));
    assert(!gs1_godot_regional_map_site_is_selectable(k_site_state_completed));
}

void unselectable_site_click_is_consumed_without_clearing_selection()
{
    assert(
        gs1_godot_resolve_regional_map_world_click(
            GS1_GODOT_REGIONAL_MAP_PICK_UNSELECTABLE_SITE,
            false) ==
        GS1_GODOT_REGIONAL_MAP_WORLD_CLICK_CONSUME_UNSELECTABLE_SITE);
    assert(
        gs1_godot_resolve_regional_map_world_click(
            GS1_GODOT_REGIONAL_MAP_PICK_UNSELECTABLE_SITE,
            true) ==
        GS1_GODOT_REGIONAL_MAP_WORLD_CLICK_CONSUME_UNSELECTABLE_SITE);
    assert(
        gs1_godot_resolve_regional_map_input(
            true,
            false,
            GS1_GODOT_REGIONAL_MAP_PICK_UNSELECTABLE_SITE,
            false) ==
        GS1_GODOT_REGIONAL_MAP_INPUT_CONSUME_UNSELECTABLE_SITE);
    assert(
        gs1_godot_resolve_regional_map_input(
            true,
            false,
            GS1_GODOT_REGIONAL_MAP_PICK_UNSELECTABLE_SITE,
            true) ==
        GS1_GODOT_REGIONAL_MAP_INPUT_CONSUME_UNSELECTABLE_SITE);
}
}

int main()
{
    non_left_mouse_input_is_ignored();
    ui_hits_are_consumed_before_world_click_logic();
    blank_world_click_without_selection_is_consumed();
    blank_world_click_with_selection_requests_clear();
    picked_site_keeps_selection_path();
    selectable_site_state_matches_gameplay_deploy_rules();
    unselectable_site_click_is_consumed_without_clearing_selection();
    return 0;
}
