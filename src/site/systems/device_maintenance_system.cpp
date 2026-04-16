#include "site/systems/device_maintenance_system.h"

#include "site/site_run_state.h"

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

}  // namespace

bool DeviceMaintenanceSystem::subscribes_to(GameCommandType type) noexcept
{
    return type == GameCommandType::SiteDevicePlaced;
}

Gs1Status DeviceMaintenanceSystem::process_command(
    SiteSystemContext<DeviceMaintenanceSystem>& context,
    const GameCommand& command)
{
    if (command.type != GameCommandType::SiteDevicePlaced)
    {
        return GS1_STATUS_OK;
    }

    const auto& payload = command.payload_as<SiteDevicePlacedCommand>();
    const TileCoord target_tile {payload.target_tile_x, payload.target_tile_y};
    if (!context.world.tile_coord_in_bounds(target_tile))
    {
        return GS1_STATUS_INVALID_ARGUMENT;
    }

    auto tile = context.world.read_tile(target_tile);
    tile.device.structure_id = StructureId {payload.structure_id};
    tile.device.device_integrity = 1.0f;
    context.world.write_tile(target_tile, tile);
    context.world.mark_tile_projection_dirty(target_tile);
    return GS1_STATUS_OK;
}

void DeviceMaintenanceSystem::run(SiteSystemContext<DeviceMaintenanceSystem>& context)
{
    if (!context.world.has_world())
    {
        return;
    }

    const float step_seconds = static_cast<float>(context.fixed_step_seconds);
    if (step_seconds <= 0.0f)
    {
        return;
    }

    const float weather_wear_base =
        std::fabs(context.world.read_weather().weather_heat) +
        std::fabs(context.world.read_weather().weather_wind) +
        std::fabs(context.world.read_weather().weather_dust);
    const float weather_wear = weather_wear_base * k_weather_wear_per_unit * step_seconds;

    const auto tile_count = context.world.tile_count();
    for (std::size_t index = 0; index < tile_count; ++index)
    {
        auto tile = context.world.read_tile_at_index(index);
        if (tile.device.structure_id.value == 0U)
        {
            continue;
        }

        const float burial_amount = std::clamp(tile.ecology.sand_burial, 0.0f, 1.0f);
        const float burial_wear = burial_amount * k_burial_wear_per_unit * step_seconds;
        const float total_wear = weather_wear + burial_wear;
        if (total_wear <= 0.0f)
        {
            continue;
        }

        const float next_integrity =
            std::clamp(tile.device.device_integrity - total_wear, 0.0f, 1.0f);
        if (std::fabs(next_integrity - tile.device.device_integrity) <= k_integrity_epsilon)
        {
            continue;
        }

        tile.device.device_integrity = next_integrity;
        context.world.write_tile_at_index(index, tile);
    }
}
}  // namespace gs1
