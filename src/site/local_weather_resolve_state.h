#pragma once

#include "site/site_world.h"

#include <cstdint>
#include <vector>

namespace gs1
{
struct LocalWeatherResolveMetaState final
{
    bool emit_full_snapshot_on_next_run {false};
};

struct LocalWeatherResolveState final
{
    std::vector<SiteWorld::TileWeatherContributionData> last_total_weather_contributions {};
    bool emit_full_snapshot_on_next_run {false};
};
}  // namespace gs1
