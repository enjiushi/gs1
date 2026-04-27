#pragma once

#include <cstdint>

namespace gs1
{
enum class ExcavationDepth : std::uint8_t
{
    None = 0,
    Rough = 1,
    Careful = 2,
    Thorough = 3
};

enum class ExcavationLootTier : std::uint8_t
{
    None = 0,
    Common = 1,
    Uncommon = 2,
    Rare = 3,
    VeryRare = 4,
    Jackpot = 5
};

[[nodiscard]] inline constexpr ExcavationDepth next_excavation_depth(
    ExcavationDepth depth) noexcept
{
    switch (depth)
    {
    case ExcavationDepth::None:
        return ExcavationDepth::Rough;
    case ExcavationDepth::Rough:
        return ExcavationDepth::Careful;
    case ExcavationDepth::Careful:
        return ExcavationDepth::Thorough;
    case ExcavationDepth::Thorough:
    default:
        return ExcavationDepth::Thorough;
    }
}
}  // namespace gs1
