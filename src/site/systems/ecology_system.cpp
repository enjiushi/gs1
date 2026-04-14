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

bool tile_coord_in_bounds(const TileGridState& tile_grid, TileCoord coord) noexcept
{
    return coord.x >= 0 &&
        coord.y >= 0 &&
        static_cast<std::uint32_t>(coord.x) < tile_grid.width &&
        static_cast<std::uint32_t>(coord.y) < tile_grid.height;
}

bool has_valid_tile_data(const TileGridState& tile_grid) noexcept
{
    const auto tile_count = tile_grid.tile_count();
    return tile_count > 0 &&
        tile_grid.plant_type_ids.size() >= tile_count &&
        tile_grid.ground_cover_type_ids.size() >= tile_count &&
        tile_grid.tile_plant_density.size() >= tile_count &&
        tile_grid.tile_sand_burial.size() >= tile_count;
}

bool has_tile_occupant(const TileGridState& tile_grid, std::size_t index) noexcept
{
    return tile_grid.plant_type_ids[index].value != 0U ||
        tile_grid.ground_cover_type_ids[index] != 0U;
}

TileCoord tile_coord_from_index(const TileGridState& tile_grid, std::size_t index) noexcept
{
    return TileCoord {
        static_cast<std::int32_t>(index % tile_grid.width),
        static_cast<std::int32_t>(index / tile_grid.width)};
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

    const auto& tile_grid = context.world.read_tile_grid();
    if (!tile_coord_in_bounds(tile_grid, coord))
    {
        return;
    }

    const auto index = tile_grid.to_index(coord);
    GameCommand command {};
    command.type = GameCommandType::TileEcologyChanged;
    command.set_payload(TileEcologyChangedCommand {
        coord.x,
        coord.y,
        changed_mask,
        tile_grid.plant_type_ids[index].value,
        tile_grid.ground_cover_type_ids[index],
        tile_grid.tile_plant_density[index],
        tile_grid.tile_sand_burial[index]});
    context.command_queue.push_back(command);
    context.world.mark_tile_projection_dirty(coord);
}

std::uint32_t apply_ground_cover(
    TileGridState& tile_grid,
    TileCoord coord,
    const SiteGroundCoverPlacedCommand& payload)
{
    if (!tile_coord_in_bounds(tile_grid, coord) || !has_valid_tile_data(tile_grid))
    {
        return TILE_ECOLOGY_CHANGED_NONE;
    }

    const auto index = tile_grid.to_index(coord);
    std::uint32_t changed_mask = TILE_ECOLOGY_CHANGED_NONE;
    bool occupancy_changed = false;

    if (tile_grid.ground_cover_type_ids[index] != payload.ground_cover_type_id)
    {
        tile_grid.ground_cover_type_ids[index] = payload.ground_cover_type_id;
        occupancy_changed = true;
    }

    if (tile_grid.plant_type_ids[index].value != 0U)
    {
        tile_grid.plant_type_ids[index] = PlantId{};
        occupancy_changed = true;
    }

    const float target_density = std::clamp(payload.initial_density, 0.0f, 1.0f);
    if (std::fabs(tile_grid.tile_plant_density[index] - target_density) > k_density_epsilon)
    {
        tile_grid.tile_plant_density[index] = target_density;
        changed_mask |= TILE_ECOLOGY_CHANGED_DENSITY;
    }

    if (occupancy_changed)
    {
        changed_mask |= TILE_ECOLOGY_CHANGED_OCCUPANCY;
    }

    return changed_mask;
}

std::uint32_t apply_planting(
    TileGridState& tile_grid,
    TileCoord coord,
    const SiteTilePlantingCompletedCommand& payload)
{
    if (!tile_coord_in_bounds(tile_grid, coord) || !has_valid_tile_data(tile_grid))
    {
        return TILE_ECOLOGY_CHANGED_NONE;
    }

    const auto index = tile_grid.to_index(coord);
    std::uint32_t changed_mask = TILE_ECOLOGY_CHANGED_NONE;
    bool occupancy_changed = false;

    const PlantId requested_plant {payload.plant_type_id};
    if (tile_grid.plant_type_ids[index].value != requested_plant.value)
    {
        tile_grid.plant_type_ids[index] = requested_plant;
        occupancy_changed = true;
    }

    if (tile_grid.ground_cover_type_ids[index] != 0U)
    {
        tile_grid.ground_cover_type_ids[index] = 0U;
        occupancy_changed = true;
    }

    const float target_density = std::clamp(payload.initial_density, 0.0f, 1.0f);
    if (std::fabs(tile_grid.tile_plant_density[index] - target_density) > k_density_epsilon)
    {
        tile_grid.tile_plant_density[index] = target_density;
        changed_mask |= TILE_ECOLOGY_CHANGED_DENSITY;
    }

    if (occupancy_changed)
    {
        changed_mask |= TILE_ECOLOGY_CHANGED_OCCUPANCY;
    }

    return changed_mask;
}

