#include "site/inventory_storage.h"

#include "content/defs/item_defs.h"
#include "site/site_world.h"
#include "site/systems/site_system_context.h"

#include <algorithm>

namespace gs1::inventory_storage
{
namespace
{
InventoryStateRef inventory_ref(SiteRunState& site_run) noexcept
{
    return InventoryStateRef {
        site_run.inventory.next_storage_id,
        site_run.inventory.worker_pack_slot_count,
        site_run.inventory.worker_pack_storage_id,
        site_run.inventory.worker_pack_container_entity_id,
        site_run.inventory.item_membership_revision,
        site_run.inventory.item_quantity_revision,
        site_run.inventory.storage_containers,
        site_run.inventory.storage_slot_item_instance_ids,
        site_run.inventory.worker_pack_slots,
        site_run.inventory.pending_deliveries,
        site_run.inventory.pending_delivery_item_stacks};
}

ConstInventoryStateRef inventory_ref(const SiteRunState& site_run) noexcept
{
    return ConstInventoryStateRef {
        site_run.inventory.next_storage_id,
        site_run.inventory.worker_pack_slot_count,
        site_run.inventory.worker_pack_storage_id,
        site_run.inventory.worker_pack_container_entity_id,
        site_run.inventory.item_membership_revision,
        site_run.inventory.item_quantity_revision,
        site_run.inventory.storage_containers,
        site_run.inventory.storage_slot_item_instance_ids,
        site_run.inventory.worker_pack_slots,
        site_run.inventory.pending_deliveries,
        site_run.inventory.pending_delivery_item_stacks};
}

std::uint64_t* slot_items_ptr(
    InventoryStateRef inventory,
    const StorageContainerEntryState& storage) noexcept
{
    if (storage.slot_item_count == 0U ||
        storage.slot_item_offset > inventory.storage_slot_item_instance_ids.size() ||
        inventory.storage_slot_item_instance_ids.size() - storage.slot_item_offset < storage.slot_item_count)
    {
        return nullptr;
    }

    return inventory.storage_slot_item_instance_ids.data() + storage.slot_item_offset;
}

const std::uint64_t* slot_items_ptr(
    ConstInventoryStateRef inventory,
    const StorageContainerEntryState& storage) noexcept
{
    if (storage.slot_item_count == 0U ||
        storage.slot_item_offset > inventory.storage_slot_item_instance_ids.size() ||
        inventory.storage_slot_item_instance_ids.size() - storage.slot_item_offset < storage.slot_item_count)
    {
        return nullptr;
    }

    return inventory.storage_slot_item_instance_ids.data() + storage.slot_item_offset;
}
}  // namespace

flecs::world* ecs_world_ptr(SiteRunState& site_run) noexcept
{
    return site_run.site_world == nullptr ? nullptr : &site_run.site_world->ecs_world();
}

const flecs::world* ecs_world_ptr(const SiteRunState& site_run) noexcept
{
    return site_run.site_world == nullptr ? nullptr : &site_run.site_world->ecs_world();
}

flecs::entity entity_from_id(
    InventoryStateRef inventory,
    flecs::world* world,
    std::uint64_t entity_id) noexcept
{
    (void)inventory;
    return world == nullptr || entity_id == 0U ? flecs::entity {} : world->entity(entity_id);
}

flecs::entity entity_from_id(
    InventoryStateRef inventory,
    const flecs::world* world,
    std::uint64_t entity_id) noexcept
{
    return entity_from_id(inventory, const_cast<flecs::world*>(world), entity_id);
}

flecs::entity entity_from_id(
    ConstInventoryStateRef inventory,
    const flecs::world* world,
    std::uint64_t entity_id) noexcept
{
    (void)inventory;
    return world == nullptr || entity_id == 0U
        ? flecs::entity {}
        : const_cast<flecs::world*>(world)->entity(entity_id);
}

flecs::entity entity_from_id(SiteRunState& site_run, std::uint64_t entity_id) noexcept
{
    auto* world = ecs_world_ptr(site_run);
    return world == nullptr || entity_id == 0U ? flecs::entity {} : world->entity(entity_id);
}

flecs::entity entity_from_id(const SiteRunState& site_run, std::uint64_t entity_id) noexcept
{
    const auto* world = ecs_world_ptr(site_run);
    return world == nullptr || entity_id == 0U ? flecs::entity {} : const_cast<flecs::world*>(world)->entity(entity_id);
}

StorageContainerEntryState* find_storage_container_state_by_entity_id(
    InventoryStateRef inventory,
    std::uint64_t container_entity_id) noexcept
{
    for (auto& storage : inventory.storage_containers)
    {
        if (storage.container_entity_id == container_entity_id)
        {
            return &storage;
        }
    }

    return nullptr;
}

const StorageContainerEntryState* find_storage_container_state_by_entity_id(
    ConstInventoryStateRef inventory,
    std::uint64_t container_entity_id) noexcept
{
    for (const auto& storage : inventory.storage_containers)
    {
        if (storage.container_entity_id == container_entity_id)
        {
            return &storage;
        }
    }

    return nullptr;
}

StorageContainerEntryState* find_storage_container_entry_by_entity_id(
    InventoryStateRef inventory,
    std::uint64_t container_entity_id) noexcept
{
    return find_storage_container_state_by_entity_id(inventory, container_entity_id);
}

const StorageContainerEntryState* find_storage_container_entry_by_entity_id(
    ConstInventoryStateRef inventory,
    std::uint64_t container_entity_id) noexcept
{
    return find_storage_container_state_by_entity_id(inventory, container_entity_id);
}

StorageContainerEntryState* find_storage_container_state_by_storage_id(
    InventoryStateRef inventory,
    std::uint32_t storage_id) noexcept
{
    for (auto& storage : inventory.storage_containers)
    {
        if (storage.storage_id == storage_id)
        {
            return &storage;
        }
    }

    return nullptr;
}

const StorageContainerEntryState* find_storage_container_state_by_storage_id(
    ConstInventoryStateRef inventory,
    std::uint32_t storage_id) noexcept
{
    for (const auto& storage : inventory.storage_containers)
    {
        if (storage.storage_id == storage_id)
        {
            return &storage;
        }
    }

    return nullptr;
}

StorageContainerEntryState* find_storage_container_entry_by_storage_id(
    InventoryStateRef inventory,
    std::uint32_t storage_id) noexcept
{
    return find_storage_container_state_by_storage_id(inventory, storage_id);
}

const StorageContainerEntryState* find_storage_container_entry_by_storage_id(
    ConstInventoryStateRef inventory,
    std::uint32_t storage_id) noexcept
{
    return find_storage_container_state_by_storage_id(inventory, storage_id);
}

void bump_item_membership_revision(InventoryStateRef inventory) noexcept
{
    inventory.item_membership_revision += 1U;
    inventory.item_quantity_revision += 1U;
}

void bump_item_quantity_revision(InventoryStateRef inventory) noexcept
{
    inventory.item_quantity_revision += 1U;
}

flecs::entity tile_entity_for_coord(SiteRunState& site_run, TileCoord coord) noexcept
{
    if (site_run.site_world == nullptr || !site_run.site_world->contains(coord))
    {
        return {};
    }

    return site_run.site_world->ecs_world().entity(site_run.site_world->tile_entity_id(coord));
}

flecs::entity find_container_entity(
    InventoryStateRef inventory,
    flecs::world* world,
    StorageContainerKind kind,
    std::uint64_t owner_device_entity_id) noexcept
{
    for (const auto& storage : inventory.storage_containers)
    {
        const bool kind_matches =
            (kind == StorageContainerKind::WorkerPack &&
                storage.container_kind == GS1_INVENTORY_CONTAINER_WORKER_PACK) ||
            (kind == StorageContainerKind::DeviceStorage &&
                storage.container_kind == GS1_INVENTORY_CONTAINER_DEVICE_STORAGE);
        if (!kind_matches)
        {
            continue;
        }

        if (owner_device_entity_id != 0U && storage.owner_device_entity_id != owner_device_entity_id)
        {
            continue;
        }

        return entity_from_id(inventory, world, storage.container_entity_id);
    }

    return {};
}

flecs::entity find_container_entity(
    ConstInventoryStateRef inventory,
    const flecs::world* world,
    StorageContainerKind kind,
    std::uint64_t owner_device_entity_id) noexcept
{
    for (const auto& storage : inventory.storage_containers)
    {
        const bool kind_matches =
            (kind == StorageContainerKind::WorkerPack &&
                storage.container_kind == GS1_INVENTORY_CONTAINER_WORKER_PACK) ||
            (kind == StorageContainerKind::DeviceStorage &&
                storage.container_kind == GS1_INVENTORY_CONTAINER_DEVICE_STORAGE);
        if (!kind_matches)
        {
            continue;
        }

        if (owner_device_entity_id != 0U && storage.owner_device_entity_id != owner_device_entity_id)
        {
            continue;
        }

        return entity_from_id(inventory, world, storage.container_entity_id);
    }

    return {};
}

flecs::entity find_container_entity(
    SiteRunState& site_run,
    StorageContainerKind kind,
    std::uint64_t owner_device_entity_id) noexcept
{
    auto inventory = inventory_ref(site_run);
    for (const auto& storage : inventory.storage_containers)
    {
        const bool kind_matches =
            (kind == StorageContainerKind::WorkerPack &&
                storage.container_kind == GS1_INVENTORY_CONTAINER_WORKER_PACK) ||
            (kind == StorageContainerKind::DeviceStorage &&
                storage.container_kind == GS1_INVENTORY_CONTAINER_DEVICE_STORAGE);
        if (!kind_matches)
        {
            continue;
        }

        if (owner_device_entity_id != 0U && storage.owner_device_entity_id != owner_device_entity_id)
        {
            continue;
        }

        return entity_from_id(inventory, ecs_world_ptr(site_run), storage.container_entity_id);
    }

    return {};
}

flecs::entity ensure_storage_container(
    InventoryStateRef inventory,
    flecs::world& world,
    StorageContainerKind kind,
    std::uint32_t slot_count,
    TileCoord tile_coord,
    std::uint64_t tile_entity_id,
    std::uint64_t owner_device_entity_id,
    std::uint32_t flags)
{
    auto container = find_container_entity(inventory, &world, kind, owner_device_entity_id);
    if (!container.is_valid())
    {
        container = world.entity()
            .add<StorageContainerTag>()
            .set<StorageContainerKindComponent>({kind});

        if (tile_entity_id != 0U)
        {
            container.add<StorageAtTile>(world.entity(tile_entity_id));
        }

        if (owner_device_entity_id != 0U)
        {
            container.add<StorageOwnedByDevice>(world.entity(owner_device_entity_id));
        }
    }

    auto* storage_state =
        find_storage_container_state_by_entity_id(inventory, container.id());
    if (storage_state == nullptr)
    {
        inventory.storage_containers.push_back(StorageContainerEntryState {});
        storage_state = &inventory.storage_containers.back();
        storage_state->storage_id = inventory.next_storage_id++;
        storage_state->container_entity_id = container.id();
        storage_state->slot_item_offset =
            static_cast<std::uint32_t>(inventory.storage_slot_item_instance_ids.size());
        storage_state->slot_item_count = slot_count;
        inventory.storage_slot_item_instance_ids.resize(
            inventory.storage_slot_item_instance_ids.size() + slot_count,
            0U);
    }

    storage_state->owner_device_entity_id = owner_device_entity_id;
    storage_state->container_kind =
        kind == StorageContainerKind::WorkerPack
            ? GS1_INVENTORY_CONTAINER_WORKER_PACK
            : GS1_INVENTORY_CONTAINER_DEVICE_STORAGE;
    storage_state->tile_coord = tile_coord;
    storage_state->flags = flags;

    return container;
}

flecs::entity ensure_storage_container(
    SiteRunState& site_run,
    StorageContainerKind kind,
    std::uint32_t slot_count,
    TileCoord tile_coord,
    bool attach_to_tile,
    std::uint64_t owner_device_entity_id,
    std::uint32_t flags)
{
    auto* world = ecs_world_ptr(site_run);
    if (world == nullptr)
    {
        return {};
    }

    const auto tile_entity_id =
        attach_to_tile
            ? (tile_entity_for_coord(site_run, tile_coord).is_valid()
                  ? tile_entity_for_coord(site_run, tile_coord).id()
                  : 0U)
            : 0U;
    return ensure_storage_container(
        inventory_ref(site_run),
        *world,
        kind,
        slot_count,
        tile_coord,
        tile_entity_id,
        owner_device_entity_id,
        flags);
}

flecs::entity worker_pack_container(SiteRunState& site_run) noexcept
{
    return entity_from_id(
        inventory_ref(site_run),
        ecs_world_ptr(site_run),
        site_run.inventory.worker_pack_container_entity_id);
}

flecs::entity worker_pack_container(
    ConstInventoryStateRef inventory,
    const flecs::world* world) noexcept
{
    return entity_from_id(inventory, world, inventory.worker_pack_container_entity_id);
}

flecs::entity worker_pack_container(
    InventoryStateRef inventory,
    const flecs::world* world) noexcept
{
    return entity_from_id(inventory, world, inventory.worker_pack_container_entity_id);
}

flecs::entity starter_storage_container(SiteRunState& site_run) noexcept
{
    if (site_run.site_world == nullptr || !site_run.site_world->contains(site_run.camp.starter_storage_tile))
    {
        return {};
    }

    const auto device_entity_id = site_run.site_world->device_entity_id(site_run.camp.starter_storage_tile);
    return device_entity_id == 0U
        ? flecs::entity {}
        : find_device_storage_container(site_run, device_entity_id);
}

flecs::entity delivery_box_container(SiteRunState& site_run) noexcept
{
    if (site_run.site_world == nullptr || !site_run.site_world->contains(site_run.camp.delivery_box_tile))
    {
        return {};
    }

    const auto device_entity_id = site_run.site_world->device_entity_id(site_run.camp.delivery_box_tile);
    return device_entity_id == 0U
        ? flecs::entity {}
        : find_device_storage_container(site_run, device_entity_id);
}

flecs::entity container_for_kind(
    ConstInventoryStateRef inventory,
    const flecs::world* world,
    Gs1InventoryContainerKind kind,
    std::uint64_t owner_entity_id) noexcept
{
    switch (kind)
    {
    case GS1_INVENTORY_CONTAINER_WORKER_PACK:
        return worker_pack_container(inventory, world);
    case GS1_INVENTORY_CONTAINER_DEVICE_STORAGE:
        return owner_entity_id == 0U
            ? flecs::entity {}
            : find_device_storage_container(
                  inventory,
                  const_cast<flecs::world*>(world),
                  owner_entity_id);
    default:
        return {};
    }
}

flecs::entity container_for_kind(
    InventoryStateRef inventory,
    const flecs::world* world,
    Gs1InventoryContainerKind kind,
    std::uint64_t owner_entity_id) noexcept
{
    switch (kind)
    {
    case GS1_INVENTORY_CONTAINER_WORKER_PACK:
        return worker_pack_container(inventory, world);
    case GS1_INVENTORY_CONTAINER_DEVICE_STORAGE:
        return owner_entity_id == 0U
            ? flecs::entity {}
            : find_device_storage_container(
                  inventory,
                  world,
                  owner_entity_id);
    default:
        return {};
    }
}

flecs::entity container_for_kind(
    SiteRunState& site_run,
    Gs1InventoryContainerKind kind,
    std::uint64_t owner_entity_id) noexcept
{
    switch (kind)
    {
    case GS1_INVENTORY_CONTAINER_WORKER_PACK:
        return worker_pack_container(site_run);
    case GS1_INVENTORY_CONTAINER_DEVICE_STORAGE:
        return owner_entity_id == 0U
            ? flecs::entity {}
            : find_device_storage_container(site_run, owner_entity_id);
    default:
        return {};
    }
}

flecs::entity container_for_storage_id(
    ConstInventoryStateRef inventory,
    flecs::world* world,
    std::uint32_t storage_id) noexcept
{
    const auto* storage_state =
        find_storage_container_state_by_storage_id(inventory, storage_id);
    return storage_state == nullptr
        ? flecs::entity {}
        : entity_from_id(inventory, world, storage_state->container_entity_id);
}

flecs::entity container_for_storage_id(
    InventoryStateRef inventory,
    const flecs::world* world,
    std::uint32_t storage_id) noexcept
{
    const auto* storage_state =
        find_storage_container_state_by_storage_id(inventory, storage_id);
    return storage_state == nullptr
        ? flecs::entity {}
        : entity_from_id(inventory, world, storage_state->container_entity_id);
}

flecs::entity container_for_storage_id(
    ConstInventoryStateRef inventory,
    const flecs::world* world,
    std::uint32_t storage_id) noexcept
{
    const auto* storage_state =
        find_storage_container_state_by_storage_id(inventory, storage_id);
    return storage_state == nullptr
        ? flecs::entity {}
        : entity_from_id(inventory, world, storage_state->container_entity_id);
}

flecs::entity container_for_storage_id(
    SiteRunState& site_run,
    std::uint32_t storage_id) noexcept
{
    auto inventory = inventory_ref(site_run);
    const auto* storage_state =
        find_storage_container_state_by_storage_id(inventory, storage_id);
    return storage_state == nullptr
        ? flecs::entity {}
        : entity_from_id(inventory, ecs_world_ptr(site_run), storage_state->container_entity_id);
}

std::uint32_t storage_id_for_container(
    ConstInventoryStateRef inventory,
    flecs::entity container) noexcept
{
    if (!container.is_valid())
    {
        return 0U;
    }

    const auto* storage_state =
        find_storage_container_state_by_entity_id(inventory, container.id());
    return storage_state == nullptr ? 0U : storage_state->storage_id;
}

std::uint32_t storage_id_for_container(
    const SiteRunState& site_run,
    flecs::entity container) noexcept
{
    if (!container.is_valid())
    {
        return 0U;
    }

    const auto* storage_state =
        find_storage_container_state_by_entity_id(inventory_ref(site_run), container.id());
    return storage_state == nullptr ? 0U : storage_state->storage_id;
}

std::uint32_t storage_id_for_container(
    InventoryStateRef inventory,
    flecs::entity container) noexcept
{
    if (!container.is_valid())
    {
        return 0U;
    }

    const auto* storage_state =
        find_storage_container_state_by_entity_id(inventory, container.id());
    return storage_state == nullptr ? 0U : storage_state->storage_id;
}

std::uint32_t worker_pack_storage_id(const SiteRunState& site_run) noexcept
{
    return site_run.inventory.worker_pack_storage_id;
}

flecs::entity item_entity_for_slot(
    InventoryStateRef inventory,
    flecs::world* world,
    flecs::entity container,
    std::uint32_t slot_index) noexcept
{
    const auto* storage_state =
        find_storage_container_state_by_entity_id(inventory, container.id());
    const auto* slot_ids = storage_state == nullptr ? nullptr : slot_items_ptr(inventory, *storage_state);
    if (storage_state == nullptr || slot_ids == nullptr || slot_index >= storage_state->slot_item_count)
    {
        return {};
    }

    return entity_from_id(inventory, world, slot_ids[slot_index]);
}

flecs::entity item_entity_for_slot(
    ConstInventoryStateRef inventory,
    const flecs::world* world,
    flecs::entity container,
    std::uint32_t slot_index) noexcept
{
    const auto* storage_state =
        find_storage_container_state_by_entity_id(inventory, container.id());
    const auto* slot_ids = storage_state == nullptr ? nullptr : slot_items_ptr(inventory, *storage_state);
    if (storage_state == nullptr || slot_ids == nullptr || slot_index >= storage_state->slot_item_count)
    {
        return {};
    }

    return entity_from_id(inventory, world, slot_ids[slot_index]);
}

flecs::entity item_entity_for_slot(
    SiteRunState& site_run,
    flecs::entity container,
    std::uint32_t slot_index) noexcept
{
    auto inventory = inventory_ref(site_run);
    const auto* storage_state =
        find_storage_container_state_by_entity_id(inventory, container.id());
    const auto* slot_ids =
        storage_state == nullptr ? nullptr : slot_items_ptr(inventory, *storage_state);
    if (storage_state == nullptr || slot_ids == nullptr || slot_index >= storage_state->slot_item_count)
    {
        return {};
    }

    return entity_from_id(inventory, ecs_world_ptr(site_run), slot_ids[slot_index]);
}

flecs::entity item_entity_for_slot(
    InventoryStateRef inventory,
    const flecs::world* world,
    flecs::entity container,
    std::uint32_t slot_index) noexcept
{
    const auto* storage_state =
        find_storage_container_state_by_entity_id(inventory, container.id());
    const auto* slot_ids =
        storage_state == nullptr ? nullptr : slot_items_ptr(inventory, *storage_state);
    if (storage_state == nullptr || slot_ids == nullptr || slot_index >= storage_state->slot_item_count)
    {
        return {};
    }

    return entity_from_id(inventory, world, slot_ids[slot_index]);
}

flecs::entity item_entity_for_slot(
    const SiteRunState& site_run,
    flecs::entity container,
    std::uint32_t slot_index) noexcept
{
    return item_entity_for_slot(const_cast<SiteRunState&>(site_run), container, slot_index);
}

const StorageItemStack* stack_data(const SiteRunState& site_run, flecs::entity item) noexcept
{
    (void)site_run;
    return item.is_valid() && item.has<StorageItemStack>()
        ? &item.get<StorageItemStack>()
        : nullptr;
}

const StorageItemStack* stack_data(
    ConstInventoryStateRef inventory,
    flecs::entity item) noexcept
{
    (void)inventory;
    return item.is_valid() && item.has<StorageItemStack>()
        ? &item.get<StorageItemStack>()
        : nullptr;
}

const StorageItemStack* stack_data(
    InventoryStateRef inventory,
    flecs::entity item) noexcept
{
    (void)inventory;
    return item.is_valid() && item.has<StorageItemStack>()
        ? &item.get<StorageItemStack>()
        : nullptr;
}

std::uint32_t stack_limit(ItemId item_id) noexcept
{
    return item_stack_size(item_id);
}

void fill_projection_slot_from_entities(
    InventorySlot& slot_view,
    flecs::entity item_entity)
{
    if (!item_entity.is_valid())
    {
        slot_view = InventorySlot {};
        return;
    }

    const auto* stack =
        item_entity.has<StorageItemStack>() ? &item_entity.get<StorageItemStack>() : nullptr;
    if (stack == nullptr)
    {
        slot_view = InventorySlot {};
        return;
    }

    slot_view.item_instance_id = static_cast<std::uint32_t>(item_entity.id());
    slot_view.item_id = stack->item_id;
    slot_view.item_quantity = stack->quantity;
    slot_view.item_condition = stack->condition;
    slot_view.item_freshness = stack->freshness;
    slot_view.occupied = stack->quantity > 0U;
}

void sync_projection_slots_for_container(
    ConstInventoryStateRef inventory,
    flecs::world* world,
    flecs::entity container,
    std::vector<InventorySlot>& destination)
{
    const auto* storage_state =
        find_storage_container_state_by_entity_id(inventory, container.id());
    if (storage_state == nullptr)
    {
        std::fill(destination.begin(), destination.end(), InventorySlot {});
        return;
    }

    if (destination.size() != storage_state->slot_item_count)
    {
        destination.resize(storage_state->slot_item_count);
    }

    for (std::size_t slot_index = 0; slot_index < storage_state->slot_item_count; ++slot_index)
    {
        fill_projection_slot_from_entities(
            destination[slot_index],
            item_entity_for_slot(
                inventory,
                world,
                container,
                static_cast<std::uint32_t>(slot_index)));
    }
}

void sync_projection_slots_for_container(
    InventoryStateRef inventory,
    const flecs::world* world,
    flecs::entity container,
    std::vector<InventorySlot>& destination)
{
    sync_projection_slots_for_container(
        static_cast<ConstInventoryStateRef>(inventory),
        const_cast<flecs::world*>(world),
        container,
        destination);
}

void sync_projection_slots_for_container(
    ConstInventoryStateRef inventory,
    const flecs::world* world,
    flecs::entity container,
    std::vector<InventorySlot>& destination)
{
    sync_projection_slots_for_container(
        inventory,
        const_cast<flecs::world*>(world),
        container,
        destination);
}

void sync_projection_slots_for_container(
    SiteRunState& site_run,
    flecs::entity container,
    std::vector<InventorySlot>& destination)
{
    auto inventory = inventory_ref(site_run);
    const auto* storage_state =
        find_storage_container_state_by_entity_id(inventory, container.id());
    if (storage_state == nullptr)
    {
        std::fill(destination.begin(), destination.end(), InventorySlot {});
        return;
    }

    if (destination.size() != storage_state->slot_item_count)
    {
        destination.resize(storage_state->slot_item_count);
    }

    for (std::size_t slot_index = 0; slot_index < storage_state->slot_item_count; ++slot_index)
    {
        fill_projection_slot_from_entities(
            destination[slot_index],
            item_entity_for_slot(site_run, container, static_cast<std::uint32_t>(slot_index)));
    }
}

void sync_projection_views(InventoryStateRef inventory, flecs::world* world)
{
    const ConstInventoryStateRef inventory_view = inventory;
    sync_projection_slots_for_container(
        inventory_view,
        static_cast<const flecs::world*>(world),
        worker_pack_container(inventory, world),
        inventory.worker_pack_slots);
}

void sync_projection_views(InventoryStateRef inventory, const flecs::world* world)
{
    sync_projection_views(inventory, const_cast<flecs::world*>(world));
}

void sync_projection_views(SiteRunState& site_run)
{
    sync_projection_slots_for_container(
        site_run,
        worker_pack_container(site_run),
        site_run.inventory.worker_pack_slots);
}

void initialize_site_inventory_storage(InventoryStateRef inventory, flecs::world* world)
{
    if (world == nullptr)
    {
        return;
    }

    if (inventory.worker_pack_slots.size() != inventory.worker_pack_slot_count)
    {
        inventory.worker_pack_slots.resize(inventory.worker_pack_slot_count);
    }

    const auto worker_container = ensure_storage_container(
        inventory,
        *world,
        StorageContainerKind::WorkerPack,
        inventory.worker_pack_slot_count);

    inventory.worker_pack_container_entity_id = worker_container.id();
    inventory.worker_pack_storage_id = storage_id_for_container(inventory, worker_container);
    sync_projection_views(inventory, world);
}

void initialize_site_inventory_storage(InventoryStateRef inventory, const flecs::world* world)
{
    initialize_site_inventory_storage(inventory, const_cast<flecs::world*>(world));
}

void initialize_site_inventory_storage(SiteRunState& site_run)
{
    initialize_site_inventory_storage(inventory_ref(site_run), ecs_world_ptr(site_run));
}

flecs::entity create_item_entity(
    flecs::world& world,
    flecs::entity container,
    std::uint32_t slot_index,
    ItemId item_id,
    std::uint32_t quantity,
    float condition,
    float freshness)
{
    return world.entity()
        .add<StorageItemTag>()
        .set<StorageItemStack>({item_id, quantity, condition, freshness})
        .set<StorageItemLocation>({container.id(), static_cast<std::uint16_t>(slot_index), 0U});
}

void import_projection_slots_into_container_if_needed(
    InventoryStateRef inventory,
    flecs::world* world,
    flecs::entity container,
    const std::vector<InventorySlot>& source_slots)
{
    const auto* storage_state =
        find_storage_container_state_by_entity_id(inventory, container.id());
    if (world == nullptr || storage_state == nullptr)
    {
        return;
    }

    auto& slot_ids = inventory.storage_slot_item_instance_ids;
    const auto slot_offset = static_cast<std::size_t>(storage_state->slot_item_offset);
    const auto slot_count = static_cast<std::size_t>(storage_state->slot_item_count);
    if (slot_offset > slot_ids.size() || slot_ids.size() - slot_offset < slot_count)
    {
        return;
    }

    for (std::size_t slot_index = 0; slot_index < source_slots.size(); ++slot_index)
    {
        if (slot_index >= slot_count || slot_ids[slot_offset + slot_index] != 0U)
        {
            continue;
        }

        const auto& source = source_slots[slot_index];
        if (!source.occupied || source.item_id.value == 0U || source.item_quantity == 0U)
        {
            continue;
        }

        const auto item = create_item_entity(
            *world,
            container,
            static_cast<std::uint32_t>(slot_index),
            source.item_id,
            source.item_quantity,
            source.item_condition,
            source.item_freshness);
        slot_ids[slot_offset + slot_index] = item.id();
        bump_item_membership_revision(inventory);
    }
}

void import_projection_slots_into_container_if_needed(
    SiteRunState& site_run,
    flecs::entity container,
    const std::vector<InventorySlot>& source_slots)
{
    import_projection_slots_into_container_if_needed(
        inventory_ref(site_run),
        ecs_world_ptr(site_run),
        container,
        source_slots);
}

void import_projection_views_into_storage_if_needed(InventoryStateRef inventory, flecs::world* world)
{
    if (world == nullptr)
    {
        return;
    }

    initialize_site_inventory_storage(inventory, world);
    import_projection_slots_into_container_if_needed(
        inventory,
        world,
        worker_pack_container(inventory, world),
        inventory.worker_pack_slots);
    sync_projection_views(inventory, world);
}

void import_projection_views_into_storage_if_needed(
    InventoryStateRef inventory,
    const flecs::world* world)
{
    import_projection_views_into_storage_if_needed(
        inventory,
        const_cast<flecs::world*>(world));
}

void import_projection_views_into_storage_if_needed(SiteRunState& site_run)
{
    import_projection_views_into_storage_if_needed(
        inventory_ref(site_run),
        ecs_world_ptr(site_run));
}

bool storage_initialized(ConstInventoryStateRef inventory) noexcept
{
    return inventory.worker_pack_container_entity_id != 0U;
}

bool storage_initialized(InventoryStateRef inventory) noexcept
{
    return inventory.worker_pack_container_entity_id != 0U;
}

bool storage_initialized(const SiteRunState& site_run) noexcept
{
    return site_run.inventory.worker_pack_container_entity_id != 0U;
}

std::uint32_t add_item_to_container(
    InventoryStateRef inventory,
    flecs::world* world,
    flecs::entity container,
    ItemId item_id,
    std::uint32_t quantity,
    float condition,
    float freshness)
{
    auto* storage_state =
        find_storage_container_state_by_entity_id(inventory, container.id());
    if (world == nullptr || storage_state == nullptr || quantity == 0U)
    {
        return quantity;
    }

    auto& slot_ids = inventory.storage_slot_item_instance_ids;
    const auto slot_offset = static_cast<std::size_t>(storage_state->slot_item_offset);
    const auto slot_count = static_cast<std::size_t>(storage_state->slot_item_count);
    if (slot_offset > slot_ids.size() || slot_ids.size() - slot_offset < slot_count)
    {
        return quantity;
    }

    std::uint32_t remaining = quantity;
    const auto max_stack = stack_limit(item_id);

    for (std::uint32_t slot_index = 0U;
         slot_index < storage_state->slot_item_count && remaining > 0U;
         ++slot_index)
    {
        const auto item = item_entity_for_slot(inventory, world, container, slot_index);
        if (!item.is_valid() || !item.has<StorageItemStack>())
        {
            continue;
        }

        auto& stack = item.get_mut<StorageItemStack>();
        if (stack.item_id != item_id || stack.quantity >= max_stack)
        {
            continue;
        }

        const auto free_capacity = max_stack - stack.quantity;
        const auto transfer = std::min<std::uint32_t>(remaining, free_capacity);
        stack.quantity += transfer;
        item.modified<StorageItemStack>();
        remaining -= transfer;
        bump_item_quantity_revision(inventory);
    }

    for (std::uint32_t slot_index = 0U;
         slot_index < storage_state->slot_item_count && remaining > 0U;
         ++slot_index)
    {
        if (slot_ids[slot_offset + slot_index] != 0U)
        {
            continue;
        }

        const auto transfer = std::min<std::uint32_t>(remaining, max_stack);
        const auto item = create_item_entity(
            *world,
            container,
            slot_index,
            item_id,
            transfer,
            condition,
            freshness);
        slot_ids[slot_offset + slot_index] = item.id();
        remaining -= transfer;
        bump_item_membership_revision(inventory);
    }

    sync_projection_views(inventory, world);
    return remaining;
}

std::uint32_t add_item_to_container(
    InventoryStateRef inventory,
    const flecs::world* world,
    flecs::entity container,
    ItemId item_id,
    std::uint32_t quantity,
    float condition,
    float freshness)
{
    return add_item_to_container(
        inventory,
        const_cast<flecs::world*>(world),
        container,
        item_id,
        quantity,
        condition,
        freshness);
}

std::uint32_t add_item_to_container(
    SiteRunState& site_run,
    flecs::entity container,
    ItemId item_id,
    std::uint32_t quantity,
    float condition,
    float freshness)
{
    return add_item_to_container(
        inventory_ref(site_run),
        ecs_world_ptr(site_run),
        container,
        item_id,
        quantity,
        condition,
        freshness);
}

std::uint32_t available_item_quantity_in_container(
    ConstInventoryStateRef inventory,
    flecs::world* world,
    flecs::entity container,
    ItemId item_id) noexcept
{
    const auto* storage_state =
        find_storage_container_state_by_entity_id(inventory, container.id());
    if (storage_state == nullptr)
    {
        return 0U;
    }

    std::uint32_t total = 0U;
    for (std::uint32_t slot_index = 0U; slot_index < storage_state->slot_item_count; ++slot_index)
    {
        const auto item = item_entity_for_slot(inventory, world, container, slot_index);
        const auto* stack = stack_data(inventory, item);
        if (stack != nullptr && stack->item_id == item_id)
        {
            total += stack->quantity;
        }
    }
    return total;
}

std::uint32_t available_item_quantity_in_container(
    InventoryStateRef inventory,
    const flecs::world* world,
    flecs::entity container,
    ItemId item_id) noexcept
{
    return available_item_quantity_in_container(
        static_cast<ConstInventoryStateRef>(inventory),
        const_cast<flecs::world*>(world),
        container,
        item_id);
}

std::uint32_t available_item_quantity_in_container(
    ConstInventoryStateRef inventory,
    const flecs::world* world,
    flecs::entity container,
    ItemId item_id) noexcept
{
    return available_item_quantity_in_container(
        inventory,
        const_cast<flecs::world*>(world),
        container,
        item_id);
}

std::uint32_t available_item_quantity_in_container(
    SiteRunState& site_run,
    flecs::entity container,
    ItemId item_id) noexcept
{
    auto inventory = inventory_ref(site_run);
    const auto* storage_state =
        find_storage_container_state_by_entity_id(inventory, container.id());
    if (storage_state == nullptr)
    {
        return 0U;
    }

    std::uint32_t total = 0U;
    for (std::uint32_t slot_index = 0U; slot_index < storage_state->slot_item_count; ++slot_index)
    {
        const auto item = item_entity_for_slot(site_run, container, slot_index);
        const auto* stack = stack_data(site_run, item);
        if (stack != nullptr && stack->item_id == item_id)
        {
            total += stack->quantity;
        }
    }
    return total;
}

std::uint32_t available_item_quantity_in_container_kind(
    ConstInventoryStateRef inventory,
    const flecs::world* world,
    Gs1InventoryContainerKind kind,
    ItemId item_id,
    std::uint64_t owner_entity_id) noexcept
{
    return available_item_quantity_in_container(
        inventory,
        const_cast<flecs::world*>(world),
        container_for_kind(inventory, world, kind, owner_entity_id),
        item_id);
}

std::uint32_t available_item_quantity_in_container_kind(
    InventoryStateRef inventory,
    const flecs::world* world,
    Gs1InventoryContainerKind kind,
    ItemId item_id,
    std::uint64_t owner_entity_id) noexcept
{
    return available_item_quantity_in_container(
        inventory,
        world,
        container_for_kind(inventory, world, kind, owner_entity_id),
        item_id);
}

std::uint32_t available_item_quantity_in_container_kind(
    SiteRunState& site_run,
    Gs1InventoryContainerKind kind,
    ItemId item_id,
    std::uint64_t owner_entity_id) noexcept
{
    return available_item_quantity_in_container(
        site_run,
        container_for_kind(site_run, kind, owner_entity_id),
        item_id);
}

std::uint32_t available_item_quantity_in_containers(
    ConstInventoryStateRef inventory,
    flecs::world* world,
    const std::vector<flecs::entity>& containers,
    ItemId item_id) noexcept
{
    std::uint32_t total = 0U;
    for (const auto& container : containers)
    {
        total += available_item_quantity_in_container(inventory, world, container, item_id);
    }
    return total;
}

std::uint32_t available_item_quantity_in_containers(
    InventoryStateRef inventory,
    const flecs::world* world,
    const std::vector<flecs::entity>& containers,
    ItemId item_id) noexcept
{
    return available_item_quantity_in_containers(
        static_cast<ConstInventoryStateRef>(inventory),
        const_cast<flecs::world*>(world),
        containers,
        item_id);
}

std::uint32_t available_item_quantity_in_containers(
    ConstInventoryStateRef inventory,
    const flecs::world* world,
    const std::vector<flecs::entity>& containers,
    ItemId item_id) noexcept
{
    return available_item_quantity_in_containers(
        inventory,
        const_cast<flecs::world*>(world),
        containers,
        item_id);
}

std::uint32_t available_item_quantity_in_containers(
    SiteRunState& site_run,
    const std::vector<flecs::entity>& containers,
    ItemId item_id) noexcept
{
    std::uint32_t total = 0U;
    for (const auto& container : containers)
    {
        total += available_item_quantity_in_container(site_run, container, item_id);
    }
    return total;
}

std::uint32_t consume_item_type_from_container(
    InventoryStateRef inventory,
    flecs::world* world,
    flecs::entity container,
    ItemId item_id,
    std::uint32_t quantity)
{
    auto* storage_state =
        find_storage_container_state_by_entity_id(inventory, container.id());
    if (storage_state == nullptr || quantity == 0U)
    {
        return quantity;
    }

    auto& slot_ids = inventory.storage_slot_item_instance_ids;
    const auto slot_offset = static_cast<std::size_t>(storage_state->slot_item_offset);
    const auto slot_count = static_cast<std::size_t>(storage_state->slot_item_count);
    if (slot_offset > slot_ids.size() || slot_ids.size() - slot_offset < slot_count)
    {
        return quantity;
    }

    std::uint32_t remaining = quantity;
    for (std::uint32_t slot_index = 0U;
         slot_index < storage_state->slot_item_count && remaining > 0U;
         ++slot_index)
    {
        auto item = item_entity_for_slot(inventory, world, container, slot_index);
        if (!item.is_valid() || !item.has<StorageItemStack>())
        {
            continue;
        }

        auto& stack = item.get_mut<StorageItemStack>();
        if (stack.item_id != item_id)
        {
            continue;
        }

        const auto removed = std::min<std::uint32_t>(remaining, stack.quantity);
        stack.quantity -= removed;
        remaining -= removed;
        if (stack.quantity == 0U)
        {
            slot_ids[slot_offset + slot_index] = 0U;
            item.destruct();
            bump_item_membership_revision(inventory);
        }
        else
        {
            item.modified<StorageItemStack>();
            bump_item_quantity_revision(inventory);
        }
    }

    sync_projection_views(inventory, world);
    return remaining;
}

std::uint32_t consume_item_type_from_container(
    InventoryStateRef inventory,
    const flecs::world* world,
    flecs::entity container,
    ItemId item_id,
    std::uint32_t quantity)
{
    return consume_item_type_from_container(
        inventory,
        const_cast<flecs::world*>(world),
        container,
        item_id,
        quantity);
}

std::uint32_t consume_item_type_from_container(
    SiteRunState& site_run,
    flecs::entity container,
    ItemId item_id,
    std::uint32_t quantity)
{
    return consume_item_type_from_container(
        inventory_ref(site_run),
        ecs_world_ptr(site_run),
        container,
        item_id,
        quantity);
}

bool consume_quantity_from_item_entity(
    InventoryStateRef inventory,
    flecs::world* world,
    flecs::entity item_entity,
    std::uint32_t quantity)
{
    if (!item_entity.is_valid() || !item_entity.has<StorageItemStack>() || !item_entity.has<StorageItemLocation>())
    {
        return false;
    }

    auto& stack = item_entity.get_mut<StorageItemStack>();
    if (quantity == 0U || quantity > stack.quantity)
    {
        return false;
    }

    const auto location = item_entity.get<StorageItemLocation>();
    auto* storage_state =
        find_storage_container_state_by_entity_id(inventory, location.container_entity_id);
    if (storage_state == nullptr || location.slot_index >= storage_state->slot_item_count)
    {
        return false;
    }

    auto& slot_ids = inventory.storage_slot_item_instance_ids;
    const auto slot_offset = static_cast<std::size_t>(storage_state->slot_item_offset);
    if (slot_offset > slot_ids.size() || slot_ids.size() - slot_offset < storage_state->slot_item_count)
    {
        return false;
    }

    stack.quantity -= quantity;
    if (stack.quantity == 0U)
    {
        slot_ids[slot_offset + location.slot_index] = 0U;
        item_entity.destruct();
        bump_item_membership_revision(inventory);
    }
    else
    {
        item_entity.modified<StorageItemStack>();
        bump_item_quantity_revision(inventory);
    }

    sync_projection_views(inventory, world);
    return true;
}

bool consume_quantity_from_item_entity(
    InventoryStateRef inventory,
    const flecs::world* world,
    flecs::entity item_entity,
    std::uint32_t quantity)
{
    return consume_quantity_from_item_entity(
        inventory,
        const_cast<flecs::world*>(world),
        item_entity,
        quantity);
}

bool consume_quantity_from_item_entity(
    SiteRunState& site_run,
    flecs::entity item_entity,
    std::uint32_t quantity)
{
    return consume_quantity_from_item_entity(
        inventory_ref(site_run),
        ecs_world_ptr(site_run),
        item_entity,
        quantity);
}

std::uint32_t consume_item_type_from_containers(
    InventoryStateRef inventory,
    flecs::world* world,
    const std::vector<flecs::entity>& containers,
    ItemId item_id,
    std::uint32_t quantity)
{
    std::uint32_t remaining = quantity;
    for (const auto& container : containers)
    {
        if (remaining == 0U)
        {
            break;
        }

        remaining = consume_item_type_from_container(inventory, world, container, item_id, remaining);
    }
    return remaining;
}

std::uint32_t consume_item_type_from_containers(
    InventoryStateRef inventory,
    const flecs::world* world,
    const std::vector<flecs::entity>& containers,
    ItemId item_id,
    std::uint32_t quantity)
{
    return consume_item_type_from_containers(
        inventory,
        const_cast<flecs::world*>(world),
        containers,
        item_id,
        quantity);
}

std::uint32_t consume_item_type_from_containers(
    SiteRunState& site_run,
    const std::vector<flecs::entity>& containers,
    ItemId item_id,
    std::uint32_t quantity)
{
    std::uint32_t remaining = quantity;
    for (const auto& container : containers)
    {
        if (remaining == 0U)
        {
            break;
        }

        remaining = consume_item_type_from_container(site_run, container, item_id, remaining);
    }
    return remaining;
}

bool can_fit_item_in_container(
    InventoryStateRef inventory,
    flecs::world* world,
    flecs::entity container,
    ItemId item_id,
    std::uint32_t quantity) noexcept
{
    const auto* storage_state =
        find_storage_container_state_by_entity_id(inventory, container.id());
    if (storage_state == nullptr)
    {
        return false;
    }

    std::uint32_t remaining = quantity;
    const auto max_stack = stack_limit(item_id);
    for (std::uint32_t slot_index = 0U;
         slot_index < storage_state->slot_item_count && remaining > 0U;
         ++slot_index)
    {
        const auto item = item_entity_for_slot(inventory, world, container, slot_index);
        const auto* stack = stack_data(inventory, item);
        if (stack == nullptr)
        {
            remaining = remaining > max_stack ? remaining - max_stack : 0U;
            continue;
        }

        if (stack->item_id != item_id || stack->quantity >= max_stack)
        {
            continue;
        }

        const auto free_capacity = max_stack - stack->quantity;
        remaining = remaining > free_capacity ? remaining - free_capacity : 0U;
    }

    return remaining == 0U;
}

bool can_fit_item_in_container(
    InventoryStateRef inventory,
    const flecs::world* world,
    flecs::entity container,
    ItemId item_id,
    std::uint32_t quantity) noexcept
{
    return can_fit_item_in_container(
        inventory,
        const_cast<flecs::world*>(world),
        container,
        item_id,
        quantity);
}

bool can_fit_item_in_container(
    SiteRunState& site_run,
    flecs::entity container,
    ItemId item_id,
    std::uint32_t quantity) noexcept
{
    return can_fit_item_in_container(
        inventory_ref(site_run),
        ecs_world_ptr(site_run),
        container,
        item_id,
        quantity);
}

std::uint32_t slot_count_in_container(
    const SiteRunState& site_run,
    flecs::entity container) noexcept
{
    auto inventory = inventory_ref(site_run);
    const auto* storage_state =
        find_storage_container_state_by_entity_id(inventory, container.id());
    return storage_state == nullptr ? 0U : storage_state->slot_item_count;
}

bool transfer_between_slots(
    InventoryStateRef inventory,
    flecs::world* world,
    Gs1InventoryContainerKind source_kind,
    std::uint32_t source_slot_index,
    Gs1InventoryContainerKind destination_kind,
    std::uint32_t destination_slot_index,
    std::uint32_t quantity,
    std::uint64_t source_owner_entity_id,
    std::uint64_t destination_owner_entity_id)
{
    if (world == nullptr)
    {
        return false;
    }

    auto source_container = container_for_kind(inventory, world, source_kind, source_owner_entity_id);
    auto destination_container =
        container_for_kind(inventory, world, destination_kind, destination_owner_entity_id);
    auto* source_state =
        find_storage_container_state_by_entity_id(inventory, source_container.id());
    auto* destination_state =
        find_storage_container_state_by_entity_id(inventory, destination_container.id());
    if (source_state == nullptr ||
        destination_state == nullptr ||
        source_slot_index >= source_state->slot_item_count ||
        destination_slot_index >= destination_state->slot_item_count)
    {
        return false;
    }

    auto& slot_ids = inventory.storage_slot_item_instance_ids;
    const auto source_slot_offset = static_cast<std::size_t>(source_state->slot_item_offset);
    const auto destination_slot_offset = static_cast<std::size_t>(destination_state->slot_item_offset);
    if (source_slot_offset > slot_ids.size() ||
        destination_slot_offset > slot_ids.size() ||
        slot_ids.size() - source_slot_offset < source_state->slot_item_count ||
        slot_ids.size() - destination_slot_offset < destination_state->slot_item_count)
    {
        return false;
    }

    auto source_item = item_entity_for_slot(inventory, world, source_container, source_slot_index);
    auto destination_item = item_entity_for_slot(inventory, world, destination_container, destination_slot_index);
    if (!source_item.is_valid() || !source_item.has<StorageItemStack>())
    {
        return false;
    }

    auto& source_stack = source_item.get_mut<StorageItemStack>();
    const auto max_stack = stack_limit(source_stack.item_id);
    std::uint32_t transfer_quantity =
        std::min<std::uint32_t>(quantity == 0U ? 1U : quantity, source_stack.quantity);

    if (!destination_item.is_valid())
    {
        if (transfer_quantity == source_stack.quantity)
        {
            slot_ids[source_slot_offset + source_slot_index] = 0U;
            slot_ids[destination_slot_offset + destination_slot_index] = source_item.id();
            source_item.set<StorageItemLocation>({
                destination_container.id(),
                static_cast<std::uint16_t>(destination_slot_index),
                0U});
            bump_item_membership_revision(inventory);
        }
        else
        {
            const auto created = create_item_entity(
                *world,
                destination_container,
                destination_slot_index,
                source_stack.item_id,
                transfer_quantity,
                source_stack.condition,
                source_stack.freshness);
            slot_ids[destination_slot_offset + destination_slot_index] = created.id();
            source_stack.quantity -= transfer_quantity;
            source_item.modified<StorageItemStack>();
            bump_item_membership_revision(inventory);
            bump_item_quantity_revision(inventory);
            sync_projection_views(inventory, world);
            return true;
        }
    }
    else
    {
        if (!destination_item.has<StorageItemStack>())
        {
            return false;
        }

        auto& destination_stack = destination_item.get_mut<StorageItemStack>();
        if (destination_stack.item_id != source_stack.item_id)
        {
            return false;
        }

        const auto free_capacity =
            destination_stack.quantity < max_stack ? max_stack - destination_stack.quantity : 0U;
        if (free_capacity == 0U)
        {
            return false;
        }

        transfer_quantity = std::min<std::uint32_t>(transfer_quantity, free_capacity);
        destination_stack.quantity += transfer_quantity;
        destination_item.modified<StorageItemStack>();
        source_stack.quantity -= transfer_quantity;
        bump_item_quantity_revision(inventory);
    }

    if (source_stack.quantity == 0U)
    {
        slot_ids[source_slot_offset + source_slot_index] = 0U;
        source_item.destruct();
        bump_item_membership_revision(inventory);
    }
    else
    {
        source_item.modified<StorageItemStack>();
        bump_item_quantity_revision(inventory);
    }

    sync_projection_views(inventory, world);
    return true;
}

bool transfer_between_slots(
    InventoryStateRef inventory,
    const flecs::world* world,
    Gs1InventoryContainerKind source_kind,
    std::uint32_t source_slot_index,
    Gs1InventoryContainerKind destination_kind,
    std::uint32_t destination_slot_index,
    std::uint32_t quantity,
    std::uint64_t source_owner_entity_id,
    std::uint64_t destination_owner_entity_id)
{
    return transfer_between_slots(
        inventory,
        const_cast<flecs::world*>(world),
        source_kind,
        source_slot_index,
        destination_kind,
        destination_slot_index,
        quantity,
        source_owner_entity_id,
        destination_owner_entity_id);
}

bool transfer_between_slots(
    SiteRunState& site_run,
    Gs1InventoryContainerKind source_kind,
    std::uint32_t source_slot_index,
    Gs1InventoryContainerKind destination_kind,
    std::uint32_t destination_slot_index,
    std::uint32_t quantity,
    std::uint64_t source_owner_entity_id,
    std::uint64_t destination_owner_entity_id)
{
    return transfer_between_slots(
        inventory_ref(site_run),
        ecs_world_ptr(site_run),
        source_kind,
        source_slot_index,
        destination_kind,
        destination_slot_index,
        quantity,
        source_owner_entity_id,
        destination_owner_entity_id);
}

std::uint64_t owner_device_entity_id_for_container(
    const SiteRunState& site_run,
    flecs::entity container) noexcept
{
    const auto* storage_state =
        find_storage_container_state_by_entity_id(inventory_ref(site_run), container.id());
    return storage_state == nullptr ? 0U : storage_state->owner_device_entity_id;
}

std::uint32_t slot_count_in_container(
    ConstInventoryStateRef inventory,
    flecs::world* world,
    flecs::entity container) noexcept
{
    (void)world;
    const auto* storage_state =
        find_storage_container_state_by_entity_id(inventory, container.id());
    return storage_state == nullptr
        ? 0U
        : storage_state->slot_item_count;
}

std::uint32_t slot_count_in_container(
    SiteRunState& site_run,
    flecs::entity container) noexcept
{
    auto inventory = inventory_ref(site_run);
    const auto* storage_state =
        find_storage_container_state_by_entity_id(inventory, container.id());
    return storage_state == nullptr
        ? 0U
        : storage_state->slot_item_count;
}

std::uint32_t slot_count_in_container(
    InventoryStateRef inventory,
    const flecs::world* world,
    flecs::entity container) noexcept
{
    (void)world;
    const auto* storage_state =
        find_storage_container_state_by_entity_id(inventory, container.id());
    return storage_state == nullptr
        ? 0U
        : storage_state->slot_item_count;
}

std::uint32_t slot_count_in_container(
    ConstInventoryStateRef inventory,
    const flecs::world* world,
    flecs::entity container) noexcept
{
    return slot_count_in_container(
        inventory,
        const_cast<flecs::world*>(world),
        container);
}

std::vector<std::uint64_t> collect_item_instance_ids_in_containers(
    ConstInventoryStateRef inventory,
    flecs::world* world,
    const std::vector<flecs::entity>& containers)
{
    std::vector<std::uint64_t> item_ids {};
    for (const auto& container : containers)
    {
        const auto container_items = collect_item_instance_ids_in_container(inventory, world, container);
        item_ids.insert(item_ids.end(), container_items.begin(), container_items.end());
    }

    std::sort(item_ids.begin(), item_ids.end());
    item_ids.erase(std::unique(item_ids.begin(), item_ids.end()), item_ids.end());
    return item_ids;
}

std::vector<std::uint64_t> collect_item_instance_ids_in_containers(
    InventoryStateRef inventory,
    const flecs::world* world,
    const std::vector<flecs::entity>& containers)
{
    return collect_item_instance_ids_in_containers(
        static_cast<ConstInventoryStateRef>(inventory),
        const_cast<flecs::world*>(world),
        containers);
}

std::vector<std::uint64_t> collect_item_instance_ids_in_containers(
    ConstInventoryStateRef inventory,
    const flecs::world* world,
    const std::vector<flecs::entity>& containers)
{
    std::vector<std::uint64_t> item_ids {};
    for (const auto& container : containers)
    {
        const auto container_items = collect_item_instance_ids_in_container(inventory, world, container);
        item_ids.insert(item_ids.end(), container_items.begin(), container_items.end());
    }

    std::sort(item_ids.begin(), item_ids.end());
    item_ids.erase(std::unique(item_ids.begin(), item_ids.end()), item_ids.end());
    return item_ids;
}

std::vector<std::uint64_t> collect_item_instance_ids_in_containers(
    SiteRunState& site_run,
    const std::vector<flecs::entity>& containers)
{
    std::vector<std::uint64_t> item_ids {};
    for (const auto& container : containers)
    {
        const auto container_items = collect_item_instance_ids_in_container(site_run, container);
        item_ids.insert(item_ids.end(), container_items.begin(), container_items.end());
    }

    std::sort(item_ids.begin(), item_ids.end());
    item_ids.erase(std::unique(item_ids.begin(), item_ids.end()), item_ids.end());
    return item_ids;
}

std::vector<std::uint64_t> collect_item_instance_ids_in_container(
    ConstInventoryStateRef inventory,
    flecs::world* world,
    flecs::entity container)
{
    std::vector<std::uint64_t> item_ids {};
    const auto* storage_state =
        find_storage_container_state_by_entity_id(inventory, container.id());
    if (storage_state == nullptr)
    {
        return item_ids;
    }

    for (std::uint32_t slot_index = 0U; slot_index < storage_state->slot_item_count; ++slot_index)
    {
        const auto item = item_entity_for_slot(inventory, world, container, slot_index);
        if (item.is_valid() && item.has<StorageItemStack>())
        {
            item_ids.push_back(item.id());
        }
    }

    return item_ids;
}

std::vector<std::uint64_t> collect_item_instance_ids_in_container(
    InventoryStateRef inventory,
    const flecs::world* world,
    flecs::entity container)
{
    return collect_item_instance_ids_in_container(
        static_cast<ConstInventoryStateRef>(inventory),
        const_cast<flecs::world*>(world),
        container);
}

std::vector<std::uint64_t> collect_item_instance_ids_in_container(
    ConstInventoryStateRef inventory,
    const flecs::world* world,
    flecs::entity container)
{
    std::vector<std::uint64_t> item_ids {};
    const auto* storage_state =
        find_storage_container_state_by_entity_id(inventory, container.id());
    if (storage_state == nullptr)
    {
        return item_ids;
    }

    for (std::uint32_t slot_index = 0U; slot_index < storage_state->slot_item_count; ++slot_index)
    {
        const auto item = item_entity_for_slot(inventory, world, container, slot_index);
        if (item.is_valid() && item.has<StorageItemStack>())
        {
            item_ids.push_back(item.id());
        }
    }

    return item_ids;
}

std::vector<std::uint64_t> collect_item_instance_ids_in_container(
    SiteRunState& site_run,
    flecs::entity container)
{
    auto inventory = inventory_ref(site_run);
    std::vector<std::uint64_t> item_ids {};
    const auto* storage_state =
        find_storage_container_state_by_entity_id(inventory, container.id());
    if (storage_state == nullptr)
    {
        return item_ids;
    }

    for (std::uint32_t slot_index = 0U; slot_index < storage_state->slot_item_count; ++slot_index)
    {
        const auto item = item_entity_for_slot(site_run, container, slot_index);
        if (item.is_valid() && item.has<StorageItemStack>())
        {
            item_ids.push_back(item.id());
        }
    }

    return item_ids;
}

std::vector<flecs::entity> collect_all_storage_containers(
    ConstInventoryStateRef inventory,
    flecs::world* world,
    bool include_device_storage)
{
    std::vector<flecs::entity> containers {};
    for (const auto& storage : inventory.storage_containers)
    {
        if (!include_device_storage &&
            storage.container_kind == GS1_INVENTORY_CONTAINER_DEVICE_STORAGE)
        {
            continue;
        }

        containers.push_back(entity_from_id(inventory, world, storage.container_entity_id));
    }

    std::sort(containers.begin(), containers.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.id() < rhs.id();
    });
    return containers;
}

