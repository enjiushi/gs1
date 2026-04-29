#pragma once

#include "site/inventory_state.h"
#include "site/site_run_state.h"
#include "site/site_world_components.h"

#include <flecs.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace gs1::inventory_storage
{
using namespace site_ecs;

inline constexpr std::uint32_t k_default_device_storage_slot_count = 6U;
inline constexpr std::uint32_t k_inventory_storage_flag_delivery_box = 1U << 0U;
inline constexpr std::uint32_t k_inventory_storage_flag_retrieval_only = 1U << 1U;

flecs::world* ecs_world_ptr(SiteRunState& site_run) noexcept;
const flecs::world* ecs_world_ptr(const SiteRunState& site_run) noexcept;
flecs::entity entity_from_id(SiteRunState& site_run, std::uint64_t entity_id) noexcept;
flecs::entity entity_from_id(const SiteRunState& site_run, std::uint64_t entity_id) noexcept;

StorageContainerState* find_storage_container_state_by_entity_id(
    InventoryState& inventory,
    std::uint64_t container_entity_id) noexcept;

const StorageContainerState* find_storage_container_state_by_entity_id(
    const InventoryState& inventory,
    std::uint64_t container_entity_id) noexcept;

StorageContainerState* find_storage_container_state_by_storage_id(
    InventoryState& inventory,
    std::uint32_t storage_id) noexcept;

const StorageContainerState* find_storage_container_state_by_storage_id(
    const InventoryState& inventory,
    std::uint32_t storage_id) noexcept;

void bump_item_membership_revision(InventoryState& inventory) noexcept;
void bump_item_quantity_revision(InventoryState& inventory) noexcept;

flecs::entity tile_entity_for_coord(SiteRunState& site_run, TileCoord coord) noexcept;
flecs::entity find_container_entity(
    SiteRunState& site_run,
    StorageContainerKind kind,
    std::uint64_t owner_device_entity_id = 0U) noexcept;

flecs::entity ensure_storage_container(
    SiteRunState& site_run,
    StorageContainerKind kind,
    std::uint32_t slot_count,
    TileCoord tile_coord = TileCoord {0, 0},
    bool attach_to_tile = false,
    std::uint64_t owner_device_entity_id = 0U,
    std::uint32_t flags = 0U);

flecs::entity worker_pack_container(SiteRunState& site_run) noexcept;
flecs::entity starter_storage_container(SiteRunState& site_run) noexcept;
flecs::entity delivery_box_container(SiteRunState& site_run) noexcept;

flecs::entity container_for_kind(
    SiteRunState& site_run,
    Gs1InventoryContainerKind kind,
    std::uint64_t owner_entity_id = 0U) noexcept;

flecs::entity container_for_storage_id(
    SiteRunState& site_run,
    std::uint32_t storage_id) noexcept;

std::uint32_t storage_id_for_container(
    const SiteRunState& site_run,
    flecs::entity container) noexcept;

std::uint32_t worker_pack_storage_id(const SiteRunState& site_run) noexcept;

flecs::entity item_entity_for_slot(
    SiteRunState& site_run,
    flecs::entity container,
    std::uint32_t slot_index) noexcept;

flecs::entity item_entity_for_slot(
    const SiteRunState& site_run,
    flecs::entity container,
    std::uint32_t slot_index) noexcept;

const StorageItemStack* stack_data(
    const SiteRunState& site_run,
    flecs::entity item) noexcept;

std::uint32_t stack_limit(ItemId item_id) noexcept;

void fill_projection_slot_from_entities(
    InventorySlot& slot_view,
    flecs::entity item_entity);

void sync_projection_slots_for_container(
    SiteRunState& site_run,
    flecs::entity container,
    std::vector<InventorySlot>& destination);

void sync_projection_views(SiteRunState& site_run);
void initialize_site_inventory_storage(SiteRunState& site_run);

flecs::entity create_item_entity(
    flecs::world& world,
    flecs::entity container,
    std::uint32_t slot_index,
    ItemId item_id,
    std::uint32_t quantity,
    float condition,
    float freshness);

void import_projection_slots_into_container_if_needed(
    SiteRunState& site_run,
    flecs::entity container,
    const std::vector<InventorySlot>& source_slots);

void import_projection_views_into_storage_if_needed(SiteRunState& site_run);
bool storage_initialized(const SiteRunState& site_run) noexcept;

std::uint32_t add_item_to_container(
    SiteRunState& site_run,
    flecs::entity container,
    ItemId item_id,
    std::uint32_t quantity,
    float condition = 1.0f,
    float freshness = 1.0f);

std::uint32_t available_item_quantity_in_container(
    SiteRunState& site_run,
    flecs::entity container,
    ItemId item_id) noexcept;

std::uint32_t available_item_quantity_in_container_kind(
    SiteRunState& site_run,
    Gs1InventoryContainerKind kind,
    ItemId item_id,
    std::uint64_t owner_entity_id = 0U) noexcept;

std::uint32_t available_item_quantity_in_containers(
    SiteRunState& site_run,
    const std::vector<flecs::entity>& containers,
    ItemId item_id) noexcept;

std::uint32_t consume_item_type_from_container(
    SiteRunState& site_run,
    flecs::entity container,
    ItemId item_id,
    std::uint32_t quantity);

bool consume_quantity_from_item_entity(
    SiteRunState& site_run,
    flecs::entity item_entity,
    std::uint32_t quantity);

std::uint32_t consume_item_type_from_containers(
    SiteRunState& site_run,
    const std::vector<flecs::entity>& containers,
    ItemId item_id,
    std::uint32_t quantity);

bool can_fit_item_in_container(
    SiteRunState& site_run,
    flecs::entity container,
    ItemId item_id,
    std::uint32_t quantity) noexcept;

bool transfer_between_slots(
    SiteRunState& site_run,
    Gs1InventoryContainerKind source_kind,
    std::uint32_t source_slot_index,
    Gs1InventoryContainerKind destination_kind,
    std::uint32_t destination_slot_index,
    std::uint32_t quantity,
    std::uint64_t source_owner_entity_id = 0U,
    std::uint64_t destination_owner_entity_id = 0U);

std::uint64_t owner_device_entity_id_for_container(
    const SiteRunState& site_run,
    flecs::entity container) noexcept;

std::uint32_t slot_count_in_container(
    SiteRunState& site_run,
    flecs::entity container) noexcept;

std::vector<std::uint64_t> collect_item_instance_ids_in_containers(
    SiteRunState& site_run,
    const std::vector<flecs::entity>& containers);

std::vector<std::uint64_t> collect_item_instance_ids_in_container(
    SiteRunState& site_run,
    flecs::entity container);

std::vector<flecs::entity> collect_all_storage_containers(
    SiteRunState& site_run,
    bool include_device_storage = true);

TileCoord container_tile_coord(
    SiteRunState& site_run,
    flecs::entity container) noexcept;

flecs::entity ensure_device_storage_container(
    SiteRunState& site_run,
    std::uint64_t device_entity_id,
    TileCoord tile_coord,
    std::uint32_t slot_count = k_default_device_storage_slot_count,
    std::uint32_t flags = 0U);

flecs::entity find_device_storage_container(
    SiteRunState& site_run,
    std::uint64_t device_entity_id) noexcept;

bool destroy_storage_container(SiteRunState& site_run, flecs::entity container);

const StorageContainerState* storage_container_state_for_storage_id(
    const SiteRunState& site_run,
    std::uint32_t storage_id) noexcept;
}  // namespace gs1::inventory_storage
