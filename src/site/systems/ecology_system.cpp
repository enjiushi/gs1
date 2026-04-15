#include "site/systems/ecology_system.h"

#include "site/site_projection_update_flags.h"
#include "site/site_run_state.h"

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

    if (!context.world.tile_coord_in_bounds(coord))
    {
        return;
    }

    const auto tile = context.world.read_tile(coord);
    GameCommand command {};
    command.type = GameCommandType::TileEcologyChanged;
    command.set_payload(TileEcologyChangedCommand {
        coord.x,
        coord.y,
        changed_mask,
        tile.ecology.plant_id.value,
        tile.ecology.ground_cover_type_id,
        tile.ecology.plant_density,
        tile.ecology.sand_burial});
    context.command_queue.push_back(command);
    context.world.mark_tile_projection_dirty(coord);
}

std::uint32_t apply_ground_cover(
    SiteWorldAccess<EcologySystem>& world,
    TileCoord coord,
    const SiteGroundCoverPlacedCommand& payload)
{
    if (!world.tile_coord_in_bounds(coord))
    {
        return TILE_ECOLOGY_CHANGED_NONE;
    }

    auto tile = world.read_tile(coord);
    std::uint32_t changed_mask = TILE_ECOLOGY_CHANGED_NONE;
    bool modified = false;
    bool occupancy_changed = false;

    if (tile.ecology.ground_cover_type_id != payload.ground_cover_type_id)
    {
        tile.ecology.ground_cover_type_id = payload.ground_cover_type_id;
        occupancy_changed = true;
        modified = true;
    }

    if (tile.ecology.plant_id.value != 0U)
    {
        tile.ecology.plant_id = PlantId {};
        occupancy_changed = true;
        modified = true;
    }

    const float target_density = std::clamp(payload.initial_density, 0.0f, 1.0f);
    if (std::fabs(tile.ecology.plant_density - target_density) > k_density_epsilon)
    {
        tile.ecology.plant_density = target_density;
        changed_mask |= TILE_ECOLOGY_CHANGED_DENSITY;
        modified = true;
    }

    if (occupancy_changed)
    {
        changed_mask |= TILE_ECOLOGY_CHANGED_OCCUPANCY;
    }

    if (modified)
    {
        world.write_tile(coord, tile);
    }

    return changed_mask;
}

std::uint32_t apply_planting(
    SiteWorldAccess<EcologySystem>& world,
    TileCoord coord,
    const SiteTilePlantingCompletedCommand& payload)
{
    if (!world.tile_coord_in_bounds(coord))
    {
        return TILE_ECOLOGY_CHANGED_NONE;
    }

    auto tile = world.read_tile(coord);
    std::uint32_t changed_mask = TILE_ECOLOGY_CHANGED_NONE;
    bool modified = false;
    bool occupancy_changed = false;

    const PlantId requested_plant {payload.plant_type_id};
    if (tile.ecology.plant_id.value != requested_plant.value)
    {
        tile.ecology.plant_id = requested_plant;
        occupancy_changed = true;
        modified = true;
    }

    if (tile.ecology.ground_cover_type_id != 0U)
    {
        tile.ecology.ground_cover_type_id = 0U;
        occupancy_changed = true;
        modified = true;
    }

    const float target_density = std::clamp(payload.initial_density, 0.0f, 1.0f);
    if (std::fabs(tile.ecology.plant_density - target_density) > k_density_epsilon)
    {
        tile.ecology.plant_density = target_density;
        changed_mask |= TILE_ECOLOGY_CHANGED_DENSITY;
        modified = true;
    }

    if (occupancy_changed)
    {
        changed_mask |= TILE_ECOLOGY_CHANGED_OCCUPANCY;
    }

    if (modified)
    {
        world.write_tile(coord, tile);
    }

    return changed_mask;
}

