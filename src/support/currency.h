#pragma once

#include <cstdint>

namespace gs1
{
inline constexpr std::uint32_t k_cash_points_per_cash = 100U;

[[nodiscard]] constexpr bool cash_points_are_cash_aligned(std::uint32_t cash_points) noexcept
{
    return (cash_points % k_cash_points_per_cash) == 0U;
}

[[nodiscard]] constexpr std::int32_t cash_from_cash_points(std::uint32_t cash_points) noexcept
{
    return static_cast<std::int32_t>(cash_points / k_cash_points_per_cash);
}

[[nodiscard]] constexpr std::uint32_t cash_points_from_cash(std::uint32_t cash) noexcept
{
    return cash * k_cash_points_per_cash;
}
}  // namespace gs1
