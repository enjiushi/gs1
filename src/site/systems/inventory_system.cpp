#include "site/systems/inventory_system.h"

#include "campaign/campaign_state.h"
#include "content/defs/craft_recipe_defs.h"
#include "content/defs/item_defs.h"
#include "content/defs/structure_defs.h"
#include "site/craft_logic.h"
#include "site/inventory_state.h"
#include "site/inventory_storage.h"
#include "site/site_projection_update_flags.h"
#include "site/site_run_state.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace gs1
{
namespace
{
constexpr double k_default_delivery_minutes = 30.0;
constexpr double k_site_minutes_per_step = 0.2;

std::uint32_t normalize_quantity(std::uint32_t quantity) noexcept
{
    return quantity == 0U ? 1U : quantity;
}

bool has_supported_inventory_transfer_route(const InventoryTransferRequestedCommand& payload) noexcept
{
    const bool source_is_worker_pack =
        payload.source_container_kind == GS1_INVENTORY_CONTAINER_WORKER_PACK;
    const bool destination_is_worker_pack =
        payload.destination_container_kind == GS1_INVENTORY_CONTAINER_WORKER_PACK;
    const bool source_is_device_storage =
        payload.source_container_kind == GS1_INVENTORY_CONTAINER_DEVICE_STORAGE;
    const bool destination_is_device_storage =
        payload.destination_container_kind == GS1_INVENTORY_CONTAINER_DEVICE_STORAGE;

    return (source_is_worker_pack && destination_is_worker_pack) ||
        (source_is_worker_pack && destination_is_device_storage) ||
        (source_is_device_storage && destination_is_worker_pack);
}

bool transfer_resolves_destination_in_dll(const InventoryTransferRequestedCommand& payload) noexcept
{
    return (payload.flags & k_inventory_transfer_flag_resolve_destination_in_dll) != 0U;
}

bool inventory_has_any_item(const InventoryState& inventory) noexcept
{
    const auto has_item = [](const InventorySlot& slot) { return slot.occupied; };
    return std::any_of(
        inventory.worker_pack_slots.begin(),
        inventory.worker_pack_slots.end(),
        has_item);
}

bool storage_has_any_item(SiteRunState& site_run) noexcept
{
    const auto containers = inventory_storage::collect_all_storage_containers(site_run, true);
    for (const auto& container : containers)
    {
        const auto slot_count = inventory_storage::slot_count_in_container(site_run, container);
        for (std::uint32_t slot_index = 0U; slot_index < slot_count; ++slot_index)
        {
            const auto slot = inventory_storage::slot_entity(site_run, container, slot_index);
            const auto item = inventory_storage::item_entity_for_slot(site_run, slot);
            const auto* stack = inventory_storage::stack_data(site_run, item);
            if (stack != nullptr && stack->quantity > 0U)
            {
                return true;
            }
        }
    }

    return false;
}

bool ensure_device_storage_containers(SiteSystemContext<InventorySystem>& context) noexcept;

bool ensure_inventory_storage_initialized(SiteSystemContext<InventorySystem>& context) noexcept
{
    auto& inventory = context.world.own_inventory();
    const bool resized =
        inventory.worker_pack_slots.size() != inventory.worker_pack_slot_count ||
        !inventory_storage::storage_initialized(context.site_run);

    inventory_storage::import_projection_views_into_storage_if_needed(context.site_run);
    (void)ensure_device_storage_containers(context);
    return resized;
}

bool ensure_device_storage_containers(SiteSystemContext<InventorySystem>& context) noexcept
{
    if (!context.world.has_world() || context.site_run.site_world == nullptr)
    {
        return false;
    }

    bool created_any = false;
    const auto tile_count = context.world.tile_count();
    for (std::size_t index = 0; index < tile_count; ++index)
    {
        const auto coord = context.world.tile_coord(index);
        const auto tile = context.world.read_tile_at_index(index);
        const auto* structure_def = find_structure_def(tile.device.structure_id);
        if (structure_def == nullptr || !structure_def->grants_storage || structure_def->storage_slot_count == 0U)
        {
            continue;
        }

        const auto device_entity_id = context.site_run.site_world->device_entity_id(coord);
        if (device_entity_id == 0U)
        {
            continue;
        }

        const auto existing =
            inventory_storage::find_device_storage_container(context.site_run, device_entity_id);
        (void)inventory_storage::ensure_device_storage_container(
            context.site_run,
            device_entity_id,
            coord,
            structure_def->storage_slot_count);
        if (!existing.is_valid())
        {
            created_any = true;
        }
    }

    if (created_any)
    {
        context.world.mark_projection_dirty(SITE_PROJECTION_UPDATE_INVENTORY);
    }

    return created_any;
}

flecs::entity resolve_starter_storage_container(SiteSystemContext<InventorySystem>& context) noexcept
{
    if (!context.world.has_world() || context.site_run.site_world == nullptr)
    {
        return {};
    }

    const auto tile = context.world.read_camp().starter_storage_tile;
    if (!context.world.tile_coord_in_bounds(tile))
    {
        return {};
    }

    const auto tile_data = context.world.read_tile(tile);
    const auto* structure_def = find_structure_def(tile_data.device.structure_id);
    if (structure_def == nullptr || !structure_def->grants_storage || structure_def->storage_slot_count == 0U)
    {
        return {};
    }

    const auto device_entity_id = context.site_run.site_world->device_entity_id(tile);
    if (device_entity_id == 0U)
    {
        return {};
    }

    return inventory_storage::ensure_device_storage_container(
        context.site_run,
        device_entity_id,
        tile,
        structure_def->storage_slot_count);
}

void mark_changed_slot_views(
    SiteSystemContext<InventorySystem>& context,
    const std::vector<InventorySlot>& before_worker) noexcept
{
    const auto& inventory = context.world.read_inventory();
    if (before_worker.size() != inventory.worker_pack_slots.size())
    {
        context.world.mark_projection_dirty(SITE_PROJECTION_UPDATE_INVENTORY);
        return;
    }

    for (std::size_t index = 0; index < before_worker.size(); ++index)
    {
        const auto& lhs = before_worker[index];
        const auto& rhs = inventory.worker_pack_slots[index];
        if (lhs.item_instance_id != rhs.item_instance_id ||
            lhs.item_id != rhs.item_id ||
            lhs.item_quantity != rhs.item_quantity ||
            lhs.occupied != rhs.occupied)
        {
            context.world.mark_inventory_slot_projection_dirty(
                GS1_INVENTORY_CONTAINER_WORKER_PACK,
                static_cast<std::uint32_t>(index));
        }
    }
}

template <typename Func>
Gs1Status mutate_inventory_storage(
    SiteSystemContext<InventorySystem>& context,
    Func&& func)
{
    const bool storage_changed = ensure_inventory_storage_initialized(context);
    const auto before_worker = context.world.read_inventory().worker_pack_slots;
    const auto status = func();
    if (status != GS1_STATUS_OK)
    {
        return status;
    }

    if (storage_changed)
    {
        context.world.mark_projection_dirty(SITE_PROJECTION_UPDATE_INVENTORY);
    }
    mark_changed_slot_views(context, before_worker);
    return GS1_STATUS_OK;
}

bool item_is_directly_usable(const ItemDef& item_def) noexcept
{
    return item_has_capability(item_def, ITEM_CAPABILITY_DRINK) ||
        item_has_capability(item_def, ITEM_CAPABILITY_EAT) ||
        item_has_capability(item_def, ITEM_CAPABILITY_USE_MEDICINE);
}

void emit_item_use_effect(
    SiteSystemContext<InventorySystem>& context,
    const ItemDef& item_def,
    std::uint32_t quantity) noexcept
{
    WorkerMeterDeltaRequestedCommand meter_delta {};
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

    GameCommand command {};
    command.type = GameCommandType::WorkerMeterDeltaRequested;
    command.set_payload(meter_delta);
    context.command_queue.push_back(command);
}

std::uint32_t allocate_delivery_id() noexcept
{
    static std::uint32_t next_delivery_id = 1U;
    return next_delivery_id++;
}

bool delivery_has_pending_items(const PendingDelivery& delivery) noexcept
{
    for (const auto& stack : delivery.item_stacks)
    {
        if (stack.occupied && stack.item_quantity > 0U)
        {
            return true;
        }
    }

    return false;
}

[[nodiscard]] bool item_prefers_worker_pack(ItemId item_id) noexcept
{
    const auto* item_def = find_item_def(item_id);
    return item_def != nullptr && item_is_directly_usable(*item_def);
}

void seed_inventory_from_loadout(SiteSystemContext<InventorySystem>& context) noexcept
{
    ensure_inventory_storage_initialized(context);
    if (inventory_has_any_item(context.world.read_inventory()) || storage_has_any_item(context.site_run))
    {
        return;
    }

    const auto& planner = context.campaign.loadout_planner_state;
    const auto& loadout_slots =
        planner.selected_loadout_slots.empty()
            ? planner.baseline_deployment_items
            : planner.selected_loadout_slots;
    if (loadout_slots.empty())
    {
        return;
    }

    const auto worker_pack = inventory_storage::worker_pack_container(context.site_run);
    const auto starter_storage = resolve_starter_storage_container(context);
    const auto default_storage = starter_storage.is_valid() ? starter_storage : worker_pack;

    for (const auto& slot : loadout_slots)
    {
        if (!slot.occupied || slot.item_id.value == 0U || slot.quantity == 0U)
        {
            continue;
        }

        const auto container =
            item_prefers_worker_pack(slot.item_id)
                ? worker_pack
                : default_storage;
        (void)inventory_storage::add_item_to_container(
            context.site_run,
            container,
            slot.item_id,
            slot.quantity);
    }

    context.world.mark_projection_dirty(SITE_PROJECTION_UPDATE_INVENTORY);
}

Gs1Status handle_inventory_item_use(
    SiteSystemContext<InventorySystem>& context,
    const InventoryItemUseRequestedCommand& payload) noexcept
{
    return mutate_inventory_storage(context, [&]() -> Gs1Status {
        const auto container =
            inventory_storage::container_for_kind(context.site_run, payload.container_kind);
        const auto slot = inventory_storage::slot_entity(
            context.site_run,
            container,
            static_cast<std::uint32_t>(payload.slot_index));
        const auto item_entity = inventory_storage::item_entity_for_slot(context.site_run, slot);
        const auto* stack = inventory_storage::stack_data(context.site_run, item_entity);
        if (stack == nullptr || stack->quantity == 0U)
        {
            return GS1_STATUS_INVALID_ARGUMENT;
        }

        if (payload.item_id != 0U &&
            payload.item_id != static_cast<std::uint32_t>(item_entity.id()) &&
            payload.item_id != stack->item_id.value)
        {
            return GS1_STATUS_INVALID_ARGUMENT;
        }

        const auto* item_def = find_item_def(stack->item_id);
        if (item_def == nullptr)
        {
            return GS1_STATUS_NOT_FOUND;
        }

        if (!item_is_directly_usable(*item_def))
        {
            return GS1_STATUS_INVALID_STATE;
        }

        const auto requested_quantity = normalize_quantity(payload.quantity);
        if (requested_quantity > stack->quantity)
        {
            return GS1_STATUS_INVALID_ARGUMENT;
        }

        emit_item_use_effect(context, *item_def, requested_quantity);
        if (!inventory_storage::consume_quantity_from_item_entity(
                context.site_run,
                item_entity,
                requested_quantity))
        {
            return GS1_STATUS_INVALID_STATE;
        }

        if (payload.container_kind == GS1_INVENTORY_CONTAINER_DEVICE_STORAGE)
        {
            context.world.mark_projection_dirty(SITE_PROJECTION_UPDATE_INVENTORY);
        }
        context.world.mark_projection_dirty(SITE_PROJECTION_UPDATE_HUD);
        return GS1_STATUS_OK;
    });
}

Gs1Status handle_inventory_item_consume(
    SiteSystemContext<InventorySystem>& context,
    const InventoryItemConsumeRequestedCommand& payload) noexcept
{
    return mutate_inventory_storage(context, [&]() -> Gs1Status {
        const ItemId item_id {payload.item_id};
        const auto quantity = normalize_quantity(payload.quantity);
        if (find_item_def(item_id) == nullptr)
        {
            return GS1_STATUS_NOT_FOUND;
        }

        if (inventory_storage::available_item_quantity_in_container_kind(
                context.site_run,
                payload.container_kind,
                item_id) < quantity)
        {
            return GS1_STATUS_INVALID_STATE;
        }

        const auto remaining = inventory_storage::consume_item_type_from_container(
            context.site_run,
            inventory_storage::container_for_kind(context.site_run, payload.container_kind),
            item_id,
            quantity);
        if (payload.container_kind == GS1_INVENTORY_CONTAINER_DEVICE_STORAGE)
        {
            context.world.mark_projection_dirty(SITE_PROJECTION_UPDATE_INVENTORY);
        }
        return remaining == 0U ? GS1_STATUS_OK : GS1_STATUS_INVALID_STATE;
    });
}

Gs1Status handle_inventory_global_item_consume(
    SiteSystemContext<InventorySystem>& context,
    const InventoryGlobalItemConsumeRequestedCommand& payload) noexcept
{
    return mutate_inventory_storage(context, [&]() -> Gs1Status {
        const ItemId item_id {payload.item_id};
        const auto quantity = normalize_quantity(payload.quantity);
        if (find_item_def(item_id) == nullptr)
        {
            return GS1_STATUS_NOT_FOUND;
        }

        const auto containers =
            inventory_storage::collect_all_storage_containers(context.site_run, true);
        if (inventory_storage::available_item_quantity_in_containers(
                context.site_run,
                containers,
                item_id) < quantity)
        {
            return GS1_STATUS_INVALID_STATE;
        }

        const auto remaining = inventory_storage::consume_item_type_from_containers(
            context.site_run,
            containers,
            item_id,
            quantity);
        context.world.mark_projection_dirty(SITE_PROJECTION_UPDATE_INVENTORY);
        return remaining == 0U ? GS1_STATUS_OK : GS1_STATUS_INVALID_STATE;
    });
}

Gs1Status handle_inventory_craft_commit(
    SiteSystemContext<InventorySystem>& context,
    const InventoryCraftCommitRequestedCommand& payload) noexcept
{
    return mutate_inventory_storage(context, [&]() -> Gs1Status {
        const auto* recipe_def = find_craft_recipe_def(RecipeId {payload.recipe_id});
        if (recipe_def == nullptr)
        {
            return GS1_STATUS_NOT_FOUND;
        }

        const TileCoord target_tile {payload.target_tile_x, payload.target_tile_y};
        if (!context.world.tile_coord_in_bounds(target_tile))
        {
            return GS1_STATUS_INVALID_ARGUMENT;
        }

        const auto tile = context.world.read_tile(target_tile);
        if (tile.device.structure_id != recipe_def->station_structure_id)
        {
            return GS1_STATUS_INVALID_STATE;
        }

        const auto* structure_def = find_structure_def(tile.device.structure_id);
        if (structure_def == nullptr || !structure_def->grants_storage || structure_def->storage_slot_count == 0U)
        {
            return GS1_STATUS_INVALID_STATE;
        }

        if (context.site_run.site_world == nullptr)
        {
            return GS1_STATUS_INVALID_STATE;
        }

        const auto device_entity_id = context.site_run.site_world->device_entity_id(target_tile);
        if (device_entity_id == 0U)
        {
            return GS1_STATUS_INVALID_STATE;
        }

        const auto output_container = inventory_storage::ensure_device_storage_container(
            context.site_run,
            device_entity_id,
            target_tile,
            structure_def->storage_slot_count);
        const auto source_containers =
            craft_logic::collect_nearby_source_containers(context.site_run, target_tile);
        if (!craft_logic::can_store_output_after_recipe_consumption(
                context.site_run,
                output_container,
                source_containers,
                *recipe_def))
        {
            return GS1_STATUS_INVALID_STATE;
        }
        for (std::uint8_t index = 0U; index < recipe_def->ingredient_count; ++index)
        {
            const auto& ingredient = recipe_def->ingredients[index];
            if (inventory_storage::available_item_quantity_in_containers(
                    context.site_run,
                    source_containers,
                    ingredient.item_id) < ingredient.quantity)
            {
                return GS1_STATUS_INVALID_STATE;
            }
        }

        for (std::uint8_t index = 0U; index < recipe_def->ingredient_count; ++index)
        {
            const auto& ingredient = recipe_def->ingredients[index];
            const auto remaining = inventory_storage::consume_item_type_from_containers(
                context.site_run,
                source_containers,
                ingredient.item_id,
                ingredient.quantity);
            if (remaining != 0U)
            {
                return GS1_STATUS_INVALID_STATE;
            }
        }

        const auto remaining_output = inventory_storage::add_item_to_container(
            context.site_run,
            output_container,
            recipe_def->output_item_id,
            recipe_def->output_quantity);
        context.world.mark_projection_dirty(SITE_PROJECTION_UPDATE_INVENTORY);
        return remaining_output == 0U ? GS1_STATUS_OK : GS1_STATUS_INVALID_STATE;
    });
}

Gs1Status handle_inventory_transfer(
    SiteSystemContext<InventorySystem>& context,
    const InventoryTransferRequestedCommand& payload) noexcept
{
    const bool auto_resolve_destination = transfer_resolves_destination_in_dll(payload);
    const bool same_container =
        payload.source_container_kind == payload.destination_container_kind &&
        payload.source_container_owner_id == payload.destination_container_owner_id;

    if (!auto_resolve_destination &&
        payload.source_slot_index == payload.destination_slot_index &&
        same_container)
    {
        return GS1_STATUS_OK;
    }

    if (!has_supported_inventory_transfer_route(payload))
    {
        return GS1_STATUS_INVALID_STATE;
    }

    return mutate_inventory_storage(context, [&]() -> Gs1Status {
        if (auto_resolve_destination)
        {
            if (same_container)
            {
                return GS1_STATUS_INVALID_STATE;
            }

            const auto source_container = inventory_storage::container_for_kind(
                context.site_run,
                payload.source_container_kind,
                payload.source_container_owner_id);
            const auto destination_container = inventory_storage::container_for_kind(
                context.site_run,
                payload.destination_container_kind,
                payload.destination_container_owner_id);
            if (!source_container.is_valid() || !destination_container.is_valid())
            {
                return GS1_STATUS_INVALID_STATE;
            }

            const auto source_slot = inventory_storage::slot_entity(
                context.site_run,
                source_container,
                payload.source_slot_index);
            const auto source_item = inventory_storage::item_entity_for_slot(context.site_run, source_slot);
            const auto* source_stack = inventory_storage::stack_data(context.site_run, source_item);
            if (source_stack == nullptr || source_stack->quantity == 0U)
            {
                return GS1_STATUS_INVALID_STATE;
            }

            const auto requested_quantity =
                payload.quantity == 0U
                    ? source_stack->quantity
                    : std::min<std::uint32_t>(normalize_quantity(payload.quantity), source_stack->quantity);
            const auto remaining_quantity = inventory_storage::add_item_to_container(
                context.site_run,
                destination_container,
                source_stack->item_id,
                requested_quantity,
                source_stack->condition,
                source_stack->freshness);
            if (remaining_quantity == requested_quantity)
            {
                return GS1_STATUS_INVALID_STATE;
            }

            const auto transferred_quantity = requested_quantity - remaining_quantity;
            if (!inventory_storage::consume_quantity_from_item_entity(
                    context.site_run,
                    source_item,
                    transferred_quantity))
            {
                return GS1_STATUS_INVALID_STATE;
            }

            if (payload.source_container_kind == GS1_INVENTORY_CONTAINER_DEVICE_STORAGE ||
                payload.destination_container_kind == GS1_INVENTORY_CONTAINER_DEVICE_STORAGE)
            {
                context.world.mark_projection_dirty(SITE_PROJECTION_UPDATE_INVENTORY);
            }
            return GS1_STATUS_OK;
        }

        const bool transferred = inventory_storage::transfer_between_slots(
            context.site_run,
            payload.source_container_kind,
            payload.source_slot_index,
            payload.destination_container_kind,
            payload.destination_slot_index,
            normalize_quantity(payload.quantity),
            payload.source_container_owner_id,
            payload.destination_container_owner_id);
        if (transferred &&
            (payload.source_container_kind == GS1_INVENTORY_CONTAINER_DEVICE_STORAGE ||
                payload.destination_container_kind == GS1_INVENTORY_CONTAINER_DEVICE_STORAGE))
        {
            context.world.mark_projection_dirty(SITE_PROJECTION_UPDATE_INVENTORY);
        }
        return transferred ? GS1_STATUS_OK : GS1_STATUS_INVALID_STATE;
    });
}

Gs1Status handle_inventory_delivery_requested(
    SiteSystemContext<InventorySystem>& context,
    const InventoryDeliveryRequestedCommand& payload) noexcept
{
    ensure_inventory_storage_initialized(context);

    const ItemId item_id {payload.item_id};
    if (find_item_def(item_id) == nullptr)
    {
        return GS1_STATUS_NOT_FOUND;
    }

    PendingDelivery delivery {};
    delivery.delivery_id = DeliveryId {allocate_delivery_id()};
    delivery.minutes_until_arrival =
        payload.minutes_until_arrival == 0U
            ? k_default_delivery_minutes
            : static_cast<double>(payload.minutes_until_arrival);
    delivery.item_stacks.push_back(InventorySlot {});
    auto& stack = delivery.item_stacks.back();
    stack.item_id = item_id;
    stack.item_quantity = normalize_quantity(payload.quantity);
    stack.item_condition = 1.0f;
    stack.item_freshness = 1.0f;
    stack.occupied = stack.item_quantity > 0U;
    inventory_storage::sync_projection_views(context.site_run);
    context.world.own_inventory().pending_delivery_queue.push_back(delivery);
    return GS1_STATUS_OK;
}

void progress_pending_deliveries(SiteSystemContext<InventorySystem>& context) noexcept
{
    ensure_inventory_storage_initialized(context);
    auto& inventory = context.world.own_inventory();
    if (inventory.pending_delivery_queue.empty())
    {
        return;
    }

    const auto before_worker = inventory.worker_pack_slots;
    const auto starter_storage = resolve_starter_storage_container(context);
    bool inventory_changed = false;

    for (auto& delivery : inventory.pending_delivery_queue)
    {
        delivery.minutes_until_arrival =
            std::max(0.0, delivery.minutes_until_arrival - k_site_minutes_per_step);
        if (delivery.minutes_until_arrival > 0.0)
        {
            continue;
        }

        for (auto& stack : delivery.item_stacks)
        {
            if (!stack.occupied || stack.item_quantity == 0U)
            {
                continue;
            }

            const auto remaining_quantity = inventory_storage::add_item_to_container(
                context.site_run,
                starter_storage,
                stack.item_id,
                stack.item_quantity,
                stack.item_condition,
                stack.item_freshness);
            if (remaining_quantity != stack.item_quantity)
            {
                inventory_changed = true;
            }
            if (remaining_quantity == 0U)
            {
                stack = InventorySlot {};
            }
            else
            {
                stack.item_quantity = remaining_quantity;
            }
        }
    }

    inventory.pending_delivery_queue.erase(
        std::remove_if(
            inventory.pending_delivery_queue.begin(),
            inventory.pending_delivery_queue.end(),
            [](const PendingDelivery& delivery) {
                return !delivery_has_pending_items(delivery);
            }),
        inventory.pending_delivery_queue.end());

    if (inventory_changed)
    {
        context.world.mark_projection_dirty(SITE_PROJECTION_UPDATE_INVENTORY);
    }
    mark_changed_slot_views(context, before_worker);
}

}  // namespace

bool InventorySystem::subscribes_to(GameCommandType type) noexcept
{
    switch (type)
    {
    case GameCommandType::SiteRunStarted:
    case GameCommandType::SiteDevicePlaced:
    case GameCommandType::InventoryDeliveryRequested:
    case GameCommandType::InventoryItemUseRequested:
    case GameCommandType::InventoryItemConsumeRequested:
    case GameCommandType::InventoryGlobalItemConsumeRequested:
    case GameCommandType::InventoryTransferRequested:
    case GameCommandType::InventoryCraftCommitRequested:
        return true;
    default:
        return false;
    }
}

Gs1Status InventorySystem::process_command(
    SiteSystemContext<InventorySystem>& context,
    const GameCommand& command)
{
    switch (command.type)
    {
    case GameCommandType::SiteRunStarted:
        seed_inventory_from_loadout(context);
        return GS1_STATUS_OK;

    case GameCommandType::SiteDevicePlaced:
        (void)ensure_inventory_storage_initialized(context);
        (void)ensure_device_storage_containers(context);
        return GS1_STATUS_OK;

    case GameCommandType::InventoryDeliveryRequested:
        return handle_inventory_delivery_requested(
            context,
            command.payload_as<InventoryDeliveryRequestedCommand>());

    case GameCommandType::InventoryItemUseRequested:
        return handle_inventory_item_use(
            context,
            command.payload_as<InventoryItemUseRequestedCommand>());

    case GameCommandType::InventoryItemConsumeRequested:
        return handle_inventory_item_consume(
            context,
            command.payload_as<InventoryItemConsumeRequestedCommand>());

    case GameCommandType::InventoryGlobalItemConsumeRequested:
        return handle_inventory_global_item_consume(
            context,
            command.payload_as<InventoryGlobalItemConsumeRequestedCommand>());

    case GameCommandType::InventoryTransferRequested:
        return handle_inventory_transfer(
            context,
            command.payload_as<InventoryTransferRequestedCommand>());

    case GameCommandType::InventoryCraftCommitRequested:
        return handle_inventory_craft_commit(
            context,
            command.payload_as<InventoryCraftCommitRequestedCommand>());

    default:
        return GS1_STATUS_OK;
    }
}

void InventorySystem::run(SiteSystemContext<InventorySystem>& context)
{
    if (ensure_inventory_storage_initialized(context))
    {
        context.world.mark_projection_dirty(SITE_PROJECTION_UPDATE_INVENTORY);
    }
    progress_pending_deliveries(context);
}
}  // namespace gs1
