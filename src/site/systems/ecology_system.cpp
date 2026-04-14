#include "site/systems/ecology_system.h"

#include "site/site_projection_update_flags.h"
#include "site/site_run_state.h"
#include "site/site_world_access.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace gs1
{
namespace
{
constexpr float k_density_epsilon = 0.0001f;
constexpr float k_growth_rate_per_second = 0.2f;
constexpr float k_water_density_per_unit = 0.2f;

bool has_tile_occupant(PlantId plant_id, std::uint32_t ground_cover_type_id) noexcept
{
    return plant_id.value != 0U || ground_cover_type_id != 0U;
}

void emit_tile_ecology_changed(
    SiteSystemContext<EcologySystem>& context,
    TileCoord coord,
    std::uint32_t changed_mask)
{
    if (changed_mask == TILE_ECOLOGY_CHANGED_NONE)
    {
        return;
    }

    if (!site_world_access::tile_coord_in_bounds(context.site_run, coord))
    {
        return;
    }

    TileEcologyChangedCommand payload {
        coord.x,
        coord.y,
        changed_mask,
        0U,
        0U,
        0.0f,
        0.0f};
    const bool has_tile = site_world_access::ecology::read_tile(
        context.site_run,
        coord,
        [&](const site_ecs::TileSandBurial& sand_burial,
            const site_ecs::TilePlantSlot& plant,
            const site_ecs::TileGroundCoverSlot& ground_cover,
            const site_ecs::TilePlantDensity& density) {
            payload.plant_type_id = plant.plant_id.value;
            payload.ground_cover_type_id = ground_cover.ground_cover_type_id;
            payload.plant_density = density.value;
            payload.sand_burial = sand_burial.value;
        });
    if (!has_tile)
    {
        return;
    }

    GameCommand command {};
    command.type = GameCommandType::TileEcologyChanged;
    command.set_payload(payload);
    context.command_queue.push_back(command);
    context.world.mark_tile_projection_dirty(coord);
}

std::uint32_t apply_ground_cover(
    SiteRunState& site_run,
    TileCoord coord,
    const SiteGroundCoverPlacedCommand& payload)
{
    if (!site_world_access::tile_coord_in_bounds(site_run, coord))
    {
        return TILE_ECOLOGY_CHANGED_NONE;
    }

    std::uint32_t changed_mask = TILE_ECOLOGY_CHANGED_NONE;
    site_world_access::ecology::with_tile_mut(
        site_run,
        coord,
        [&](site_ecs::TileSandBurial&,
            site_ecs::TilePlantSlot& plant,
            site_ecs::TileGroundCoverSlot& ground_cover,
            site_ecs::TilePlantDensity& density) {
            bool modified = false;
            bool occupancy_changed = false;

            if (ground_cover.ground_cover_type_id != payload.ground_cover_type_id)
            {
                ground_cover.ground_cover_type_id = payload.ground_cover_type_id;
                occupancy_changed = true;
                modified = true;
            }

            if (plant.plant_id.value != 0U)
            {
                plant.plant_id = PlantId {};
                occupancy_changed = true;
                modified = true;
            }

            const float target_density = std::clamp(payload.initial_density, 0.0f, 1.0f);
            if (std::fabs(density.value - target_density) > k_density_epsilon)
            {
                density.value = target_density;
                changed_mask |= TILE_ECOLOGY_CHANGED_DENSITY;
                modified = true;
            }

            if (occupancy_changed)
            {
                changed_mask |= TILE_ECOLOGY_CHANGED_OCCUPANCY;
            }

            return modified;
        });

    return changed_mask;
}

std::uint32_t apply_planting(
    SiteRunState& site_run,
    TileCoord coord,
    const SiteTilePlantingCompletedCommand& payload)
{
    if (!site_world_access::tile_coord_in_bounds(site_run, coord))
    {
        return TILE_ECOLOGY_CHANGED_NONE;
    }

    std::uint32_t changed_mask = TILE_ECOLOGY_CHANGED_NONE;
    site_world_access::ecology::with_tile_mut(
        site_run,
        coord,
        [&](site_ecs::TileSandBurial&,
            site_ecs::TilePlantSlot& plant,
            site_ecs::TileGroundCoverSlot& ground_cover,
            site_ecs::TilePlantDensity& density) {
            bool modified = false;
            bool occupancy_changed = false;

            const PlantId requested_plant {payload.plant_type_id};
            if (plant.plant_id.value != requested_plant.value)
            {
                plant.plant_id = requested_plant;
                occupancy_changed = true;
                modified = true;
            }

            if (ground_cover.ground_cover_type_id != 0U)
            {
                ground_cover.ground_cover_type_id = 0U;
                occupancy_changed = true;
                modified = true;
            }

            const float target_density = std::clamp(payload.initial_density, 0.0f, 1.0f);
            if (std::fabs(density.value - target_density) > k_density_epsilon)
            {
                density.value = target_density;
                changed_mask |= TILE_ECOLOGY_CHANGED_DENSITY;
                modified = true;
            }

            if (occupancy_changed)
            {
                changed_mask |= TILE_ECOLOGY_CHANGED_OCCUPANCY;
            }

            return modified;
        });

    return changed_mask;
}

std::uint32_t apply_watering(
    SiteRunState& site_run,
    TileCoord coord,
    const SiteTileWateredCommand& payload)
{
    if (!site_world_access::tile_coord_in_bounds(site_run, coord))
    {
        return TILE_ECOLOGY_CHANGED_NONE;
    }

    const float amount = std::max(0.0f, payload.water_amount);
    if (amount <= 0.0f)
    {
        return TILE_ECOLOGY_CHANGED_NONE;
    }

    std::uint32_t changed_mask = TILE_ECOLOGY_CHANGED_NONE;
    site_world_access::ecology::with_tile_mut(
        site_run,
        coord,
        [&](site_ecs::TileSandBurial&,
            site_ecs::TilePlantSlot& plant,
            site_ecs::TileGroundCoverSlot& ground_cover,
            site_ecs::TilePlantDensity& density) {
            if (!has_tile_occupant(plant.plant_id, ground_cover.ground_cover_type_id))
            {
                return false;
            }

            const float current_density = density.value;
            if (current_density >= 1.0f - k_density_epsilon)
            {
                return false;
            }

            const float next_density =
                std::min(1.0f, current_density + amount * k_water_density_per_unit);
            if (std::fabs(next_density - current_density) <= k_density_epsilon)
            {
                return false;
            }

            density.value = next_density;
            changed_mask = TILE_ECOLOGY_CHANGED_DENSITY;
            return true;
        });
    return changed_mask;
}

std::uint32_t apply_burial_cleared(
    SiteRunState& site_run,
    TileCoord coord,
    const SiteTileBurialClearedCommand& payload)
{
    if (!site_world_access::tile_coord_in_bounds(site_run, coord))
    {
        return TILE_ECOLOGY_CHANGED_NONE;
    }

    const float reduction = std::max(0.0f, payload.cleared_amount);
    std::uint32_t changed_mask = TILE_ECOLOGY_CHANGED_NONE;
    site_world_access::ecology::with_tile_mut(
        site_run,
        coord,
        [&](site_ecs::TileSandBurial& sand_burial,
            site_ecs::TilePlantSlot&,
            site_ecs::TileGroundCoverSlot&,
            site_ecs::TilePlantDensity&) {
            const float next_burial = std::max(0.0f, sand_burial.value - reduction);
            if (std::fabs(sand_burial.value - next_burial) <= k_density_epsilon)
            {
                return false;
            }

            sand_burial.value = next_burial;
            changed_mask = TILE_ECOLOGY_CHANGED_SAND_BURIAL;
            return true;
        });
    return changed_mask;
}

void update_restoration_progress(
    SiteSystemContext<EcologySystem>& context,
    std::uint32_t new_count)
{
    auto& counters = context.world.own_counters();
    const auto previous_count = counters.fully_grown_tile_count;
    if (previous_count == new_count)
    {
        return;
    }

    counters.fully_grown_tile_count = new_count;
    float normalized_progress = 0.0f;
    if (counters.site_completion_tile_threshold > 0U)
    {
        normalized_progress = static_cast<float>(new_count) /
            static_cast<float>(counters.site_completion_tile_threshold);
        normalized_progress = std::clamp(normalized_progress, 0.0f, 1.0f);
    }

    GameCommand progress_command {};
    progress_command.type = GameCommandType::RestorationProgressChanged;
    progress_command.set_payload(RestorationProgressChangedCommand {
        new_count,
        counters.site_completion_tile_threshold,
        normalized_progress});
    context.command_queue.push_back(progress_command);
    context.world.mark_projection_dirty(SITE_PROJECTION_UPDATE_HUD);
}
}  // namespace

bool EcologySystem::subscribes_to(GameCommandType type) noexcept
{
    switch (type)
    {
    case GameCommandType::SiteGroundCoverPlaced:
    case GameCommandType::SiteTilePlantingCompleted:
    case GameCommandType::SiteTileWatered:
    case GameCommandType::SiteTileBurialCleared:
        return true;
    default:
        return false;
    }
}

Gs1Status EcologySystem::process_command(
    SiteSystemContext<EcologySystem>& context,
    const GameCommand& command)
{
    switch (command.type)
    {
    case GameCommandType::SiteGroundCoverPlaced:
    {
        const auto& payload = command.payload_as<SiteGroundCoverPlacedCommand>();
        const TileCoord coord {payload.target_tile_x, payload.target_tile_y};
        emit_tile_ecology_changed(
            context,
            coord,
            apply_ground_cover(context.site_run, coord, payload));
        break;
    }

    case GameCommandType::SiteTilePlantingCompleted:
    {
        const auto& payload = command.payload_as<SiteTilePlantingCompletedCommand>();
        const TileCoord coord {payload.target_tile_x, payload.target_tile_y};
        emit_tile_ecology_changed(
            context,
            coord,
            apply_planting(context.site_run, coord, payload));
        break;
    }

    case GameCommandType::SiteTileWatered:
    {
        const auto& payload = command.payload_as<SiteTileWateredCommand>();
        const TileCoord coord {payload.target_tile_x, payload.target_tile_y};
        emit_tile_ecology_changed(
            context,
            coord,
            apply_watering(context.site_run, coord, payload));
        break;
    }

    case GameCommandType::SiteTileBurialCleared:
    {
        const auto& payload = command.payload_as<SiteTileBurialClearedCommand>();
        const TileCoord coord {payload.target_tile_x, payload.target_tile_y};
        emit_tile_ecology_changed(
            context,
            coord,
            apply_burial_cleared(context.site_run, coord, payload));
        break;
    }

    default:
        break;
    }

    return GS1_STATUS_OK;
}

void EcologySystem::run(SiteSystemContext<EcologySystem>& context)
{
    auto& site_run = context.site_run;
    if (!site_world_access::has_world(site_run))
    {
        return;
    }

    const float growth_delta =
        static_cast<float>(context.fixed_step_seconds) * k_growth_rate_per_second;
    if (growth_delta <= 0.0f)
    {
        return;
    }

    std::uint32_t fully_grown_count = 0U;
    site_world_access::ecology::for_each_occupied_tile_mut(
        site_run,
        [&](TileCoord coord,
            site_ecs::TilePlantSlot&,
            site_ecs::TileGroundCoverSlot&,
            site_ecs::TilePlantDensity& density) {
            bool modified = false;
            if (density.value < 1.0f - k_density_epsilon)
            {
                const float next_density = std::min(1.0f, density.value + growth_delta);
                if (std::fabs(next_density - density.value) > k_density_epsilon)
                {
                    density.value = next_density;
                    modified = true;
                    emit_tile_ecology_changed(
                        context,
                        coord,
                        TILE_ECOLOGY_CHANGED_DENSITY);
                }
            }

            if (density.value >= 1.0f - k_density_epsilon)
            {
                ++fully_grown_count;
            }

            return modified;
        });

    update_restoration_progress(context, fully_grown_count);
}
}  // namespace gs1
