#include "site/systems/device_maintenance_system.h"

#include "site/site_run_state.h"
#include "site/site_world_access.h"

#include <algorithm>
#include <cmath>
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
    (void)type;
    return false;
}

Gs1Status DeviceMaintenanceSystem::process_command(
    SiteSystemContext<DeviceMaintenanceSystem>& context,
    const GameCommand& command)
{
    (void)context;
    (void)command;
    return GS1_STATUS_OK;
}

void DeviceMaintenanceSystem::run(SiteSystemContext<DeviceMaintenanceSystem>& context)
{
    auto& site_run = context.site_run;

    if (!site_world_access::has_world(site_run))
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

    site_world_access::device_maintenance::for_each_device_mut(
        site_run,
        [&](TileCoord,
            const site_ecs::DeviceStructureId& structure,
            const site_ecs::TileSandBurial& burial,
            site_ecs::DeviceIntegrity& integrity) {
            if (structure.structure_id.value == 0U)
            {
                return false;
            }

            const float burial_amount = std::clamp(burial.value, 0.0f, 1.0f);
            const float burial_wear = burial_amount * k_burial_wear_per_unit * step_seconds;
            const float total_wear = weather_wear + burial_wear;
            if (total_wear <= 0.0f)
            {
                return false;
            }

            const float next_integrity = std::clamp(integrity.value - total_wear, 0.0f, 1.0f);
            if (std::fabs(next_integrity - integrity.value) <= k_integrity_epsilon)
            {
                return false;
            }

            integrity.value = next_integrity;
            return true;
        });
}
}  // namespace gs1
