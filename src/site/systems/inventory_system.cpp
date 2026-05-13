#include "site/systems/inventory_system.h"

#include "campaign/campaign_state.h"
#include "campaign/systems/technology_system.h"
#include "content/defs/craft_recipe_defs.h"
#include "content/defs/item_defs.h"
#include "content/defs/structure_defs.h"
#include "runtime/game_runtime.h"
#include "site/craft_logic.h"
#include "site/defs/site_action_defs.h"
#include "site/inventory_state.h"
#include "site/inventory_storage.h"
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
template <>
struct system_state_tags<InventorySystem>
{
    using type = type_list<
        RuntimeCampaignTag,
        RuntimeActiveSiteRunTag,
        RuntimeFixedStepSecondsTag>;
};

namespace
{
constexpr std::uint32_t k_delivery_box_storage_flags =
    inventory_storage::k_inventory_storage_flag_delivery_box |
    inventory_storage::k_inventory_storage_flag_retrieval_only;

[[nodiscard]] auto inventory_access(RuntimeInvocation& invocation)
    -> GameStateAccess<InventorySystem>
{
    return make_game_state_access<InventorySystem>(invocation);
}

[[nodiscard]] const CampaignState& inventory_campaign(RuntimeInvocation& invocation)
{
    auto access = inventory_access(invocation);
    auto& campaign = access.template read<RuntimeCampaignTag>();
    return *campaign;
}

[[nodiscard]] SiteRunState& inventory_site_run(RuntimeInvocation& invocation)
{
    auto access = inventory_access(invocation);
    auto& site_run = access.template read<RuntimeActiveSiteRunTag>();
    return *site_run;
}

[[nodiscard]] SiteWorldAccess<InventorySystem> inventory_world(RuntimeInvocation& invocation)
{
    return SiteWorldAccess<InventorySystem> {inventory_site_run(invocation)};
}

[[nodiscard]] GameMessageQueue& inventory_message_queue(RuntimeInvocation& invocation)
{
    return invocation.game_message_queue();
}

std::uint32_t normalize_quantity(std::uint32_t quantity) noexcept
{
    return quantity == 0U ? 1U : quantity;
}

Gs1Status resolve_inventory_item_use(
    RuntimeInvocation& invocation,
    flecs::entity item_entity,
    std::uint32_t quantity) noexcept;

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

bool ensure_device_storage_containers(RuntimeInvocation& invocation) noexcept;

bool storage_is_retrieval_only(const StorageContainerState& storage_state) noexcept
{
    return (storage_state.flags & inventory_storage::k_inventory_storage_flag_retrieval_only) != 0U;
}

bool ensure_inventory_storage_initialized(RuntimeInvocation& invocation) noexcept
{
    auto& inventory = inventory_world(invocation).own_inventory();
    const bool resized =
        inventory.worker_pack_slots.size() != inventory.worker_pack_slot_count ||
        !inventory_storage::storage_initialized(inventory_site_run(invocation));

    inventory_storage::import_projection_views_into_storage_if_needed(inventory_site_run(invocation));
    (void)ensure_device_storage_containers(invocation);
    return resized;
}

bool ensure_device_storage_containers(RuntimeInvocation& invocation) noexcept
{
    if (!inventory_world(invocation).has_world() || inventory_site_run(invocation).site_world == nullptr)
    {
        return false;
    }

    bool created_any = false;
    auto source_query =
        inventory_site_run(invocation).site_world->ecs_world()
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
                    inventory_site_run(invocation),
                    device_entity.id());
            const std::uint32_t storage_flags =
                (coord.x == inventory_world(invocation).read_camp().delivery_box_tile.x &&
                    coord.y == inventory_world(invocation).read_camp().delivery_box_tile.y)
                    ? k_delivery_box_storage_flags
                    : 0U;
            (void)inventory_storage::ensure_device_storage_container(
                inventory_site_run(invocation),
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
        // Storage membership is reflected through the exported state view.
    }

    return created_any;
}

