#pragma once

#include "content/defs/item_defs.h"
#include "support/id_types.h"

#include <cstdint>
#include <span>

namespace gs1
{
enum class RewardEffectKind : std::uint32_t
{
    None = 0,
    Money = 1,
    ItemDelivery = 2,
    RevealUnlockable = 3,
    RunModifier = 4,
    CampaignReputation = 5,
    FactionReputation = 6
};

struct RewardCandidateDef final
{
    RewardCandidateId reward_candidate_id {};
    RewardEffectKind effect_kind {RewardEffectKind::None};
    std::int32_t money_delta {0};
    ItemId item_id {};
    std::uint32_t quantity {0};
    std::uint32_t unlockable_id {0};
    ModifierId modifier_id {};
    FactionId faction_id {};
    std::uint16_t delivery_minutes {0};
    std::int32_t reputation_delta {0};
};

inline constexpr std::uint32_t k_reward_candidate_site1_money_stipend = 1U;
inline constexpr std::uint32_t k_reward_candidate_site1_food_delivery = 2U;
inline constexpr std::uint32_t k_reward_candidate_site1_unlockable_reveal = 3U;
inline constexpr std::uint32_t k_reward_candidate_site1_run_modifier = 4U;
inline constexpr std::uint32_t k_reward_candidate_site1_water_delivery = 5U;
inline constexpr std::uint32_t k_reward_candidate_site1_second_unlockable_reveal = 6U;
inline constexpr std::uint32_t k_reward_candidate_site1_second_run_modifier = 7U;

[[nodiscard]] std::span<const RewardCandidateDef> all_reward_candidate_defs() noexcept;
[[nodiscard]] const RewardCandidateDef* find_reward_candidate_def(
    RewardCandidateId reward_candidate_id) noexcept;
}  // namespace gs1