std::uint32_t apply_watering(
    TileGridState& tile_grid,
    TileCoord coord,
    const SiteTileWateredCommand& payload)
{
    if (!tile_coord_in_bounds(tile_grid, coord) || !has_valid_tile_data(tile_grid))
    {
        return TILE_ECOLOGY_CHANGED_NONE;
    }

    const auto index = tile_grid.to_index(coord);
    if (!has_tile_occupant(tile_grid, index))
    {
        return TILE_ECOLOGY_CHANGED_NONE;
    }

    const float amount = std::max(0.0f, payload.water_amount);
    if (amount <= 0.0f)
    {
        return TILE_ECOLOGY_CHANGED_NONE;
    }

    const float current_density = tile_grid.tile_plant_density[index];
    if (current_density >= 1.0f - k_density_epsilon)
    {
        return TILE_ECOLOGY_CHANGED_NONE;
    }

    const float next_density = std::min(1.0f, current_density + amount * k_water_density_per_unit);
    if (std::fabs(next_density - current_density) <= k_density_epsilon)
    {
        return TILE_ECOLOGY_CHANGED_NONE;
    }

    tile_grid.tile_plant_density[index] = next_density;
    return TILE_ECOLOGY_CHANGED_DENSITY;
}

std::uint32_t apply_burial_cleared(
    TileGridState& tile_grid,
    TileCoord coord,
    const SiteTileBurialClearedCommand& payload)
{
    if (!tile_coord_in_bounds(tile_grid, coord) || !has_valid_tile_data(tile_grid))
    {
        return TILE_ECOLOGY_CHANGED_NONE;
    }

    const auto index = tile_grid.to_index(coord);
    const float previous_burial = tile_grid.tile_sand_burial[index];
    const float reduction = std::max(0.0f, payload.cleared_amount);
    const float next_burial = std::max(0.0f, previous_burial - reduction);
    if (std::fabs(previous_burial - next_burial) <= k_density_epsilon)
    {
        return TILE_ECOLOGY_CHANGED_NONE;
    }

    tile_grid.tile_sand_burial[index] = next_burial;
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
    auto& tile_grid = context.world.own_tile_grid();
    switch (command.type)
    {
    case GameCommandType::SiteGroundCoverPlaced:
    {
        const auto& payload = command.payload_as<SiteGroundCoverPlacedCommand>();
        const TileCoord coord {payload.target_tile_x, payload.target_tile_y};
        emit_tile_ecology_changed(
            context,
            coord,
            apply_ground_cover(tile_grid, coord, payload));
        break;
    }

    case GameCommandType::SiteTilePlantingCompleted:
    {
        const auto& payload = command.payload_as<SiteTilePlantingCompletedCommand>();
        const TileCoord coord {payload.target_tile_x, payload.target_tile_y};
        emit_tile_ecology_changed(
            context,
            coord,
            apply_planting(tile_grid, coord, payload));
        break;
    }

    case GameCommandType::SiteTileWatered:
    {
        const auto& payload = command.payload_as<SiteTileWateredCommand>();
        const TileCoord coord {payload.target_tile_x, payload.target_tile_y};
        emit_tile_ecology_changed(
            context,
            coord,
            apply_watering(tile_grid, coord, payload));
        break;
    }

    case GameCommandType::SiteTileBurialCleared:
    {
        const auto& payload = command.payload_as<SiteTileBurialClearedCommand>();
        const TileCoord coord {payload.target_tile_x, payload.target_tile_y};
        emit_tile_ecology_changed(
            context,
            coord,
            apply_burial_cleared(tile_grid, coord, payload));
        break;
    }

    default:
        break;
    }

    return GS1_STATUS_OK;
}

void EcologySystem::run(SiteSystemContext<EcologySystem>& context)
{
    auto& tile_grid = context.world.own_tile_grid();
    const auto tile_count = tile_grid.tile_count();
    if (tile_count == 0 || !has_valid_tile_data(tile_grid))
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
    for (std::size_t index = 0; index < tile_count; ++index)
    {
        if (!has_tile_occupant(tile_grid, index))
        {
            continue;
        }

        float& density = tile_grid.tile_plant_density[index];
        if (density < 1.0f - k_density_epsilon)
        {
            const float next_density = std::min(1.0f, density + growth_delta);
            if (std::fabs(next_density - density) > k_density_epsilon)
            {
                density = next_density;
                emit_tile_ecology_changed(
                    context,
                    tile_coord_from_index(tile_grid, index),
                    TILE_ECOLOGY_CHANGED_DENSITY);
            }
        }

        if (density >= 1.0f - k_density_epsilon)
        {
            ++fully_grown_count;
        }
    }

    update_restoration_progress(context, fully_grown_count);
}
}  // namespace gs1
