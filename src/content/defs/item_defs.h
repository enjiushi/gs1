#pragma once

#include "support/id_types.h"

namespace gs1
{
struct ItemDef final
{
    ItemId item_id {};
    std::int32_t buy_price {0};
    std::int32_t sell_price {0};
};
}  // namespace gs1
