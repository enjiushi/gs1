#include "site/systems/device_support_system.h"

#include "site/site_run_state.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
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

    const auto tile_count = context.world.tile_count();
    for (std::size_t index = 0; index < tile_count; ++index)
    {
        auto tile = context.world.read_tile_at_index(index);
        if (tile.device.structure_id.value == 0U)
        {
            continue;
        }

        const float clamped_integrity = std::clamp(tile.device.device_integrity, 0.0f, 1.0f);
        const float target_efficiency = clamped_integrity;
        const float previous_efficiency = tile.device.device_efficiency;
        const bool efficiency_changed =
            std::fabs(previous_efficiency - target_efficiency) > k_device_efficiency_epsilon;
        if (efficiency_changed)
        {
            tile.device.device_efficiency = target_efficiency;
        }

        const float previous_water = tile.device.device_stored_water;
        const float clamped_heat = std::max(tile.local_weather.heat, 0.0f);
        const float evaporation_rate =
            (k_device_water_evaporation_base + clamped_heat * k_device_heat_evaporation_multiplier) *
            step_seconds;
        const float next_water = std::max(0.0f, previous_water - evaporation_rate);
        const bool water_changed = std::fabs(previous_water - next_water) > k_device_water_epsilon;
        if (water_changed)
        {
            tile.device.device_stored_water = next_water;
        }

        if (efficiency_changed || water_changed)
        {
            context.world.write_tile_at_index(index, tile);
        }
    }
}
}  // namespace gs1