flecs::entity resolve_delivery_box_container(RuntimeInvocation& invocation) noexcept
{
    if (!inventory_world(invocation).has_world() || inventory_site_run(invocation).site_world == nullptr)
    {
        return {};
    }

    const auto tile = inventory_world(invocation).read_camp().delivery_box_tile;
    if (!inventory_world(invocation).tile_coord_in_bounds(tile))
    {
        return {};
    }

    const auto tile_data = inventory_world(invocation).read_tile(tile);
    const auto* structure_def = find_structure_def(tile_data.device.structure_id);
    if (structure_def == nullptr || !structure_def->grants_storage || structure_def->storage_slot_count == 0U)
    {
        return {};
    }

    const auto device_entity_id = inventory_site_run(invocation).site_world->device_entity_id(tile);
    if (device_entity_id == 0U)
    {
        return {};
    }

    return inventory_storage::ensure_device_storage_container(
        inventory_site_run(invocation),
        device_entity_id,
        tile,
        structure_def->storage_slot_count,
        k_delivery_box_storage_flags);
}

void mark_changed_slot_views(
    RuntimeInvocation& invocation,
    const std::vector<InventorySlot>& before_worker) noexcept
{
    const auto& inventory = inventory_world(invocation).read_inventory();
    if (before_worker.size() == inventory.worker_pack_slots.size())
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
                break;
            }
        }
    }
}

template <typename Func>
Gs1Status mutate_inventory_storage(
    RuntimeInvocation& invocation,
    Func&& func)
{
    const bool storage_changed = ensure_inventory_storage_initialized(invocation);
    const auto before_worker = inventory_world(invocation).read_inventory().worker_pack_slots;
    const auto status = func();
    if (status != GS1_STATUS_OK)
    {
        return status;
    }

    (void)storage_changed;
    mark_changed_slot_views(invocation, before_worker);
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
    RuntimeInvocation& invocation,
    ItemId item_id,
    std::uint32_t quantity) noexcept
{
    GameMessage completed_message {};
    completed_message.type = GameMessageType::InventoryItemUseCompleted;
    completed_message.set_payload(InventoryItemUseCompletedMessage {
        item_id.value,
        static_cast<std::uint16_t>(std::min<std::uint32_t>(quantity, 65535U)),
        0U});
    inventory_message_queue(invocation).push_back(completed_message);
}

void emit_item_use_action_request(
    RuntimeInvocation& invocation,
    ActionKind action_kind,
    const ItemDef& item_def,
    std::uint16_t quantity) noexcept
{
    const auto worker = inventory_world(invocation).read_worker();
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
    inventory_message_queue(invocation).push_back(message);
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
    RuntimeInvocation& invocation,
    PendingDelivery& delivery) noexcept
{
    const auto delivery_box = resolve_delivery_box_container(invocation);
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
            inventory_site_run(invocation),
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

void seed_inventory_from_loadout(RuntimeInvocation& invocation) noexcept
{
    ensure_inventory_storage_initialized(invocation);
    if (inventory_has_any_item(inventory_world(invocation).read_inventory()) || storage_has_any_item(inventory_site_run(invocation)))
    {
        return;
    }

    const auto& planner = inventory_campaign(invocation).loadout_planner_state;
    const auto& loadout_slots =
        planner.selected_loadout_slots.empty()
            ? planner.baseline_deployment_items
            : planner.selected_loadout_slots;
    if (loadout_slots.empty())
    {
        return;
    }

    const auto delivery_box = resolve_delivery_box_container(invocation);
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
            inventory_site_run(invocation),
            delivery_box,
            slot.item_id,
            slot.quantity);
    }
}

Gs1Status handle_inventory_item_use(
    RuntimeInvocation& invocation,
    const InventoryItemUseRequestedMessage& payload) noexcept
{
    return mutate_inventory_storage(invocation, [&]() -> Gs1Status {
        const auto container =
            inventory_storage::container_for_storage_id(inventory_site_run(invocation), payload.storage_id);
        const auto item_entity = inventory_storage::item_entity_for_slot(
            inventory_site_run(invocation),
            container,
            static_cast<std::uint32_t>(payload.slot_index));
        const auto* stack = inventory_storage::stack_data(inventory_site_run(invocation), item_entity);
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

        return resolve_inventory_item_use(invocation, item_entity, payload.quantity);
    });
}

