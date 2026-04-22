#include "site/systems/action_execution_system.h"

#include "content/defs/craft_recipe_defs.h"
#include "content/defs/item_defs.h"
#include "content/defs/plant_defs.h"
#include "content/defs/structure_defs.h"
#include "runtime/runtime_clock.h"
#include "site/device_interaction_logic.h"
#include "site/craft_logic.h"
#include "site/defs/site_action_defs.h"
#include "site/inventory_storage.h"
#include "site/placement_preview.h"
#include "site/site_projection_update_flags.h"
#include "site/site_run_state.h"
#include "support/id_types.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>

namespace gs1
{
namespace
{
constexpr double k_minimum_action_duration_minutes = 0.25;
constexpr float k_repair_integrity_full = 1.0f;
constexpr float k_repair_integrity_epsilon = 0.0001f;
constexpr float k_minimum_action_efficiency = 0.4f;
constexpr float k_maximum_action_efficiency = 1.0f;

ItemId repair_tool_item_id(std::uint32_t requested_item_id) noexcept
{
    return requested_item_id == 0U || requested_item_id == k_item_hammer
        ? ItemId {k_item_hammer}
        : ItemId {};
}

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
    case GS1_SITE_ACTION_CRAFT:
        return ActionKind::Craft;
    case GS1_SITE_ACTION_DRINK:
        return ActionKind::Drink;
    case GS1_SITE_ACTION_EAT:
        return ActionKind::Eat;
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
    case ActionKind::Craft:
        return GS1_SITE_ACTION_CRAFT;
    case ActionKind::Drink:
        return GS1_SITE_ACTION_DRINK;
    case ActionKind::Eat:
        return GS1_SITE_ACTION_EAT;
    default:
        return GS1_SITE_ACTION_NONE;
    }
}

double base_duration_minutes(ActionKind kind) noexcept
{
    const auto* action_def = find_site_action_def(kind);
    return action_def == nullptr
        ? 1.0
        : std::max(
            static_cast<double>(action_def->duration_minutes_per_unit),
            k_minimum_action_duration_minutes);
}

float action_energy_cost(ActionKind kind, std::uint16_t quantity) noexcept
{
    const float scale = static_cast<float>(quantity == 0U ? 1U : quantity);
    const auto* action_def = find_site_action_def(kind);
    return action_def == nullptr ? 0.0f : action_def->energy_cost_per_unit * scale;
}

float action_hydration_cost(ActionKind kind, std::uint16_t quantity) noexcept
{
    const float scale = static_cast<float>(quantity == 0U ? 1U : quantity);
    const auto* action_def = find_site_action_def(kind);
    return action_def == nullptr ? 0.0f : action_def->hydration_cost_per_unit * scale;
}

float action_nourishment_cost(ActionKind kind, std::uint16_t quantity) noexcept
{
    const float scale = static_cast<float>(quantity == 0U ? 1U : quantity);
    const auto* action_def = find_site_action_def(kind);
    return action_def == nullptr ? 0.0f : action_def->nourishment_cost_per_unit * scale;
}

double action_unit_duration_minutes(ActionKind kind, std::uint32_t item_id) noexcept
{
    if (kind == ActionKind::Plant && item_id != 0U)
    {
        const auto* item_def = find_item_def(ItemId {item_id});
        if (item_def != nullptr && item_def->linked_plant_id.value != 0U)
        {
            const auto* plant_def = find_plant_def(item_def->linked_plant_id);
            if (plant_def != nullptr)
            {
                return std::max(
                    static_cast<double>(plant_def->plant_action_duration_minutes),
                    k_minimum_action_duration_minutes);
            }
        }
    }

    return base_duration_minutes(kind);
}

float resolve_action_cost_multiplier(const SiteWorld::WorkerConditionData& worker) noexcept
{
    const float clamped_efficiency =
        std::clamp(worker.work_efficiency, k_minimum_action_efficiency, k_maximum_action_efficiency);
    return 2.0f - clamped_efficiency;
}

double compute_duration_minutes(
    SiteSystemContext<ActionExecutionSystem>& context,
    ActionKind kind,
    std::uint16_t quantity,
    std::uint32_t item_id) noexcept
{
    const std::uint16_t safe_quantity = quantity == 0U ? 1U : quantity;
    const double duration = action_unit_duration_minutes(kind, item_id) * static_cast<double>(safe_quantity);
    return std::max(
        k_minimum_action_duration_minutes,
        duration * static_cast<double>(resolve_action_cost_multiplier(context.world.read_worker().conditions)));
}

RuntimeActionId allocate_runtime_action_id() noexcept
{
    static std::uint32_t next_id = 1U;
    return RuntimeActionId{next_id++};
}

template <typename Payload>
void enqueue_message(GameMessageQueue& queue, GameMessageType type, const Payload& payload)
{
    GameMessage message {};
    message.type = type;
    message.set_payload(payload);
    queue.push_back(message);
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
    action_state.reactivate_placement_mode_on_completion = false;
    action_state.total_action_minutes = 0.0;
    action_state.remaining_action_minutes = 0.0;
    action_state.reserved_input_item_stacks.clear();
    action_state.started_at_world_minute.reset();
}

void clear_placement_mode(PlacementModeState& placement_mode) noexcept
{
    placement_mode.active = false;
    placement_mode.action_kind = ActionKind::None;
    placement_mode.target_tile.reset();
    placement_mode.primary_subject_id = 0U;
    placement_mode.secondary_subject_id = 0U;
    placement_mode.item_id = 0U;
    placement_mode.quantity = 0U;
    placement_mode.request_flags = 0U;
    placement_mode.footprint_width = 1U;
    placement_mode.footprint_height = 1U;
    placement_mode.blocked_mask = 0ULL;
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
    const auto* action_def = find_site_action_def(action_kind);
    return action_def != nullptr && action_def->requests_placement_reservation;
}

bool action_requires_worker_approach(ActionKind action_kind) noexcept
{
    const auto* action_def = find_site_action_def(action_kind);
    return action_def != nullptr && action_def->requires_worker_approach;
}

PlacementOccupancyLayer placement_occupancy_layer(ActionKind action_kind) noexcept
{
    const auto* action_def = find_site_action_def(action_kind);
    return action_def == nullptr ? PlacementOccupancyLayer::None : action_def->placement_occupancy_layer;
}

