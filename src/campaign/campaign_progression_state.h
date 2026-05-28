#pragma once

#include <cstdint>

namespace gs1
{
struct CampaignTokenBalanceState final
{
    std::uint32_t token_kind_id {0};
    std::uint32_t scope_id {0};
    std::int32_t amount {0};
};

inline constexpr std::uint16_t k_max_campaign_token_balance_count = 16U;
inline constexpr std::uint16_t k_max_campaign_progression_threshold_count = 160U;

struct CampaignProgressionState final
{
    std::int32_t total_reputation {0};
    std::uint16_t token_balance_count {0};
    std::uint16_t fired_threshold_count {0};
    CampaignTokenBalanceState token_balances[k_max_campaign_token_balance_count] {};
    std::uint32_t fired_threshold_ids[k_max_campaign_progression_threshold_count] {};
};
}  // namespace gs1
