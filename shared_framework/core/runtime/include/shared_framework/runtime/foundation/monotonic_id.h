#pragma once

#include <cstdint>

namespace shared_framework::runtime
{
template <typename Value = std::uint32_t>
class MonotonicIdSource final
{
public:
    [[nodiscard]] Value peek_next() const noexcept
    {
        return next_;
    }

    [[nodiscard]] Value take_next() noexcept
    {
        const Value value = next_;
        ++next_;
        return value;
    }

    void reset(Value next = Value {1}) noexcept
    {
        next_ = next;
    }

private:
    Value next_ {1};
};
}  // namespace shared_framework::runtime