std::vector<flecs::entity> collect_all_storage_containers(
    InventoryStateRef inventory,
    const flecs::world* world,
    bool include_device_storage)
{
    return collect_all_storage_containers(
        static_cast<ConstInventoryStateRef>(inventory),
        const_cast<flecs::world*>(world),
        include_device_storage);
}

std::vector<flecs::entity> collect_all_storage_containers(
    ConstInventoryStateRef inventory,
    const flecs::world* world,
    bool include_device_storage)
{
    std::vector<flecs::entity> containers {};
    for (const auto& storage : inventory.storage_containers)
    {
        if (!include_device_storage &&
            storage.container_kind == GS1_INVENTORY_CONTAINER_DEVICE_STORAGE)
        {
            continue;
        }

        containers.push_back(entity_from_id(inventory, world, storage.container_entity_id));
    }

    std::sort(containers.begin(), containers.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.id() < rhs.id();
    });
    return containers;
}

std::vector<flecs::entity> collect_all_storage_containers(
    SiteRunState& site_run,
    bool include_device_storage)
{
    auto inventory = inventory_ref(site_run);
    std::vector<flecs::entity> containers {};
    for (const auto& storage : inventory.storage_containers)
    {
        if (!include_device_storage &&
            storage.container_kind == GS1_INVENTORY_CONTAINER_DEVICE_STORAGE)
        {
            continue;
        }

        containers.push_back(entity_from_id(inventory, ecs_world_ptr(site_run), storage.container_entity_id));
    }

    std::sort(containers.begin(), containers.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.id() < rhs.id();
    });
    return containers;
}

