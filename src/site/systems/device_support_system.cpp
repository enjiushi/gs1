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

bool has_valid_device_tile_data(const gs1::TileGridState& tile_grid) noexcept
{
    const auto tile_count = tile_grid.tile_count();
    return tile_count > 0 &&
        tile_grid.width > 0 &&
        tile_grid.height > 0 &&
        tile_grid.structure_type_ids.size() >= tile_count &&
        tile_grid.device_integrity.size() >= tile_count &&
        tile_grid.device_efficiency.size() >= tile_count &&
        tile_grid.device_stored_water.size() >= tile_count &&
        tile_grid.tile_heat.size() >= tile_count;
}

}  // namespace

namespace gs1
{
bool DeviceSupportSystem::subscribes_to(GameCommandType type) noexcept
{
    (void)type;
    return false;
}

Gs1Status DeviceSupportSystem::process_command(
    SiteSystemContext<DeviceSupportSystem>& context,
    const GameCommand& command)
{
    (void)context;
    (void)command;
    return GS1_STATUS_OK;
}

void DeviceSupportSystem::run(SiteSystemContext<DeviceSupportSystem>& context)
{
    auto& tile_grid = context.world.own_tile_grid();
    if (!has_valid_device_tile_data(tile_grid))
    {
        return;
    }

    const auto tile_count = tile_grid.tile_count();
    if (tile_count == 0U)
    {
        return;
    }

    const float step_seconds = static_cast<float>(context.fixed_step_seconds);
    if (step_seconds <= 0.0f)
    {
        return;
    }

    for (std::size_t index = 0; index < tile_count; ++index)
    {
        const bool has_structure = tile_grid.structure_type_ids[index].value != 0U;

        const float clamped_integrity = std::clamp(tile_grid.device_integrity[index], 0.0f, 1.0f);
        const float target_efficiency = has_structure ? clamped_integrity : 0.0f;
        const float previous_efficiency = tile_grid.device_efficiency[index];
        if (std::fabs(previous_efficiency - target_efficiency) > k_device_efficiency_epsilon)
        {
            tile_grid.device_efficiency[index] = target_efficiency;
        }

        const float previous_water = tile_grid.device_stored_water[index];
        float next_water = previous_water;
        if (has_structure)
        {
            const float heat = std::max(tile_grid.tile_heat[index], 0.0f);
            const float evaporation_rate =
                (k_device_water_evaporation_base + heat * k_device_heat_evaporation_multiplier) * step_seconds;
            next_water = std::max(0.0f, previous_water - evaporation_rate);
        }
        else
        {
            next_water = 0.0f;
        }

        if (std::fabs(previous_water - next_water) > k_device_water_epsilon)
        {
            tile_grid.device_stored_water[index] = next_water;
        }
    }
}
}  // namespace gs1
