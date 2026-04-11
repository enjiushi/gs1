#pragma once

#include "support/id_types.h"

namespace gs1
{
struct TaskTemplateDef final
{
    TaskTemplateId task_template_id {};
    std::uint32_t target_amount {0};
};
}  // namespace gs1
