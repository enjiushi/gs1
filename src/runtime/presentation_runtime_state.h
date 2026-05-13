#pragma once

#include <cstdint>
#include <vector>

namespace gs1
{
struct CampaignUnlockPresentationSnapshot final
{
    std::vector<std::uint32_t> unlocked_reputation_unlock_ids {};
    std::vector<std::uint32_t> unlocked_technology_node_ids {};
};

struct PresentationRuntimeState final
{
    std::vector<std::uint32_t> last_emitted_phone_listing_ids {};
    CampaignUnlockPresentationSnapshot campaign_unlock_snapshot {};
    std::uint64_t presentation_dirty_revision {0U};
};
}  // namespace gs1
