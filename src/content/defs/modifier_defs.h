#pragma once

#include "support/id_types.h"

namespace gs1
{
struct ModifierDef final
{
    ModifierId modifier_id {};
    float magnitude {0.0f};
};
}  // namespace gs1
