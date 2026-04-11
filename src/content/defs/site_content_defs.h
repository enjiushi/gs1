#pragma once

#include "support/id_types.h"

namespace gs1
{
struct SiteContentDef final
{
    SiteId site_id {};
    std::uint32_t site_archetype_id {0};
    std::uint32_t width {0};
    std::uint32_t height {0};
};
}  // namespace gs1
