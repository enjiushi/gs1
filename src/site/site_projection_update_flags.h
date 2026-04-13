#pragma once

#include <cstdint>

namespace gs1
{
enum SiteProjectionUpdateFlags : std::uint64_t
{
    SITE_PROJECTION_UPDATE_NONE = 0,
    SITE_PROJECTION_UPDATE_TILES = 1ull << 0,
    SITE_PROJECTION_UPDATE_WORKER = 1ull << 1,
    SITE_PROJECTION_UPDATE_CAMP = 1ull << 2,
    SITE_PROJECTION_UPDATE_WEATHER = 1ull << 3,
    SITE_PROJECTION_UPDATE_HUD = 1ull << 4
};
}  // namespace gs1
