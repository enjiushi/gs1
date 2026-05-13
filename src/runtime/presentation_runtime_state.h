#pragma once

#include "site/phone_panel_state.h"

#include <cstdint>
#include <vector>

namespace gs1
{
struct CampaignUnlockPresentationSnapshot final
{
    std::vector<std::uint32_t> unlocked_reputation_unlock_ids {};
    std::vector<std::uint32_t> unlocked_technology_node_ids {};
};

struct PhonePanelPresentationState final
{
    bool open {false};
    bool notification_state_initialized {false};
    PhonePanelSection active_section {PhonePanelSection::Home};
    std::uint32_t badge_flags {0U};
    std::uint64_t task_snapshot_signature {0U};
    std::uint64_t buy_snapshot_signature {0U};
    std::uint64_t sell_snapshot_signature {0U};
    std::uint64_t service_snapshot_signature {0U};
};

struct PresentationRuntimeState final
{
    PhonePanelPresentationState phone_panel {};
    std::vector<std::uint32_t> last_emitted_phone_listing_ids {};
    CampaignUnlockPresentationSnapshot campaign_unlock_snapshot {};
    std::uint64_t presentation_dirty_revision {0U};
};
}  // namespace gs1