Gs1Status resolve_inventory_item_use(
    RuntimeInvocation& invocation,
    flecs::entity item_entity,
    std::uint32_t quantity) noexcept
{
    const auto* stack = inventory_storage::stack_data(inventory_site_run(invocation), item_entity);
    if (stack == nullptr || stack->quantity == 0U)
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

    const auto requested_quantity = normalize_quantity(quantity);
    if (requested_quantity > stack->quantity)
    {
        return GS1_STATUS_INVALID_ARGUMENT;
    }

    const ActionKind consume_action_kind = consume_action_kind_for_item(*item_def);
    if (consume_action_kind != ActionKind::None)
    {
        emit_item_use_action_request(
            invocation,
            consume_action_kind,
            *item_def,
            static_cast<std::uint16_t>(requested_quantity));
        return GS1_STATUS_OK;
    }

    if (!inventory_storage::consume_quantity_from_item_entity(
            inventory_site_run(invocation),
            item_entity,
            requested_quantity))
    {
        return GS1_STATUS_INVALID_STATE;
    }

    emit_item_use_completed(invocation, stack->item_id, requested_quantity);
    return GS1_STATUS_OK;
}

Gs1Status handle_inventory_item_consume(
    RuntimeInvocation& invocation,
    const InventoryItemConsumeRequestedMessage& payload) noexcept
{
    return mutate_inventory_storage(invocation, [&]() -> Gs1Status {
        const ItemId item_id {payload.item_id};
        const auto quantity = normalize_quantity(payload.quantity);
        if (find_item_def(item_id) == nullptr)
        {
            return GS1_STATUS_NOT_FOUND;
        }

        const auto available_quantity =
            (payload.flags & k_inventory_item_consume_flag_ignore_action_reservations) != 0U
            ? inventory_storage::available_item_quantity_in_container_kind(
                  inventory_site_run(invocation),
                  payload.container_kind,
                  item_id)
            : available_unreserved_item_quantity_in_container_kind(
                  inventory_site_run(invocation),
                  inventory_world(invocation).read_action(),
                  payload.container_kind,
                  item_id);
        if (available_quantity < quantity)
        {
            return GS1_STATUS_INVALID_STATE;
        }

        const auto remaining = inventory_storage::consume_item_type_from_container(
            inventory_site_run(invocation),
            inventory_storage::container_for_kind(inventory_site_run(invocation), payload.container_kind),
            item_id,
            quantity);
        if (remaining != 0U)
        {
            return GS1_STATUS_INVALID_STATE;
        }

        const auto* item_def = find_item_def(item_id);
        if (item_def != nullptr && item_is_directly_usable(*item_def))
        {
            emit_item_use_completed(invocation, item_id, quantity);
        }
        return GS1_STATUS_OK;
    });
}

Gs1Status handle_inventory_global_item_consume(
    RuntimeInvocation& invocation,
    const InventoryGlobalItemConsumeRequestedMessage& payload) noexcept
{
    return mutate_inventory_storage(invocation, [&]() -> Gs1Status {
        const ItemId item_id {payload.item_id};
        const auto quantity = normalize_quantity(payload.quantity);
        if (find_item_def(item_id) == nullptr)
        {
            return GS1_STATUS_NOT_FOUND;
        }

        const auto containers =
            inventory_storage::collect_all_storage_containers(inventory_site_run(invocation), true);
        const auto reserved_quantity =
            (payload.flags & k_inventory_item_consume_flag_ignore_action_reservations) != 0U
            ? 0U
            : total_reserved_item_quantity(inventory_world(invocation).read_action(), item_id);
        const auto available_quantity = inventory_storage::available_item_quantity_in_containers(
            inventory_site_run(invocation),
            containers,
            item_id);
        if (available_quantity < quantity || available_quantity - quantity < reserved_quantity)
        {
            return GS1_STATUS_INVALID_STATE;
        }

        auto ordered_containers = containers;
        if (reserved_quantity > 0U)
        {
            const auto worker_pack = inventory_storage::worker_pack_container(inventory_site_run(invocation));
            std::stable_partition(
                ordered_containers.begin(),
                ordered_containers.end(),
                [&](const flecs::entity& container) {
                    return container.id() != worker_pack.id();
                });
        }

        const auto remaining = inventory_storage::consume_item_type_from_containers(
            inventory_site_run(invocation),
            ordered_containers,
            item_id,
            quantity);
        return remaining == 0U ? GS1_STATUS_OK : GS1_STATUS_INVALID_STATE;
    });
}

