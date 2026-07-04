#pragma once

#include <cstdint>
#include <type_traits>

namespace shared_framework::runtime
{
template <typename ValueType = std::uint32_t>
class MonotonicIdSource final
{
    static_assert(std::is_integral_v<ValueType>, "MonotonicIdSource requires an integral value type.");

public:
    constexpr explicit MonotonicIdSource(ValueType initial_value = static_cast<ValueType>(1)) noexcept
        : next_value_(initial_value)
    {
    }

    [[nodiscard]] constexpr ValueType next() noexcept
    {
        return next_value_++;
    }

private:
    ValueType next_value_;
};
}  // namespace shared_framework::runtime
