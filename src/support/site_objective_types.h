#pragma once

#include <cstdint>

namespace gs1
{
enum class SiteObjectiveType : std::uint32_t
{
    DenseRestoration = 0,
    HighwayProtection = 1,
    GreenWallConnection = 2,
    PureSurvival = 3,
    CashTargetSurvival = 4
};

enum class SiteObjectiveTargetEdge : std::uint8_t
{
    North = 0,
    East = 1,
    South = 2,
    West = 3
};
}  // namespace gs1
