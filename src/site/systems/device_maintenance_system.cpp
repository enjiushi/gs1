#include "site/systems/device_maintenance_system.h"

#include "content/defs/structure_defs.h"
#include "runtime/game_runtime.h"
#include "site/site_run_state.h"
#include "site/site_world_components.h"
#include "site/tile_footprint.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4127)
#endif
#include <flecs.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace
{
constexpr float k_integrity_epsilon = 0.0001f;
constexpr float k_weather_meter_max = 100.0f;

struct BrokenDeviceEntry final
{
    gs1::TileCoord coord {};
    std::uint64_t device_entity_id {0U};
    gs1::StructureId structure_id {};
};

void emit_device_broken_message(
    gs1::GameMessageQueue& queue,
    std::uint64_t device_entity_id,
    gs1::TileCoord target_tile,
    gs1::StructureId structure_id)
{
    gs1::GameMessage message {};
    message.type = gs1::GameMessageType::SiteDeviceBroken;
    message.set_payload(gs1::SiteDeviceBrokenMessage {
        device_entity_id,
        target_tile.x,
        target_tile.y,
        structure_id.value});
    queue.push_back(message);
}

void emit_device_condition_changed_message(
    gs1::GameMessageQueue& queue,
    gs1::TileCoord target_tile,
    gs1::StructureId structure_id,
    float integrity,
    std::uint32_t flags = 0U)
{
    gs1::GameMessage message {};
    message.type = gs1::GameMessageType::SiteDeviceConditionChanged;
    message.set_payload(gs1::SiteDeviceConditionChangedMessage {
        target_tile.x,
        target_tile.y,
        structure_id.value,
        integrity,
        flags});
    queue.push_back(message);
}

Gs1Status process_message(
    gs1::RuntimeInvocation& invocation,
    const gs1::GameMessage& message)
{
    auto access = gs1::make_game_state_access<gs1::DeviceMaintenanceSystem>(invocation);
    auto& site_run = access.template read<gs1::RuntimeActiveSiteRunTag>();
    gs1::SiteWorldAccess<gs1::DeviceMaintenanceSystem> world {*site_run};
    auto& message_queue = invocation.game_message_queue();
    if (message.type == gs1::GameMessageType::SiteRunStarted)
    {
        if (!world.has_world() || site_run->site_world == nullptr)
        {
            return GS1_STATUS_OK;
        }

        auto source_query =
            site_run->site_world->ecs_world()
                .query_builder<
                    const gs1::site_ecs::TileCoordComponent,
                    const gs1::site_ecs::DeviceStructureId,
                    const gs1::site_ecs::DeviceIntegrity>()
                .with<gs1::site_ecs::DeviceTag>()
                .build();

        source_query.each(
            [&](flecs::entity,
                const gs1::site_ecs::TileCoordComponent& coord_component,
                const gs1::site_ecs::DeviceStructureId& structure_component,
                const gs1::site_ecs::DeviceIntegrity& integrity_component) {
                if (structure_component.structure_id.value == 0U)
                {
                    return;
                }

                emit_device_condition_changed_message(
                    message_queue,
                    coord_component.value,
                    structure_component.structure_id,
                    integrity_component.value);
            });
        return GS1_STATUS_OK;
    }

    if (message.type == gs1::GameMessageType::SiteDeviceRepaired)
    {
        const auto& payload = message.payload_as<gs1::SiteDeviceRepairedMessage>();
        const gs1::TileCoord target_tile {payload.target_tile_x, payload.target_tile_y};
        if (!world.tile_coord_in_bounds(target_tile) || site_run->site_world == nullptr)
        {
            return GS1_STATUS_INVALID_ARGUMENT;
        }

        const auto anchor_device = site_run->site_world->tile_device(target_tile);
        if (anchor_device.structure_id.value == 0U)
        {
            return GS1_STATUS_OK;
        }

        gs1::for_each_tile_in_footprint(
            target_tile,
            gs1::resolve_structure_tile_footprint(anchor_device.structure_id),
            [&](gs1::TileCoord footprint_coord) {
                if (!world.tile_coord_in_bounds(footprint_coord))
                {
                    return;
                }

                auto device = site_run->site_world->tile_device(footprint_coord);
                if (device.structure_id != anchor_device.structure_id)
                {
                    return;
                }

                device.device_integrity = 1.0f;
                site_run->site_world->set_tile_device(footprint_coord, device);
                emit_device_condition_changed_message(
                    message_queue,
                    footprint_coord,
                    device.structure_id,
                    device.device_integrity);
            });
        return GS1_STATUS_OK;
    }

    if (message.type != gs1::GameMessageType::SiteDevicePlaced)
    {
        return GS1_STATUS_OK;
    }

    const auto& payload = message.payload_as<gs1::SiteDevicePlacedMessage>();
    const gs1::TileCoord target_tile {payload.target_tile_x, payload.target_tile_y};
    if (!world.tile_coord_in_bounds(target_tile) || site_run->site_world == nullptr)
    {
            return GS1_STATUS_INVALID_ARGUMENT;
    }

    gs1::for_each_tile_in_footprint(
        target_tile,
        gs1::resolve_structure_tile_footprint(gs1::StructureId {payload.structure_id}),
        [&](gs1::TileCoord footprint_coord) {
            if (!world.tile_coord_in_bounds(footprint_coord))
            {
                return;
            }

            site_run->site_world->set_tile_device(
                footprint_coord,
                gs1::SiteWorld::TileDeviceData {
                    gs1::StructureId {payload.structure_id},
                    1.0f,
                    1.0f,
                    0.0f,
                    false});
            emit_device_condition_changed_message(
                message_queue,
                footprint_coord,
                gs1::StructureId {payload.structure_id},
                1.0f);
        });
    return GS1_STATUS_OK;
}

void run_system(gs1::RuntimeInvocation& invocation)
{
    auto access = gs1::make_game_state_access<gs1::DeviceMaintenanceSystem>(invocation);
    auto& site_run = access.template read<gs1::RuntimeActiveSiteRunTag>();
    gs1::SiteWorldAccess<gs1::DeviceMaintenanceSystem> world {*site_run};
    auto& message_queue = invocation.game_message_queue();
    if (!world.has_world())
    {
        return;
    }

    const float step_seconds = static_cast<float>(
        access.template read<gs1::RuntimeFixedStepSecondsTag>());
    if (step_seconds <= 0.0f)
    {
        return;
    }

    const float max_weather_meter = std::max({
        std::fabs(world.read_weather().weather_heat),
        std::fabs(world.read_weather().weather_wind),
        std::fabs(world.read_weather().weather_dust)});
    const float weather_factor = std::clamp(max_weather_meter / k_weather_meter_max, 0.0f, 1.0f);
    if (site_run->site_world == nullptr)
    {
        return;
    }

    auto& ecs_world = site_run->site_world->ecs_world();
    auto device_query =
        ecs_world.query_builder<
            const gs1::site_ecs::TileCoordComponent,
            const gs1::site_ecs::DeviceStructureId,
            const gs1::site_ecs::DeviceIntegrity,
            const gs1::site_ecs::DeviceFixedIntegrity>()
            .with<gs1::site_ecs::DeviceTag>()
            .build();
    std::vector<BrokenDeviceEntry> broken_devices {};

    // Iterate only actual device entities; dense tile scans are disproportionately
    // expensive because TileData read/write fans out into many ECS component fetches.
    device_query.each(
        [&](flecs::entity entity,
            const gs1::site_ecs::TileCoordComponent& coord_component,
            const gs1::site_ecs::DeviceStructureId& structure_component,
            const gs1::site_ecs::DeviceIntegrity& integrity_component,
            const gs1::site_ecs::DeviceFixedIntegrity& fixed_integrity_component) {
            const auto structure_id = structure_component.structure_id;
            if (structure_id.value == 0U)
            {
                return;
            }

            if (fixed_integrity_component.value)
            {
                return;
            }

            const auto* structure_def = gs1::find_structure_def(structure_id);
            const float base_loss_per_second =
                structure_def == nullptr ? 0.0f : structure_def->integrity_loss_per_second_at_max_weather;
            const float total_wear = std::lerp(0.0f, base_loss_per_second, weather_factor) * step_seconds;
            if (total_wear <= 0.0f)
            {
                return;
            }

            const auto coord = coord_component.value;
            const float next_integrity =
                std::clamp(integrity_component.value - total_wear, 0.0f, 1.0f);
            if (std::fabs(next_integrity - integrity_component.value) <= k_integrity_epsilon)
            {
                return;
            }

            if (next_integrity <= 0.0f)
            {
                broken_devices.push_back(BrokenDeviceEntry {
                    coord,
                    static_cast<std::uint64_t>(entity.id()),
                    structure_id});
                return;
            }

            entity.set<gs1::site_ecs::DeviceIntegrity>({next_integrity});
            emit_device_condition_changed_message(
                message_queue,
                coord,
                structure_id,
                next_integrity);
        });

    for (const BrokenDeviceEntry& broken : broken_devices)
    {
        site_run->site_world->set_tile_device(
            broken.coord,
            gs1::SiteWorld::TileDeviceData {});
        emit_device_condition_changed_message(
            message_queue,
            broken.coord,
            gs1::StructureId {},
            0.0f);
        if (broken.device_entity_id != 0U)
        {
            emit_device_broken_message(
                message_queue,
                broken.device_entity_id,
                broken.coord,
                broken.structure_id);
        }
    }
}

}  // namespace

