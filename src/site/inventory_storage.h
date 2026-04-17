#pragma once

#include "content/defs/item_defs.h"
#include "gs1/types.h"
#include "site/inventory_state.h"
#include "site/site_run_state.h"
#include "site/site_world.h"
#include "site/site_world_components.h"

#include <flecs.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace gs1::inventory_storage
{
using namespace site_ecs;

inline constexpr std::uint32_t k_default_device_storage_slot_count = 6U;

inline flecs::world* ecs_world_ptr(SiteRunState& site_run) noexcept
{
    return site_run.site_world == nullptr ? nullptr : &site_run.site_world->ecs_world();
}

inline const flecs::world* ecs_world_ptr(const SiteRunState& site_run) noexcept
{
    return site_run.site_world == nullptr ? nullptr : &site_run.site_world->ecs_world();
}

inline flecs::entity entity_from_id(SiteRunState& site_run, std::uint64_t entity_id) noexcept
{
    auto* world = ecs_world_ptr(site_run);
    return world == nullptr || entity_id == 0U ? flecs::entity {} : world->entity(entity_id);
}

inline flecs::entity entity_from_id(const SiteRunState& site_run, std::uint64_t entity_id) noexcept
{
    const auto* world = ecs_world_ptr(site_run);
    return world == nullptr || entity_id == 0U ? flecs::entity {} : const_cast<flecs::world*>(world)->entity(entity_id);
}

inline StorageContainerState* find_storage_container_state_by_entity_id(
    InventoryState& inventory,
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

inline const StorageContainerState* find_storage_container_state_by_entity_id(
    const InventoryState& inventory,
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

inline StorageContainerState* find_storage_container_state_by_storage_id(
    InventoryState& inventory,
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

inline const StorageContainerState* find_storage_container_state_by_storage_id(
    const InventoryState& inventory,
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

inline void bump_item_membership_revision(InventoryState& inventory) noexcept
{
    inventory.item_membership_revision += 1U;
    inventory.item_quantity_revision += 1U;
}

inline void bump_item_quantity_revision(InventoryState& inventory) noexcept
{
    inventory.item_quantity_revision += 1U;
}

inline flecs::entity tile_entity_for_coord(SiteRunState& site_run, TileCoord coord) noexcept
{
    if (site_run.site_world == nullptr || !site_run.site_world->contains(coord))
    {
        return {};
    }

    return site_run.site_world->ecs_world().entity(site_run.site_world->tile_entity_id(coord));
}

inline flecs::entity find_container_entity(
    SiteRunState& site_run,
    StorageContainerKind kind,
    std::uint64_t owner_device_entity_id = 0U) noexcept
{
    for (const auto& storage : site_run.inventory.storage_containers)
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

        return entity_from_id(site_run, storage.container_entity_id);
    }

    return {};
}

inline flecs::entity ensure_storage_container(
    SiteRunState& site_run,
    StorageContainerKind kind,
    std::uint32_t slot_count,
    TileCoord tile_coord = TileCoord {0, 0},
    bool attach_to_tile = false,
    std::uint64_t owner_device_entity_id = 0U)
{
    auto* world = ecs_world_ptr(site_run);
    if (world == nullptr)
    {
        return {};
    }

    auto container = find_container_entity(site_run, kind, owner_device_entity_id);
    if (!container.is_valid())
    {
        container = world->entity()
            .add<StorageContainerTag>()
            .set<StorageContainerKindComponent>({kind});

        if (attach_to_tile)
        {
            const auto tile_entity = tile_entity_for_coord(site_run, tile_coord);
            if (tile_entity.is_valid())
            {
                container.add<StorageAtTile>(tile_entity);
            }
        }

        if (owner_device_entity_id != 0U)
        {
            container.add<StorageOwnedByDevice>(world->entity(owner_device_entity_id));
        }
    }

    auto* storage_state =
        find_storage_container_state_by_entity_id(site_run.inventory, container.id());
    if (storage_state == nullptr)
    {
        site_run.inventory.storage_containers.push_back(StorageContainerState {});
        storage_state = &site_run.inventory.storage_containers.back();
        storage_state->storage_id = site_run.inventory.next_storage_id++;
        storage_state->container_entity_id = container.id();
    }

    storage_state->owner_device_entity_id = owner_device_entity_id;
    storage_state->container_kind =
        kind == StorageContainerKind::WorkerPack
            ? GS1_INVENTORY_CONTAINER_WORKER_PACK
            : GS1_INVENTORY_CONTAINER_DEVICE_STORAGE;
    storage_state->tile_coord = tile_coord;
    if (storage_state->slot_item_instance_ids.size() != slot_count)
    {
        storage_state->slot_item_instance_ids.resize(slot_count, 0ULL);
    }

    return container;
}

inline flecs::entity worker_pack_container(SiteRunState& site_run) noexcept
{
    return entity_from_id(site_run, site_run.inventory.worker_pack_container_entity_id);
}

inline flecs::entity find_device_storage_container(
    SiteRunState& site_run,
    std::uint64_t device_entity_id) noexcept;

inline flecs::entity starter_storage_container(SiteRunState& site_run) noexcept
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

inline flecs::entity container_for_kind(
    SiteRunState& site_run,
    Gs1InventoryContainerKind kind,
    std::uint64_t owner_entity_id = 0U) noexcept
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

inline flecs::entity container_for_storage_id(
    SiteRunState& site_run,
    std::uint32_t storage_id) noexcept
{
    const auto* storage_state =
        find_storage_container_state_by_storage_id(site_run.inventory, storage_id);
    return storage_state == nullptr
        ? flecs::entity {}
        : entity_from_id(site_run, storage_state->container_entity_id);
}

inline std::uint32_t storage_id_for_container(
    const SiteRunState& site_run,
    flecs::entity container) noexcept
{
    if (!container.is_valid())
    {
        return 0U;
    }

    const auto* storage_state =
        find_storage_container_state_by_entity_id(site_run.inventory, container.id());
    return storage_state == nullptr ? 0U : storage_state->storage_id;
}

inline std::uint32_t worker_pack_storage_id(const SiteRunState& site_run) noexcept
{
    return site_run.inventory.worker_pack_storage_id;
}

inline flecs::entity item_entity_for_slot(
    SiteRunState& site_run,
    flecs::entity container,
    std::uint32_t slot_index) noexcept
{
    const auto* storage_state =
        find_storage_container_state_by_entity_id(site_run.inventory, container.id());
    if (storage_state == nullptr || slot_index >= storage_state->slot_item_instance_ids.size())
    {
        return {};
    }

    return entity_from_id(site_run, storage_state->slot_item_instance_ids[slot_index]);
}

inline flecs::entity item_entity_for_slot(
    const SiteRunState& site_run,
    flecs::entity container,
    std::uint32_t slot_index) noexcept
{
    return item_entity_for_slot(const_cast<SiteRunState&>(site_run), container, slot_index);
}

inline const StorageItemStack* stack_data(const SiteRunState& site_run, flecs::entity item) noexcept
{
    (void)site_run;
    return item.is_valid() && item.has<StorageItemStack>()
        ? &item.get<StorageItemStack>()
        : nullptr;
}

inline std::uint32_t stack_limit(ItemId item_id) noexcept
{
    return item_stack_size(item_id);
}

inline std::vector<std::uint32_t> collect_item_instance_ids_in_container(
    SiteRunState& site_run,
    flecs::entity container);

inline void fill_projection_slot_from_entities(
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

inline void sync_projection_slots_for_container(
    SiteRunState& site_run,
    flecs::entity container,
    std::vector<InventorySlot>& destination)
{
    const auto* storage_state =
        find_storage_container_state_by_entity_id(site_run.inventory, container.id());
    if (storage_state == nullptr)
    {
        std::fill(destination.begin(), destination.end(), InventorySlot {});
        return;
    }

    if (destination.size() != storage_state->slot_item_instance_ids.size())
    {
        destination.resize(storage_state->slot_item_instance_ids.size());
    }

    for (std::size_t slot_index = 0; slot_index < storage_state->slot_item_instance_ids.size(); ++slot_index)
    {
        fill_projection_slot_from_entities(
            destination[slot_index],
            item_entity_for_slot(site_run, container, static_cast<std::uint32_t>(slot_index)));
    }
}

inline void sync_projection_views(SiteRunState& site_run)
{
    sync_projection_slots_for_container(
        site_run,
        worker_pack_container(site_run),
        site_run.inventory.worker_pack_slots);
}

inline void initialize_site_inventory_storage(SiteRunState& site_run)
{
    if (site_run.inventory.worker_pack_slots.size() != site_run.inventory.worker_pack_slot_count)
    {
        site_run.inventory.worker_pack_slots.resize(site_run.inventory.worker_pack_slot_count);
    }

    const auto worker_container = ensure_storage_container(
        site_run,
        StorageContainerKind::WorkerPack,
        site_run.inventory.worker_pack_slot_count);

    site_run.inventory.worker_pack_container_entity_id = worker_container.id();
    site_run.inventory.worker_pack_storage_id = storage_id_for_container(site_run, worker_container);
    sync_projection_views(site_run);
}

inline flecs::entity create_item_entity(
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

inline void import_projection_slots_into_container_if_needed(
    SiteRunState& site_run,
    flecs::entity container,
    const std::vector<InventorySlot>& source_slots)
{
    auto* world = ecs_world_ptr(site_run);
    auto* storage_state =
        find_storage_container_state_by_entity_id(site_run.inventory, container.id());
    if (world == nullptr || storage_state == nullptr)
    {
        return;
    }

    for (std::size_t slot_index = 0; slot_index < source_slots.size(); ++slot_index)
    {
        if (slot_index >= storage_state->slot_item_instance_ids.size() ||
            storage_state->slot_item_instance_ids[slot_index] != 0U)
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
        storage_state->slot_item_instance_ids[slot_index] = item.id();
        bump_item_membership_revision(site_run.inventory);
    }
}

inline void import_projection_views_into_storage_if_needed(SiteRunState& site_run)
{
    const auto worker_projection = site_run.inventory.worker_pack_slots;
    initialize_site_inventory_storage(site_run);
    import_projection_slots_into_container_if_needed(
        site_run,
        worker_pack_container(site_run),
        worker_projection);
    sync_projection_views(site_run);
}

inline bool storage_initialized(const SiteRunState& site_run) noexcept
{
    return site_run.inventory.worker_pack_container_entity_id != 0U;
}

inline std::uint32_t add_item_to_container(
    SiteRunState& site_run,
    flecs::entity container,
    ItemId item_id,
    std::uint32_t quantity,
    float condition = 1.0f,
    float freshness = 1.0f)
{
    auto* world = ecs_world_ptr(site_run);
    auto* storage_state =
        find_storage_container_state_by_entity_id(site_run.inventory, container.id());
    if (world == nullptr || storage_state == nullptr || quantity == 0U)
    {
        return quantity;
    }

    std::uint32_t remaining = quantity;
    const auto max_stack = stack_limit(item_id);

    for (std::uint32_t slot_index = 0U;
         slot_index < storage_state->slot_item_instance_ids.size() && remaining > 0U;
         ++slot_index)
    {
        const auto item = item_entity_for_slot(site_run, container, slot_index);
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
        bump_item_quantity_revision(site_run.inventory);
    }

    for (std::uint32_t slot_index = 0U;
         slot_index < storage_state->slot_item_instance_ids.size() && remaining > 0U;
         ++slot_index)
    {
        if (storage_state->slot_item_instance_ids[slot_index] != 0U)
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
        storage_state->slot_item_instance_ids[slot_index] = item.id();
        remaining -= transfer;
        bump_item_membership_revision(site_run.inventory);
    }

    sync_projection_views(site_run);
    return remaining;
}

inline std::uint32_t available_item_quantity_in_container(
    SiteRunState& site_run,
    flecs::entity container,
    ItemId item_id) noexcept
{
    const auto* storage_state =
        find_storage_container_state_by_entity_id(site_run.inventory, container.id());
    if (storage_state == nullptr)
    {
        return 0U;
    }

    std::uint32_t total = 0U;
    for (std::uint32_t slot_index = 0U; slot_index < storage_state->slot_item_instance_ids.size(); ++slot_index)
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

inline std::uint32_t available_item_quantity_in_container_kind(
    SiteRunState& site_run,
    Gs1InventoryContainerKind kind,
    ItemId item_id,
    std::uint64_t owner_entity_id = 0U) noexcept
{
    return available_item_quantity_in_container(
        site_run,
        container_for_kind(site_run, kind, owner_entity_id),
        item_id);
}

inline std::uint32_t available_item_quantity_in_containers(
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

inline std::uint32_t consume_item_type_from_container(
    SiteRunState& site_run,
    flecs::entity container,
    ItemId item_id,
    std::uint32_t quantity)
{
    auto* storage_state =
        find_storage_container_state_by_entity_id(site_run.inventory, container.id());
    if (storage_state == nullptr || quantity == 0U)
    {
        return quantity;
    }

    std::uint32_t remaining = quantity;
    for (std::uint32_t slot_index = 0U;
         slot_index < storage_state->slot_item_instance_ids.size() && remaining > 0U;
         ++slot_index)
    {
        auto item = item_entity_for_slot(site_run, container, slot_index);
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
            storage_state->slot_item_instance_ids[slot_index] = 0U;
            item.destruct();
            bump_item_membership_revision(site_run.inventory);
        }
        else
        {
            item.modified<StorageItemStack>();
            bump_item_quantity_revision(site_run.inventory);
        }
    }

    sync_projection_views(site_run);
    return remaining;
}

inline bool consume_quantity_from_item_entity(
    SiteRunState& site_run,
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
        find_storage_container_state_by_entity_id(site_run.inventory, location.container_entity_id);
    if (storage_state == nullptr || location.slot_index >= storage_state->slot_item_instance_ids.size())
    {
        return false;
    }

    stack.quantity -= quantity;
    if (stack.quantity == 0U)
    {
        storage_state->slot_item_instance_ids[location.slot_index] = 0U;
        item_entity.destruct();
        bump_item_membership_revision(site_run.inventory);
    }
    else
    {
        item_entity.modified<StorageItemStack>();
        bump_item_quantity_revision(site_run.inventory);
    }

    sync_projection_views(site_run);
    return true;
}

inline std::uint32_t consume_item_type_from_containers(
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

inline bool can_fit_item_in_container(
    SiteRunState& site_run,
    flecs::entity container,
    ItemId item_id,
    std::uint32_t quantity) noexcept
{
    const auto* storage_state =
        find_storage_container_state_by_entity_id(site_run.inventory, container.id());
    if (storage_state == nullptr)
    {
        return false;
    }

    std::uint32_t remaining = quantity;
    const auto max_stack = stack_limit(item_id);
    for (std::uint32_t slot_index = 0U;
         slot_index < storage_state->slot_item_instance_ids.size() && remaining > 0U;
         ++slot_index)
    {
        const auto item = item_entity_for_slot(site_run, container, slot_index);
        const auto* stack = stack_data(site_run, item);
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

inline bool transfer_between_slots(
    SiteRunState& site_run,
    Gs1InventoryContainerKind source_kind,
    std::uint32_t source_slot_index,
    Gs1InventoryContainerKind destination_kind,
    std::uint32_t destination_slot_index,
    std::uint32_t quantity,
    std::uint64_t source_owner_entity_id = 0U,
    std::uint64_t destination_owner_entity_id = 0U)
{
    auto source_container = container_for_kind(site_run, source_kind, source_owner_entity_id);
    auto destination_container =
        container_for_kind(site_run, destination_kind, destination_owner_entity_id);
    auto* source_state =
        find_storage_container_state_by_entity_id(site_run.inventory, source_container.id());
    auto* destination_state =
        find_storage_container_state_by_entity_id(site_run.inventory, destination_container.id());
    if (source_state == nullptr ||
        destination_state == nullptr ||
        source_slot_index >= source_state->slot_item_instance_ids.size() ||
        destination_slot_index >= destination_state->slot_item_instance_ids.size())
    {
        return false;
    }

    auto source_item = item_entity_for_slot(site_run, source_container, source_slot_index);
    auto destination_item = item_entity_for_slot(site_run, destination_container, destination_slot_index);
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
            source_state->slot_item_instance_ids[source_slot_index] = 0U;
            destination_state->slot_item_instance_ids[destination_slot_index] = source_item.id();
            source_item.set<StorageItemLocation>({
                destination_container.id(),
                static_cast<std::uint16_t>(destination_slot_index),
                0U});
            bump_item_membership_revision(site_run.inventory);
        }
        else
        {
            const auto created = create_item_entity(
                site_run.site_world->ecs_world(),
                destination_container,
                destination_slot_index,
                source_stack.item_id,
                transfer_quantity,
                source_stack.condition,
                source_stack.freshness);
            destination_state->slot_item_instance_ids[destination_slot_index] = created.id();
            source_stack.quantity -= transfer_quantity;
            source_item.modified<StorageItemStack>();
            bump_item_membership_revision(site_run.inventory);
            bump_item_quantity_revision(site_run.inventory);
            sync_projection_views(site_run);
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
        bump_item_quantity_revision(site_run.inventory);
    }

    if (source_stack.quantity == 0U)
    {
        source_state->slot_item_instance_ids[source_slot_index] = 0U;
        source_item.destruct();
        bump_item_membership_revision(site_run.inventory);
    }
    else
    {
        source_item.modified<StorageItemStack>();
        bump_item_quantity_revision(site_run.inventory);
    }

    sync_projection_views(site_run);
    return true;
}

inline std::uint64_t owner_device_entity_id_for_container(
    const SiteRunState& site_run,
    flecs::entity container) noexcept
{
    const auto* storage_state =
        find_storage_container_state_by_entity_id(site_run.inventory, container.id());
    return storage_state == nullptr ? 0U : storage_state->owner_device_entity_id;
}

inline std::uint32_t slot_count_in_container(
    SiteRunState& site_run,
    flecs::entity container) noexcept
{
    const auto* storage_state =
        find_storage_container_state_by_entity_id(site_run.inventory, container.id());
    return storage_state == nullptr
        ? 0U
        : static_cast<std::uint32_t>(storage_state->slot_item_instance_ids.size());
}

inline std::vector<std::uint32_t> collect_item_instance_ids_in_containers(
    SiteRunState& site_run,
    const std::vector<flecs::entity>& containers)
{
    std::vector<std::uint32_t> item_ids {};
    for (const auto& container : containers)
    {
        const auto container_items = collect_item_instance_ids_in_container(site_run, container);
        item_ids.insert(item_ids.end(), container_items.begin(), container_items.end());
    }

    std::sort(item_ids.begin(), item_ids.end());
    item_ids.erase(std::unique(item_ids.begin(), item_ids.end()), item_ids.end());
    return item_ids;
}

inline std::vector<std::uint32_t> collect_item_instance_ids_in_container(
    SiteRunState& site_run,
    flecs::entity container)
{
    std::vector<std::uint32_t> item_ids {};
    const auto* storage_state =
        find_storage_container_state_by_entity_id(site_run.inventory, container.id());
    if (storage_state == nullptr)
    {
        return item_ids;
    }

    for (const auto item_id : storage_state->slot_item_instance_ids)
    {
        if (item_id != 0U)
        {
            item_ids.push_back(static_cast<std::uint32_t>(item_id));
        }
    }

    return item_ids;
}

inline std::vector<flecs::entity> collect_all_storage_containers(
    SiteRunState& site_run,
    bool include_device_storage = true)
{
    std::vector<flecs::entity> containers {};
    for (const auto& storage : site_run.inventory.storage_containers)
    {
        if (!include_device_storage &&
            storage.container_kind == GS1_INVENTORY_CONTAINER_DEVICE_STORAGE)
        {
            continue;
        }

        containers.push_back(entity_from_id(site_run, storage.container_entity_id));
    }

    std::sort(containers.begin(), containers.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.id() < rhs.id();
    });
    return containers;
}

inline TileCoord container_tile_coord(SiteRunState& site_run, flecs::entity container) noexcept
{
    const auto* storage_state =
        find_storage_container_state_by_entity_id(site_run.inventory, container.id());
    return storage_state == nullptr ? TileCoord {} : storage_state->tile_coord;
}

inline flecs::entity ensure_device_storage_container(
    SiteRunState& site_run,
    std::uint64_t device_entity_id,
    TileCoord tile_coord,
    std::uint32_t slot_count = k_default_device_storage_slot_count)
{
    return ensure_storage_container(
        site_run,
        StorageContainerKind::DeviceStorage,
        slot_count,
        tile_coord,
        true,
        device_entity_id);
}

inline flecs::entity find_device_storage_container(
    SiteRunState& site_run,
    std::uint64_t device_entity_id) noexcept
{
    return find_container_entity(
        site_run,
        StorageContainerKind::DeviceStorage,
        device_entity_id);
}

inline const StorageContainerState* storage_container_state_for_storage_id(
    const SiteRunState& site_run,
    std::uint32_t storage_id) noexcept
{
    return find_storage_container_state_by_storage_id(site_run.inventory, storage_id);
}
}  // namespace gs1::inventory_storage
