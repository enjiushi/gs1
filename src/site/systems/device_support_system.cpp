#include "site/systems/device_support_system.h"

#include "content/defs/gameplay_tuning_defs.h"
#include "runtime/game_runtime.h"
#include "site/site_run_state.h"
#include "site/site_world_components.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4127)
#endif
#include <flecs.h>

#include <algorithm>
#include <cmath>

namespace
{
constexpr float k_device_efficiency_epsilon = 0.0005f;
constexpr float k_device_water_epsilon = 0.0005f;
}  // namespace

namespace gs1
{
const char* DeviceSupportSystem::name() const noexcept
{
    return access().system_name.data();
}

GameMessageSubscriptionSpan DeviceSupportSystem::subscribed_game_messages() const noexcept
{
    return runtime_subscription_list<
        GameMessageType,
        k_game_message_type_count,
        &DeviceSupportSystem::subscribes_to>();
}

HostMessageSubscriptionSpan DeviceSupportSystem::subscribed_host_messages() const noexcept
{
    return {};
}

std::optional<Gs1RuntimeProfileSystemId> DeviceSupportSystem::profile_system_id() const noexcept
{
    return GS1_RUNTIME_PROFILE_SYSTEM_DEVICE_SUPPORT;
}

std::optional<std::uint32_t> DeviceSupportSystem::fixed_step_order() const noexcept
{
    return 12U;
}

bool DeviceSupportSystem::subscribes_to(GameMessageType type) noexcept
{
    (void)type;
    return false;
}

Gs1Status DeviceSupportSystem::process_message(
    SiteSystemContext<DeviceSupportSystem>& context,
    const GameMessage& message)
{
    (void)context;
    (void)message;
    return GS1_STATUS_OK;
}

Gs1Status DeviceSupportSystem::process_game_message(
    RuntimeInvocation& invocation,
    const GameMessage& message)
{
    auto access = make_game_state_access<DeviceSupportSystem>(invocation);
    (void)access;
    return with_site_system_context<DeviceSupportSystem>(
        invocation,
        [&](SiteSystemContext<DeviceSupportSystem>& context) -> Gs1Status
        {
            return process_message(context, message);
        });
}

Gs1Status DeviceSupportSystem::process_host_message(
    RuntimeInvocation& invocation,
    const Gs1HostMessage& message)
{
    auto access = make_game_state_access<DeviceSupportSystem>(invocation);
    (void)access;
    (void)message;
    return GS1_STATUS_OK;
}

void DeviceSupportSystem::run(SiteSystemContext<DeviceSupportSystem>& context)
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

    const auto& tuning = gameplay_tuning_def().device_support;
    auto& ecs_world = context.site_run.site_world->ecs_world();
    auto device_query =
        ecs_world.query_builder<
            const site_ecs::TileCoordComponent,
            const site_ecs::DeviceStructureId,
            const site_ecs::DeviceIntegrity,
            site_ecs::DeviceEfficiency,
            site_ecs::DeviceStoredWater>()
            .with<site_ecs::DeviceTag>()
            .build();

    device_query.each(
        [&](flecs::entity,
            const site_ecs::TileCoordComponent& coord_component,
            const site_ecs::DeviceStructureId& structure_component,
            const site_ecs::DeviceIntegrity& integrity_component,
            site_ecs::DeviceEfficiency& efficiency_component,
            site_ecs::DeviceStoredWater& water_component) {
            if (structure_component.structure_id.value == 0U)
            {
                return;
            }

            const float clamped_integrity = std::clamp(integrity_component.value, 0.0f, 1.0f);
            const float target_efficiency = clamped_integrity;
            const float previous_efficiency = efficiency_component.value;
            const bool efficiency_changed =
                std::fabs(previous_efficiency - target_efficiency) > k_device_efficiency_epsilon;
            if (efficiency_changed)
            {
                efficiency_component.value = target_efficiency;
            }

            const auto tile_entity_id =
                context.site_run.site_world->tile_entity_id(coord_component.value);
            const float tile_heat = tile_entity_id == 0U
                ? 0.0f
                : ecs_world.entity(tile_entity_id).get<site_ecs::TileHeat>().value;
            const float previous_water = water_component.value;
            const float clamped_heat = std::max(tile_heat, 0.0f);
            const float evaporation_rate =
                (tuning.water_evaporation_base + clamped_heat * tuning.heat_evaporation_multiplier) *
                step_seconds;
            const float next_water = std::max(0.0f, previous_water - evaporation_rate);
            const bool water_changed =
                std::fabs(previous_water - next_water) > k_device_water_epsilon;
            if (water_changed)
            {
                water_component.value = next_water;
            }
        });
}

void DeviceSupportSystem::run(RuntimeInvocation& invocation)
{
    auto access = make_game_state_access<DeviceSupportSystem>(invocation);
    (void)access;
    (void)with_site_system_context<DeviceSupportSystem>(
        invocation,
        [&](SiteSystemContext<DeviceSupportSystem>& context) -> Gs1Status
        {
            run(context);
            return GS1_STATUS_OK;
        });
}
}  // namespace gs1

#ifdef _MSC_VER
#pragma warning(pop)
#endif
