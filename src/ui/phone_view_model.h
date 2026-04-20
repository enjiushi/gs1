#pragma once

#include <cstdint>

namespace gs1
{
struct PhoneViewModel final
{
    std::uint32_t visible_task_count {0};
    std::uint32_t accepted_task_count {0};
    std::uint32_t completed_task_count {0};
    std::uint32_t claimed_task_count {0};
    std::uint32_t listing_count {0};
};
}  // namespace gs1
