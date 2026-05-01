#include "site/systems/inventory_system.h"

#include "campaign/campaign_state.h"
#include "campaign/systems/technology_system.h"
#include "content/defs/craft_recipe_defs.h"
#include "content/defs/item_defs.h"
#include "content/defs/structure_defs.h"
#include "site/craft_logic.h"
#include "site/defs/site_action_defs.h"
#include "site/device_interaction_logic.h"
#include "site/inventory_state.h"
#include "site/inventory_storage.h"
#include "site/site_projection_update_flags.h"
#include "site/site_run_state.h"
#include "site/site_world_components.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4127)
#endif
#include <flecs.h>
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace gs1
{
namespace
{
constexpr std::uint32_t k_delivery_box_storage_flags =
    inventory_storage::k_inventory_storage_flag_delivery_box |
    inventory_storage::k_inventory_storage_flag_retrieval_only;

std::uint32_t normalize_quantity(std::uint32_t quantity) noexcept
{
    return quantity == 0U ? 1U : quantity;
}

bool has_supported_inventory_transfer_route(const InventoryTransferRequestedMessage& payload) noexcept
{
    return payload.source_storage_id != 0U && payload.destination_storage_id != 0U;
}

bool transfer_resolves_destination_in_dll(const InventoryTransferRequestedMessage& payload) noexcept
{
    return (payload.flags & k_inventory_transfer_flag_resolve_destination_in_dll) != 0U;
}

const StorageContainerState* storage_state_for_transfer(
    const SiteRunState& site_run,
    std::uint32_t storage_id) noexcept
{
    return inventory_storage::storage_container_state_for_storage_id(site_run, storage_id);
}

std::vector<InventorySlot> capture_storage_projection_slots(
    SiteRunState& site_run,
    std::uint32_t storage_id)
{
    std::vector<InventorySlot> slots {};
    const auto container = inventory_storage::container_for_storage_id(site_run, storage_id);
    if (!container.is_valid())
    {
        return slots;
    }

    slots.resize(inventory_storage::slot_count_in_container(site_run, container));
    inventory_storage::sync_projection_slots_for_container(site_run, container, slots);
    return slots;
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
            const auto item =
                inventory_storage::item_entity_for_slot(site_run, container, slot_index);
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

bool storage_is_retrieval_only(const StorageContainerState& storage_state) noexcept
{
    return (storage_state.flags & inventory_storage::k_inventory_storage_flag_retrieval_only) != 0U;
}

void clear_pending_device_storage_open(InventoryState& inventory) noexcept
{
    inventory.pending_device_storage_open = {};
}

bool has_pending_device_storage_open(const InventoryState& inventory) noexcept
{
    return inventory.pending_device_storage_open.active &&
        inventory.pending_device_storage_open.storage_id != 0U;
}

bool clear_pending_device_storage_open_for_storage(
    InventoryState& inventory,
    std::uint32_t storage_id) noexcept
{
    if (!has_pending_device_storage_open(inventory) ||
        inventory.pending_device_storage_open.storage_id != storage_id)
    {
        return false;
    }

    clear_pending_device_storage_open(inventory);
    return true;
}

bool open_device_storage_now(
    SiteSystemContext<InventorySystem>& context,
    std::uint32_t storage_id) noexcept
{
    auto& inventory = context.world.own_inventory();
    clear_pending_device_storage_open(inventory);
    inventory.opened_device_storage_id = storage_id;
    context.world.mark_inventory_view_state_projection_dirty();
    context.world.mark_opened_inventory_storage_full_projection_dirty();
    return true;
}

void close_opened_device_storage_if_out_of_range(
    SiteSystemContext<InventorySystem>& context) noexcept
{
    auto& inventory = context.world.own_inventory();
    if (inventory.opened_device_storage_id == 0U)
    {
        return;
    }

    const auto* storage_state =
        inventory_storage::storage_container_state_for_storage_id(
            context.site_run,
            inventory.opened_device_storage_id);
    if (storage_state == nullptr ||
        storage_state->container_kind != GS1_INVENTORY_CONTAINER_DEVICE_STORAGE ||
        !device_interaction_logic::worker_is_in_interaction_range(
            context.site_run,
            context.world.read_worker(),
            storage_state->tile_coord))
    {
        inventory.opened_device_storage_id = 0U;
        context.world.mark_inventory_view_state_projection_dirty();
    }
}

void progress_pending_device_storage_open(SiteSystemContext<InventorySystem>& context) noexcept
{
    auto& inventory = context.world.own_inventory();
    if (!has_pending_device_storage_open(inventory))
    {
        return;
    }

    const auto storage_id = inventory.pending_device_storage_open.storage_id;
    const auto* storage_state =
        inventory_storage::storage_container_state_for_storage_id(context.site_run, storage_id);
    if (storage_state == nullptr ||
        storage_state->container_kind != GS1_INVENTORY_CONTAINER_DEVICE_STORAGE)
    {
        clear_pending_device_storage_open(inventory);
        return;
    }

    if (!device_interaction_logic::tile_is_traversable(
            context.site_run,
            inventory.pending_device_storage_open.approach_tile))
    {
        clear_pending_device_storage_open(inventory);
        return;
    }

    if (!device_interaction_logic::worker_is_at_tile(
            context.world.read_worker(),
            inventory.pending_device_storage_open.approach_tile))
    {
        return;
    }

    (void)open_device_storage_now(context, storage_id);
}

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
    auto source_query =
        context.site_run.site_world->ecs_world()
            .query_builder<
                const site_ecs::TileCoordComponent,
                const site_ecs::DeviceStructureId>()
            .with<site_ecs::DeviceTag>()
            .build();

    source_query.each(
        [&](flecs::entity device_entity,
            const site_ecs::TileCoordComponent& coord_component,
            const site_ecs::DeviceStructureId& structure_component) {
            const auto* structure_def = find_structure_def(structure_component.structure_id);
            if (structure_def == nullptr ||
                !structure_def->grants_storage ||
                structure_def->storage_slot_count == 0U)
            {
                return;
            }

            const auto coord = coord_component.value;
            const auto existing =
                inventory_storage::find_device_storage_container(
                    context.site_run,
                    device_entity.id());
            const std::uint32_t storage_flags =
                (coord.x == context.world.read_camp().delivery_box_tile.x &&
                    coord.y == context.world.read_camp().delivery_box_tile.y)
                    ? k_delivery_box_storage_flags
                    : 0U;
            (void)inventory_storage::ensure_device_storage_container(
                context.site_run,
                device_entity.id(),
                coord,
                structure_def->storage_slot_count,
                storage_flags);
            if (!existing.is_valid())
            {
                created_any = true;
            }
        });

    if (created_any)
    {
        context.world.mark_inventory_storage_descriptors_projection_dirty();
    }

    return created_any;
}

flecs::entity resolve_delivery_box_container(SiteSystemContext<InventorySystem>& context) noexcept
{
    if (!context.world.has_world() || context.site_run.site_world == nullptr)
    {
        return {};
    }

    const auto tile = context.world.read_camp().delivery_box_tile;
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
        structure_def->storage_slot_count,
        k_delivery_box_storage_flags);
}

void mark_changed_slot_views(
    SiteSystemContext<InventorySystem>& context,
    const std::vector<InventorySlot>& before_worker,
    std::uint32_t before_opened_storage_id,
    const std::vector<InventorySlot>& before_opened_storage) noexcept
{
    const auto& inventory = context.world.read_inventory();
    if (before_worker.size() != inventory.worker_pack_slots.size())
    {
        context.world.mark_inventory_storage_descriptors_projection_dirty();
    }
    else
    {
        for (std::size_t index = 0; index < before_worker.size(); ++index)
        {
            const auto& lhs = before_worker[index];
            const auto& rhs = inventory.worker_pack_slots[index];
            if (lhs.item_instance_id != rhs.item_instance_id ||
                lhs.item_id != rhs.item_id ||
                lhs.item_quantity != rhs.item_quantity ||
                lhs.item_condition != rhs.item_condition ||
                lhs.item_freshness != rhs.item_freshness ||
                lhs.occupied != rhs.occupied)
            {
                context.world.mark_inventory_slot_projection_dirty(
                    GS1_INVENTORY_CONTAINER_WORKER_PACK,
                    static_cast<std::uint32_t>(index));
            }
        }
    }

    const auto after_opened_storage_id = inventory.opened_device_storage_id;
    if (before_opened_storage_id != after_opened_storage_id)
    {
        context.world.mark_inventory_view_state_projection_dirty();
        if (after_opened_storage_id != 0U)
        {
            context.world.mark_opened_inventory_storage_full_projection_dirty();
        }
        return;
    }

    if (after_opened_storage_id == 0U)
    {
        return;
    }

    const auto after_opened_storage =
        capture_storage_projection_slots(context.site_run, after_opened_storage_id);
    if (before_opened_storage.size() != after_opened_storage.size())
    {
        context.world.mark_opened_inventory_storage_full_projection_dirty();
        return;
    }

    for (std::size_t index = 0; index < before_opened_storage.size(); ++index)
    {
        const auto& lhs = before_opened_storage[index];
        const auto& rhs = after_opened_storage[index];
        if (lhs.item_instance_id != rhs.item_instance_id ||
            lhs.item_id != rhs.item_id ||
            lhs.item_quantity != rhs.item_quantity ||
            lhs.item_condition != rhs.item_condition ||
            lhs.item_freshness != rhs.item_freshness ||
            lhs.occupied != rhs.occupied)
        {
            context.world.mark_inventory_slot_projection_dirty_by_storage(
                after_opened_storage_id,
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
    const auto before_opened_storage_id = context.world.read_inventory().opened_device_storage_id;
    const auto before_opened_storage =
        capture_storage_projection_slots(context.site_run, before_opened_storage_id);
    const auto status = func();
    if (status != GS1_STATUS_OK)
    {
        return status;
    }

    if (storage_changed)
    {
        context.world.mark_inventory_storage_descriptors_projection_dirty();
    }
    mark_changed_slot_views(
        context,
        before_worker,
        before_opened_storage_id,
        before_opened_storage);
    return GS1_STATUS_OK;
}

bool item_is_directly_usable(const ItemDef& item_def) noexcept
{
    return item_has_capability(item_def, ITEM_CAPABILITY_DRINK) ||
        item_has_capability(item_def, ITEM_CAPABILITY_EAT) ||
        item_has_capability(item_def, ITEM_CAPABILITY_USE_MEDICINE);
}

ActionKind consume_action_kind_for_item(const ItemDef& item_def) noexcept
{
    if (item_has_capability(item_def, ITEM_CAPABILITY_DRINK))
    {
        return ActionKind::Drink;
    }
    if (item_has_capability(item_def, ITEM_CAPABILITY_EAT))
    {
        return ActionKind::Eat;
    }

    return ActionKind::None;
}

std::uint32_t reserved_item_quantity_in_container(
    const ActionState& action_state,
    Gs1InventoryContainerKind container_kind,
    ItemId item_id) noexcept
{
    std::uint32_t reserved_quantity = 0U;
    for (const auto& reserved_stack : action_state.reserved_input_item_stacks)
    {
        if (reserved_stack.container_kind == container_kind &&
            reserved_stack.item_id == item_id)
        {
            reserved_quantity += reserved_stack.quantity;
        }
    }

    return reserved_quantity;
}

std::uint32_t available_unreserved_item_quantity_in_container_kind(
    SiteRunState& site_run,
    const ActionState& action_state,
    Gs1InventoryContainerKind container_kind,
    ItemId item_id) noexcept
{
    const auto available = inventory_storage::available_item_quantity_in_container_kind(
        site_run,
        container_kind,
        item_id);
    const auto reserved = reserved_item_quantity_in_container(action_state, container_kind, item_id);
    return available > reserved ? available - reserved : 0U;
}

std::uint32_t total_reserved_item_quantity(
    const ActionState& action_state,
    ItemId item_id) noexcept
{
    std::uint32_t reserved_quantity = 0U;
    for (const auto& reserved_stack : action_state.reserved_input_item_stacks)
    {
        if (reserved_stack.item_id == item_id)
        {
            reserved_quantity += reserved_stack.quantity;
        }
    }

    return reserved_quantity;
}

void emit_item_use_completed(
    SiteSystemContext<InventorySystem>& context,
    ItemId item_id,
    std::uint32_t quantity) noexcept
{
    GameMessage completed_message {};
    completed_message.type = GameMessageType::InventoryItemUseCompleted;
    completed_message.set_payload(InventoryItemUseCompletedMessage {
        item_id.value,
        static_cast<std::uint16_t>(std::min<std::uint32_t>(quantity, 65535U)),
        0U});
    context.message_queue.push_back(completed_message);
}

void emit_item_use_action_request(
    SiteSystemContext<InventorySystem>& context,
    ActionKind action_kind,
    const ItemDef& item_def,
    std::uint16_t quantity) noexcept
{
    const auto worker = context.world.read_worker();
    const auto tile = worker.position.tile_coord;

    GameMessage message {};
    message.type = GameMessageType::StartSiteAction;
    message.set_payload(StartSiteActionMessage {
        static_cast<Gs1SiteActionKind>(action_kind),
        GS1_SITE_ACTION_REQUEST_FLAG_HAS_ITEM,
        quantity == 0U ? 1U : quantity,
        tile.x,
        tile.y,
        0U,
        0U,
        item_def.item_id.value});
    context.message_queue.push_back(message);
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

PendingDelivery make_pending_delivery(
    DeliveryId delivery_id,
    const InventoryDeliveryBatchEntry* entries,
    std::uint8_t entry_count)
{
    PendingDelivery delivery {};
    delivery.delivery_id = delivery_id;
    delivery.state = PendingDeliveryState::Pending;
    delivery.item_stacks.reserve(entry_count);
    for (std::uint8_t index = 0U; index < entry_count; ++index)
    {
        const auto& entry = entries[index];
        if (entry.item_id == 0U || entry.quantity == 0U)
        {
            continue;
        }

        delivery.item_stacks.push_back(InventorySlot {});
        auto& stack = delivery.item_stacks.back();
        stack.item_id = ItemId {entry.item_id};
        stack.item_quantity = entry.quantity;
        stack.item_condition = 1.0f;
        stack.item_freshness = 1.0f;
        stack.occupied = true;
    }

    return delivery;
}

void try_add_delivery_to_crate(
    SiteSystemContext<InventorySystem>& context,
    PendingDelivery& delivery) noexcept
{
    const auto delivery_box = resolve_delivery_box_container(context);
    if (!delivery_box.is_valid())
    {
        return;
    }

    for (auto& stack : delivery.item_stacks)
    {
        if (!stack.occupied || stack.item_quantity == 0U)
        {
            continue;
        }

        const auto remaining_quantity = inventory_storage::add_item_to_container(
            context.site_run,
            delivery_box,
            stack.item_id,
            stack.item_quantity,
            stack.item_condition,
            stack.item_freshness);
        if (remaining_quantity == 0U)
        {
            stack = InventorySlot {};
        }
        else
        {
            stack.item_quantity = remaining_quantity;
            stack.occupied = remaining_quantity > 0U;
        }
    }
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

    const auto delivery_box = resolve_delivery_box_container(context);
    if (!delivery_box.is_valid())
    {
        return;
    }

    for (const auto& slot : loadout_slots)
    {
        if (!slot.occupied || slot.item_id.value == 0U || slot.quantity == 0U)
        {
            continue;
        }

        (void)inventory_storage::add_item_to_container(
            context.site_run,
            delivery_box,
            slot.item_id,
            slot.quantity);
    }
}

Gs1Status handle_inventory_item_use(
    SiteSystemContext<InventorySystem>& context,
    const InventoryItemUseRequestedMessage& payload) noexcept
{
    return mutate_inventory_storage(context, [&]() -> Gs1Status {
        const auto container =
            inventory_storage::container_for_storage_id(context.site_run, payload.storage_id);
        const auto item_entity = inventory_storage::item_entity_for_slot(
            context.site_run,
            container,
            static_cast<std::uint32_t>(payload.slot_index));
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

        const ActionKind consume_action_kind = consume_action_kind_for_item(*item_def);
        if (consume_action_kind != ActionKind::None)
        {
            emit_item_use_action_request(
                context,
                consume_action_kind,
                *item_def,
                static_cast<std::uint16_t>(requested_quantity));
            return GS1_STATUS_OK;
        }

        if (!inventory_storage::consume_quantity_from_item_entity(
                context.site_run,
                item_entity,
                requested_quantity))
        {
            return GS1_STATUS_INVALID_STATE;
        }

        emit_item_use_completed(context, stack->item_id, requested_quantity);
        context.world.mark_projection_dirty(SITE_PROJECTION_UPDATE_HUD);
        return GS1_STATUS_OK;
    });
}

Gs1Status handle_inventory_item_consume(
    SiteSystemContext<InventorySystem>& context,
    const InventoryItemConsumeRequestedMessage& payload) noexcept
{
    return mutate_inventory_storage(context, [&]() -> Gs1Status {
        const ItemId item_id {payload.item_id};
        const auto quantity = normalize_quantity(payload.quantity);
        if (find_item_def(item_id) == nullptr)
        {
            return GS1_STATUS_NOT_FOUND;
        }

        const auto available_quantity =
            (payload.flags & k_inventory_item_consume_flag_ignore_action_reservations) != 0U
            ? inventory_storage::available_item_quantity_in_container_kind(
                  context.site_run,
                  payload.container_kind,
                  item_id)
            : available_unreserved_item_quantity_in_container_kind(
                  context.site_run,
                  context.world.read_action(),
                  payload.container_kind,
                  item_id);
        if (available_quantity < quantity)
        {
            return GS1_STATUS_INVALID_STATE;
        }

        const auto remaining = inventory_storage::consume_item_type_from_container(
            context.site_run,
            inventory_storage::container_for_kind(context.site_run, payload.container_kind),
            item_id,
            quantity);
        if (remaining != 0U)
        {
            return GS1_STATUS_INVALID_STATE;
        }

        const auto* item_def = find_item_def(item_id);
        if (item_def != nullptr && item_is_directly_usable(*item_def))
        {
            emit_item_use_completed(context, item_id, quantity);
            context.world.mark_projection_dirty(SITE_PROJECTION_UPDATE_HUD);
        }
        return GS1_STATUS_OK;
    });
}

Gs1Status handle_inventory_global_item_consume(
    SiteSystemContext<InventorySystem>& context,
    const InventoryGlobalItemConsumeRequestedMessage& payload) noexcept
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
        const auto reserved_quantity =
            (payload.flags & k_inventory_item_consume_flag_ignore_action_reservations) != 0U
            ? 0U
            : total_reserved_item_quantity(context.world.read_action(), item_id);
        const auto available_quantity = inventory_storage::available_item_quantity_in_containers(
            context.site_run,
            containers,
            item_id);
        if (available_quantity < quantity || available_quantity - quantity < reserved_quantity)
        {
            return GS1_STATUS_INVALID_STATE;
        }

        auto ordered_containers = containers;
        if (reserved_quantity > 0U)
        {
            const auto worker_pack = inventory_storage::worker_pack_container(context.site_run);
            std::stable_partition(
                ordered_containers.begin(),
                ordered_containers.end(),
                [&](const flecs::entity& container) {
                    return container.id() != worker_pack.id();
                });
        }

        const auto remaining = inventory_storage::consume_item_type_from_containers(
            context.site_run,
            ordered_containers,
            item_id,
            quantity);
        return remaining == 0U ? GS1_STATUS_OK : GS1_STATUS_INVALID_STATE;
    });
}

Gs1Status handle_inventory_craft_commit(
    SiteSystemContext<InventorySystem>& context,
    const InventoryCraftCommitRequestedMessage& payload) noexcept
{
    return mutate_inventory_storage(context, [&]() -> Gs1Status {
        const auto* recipe_def = find_craft_recipe_def(RecipeId {payload.recipe_id});
        if (recipe_def == nullptr)
        {
            return GS1_STATUS_NOT_FOUND;
        }

        if (!TechnologySystem::recipe_unlocked(context.campaign, recipe_def->recipe_id))
        {
            return GS1_STATUS_INVALID_STATE;
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
        const auto nearby_item_instance_ids =
            inventory_storage::collect_item_instance_ids_in_containers(
                context.site_run,
                source_containers);
        if (!craft_logic::can_store_output_after_recipe_consumption(
                context.site_run,
                output_container,
                source_containers,
                *recipe_def))
        {
            return GS1_STATUS_INVALID_STATE;
        }
        if (!craft_logic::can_satisfy_recipe_requirements(
                context.site_run,
                nearby_item_instance_ids,
                *recipe_def))
        {
            return GS1_STATUS_INVALID_STATE;
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
        if (remaining_output != 0U)
        {
            return GS1_STATUS_INVALID_STATE;
        }

        GameMessage completed_message {};
        completed_message.type = GameMessageType::InventoryCraftCompleted;
        completed_message.set_payload(InventoryCraftCompletedMessage {
            recipe_def->recipe_id.value,
            recipe_def->output_item_id.value,
            recipe_def->output_quantity,
            0U,
            inventory_storage::storage_id_for_container(context.site_run, output_container)});
        context.message_queue.push_back(completed_message);
        return GS1_STATUS_OK;
    });
}

Gs1Status handle_site_device_broken(
    SiteSystemContext<InventorySystem>& context,
    const SiteDeviceBrokenMessage& payload) noexcept
{
    return mutate_inventory_storage(context, [&]() -> Gs1Status {
        const auto container =
            inventory_storage::find_device_storage_container(context.site_run, payload.device_entity_id);
        if (!container.is_valid())
        {
            return GS1_STATUS_OK;
        }

        const auto storage_id =
            inventory_storage::storage_id_for_container(context.site_run, container);
        if (context.world.own_inventory().opened_device_storage_id == storage_id)
        {
            context.world.own_inventory().opened_device_storage_id = 0U;
        }

        if (inventory_storage::destroy_storage_container(context.site_run, container))
        {
            context.world.mark_inventory_storage_descriptors_projection_dirty();
        }
        return GS1_STATUS_OK;
    });
}

Gs1Status handle_inventory_transfer(
    SiteSystemContext<InventorySystem>& context,
    const InventoryTransferRequestedMessage& payload) noexcept
{
    const bool auto_resolve_destination = transfer_resolves_destination_in_dll(payload);
    const bool same_container = payload.source_storage_id == payload.destination_storage_id;

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
        const auto* source_storage = storage_state_for_transfer(context.site_run, payload.source_storage_id);
        const auto* destination_storage =
            storage_state_for_transfer(context.site_run, payload.destination_storage_id);
        if (source_storage == nullptr || destination_storage == nullptr)
        {
            return GS1_STATUS_INVALID_STATE;
        }

        const bool source_is_worker_pack =
            source_storage->container_kind == GS1_INVENTORY_CONTAINER_WORKER_PACK;
        const bool destination_is_worker_pack =
            destination_storage->container_kind == GS1_INVENTORY_CONTAINER_WORKER_PACK;
        const bool source_is_device_storage =
            source_storage->container_kind == GS1_INVENTORY_CONTAINER_DEVICE_STORAGE;
        const bool destination_is_device_storage =
            destination_storage->container_kind == GS1_INVENTORY_CONTAINER_DEVICE_STORAGE;
        if (!((source_is_worker_pack && destination_is_worker_pack) ||
                (source_is_worker_pack && destination_is_device_storage) ||
                (source_is_device_storage && destination_is_worker_pack)))
        {
            return GS1_STATUS_INVALID_STATE;
        }

        if (source_is_worker_pack && source_storage->container_kind == GS1_INVENTORY_CONTAINER_WORKER_PACK &&
            destination_is_device_storage && storage_is_retrieval_only(*destination_storage))
        {
            return GS1_STATUS_INVALID_STATE;
        }

        if (auto_resolve_destination)
        {
            if (same_container)
            {
                return GS1_STATUS_INVALID_STATE;
            }

            const auto source_container =
                inventory_storage::container_for_storage_id(context.site_run, payload.source_storage_id);
            const auto destination_container =
                inventory_storage::container_for_storage_id(context.site_run, payload.destination_storage_id);
            if (!source_container.is_valid() || !destination_container.is_valid())
            {
                return GS1_STATUS_INVALID_STATE;
            }

            const auto source_item = inventory_storage::item_entity_for_slot(
                context.site_run,
                source_container,
                payload.source_slot_index);
            const auto* source_stack = inventory_storage::stack_data(context.site_run, source_item);
            if (source_stack == nullptr || source_stack->quantity == 0U)
            {
                return GS1_STATUS_INVALID_STATE;
            }

            const auto requested_quantity =
                payload.quantity == 0U
                    ? source_stack->quantity
                    : std::min<std::uint32_t>(normalize_quantity(payload.quantity), source_stack->quantity);
            const auto source_reserved = reserved_item_quantity_in_container(
                context.world.read_action(),
                source_storage->container_kind,
                source_stack->item_id);
            if (source_reserved > 0U)
            {
                const auto source_available = inventory_storage::available_item_quantity_in_container_kind(
                    context.site_run,
                    source_storage->container_kind,
                    source_stack->item_id);
                if (source_available < requested_quantity ||
                    source_available - requested_quantity < source_reserved)
                {
                    return GS1_STATUS_INVALID_STATE;
                }
            }
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

            GameMessage completed_message {};
            completed_message.type = GameMessageType::InventoryTransferCompleted;
            completed_message.set_payload(InventoryTransferCompletedMessage {
                payload.source_storage_id,
                payload.destination_storage_id,
                source_stack->item_id.value,
                static_cast<std::uint16_t>(transferred_quantity),
                payload.flags});
            context.message_queue.push_back(completed_message);
            return GS1_STATUS_OK;
        }

        const auto source_container =
            inventory_storage::container_for_storage_id(context.site_run, payload.source_storage_id);
        const auto source_item = inventory_storage::item_entity_for_slot(
            context.site_run,
            source_container,
            payload.source_slot_index);
        const auto* source_stack = inventory_storage::stack_data(context.site_run, source_item);
        if (source_stack == nullptr || source_stack->quantity == 0U)
        {
            return GS1_STATUS_INVALID_STATE;
        }

        const auto source_reserved = reserved_item_quantity_in_container(
            context.world.read_action(),
            source_storage->container_kind,
            source_stack->item_id);
        if (source_reserved > 0U)
        {
            const auto transfer_quantity =
                std::min<std::uint32_t>(normalize_quantity(payload.quantity), source_stack->quantity);
            const auto source_available = inventory_storage::available_item_quantity_in_container_kind(
                context.site_run,
                source_storage->container_kind,
                source_stack->item_id);
            if (source_available < transfer_quantity ||
                source_available - transfer_quantity < source_reserved)
            {
                return GS1_STATUS_INVALID_STATE;
            }
        }

        const auto transferred_quantity =
            std::min<std::uint32_t>(normalize_quantity(payload.quantity), source_stack->quantity);

        const bool transferred = inventory_storage::transfer_between_slots(
            context.site_run,
            source_storage->container_kind,
            payload.source_slot_index,
            destination_storage->container_kind,
            payload.destination_slot_index,
            transferred_quantity,
            source_storage->owner_device_entity_id,
            destination_storage->owner_device_entity_id);
        if (!transferred)
        {
            return GS1_STATUS_INVALID_STATE;
        }

        GameMessage completed_message {};
        completed_message.type = GameMessageType::InventoryTransferCompleted;
        completed_message.set_payload(InventoryTransferCompletedMessage {
            payload.source_storage_id,
            payload.destination_storage_id,
            source_stack->item_id.value,
            static_cast<std::uint16_t>(transferred_quantity),
            payload.flags});
        context.message_queue.push_back(completed_message);
        return GS1_STATUS_OK;
    });
}

Gs1Status handle_inventory_item_submit(
    SiteSystemContext<InventorySystem>& context,
    const InventoryItemSubmitRequestedMessage& payload) noexcept
{
    return mutate_inventory_storage(context, [&]() -> Gs1Status {
        const auto* source_storage = storage_state_for_transfer(context.site_run, payload.source_storage_id);
        if (source_storage == nullptr)
        {
            return GS1_STATUS_INVALID_STATE;
        }

        const bool source_is_worker_pack =
            source_storage->container_kind == GS1_INVENTORY_CONTAINER_WORKER_PACK;
        const bool source_is_device_storage =
            source_storage->container_kind == GS1_INVENTORY_CONTAINER_DEVICE_STORAGE;
        if (!source_is_worker_pack && !source_is_device_storage)
        {
            return GS1_STATUS_INVALID_STATE;
        }

        const auto source_container =
            inventory_storage::container_for_storage_id(context.site_run, payload.source_storage_id);
        const auto source_item = inventory_storage::item_entity_for_slot(
            context.site_run,
            source_container,
            payload.source_slot_index);
        const auto* source_stack = inventory_storage::stack_data(context.site_run, source_item);
        if (source_stack == nullptr || source_stack->quantity == 0U)
        {
            return GS1_STATUS_INVALID_ARGUMENT;
        }

        const auto requested_quantity =
            std::min<std::uint32_t>(normalize_quantity(payload.quantity), source_stack->quantity);
        const auto source_reserved = reserved_item_quantity_in_container(
            context.world.read_action(),
            source_storage->container_kind,
            source_stack->item_id);
        if (source_reserved > 0U)
        {
            const auto source_available = inventory_storage::available_item_quantity_in_container_kind(
                context.site_run,
                source_storage->container_kind,
                source_stack->item_id);
            if (source_available < requested_quantity ||
                source_available - requested_quantity < source_reserved)
            {
                return GS1_STATUS_INVALID_STATE;
            }
        }

        if (!inventory_storage::consume_quantity_from_item_entity(
                context.site_run,
                source_item,
                requested_quantity))
        {
            return GS1_STATUS_INVALID_STATE;
        }

        GameMessage completed_message {};
        completed_message.type = GameMessageType::InventoryItemSubmitted;
        completed_message.set_payload(InventoryItemSubmittedMessage {
            source_stack->item_id.value,
            static_cast<std::uint16_t>(requested_quantity),
            0U});
        context.message_queue.push_back(completed_message);
        return GS1_STATUS_OK;
    });
}

Gs1Status handle_inventory_storage_view_request(
    SiteSystemContext<InventorySystem>& context,
    const InventoryStorageViewRequestMessage& payload) noexcept
{
    ensure_inventory_storage_initialized(context);

    if (payload.event_kind == GS1_INVENTORY_VIEW_EVENT_CLOSE)
    {
        auto& inventory = context.world.own_inventory();
        if (payload.storage_id == inventory.worker_pack_storage_id)
        {
            if (inventory.worker_pack_panel_open)
            {
                inventory.worker_pack_panel_open = false;
                context.world.mark_inventory_view_state_projection_dirty();
            }
            return GS1_STATUS_OK;
        }

        const bool cleared_pending =
            clear_pending_device_storage_open_for_storage(inventory, payload.storage_id);
        if (inventory.opened_device_storage_id == payload.storage_id)
        {
            inventory.opened_device_storage_id = 0U;
            context.world.mark_inventory_view_state_projection_dirty();
        }
        else if (cleared_pending)
        {
            return GS1_STATUS_OK;
        }
        return GS1_STATUS_OK;
    }

    if (payload.event_kind != GS1_INVENTORY_VIEW_EVENT_OPEN_SNAPSHOT)
    {
        return GS1_STATUS_INVALID_ARGUMENT;
    }

    if (payload.storage_id == context.world.read_inventory().worker_pack_storage_id)
    {
        auto& inventory = context.world.own_inventory();
        if (!inventory.worker_pack_panel_open)
        {
            inventory.worker_pack_panel_open = true;
            context.world.mark_inventory_view_state_projection_dirty();
        }
        return GS1_STATUS_OK;
    }

    const auto* storage_state =
        inventory_storage::storage_container_state_for_storage_id(context.site_run, payload.storage_id);
    if (storage_state == nullptr ||
        storage_state->container_kind != GS1_INVENTORY_CONTAINER_DEVICE_STORAGE)
    {
        return GS1_STATUS_INVALID_ARGUMENT;
    }

    const auto worker = context.world.read_worker();
    const auto approach_tile =
        device_interaction_logic::resolve_interaction_range_approach_tile(
            context.site_run,
            worker,
            storage_state->tile_coord);
    if (!approach_tile.has_value())
    {
        return GS1_STATUS_INVALID_ARGUMENT;
    }

    if (device_interaction_logic::worker_is_at_tile(worker, *approach_tile))
    {
        (void)open_device_storage_now(context, payload.storage_id);
        return GS1_STATUS_OK;
    }

    auto& inventory = context.world.own_inventory();
    inventory.pending_device_storage_open.storage_id = payload.storage_id;
    inventory.pending_device_storage_open.approach_tile = *approach_tile;
    inventory.pending_device_storage_open.active = true;
    return GS1_STATUS_OK;
}

Gs1Status handle_inventory_delivery_requested(
    SiteSystemContext<InventorySystem>& context,
    const InventoryDeliveryRequestedMessage& payload) noexcept
{
    return mutate_inventory_storage(context, [&]() -> Gs1Status {
        const ItemId item_id {payload.item_id};
        if (find_item_def(item_id) == nullptr)
        {
            return GS1_STATUS_NOT_FOUND;
        }

        InventoryDeliveryBatchEntry entry {};
        entry.item_id = static_cast<std::uint16_t>(item_id.value);
        entry.quantity = static_cast<std::uint16_t>(normalize_quantity(payload.quantity));
        PendingDelivery delivery = make_pending_delivery(
            DeliveryId {allocate_delivery_id()},
            &entry,
            1U);
        if (!delivery_has_pending_items(delivery))
        {
            return GS1_STATUS_INVALID_ARGUMENT;
        }

        try_add_delivery_to_crate(context, delivery);
        if (delivery_has_pending_items(delivery))
        {
            context.world.own_inventory().pending_delivery_queue.push_back(std::move(delivery));
        }
        return GS1_STATUS_OK;
    });
}

Gs1Status handle_inventory_delivery_batch_requested(
    SiteSystemContext<InventorySystem>& context,
    const InventoryDeliveryBatchRequestedMessage& payload) noexcept
{
    return mutate_inventory_storage(context, [&]() -> Gs1Status {
        if (payload.entry_count == 0U || payload.entry_count > k_inventory_delivery_batch_entry_count)
        {
            return GS1_STATUS_INVALID_ARGUMENT;
        }

        for (std::uint8_t index = 0U; index < payload.entry_count; ++index)
        {
            if (payload.entries[index].item_id == 0U ||
                payload.entries[index].quantity == 0U ||
                find_item_def(ItemId {payload.entries[index].item_id}) == nullptr)
            {
                return GS1_STATUS_NOT_FOUND;
            }
        }

        PendingDelivery delivery = make_pending_delivery(
            DeliveryId {allocate_delivery_id()},
            payload.entries,
            payload.entry_count);
        if (!delivery_has_pending_items(delivery))
        {
            return GS1_STATUS_INVALID_ARGUMENT;
        }

        try_add_delivery_to_crate(context, delivery);
        if (delivery_has_pending_items(delivery))
        {
            context.world.own_inventory().pending_delivery_queue.push_back(std::move(delivery));
        }
        return GS1_STATUS_OK;
    });
}

Gs1Status handle_inventory_worker_pack_insert_requested(
    SiteSystemContext<InventorySystem>& context,
    const InventoryWorkerPackInsertRequestedMessage& payload) noexcept
{
    return mutate_inventory_storage(context, [&]() -> Gs1Status {
        const ItemId item_id {payload.item_id};
        const std::uint32_t quantity = normalize_quantity(payload.quantity);
        if (item_id.value == 0U || find_item_def(item_id) == nullptr)
        {
            return GS1_STATUS_NOT_FOUND;
        }

        const auto worker_pack = inventory_storage::worker_pack_container(context.site_run);
        if (!worker_pack.is_valid() ||
            !inventory_storage::can_fit_item_in_container(
                context.site_run,
                worker_pack,
                item_id,
                quantity))
        {
            return GS1_STATUS_INVALID_STATE;
        }

        return inventory_storage::add_item_to_container(
                   context.site_run,
                   worker_pack,
                   item_id,
                   quantity) == 0U
            ? GS1_STATUS_OK
            : GS1_STATUS_INVALID_STATE;
    });
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
    const auto before_opened_storage_id = inventory.opened_device_storage_id;
    const auto before_opened_storage =
        capture_storage_projection_slots(context.site_run, before_opened_storage_id);

    for (auto& delivery : inventory.pending_delivery_queue)
    {
        try_add_delivery_to_crate(context, delivery);
    }

    inventory.pending_delivery_queue.erase(
        std::remove_if(
            inventory.pending_delivery_queue.begin(),
            inventory.pending_delivery_queue.end(),
            [](const PendingDelivery& delivery) {
                return !delivery_has_pending_items(delivery);
            }),
        inventory.pending_delivery_queue.end());

    mark_changed_slot_views(
        context,
        before_worker,
        before_opened_storage_id,
        before_opened_storage);
}

}  // namespace

bool InventorySystem::subscribes_to(GameMessageType type) noexcept
{
    switch (type)
    {
    case GameMessageType::StartSiteAction:
    case GameMessageType::SiteRunStarted:
    case GameMessageType::SiteDevicePlaced:
    case GameMessageType::SiteDeviceBroken:
    case GameMessageType::InventoryDeliveryRequested:
    case GameMessageType::InventoryDeliveryBatchRequested:
    case GameMessageType::InventoryWorkerPackInsertRequested:
    case GameMessageType::InventoryItemUseRequested:
    case GameMessageType::InventoryItemConsumeRequested:
    case GameMessageType::InventoryGlobalItemConsumeRequested:
    case GameMessageType::InventoryTransferRequested:
    case GameMessageType::InventoryItemSubmitRequested:
    case GameMessageType::InventoryStorageViewRequest:
    case GameMessageType::InventoryCraftCommitRequested:
        return true;
    default:
        return false;
    }
}

Gs1Status InventorySystem::process_message(
    SiteSystemContext<InventorySystem>& context,
    const GameMessage& message)
{
    switch (message.type)
    {
    case GameMessageType::StartSiteAction:
        if (gs1_action_impacts_worker_movement(
                message.payload_as<StartSiteActionMessage>().action_kind))
        {
            clear_pending_device_storage_open(context.world.own_inventory());
        }
        return GS1_STATUS_OK;

    case GameMessageType::SiteRunStarted:
        seed_inventory_from_loadout(context);
        return GS1_STATUS_OK;

    case GameMessageType::SiteDevicePlaced:
        (void)ensure_inventory_storage_initialized(context);
        (void)ensure_device_storage_containers(context);
        return GS1_STATUS_OK;

    case GameMessageType::SiteDeviceBroken:
        return handle_site_device_broken(
            context,
            message.payload_as<SiteDeviceBrokenMessage>());

    case GameMessageType::InventoryDeliveryRequested:
        return handle_inventory_delivery_requested(
            context,
            message.payload_as<InventoryDeliveryRequestedMessage>());

    case GameMessageType::InventoryDeliveryBatchRequested:
        return handle_inventory_delivery_batch_requested(
            context,
            message.payload_as<InventoryDeliveryBatchRequestedMessage>());

    case GameMessageType::InventoryWorkerPackInsertRequested:
        return handle_inventory_worker_pack_insert_requested(
            context,
            message.payload_as<InventoryWorkerPackInsertRequestedMessage>());

    case GameMessageType::InventoryItemUseRequested:
        return handle_inventory_item_use(
            context,
            message.payload_as<InventoryItemUseRequestedMessage>());

    case GameMessageType::InventoryItemConsumeRequested:
        return handle_inventory_item_consume(
            context,
            message.payload_as<InventoryItemConsumeRequestedMessage>());

    case GameMessageType::InventoryGlobalItemConsumeRequested:
        return handle_inventory_global_item_consume(
            context,
            message.payload_as<InventoryGlobalItemConsumeRequestedMessage>());

    case GameMessageType::InventoryTransferRequested:
        return handle_inventory_transfer(
            context,
            message.payload_as<InventoryTransferRequestedMessage>());

    case GameMessageType::InventoryItemSubmitRequested:
        return handle_inventory_item_submit(
            context,
            message.payload_as<InventoryItemSubmitRequestedMessage>());

    case GameMessageType::InventoryStorageViewRequest:
        return handle_inventory_storage_view_request(
            context,
            message.payload_as<InventoryStorageViewRequestMessage>());

    case GameMessageType::InventoryCraftCommitRequested:
        return handle_inventory_craft_commit(
            context,
            message.payload_as<InventoryCraftCommitRequestedMessage>());

    default:
        return GS1_STATUS_OK;
    }
}

void InventorySystem::run(SiteSystemContext<InventorySystem>& context)
{
    if (ensure_inventory_storage_initialized(context))
    {
        context.world.mark_inventory_storage_descriptors_projection_dirty();
    }
    close_opened_device_storage_if_out_of_range(context);
    progress_pending_device_storage_open(context);
    progress_pending_deliveries(context);
}
}  // namespace gs1
