#include "site/systems/device_weather_contribution_system.h"

#include "content/defs/structure_defs.h"
#include "site/site_world_components.h"
#include "site/tile_footprint.h"
#include "site/weather_contribution_logic.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4127)
#endif
#include <flecs.h>

#include <algorithm>

namespace
{
[[nodiscard]] bool device_tile_is_support_anchor(
    gs1::TileCoord coord,
    gs1::StructureId structure_id) noexcept
{
    const auto anchor =
        gs1::align_tile_anchor_to_footprint(coord, gs1::resolve_structure_tile_footprint(structure_id));
    return anchor.x == coord.x && anchor.y == coord.y;
}

void clear_tile_contributions(gs1::SiteSystemContext<gs1::DeviceWeatherContributionSystem>& context)
{
    const auto tile_count = context.world.tile_count();
    for (std::size_t index = 0U; index < tile_count; ++index)
    {
        context.world.write_tile_device_weather_contribution_at_index(
            index,
            gs1::zero_weather_contribution());
    }
}

void add_contribution(
    gs1::SiteSystemContext<gs1::DeviceWeatherContributionSystem>& context,
    gs1::TileCoord target_coord,
    const gs1::SiteWorld::TileWeatherContributionData& delta)
{
    auto contribution = context.world.read_tile_device_weather_contribution(target_coord);
    gs1::accumulate_weather_contribution(contribution, delta);
    context.world.write_tile_device_weather_contribution(target_coord, contribution);
}
}  // namespace

namespace gs1
{
bool DeviceWeatherContributionSystem::subscribes_to(GameMessageType type) noexcept
{
    (void)type;
    return false;
}

Gs1Status DeviceWeatherContributionSystem::process_message(
    SiteSystemContext<DeviceWeatherContributionSystem>& context,
    const GameMessage& message)
{
    (void)context;
    (void)message;
    return GS1_STATUS_OK;
}

void DeviceWeatherContributionSystem::run(SiteSystemContext<DeviceWeatherContributionSystem>& context)
{
    if (!context.world.has_world())
    {
        return;
    }

    clear_tile_contributions(context);

    auto& ecs_world = context.site_run.site_world->ecs_world();
    const WeatherUnitVector wind_direction =
        resolve_wind_direction_unit_vector(context.world.read_weather().weather_wind_direction_degrees);

    auto source_query =
        ecs_world.query_builder<
            const site_ecs::TileCoordComponent,
            const site_ecs::DeviceStructureId,
            const site_ecs::DeviceEfficiency>()
            .with<site_ecs::DeviceTag>()
            .build();

    source_query.each(
        [&](flecs::entity,
            const site_ecs::TileCoordComponent& coord_component,
            const site_ecs::DeviceStructureId& structure_component,
            const site_ecs::DeviceEfficiency& efficiency_component) {
            const StructureId structure_id = structure_component.structure_id;
            if (structure_id.value == 0U)
            {
                return;
            }

            const TileCoord source_coord = coord_component.value;
            if (!device_tile_is_support_anchor(source_coord, structure_id))
            {
                return;
            }

            const auto* structure_def = find_structure_def(structure_id);
            if (structure_def == nullptr)
            {
                return;
            }

            const float efficiency = std::clamp(efficiency_component.value, 0.0f, 1.0f);
            if (efficiency <= k_weather_contribution_epsilon)
            {
                return;
            }

            for (const auto sample : k_weather_contribution_samples)
            {
                const TileCoord target_coord {source_coord.x + sample.dx, source_coord.y + sample.dy};
                if (!context.world.tile_coord_in_bounds(target_coord))
                {
                    continue;
                }

                SiteWorld::TileWeatherContributionData delta = zero_weather_contribution();
                const float contribution_scale = resolve_contribution_scale(sample.manhattan_distance);
                const bool within_shared_aura =
                    sample.manhattan_distance == 0 ||
                    sample.manhattan_distance <= static_cast<int>(structure_def->aura_size);
                if (within_shared_aura)
                {
                    delta.heat_protection =
                        structure_def->heat_protection_value * efficiency * contribution_scale;
                    delta.dust_protection =
                        structure_def->dust_protection_value * efficiency * contribution_scale;
                    delta.fertility_improve =
                        structure_def->fertility_improve_value * efficiency * contribution_scale;
                    delta.salinity_reduction =
                        structure_def->salinity_reduction_value * efficiency * contribution_scale;
                    delta.irrigation =
                        structure_def->irrigation_value * efficiency * contribution_scale;
                }

                if (sample.manhattan_distance == 0)
                {
                    delta.wind_protection =
                        structure_def->wind_protection_value * efficiency * contribution_scale;
                }
                else if (
                    sample.manhattan_distance <=
                    static_cast<int>(structure_def->wind_protection_range))
                {
                    const float shadow_scale = compute_directional_wind_shadow_scale(
                        source_coord.x,
                        source_coord.y,
                        target_coord.x,
                        target_coord.y,
                        structure_def->wind_protection_range,
                        wind_direction);
                    if (shadow_scale > k_weather_contribution_epsilon)
                    {
                        delta.wind_protection =
                            structure_def->wind_protection_value *
                            efficiency *
                            contribution_scale *
                            shadow_scale;
                    }
                }

                add_contribution(context, target_coord, delta);
            }
        });
}
}  // namespace gs1

#ifdef _MSC_VER
#pragma warning(pop)
#endif
