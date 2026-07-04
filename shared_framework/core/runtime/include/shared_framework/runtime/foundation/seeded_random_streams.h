#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <random>

namespace shared_framework::runtime
{
template <std::size_t StreamCount>
class SeededRandomStreams final
{
public:
    [[nodiscard]] std::mt19937_64& stream(std::size_t index) noexcept
    {
        return streams_[index];
    }

    [[nodiscard]] const std::mt19937_64& stream(std::size_t index) const noexcept
    {
        return streams_[index];
    }

    void reseed(std::uint64_t base_seed) noexcept
    {
        for (std::size_t index = 0; index < StreamCount; ++index)
        {
            streams_[index].seed(base_seed + (11U * static_cast<std::uint64_t>(index + 1U)));
        }
    }

private:
    std::array<std::mt19937_64, StreamCount> streams_ {};
};
}  // namespace shared_framework::runtime