std::uint32_t apply_watering(
    SiteWorldAccess<EcologySystem>& world,
    TileCoord coord,
    const SiteTileWateredCommand& payload)
{
    if (!world.tile_coord_in_bounds(coord))
    {
        return TILE_ECOLOGY_CHANGED_NONE;
    }

    const float amount = std::max(0.0f, payload.water_amount);
    if (amount <= 0.0f)
    {
        return TILE_ECOLOGY_CHANGED_NONE;
    }

    auto tile = world.read_tile(coord);
    if (!has_tile_occupant(tile.ecology.plant_id, tile.ecology.ground_cover_type_id))
    {
        return TILE_ECOLOGY_CHANGED_NONE;
    }

    const float current_density = tile.ecology.plant_density;
    if (current_density >= 1.0f - k_density_epsilon)
    {
        return TILE_ECOLOGY_CHANGED_NONE;
    }

    const float next_density =
        std::min(1.0f, current_density + amount * k_water_density_per_unit);
    if (std::fabs(next_density - current_density) <= k_density_epsilon)
    {
        return TILE_ECOLOGY_CHANGED_NONE;
    }

    tile.ecology.plant_density = next_density;
    world.write_tile(coord, tile);
    return TILE_ECOLOGY_CHANGED_DENSITY;
}

std::uint32_t apply_burial_cleared(
    SiteWorldAccess<EcologySystem>& world,
    TileCoord coord,
    const SiteTileBurialClearedCommand& payload)
{
    if (!world.tile_coord_in_bounds(coord))
    {
        return TILE_ECOLOGY_CHANGED_NONE;
    }

    auto tile = world.read_tile(coord);
    const float reduction = std::max(0.0f, payload.cleared_amount);
    const float next_burial = std::max(0.0f, tile.ecology.sand_burial - reduction);
    if (std::fabs(tile.ecology.sand_burial - next_burial) <= k_density_epsilon)
    {
        return TILE_ECOLOGY_CHANGED_NONE;
    }

    tile.ecology.sand_burial = next_burial;
    world.write_tile(coord, tile);
    return TILE_ECOLOGY_CHANGED_SAND_BURIAL;
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
            apply_ground_cover(context.world, coord, payload));
        break;
    }

    case GameCommandType::SiteTilePlantingCompleted:
    {
        const auto& payload = command.payload_as<SiteTilePlantingCompletedCommand>();
        const TileCoord coord {payload.target_tile_x, payload.target_tile_y};
        emit_tile_ecology_changed(
            context,
            coord,
            apply_planting(context.world, coord, payload));
        break;
    }

    case GameCommandType::SiteTileWatered:
    {
        const auto& payload = command.payload_as<SiteTileWateredCommand>();
        const TileCoord coord {payload.target_tile_x, payload.target_tile_y};
        emit_tile_ecology_changed(
            context,
            coord,
            apply_watering(context.world, coord, payload));
        break;
    }

    case GameCommandType::SiteTileBurialCleared:
    {
        const auto& payload = command.payload_as<SiteTileBurialClearedCommand>();
        const TileCoord coord {payload.target_tile_x, payload.target_tile_y};
        emit_tile_ecology_changed(
            context,
            coord,
            apply_burial_cleared(context.world, coord, payload));
        break;
    }

    default:
        break;
    }

    return GS1_STATUS_OK;
}

void EcologySystem::run(SiteSystemContext<EcologySystem>& context)
{
    if (!context.world.has_world())
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
    const auto tile_count = context.world.tile_count();
    for (std::size_t index = 0; index < tile_count; ++index)
    {
        auto tile = context.world.read_tile_at_index(index);
        if (!has_tile_occupant(tile.ecology.plant_id, tile.ecology.ground_cover_type_id))
        {
            continue;
        }

        bool modified = false;
        if (tile.ecology.plant_density < 1.0f - k_density_epsilon)
        {
            const float next_density = std::min(1.0f, tile.ecology.plant_density + growth_delta);
            if (std::fabs(next_density - tile.ecology.plant_density) > k_density_epsilon)
            {
                tile.ecology.plant_density = next_density;
                modified = true;
            }
        }

        if (modified)
        {
            const auto coord = context.world.tile_coord(index);
            context.world.write_tile_at_index(index, tile);
            emit_tile_ecology_changed(
                context,
                coord,
                TILE_ECOLOGY_CHANGED_DENSITY);
        }

        if (tile.ecology.plant_density >= 1.0f - k_density_epsilon)
        {
            ++fully_grown_count;
        }
    }

    update_restoration_progress(context, fully_grown_count);
}
}  // namespace gs1
