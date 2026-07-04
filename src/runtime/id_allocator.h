#pragma once

#include "shared_framework/runtime/foundation/monotonic_id.h"
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
    shared_framework::runtime::MonotonicIdSource<> next_site_run_id_ {};
    shared_framework::runtime::MonotonicIdSource<> next_task_instance_id_ {};
    shared_framework::runtime::MonotonicIdSource<> next_runtime_action_id_ {};
};
}  // namespace gs1
