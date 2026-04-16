#pragma once

#include "support/id_types.h"

#include <cstdint>
#include <vector>

namespace gs1
{
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
    std::uint32_t worker_pack_slot_count {0};
    std::uint32_t camp_storage_slot_count {0};
    std::uint64_t worker_pack_container_entity_id {0};
    std::uint64_t camp_storage_container_entity_id {0};
    std::uint64_t item_membership_revision {1U};
    std::uint64_t item_quantity_revision {1U};
    std::vector<InventorySlot> worker_pack_slots {};
    std::vector<InventorySlot> camp_storage_slots {};
    std::vector<PendingDelivery> pending_delivery_queue {};
};
}  // namespace gs1
