#pragma once

#include <cstdint>

namespace gs1
{
struct RegionalMapViewModel final
{
    std::uint32_t revealed_site_count {0};
    std::uint32_t available_site_count {0};
    std::uint32_t completed_site_count {0};
    std::uint32_t selected_site_id {0};
    bool has_selected_site {false};
};
}  // namespace gs1
