#pragma once

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
    ClearBurial = 5
};

struct ReservedItemStack final
{
    ItemId item_id {};
    std::uint32_t quantity {0};
};

struct ActionState final
{
    std::optional<RuntimeActionId> current_action_id {};
    ActionKind action_kind {ActionKind::None};
    std::optional<TileCoord> target_tile {};
    double remaining_action_minutes {0.0};
    std::vector<ReservedItemStack> reserved_input_item_stacks {};
    std::optional<double> started_at_world_minute {};
};
}  // namespace gs1
