#pragma once

#include <cstdint>

enum Gs1GodotRegionalMapWorldClickOutcome : std::uint8_t
{
    GS1_GODOT_REGIONAL_MAP_WORLD_CLICK_CONSUME_BLANK = 0,
    GS1_GODOT_REGIONAL_MAP_WORLD_CLICK_CLEAR_SELECTION = 1,
    GS1_GODOT_REGIONAL_MAP_WORLD_CLICK_SITE_PICKED = 2,
};

enum Gs1GodotRegionalMapInputOutcome : std::uint8_t
{
    GS1_GODOT_REGIONAL_MAP_INPUT_IGNORE = 0,
    GS1_GODOT_REGIONAL_MAP_INPUT_CONSUME_UI = 1,
    GS1_GODOT_REGIONAL_MAP_INPUT_CONSUME_BLANK = 2,
    GS1_GODOT_REGIONAL_MAP_INPUT_CLEAR_SELECTION = 3,
    GS1_GODOT_REGIONAL_MAP_INPUT_SITE_PICKED = 4,
};

inline constexpr Gs1GodotRegionalMapWorldClickOutcome gs1_godot_resolve_regional_map_world_click(
    bool picked_site,
    bool has_selected_site) noexcept
{
    if (picked_site)
    {
        return GS1_GODOT_REGIONAL_MAP_WORLD_CLICK_SITE_PICKED;
    }

    return has_selected_site
        ? GS1_GODOT_REGIONAL_MAP_WORLD_CLICK_CLEAR_SELECTION
        : GS1_GODOT_REGIONAL_MAP_WORLD_CLICK_CONSUME_BLANK;
}

inline constexpr Gs1GodotRegionalMapInputOutcome gs1_godot_resolve_regional_map_input(
    bool left_mouse_pressed,
    bool ui_contains_screen_point,
    bool picked_site,
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

    switch (gs1_godot_resolve_regional_map_world_click(picked_site, has_selected_site))
    {
    case GS1_GODOT_REGIONAL_MAP_WORLD_CLICK_SITE_PICKED:
        return GS1_GODOT_REGIONAL_MAP_INPUT_SITE_PICKED;
    case GS1_GODOT_REGIONAL_MAP_WORLD_CLICK_CLEAR_SELECTION:
        return GS1_GODOT_REGIONAL_MAP_INPUT_CLEAR_SELECTION;
    case GS1_GODOT_REGIONAL_MAP_WORLD_CLICK_CONSUME_BLANK:
    default:
        return GS1_GODOT_REGIONAL_MAP_INPUT_CONSUME_BLANK;
    }
}
