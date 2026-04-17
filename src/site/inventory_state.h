#pragma once

#include "gs1/types.h"
#include "support/id_types.h"

#include <cstdint>
#include <vector>

namespace gs1
{
struct StorageContainerState final
{
    std::uint32_t storage_id {0};
    std::uint64_t container_entity_id {0};
    // Recycled Flecs device entities can carry generation bits above 32 bits.
    std::uint64_t owner_device_entity_id {0};
    Gs1InventoryContainerKind container_kind {GS1_INVENTORY_CONTAINER_WORKER_PACK};
    TileCoord tile_coord {};
    std::vector<std::uint64_t> slot_item_instance_ids {};
};

struct InventorySlot final
{
    std::uint32_t item_instance_id {0};
    ItemId item_id {};
    std::uint32_t item_quantity {0};
    float item_condition {1.0f};
    float item_freshness {1.0f};
    bool occupied {false};
};

struct PendingDelivery final
{
    DeliveryId delivery_id {};
    std::vector<InventorySlot> item_stacks {};
    double minutes_until_arrival {0.0};
};

struct InventoryState final
{
    std::uint32_t next_storage_id {1};
    std::uint32_t worker_pack_slot_count {0};
    std::uint32_t worker_pack_storage_id {0};
    std::uint32_t opened_device_storage_id {0};
    std::uint32_t last_projected_opened_device_storage_id {0};
    std::uint64_t worker_pack_container_entity_id {0};
    std::uint64_t item_membership_revision {1U};
    std::uint64_t item_quantity_revision {1U};
    std::vector<StorageContainerState> storage_containers {};
    std::vector<InventorySlot> worker_pack_slots {};
    std::vector<PendingDelivery> pending_delivery_queue {};
};
}  // namespace gs1
