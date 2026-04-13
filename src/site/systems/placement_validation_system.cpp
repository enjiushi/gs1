#include "site/systems/placement_validation_system.h"

#include "site/site_run_state.h"

#include <algorithm>
#include <cstddef>
#include <vector>

namespace gs1
{
namespace
{
struct PlacementReservationRecord final
{
    SiteRunId site_run_id {};
    std::uint32_t action_id {0};
    std::uint32_t reservation_token {0};
    TileCoord target_tile {};
    PlacementOccupancyLayer occupancy_layer {PlacementOccupancyLayer::None};
    std::uint32_t subject_id {0};
};

std::vector<PlacementReservationRecord> g_reservations {};
std::uint32_t g_next_reservation_token = 1U;

template <typename Payload>
void enqueue_command(GameCommandQueue& queue, GameCommandType type, const Payload& payload)
{
    GameCommand command {};
    command.type = type;
    command.set_payload(payload);
    queue.push_back(command);
}

bool tile_coord_in_bounds(const TileGridState& tile_grid, TileCoord coord) noexcept
{
    return coord.x >= 0 &&
        coord.y >= 0 &&
        static_cast<std::uint32_t>(coord.x) < tile_grid.width &&
        static_cast<std::uint32_t>(coord.y) < tile_grid.height;
}

void prune_reservations_for_run(SiteRunId site_run_id) noexcept
{
    g_reservations.erase(
        std::remove_if(
            g_reservations.begin(),
            g_reservations.end(),
            [&](const PlacementReservationRecord& record) {
                return record.site_run_id != site_run_id;
            }),
        g_reservations.end());
}

bool is_tile_reserved(
    SiteRunId site_run_id,
    TileCoord coord,
    std::uint32_t excluding_action_id = 0U) noexcept
{
    return std::any_of(
        g_reservations.begin(),
        g_reservations.end(),
        [&](const PlacementReservationRecord& record) {
            return record.site_run_id == site_run_id &&
                record.target_tile.x == coord.x &&
                record.target_tile.y == coord.y &&
                record.action_id != excluding_action_id;
        });
}

PlacementReservationRejectionReason validate_request(
    const SiteRunState& site_run,
    TileCoord target_tile,
    PlacementOccupancyLayer occupancy_layer,
    std::uint32_t excluding_action_id) noexcept
{
    const auto& tile_grid = site_run.tile_grid;
    if (!tile_coord_in_bounds(tile_grid, target_tile))
    {
        return PlacementReservationRejectionReason::OutOfBounds;
    }

    const auto index = tile_grid.to_index(target_tile);
    if (occupancy_layer == PlacementOccupancyLayer::GroundCover)
    {
        if (index >= tile_grid.tile_plantable.size() || tile_grid.tile_plantable[index] == 0U)
        {
            return PlacementReservationRejectionReason::TerrainBlocked;
        }
        if (index < tile_grid.tile_reserved_by_structure.size() &&
            tile_grid.tile_reserved_by_structure[index] != 0U)
        {
            return PlacementReservationRejectionReason::Occupied;
        }
        if (index < tile_grid.structure_type_ids.size() &&
            tile_grid.structure_type_ids[index].value != 0U)
        {
            return PlacementReservationRejectionReason::Occupied;
        }
        if (index < tile_grid.plant_type_ids.size() &&
            tile_grid.plant_type_ids[index].value != 0U)
        {
            return PlacementReservationRejectionReason::Occupied;
        }
        if (index < tile_grid.ground_cover_type_ids.size() &&
            tile_grid.ground_cover_type_ids[index] != 0U)
        {
            return PlacementReservationRejectionReason::Occupied;
        }
    }
    else if (occupancy_layer == PlacementOccupancyLayer::Structure)
    {
        if (index >= tile_grid.tile_traversable.size() || tile_grid.tile_traversable[index] == 0U)
        {
            return PlacementReservationRejectionReason::TerrainBlocked;
        }
        if (index < tile_grid.structure_type_ids.size() &&
            tile_grid.structure_type_ids[index].value != 0U)
        {
            return PlacementReservationRejectionReason::Occupied;
        }
    }
    else
    {
        return PlacementReservationRejectionReason::TerrainBlocked;
    }

    if (is_tile_reserved(site_run.site_run_id, target_tile, excluding_action_id))
    {
        return PlacementReservationRejectionReason::Reserved;
    }

    return PlacementReservationRejectionReason::None;
}

void handle_site_run_started(SiteSystemContext& context) noexcept
{
    prune_reservations_for_run(context.site_run.site_run_id);
}

void handle_reservation_requested(
    SiteSystemContext& context,
    const PlacementReservationRequestedCommand& payload)
{
    const TileCoord target_tile {payload.target_tile_x, payload.target_tile_y};
    const auto rejection_reason = validate_request(
        context.site_run,
        target_tile,
        payload.occupancy_layer,
        payload.action_id);
    if (rejection_reason != PlacementReservationRejectionReason::None)
    {
        enqueue_command(
            context.command_queue,
            GameCommandType::PlacementReservationRejected,
            PlacementReservationRejectedCommand {
                payload.action_id,
                payload.target_tile_x,
                payload.target_tile_y,
                rejection_reason,
                {0U, 0U, 0U}});
        return;
    }

    const std::uint32_t reservation_token = g_next_reservation_token++;
    g_reservations.push_back(PlacementReservationRecord {
        context.site_run.site_run_id,
        payload.action_id,
        reservation_token,
        target_tile,
        payload.occupancy_layer,
        payload.subject_id});

    enqueue_command(
        context.command_queue,
        GameCommandType::PlacementReservationAccepted,
        PlacementReservationAcceptedCommand {
            payload.action_id,
            payload.target_tile_x,
            payload.target_tile_y,
            reservation_token});
}

void handle_reservation_released(const PlacementReservationReleasedCommand& payload) noexcept
{
    g_reservations.erase(
        std::remove_if(
            g_reservations.begin(),
            g_reservations.end(),
            [&](const PlacementReservationRecord& record) {
                const bool action_match =
                    payload.action_id != 0U && record.action_id == payload.action_id;
                const bool token_match =
                    payload.reservation_token != 0U && record.reservation_token == payload.reservation_token;
                return action_match || token_match;
            }),
        g_reservations.end());
}
}  // namespace

bool PlacementValidationSystem::subscribes_to(GameCommandType type) noexcept
{
    switch (type)
    {
    case GameCommandType::SiteRunStarted:
    case GameCommandType::PlacementReservationRequested:
    case GameCommandType::PlacementReservationReleased:
        return true;
    default:
        return false;
    }
}

Gs1Status PlacementValidationSystem::process_command(
    SiteSystemContext& context,
    const GameCommand& command)
{
    switch (command.type)
    {
    case GameCommandType::SiteRunStarted:
        handle_site_run_started(context);
        break;

    case GameCommandType::PlacementReservationRequested:
        handle_reservation_requested(
            context,
            command.payload_as<PlacementReservationRequestedCommand>());
        break;

    case GameCommandType::PlacementReservationReleased:
        handle_reservation_released(command.payload_as<PlacementReservationReleasedCommand>());
        break;

    default:
        break;
    }

    return GS1_STATUS_OK;
}

void PlacementValidationSystem::run(SiteSystemContext& context)
{
    (void)context;
}
}  // namespace gs1
