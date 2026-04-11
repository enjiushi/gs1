#pragma once

#include "support/id_types.h"

namespace gs1
{
class IdAllocator final
{
public:
    [[nodiscard]] SiteRunId next_site_run_id() noexcept
    {
        return SiteRunId{next_site_run_id_++};
    }

    [[nodiscard]] TaskInstanceId next_task_instance_id() noexcept
    {
        return TaskInstanceId{next_task_instance_id_++};
    }

    [[nodiscard]] RuntimeActionId next_runtime_action_id() noexcept
    {
        return RuntimeActionId{next_runtime_action_id_++};
    }

private:
    std::uint32_t next_site_run_id_ {1};
    std::uint32_t next_task_instance_id_ {1};
    std::uint32_t next_runtime_action_id_ {1};
};
}  // namespace gs1
