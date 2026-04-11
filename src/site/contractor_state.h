#pragma once

#include "support/id_types.h"

#include <cstdint>
#include <optional>
#include <vector>

namespace gs1
{
enum class WorkTaskKind : std::uint32_t
{
    None = 0,
    Build = 1,
    Repair = 2,
    ClearBurial = 3
};

struct WorkOrderState final
{
    WorkOrderId work_order_id {};
    WorkTaskKind task_kind {WorkTaskKind::None};
    std::optional<TileCoord> target_tile {};
    std::uint32_t remaining_work_units {0};
};

struct ContractorState final
{
    std::uint32_t available_work_units {0};
    std::vector<WorkOrderState> assigned_work_orders {};
};
}  // namespace gs1