namespace gs1
{
const char* DeviceMaintenanceSystem::name() const noexcept
{
    return access().system_name.data();
}

GameMessageSubscriptionSpan DeviceMaintenanceSystem::subscribed_game_messages() const noexcept
{
    static constexpr GameMessageType subscriptions[] = {
        GameMessageType::SiteRunStarted,
        GameMessageType::SiteDevicePlaced,
        GameMessageType::SiteDeviceRepaired,
    };
    return subscriptions;
}

HostMessageSubscriptionSpan DeviceMaintenanceSystem::subscribed_host_messages() const noexcept
{
    return {};
}

std::optional<Gs1RuntimeProfileSystemId> DeviceMaintenanceSystem::profile_system_id() const noexcept
{
    return GS1_RUNTIME_PROFILE_SYSTEM_DEVICE_MAINTENANCE;
}

std::optional<std::uint32_t> DeviceMaintenanceSystem::fixed_step_order() const noexcept
{
    return 11U;
}

Gs1Status DeviceMaintenanceSystem::process_game_message(
    RuntimeInvocation& invocation,
    const GameMessage& message)
{
    auto access = make_game_state_access<DeviceMaintenanceSystem>(invocation);
    auto& site_run = access.template read<RuntimeActiveSiteRunTag>();
    if (!site_run.has_value())
    {
        return GS1_STATUS_INVALID_STATE;
    }

    return process_message(invocation, message);
}

Gs1Status DeviceMaintenanceSystem::process_host_message(
    RuntimeInvocation& invocation,
    const Gs1HostMessage& message)
{
    (void)message;
    (void)invocation;
    return GS1_STATUS_OK;
}

void DeviceMaintenanceSystem::run(RuntimeInvocation& invocation)
{
    auto access = make_game_state_access<DeviceMaintenanceSystem>(invocation);
    auto& site_run = access.template read<RuntimeActiveSiteRunTag>();
    if (!site_run.has_value())
    {
        return;
    }

    run_system(invocation);
}
}  // namespace gs1

#ifdef _MSC_VER
#pragma warning(pop)
#endif

