#pragma once

#include "gs1_godot_projection_types.h"

#include <cstdint>
#include <optional>
#include <span>

inline constexpr const Gs1RuntimeRegionalMapSiteProjection* gs1_godot_find_explicitly_selected_regional_site(
    std::span<const Gs1RuntimeRegionalMapSiteProjection> sites,
    std::optional<std::uint32_t> selected_site_id) noexcept
{
    if (!selected_site_id.has_value())
    {
        return nullptr;
    }

    for (const auto& site : sites)
    {
        if (site.site_id == selected_site_id.value())
        {
            return &site;
        }
    }

    return nullptr;
}
