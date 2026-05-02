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
    bool open {false};
    bool notification_state_initialized {false};
    PhonePanelSection active_section {PhonePanelSection::Home};
    std::uint32_t visible_task_count {0U};
    std::uint32_t accepted_task_count {0U};
    std::uint32_t completed_task_count {0U};
    std::uint32_t claimed_task_count {0U};
    std::uint32_t buy_listing_count {0U};
    std::uint32_t sell_listing_count {0U};
    std::uint32_t service_listing_count {0U};
    std::uint32_t cart_item_count {0U};
    std::uint32_t badge_flags {0U};
    std::uint64_t task_snapshot_signature {0U};
    std::uint64_t buy_snapshot_signature {0U};
    std::uint64_t sell_snapshot_signature {0U};
    std::uint64_t service_snapshot_signature {0U};
    std::vector<PhoneListingState> listings {};
};
}  // namespace gs1