TileCoord container_tile_coord(SiteRunState& site_run, flecs::entity container) noexcept
{
    auto inventory = inventory_ref(site_run);
    const auto* storage_state =
        find_storage_container_state_by_entity_id(inventory, container.id());
    return storage_state == nullptr ? TileCoord {} : storage_state->tile_coord;
}

flecs::entity ensure_device_storage_container(
    InventoryStateRef inventory,
    flecs::world& world,
    std::uint64_t device_entity_id,
    TileCoord tile_coord,
    std::uint32_t slot_count,
    std::uint32_t flags)
{
    return ensure_storage_container(
        inventory,
        world,
        StorageContainerKind::DeviceStorage,
        slot_count,
        tile_coord,
        0U,
        device_entity_id,
        flags);
}

flecs::entity ensure_device_storage_container(
    SiteRunState& site_run,
    std::uint64_t device_entity_id,
    TileCoord tile_coord,
    std::uint32_t slot_count,
    std::uint32_t flags)
{
    return ensure_storage_container(
        site_run,
        StorageContainerKind::DeviceStorage,
        slot_count,
        tile_coord,
        true,
        device_entity_id,
        flags);
}

flecs::entity find_device_storage_container(
    InventoryStateRef inventory,
    flecs::world* world,
    std::uint64_t device_entity_id) noexcept
{
    return find_container_entity(
        inventory,
        world,
        StorageContainerKind::DeviceStorage,
        device_entity_id);
}

