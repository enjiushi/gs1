#pragma once

#include <cstdint>

enum Gs1GodotRegionalMapWorldClickOutcome : std::uint8_t
{
    GS1_GODOT_REGIONAL_MAP_WORLD_CLICK_CONSUME_BLANK = 0,
    GS1_GODOT_REGIONAL_MAP_WORLD_CLICK_CLEAR_SELECTION = 1,
    GS1_GODOT_REGIONAL_MAP_WORLD_CLICK_SITE_PICKED = 2,
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
