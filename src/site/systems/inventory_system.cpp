#include "site/systems/inventory_system.h"

#include "content/defs/item_defs.h"
#include "site/inventory_state.h"
#include "site/site_projection_update_flags.h"
#include "site/site_run_state.h"

#include <algorithm>
#include <cstddef>

namespace gs1
{
namespace
{
constexpr double k_default_delivery_minutes = 30.0;
constexpr double k_site_minutes_per_step = 0.2;

std::vector<InventorySlot>* container_slots(
    InventoryState& inventory,
    Gs1InventoryContainerKind kind) noexcept
{
    switch (kind)
    {
    case GS1_INVENTORY_CONTAINER_WORKER_PACK:
        return &inventory.worker_pack_slots;
    case GS1_INVENTORY_CONTAINER_CAMP_STORAGE:
        return &inventory.camp_storage_slots;
    default:
        return nullptr;
    }
}

const std::vector<InventorySlot>* container_slots(
    const InventoryState& inventory,
    Gs1InventoryContainerKind kind) noexcept
{
    switch (kind)
    {
    case GS1_INVENTORY_CONTAINER_WORKER_PACK:
        return &inventory.worker_pack_slots;
    case GS1_INVENTORY_CONTAINER_CAMP_STORAGE:
        return &inventory.camp_storage_slots;
    default:
        return nullptr;
    }
}

InventorySlot* find_slot(
    InventoryState& inventory,
    Gs1InventoryContainerKind container_kind,
    std::size_t slot_index) noexcept
{
    auto* slots = container_slots(inventory, container_kind);
    if (slots == nullptr || slot_index >= slots->size())
    {
        return nullptr;
    }

    return &(*slots)[slot_index];
}

bool ensure_inventory_slots(InventoryState& inventory) noexcept
{
    bool resized = false;
    if (inventory.worker_pack_slots.size() != inventory.worker_pack_slot_count)
    {
        inventory.worker_pack_slots.resize(inventory.worker_pack_slot_count);
        resized = true;
    }
    if (inventory.camp_storage_slots.size() != inventory.camp_storage_slot_count)
    {
        inventory.camp_storage_slots.resize(inventory.camp_storage_slot_count);
        resized = true;
    }

    return resized;
}

void clear_slot(InventorySlot& slot) noexcept
{
    slot = InventorySlot {};
}

std::uint32_t normalize_quantity(std::uint32_t quantity) noexcept
{
    return quantity == 0U ? 1U : quantity;
}

bool inventory_has_any_item(const InventoryState& inventory) noexcept
{
    const auto has_item = [](const InventorySlot& slot) { return slot.occupied; };
    return std::any_of(
               inventory.worker_pack_slots.begin(),
               inventory.worker_pack_slots.end(),
               has_item) ||
        std::any_of(
            inventory.camp_storage_slots.begin(),
            inventory.camp_storage_slots.end(),
            has_item);
}

void assign_slot(
    InventorySlot& slot,
    ItemId item_id,
    std::uint32_t quantity,
    float item_condition = 1.0f,
    float item_freshness = 1.0f) noexcept
{
    slot.item_id = item_id;
    slot.item_quantity = quantity;
    slot.item_condition = item_condition;
    slot.item_freshness = item_freshness;
    slot.occupied = quantity > 0U;
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

std::uint32_t available_item_quantity(
    const InventoryState& inventory,
    Gs1InventoryContainerKind container_kind,
    ItemId item_id) noexcept
{
    const auto* slots = container_slots(inventory, container_kind);
    if (slots == nullptr)
    {
        return 0U;
    }

    std::uint32_t total_quantity = 0U;
    for (const auto& slot : *slots)
    {
        if (!slot.occupied || slot.item_id != item_id)
        {
            continue;
        }

        total_quantity += slot.item_quantity;
    }

    return total_quantity;
}

void track_touched_slot(std::vector<std::size_t>* touched_slots, std::size_t slot_index) noexcept
{
    if (touched_slots == nullptr)
    {
        return;
    }

    const auto existing = std::find(touched_slots->begin(), touched_slots->end(), slot_index);
    if (existing == touched_slots->end())
    {
        touched_slots->push_back(slot_index);
    }
}

void mark_touched_inventory_slots(
    SiteSystemContext<InventorySystem>& context,
    Gs1InventoryContainerKind container_kind,
    const std::vector<std::size_t>& touched_slots) noexcept
{
    for (const auto slot_index : touched_slots)
    {
        context.world.mark_inventory_slot_projection_dirty(
            container_kind,
            static_cast<std::uint32_t>(slot_index));
    }
}

std::uint32_t add_item_to_slots(
    std::vector<InventorySlot>& slots,
    ItemId item_id,
    std::uint32_t quantity,
    float item_condition = 1.0f,
    float item_freshness = 1.0f,
    std::vector<std::size_t>* touched_slots = nullptr) noexcept
{
    std::uint32_t remaining_quantity = quantity;
    const auto stack_limit = item_stack_size(item_id);

    for (std::size_t slot_index = 0; slot_index < slots.size(); ++slot_index)
    {
        auto& slot = slots[slot_index];
        if (!slot.occupied || slot.item_id != item_id || slot.item_quantity >= stack_limit)
        {
            continue;
        }

        const auto free_capacity = stack_limit - slot.item_quantity;
        const auto transfer_quantity = std::min(remaining_quantity, free_capacity);
        slot.item_quantity += transfer_quantity;
        track_touched_slot(touched_slots, slot_index);
        remaining_quantity -= transfer_quantity;
        if (remaining_quantity == 0U)
        {
            return 0U;
        }
    }

    for (std::size_t slot_index = 0; slot_index < slots.size(); ++slot_index)
    {
        auto& slot = slots[slot_index];
        if (slot.occupied)
        {
            continue;
        }

        const auto transfer_quantity = std::min(remaining_quantity, stack_limit);
        assign_slot(slot, item_id, transfer_quantity, item_condition, item_freshness);
        track_touched_slot(touched_slots, slot_index);
        remaining_quantity -= transfer_quantity;
        if (remaining_quantity == 0U)
        {
            return 0U;
        }
    }

    return remaining_quantity;
}

std::uint32_t add_item_to_container(
    InventoryState& inventory,
    Gs1InventoryContainerKind container_kind,
    ItemId item_id,
    std::uint32_t quantity,
    float item_condition = 1.0f,
    float item_freshness = 1.0f,
    std::vector<std::size_t>* touched_slots = nullptr) noexcept
{
    auto* slots = container_slots(inventory, container_kind);
    if (slots == nullptr)
    {
        return quantity;
    }

    return add_item_to_slots(*slots, item_id, quantity, item_condition, item_freshness, touched_slots);
}

std::uint32_t consume_item_from_slots(
    std::vector<InventorySlot>& slots,
    ItemId item_id,
    std::uint32_t quantity,
    std::vector<std::size_t>* touched_slots = nullptr) noexcept
{
    std::uint32_t remaining_quantity = quantity;

    for (std::size_t slot_index = 0; slot_index < slots.size(); ++slot_index)
    {
        auto& slot = slots[slot_index];
        if (!slot.occupied || slot.item_id != item_id)
        {
            continue;
        }

        const auto removed_quantity = std::min(remaining_quantity, slot.item_quantity);
        slot.item_quantity -= removed_quantity;
        track_touched_slot(touched_slots, slot_index);
        remaining_quantity -= removed_quantity;
        if (slot.item_quantity == 0U)
        {
            clear_slot(slot);
        }
        if (remaining_quantity == 0U)
        {
            return 0U;
        }
    }

    return remaining_quantity;
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

void seed_inventory_for_site_one(SiteSystemContext<InventorySystem>& context) noexcept
{
    auto& inventory = context.world.own_inventory();
    if (context.world.site_id_value() != 1U)
    {
        return;
    }

    ensure_inventory_slots(inventory);
    if (inventory_has_any_item(inventory))
    {
        return;
    }

    add_item_to_container(
        inventory,
        GS1_INVENTORY_CONTAINER_WORKER_PACK,
        ItemId {k_item_water_container},
        2U);
    add_item_to_container(
        inventory,
        GS1_INVENTORY_CONTAINER_WORKER_PACK,
        ItemId {k_item_food_pack},
        1U);
    add_item_to_container(
        inventory,
        GS1_INVENTORY_CONTAINER_WORKER_PACK,
        ItemId {k_item_medicine_pack},
        1U);

    add_item_to_container(
        inventory,
        GS1_INVENTORY_CONTAINER_CAMP_STORAGE,
        ItemId {k_item_wind_reed_seed_bundle},
        8U);
    add_item_to_container(
        inventory,
        GS1_INVENTORY_CONTAINER_CAMP_STORAGE,
        ItemId {k_item_saltbush_seed_bundle},
        6U);
    add_item_to_container(
        inventory,
        GS1_INVENTORY_CONTAINER_CAMP_STORAGE,
        ItemId {k_item_shade_cactus_seed_bundle},
        4U);
    add_item_to_container(
        inventory,
        GS1_INVENTORY_CONTAINER_CAMP_STORAGE,
        ItemId {k_item_sunfruit_vine_seed_bundle},
        3U);
}

Gs1Status handle_inventory_item_use(
    SiteSystemContext<InventorySystem>& context,
    const InventoryItemUseRequestedCommand& payload) noexcept
{
    auto& inventory = context.world.own_inventory();
    const bool inventory_resized = ensure_inventory_slots(inventory);
    auto* slot = find_slot(
        inventory,
        payload.container_kind,
        static_cast<std::size_t>(payload.slot_index));
    if (slot == nullptr || !slot->occupied)
    {
        return GS1_STATUS_INVALID_ARGUMENT;
    }

    if (payload.item_id != 0U && payload.item_id != slot->item_id.value)
    {
        return GS1_STATUS_INVALID_ARGUMENT;
    }

    const auto* item_def = find_item_def(slot->item_id);
    if (item_def == nullptr)
    {
        return GS1_STATUS_NOT_FOUND;
    }

    if (!item_is_directly_usable(*item_def))
    {
        return GS1_STATUS_INVALID_STATE;
    }

    const auto requested_quantity = normalize_quantity(payload.quantity);
    if (requested_quantity > slot->item_quantity)
    {
        return GS1_STATUS_INVALID_ARGUMENT;
    }

    emit_item_use_effect(context, *item_def, requested_quantity);
    slot->item_quantity -= requested_quantity;
    if (slot->item_quantity == 0U)
    {
        clear_slot(*slot);
    }

    if (inventory_resized)
    {
        context.world.mark_projection_dirty(SITE_PROJECTION_UPDATE_INVENTORY);
    }
    context.world.mark_inventory_slot_projection_dirty(
        payload.container_kind,
        static_cast<std::uint32_t>(payload.slot_index));
    return GS1_STATUS_OK;
}

Gs1Status handle_inventory_item_consume(
    SiteSystemContext<InventorySystem>& context,
    const InventoryItemConsumeRequestedCommand& payload) noexcept
{
    auto& inventory = context.world.own_inventory();
    const bool inventory_resized = ensure_inventory_slots(inventory);

    const ItemId item_id {payload.item_id};
    const auto quantity = normalize_quantity(payload.quantity);
    if (find_item_def(item_id) == nullptr)
    {
        return GS1_STATUS_NOT_FOUND;
    }

    if (available_item_quantity(inventory, payload.container_kind, item_id) < quantity)
    {
        return GS1_STATUS_INVALID_STATE;
    }

    auto* slots = container_slots(inventory, payload.container_kind);
    if (slots == nullptr)
    {
        return GS1_STATUS_INVALID_ARGUMENT;
    }

    std::vector<std::size_t> touched_slots {};
    const auto remaining_quantity = consume_item_from_slots(*slots, item_id, quantity, &touched_slots);
    if (remaining_quantity != 0U)
    {
        return GS1_STATUS_INVALID_STATE;
    }

    if (inventory_resized)
    {
        context.world.mark_projection_dirty(SITE_PROJECTION_UPDATE_INVENTORY);
    }
    mark_touched_inventory_slots(context, payload.container_kind, touched_slots);
    return GS1_STATUS_OK;
}

Gs1Status handle_inventory_transfer(
    SiteSystemContext<InventorySystem>& context,
    const InventoryTransferRequestedCommand& payload) noexcept
{
    if (payload.source_slot_index == payload.destination_slot_index &&
        payload.source_container_kind == payload.destination_container_kind)
    {
        return GS1_STATUS_OK;
    }

    auto& inventory = context.world.own_inventory();
    const bool inventory_resized = ensure_inventory_slots(inventory);
    auto* source_slot = find_slot(
        inventory,
        payload.source_container_kind,
        static_cast<std::size_t>(payload.source_slot_index));
    auto* destination_slot = find_slot(
        inventory,
        payload.destination_container_kind,
        static_cast<std::size_t>(payload.destination_slot_index));
    if (source_slot == nullptr || destination_slot == nullptr || !source_slot->occupied)
    {
        return GS1_STATUS_INVALID_ARGUMENT;
    }

    const auto transfer_quantity = normalize_quantity(payload.quantity);
    if (transfer_quantity > source_slot->item_quantity)
    {
        return GS1_STATUS_INVALID_ARGUMENT;
    }

    const auto stack_limit = item_stack_size(source_slot->item_id);
    std::uint32_t actual_transfer_quantity = transfer_quantity;

    if (!destination_slot->occupied)
    {
        actual_transfer_quantity = std::min(actual_transfer_quantity, stack_limit);
        assign_slot(
            *destination_slot,
            source_slot->item_id,
            actual_transfer_quantity,
            source_slot->item_condition,
            source_slot->item_freshness);
    }
    else if (destination_slot->item_id != source_slot->item_id)
    {
        return GS1_STATUS_INVALID_ARGUMENT;
    }
    else
    {
        const auto free_capacity =
            stack_limit > destination_slot->item_quantity
                ? stack_limit - destination_slot->item_quantity
                : 0U;
        if (free_capacity == 0U)
        {
            return GS1_STATUS_INVALID_STATE;
        }

        actual_transfer_quantity = std::min(actual_transfer_quantity, free_capacity);
        destination_slot->item_quantity += actual_transfer_quantity;
    }

    source_slot->item_quantity -= actual_transfer_quantity;
    if (source_slot->item_quantity == 0U)
    {
        clear_slot(*source_slot);
    }

    if (inventory_resized)
    {
        context.world.mark_projection_dirty(SITE_PROJECTION_UPDATE_INVENTORY);
    }
    context.world.mark_inventory_slot_projection_dirty(
        payload.source_container_kind,
        static_cast<std::uint32_t>(payload.source_slot_index));
    context.world.mark_inventory_slot_projection_dirty(
        payload.destination_container_kind,
        static_cast<std::uint32_t>(payload.destination_slot_index));
    return GS1_STATUS_OK;
}

Gs1Status handle_inventory_delivery_requested(
    SiteSystemContext<InventorySystem>& context,
    const InventoryDeliveryRequestedCommand& payload) noexcept
{
    auto& inventory = context.world.own_inventory();
    const bool inventory_resized = ensure_inventory_slots(inventory);
    if (inventory_resized)
    {
        context.world.mark_projection_dirty(SITE_PROJECTION_UPDATE_INVENTORY);
    }

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
    assign_slot(
        delivery.item_stacks.back(),
        item_id,
        normalize_quantity(payload.quantity));
    inventory.pending_delivery_queue.push_back(delivery);
    return GS1_STATUS_OK;
}

void progress_pending_deliveries(SiteSystemContext<InventorySystem>& context) noexcept
{
    auto& inventory = context.world.own_inventory();
    const bool inventory_resized = ensure_inventory_slots(inventory);
    if (inventory_resized)
    {
        context.world.mark_projection_dirty(SITE_PROJECTION_UPDATE_INVENTORY);
    }
    if (inventory.pending_delivery_queue.empty())
    {
        return;
    }

    bool inventory_changed = false;
    std::vector<std::size_t> touched_camp_slots {};

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

            const auto remaining_quantity = add_item_to_container(
                inventory,
                GS1_INVENTORY_CONTAINER_CAMP_STORAGE,
                stack.item_id,
                stack.item_quantity,
                stack.item_condition,
                stack.item_freshness,
                &touched_camp_slots);
            if (remaining_quantity != stack.item_quantity)
            {
                inventory_changed = true;
            }

            if (remaining_quantity == 0U)
            {
                clear_slot(stack);
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
        mark_touched_inventory_slots(context, GS1_INVENTORY_CONTAINER_CAMP_STORAGE, touched_camp_slots);
    }
}

}  // namespace

bool InventorySystem::subscribes_to(GameCommandType type) noexcept
{
    switch (type)
    {
    case GameCommandType::SiteRunStarted:
    case GameCommandType::InventoryDeliveryRequested:
    case GameCommandType::InventoryItemUseRequested:
    case GameCommandType::InventoryItemConsumeRequested:
    case GameCommandType::InventoryTransferRequested:
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
        seed_inventory_for_site_one(context);
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

    case GameCommandType::InventoryTransferRequested:
        return handle_inventory_transfer(
            context,
            command.payload_as<InventoryTransferRequestedCommand>());

    default:
        return GS1_STATUS_OK;
    }
}

void InventorySystem::run(SiteSystemContext<InventorySystem>& context)
{
    if (ensure_inventory_slots(context.world.own_inventory()))
    {
        context.world.mark_projection_dirty(SITE_PROJECTION_UPDATE_INVENTORY);
    }
    progress_pending_deliveries(context);
}
}  // namespace gs1
