#pragma once

#include "gs1/types.h"
#include "support/id_types.h"

#include <optional>
#include <vector>

namespace gs1
{
enum class ActionKind : std::uint32_t
{
    None = 0,
    Plant = 1,
    Build = 2,
    Repair = 3,
    Water = 4,
    ClearBurial = 5,
    Craft = 6,
    Drink = 7,
    Eat = 8
};

struct ReservedItemStack final
{
    ItemId item_id {};
    std::uint32_t quantity {0};
    Gs1InventoryContainerKind container_kind {GS1_INVENTORY_CONTAINER_WORKER_PACK};
    std::uint8_t reserved0[3] {};
};

struct ActionState final
{
    std::optional<RuntimeActionId> current_action_id {};
    ActionKind action_kind {ActionKind::None};
    std::optional<TileCoord> target_tile {};
    std::optional<TileCoord> approach_tile {};
    std::uint32_t primary_subject_id {0};
    std::uint32_t secondary_subject_id {0};
    std::uint32_t item_id {0};
    std::uint32_t placement_reservation_token {0};
    std::uint16_t quantity {0};
    std::uint8_t request_flags {0};
    bool awaiting_placement_reservation {false};
    double total_action_minutes {0.0};
    double remaining_action_minutes {0.0};
    std::vector<ReservedItemStack> reserved_input_item_stacks {};
    std::optional<double> started_at_world_minute {};
};
}  // namespace gs1
