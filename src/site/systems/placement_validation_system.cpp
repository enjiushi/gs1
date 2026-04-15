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
    SiteRunId site_run_id,
    const SiteWorldAccess<PlacementValidationSystem>& world,
    TileCoord target_tile,
    PlacementOccupancyLayer occupancy_layer,
    std::uint32_t excluding_action_id) noexcept
{
    if (!world.tile_coord_in_bounds(target_tile))
    {
        return PlacementReservationRejectionReason::OutOfBounds;
    }

    const auto tile = world.read_tile(target_tile);
    if (occupancy_layer == PlacementOccupancyLayer::GroundCover)
    {
        if (!tile.static_data.plantable)
        {
            return PlacementReservationRejectionReason::TerrainBlocked;
        }

        if (tile.static_data.reserved_by_structure ||
            tile.device.structure_id.value != 0U ||
            tile.ecology.plant_id.value != 0U ||
            tile.ecology.ground_cover_type_id != 0U)
        {
            return PlacementReservationRejectionReason::Occupied;
        }
    }
    else if (occupancy_layer == PlacementOccupancyLayer::Structure)
    {
        if (!tile.static_data.traversable)
        {
            return PlacementReservationRejectionReason::TerrainBlocked;
        }

        if (tile.device.structure_id.value != 0U)
        {
            return PlacementReservationRejectionReason::Occupied;
        }
    }
    else
    {
        return PlacementReservationRejectionReason::TerrainBlocked;
    }

    if (is_tile_reserved(site_run_id, target_tile, excluding_action_id))
    {
        return PlacementReservationRejectionReason::Reserved;
    }

    return PlacementReservationRejectionReason::None;
}

void handle_site_run_started(SiteSystemContext<PlacementValidationSystem>& context) noexcept
{
    prune_reservations_for_run(context.world.site_run_id());
}

void handle_reservation_requested(
    SiteSystemContext<PlacementValidationSystem>& context,
    const PlacementReservationRequestedCommand& payload)
{
    const TileCoord target_tile {payload.target_tile_x, payload.target_tile_y};
    const auto rejection_reason = validate_request(
        context.world.site_run_id(),
        context.world,
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
        context.world.site_run_id(),
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
    SiteSystemContext<PlacementValidationSystem>& context,
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

void PlacementValidationSystem::run(SiteSystemContext<PlacementValidationSystem>& context)
{
    (void)context;
}
}  // namespace gs1
