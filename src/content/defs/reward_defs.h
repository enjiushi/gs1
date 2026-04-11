#pragma once

#include "support/id_types.h"

namespace gs1
{
struct RewardCandidateDef final
{
    RewardCandidateId reward_candidate_id {};
    ItemId item_id {};
    std::uint32_t quantity {0};
};
}  // namespace gs1
