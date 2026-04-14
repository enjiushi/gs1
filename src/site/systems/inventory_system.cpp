#include "site/systems/inventory_system.h"

#include "site/inventory_state.h"
#include "site/site_projection_update_flags.h"
#include "site/site_run_state.h"

#include <algorithm>
#include <cstddef>

namespace gs1
{
namespace
{
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

void ensure_inventory_slots(InventoryState& inventory) noexcept
{
    if (inventory.worker_pack_slots.size() != inventory.worker_pack_slot_count)
    {
        inventory.worker_pack_slots.resize(inventory.worker_pack_slot_count);
    }
    if (inventory.camp_storage_slots.size() != inventory.camp_storage_slot_count)
    {
        inventory.camp_storage_slots.resize(inventory.camp_storage_slot_count);
    }
}

void clear_slot(InventorySlot& slot) noexcept
{
    slot = InventorySlot{};
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

void assign_slot(InventorySlot& slot, ItemId item_id, std::uint32_t quantity) noexcept
{
    slot.item_id = item_id;
    slot.item_quantity = quantity;
    slot.item_condition = 1.0f;
    slot.item_freshness = 1.0f;
    slot.occupied = quantity > 0U;
}

void emit_item_use_effect_if_supported(
    SiteSystemContext<InventorySystem>& context,
    const InventorySlot& slot,
    std::uint32_t quantity)
{
    WorkerMeterDeltaRequestedCommand meter_delta {};
    meter_delta.source_id = slot.item_id.value;

    switch (slot.item_id.value)
    {
    case 1U:
        meter_delta.flags = WORKER_METER_CHANGED_HYDRATION;
        meter_delta.hydration_delta = 12.0f * static_cast<float>(quantity);
        break;
    case 2U:
        meter_delta.flags = WORKER_METER_CHANGED_NOURISHMENT | WORKER_METER_CHANGED_ENERGY;
        meter_delta.nourishment_delta = 10.0f * static_cast<float>(quantity);
        meter_delta.energy_delta = 8.0f * static_cast<float>(quantity);
        break;
    default:
        return;
    }

    GameCommand command {};
    command.type = GameCommandType::WorkerMeterDeltaRequested;
    command.set_payload(meter_delta);
    context.command_queue.push_back(command);
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

    if (inventory.worker_pack_slots.size() >= 3)
    {
        assign_slot(inventory.worker_pack_slots[0], ItemId{1}, 2U);
        assign_slot(inventory.worker_pack_slots[1], ItemId{2}, 1U);
        assign_slot(inventory.worker_pack_slots[2], ItemId{3}, 1U);
    }

    if (!inventory.camp_storage_slots.empty())
    {
        assign_slot(inventory.camp_storage_slots[0], ItemId{4}, 5U);
    }

    context.world.mark_projection_dirty(SITE_PROJECTION_UPDATE_INVENTORY);
}

Gs1Status handle_inventory_item_use(
    SiteSystemContext<InventorySystem>& context,
    const InventoryItemUseRequestedCommand& payload) noexcept
{
    auto& inventory = context.world.own_inventory();
    ensure_inventory_slots(inventory);
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

    const auto requested_quantity = normalize_quantity(payload.quantity);
    if (requested_quantity > slot->item_quantity)
    {
        return GS1_STATUS_INVALID_ARGUMENT;
    }

    emit_item_use_effect_if_supported(context, *slot, requested_quantity);
    slot->item_quantity -= requested_quantity;
    if (slot->item_quantity == 0U)
    {
        clear_slot(*slot);
    }

    context.world.mark_projection_dirty(SITE_PROJECTION_UPDATE_INVENTORY);
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
    ensure_inventory_slots(inventory);
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

    if (!destination_slot->occupied)
    {
        assign_slot(*destination_slot, source_slot->item_id, transfer_quantity);
        destination_slot->item_condition = source_slot->item_condition;
        destination_slot->item_freshness = source_slot->item_freshness;
    }
    else if (destination_slot->item_id != source_slot->item_id)
    {
        return GS1_STATUS_INVALID_ARGUMENT;
    }
    else
    {
        destination_slot->item_quantity += transfer_quantity;
    }

    source_slot->item_quantity -= transfer_quantity;
    if (source_slot->item_quantity == 0U)
    {
        clear_slot(*source_slot);
    }

    context.world.mark_projection_dirty(SITE_PROJECTION_UPDATE_INVENTORY);
    return GS1_STATUS_OK;
}

}  // namespace

bool InventorySystem::subscribes_to(GameCommandType type) noexcept
{
    switch (type)
    {
    case GameCommandType::SiteRunStarted:
    case GameCommandType::InventoryItemUseRequested:
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
    case GameCommandType::InventoryItemUseRequested:
        return handle_inventory_item_use(
            context,
            command.payload_as<InventoryItemUseRequestedCommand>());
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
    ensure_inventory_slots(context.world.own_inventory());
}
}  // namespace gs1