flecs::entity find_device_storage_container(
    ConstInventoryStateRef inventory,
    const flecs::world* world,
    std::uint64_t device_entity_id) noexcept
{
    return find_container_entity(
        inventory,
        world,
        StorageContainerKind::DeviceStorage,
        device_entity_id);
}

flecs::entity find_device_storage_container(
    InventoryStateRef inventory,
    const flecs::world* world,
    std::uint64_t device_entity_id) noexcept
{
    return find_device_storage_container(
        inventory,
        const_cast<flecs::world*>(world),
        device_entity_id);
}

flecs::entity find_device_storage_container(
    SiteRunState& site_run,
    std::uint64_t device_entity_id) noexcept
{
    return find_container_entity(
        site_run,
        StorageContainerKind::DeviceStorage,
        device_entity_id);
}

bool destroy_storage_container(
    InventoryStateRef inventory,
    flecs::world* world,
    flecs::entity container)
{
    auto* storage_state =
        find_storage_container_state_by_entity_id(inventory, container.id());
    if (storage_state == nullptr || !container.is_valid() || world == nullptr)
    {
        return false;
    }

    bool removed_items = false;
    const auto* slot_ids = slot_items_ptr(inventory, *storage_state);
    if (slot_ids == nullptr)
    {
        return false;
    }

    for (std::uint32_t slot_index = 0U; slot_index < storage_state->slot_item_count; ++slot_index)
    {
        const auto item_entity_id = slot_ids[slot_index];
        if (item_entity_id == 0U)
        {
            continue;
        }

        auto item_entity = entity_from_id(inventory, world, item_entity_id);
        if (item_entity.is_valid())
        {
            item_entity.destruct();
        }

        removed_items = true;
    }

    const auto storage_id = storage_state->storage_id;
    container.destruct();
    auto& containers = inventory.storage_containers;
    containers.erase(
        std::remove_if(
            containers.begin(),
            containers.end(),
            [storage_id](const StorageContainerEntryState& storage) {
                return storage.storage_id == storage_id;
            }),
        containers.end());

    if (removed_items)
    {
        bump_item_membership_revision(inventory);
    }

    sync_projection_views(inventory, world);
    return true;
}

bool destroy_storage_container(
    InventoryStateRef inventory,
    const flecs::world* world,
    flecs::entity container)
{
    return destroy_storage_container(
        inventory,
        const_cast<flecs::world*>(world),
        container);
}

bool destroy_storage_container(SiteRunState& site_run, flecs::entity container)
{
    return destroy_storage_container(inventory_ref(site_run), ecs_world_ptr(site_run), container);
}

const StorageContainerEntryState* storage_container_state_for_storage_id(
    ConstInventoryStateRef inventory,
    std::uint32_t storage_id) noexcept
{
    return find_storage_container_state_by_storage_id(inventory, storage_id);
}

const StorageContainerEntryState* storage_container_state_for_storage_id(
    InventoryStateRef inventory,
    std::uint32_t storage_id) noexcept
{
    return find_storage_container_state_by_storage_id(inventory, storage_id);
}

const StorageContainerEntryState* storage_container_state_for_storage_id(
    const SiteRunState& site_run,
    std::uint32_t storage_id) noexcept
{
    return find_storage_container_state_by_storage_id(inventory_ref(site_run), storage_id);
}
}  // namespace gs1::inventory_storage