void emit_site_action_started(
    GameMessageQueue& queue,
    RuntimeActionId action_id,
    ActionKind action_kind,
    std::uint8_t flags,
    TileCoord target_tile,
    std::uint32_t primary_subject_id,
    float duration_minutes)
{
    enqueue_message(
        queue,
        GameMessageType::SiteActionStarted,
        SiteActionStartedMessage {
            action_id.value,
            to_gs1_action_kind(action_kind),
            flags,
            0U,
            target_tile.x,
            target_tile.y,
            primary_subject_id,
            duration_minutes});
}

void emit_site_action_completed(GameMessageQueue& queue, const ActionState& action_state)
{
    const TileCoord target_tile = action_target_tile(action_state);
    enqueue_message(
        queue,
        GameMessageType::SiteActionCompleted,
        SiteActionCompletedMessage {
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
    SiteSystemContext<ActionExecutionSystem>& context,
    std::uint32_t action_id,
    ActionKind action_kind,
    std::uint16_t quantity)
{
    const float action_multiplier =
        resolve_action_cost_multiplier(context.world.read_worker().conditions);
    const float hydration_cost = action_hydration_cost(action_kind, quantity);
    const float nourishment_cost = action_nourishment_cost(action_kind, quantity);
    const float energy_cost = action_energy_cost(action_kind, quantity) * action_multiplier;
    if (hydration_cost == 0.0f && nourishment_cost == 0.0f && energy_cost == 0.0f)
    {
        return;
    }

    enqueue_message(
        context.message_queue,
        GameMessageType::WorkerMeterDeltaRequested,
        WorkerMeterDeltaRequestedMessage {
            action_id,
            WORKER_METER_CHANGED_HYDRATION |
                WORKER_METER_CHANGED_NOURISHMENT |
                WORKER_METER_CHANGED_ENERGY,
            0.0f,
            -hydration_cost,
            -nourishment_cost,
            0.0f,
            -energy_cost,
            0.0f,
            0.0f});
}

void emit_site_action_failed(
    GameMessageQueue& queue,
    std::uint32_t action_id,
    ActionKind action_kind,
    SiteActionFailureReason reason,
    std::uint16_t flags,
    TileCoord target_tile,
    std::uint32_t primary_subject_id,
    std::uint32_t secondary_subject_id)
{
    enqueue_message(
        queue,
        GameMessageType::SiteActionFailed,
        SiteActionFailedMessage {
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

bool has_active_placement_mode(const ActionState& action_state) noexcept
{
    return action_state.placement_mode.active &&
        action_state.placement_mode.action_kind != ActionKind::None;
}

bool action_supports_deferred_target_selection(ActionKind action_kind) noexcept
{
    return action_kind == ActionKind::Plant || action_kind == ActionKind::Build;
}

TileFootprint resolve_action_placement_footprint(
    ActionKind action_kind,
    std::uint32_t item_id,
    std::uint32_t primary_subject_id) noexcept;

std::uint64_t full_blocked_mask_for_footprint(TileFootprint footprint) noexcept
{
    const std::uint32_t cell_count =
        static_cast<std::uint32_t>(footprint.width) *
        static_cast<std::uint32_t>(footprint.height);
    if (cell_count == 0U)
    {
        return 0ULL;
    }

    if (cell_count >= 64U)
    {
        return ~0ULL;
    }

    return (1ULL << cell_count) - 1ULL;
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

std::uint32_t reserved_worker_pack_item_quantity(
    const ActionState& action_state,
    ItemId item_id) noexcept
{
    std::uint32_t total_quantity = 0U;
    for (const auto& reserved_stack : action_state.reserved_input_item_stacks)
    {
        if (reserved_stack.container_kind != GS1_INVENTORY_CONTAINER_WORKER_PACK ||
            reserved_stack.item_id != item_id)
        {
            continue;
        }

        total_quantity += reserved_stack.quantity;
    }

    return total_quantity;
}

const ItemDef* action_item_def(
    ActionKind action_kind,
    std::uint32_t item_id) noexcept
{
    if (item_id == 0U)
    {
        return nullptr;
    }

    const auto* item_def = find_item_def(ItemId {item_id});
    if (item_def == nullptr)
    {
        return nullptr;
    }

    switch (action_kind)
    {
    case ActionKind::Plant:
        return item_has_capability(*item_def, ITEM_CAPABILITY_PLANT) &&
                item_def->linked_plant_id.value != 0U
            ? item_def
            : nullptr;

    case ActionKind::Build:
        return item_has_capability(*item_def, ITEM_CAPABILITY_DEPLOY) &&
                item_def->linked_structure_id.value != 0U
            ? item_def
            : nullptr;

    case ActionKind::Drink:
        return item_has_capability(*item_def, ITEM_CAPABILITY_DRINK) ? item_def : nullptr;

    case ActionKind::Eat:
        return item_has_capability(*item_def, ITEM_CAPABILITY_EAT) ? item_def : nullptr;

    default:
        return nullptr;
    }
}

bool action_requires_item(ActionKind action_kind, std::uint32_t item_id) noexcept
{
    if (action_kind == ActionKind::Build)
    {
        return true;
    }

    if (action_kind == ActionKind::Plant ||
        action_kind == ActionKind::Drink ||
        action_kind == ActionKind::Eat)
    {
        return item_id != 0U;
    }

    return false;
}

bool should_reactivate_plant_placement_mode_after_completion(
    SiteSystemContext<ActionExecutionSystem>& context,
    const ActionState& action_state)
{
    if (!action_state.reactivate_placement_mode_on_completion ||
        action_state.action_kind != ActionKind::Plant)
    {
        return false;
    }

    const auto* item_def = action_item_def(action_state.action_kind, action_state.item_id);
    if (item_def == nullptr || item_def->linked_plant_id.value == 0U)
    {
        return false;
    }

    const std::uint32_t available_quantity =
        available_worker_pack_item_quantity(
            context.world.read_inventory(),
            item_def->item_id);
    const std::uint32_t reserved_quantity =
        reserved_worker_pack_item_quantity(
            action_state,
            item_def->item_id);
    return available_quantity > reserved_quantity;
}

void reactivate_plant_placement_mode_from_completed_action(
    ActionState& action_state)
{
    const TileCoord target_tile = action_target_tile(action_state);
    const TileFootprint footprint =
        resolve_action_placement_footprint(
            action_state.action_kind,
            action_state.item_id,
            action_state.primary_subject_id);

    auto& placement_mode = action_state.placement_mode;
    clear_placement_mode(placement_mode);
    placement_mode.active = true;
    placement_mode.action_kind = action_state.action_kind;
    placement_mode.target_tile = target_tile;
    placement_mode.primary_subject_id = action_state.primary_subject_id;
    placement_mode.secondary_subject_id = action_state.secondary_subject_id;
    placement_mode.item_id = action_state.item_id;
    placement_mode.quantity = 1U;
    placement_mode.request_flags = action_state.request_flags;
    placement_mode.footprint_width = footprint.width;
    placement_mode.footprint_height = footprint.height;
    placement_mode.blocked_mask = full_blocked_mask_for_footprint(footprint);
}

Gs1InventoryContainerKind reserved_item_container_kind(const ActionState& action_state) noexcept
{
    if (!action_state.reserved_input_item_stacks.empty())
    {
        return action_state.reserved_input_item_stacks.front().container_kind;
    }

    return GS1_INVENTORY_CONTAINER_WORKER_PACK;
}

void populate_reserved_input_items(ActionState& action_state)
{
    action_state.reserved_input_item_stacks.clear();
    if (!action_requires_item(action_state.action_kind, action_state.item_id))
    {
        return;
    }

    const auto* item_def = action_item_def(action_state.action_kind, action_state.item_id);
    if (item_def == nullptr)
    {
        return;
    }

    action_state.reserved_input_item_stacks.push_back(ReservedItemStack {
        item_def->item_id,
        static_cast<std::uint32_t>(action_state.quantity == 0U ? 1U : action_state.quantity),
        GS1_INVENTORY_CONTAINER_WORKER_PACK,
        {0U, 0U, 0U}});
}

SiteActionFailureReason validate_reserved_item_completion(
    SiteSystemContext<ActionExecutionSystem>& context,
    const ActionState& action_state)
{
    for (const auto& reserved_stack : action_state.reserved_input_item_stacks)
    {
        const auto available_quantity =
            reserved_stack.container_kind == GS1_INVENTORY_CONTAINER_WORKER_PACK
            ? available_worker_pack_item_quantity(context.world.read_inventory(), reserved_stack.item_id)
            : inventory_storage::available_item_quantity_in_container_kind(
                  context.site_run,
                  reserved_stack.container_kind,
                  reserved_stack.item_id);
        if (available_quantity < reserved_stack.quantity)
        {
            return SiteActionFailureReason::InsufficientResources;
        }
    }

    return SiteActionFailureReason::None;
}

void emit_item_meter_delta_request(
    GameMessageQueue& queue,
    const ItemDef& item_def,
    std::uint16_t quantity)
{
    WorkerMeterDeltaRequestedMessage meter_delta {};
    meter_delta.source_id = item_def.item_id.value;

    if (item_def.health_delta != 0.0f)
    {
        meter_delta.flags |= WORKER_METER_CHANGED_HEALTH;
        meter_delta.health_delta = item_def.health_delta * static_cast<float>(quantity);
    }
    if (item_def.hydration_delta != 0.0f)
    {
        meter_delta.flags |= WORKER_METER_CHANGED_HYDRATION;
        meter_delta.hydration_delta = item_def.hydration_delta * static_cast<float>(quantity);
    }
    if (item_def.nourishment_delta != 0.0f)
    {
        meter_delta.flags |= WORKER_METER_CHANGED_NOURISHMENT;
        meter_delta.nourishment_delta = item_def.nourishment_delta * static_cast<float>(quantity);
    }
    if (item_def.energy_delta != 0.0f)
    {
        meter_delta.flags |= WORKER_METER_CHANGED_ENERGY;
        meter_delta.energy_delta = item_def.energy_delta * static_cast<float>(quantity);
    }

    if (meter_delta.flags == WORKER_METER_CHANGED_NONE)
    {
        return;
    }

    enqueue_message(
        queue,
        GameMessageType::WorkerMeterDeltaRequested,
        meter_delta);
}

SiteActionFailureReason validate_repair_target(
    SiteSystemContext<ActionExecutionSystem>& context,
    TileCoord target_tile) noexcept
{
    if (!context.world.tile_coord_in_bounds(target_tile))
    {
        return SiteActionFailureReason::InvalidTarget;
    }

    const auto tile = context.world.read_tile(target_tile);
    if (tile.device.structure_id.value == 0U || tile.device.device_integrity <= 0.0f)
    {
        return SiteActionFailureReason::InvalidTarget;
    }

    if (tile.device.device_integrity >= (k_repair_integrity_full - k_repair_integrity_epsilon))
    {
        return SiteActionFailureReason::InvalidState;
    }

    return SiteActionFailureReason::None;
}

const CraftRecipeDef* resolve_craft_recipe_for_action(
    SiteSystemContext<ActionExecutionSystem>& context,
    TileCoord target_tile,
    std::uint32_t output_item_id,
    std::uint64_t* out_device_entity_id = nullptr) noexcept
{
    if (output_item_id == 0U || !context.world.tile_coord_in_bounds(target_tile))
    {
        return nullptr;
    }

    const auto tile = context.world.read_tile(target_tile);
    if (!structure_is_crafting_station(tile.device.structure_id) || context.site_run.site_world == nullptr)
    {
        return nullptr;
    }

    if (out_device_entity_id != nullptr)
    {
        *out_device_entity_id = context.site_run.site_world->device_entity_id(target_tile);
    }

    return find_craft_recipe_def(tile.device.structure_id, ItemId {output_item_id});
}

bool craft_ingredients_available_for_action(
    SiteSystemContext<ActionExecutionSystem>& context,
    TileCoord target_tile,
    std::uint64_t device_entity_id,
    const CraftRecipeDef& recipe_def)
{
    const auto cached_items = craft_logic::nearby_item_instance_ids_for_device(
        context.site_run,
        context.world.read_craft(),
        device_entity_id,
        target_tile);
    return craft_logic::can_satisfy_recipe_ingredients(
        context.site_run,
        cached_items,
        recipe_def);
}

bool craft_output_fits_for_action(
    SiteSystemContext<ActionExecutionSystem>& context,
    TileCoord target_tile,
    std::uint64_t device_entity_id,
    const CraftRecipeDef& recipe_def)
{
    const auto* structure_def = find_structure_def(recipe_def.station_structure_id);
    if (structure_def == nullptr || !structure_def->grants_storage || structure_def->storage_slot_count == 0U)
    {
        return false;
    }

    const auto source_containers =
        craft_logic::collect_nearby_source_containers(context.site_run, target_tile);
    const auto output_container =
        inventory_storage::find_device_storage_container(context.site_run, device_entity_id);
    if (!output_container.is_valid())
    {
        auto remaining = static_cast<std::uint32_t>(recipe_def.output_quantity);
        const auto max_stack = item_stack_size(recipe_def.output_item_id);
        for (std::uint16_t slot_index = 0U;
             slot_index < structure_def->storage_slot_count && remaining > 0U;
             ++slot_index)
        {
            remaining = remaining > max_stack ? remaining - max_stack : 0U;
        }
        return remaining == 0U;
    }

    return craft_logic::can_store_output_after_recipe_consumption(
        context.site_run,
        output_container,
        source_containers,
        recipe_def);
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

double current_action_total_duration_minutes(const ActionState& action_state) noexcept
{
    return std::max(action_state.total_action_minutes, k_minimum_action_duration_minutes);
}

float resolve_action_progress_scale(
    SiteSystemContext<ActionExecutionSystem>& context,
    const ActionState& action_state) noexcept
{
    static_cast<void>(context);
    static_cast<void>(action_state);
    return 1.0f;
}

double action_elapsed_minutes_for_step(
    SiteSystemContext<ActionExecutionSystem>& context,
    const ActionState& action_state) noexcept
{
    return std::max(0.0, context.fixed_step_seconds) *
        k_runtime_minutes_per_real_second *
        static_cast<double>(resolve_action_progress_scale(context, action_state));
}

std::optional<TileCoord> resolve_action_approach_tile(
    const SiteRunState& site_run,
    const SiteWorld::WorkerData& worker,
    TileCoord target_tile,
    bool interaction_range_allowed)
{
    if (interaction_range_allowed)
    {
        return device_interaction_logic::resolve_interaction_range_approach_tile(
            site_run,
            worker,
            target_tile);
    }

    return device_interaction_logic::resolve_direct_approach_tile(
        site_run,
        worker,
        target_tile);
}

bool action_target_supports_interaction_range(
    SiteSystemContext<ActionExecutionSystem>& context,
    TileCoord target_tile) noexcept
{
    if (!context.world.tile_coord_in_bounds(target_tile))
    {
        return false;
    }

    return context.world.read_tile(target_tile).device.structure_id.value != 0U;
}

bool worker_is_at_action_approach_tile(
    SiteSystemContext<ActionExecutionSystem>& context,
    const ActionState& action_state) noexcept
{
    if (!action_state.approach_tile.has_value())
    {
        return true;
    }

    return device_interaction_logic::worker_is_at_tile(
        context.world.read_worker(),
        *action_state.approach_tile);
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
        context.message_queue,
        action_state.current_action_id.value(),
        action_state.action_kind,
        action_state.request_flags,
        action_target_tile(action_state),
        action_state.primary_subject_id,
        static_cast<float>(action_state.remaining_action_minutes));
    emit_worker_meter_cost_request(
        context,
        action_state.current_action_id->value,
        action_state.action_kind,
        action_state.quantity);
    context.world.mark_projection_dirty(
        SITE_PROJECTION_UPDATE_WORKER | SITE_PROJECTION_UPDATE_HUD);
}

void emit_placement_reservation_requested(
    GameMessageQueue& queue,
    std::uint32_t action_id,
    ActionKind action_kind,
    TileCoord target_tile,
    PlacementReservationSubjectKind subject_kind,
    std::uint32_t subject_id)
{
    enqueue_message(
        queue,
        GameMessageType::PlacementReservationRequested,
        PlacementReservationRequestedMessage {
            action_id,
            target_tile.x,
            target_tile.y,
            placement_occupancy_layer(action_kind),
            subject_kind,
            {0U, 0U},
            subject_id});
}

PlacementReservationSubjectKind placement_reservation_subject_kind(
    ActionKind action_kind,
    std::uint32_t item_id) noexcept
{
    if (action_kind == ActionKind::Build)
    {
        return PlacementReservationSubjectKind::StructureType;
    }

    if (action_kind != ActionKind::Plant)
    {
        return PlacementReservationSubjectKind::None;
    }

    const auto* item_def = action_item_def(action_kind, item_id);
    return item_def == nullptr
        ? PlacementReservationSubjectKind::GroundCoverType
        : PlacementReservationSubjectKind::PlantType;
}

std::uint32_t placement_reservation_subject_id(
    ActionKind action_kind,
    std::uint32_t item_id,
    std::uint32_t primary_subject_id) noexcept
{
    if (action_kind == ActionKind::Build)
    {
        const auto* item_def = action_item_def(action_kind, item_id);
        return item_def == nullptr ? 0U : item_def->linked_structure_id.value;
    }

    if (action_kind != ActionKind::Plant)
    {
        return primary_subject_id;
    }

    const auto* item_def = action_item_def(action_kind, item_id);
    return item_def == nullptr ? primary_subject_id : item_def->linked_plant_id.value;
}

TileFootprint resolve_action_placement_footprint(
    ActionKind action_kind,
    std::uint32_t item_id,
    std::uint32_t primary_subject_id) noexcept
{
    return resolve_placement_reservation_footprint(
        placement_occupancy_layer(action_kind),
        placement_reservation_subject_kind(action_kind, item_id),
        placement_reservation_subject_id(action_kind, item_id, primary_subject_id));
}

TileCoord align_action_target_tile(
    ActionKind action_kind,
    std::uint32_t item_id,
    std::uint32_t primary_subject_id,
    TileCoord target_tile) noexcept
{
    if (!should_request_placement_reservation(action_kind))
    {
        return target_tile;
    }

    return align_tile_anchor_to_footprint(
        target_tile,
        resolve_action_placement_footprint(
            action_kind,
            item_id,
            primary_subject_id));
}

TileCoord resolve_initial_placement_mode_tile(
    SiteSystemContext<ActionExecutionSystem>& context,
    TileCoord requested_target_tile) noexcept
{
    if (context.world.tile_coord_in_bounds(requested_target_tile))
    {
        return requested_target_tile;
    }

    const auto worker_tile = context.world.read_worker().position.tile_coord;
    return context.world.tile_coord_in_bounds(worker_tile)
        ? worker_tile
        : TileCoord {};
}

void update_placement_mode_preview(
    SiteSystemContext<ActionExecutionSystem>& context,
    PlacementModeState& placement_mode,
    TileCoord target_tile)
{
    const TileCoord aligned_target_tile = align_tile_anchor_to_footprint(
        target_tile,
        resolve_action_placement_footprint(
            placement_mode.action_kind,
            placement_mode.item_id,
            placement_mode.primary_subject_id));
    const auto preview = build_placement_preview(
        context.world,
        aligned_target_tile,
        placement_occupancy_layer(placement_mode.action_kind),
        placement_reservation_subject_kind(
            placement_mode.action_kind,
            placement_mode.item_id),
        placement_reservation_subject_id(
            placement_mode.action_kind,
            placement_mode.item_id,
            placement_mode.primary_subject_id));
    placement_mode.target_tile = aligned_target_tile;
    placement_mode.footprint_width = preview.footprint.width;
    placement_mode.footprint_height = preview.footprint.height;
    placement_mode.blocked_mask = preview.blocked_mask;
}

bool placement_mode_can_commit_to_tile(
    SiteSystemContext<ActionExecutionSystem>& context,
    PlacementModeState& placement_mode,
    TileCoord target_tile)
{
    update_placement_mode_preview(context, placement_mode, target_tile);
    return placement_mode.target_tile.has_value() &&
        context.world.tile_coord_in_bounds(*placement_mode.target_tile) &&
        placement_mode.blocked_mask == 0ULL;
}

bool placement_mode_matches_request(
    const PlacementModeState& placement_mode,
    ActionKind action_kind,
    std::uint16_t quantity,
    std::uint32_t primary_subject_id,
    std::uint32_t secondary_subject_id,
    std::uint32_t item_id) noexcept
{
    return placement_mode.active &&
        placement_mode.action_kind == action_kind &&
        placement_mode.quantity == quantity &&
        placement_mode.primary_subject_id == primary_subject_id &&
        placement_mode.secondary_subject_id == secondary_subject_id &&
        placement_mode.item_id == item_id;
}

void emit_placement_reservation_released(
    GameMessageQueue& queue,
    const ActionState& action_state)
{
    if (!action_state.current_action_id.has_value())
    {
        return;
    }

    enqueue_message(
        queue,
        GameMessageType::PlacementReservationReleased,
        PlacementReservationReleasedMessage {
            action_state.current_action_id->value,
            action_state.placement_reservation_token});
}

void emit_placement_mode_commit_rejected(
    GameMessageQueue& queue,
    const PlacementModeState& placement_mode,
    TileCoord target_tile)
{
    const TileCoord rejected_tile = placement_mode.target_tile.value_or(target_tile);
    enqueue_message(
        queue,
        GameMessageType::PlacementModeCommitRejected,
        PlacementModeCommitRejectedMessage {
            rejected_tile.x,
            rejected_tile.y,
            placement_mode.blocked_mask,
            to_gs1_action_kind(placement_mode.action_kind),
            placement_mode.item_id});
}

void emit_action_fact_messages(GameMessageQueue& queue, const ActionState& action_state)
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
            enqueue_message(
                queue,
                GameMessageType::InventoryItemConsumeRequested,
                InventoryItemConsumeRequestedMessage {
                    item_def->item_id.value,
                    safe_quantity,
                    GS1_INVENTORY_CONTAINER_WORKER_PACK,
                    k_inventory_item_consume_flag_ignore_action_reservations});
            enqueue_message(
                queue,
                GameMessageType::SiteTilePlantingCompleted,
                SiteTilePlantingCompletedMessage {
                    action_id,
                    target_tile.x,
                    target_tile.y,
                    item_def->linked_plant_id.value,
                    std::min(1.0f, 0.2f * static_cast<float>(safe_quantity)),
                    flags});
        }
        else
        {
            enqueue_message(
                queue,
                GameMessageType::SiteGroundCoverPlaced,
                SiteGroundCoverPlacedMessage {
                    action_id,
                    target_tile.x,
                    target_tile.y,
                    action_state.primary_subject_id == 0U ? 1U : action_state.primary_subject_id,
                    std::min(1.0f, 0.25f * static_cast<float>(safe_quantity)),
                    flags});
        }
        break;

    case ActionKind::Water:
        enqueue_message(
            queue,
            GameMessageType::SiteTileWatered,
            SiteTileWateredMessage {
                action_id,
                target_tile.x,
                target_tile.y,
                static_cast<float>(action_state.quantity == 0U ? 1U : action_state.quantity),
                flags});
        break;

    case ActionKind::ClearBurial:
        enqueue_message(
            queue,
            GameMessageType::SiteTileBurialCleared,
            SiteTileBurialClearedMessage {
                action_id,
                target_tile.x,
                target_tile.y,
                0.35f * static_cast<float>(action_state.quantity == 0U ? 1U : action_state.quantity),
                flags});
        break;

    case ActionKind::Build:
        if (const auto* item_def = action_item_def(
                action_state.action_kind,
                action_state.item_id))
        {
            enqueue_message(
                queue,
                GameMessageType::InventoryItemConsumeRequested,
                InventoryItemConsumeRequestedMessage {
                    item_def->item_id.value,
                    safe_quantity,
                    GS1_INVENTORY_CONTAINER_WORKER_PACK,
                    k_inventory_item_consume_flag_ignore_action_reservations});
            enqueue_message(
                queue,
                GameMessageType::SiteDevicePlaced,
                SiteDevicePlacedMessage {
                    action_id,
                    target_tile.x,
                    target_tile.y,
                    item_def->linked_structure_id.value,
                    flags});
        }
        break;

    case ActionKind::Drink:
    case ActionKind::Eat:
        if (const auto* item_def = action_item_def(
                action_state.action_kind,
                action_state.item_id))
        {
            enqueue_message(
                queue,
                GameMessageType::InventoryItemConsumeRequested,
                InventoryItemConsumeRequestedMessage {
                    item_def->item_id.value,
                    safe_quantity,
                    reserved_item_container_kind(action_state),
                    k_inventory_item_consume_flag_ignore_action_reservations});
            emit_item_meter_delta_request(queue, *item_def, safe_quantity);
        }
        break;

    case ActionKind::Craft:
        if (action_state.secondary_subject_id != 0U)
        {
            enqueue_message(
                queue,
                GameMessageType::InventoryCraftCommitRequested,
                InventoryCraftCommitRequestedMessage {
                    action_state.secondary_subject_id,
                    target_tile.x,
                    target_tile.y,
                    flags});
        }
        break;

    case ActionKind::Repair:
        enqueue_message(
            queue,
            GameMessageType::SiteDeviceRepaired,
            SiteDeviceRepairedMessage {
                action_id,
                target_tile.x,
                target_tile.y,
                flags});
        break;

    case ActionKind::None:
    default:
        break;
    }
}

SiteActionFailureReason validate_craft_completion(
    SiteSystemContext<ActionExecutionSystem>& context,
    const ActionState& action_state)
{
    if (!action_state.target_tile.has_value() || action_state.secondary_subject_id == 0U)
    {
        return SiteActionFailureReason::InvalidState;
    }

    const auto* recipe_def = find_craft_recipe_def(RecipeId {action_state.secondary_subject_id});
    if (recipe_def == nullptr)
    {
        return SiteActionFailureReason::InvalidState;
    }

    if (!context.world.tile_coord_in_bounds(*action_state.target_tile) || context.site_run.site_world == nullptr)
    {
        return SiteActionFailureReason::InvalidTarget;
    }

    const auto tile = context.world.read_tile(*action_state.target_tile);
    if (tile.device.structure_id != recipe_def->station_structure_id)
    {
        return SiteActionFailureReason::InvalidTarget;
    }

    const auto device_entity_id = context.site_run.site_world->device_entity_id(*action_state.target_tile);
    if (device_entity_id == 0U)
    {
        return SiteActionFailureReason::InvalidTarget;
    }

    if (!craft_ingredients_available_for_action(
            context,
            *action_state.target_tile,
            device_entity_id,
            *recipe_def))
    {
        return SiteActionFailureReason::InsufficientResources;
    }

    if (!craft_output_fits_for_action(
            context,
            *action_state.target_tile,
            device_entity_id,
            *recipe_def))
    {
        return SiteActionFailureReason::InvalidState;
    }

    return SiteActionFailureReason::None;
}

SiteActionFailureReason validate_repair_completion(
    SiteSystemContext<ActionExecutionSystem>& context,
    const ActionState& action_state)
{
    if (!action_state.target_tile.has_value())
    {
        return SiteActionFailureReason::InvalidState;
    }

    return validate_repair_target(context, *action_state.target_tile);
}

}  // namespace

bool ActionExecutionSystem::subscribes_to(GameMessageType type) noexcept
{
    switch (type)
    {
    case GameMessageType::StartSiteAction:
    case GameMessageType::CancelSiteAction:
    case GameMessageType::PlacementModeCursorMoved:
    case GameMessageType::PlacementReservationAccepted:
    case GameMessageType::PlacementReservationRejected:
        return true;
    default:
        return false;
    }
}

Gs1Status ActionExecutionSystem::process_message(
    SiteSystemContext<ActionExecutionSystem>& context,
    const GameMessage& message)
{
    auto& action_state = context.world.own_action();
    const auto& payload_type = message.type;
    switch (payload_type)
    {
    case GameMessageType::StartSiteAction:
    {
        const auto& payload = message.payload_as<StartSiteActionMessage>();
        const ActionKind action_kind = to_action_kind(payload.action_kind);
        const bool deferred_target_selection =
            (payload.flags & GS1_SITE_ACTION_REQUEST_FLAG_DEFERRED_TARGET_SELECTION) != 0U;
        const std::uint8_t request_flags = static_cast<std::uint8_t>(
            payload.flags & ~GS1_SITE_ACTION_REQUEST_FLAG_DEFERRED_TARGET_SELECTION);
        const std::uint16_t requested_quantity = payload.quantity == 0U ? 1U : payload.quantity;
        TileCoord target_tile {payload.target_tile_x, payload.target_tile_y};
        std::uint32_t primary_subject_id = payload.primary_subject_id;
        std::uint32_t secondary_subject_id = payload.secondary_subject_id;
        std::uint32_t item_id = payload.item_id;
        bool reactivate_plant_placement_mode_on_completion = false;

        if (has_active_placement_mode(action_state))
        {
            if (deferred_target_selection)
            {
                emit_site_action_failed(
                    context.message_queue,
                    0U,
                    action_kind,
                    SiteActionFailureReason::Busy,
                    request_flags,
                    target_tile,
                    primary_subject_id,
                    secondary_subject_id);
                return GS1_STATUS_OK;
            }

            if (!placement_mode_matches_request(
                    action_state.placement_mode,
                    action_kind,
                    requested_quantity,
                    primary_subject_id,
                    secondary_subject_id,
                    item_id))
            {
                emit_site_action_failed(
                    context.message_queue,
                    0U,
                    action_kind,
                    SiteActionFailureReason::Busy,
                    request_flags,
                    target_tile,
                    primary_subject_id,
                    secondary_subject_id);
                return GS1_STATUS_OK;
            }

            if (!placement_mode_can_commit_to_tile(
                    context,
                    action_state.placement_mode,
                    target_tile))
            {
                emit_placement_mode_commit_rejected(
                    context.message_queue,
                    action_state.placement_mode,
                    target_tile);
                context.world.mark_projection_dirty(SITE_PROJECTION_UPDATE_PLACEMENT_PREVIEW);
                return GS1_STATUS_OK;
            }

            const auto placement_mode = action_state.placement_mode;
            reactivate_plant_placement_mode_on_completion =
                placement_mode.action_kind == ActionKind::Plant &&
                placement_mode.item_id != 0U;
            clear_placement_mode(action_state.placement_mode);
            context.world.mark_projection_dirty(SITE_PROJECTION_UPDATE_PLACEMENT_PREVIEW);

            target_tile = placement_mode.target_tile.value_or(target_tile);
            primary_subject_id = placement_mode.primary_subject_id;
            secondary_subject_id = placement_mode.secondary_subject_id;
            item_id = placement_mode.item_id;
        }
        else if (has_active_action(action_state))
        {
            emit_site_action_failed(
                context.message_queue,
                0U,
                action_kind,
                SiteActionFailureReason::Busy,
                request_flags,
                target_tile,
                primary_subject_id,
                secondary_subject_id);
            return GS1_STATUS_OK;
        }

        if (action_kind == ActionKind::None)
        {
            emit_site_action_failed(
                context.message_queue,
                0U,
                action_kind,
                SiteActionFailureReason::InvalidState,
                request_flags,
                target_tile,
                primary_subject_id,
                secondary_subject_id);
            return GS1_STATUS_OK;
        }

        if (deferred_target_selection &&
            !action_supports_deferred_target_selection(action_kind))
        {
            emit_site_action_failed(
                context.message_queue,
                0U,
                action_kind,
                SiteActionFailureReason::InvalidState,
                request_flags,
                target_tile,
                primary_subject_id,
                secondary_subject_id);
            return GS1_STATUS_OK;
        }

        const ItemId repair_tool_id = action_kind == ActionKind::Repair
            ? repair_tool_item_id(item_id)
            : ItemId {};

        if (action_requires_item(action_kind, item_id))
        {
            const auto* item_def = action_item_def(action_kind, item_id);
            if (item_def == nullptr)
            {
                emit_site_action_failed(
                    context.message_queue,
                    0U,
                    action_kind,
                    SiteActionFailureReason::InvalidState,
                    request_flags,
                    target_tile,
                    primary_subject_id,
                    secondary_subject_id);
                return GS1_STATUS_OK;
            }

            const auto required_quantity = requested_quantity;
            if (available_worker_pack_item_quantity(context.world.read_inventory(), item_def->item_id) <
                required_quantity)
            {
                emit_site_action_failed(
                    context.message_queue,
                    0U,
                    action_kind,
                    SiteActionFailureReason::InsufficientResources,
                    request_flags,
                    target_tile,
                    primary_subject_id,
                    secondary_subject_id);
                return GS1_STATUS_OK;
            }
        }

        if (deferred_target_selection)
        {
            auto& placement_mode = action_state.placement_mode;
            clear_placement_mode(placement_mode);
            placement_mode.active = true;
            placement_mode.action_kind = action_kind;
            placement_mode.primary_subject_id = primary_subject_id;
            placement_mode.secondary_subject_id = secondary_subject_id;
            placement_mode.item_id = item_id;
            placement_mode.quantity = requested_quantity;
            placement_mode.request_flags = request_flags;
            update_placement_mode_preview(
                context,
                placement_mode,
                resolve_initial_placement_mode_tile(context, target_tile));
            context.world.mark_projection_dirty(SITE_PROJECTION_UPDATE_PLACEMENT_PREVIEW);
            return GS1_STATUS_OK;
        }

        if (!context.world.tile_coord_in_bounds(target_tile))
        {
            emit_site_action_failed(
                context.message_queue,
                0U,
                action_kind,
                SiteActionFailureReason::InvalidTarget,
                request_flags,
                target_tile,
                primary_subject_id,
                secondary_subject_id);
            return GS1_STATUS_OK;
        }

        target_tile = align_action_target_tile(
            action_kind,
            item_id,
            primary_subject_id,
            target_tile);

        if (action_kind == ActionKind::Repair)
        {
            const auto repair_failure_reason = validate_repair_target(context, target_tile);
            if (repair_failure_reason != SiteActionFailureReason::None)
            {
                emit_site_action_failed(
                    context.message_queue,
                    0U,
                    action_kind,
                    repair_failure_reason,
                    request_flags,
                    target_tile,
                    primary_subject_id,
                    secondary_subject_id);
                return GS1_STATUS_OK;
            }

            if (repair_tool_id.value == 0U)
            {
                emit_site_action_failed(
                    context.message_queue,
                    0U,
                    action_kind,
                    SiteActionFailureReason::InvalidState,
                    request_flags,
                    target_tile,
                    primary_subject_id,
                    secondary_subject_id);
                return GS1_STATUS_OK;
            }

            if (available_worker_pack_item_quantity(context.world.read_inventory(), repair_tool_id) == 0U)
            {
                emit_site_action_failed(
                    context.message_queue,
                    0U,
                    action_kind,
                    SiteActionFailureReason::InsufficientResources,
                    request_flags,
                    target_tile,
                    primary_subject_id,
                    secondary_subject_id);
                return GS1_STATUS_OK;
            }
        }

        const CraftRecipeDef* craft_recipe = nullptr;
        std::uint64_t craft_device_entity_id = 0U;
        if (action_kind == ActionKind::Craft)
        {
            craft_recipe = resolve_craft_recipe_for_action(
                context,
                target_tile,
                item_id,
                &craft_device_entity_id);
            if (craft_recipe == nullptr || craft_device_entity_id == 0U)
            {
                emit_site_action_failed(
                    context.message_queue,
                    0U,
                    action_kind,
                    SiteActionFailureReason::InvalidTarget,
                    request_flags,
                    target_tile,
                    primary_subject_id,
                    secondary_subject_id);
                return GS1_STATUS_OK;
            }

            if (!craft_ingredients_available_for_action(
                    context,
                    target_tile,
                    craft_device_entity_id,
                    *craft_recipe))
            {
                emit_site_action_failed(
                    context.message_queue,
                    0U,
                    action_kind,
                    SiteActionFailureReason::InsufficientResources,
                    request_flags,
                    target_tile,
                    primary_subject_id,
                    secondary_subject_id);
                return GS1_STATUS_OK;
            }
        }

        const double duration_minutes = action_kind == ActionKind::Craft && craft_recipe != nullptr
            ? std::max(
                  static_cast<double>(craft_recipe->craft_minutes),
                  k_minimum_action_duration_minutes)
            : compute_duration_minutes(
                  context,
                  action_kind,
                  requested_quantity,
                  item_id);
        const RuntimeActionId action_id = allocate_runtime_action_id();

        action_state.current_action_id = action_id;
        action_state.action_kind = action_kind;
        action_state.target_tile = target_tile;
        action_state.approach_tile.reset();
        action_state.primary_subject_id = primary_subject_id;
        action_state.secondary_subject_id =
            action_kind == ActionKind::Craft && craft_recipe != nullptr
            ? craft_recipe->recipe_id.value
            : secondary_subject_id;
        action_state.item_id = action_kind == ActionKind::Repair
            ? repair_tool_id.value
            : item_id;
        action_state.quantity =
            action_kind == ActionKind::Craft
            ? 1U
            : requested_quantity;
        action_state.request_flags = request_flags;
        action_state.reactivate_placement_mode_on_completion =
            reactivate_plant_placement_mode_on_completion;
        action_state.total_action_minutes = duration_minutes;
        action_state.remaining_action_minutes = duration_minutes;
        action_state.started_at_world_minute.reset();
        action_state.awaiting_placement_reservation =
            should_request_placement_reservation(action_kind);
        populate_reserved_input_items(action_state);

        if (action_requires_worker_approach(action_kind))
        {
            const auto worker = context.world.read_worker();
            const auto approach_tile =
                resolve_action_approach_tile(
                    context.site_run,
                    worker,
                    target_tile,
                    action_target_supports_interaction_range(context, target_tile));
            if (!approach_tile.has_value())
            {
                clear_action_state(action_state);
                emit_site_action_failed(
                    context.message_queue,
                    0U,
                    action_kind,
                    SiteActionFailureReason::InvalidTarget,
                    request_flags,
                    target_tile,
                    primary_subject_id,
                    secondary_subject_id);
                return GS1_STATUS_OK;
            }

            action_state.approach_tile = approach_tile;
        }

        if (action_state.awaiting_placement_reservation)
        {
            emit_placement_reservation_requested(
                context.message_queue,
                action_id.value,
                action_kind,
                target_tile,
                placement_reservation_subject_kind(
                    action_kind,
                    action_state.item_id),
                placement_reservation_subject_id(
                    action_kind,
                    action_state.item_id,
                    primary_subject_id));
        }
        else if (!action_requires_worker_approach(action_kind) ||
            worker_is_at_action_approach_tile(context, action_state))
        {
            begin_action_execution(context, action_state);
        }

        context.world.mark_projection_dirty(
            SITE_PROJECTION_UPDATE_WORKER | SITE_PROJECTION_UPDATE_HUD);
        return GS1_STATUS_OK;
    }

    case GameMessageType::CancelSiteAction:
    {
        const auto& payload = message.payload_as<CancelSiteActionMessage>();
        const bool cancels_current =
            (payload.flags & GS1_SITE_ACTION_CANCEL_FLAG_CURRENT_ACTION) != 0U;
        const bool cancels_placement_mode =
            (payload.flags & GS1_SITE_ACTION_CANCEL_FLAG_PLACEMENT_MODE) != 0U;

        if (!has_active_action(action_state))
        {
            if (has_active_placement_mode(action_state) &&
                (cancels_current || cancels_placement_mode))
            {
                clear_placement_mode(action_state.placement_mode);
                context.world.mark_projection_dirty(SITE_PROJECTION_UPDATE_PLACEMENT_PREVIEW);
            }
            return GS1_STATUS_OK;
        }

        const bool matches_id =
            action_state.current_action_id.has_value() &&
            payload.action_id == action_state.current_action_id->value;

        if (!cancels_current && !matches_id)
        {
            return GS1_STATUS_OK;
        }

        emit_placement_reservation_released(context.message_queue, action_state);
        emit_site_action_failed(
            context.message_queue,
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

    case GameMessageType::PlacementModeCursorMoved:
    {
        if (!has_active_placement_mode(action_state))
        {
            return GS1_STATUS_OK;
        }

        const auto& payload = message.payload_as<PlacementModeCursorMovedMessage>();
        const TileCoord target_tile {payload.tile_x, payload.tile_y};
        update_placement_mode_preview(
            context,
            action_state.placement_mode,
            target_tile);
        context.world.mark_projection_dirty(SITE_PROJECTION_UPDATE_PLACEMENT_PREVIEW);
        return GS1_STATUS_OK;
    }

    case GameMessageType::PlacementReservationAccepted:
    {
        if (!is_action_waiting_for_placement(action_state))
        {
            return GS1_STATUS_OK;
        }

        const auto& payload = message.payload_as<PlacementReservationAcceptedMessage>();
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

    case GameMessageType::PlacementReservationRejected:
    {
        if (!is_action_waiting_for_placement(action_state))
        {
            return GS1_STATUS_OK;
        }

        const auto& payload = message.payload_as<PlacementReservationRejectedMessage>();
        if (payload.action_id != action_id_value(action_state))
        {
            return GS1_STATUS_OK;
        }

        emit_site_action_failed(
            context.message_queue,
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

    action_state.remaining_action_minutes =
        std::max(
            0.0,
            action_state.remaining_action_minutes -
                action_elapsed_minutes_for_step(context, action_state));

    if (action_state.remaining_action_minutes > 0.0)
    {
        return;
    }

    if (action_state.action_kind == ActionKind::Craft)
    {
        const auto failure_reason = validate_craft_completion(context, action_state);
        if (failure_reason != SiteActionFailureReason::None)
        {
            emit_site_action_failed(
                context.message_queue,
                action_id_value(action_state),
                action_state.action_kind,
                failure_reason,
                action_state.request_flags,
                action_target_tile(action_state),
                action_state.primary_subject_id,
                action_state.secondary_subject_id);
            emit_placement_reservation_released(context.message_queue, action_state);
            clear_action_state(action_state);
            context.world.mark_projection_dirty(
                SITE_PROJECTION_UPDATE_WORKER | SITE_PROJECTION_UPDATE_HUD);
            return;
        }
    }
    else if (action_state.action_kind == ActionKind::Repair)
    {
        const auto failure_reason = validate_repair_completion(context, action_state);
        if (failure_reason != SiteActionFailureReason::None)
        {
            emit_site_action_failed(
                context.message_queue,
                action_id_value(action_state),
                action_state.action_kind,
                failure_reason,
                action_state.request_flags,
                action_target_tile(action_state),
                action_state.primary_subject_id,
                action_state.secondary_subject_id);
            emit_placement_reservation_released(context.message_queue, action_state);
            clear_action_state(action_state);
            context.world.mark_projection_dirty(
                SITE_PROJECTION_UPDATE_WORKER | SITE_PROJECTION_UPDATE_HUD);
            return;
        }
    }

    const auto item_failure_reason = validate_reserved_item_completion(context, action_state);
    if (item_failure_reason != SiteActionFailureReason::None)
    {
        emit_site_action_failed(
            context.message_queue,
            action_id_value(action_state),
            action_state.action_kind,
            item_failure_reason,
            action_state.request_flags,
            action_target_tile(action_state),
            action_state.primary_subject_id,
            action_state.secondary_subject_id);
        emit_placement_reservation_released(context.message_queue, action_state);
        clear_action_state(action_state);
        context.world.mark_projection_dirty(
            SITE_PROJECTION_UPDATE_WORKER | SITE_PROJECTION_UPDATE_HUD);
        return;
    }

    emit_site_action_completed(context.message_queue, action_state);
    emit_action_fact_messages(context.message_queue, action_state);
    emit_placement_reservation_released(context.message_queue, action_state);
    const bool reactivated_placement_mode =
        should_reactivate_plant_placement_mode_after_completion(
            context,
            action_state);
    if (reactivated_placement_mode)
    {
        reactivate_plant_placement_mode_from_completed_action(action_state);
    }
    clear_action_state(action_state);
    std::uint32_t dirty_flags =
        SITE_PROJECTION_UPDATE_WORKER |
        SITE_PROJECTION_UPDATE_HUD;
    if (reactivated_placement_mode)
    {
        dirty_flags |= SITE_PROJECTION_UPDATE_PLACEMENT_PREVIEW;
    }
    context.world.mark_projection_dirty(dirty_flags);
}
}  // namespace gs1
