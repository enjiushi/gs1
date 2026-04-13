#include "site/systems/device_maintenance_system.h"

#include "site/site_run_state.h"
#include "site/tile_grid_state.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace gs1
{
namespace
{
constexpr float k_integrity_epsilon = 0.0001f;
constexpr float k_weather_wear_per_unit = 0.0001f;
constexpr float k_burial_wear_per_unit = 0.05f;

bool has_valid_device_data(const TileGridState& tile_grid) noexcept
{
    const auto tile_count = tile_grid.tile_count();
    return tile_count > 0 &&
        tile_grid.structure_type_ids.size() >= tile_count &&
        tile_grid.device_integrity.size() >= tile_count &&
        tile_grid.device_efficiency.size() >= tile_count &&
        tile_grid.tile_sand_burial.size() >= tile_count;
}

}  // namespace

bool DeviceMaintenanceSystem::subscribes_to(GameCommandType type) noexcept
{
    (void)type;
    return false;
}

Gs1Status DeviceMaintenanceSystem::process_command(
    SiteSystemContext& context,
    const GameCommand& command)
{
    (void)context;
    (void)command;
    return GS1_STATUS_OK;
}

void DeviceMaintenanceSystem::run(SiteSystemContext& context)
{
    auto& site_run = context.site_run;
    auto& tile_grid = site_run.tile_grid;

    const auto tile_count = tile_grid.tile_count();
    if (tile_count == 0U || !has_valid_device_data(tile_grid))
    {
        return;
    }

    const float step_seconds = static_cast<float>(context.fixed_step_seconds);
    if (step_seconds <= 0.0f)
    {
        return;
    }

    const float weather_wear_base =
        std::fabs(site_run.weather.weather_heat) +
        std::fabs(site_run.weather.weather_wind) +
        std::fabs(site_run.weather.weather_dust);
    const float weather_wear = weather_wear_base * k_weather_wear_per_unit * step_seconds;

    for (std::size_t index = 0; index < tile_count; ++index)
    {
        if (tile_grid.structure_type_ids[index].value == 0U)
        {
            continue;
        }

        const float burial_amount = std::clamp(tile_grid.tile_sand_burial[index], 0.0f, 1.0f);
        const float burial_wear = burial_amount * k_burial_wear_per_unit * step_seconds;
        const float total_wear = weather_wear + burial_wear;

        if (total_wear <= 0.0f)
        {
            continue;
        }

        const float previous_integrity = tile_grid.device_integrity[index];
        const float next_integrity = std::clamp(previous_integrity - total_wear, 0.0f, 1.0f);
        const bool integrity_changed = std::fabs(next_integrity - previous_integrity) > k_integrity_epsilon;

        const float previous_efficiency = tile_grid.device_efficiency[index];
        const float next_efficiency = next_integrity;
        const bool efficiency_changed = std::fabs(next_efficiency - previous_efficiency) > k_integrity_epsilon;

        if (!integrity_changed && !efficiency_changed)
        {
            continue;
        }

        if (integrity_changed)
        {
            tile_grid.device_integrity[index] = next_integrity;
        }

        if (efficiency_changed)
        {
            tile_grid.device_efficiency[index] = next_efficiency;
        }
    }
}
}  // namespace gs1
