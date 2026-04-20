#pragma once

#include "content/defs/item_defs.h"
#include "support/id_types.h"

#include <array>
#include <cstdint>

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

inline constexpr std::array<RewardCandidateDef, 7> k_prototype_reward_candidate_defs {{
    RewardCandidateDef {
        RewardCandidateId {k_reward_candidate_site1_money_stipend},
        RewardEffectKind::Money,
        12,
        ItemId {},
        0U,
        0U,
        ModifierId {},
        FactionId {},
        0U,
        0},
    RewardCandidateDef {
        RewardCandidateId {k_reward_candidate_site1_food_delivery},
        RewardEffectKind::ItemDelivery,
        0,
        ItemId {k_item_food_pack},
        1U,
        0U,
        ModifierId {},
        FactionId {},
        10U,
        0},
    RewardCandidateDef {
        RewardCandidateId {k_reward_candidate_site1_unlockable_reveal},
        RewardEffectKind::RevealUnlockable,
        0,
        ItemId {},
        0U,
        102U,
        ModifierId {},
        FactionId {},
        0U,
        0},
    RewardCandidateDef {
        RewardCandidateId {k_reward_candidate_site1_run_modifier},
        RewardEffectKind::RunModifier,
        0,
        ItemId {},
        0U,
        0U,
        ModifierId {4U},
        FactionId {},
        0U,
        0},
    RewardCandidateDef {
        RewardCandidateId {k_reward_candidate_site1_water_delivery},
        RewardEffectKind::ItemDelivery,
        0,
        ItemId {k_item_water_container},
        1U,
        0U,
        ModifierId {},
        FactionId {},
        10U,
        0},
    RewardCandidateDef {
        RewardCandidateId {k_reward_candidate_site1_second_unlockable_reveal},
        RewardEffectKind::RevealUnlockable,
        0,
        ItemId {},
        0U,
        103U,
        ModifierId {},
        FactionId {},
        0U,
        0},
    RewardCandidateDef {
        RewardCandidateId {k_reward_candidate_site1_second_run_modifier},
        RewardEffectKind::RunModifier,
        0,
        ItemId {},
        0U,
        0U,
        ModifierId {5U},
        FactionId {},
        0U,
        0},
}};

[[nodiscard]] inline constexpr const RewardCandidateDef* find_reward_candidate_def(
    RewardCandidateId reward_candidate_id) noexcept
{
    for (const auto& reward_candidate_def : k_prototype_reward_candidate_defs)
    {
        if (reward_candidate_def.reward_candidate_id == reward_candidate_id)
        {
            return &reward_candidate_def;
        }
    }

    return nullptr;
}
}  // namespace gs1
