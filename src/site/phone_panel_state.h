#pragma once

#include "site/economy_state.h"

#include <cstdint>
#include <vector>

namespace gs1
{
enum class PhonePanelSection : std::uint8_t
{
    Home = 0,
    Tasks = 1,
    Buy = 2,
    Sell = 3,
    Hire = 4,
    Cart = 5
};

struct PhonePanelState final
{
    PhonePanelSection active_section {PhonePanelSection::Home};
    std::uint32_t visible_task_count {0U};
    std::uint32_t accepted_task_count {0U};
    std::uint32_t completed_task_count {0U};
    std::uint32_t claimed_task_count {0U};
    std::uint32_t buy_listing_count {0U};
    std::uint32_t sell_listing_count {0U};
    std::uint32_t service_listing_count {0U};
    std::uint32_t cart_item_count {0U};
    std::vector<PhoneListingState> listings {};
};
}  // namespace gs1
