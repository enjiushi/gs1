#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <random>

namespace shared_framework::runtime
{
template <std::size_t Count>
struct SeededRandomStreams final
{
    std::array<std::mt19937_64, Count> streams {};

    void reseed(std::uint64_t base_seed) noexcept
    {
        for (std::size_t index = 0; index < Count; ++index)
        {
            streams[index].seed(base_seed + static_cast<std::uint64_t>((index + 1U) * 17U));
        }
    }

    [[nodiscard]] std::mt19937_64& stream(std::size_t index) noexcept
    {
        return streams[index];
    }

    [[nodiscard]] const std::mt19937_64& stream(std::size_t index) const noexcept
    {
        return streams[index];
    }
};
}  // namespace shared_framework::runtime
