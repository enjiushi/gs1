#pragma once

#include <cstdint>
#include <random>

namespace gs1
{
struct RandomService final
{
    std::mt19937_64 task_rng {};
    std::mt19937_64 reward_rng {};
    std::mt19937_64 event_rng {};
    std::mt19937_64 minor_variation_rng {};

    void reseed(std::uint64_t base_seed)
    {
        task_rng.seed(base_seed + 11U);
        reward_rng.seed(base_seed + 23U);
        event_rng.seed(base_seed + 37U);
        minor_variation_rng.seed(base_seed + 53U);
    }
};
}  // namespace gs1
