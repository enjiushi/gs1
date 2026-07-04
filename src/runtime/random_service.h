#pragma once

#include "shared_framework/runtime/foundation/seeded_random_streams.h"

#include <cstdint>
#include <random>

namespace gs1
{
struct RandomService final
{
    shared_framework::runtime::SeededRandomStreams<4> streams {};

    [[nodiscard]] std::mt19937_64& task_rng() noexcept { return streams.stream(0U); }
    [[nodiscard]] std::mt19937_64& reward_rng() noexcept { return streams.stream(1U); }
    [[nodiscard]] std::mt19937_64& event_rng() noexcept { return streams.stream(2U); }
    [[nodiscard]] std::mt19937_64& minor_variation_rng() noexcept { return streams.stream(3U); }

    [[nodiscard]] const std::mt19937_64& task_rng() const noexcept { return streams.stream(0U); }
    [[nodiscard]] const std::mt19937_64& reward_rng() const noexcept { return streams.stream(1U); }
    [[nodiscard]] const std::mt19937_64& event_rng() const noexcept { return streams.stream(2U); }
    [[nodiscard]] const std::mt19937_64& minor_variation_rng() const noexcept { return streams.stream(3U); }

    void reseed(std::uint64_t base_seed)
    {
        streams.reseed(base_seed);
    }
};
}  // namespace gs1
