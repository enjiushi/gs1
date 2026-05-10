#pragma once

#include <cstdint>

enum Gs1GodotRegionalMapPickOutcome : std::uint8_t
{
    GS1_GODOT_REGIONAL_MAP_PICK_NONE = 0,
    GS1_GODOT_REGIONAL_MAP_PICK_SELECTABLE_SITE = 1,
    GS1_GODOT_REGIONAL_MAP_PICK_UNSELECTABLE_SITE = 2,
};

enum Gs1GodotRegionalMapWorldClickOutcome : std::uint8_t
{
    GS1_GODOT_REGIONAL_MAP_WORLD_CLICK_CONSUME_BLANK = 0,
    GS1_GODOT_REGIONAL_MAP_WORLD_CLICK_CLEAR_SELECTION = 1,
    GS1_GODOT_REGIONAL_MAP_WORLD_CLICK_SITE_PICKED = 2,
    GS1_GODOT_REGIONAL_MAP_WORLD_CLICK_CONSUME_UNSELECTABLE_SITE = 3,
};

enum Gs1GodotRegionalMapInputOutcome : std::uint8_t
{
    GS1_GODOT_REGIONAL_MAP_INPUT_IGNORE = 0,
    GS1_GODOT_REGIONAL_MAP_INPUT_CONSUME_UI = 1,
    GS1_GODOT_REGIONAL_MAP_INPUT_CONSUME_BLANK = 2,
    GS1_GODOT_REGIONAL_MAP_INPUT_CLEAR_SELECTION = 3,
    GS1_GODOT_REGIONAL_MAP_INPUT_SITE_PICKED = 4,
    GS1_GODOT_REGIONAL_MAP_INPUT_CONSUME_UNSELECTABLE_SITE = 5,
};

inline constexpr bool gs1_godot_regional_map_site_is_selectable(std::uint32_t site_state) noexcept
{
    // Mirrors the shared runtime contract where 1 == GS1_SITE_STATE_AVAILABLE.
    return site_state == 1U;
}

inline constexpr Gs1GodotRegionalMapWorldClickOutcome gs1_godot_resolve_regional_map_world_click(
    Gs1GodotRegionalMapPickOutcome pick_outcome,
    bool has_selected_site) noexcept
{
    if (pick_outcome == GS1_GODOT_REGIONAL_MAP_PICK_SELECTABLE_SITE)
    {
        return GS1_GODOT_REGIONAL_MAP_WORLD_CLICK_SITE_PICKED;
    }
    if (pick_outcome == GS1_GODOT_REGIONAL_MAP_PICK_UNSELECTABLE_SITE)
    {
        return GS1_GODOT_REGIONAL_MAP_WORLD_CLICK_CONSUME_UNSELECTABLE_SITE;
    }

    return has_selected_site
        ? GS1_GODOT_REGIONAL_MAP_WORLD_CLICK_CLEAR_SELECTION
        : GS1_GODOT_REGIONAL_MAP_WORLD_CLICK_CONSUME_BLANK;
}

inline constexpr Gs1GodotRegionalMapInputOutcome gs1_godot_resolve_regional_map_input(
    bool left_mouse_pressed,
    bool ui_contains_screen_point,
    Gs1GodotRegionalMapPickOutcome pick_outcome,
    bool has_selected_site) noexcept
{
    if (!left_mouse_pressed)
    {
        return GS1_GODOT_REGIONAL_MAP_INPUT_IGNORE;
    }

    if (ui_contains_screen_point)
    {
        return GS1_GODOT_REGIONAL_MAP_INPUT_CONSUME_UI;
    }

    switch (gs1_godot_resolve_regional_map_world_click(pick_outcome, has_selected_site))
    {
    case GS1_GODOT_REGIONAL_MAP_WORLD_CLICK_SITE_PICKED:
        return GS1_GODOT_REGIONAL_MAP_INPUT_SITE_PICKED;
    case GS1_GODOT_REGIONAL_MAP_WORLD_CLICK_CONSUME_UNSELECTABLE_SITE:
        return GS1_GODOT_REGIONAL_MAP_INPUT_CONSUME_UNSELECTABLE_SITE;
    case GS1_GODOT_REGIONAL_MAP_WORLD_CLICK_CLEAR_SELECTION:
        return GS1_GODOT_REGIONAL_MAP_INPUT_CLEAR_SELECTION;
    case GS1_GODOT_REGIONAL_MAP_WORLD_CLICK_CONSUME_BLANK:
    default:
        return GS1_GODOT_REGIONAL_MAP_INPUT_CONSUME_BLANK;
    }
}