Gs1Status handle_inventory_craft_commit(
    RuntimeInvocation& invocation,
    const InventoryCraftCommitRequestedMessage& payload) noexcept
{
    return mutate_inventory_storage(invocation, [&]() -> Gs1Status {
        const auto* recipe_def = find_craft_recipe_def(RecipeId {payload.recipe_id});
        if (recipe_def == nullptr)
        {
            return GS1_STATUS_NOT_FOUND;
        }

        if (!TechnologySystem::recipe_unlocked(inventory_campaign(invocation), recipe_def->recipe_id))
        {
            return GS1_STATUS_INVALID_STATE;
        }

        const TileCoord target_tile {payload.target_tile_x, payload.target_tile_y};
        if (!inventory_world(invocation).tile_coord_in_bounds(target_tile))
        {
            return GS1_STATUS_INVALID_ARGUMENT;
        }

        const auto tile = inventory_world(invocation).read_tile(target_tile);
        if (tile.device.structure_id != recipe_def->station_structure_id)
        {
            return GS1_STATUS_INVALID_STATE;
        }

        const auto* structure_def = find_structure_def(tile.device.structure_id);
        if (structure_def == nullptr || !structure_def->grants_storage || structure_def->storage_slot_count == 0U)
        {
            return GS1_STATUS_INVALID_STATE;
        }

        if (inventory_site_run(invocation).site_world == nullptr)
        {
            return GS1_STATUS_INVALID_STATE;
        }

        const auto device_entity_id = inventory_site_run(invocation).site_world->device_entity_id(target_tile);
        if (device_entity_id == 0U)
        {
            return GS1_STATUS_INVALID_STATE;
        }

        const auto output_container = inventory_storage::ensure_device_storage_container(
            inventory_site_run(invocation),
            device_entity_id,
            target_tile,
            structure_def->storage_slot_count);
        const auto source_containers =
            craft_logic::collect_nearby_source_containers(inventory_site_run(invocation), target_tile);
        const auto nearby_item_instance_ids =
            inventory_storage::collect_item_instance_ids_in_containers(
                inventory_site_run(invocation),
                source_containers);
        if (!craft_logic::can_store_output_after_recipe_consumption(
                inventory_site_run(invocation),
                output_container,
                source_containers,
                *recipe_def))
        {
            return GS1_STATUS_INVALID_STATE;
        }
        if (!craft_logic::can_satisfy_recipe_requirements(
                inventory_site_run(invocation),
                nearby_item_instance_ids,
                *recipe_def))
        {
            return GS1_STATUS_INVALID_STATE;
        }

        for (std::uint8_t index = 0U; index < recipe_def->ingredient_count; ++index)
        {
            const auto& ingredient = recipe_def->ingredients[index];
            const auto remaining = inventory_storage::consume_item_type_from_containers(
                inventory_site_run(invocation),
                source_containers,
                ingredient.item_id,
                ingredient.quantity);
            if (remaining != 0U)
            {
                return GS1_STATUS_INVALID_STATE;
            }
        }

        const auto remaining_output = inventory_storage::add_item_to_container(
            inventory_site_run(invocation),
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
            inventory_storage::storage_id_for_container(inventory_site_run(invocation), output_container)});
        inventory_message_queue(invocation).push_back(completed_message);
        return GS1_STATUS_OK;
    });
}

Gs1Status handle_site_device_broken(
    RuntimeInvocation& invocation,
    const SiteDeviceBrokenMessage& payload) noexcept
{
    return mutate_inventory_storage(invocation, [&]() -> Gs1Status {
        const auto container =
            inventory_storage::find_device_storage_container(inventory_site_run(invocation), payload.device_entity_id);
        if (!container.is_valid())
        {
            return GS1_STATUS_OK;
        }

        const auto storage_id =
            inventory_storage::storage_id_for_container(inventory_site_run(invocation), container);
        (void)inventory_storage::destroy_storage_container(inventory_site_run(invocation), container);
        return GS1_STATUS_OK;
    });
}

