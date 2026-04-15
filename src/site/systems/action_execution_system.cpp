#include "site/systems/action_execution_system.h"

#include "content/defs/item_defs.h"
#include "site/site_projection_update_flags.h"
#include "site/site_run_state.h"
#include "support/id_types.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>

namespace gs1
{
namespace
{
constexpr double k_seconds_per_minute = 60.0;
constexpr double k_minimum_action_duration_minutes = 0.25;

ActionKind to_action_kind(Gs1SiteActionKind kind) noexcept
{
    switch (kind)
    {
    case GS1_SITE_ACTION_PLANT:
        return ActionKind::Plant;
    case GS1_SITE_ACTION_BUILD:
        return ActionKind::Build;
    case GS1_SITE_ACTION_REPAIR:
        return ActionKind::Repair;
    case GS1_SITE_ACTION_WATER:
        return ActionKind::Water;
    case GS1_SITE_ACTION_CLEAR_BURIAL:
        return ActionKind::ClearBurial;
    default:
        return ActionKind::None;
    }
}

Gs1SiteActionKind to_gs1_action_kind(ActionKind kind) noexcept
{
    switch (kind)
    {
    case ActionKind::Plant:
        return GS1_SITE_ACTION_PLANT;
    case ActionKind::Build:
        return GS1_SITE_ACTION_BUILD;
    case ActionKind::Repair:
        return GS1_SITE_ACTION_REPAIR;
    case ActionKind::Water:
        return GS1_SITE_ACTION_WATER;
    case ActionKind::ClearBurial:
        return GS1_SITE_ACTION_CLEAR_BURIAL;
    default:
        return GS1_SITE_ACTION_NONE;
    }
}

double base_duration_minutes(ActionKind kind) noexcept
{
    switch (kind)
    {
    case ActionKind::Plant:
        return 1.0;
    case ActionKind::Build:
        return 2.5;
    case ActionKind::Repair:
        return 1.5;
    case ActionKind::Water:
        return 0.75;
    case ActionKind::ClearBurial:
        return 1.0;
    case ActionKind::None:
    default:
        return 1.0;
    }
}

float action_energy_cost(ActionKind kind, std::uint16_t quantity) noexcept
{
    const float scale = static_cast<float>(quantity == 0U ? 1U : quantity);
    switch (kind)
    {
    case ActionKind::Plant:
        return 2.0f * scale;
    case ActionKind::Water:
        return 1.0f * scale;
    case ActionKind::ClearBurial:
        return 1.5f * scale;
    default:
        return 0.0f;
    }
}

float action_hydration_cost(ActionKind kind, std::uint16_t quantity) noexcept
{
    const float scale = static_cast<float>(quantity == 0U ? 1U : quantity);
    switch (kind)
    {
    case ActionKind::Plant:
        return 0.75f * scale;
    case ActionKind::Water:
        return 0.35f * scale;
    case ActionKind::ClearBurial:
        return 0.5f * scale;
    default:
        return 0.0f;
    }
}

double compute_duration_minutes(ActionKind kind, std::uint16_t quantity) noexcept
{
    const std::uint16_t safe_quantity = quantity == 0U ? 1U : quantity;
    const double duration = base_duration_minutes(kind) * static_cast<double>(safe_quantity);
    return std::max(k_minimum_action_duration_minutes, duration);
}

RuntimeActionId allocate_runtime_action_id() noexcept
{
    static std::uint32_t next_id = 1U;
    return RuntimeActionId{next_id++};
}

template <typename Payload>
void enqueue_command(GameCommandQueue& queue, GameCommandType type, const Payload& payload)
{
    GameCommand command {};
    command.type = type;
    command.set_payload(payload);
    queue.push_back(command);
}

void clear_action_state(ActionState& action_state) noexcept
{
    action_state.current_action_id.reset();
    action_state.action_kind = ActionKind::None;
    action_state.target_tile.reset();
    action_state.approach_tile.reset();
    action_state.primary_subject_id = 0U;
    action_state.secondary_subject_id = 0U;
    action_state.item_id = 0U;
    action_state.placement_reservation_token = 0U;
    action_state.quantity = 0U;
    action_state.request_flags = 0U;
    action_state.awaiting_placement_reservation = false;
    action_state.remaining_action_minutes = 0.0;
    action_state.reserved_input_item_stacks.clear();
    action_state.started_at_world_minute.reset();
}

std::uint32_t action_id_value(const ActionState& action_state) noexcept
{
    if (action_state.current_action_id.has_value())
    {
        return action_state.current_action_id->value;
    }
    return 0U;
}

TileCoord action_target_tile(const ActionState& action_state) noexcept
{
    if (action_state.target_tile.has_value())
    {
        return *action_state.target_tile;
    }
    return TileCoord {};
}

TileCoord action_approach_tile(const ActionState& action_state) noexcept
{
    if (action_state.approach_tile.has_value())
    {
        return *action_state.approach_tile;
    }
    return action_target_tile(action_state);
}

bool should_request_placement_reservation(ActionKind action_kind) noexcept
{
    return action_kind == ActionKind::Plant;
}

bool action_requires_worker_approach(ActionKind action_kind) noexcept
{
    return action_kind == ActionKind::Plant;
}

PlacementOccupancyLayer placement_occupancy_layer(ActionKind action_kind) noexcept
{
    switch (action_kind)
    {
    case ActionKind::Plant:
        return PlacementOccupancyLayer::GroundCover;
    case ActionKind::Build:
        return PlacementOccupancyLayer::Structure;
    default:
        return PlacementOccupancyLayer::None;
    }
}

void emit_site_action_started(
    GameCommandQueue& queue,
    RuntimeActionId action_id,
    ActionKind action_kind,
    std::uint8_t flags,
    TileCoord target_tile,
    std::uint32_t primary_subject_id,
    float duration_minutes)
{
    enqueue_command(
        queue,
        GameCommandType::SiteActionStarted,
        SiteActionStartedCommand {
            action_id.value,
            to_gs1_action_kind(action_kind),
            flags,
            0U,
            target_tile.x,
            target_tile.y,
            primary_subject_id,
            duration_minutes});
}

void emit_site_action_completed(GameCommandQueue& queue, const ActionState& action_state)
{
    const TileCoord target_tile = action_target_tile(action_state);
    enqueue_command(
        queue,
        GameCommandType::SiteActionCompleted,
        SiteActionCompletedCommand {
            action_id_value(action_state),
            to_gs1_action_kind(action_state.action_kind),
            0U,
            0U,
            target_tile.x,
            target_tile.y,
            action_state.primary_subject_id,
            action_state.secondary_subject_id});
}

void emit_worker_meter_cost_request(
    GameCommandQueue& queue,
    std::uint32_t action_id,
    ActionKind action_kind,
    std::uint16_t quantity)
{
    enqueue_command(
        queue,
        GameCommandType::WorkerMeterDeltaRequested,
        WorkerMeterDeltaRequestedCommand {
            action_id,
            WORKER_METER_CHANGED_HYDRATION | WORKER_METER_CHANGED_ENERGY,
            0.0f,
            -action_hydration_cost(action_kind, quantity),
            0.0f,
            0.0f,
            -action_energy_cost(action_kind, quantity),
            0.0f,
            0.0f});
}

void emit_site_action_failed(
    GameCommandQueue& queue,
    std::uint32_t action_id,
    ActionKind action_kind,
    SiteActionFailureReason reason,
    std::uint16_t flags,
    TileCoord target_tile,
    std::uint32_t primary_subject_id,
    std::uint32_t secondary_subject_id)
{
    enqueue_command(
        queue,
        GameCommandType::SiteActionFailed,
        SiteActionFailedCommand {
            action_id,
            to_gs1_action_kind(action_kind),
            reason,
            flags,
            target_tile.x,
            target_tile.y,
            primary_subject_id,
            secondary_subject_id});
}

bool has_active_action(const ActionState& action_state) noexcept
{
    return action_state.current_action_id.has_value() &&
        action_state.action_kind != ActionKind::None;
}

std::uint32_t available_worker_pack_item_quantity(
    const InventoryState& inventory,
    ItemId item_id) noexcept
{
    std::uint32_t total_quantity = 0U;
    for (const auto& slot : inventory.worker_pack_slots)
    {
        if (!slot.occupied || slot.item_id != item_id)
        {
            continue;
        }

        total_quantity += slot.item_quantity;
    }

    return total_quantity;
}

const ItemDef* action_item_def(
    ActionKind action_kind,
    std::uint32_t item_id) noexcept
{
    if (action_kind != ActionKind::Plant || item_id == 0U)
    {
        return nullptr;
    }

    const auto* item_def = find_item_def(ItemId {item_id});
    if (item_def == nullptr ||
        !item_has_capability(*item_def, ITEM_CAPABILITY_PLANT) ||
        item_def->linked_plant_id.value == 0U)
    {
        return nullptr;
    }

    return item_def;
}

bool action_requires_item(ActionKind action_kind, std::uint32_t item_id) noexcept
{
    return action_kind == ActionKind::Plant && item_id != 0U;
}

bool is_action_waiting_for_placement(const ActionState& action_state) noexcept
{
    return has_active_action(action_state) && action_state.awaiting_placement_reservation;
}

bool action_has_started(const ActionState& action_state) noexcept
{
    return action_state.started_at_world_minute.has_value();
}

bool is_action_waiting_for_worker_approach(const ActionState& action_state) noexcept
{
    return has_active_action(action_state) &&
        !action_state.awaiting_placement_reservation &&
        action_requires_worker_approach(action_state.action_kind) &&
        !action_has_started(action_state);
}

bool is_action_in_progress(const ActionState& action_state) noexcept
{
    return has_active_action(action_state) && action_has_started(action_state);
}

bool tile_is_traversable(
    const SiteWorldAccess<ActionExecutionSystem>& world,
    TileCoord coord) noexcept
{
    return world.tile_coord_in_bounds(coord) && world.read_tile(coord).static_data.traversable;
}

std::optional<TileCoord> resolve_action_approach_tile(
    const SiteWorldAccess<ActionExecutionSystem>& world,
    const SiteWorld::WorkerData& worker,
    TileCoord target_tile)
{
    if (tile_is_traversable(world, target_tile))
    {
        return target_tile;
    }

    constexpr std::array<TileCoord, 8> k_neighbor_offsets {{
        TileCoord {0, -1},
        TileCoord {1, 0},
        TileCoord {0, 1},
        TileCoord {-1, 0},
        TileCoord {1, -1},
        TileCoord {1, 1},
        TileCoord {-1, 1},
        TileCoord {-1, -1},
    }};

    float best_distance_squared = std::numeric_limits<float>::max();
    std::optional<TileCoord> best_tile {};
    for (const auto offset : k_neighbor_offsets)
    {
        const TileCoord candidate {
            target_tile.x + offset.x,
            target_tile.y + offset.y};
        if (!tile_is_traversable(world, candidate))
        {
            continue;
        }

        const float dx =
            worker.position.tile_x - static_cast<float>(candidate.x);
        const float dy =
            worker.position.tile_y - static_cast<float>(candidate.y);
        const float distance_squared = dx * dx + dy * dy;
        if (distance_squared < best_distance_squared)
        {
            best_distance_squared = distance_squared;
            best_tile = candidate;
        }
    }

    return best_tile;
}

bool worker_is_at_action_approach_tile(
    SiteSystemContext<ActionExecutionSystem>& context,
    const ActionState& action_state) noexcept
{
    if (!action_state.approach_tile.has_value())
    {
        return true;
    }

    const auto worker = context.world.read_worker();
    const float dx =
        worker.position.tile_x - static_cast<float>(action_state.approach_tile->x);
    const float dy =
        worker.position.tile_y - static_cast<float>(action_state.approach_tile->y);
    return (dx * dx + dy * dy) <= 0.0025f;
}

void begin_action_execution(
    SiteSystemContext<ActionExecutionSystem>& context,
    ActionState& action_state)
{
    if (!action_state.current_action_id.has_value() || action_has_started(action_state))
    {
        return;
    }

    action_state.started_at_world_minute = context.world.read_time().world_time_minutes;
    emit_site_action_started(
        context.command_queue,
        action_state.current_action_id.value(),
        action_state.action_kind,
        action_state.request_flags,
        action_target_tile(action_state),
        action_state.primary_subject_id,
        static_cast<float>(action_state.remaining_action_minutes));
    emit_worker_meter_cost_request(
        context.command_queue,
        action_state.current_action_id->value,
        action_state.action_kind,
        action_state.quantity);
    context.world.mark_projection_dirty(
        SITE_PROJECTION_UPDATE_WORKER | SITE_PROJECTION_UPDATE_HUD);
}

void emit_placement_reservation_requested(
    GameCommandQueue& queue,
    std::uint32_t action_id,
    ActionKind action_kind,
    TileCoord target_tile,
    std::uint32_t subject_id)
{
    enqueue_command(
        queue,
        GameCommandType::PlacementReservationRequested,
        PlacementReservationRequestedCommand {
            action_id,
            target_tile.x,
            target_tile.y,
            placement_occupancy_layer(action_kind),
            {0U, 0U, 0U},
            subject_id});
}

void emit_placement_reservation_released(
    GameCommandQueue& queue,
    const ActionState& action_state)
{
    if (!action_state.current_action_id.has_value())
    {
        return;
    }

    enqueue_command(
        queue,
        GameCommandType::PlacementReservationReleased,
        PlacementReservationReleasedCommand {
            action_state.current_action_id->value,
            action_state.placement_reservation_token});
}

void emit_action_fact_commands(GameCommandQueue& queue, const ActionState& action_state)
{
    const TileCoord target_tile = action_target_tile(action_state);
    const auto action_id = action_id_value(action_state);
    const auto flags = static_cast<std::uint32_t>(action_state.request_flags);
    const std::uint16_t safe_quantity = static_cast<std::uint16_t>(
        action_state.quantity == 0U ? 1U : action_state.quantity);

    switch (action_state.action_kind)
    {
    case ActionKind::Plant:
        if (const auto* item_def = action_item_def(
                action_state.action_kind,
                action_state.item_id))
        {
            enqueue_command(
                queue,
                GameCommandType::InventoryItemConsumeRequested,
                InventoryItemConsumeRequestedCommand {
                    item_def->item_id.value,
                    safe_quantity,
                    GS1_INVENTORY_CONTAINER_WORKER_PACK,
                    0U});
            enqueue_command(
                queue,
                GameCommandType::SiteTilePlantingCompleted,
                SiteTilePlantingCompletedCommand {
                    action_id,
                    target_tile.x,
                    target_tile.y,
                    item_def->linked_plant_id.value,
                    std::min(1.0f, 0.2f * static_cast<float>(safe_quantity)),
                    flags});
        }
        else
        {
            enqueue_command(
                queue,
                GameCommandType::SiteGroundCoverPlaced,
                SiteGroundCoverPlacedCommand {
                    action_id,
                    target_tile.x,
                    target_tile.y,
                    action_state.primary_subject_id == 0U ? 1U : action_state.primary_subject_id,
                    std::min(1.0f, 0.25f * static_cast<float>(safe_quantity)),
                    flags});
        }
        break;

    case ActionKind::Water:
        enqueue_command(
            queue,
            GameCommandType::SiteTileWatered,
            SiteTileWateredCommand {
                action_id,
                target_tile.x,
                target_tile.y,
                static_cast<float>(action_state.quantity == 0U ? 1U : action_state.quantity),
                flags});
        break;

    case ActionKind::ClearBurial:
        enqueue_command(
            queue,
            GameCommandType::SiteTileBurialCleared,
            SiteTileBurialClearedCommand {
                action_id,
                target_tile.x,
                target_tile.y,
                0.35f * static_cast<float>(action_state.quantity == 0U ? 1U : action_state.quantity),
                flags});
        break;

    case ActionKind::Build:
    case ActionKind::Repair:
    case ActionKind::None:
    default:
        break;
    }
}

}  // namespace

bool ActionExecutionSystem::subscribes_to(GameCommandType type) noexcept
{
    switch (type)
    {
    case GameCommandType::StartSiteAction:
    case GameCommandType::CancelSiteAction:
    case GameCommandType::PlacementReservationAccepted:
    case GameCommandType::PlacementReservationRejected:
        return true;
    default:
        return false;
    }
}

Gs1Status ActionExecutionSystem::process_command(
    SiteSystemContext<ActionExecutionSystem>& context,
    const GameCommand& command)
{
    auto& action_state = context.world.own_action();
    const auto& payload_type = command.type;
    switch (payload_type)
    {
    case GameCommandType::StartSiteAction:
    {
        const auto& payload = command.payload_as<StartSiteActionCommand>();

        if (has_active_action(action_state))
        {
            emit_site_action_failed(
                context.command_queue,
                0U,
                to_action_kind(payload.action_kind),
                SiteActionFailureReason::Busy,
                payload.flags,
                {payload.target_tile_x, payload.target_tile_y},
                payload.primary_subject_id,
                payload.secondary_subject_id);
            return GS1_STATUS_OK;
        }

        const ActionKind action_kind = to_action_kind(payload.action_kind);
        if (action_kind == ActionKind::None ||
            action_kind == ActionKind::Build ||
            action_kind == ActionKind::Repair)
        {
            emit_site_action_failed(
                context.command_queue,
                0U,
                action_kind,
                SiteActionFailureReason::InvalidState,
                payload.flags,
                {payload.target_tile_x, payload.target_tile_y},
                payload.primary_subject_id,
                payload.secondary_subject_id);
            return GS1_STATUS_OK;
        }

        const TileCoord target_tile {payload.target_tile_x, payload.target_tile_y};
        if (!context.world.tile_coord_in_bounds(target_tile))
        {
            emit_site_action_failed(
                context.command_queue,
                0U,
                action_kind,
                SiteActionFailureReason::InvalidTarget,
                payload.flags,
                target_tile,
                payload.primary_subject_id,
                payload.secondary_subject_id);
            return GS1_STATUS_OK;
        }

        if (action_requires_item(action_kind, payload.item_id))
        {
            const auto* item_def = action_item_def(action_kind, payload.item_id);
            if (item_def == nullptr)
            {
                emit_site_action_failed(
                    context.command_queue,
                    0U,
                    action_kind,
                    SiteActionFailureReason::InvalidState,
                    payload.flags,
                    target_tile,
                    payload.primary_subject_id,
                    payload.secondary_subject_id);
                return GS1_STATUS_OK;
            }

            const auto required_quantity = payload.quantity == 0U ? 1U : payload.quantity;
            if (available_worker_pack_item_quantity(context.world.read_inventory(), item_def->item_id) <
                required_quantity)
            {
                emit_site_action_failed(
                    context.command_queue,
                    0U,
                    action_kind,
                    SiteActionFailureReason::InsufficientResources,
                    payload.flags,
                    target_tile,
                    payload.primary_subject_id,
                    payload.secondary_subject_id);
                return GS1_STATUS_OK;
            }
        }

        const double duration_minutes =
            compute_duration_minutes(action_kind, payload.quantity == 0U ? 1U : payload.quantity);
        const RuntimeActionId action_id = allocate_runtime_action_id();

        action_state.current_action_id = action_id;
        action_state.action_kind = action_kind;
        action_state.target_tile = target_tile;
        action_state.approach_tile.reset();
        action_state.primary_subject_id = payload.primary_subject_id;
        action_state.secondary_subject_id = payload.secondary_subject_id;
        action_state.item_id = payload.item_id;
        action_state.quantity = payload.quantity == 0U ? 1U : payload.quantity;
        action_state.request_flags = payload.flags;
        action_state.remaining_action_minutes = duration_minutes;
        action_state.started_at_world_minute.reset();
        action_state.awaiting_placement_reservation =
            should_request_placement_reservation(action_kind);
        action_state.reserved_input_item_stacks.clear();

        if (action_requires_worker_approach(action_kind))
        {
            const auto worker = context.world.read_worker();
            const auto approach_tile =
                resolve_action_approach_tile(context.world, worker, target_tile);
            if (!approach_tile.has_value())
            {
                clear_action_state(action_state);
                emit_site_action_failed(
                    context.command_queue,
                    0U,
                    action_kind,
                    SiteActionFailureReason::InvalidTarget,
                    payload.flags,
                    target_tile,
                    payload.primary_subject_id,
                    payload.secondary_subject_id);
                return GS1_STATUS_OK;
            }

            action_state.approach_tile = approach_tile;
        }

        if (action_state.awaiting_placement_reservation)
        {
            emit_placement_reservation_requested(
                context.command_queue,
                action_id.value,
                action_kind,
                target_tile,
                payload.primary_subject_id);
        }
        else
        {
            begin_action_execution(context, action_state);
        }

        context.world.mark_projection_dirty(
            SITE_PROJECTION_UPDATE_WORKER | SITE_PROJECTION_UPDATE_HUD);
        return GS1_STATUS_OK;
    }

    case GameCommandType::CancelSiteAction:
    {
        const auto& payload = command.payload_as<CancelSiteActionCommand>();

        if (!has_active_action(action_state))
        {
            return GS1_STATUS_OK;
        }

        const bool cancels_current =
            (payload.flags & GS1_SITE_ACTION_CANCEL_FLAG_CURRENT_ACTION) != 0U;
        const bool matches_id =
            action_state.current_action_id.has_value() &&
            payload.action_id == action_state.current_action_id->value;

        if (!cancels_current && !matches_id)
        {
            return GS1_STATUS_OK;
        }

        emit_placement_reservation_released(context.command_queue, action_state);
        emit_site_action_failed(
            context.command_queue,
            action_id_value(action_state),
            action_state.action_kind,
            SiteActionFailureReason::Cancelled,
            static_cast<std::uint16_t>(payload.flags),
            action_target_tile(action_state),
            0U,
            0U);

        clear_action_state(action_state);
        context.world.mark_projection_dirty(
            SITE_PROJECTION_UPDATE_WORKER | SITE_PROJECTION_UPDATE_HUD);
        return GS1_STATUS_OK;
    }

    case GameCommandType::PlacementReservationAccepted:
    {
        if (!is_action_waiting_for_placement(action_state))
        {
            return GS1_STATUS_OK;
        }

        const auto& payload = command.payload_as<PlacementReservationAcceptedCommand>();
        if (payload.action_id != action_id_value(action_state))
        {
            return GS1_STATUS_OK;
        }

        action_state.awaiting_placement_reservation = false;
        action_state.placement_reservation_token = payload.reservation_token;
        if (!action_requires_worker_approach(action_state.action_kind) ||
            worker_is_at_action_approach_tile(context, action_state))
        {
            begin_action_execution(context, action_state);
        }
        else
        {
            context.world.mark_projection_dirty(SITE_PROJECTION_UPDATE_WORKER);
        }
        return GS1_STATUS_OK;
    }

    case GameCommandType::PlacementReservationRejected:
    {
        if (!is_action_waiting_for_placement(action_state))
        {
            return GS1_STATUS_OK;
        }

        const auto& payload = command.payload_as<PlacementReservationRejectedCommand>();
        if (payload.action_id != action_id_value(action_state))
        {
            return GS1_STATUS_OK;
        }

        emit_site_action_failed(
            context.command_queue,
            action_id_value(action_state),
            action_state.action_kind,
            SiteActionFailureReason::InvalidTarget,
            action_state.request_flags,
            action_target_tile(action_state),
            action_state.primary_subject_id,
            action_state.secondary_subject_id);
        clear_action_state(action_state);
        context.world.mark_projection_dirty(
            SITE_PROJECTION_UPDATE_WORKER | SITE_PROJECTION_UPDATE_HUD);
        return GS1_STATUS_OK;
    }

    default:
        return GS1_STATUS_OK;
    }
}

void ActionExecutionSystem::run(SiteSystemContext<ActionExecutionSystem>& context)
{
    auto& action_state = context.world.own_action();
    if (is_action_waiting_for_worker_approach(action_state) &&
        worker_is_at_action_approach_tile(context, action_state))
    {
        begin_action_execution(context, action_state);
    }

    if (!is_action_in_progress(action_state))
    {
        return;
    }

    const double delta_minutes = context.fixed_step_seconds / k_seconds_per_minute;
    action_state.remaining_action_minutes =
        std::max(0.0, action_state.remaining_action_minutes - delta_minutes);

    if (action_state.remaining_action_minutes > 0.0)
    {
        return;
    }

    emit_site_action_completed(context.command_queue, action_state);
    emit_action_fact_commands(context.command_queue, action_state);
    emit_placement_reservation_released(context.command_queue, action_state);
    clear_action_state(action_state);
    context.world.mark_projection_dirty(
        SITE_PROJECTION_UPDATE_WORKER | SITE_PROJECTION_UPDATE_HUD);
}
}  // namespace gs1
