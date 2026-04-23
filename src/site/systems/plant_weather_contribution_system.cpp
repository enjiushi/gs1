#include "site/systems/plant_weather_contribution_system.h"

#include "content/defs/plant_defs.h"
#include "site/site_world_components.h"
#include "site/weather_contribution_logic.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4127)
#endif
#include <flecs.h>

#include <algorithm>

namespace
{
const gs1::PlantDef& resolve_occupant_def(
    gs1::PlantId plant_id,
    std::uint32_t ground_cover_type_id) noexcept
{
    if (plant_id.value != 0U)
    {
        const auto* plant_def = gs1::find_plant_def(plant_id);
        if (plant_def != nullptr)
        {
            return *plant_def;
        }

        return gs1::k_generic_living_plant_def;
    }

    (void)ground_cover_type_id;
    return gs1::k_generic_ground_cover_def;
}

void clear_tile_contributions(gs1::SiteSystemContext<gs1::PlantWeatherContributionSystem>& context)
{
    const auto tile_count = context.world.tile_count();
    for (std::size_t index = 0U; index < tile_count; ++index)
    {
        context.world.write_tile_plant_weather_contribution_at_index(
            index,
            gs1::zero_weather_contribution());
    }
}

void add_contribution(
    gs1::SiteSystemContext<gs1::PlantWeatherContributionSystem>& context,
    gs1::TileCoord target_coord,
    const gs1::SiteWorld::TileWeatherContributionData& delta)
{
    auto contribution = context.world.read_tile_plant_weather_contribution(target_coord);
    gs1::accumulate_weather_contribution(contribution, delta);
    context.world.write_tile_plant_weather_contribution(target_coord, contribution);
}
}  // namespace

namespace gs1
{
bool PlantWeatherContributionSystem::subscribes_to(GameMessageType type) noexcept
{
    (void)type;
    return false;
}

Gs1Status PlantWeatherContributionSystem::process_message(
    SiteSystemContext<PlantWeatherContributionSystem>& context,
    const GameMessage& message)
{
    (void)context;
    (void)message;
    return GS1_STATUS_OK;
}

void PlantWeatherContributionSystem::run(SiteSystemContext<PlantWeatherContributionSystem>& context)
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
            const site_ecs::TilePlantSlot,
            const site_ecs::TileGroundCoverSlot,
            const site_ecs::TilePlantDensity>()
            .with<site_ecs::TileOccupantTag>()
            .build();

    source_query.each(
        [&](flecs::entity,
            const site_ecs::TileCoordComponent& coord_component,
            const site_ecs::TilePlantSlot& plant_slot,
            const site_ecs::TileGroundCoverSlot& ground_cover_slot,
            const site_ecs::TilePlantDensity& density_component) {
            const float density =
                std::clamp(density_component.value * k_inverse_meter_scale, 0.0f, 1.0f);
            if (density <= k_weather_contribution_epsilon)
            {
                return;
            }

            const auto& plant_def =
                resolve_occupant_def(plant_slot.plant_id, ground_cover_slot.ground_cover_type_id);
            const TileCoord source_coord = coord_component.value;

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
                    sample.manhattan_distance <= static_cast<int>(plant_def.aura_size);
                if (within_shared_aura)
                {
                    delta.heat_protection =
                        plant_def.heat_protection_power * density * contribution_scale;
                    delta.dust_protection =
                        plant_def.dust_protection_power * density * contribution_scale;
                    delta.fertility_improve =
                        plant_def.fertility_improve_power * density * contribution_scale;
                    delta.salinity_reduction =
                        plant_def.salinity_reduction_power * density * contribution_scale;
                }

                if (sample.manhattan_distance == 0)
                {
                    delta.wind_protection =
                        plant_def.wind_protection_power * density * contribution_scale;
                }
                else if (
                    sample.manhattan_distance <= static_cast<int>(plant_def.wind_protection_range))
                {
                    const float shadow_scale = compute_directional_wind_shadow_scale(
                        source_coord.x,
                        source_coord.y,
                        target_coord.x,
                        target_coord.y,
                        plant_def.wind_protection_range,
                        wind_direction);
                    if (shadow_scale > k_weather_contribution_epsilon)
                    {
                        delta.wind_protection =
                            plant_def.wind_protection_power *
                            density *
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
