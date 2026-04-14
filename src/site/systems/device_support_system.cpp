#include "site/systems/device_support_system.h"

#include "site/site_run_state.h"
#include "site/site_world_access.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace
{
constexpr float k_device_efficiency_epsilon = 0.0005f;
constexpr float k_device_water_epsilon = 0.0005f;
constexpr float k_device_water_evaporation_base = 0.0035f;
constexpr float k_device_heat_evaporation_multiplier = 0.0075f;
}  // namespace

namespace gs1
{
bool DeviceSupportSystem::subscribes_to(GameCommandType type) noexcept
{
    (void)type;
    return false;
}

Gs1Status DeviceSupportSystem::process_command(
    SiteSystemContext& context,
    const GameCommand& command)
{
    (void)context;
    (void)command;
    return GS1_STATUS_OK;
}

void DeviceSupportSystem::run(SiteSystemContext& context)
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

    site_world_access::device_support::for_each_device_mut(
        site_run,
        [&](TileCoord,
            const site_ecs::DeviceStructureId& structure,
            const site_ecs::DeviceIntegrity& integrity,
            const site_ecs::TileHeat& heat,
            site_ecs::DeviceEfficiency& efficiency,
            site_ecs::DeviceStoredWater& stored_water) {
            const bool has_structure = structure.structure_id.value != 0U;
            const float clamped_integrity = std::clamp(integrity.value, 0.0f, 1.0f);
            const float target_efficiency = has_structure ? clamped_integrity : 0.0f;
            const float previous_efficiency = efficiency.value;
            const bool efficiency_changed =
                std::fabs(previous_efficiency - target_efficiency) > k_device_efficiency_epsilon;
            if (efficiency_changed)
            {
                efficiency.value = target_efficiency;
            }

            const float previous_water = stored_water.value;
            float next_water = previous_water;
            if (has_structure)
            {
                const float clamped_heat = std::max(heat.value, 0.0f);
                const float evaporation_rate =
                    (k_device_water_evaporation_base +
                        clamped_heat * k_device_heat_evaporation_multiplier) *
                    step_seconds;
                next_water = std::max(0.0f, previous_water - evaporation_rate);
            }
            else
            {
                next_water = 0.0f;
            }

            const bool water_changed = std::fabs(previous_water - next_water) > k_device_water_epsilon;
            if (water_changed)
            {
                stored_water.value = next_water;
            }

            return efficiency_changed || water_changed;
        });
}
}  // namespace gs1
