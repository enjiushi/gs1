#include "site/systems/placement_validation_system.h"

#include "site/site_run_state.h"
#include "site/tile_footprint.h"

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
void enqueue_message(GameMessageQueue& queue, GameMessageType type, const Payload& payload)
{
    GameMessage message {};
    message.type = type;
    message.set_payload(payload);
    queue.push_back(message);
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
    std::uint32_t excluding_action_id,
    TileFootprint footprint) noexcept
{
    if (!is_power_of_two_tile_footprint(footprint))
    {
        return PlacementReservationRejectionReason::TerrainBlocked;
    }

    if (!is_tile_anchor_aligned_to_footprint(target_tile, footprint))
    {
        return PlacementReservationRejectionReason::Misaligned;
    }

    PlacementReservationRejectionReason rejection_reason =
        PlacementReservationRejectionReason::None;
    for_each_tile_in_footprint(
        target_tile,
        footprint,
        [&](TileCoord coord) {
            if (rejection_reason != PlacementReservationRejectionReason::None)
            {
                return;
            }

            if (!world.tile_coord_in_bounds(coord))
            {
                rejection_reason = PlacementReservationRejectionReason::OutOfBounds;
                return;
            }

            const auto tile = world.read_tile(coord);
            if (occupancy_layer == PlacementOccupancyLayer::GroundCover)
            {
                if (!tile.static_data.plantable)
                {
                    rejection_reason = PlacementReservationRejectionReason::TerrainBlocked;
                    return;
                }

                if (tile.static_data.reserved_by_structure ||
                    tile.device.structure_id.value != 0U ||
                    tile.ecology.plant_id.value != 0U ||
                    tile.ecology.ground_cover_type_id != 0U)
                {
                    rejection_reason = PlacementReservationRejectionReason::Occupied;
                    return;
                }
            }
            else if (occupancy_layer == PlacementOccupancyLayer::Structure)
            {
                if (!tile.static_data.traversable)
                {
                    rejection_reason = PlacementReservationRejectionReason::TerrainBlocked;
                    return;
                }

                if (tile.device.structure_id.value != 0U)
                {
                    rejection_reason = PlacementReservationRejectionReason::Occupied;
                    return;
                }
            }
            else
            {
                rejection_reason = PlacementReservationRejectionReason::TerrainBlocked;
                return;
            }

            if (is_tile_reserved(site_run_id, coord, excluding_action_id))
            {
                rejection_reason = PlacementReservationRejectionReason::Reserved;
            }
        });

    return rejection_reason;
}

void handle_site_run_started(SiteSystemContext<PlacementValidationSystem>& context) noexcept
{
    prune_reservations_for_run(context.world.site_run_id());
}

void handle_reservation_requested(
    SiteSystemContext<PlacementValidationSystem>& context,
    const PlacementReservationRequestedMessage& payload)
{
    const TileCoord target_tile {payload.target_tile_x, payload.target_tile_y};
    const TileFootprint footprint = resolve_placement_reservation_footprint(
        payload.occupancy_layer,
        payload.subject_kind,
        payload.subject_id);
    const auto rejection_reason = validate_request(
        context.world.site_run_id(),
        context.world,
        target_tile,
        payload.occupancy_layer,
        payload.action_id,
        footprint);
    if (rejection_reason != PlacementReservationRejectionReason::None)
    {
        enqueue_message(
            context.message_queue,
            GameMessageType::PlacementReservationRejected,
            PlacementReservationRejectedMessage {
                payload.action_id,
                payload.target_tile_x,
                payload.target_tile_y,
                rejection_reason,
                {0U, 0U, 0U}});
        return;
    }

    const std::uint32_t reservation_token = g_next_reservation_token++;
    for_each_tile_in_footprint(
        target_tile,
        footprint,
        [&](TileCoord coord) {
            g_reservations.push_back(PlacementReservationRecord {
                context.world.site_run_id(),
                payload.action_id,
                reservation_token,
                coord,
                payload.occupancy_layer,
                payload.subject_id});
        });

    enqueue_message(
        context.message_queue,
        GameMessageType::PlacementReservationAccepted,
        PlacementReservationAcceptedMessage {
            payload.action_id,
            payload.target_tile_x,
            payload.target_tile_y,
            reservation_token});
}

void handle_reservation_released(const PlacementReservationReleasedMessage& payload) noexcept
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

bool PlacementValidationSystem::subscribes_to(GameMessageType type) noexcept
{
    switch (type)
    {
    case GameMessageType::SiteRunStarted:
    case GameMessageType::PlacementReservationRequested:
    case GameMessageType::PlacementReservationReleased:
        return true;
    default:
        return false;
    }
}

Gs1Status PlacementValidationSystem::process_message(
    SiteSystemContext<PlacementValidationSystem>& context,
    const GameMessage& message)
{
    switch (message.type)
    {
    case GameMessageType::SiteRunStarted:
        handle_site_run_started(context);
        break;

    case GameMessageType::PlacementReservationRequested:
        handle_reservation_requested(
            context,
            message.payload_as<PlacementReservationRequestedMessage>());
        break;

    case GameMessageType::PlacementReservationReleased:
        handle_reservation_released(message.payload_as<PlacementReservationReleasedMessage>());
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
