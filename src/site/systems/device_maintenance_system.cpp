#include "site/systems/device_maintenance_system.h"

#include "site/site_run_state.h"
#include "site/tile_footprint.h"

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

void emit_device_broken_message(
    GameMessageQueue& queue,
    std::uint64_t device_entity_id,
    TileCoord target_tile,
    StructureId structure_id)
{
    GameMessage message {};
    message.type = GameMessageType::SiteDeviceBroken;
    message.set_payload(SiteDeviceBrokenMessage {
        device_entity_id,
        target_tile.x,
        target_tile.y,
        structure_id.value});
    queue.push_back(message);
}

}  // namespace

bool DeviceMaintenanceSystem::subscribes_to(GameMessageType type) noexcept
{
    return type == GameMessageType::SiteDevicePlaced ||
        type == GameMessageType::SiteDeviceRepaired;
}

Gs1Status DeviceMaintenanceSystem::process_message(
    SiteSystemContext<DeviceMaintenanceSystem>& context,
    const GameMessage& message)
{
    if (message.type == GameMessageType::SiteDeviceRepaired)
    {
        const auto& payload = message.payload_as<SiteDeviceRepairedMessage>();
        const TileCoord target_tile {payload.target_tile_x, payload.target_tile_y};
        if (!context.world.tile_coord_in_bounds(target_tile))
        {
            return GS1_STATUS_INVALID_ARGUMENT;
        }

        const auto anchor_tile = context.world.read_tile(target_tile);
        if (anchor_tile.device.structure_id.value == 0U)
        {
            return GS1_STATUS_OK;
        }

        for_each_tile_in_footprint(
            target_tile,
            resolve_structure_tile_footprint(anchor_tile.device.structure_id),
            [&](TileCoord footprint_coord) {
                if (!context.world.tile_coord_in_bounds(footprint_coord))
                {
                    return;
                }

                auto tile = context.world.read_tile(footprint_coord);
                if (tile.device.structure_id != anchor_tile.device.structure_id)
                {
                    return;
                }

                tile.device.device_integrity = 1.0f;
                context.world.write_tile(footprint_coord, tile);
            });
        return GS1_STATUS_OK;
    }

    if (message.type != GameMessageType::SiteDevicePlaced)
    {
        return GS1_STATUS_OK;
    }

    const auto& payload = message.payload_as<SiteDevicePlacedMessage>();
    const TileCoord target_tile {payload.target_tile_x, payload.target_tile_y};
    if (!context.world.tile_coord_in_bounds(target_tile))
    {
        return GS1_STATUS_INVALID_ARGUMENT;
    }

    for_each_tile_in_footprint(
        target_tile,
        resolve_structure_tile_footprint(StructureId {payload.structure_id}),
        [&](TileCoord footprint_coord) {
            if (!context.world.tile_coord_in_bounds(footprint_coord))
            {
                return;
            }

            auto tile = context.world.read_tile(footprint_coord);
            tile.device.structure_id = StructureId {payload.structure_id};
            tile.device.device_integrity = 1.0f;
            context.world.write_tile(footprint_coord, tile);
            context.world.mark_tile_projection_dirty(footprint_coord);
        });
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

        const float burial_amount = std::clamp(tile.ecology.sand_burial / 100.0f, 0.0f, 1.0f);
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

        const auto coord = context.world.tile_coord(index);
        if (next_integrity <= 0.0f)
        {
            const auto device_entity_id = context.site_run.site_world->device_entity_id(coord);
            const auto structure_id = tile.device.structure_id;
            tile.device = SiteWorld::TileDeviceData {};
            context.world.write_tile_at_index(index, tile);
            context.world.mark_tile_projection_dirty(coord);
            if (device_entity_id != 0U)
            {
                emit_device_broken_message(
                    context.message_queue,
                    device_entity_id,
                    coord,
                    structure_id);
            }
            continue;
        }

        tile.device.device_integrity = next_integrity;
        context.world.write_tile_at_index(index, tile);
    }
}
}  // namespace gs1
