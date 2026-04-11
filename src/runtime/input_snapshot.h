#pragma once

#include "gs1/types.h"

namespace gs1
{
struct InputSnapshot final
{
    Gs1InputSnapshot current {};
    Gs1InputSnapshot previous {};
    bool has_previous {false};
};
}  // namespace gs1