Gs1Status resolve_inventory_transfer(
    RuntimeInvocation& invocation,
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

    const auto* source_storage = storage_state_for_transfer(inventory_site_run(invocation), payload.source_storage_id);
    const auto* destination_storage =
        storage_state_for_transfer(inventory_site_run(invocation), payload.destination_storage_id);
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
            inventory_storage::container_for_storage_id(inventory_site_run(invocation), payload.source_storage_id);
        const auto destination_container =
            inventory_storage::container_for_storage_id(inventory_site_run(invocation), payload.destination_storage_id);
        if (!source_container.is_valid() || !destination_container.is_valid())
        {
            return GS1_STATUS_INVALID_STATE;
        }

        const auto source_item = inventory_storage::item_entity_for_slot(
            inventory_site_run(invocation),
            source_container,
            payload.source_slot_index);
        const auto* source_stack = inventory_storage::stack_data(inventory_site_run(invocation), source_item);
        if (source_stack == nullptr || source_stack->quantity == 0U)
        {
            return GS1_STATUS_INVALID_STATE;
        }

        const auto requested_quantity =
            payload.quantity == 0U
                ? source_stack->quantity
                : std::min<std::uint32_t>(normalize_quantity(payload.quantity), source_stack->quantity);
        const auto source_reserved = reserved_item_quantity_in_container(
            inventory_world(invocation).read_action(),
            source_storage->container_kind,
            source_stack->item_id);
        if (source_reserved > 0U)
        {
            const auto source_available = inventory_storage::available_item_quantity_in_container_kind(
                inventory_site_run(invocation),
                source_storage->container_kind,
                source_stack->item_id);
            if (source_available < requested_quantity ||
                source_available - requested_quantity < source_reserved)
            {
                return GS1_STATUS_INVALID_STATE;
            }
        }
        const auto remaining_quantity = inventory_storage::add_item_to_container(
            inventory_site_run(invocation),
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
                inventory_site_run(invocation),
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
        inventory_message_queue(invocation).push_back(completed_message);
        return GS1_STATUS_OK;
    }

    const auto source_container =
        inventory_storage::container_for_storage_id(inventory_site_run(invocation), payload.source_storage_id);
    const auto source_item = inventory_storage::item_entity_for_slot(
        inventory_site_run(invocation),
        source_container,
        payload.source_slot_index);
    const auto* source_stack = inventory_storage::stack_data(inventory_site_run(invocation), source_item);
    if (source_stack == nullptr || source_stack->quantity == 0U)
    {
        return GS1_STATUS_INVALID_STATE;
    }

    const auto source_reserved = reserved_item_quantity_in_container(
        inventory_world(invocation).read_action(),
        source_storage->container_kind,
        source_stack->item_id);
    if (source_reserved > 0U)
    {
        const auto transfer_quantity =
            std::min<std::uint32_t>(normalize_quantity(payload.quantity), source_stack->quantity);
        const auto source_available = inventory_storage::available_item_quantity_in_container_kind(
            inventory_site_run(invocation),
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
        inventory_site_run(invocation),
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
    inventory_message_queue(invocation).push_back(completed_message);
    return GS1_STATUS_OK;
}

Gs1Status handle_inventory_transfer(
    RuntimeInvocation& invocation,
    const InventoryTransferRequestedMessage& payload) noexcept
{
    return mutate_inventory_storage(invocation, [&]() -> Gs1Status {
        return resolve_inventory_transfer(invocation, payload);
    });
}

Gs1Status handle_inventory_slot_tapped(
    RuntimeInvocation& invocation,
    const InventorySlotTappedMessage& payload) noexcept
{
    if (payload.storage_id == 0U ||
        (payload.container_kind != GS1_INVENTORY_CONTAINER_WORKER_PACK &&
            payload.container_kind != GS1_INVENTORY_CONTAINER_DEVICE_STORAGE))
    {
        return GS1_STATUS_INVALID_ARGUMENT;
    }

    return mutate_inventory_storage(invocation, [&]() -> Gs1Status {
        const auto* source_storage =
            inventory_storage::storage_container_state_for_storage_id(
                inventory_site_run(invocation),
                payload.storage_id);
        if (source_storage == nullptr ||
            source_storage->container_kind != payload.container_kind)
        {
            return GS1_STATUS_OK;
        }

        const auto source_container =
            inventory_storage::container_for_storage_id(inventory_site_run(invocation), payload.storage_id);
        const auto source_item = inventory_storage::item_entity_for_slot(
            inventory_site_run(invocation),
            source_container,
            payload.slot_index);
        const auto* source_stack = inventory_storage::stack_data(inventory_site_run(invocation), source_item);
        if (source_stack == nullptr || source_stack->quantity == 0U)
        {
            return GS1_STATUS_OK;
        }

        if (payload.item_instance_id != 0U &&
            payload.item_instance_id != static_cast<std::uint32_t>(source_item.id()))
        {
            return GS1_STATUS_OK;
        }

        if (payload.container_kind == GS1_INVENTORY_CONTAINER_DEVICE_STORAGE)
        {
            const auto destination_storage_id = inventory_world(invocation).read_inventory().worker_pack_storage_id;
            if (destination_storage_id == 0U ||
                destination_storage_id == payload.storage_id)
            {
                return GS1_STATUS_OK;
            }

            InventoryTransferRequestedMessage transfer {};
            transfer.source_storage_id = payload.storage_id;
            transfer.destination_storage_id = destination_storage_id;
            transfer.source_slot_index = payload.slot_index;
            transfer.quantity = static_cast<std::uint16_t>(
                std::min<std::uint32_t>(source_stack->quantity, 65535U));
            transfer.flags = k_inventory_transfer_flag_resolve_destination_in_dll;
            return resolve_inventory_transfer(invocation, transfer);
        }

        if (payload.container_kind != GS1_INVENTORY_CONTAINER_WORKER_PACK)
        {
            return GS1_STATUS_OK;
        }

        if (payload.companion_storage_id != 0U &&
            payload.companion_storage_id != payload.storage_id)
        {
            const auto* destination_storage =
                inventory_storage::storage_container_state_for_storage_id(
                    inventory_site_run(invocation),
                    payload.companion_storage_id);
            if (destination_storage != nullptr &&
                destination_storage->container_kind == GS1_INVENTORY_CONTAINER_DEVICE_STORAGE &&
                !storage_is_retrieval_only(*destination_storage))
            {
                InventoryTransferRequestedMessage transfer {};
                transfer.source_storage_id = payload.storage_id;
                transfer.destination_storage_id = payload.companion_storage_id;
                transfer.source_slot_index = payload.slot_index;
                transfer.quantity = static_cast<std::uint16_t>(
                    std::min<std::uint32_t>(source_stack->quantity, 65535U));
                transfer.flags = k_inventory_transfer_flag_resolve_destination_in_dll;
                return resolve_inventory_transfer(invocation, transfer);
            }
        }

        const auto* item_def = find_item_def(source_stack->item_id);
        if (item_def == nullptr || !item_is_directly_usable(*item_def))
        {
            return GS1_STATUS_OK;
        }

        return resolve_inventory_item_use(
            invocation,
            source_item,
            1U);
    });
}

Gs1Status handle_inventory_item_submit(
    RuntimeInvocation& invocation,
    const InventoryItemSubmitRequestedMessage& payload) noexcept
{
    return mutate_inventory_storage(invocation, [&]() -> Gs1Status {
        const auto* source_storage = storage_state_for_transfer(inventory_site_run(invocation), payload.source_storage_id);
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
            inventory_storage::container_for_storage_id(inventory_site_run(invocation), payload.source_storage_id);
        const auto source_item = inventory_storage::item_entity_for_slot(
            inventory_site_run(invocation),
            source_container,
            payload.source_slot_index);
        const auto* source_stack = inventory_storage::stack_data(inventory_site_run(invocation), source_item);
        if (source_stack == nullptr || source_stack->quantity == 0U)
        {
            return GS1_STATUS_INVALID_ARGUMENT;
        }

        const auto requested_quantity =
            std::min<std::uint32_t>(normalize_quantity(payload.quantity), source_stack->quantity);
        const auto source_reserved = reserved_item_quantity_in_container(
            inventory_world(invocation).read_action(),
            source_storage->container_kind,
            source_stack->item_id);
        if (source_reserved > 0U)
        {
            const auto source_available = inventory_storage::available_item_quantity_in_container_kind(
                inventory_site_run(invocation),
                source_storage->container_kind,
                source_stack->item_id);
            if (source_available < requested_quantity ||
                source_available - requested_quantity < source_reserved)
            {
                return GS1_STATUS_INVALID_STATE;
            }
        }

        if (!inventory_storage::consume_quantity_from_item_entity(
                inventory_site_run(invocation),
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
        inventory_message_queue(invocation).push_back(completed_message);
        return GS1_STATUS_OK;
    });
}

Gs1Status handle_inventory_delivery_requested(
    RuntimeInvocation& invocation,
    const InventoryDeliveryRequestedMessage& payload) noexcept
{
    return mutate_inventory_storage(invocation, [&]() -> Gs1Status {
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

        try_add_delivery_to_crate(invocation, delivery);
        if (delivery_has_pending_items(delivery))
        {
            inventory_world(invocation).own_inventory().pending_delivery_queue.push_back(std::move(delivery));
        }
        return GS1_STATUS_OK;
    });
}

Gs1Status handle_inventory_delivery_batch_requested(
    RuntimeInvocation& invocation,
    const InventoryDeliveryBatchRequestedMessage& payload) noexcept
{
    return mutate_inventory_storage(invocation, [&]() -> Gs1Status {
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

        try_add_delivery_to_crate(invocation, delivery);
        if (delivery_has_pending_items(delivery))
        {
            inventory_world(invocation).own_inventory().pending_delivery_queue.push_back(std::move(delivery));
        }
        return GS1_STATUS_OK;
    });
}

Gs1Status handle_inventory_worker_pack_insert_requested(
    RuntimeInvocation& invocation,
    const InventoryWorkerPackInsertRequestedMessage& payload) noexcept
{
    return mutate_inventory_storage(invocation, [&]() -> Gs1Status {
        const ItemId item_id {payload.item_id};
        const std::uint32_t quantity = normalize_quantity(payload.quantity);
        if (item_id.value == 0U || find_item_def(item_id) == nullptr)
        {
            return GS1_STATUS_NOT_FOUND;
        }

        const auto worker_pack = inventory_storage::worker_pack_container(inventory_site_run(invocation));
        if (!worker_pack.is_valid() ||
            !inventory_storage::can_fit_item_in_container(
                inventory_site_run(invocation),
                worker_pack,
                item_id,
                quantity))
        {
            return GS1_STATUS_INVALID_STATE;
        }

        return inventory_storage::add_item_to_container(
                   inventory_site_run(invocation),
                   worker_pack,
                   item_id,
                   quantity) == 0U
            ? GS1_STATUS_OK
            : GS1_STATUS_INVALID_STATE;
    });
}

void progress_pending_deliveries(RuntimeInvocation& invocation) noexcept
{
    ensure_inventory_storage_initialized(invocation);
    auto& inventory = inventory_world(invocation).own_inventory();
    if (inventory.pending_delivery_queue.empty())
    {
        return;
    }

    const auto before_worker = inventory.worker_pack_slots;
    for (auto& delivery : inventory.pending_delivery_queue)
    {
        try_add_delivery_to_crate(invocation, delivery);
    }

    inventory.pending_delivery_queue.erase(
        std::remove_if(
            inventory.pending_delivery_queue.begin(),
            inventory.pending_delivery_queue.end(),
            [](const PendingDelivery& delivery) {
                return !delivery_has_pending_items(delivery);
            }),
        inventory.pending_delivery_queue.end());

    mark_changed_slot_views(invocation, before_worker);
}

}  // namespace

const char* InventorySystem::name() const noexcept
{
    return "InventorySystem";
}

GameMessageSubscriptionSpan InventorySystem::subscribed_game_messages() const noexcept
{
    static constexpr GameMessageType subscriptions[] = {
        GameMessageType::StartSiteAction,
        GameMessageType::SiteRunStarted,
        GameMessageType::SiteDevicePlaced,
        GameMessageType::SiteDeviceBroken,
        GameMessageType::InventoryDeliveryRequested,
        GameMessageType::InventoryDeliveryBatchRequested,
        GameMessageType::InventoryWorkerPackInsertRequested,
        GameMessageType::InventoryItemUseRequested,
        GameMessageType::InventoryItemConsumeRequested,
        GameMessageType::InventoryGlobalItemConsumeRequested,
        GameMessageType::InventoryTransferRequested,
        GameMessageType::InventoryItemSubmitRequested,
        GameMessageType::InventorySlotTapped,
        GameMessageType::InventoryCraftCommitRequested,
    };
    return subscriptions;
}

HostMessageSubscriptionSpan InventorySystem::subscribed_host_messages() const noexcept
{
    static constexpr Gs1HostMessageType subscriptions[] = {
        GS1_HOST_EVENT_SITE_INVENTORY_SLOT_TAP,
    };
    return subscriptions;
}

std::optional<Gs1RuntimeProfileSystemId> InventorySystem::profile_system_id() const noexcept
{
    return GS1_RUNTIME_PROFILE_SYSTEM_INVENTORY;
}

std::optional<std::uint32_t> InventorySystem::fixed_step_order() const noexcept
{
    return 14U;
}

Gs1Status InventorySystem::process_game_message(
    RuntimeInvocation& invocation,
    const GameMessage& message)
{
    auto access = make_game_state_access<InventorySystem>(invocation);
    auto& campaign = access.template read<RuntimeCampaignTag>();
    auto& site_run = access.template read<RuntimeActiveSiteRunTag>();
    if (!campaign.has_value() || !site_run.has_value())
    {
        return GS1_STATUS_INVALID_STATE;
    }

    switch (message.type)
    {
    case GameMessageType::SiteRunStarted:
        seed_inventory_from_loadout(invocation);
        return GS1_STATUS_OK;

    case GameMessageType::SiteDevicePlaced:
        (void)ensure_inventory_storage_initialized(invocation);
        (void)ensure_device_storage_containers(invocation);
        return GS1_STATUS_OK;

    case GameMessageType::SiteDeviceBroken:
        return handle_site_device_broken(
            invocation,
            message.payload_as<SiteDeviceBrokenMessage>());

    case GameMessageType::InventoryDeliveryRequested:
        return handle_inventory_delivery_requested(
            invocation,
            message.payload_as<InventoryDeliveryRequestedMessage>());

    case GameMessageType::InventoryDeliveryBatchRequested:
        return handle_inventory_delivery_batch_requested(
            invocation,
            message.payload_as<InventoryDeliveryBatchRequestedMessage>());

    case GameMessageType::InventoryWorkerPackInsertRequested:
        return handle_inventory_worker_pack_insert_requested(
            invocation,
            message.payload_as<InventoryWorkerPackInsertRequestedMessage>());

    case GameMessageType::InventoryItemUseRequested:
        return handle_inventory_item_use(
            invocation,
            message.payload_as<InventoryItemUseRequestedMessage>());

    case GameMessageType::InventoryItemConsumeRequested:
        return handle_inventory_item_consume(
            invocation,
            message.payload_as<InventoryItemConsumeRequestedMessage>());

    case GameMessageType::InventoryGlobalItemConsumeRequested:
        return handle_inventory_global_item_consume(
            invocation,
            message.payload_as<InventoryGlobalItemConsumeRequestedMessage>());

    case GameMessageType::InventoryTransferRequested:
        return handle_inventory_transfer(
            invocation,
            message.payload_as<InventoryTransferRequestedMessage>());

    case GameMessageType::InventoryItemSubmitRequested:
        return handle_inventory_item_submit(
            invocation,
            message.payload_as<InventoryItemSubmitRequestedMessage>());

    case GameMessageType::InventorySlotTapped:
        return handle_inventory_slot_tapped(
            invocation,
            message.payload_as<InventorySlotTappedMessage>());

    case GameMessageType::InventoryCraftCommitRequested:
        return handle_inventory_craft_commit(
            invocation,
            message.payload_as<InventoryCraftCommitRequestedMessage>());

    default:
        return GS1_STATUS_OK;
    }
}

Gs1Status InventorySystem::process_host_message(
    RuntimeInvocation& invocation,
    const Gs1HostMessage& message)
{
    auto access = make_game_state_access<InventorySystem>(invocation);
    auto& campaign = access.template read<RuntimeCampaignTag>();
    auto& site_run = access.template read<RuntimeActiveSiteRunTag>();
    if (!campaign.has_value() || !site_run.has_value())
    {
        return GS1_STATUS_INVALID_STATE;
    }

    switch (message.type)
    {
    case GS1_HOST_EVENT_SITE_INVENTORY_SLOT_TAP:
        return handle_inventory_slot_tapped(
            invocation,
            InventorySlotTappedMessage {
                message.payload.site_inventory_slot_tap.storage_id,
                message.payload.site_inventory_slot_tap.item_instance_id,
                message.payload.site_inventory_slot_tap.slot_index,
                message.payload.site_inventory_slot_tap.container_kind,
                0U,
                message.payload.site_inventory_slot_tap.companion_storage_id});

    default:
        return GS1_STATUS_OK;
    }
}

void InventorySystem::run(RuntimeInvocation& invocation)
{
    auto access = make_game_state_access<InventorySystem>(invocation);
    auto& campaign = access.template read<RuntimeCampaignTag>();
    auto& site_run = access.template read<RuntimeActiveSiteRunTag>();
    if (!campaign.has_value() || !site_run.has_value())
    {
        return;
    }

    if (ensure_inventory_storage_initialized(invocation))
    {
        return;
    }
    progress_pending_deliveries(invocation);
}
}  // namespace gs1

