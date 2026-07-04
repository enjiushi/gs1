#pragma once

#include <concepts>
#include <optional>
#include <utility>

namespace shared_framework::runtime
{
template <typename T>
struct alignas(64) cache_aligned_state_slot final
{
    T value {};

    [[nodiscard]] T& get() noexcept { return value; }
    [[nodiscard]] const T& get() const noexcept { return value; }

    template <typename U>
        requires std::assignable_from<T&, U&&>
    cache_aligned_state_slot& operator=(U&& next_value)
    {
        value = std::forward<U>(next_value);
        return *this;
    }
};

template <typename T>
struct alignas(64) cache_aligned_state_slot<std::optional<T>> final
{
    std::optional<T> value {};

    [[nodiscard]] std::optional<T>& get() noexcept { return value; }
    [[nodiscard]] const std::optional<T>& get() const noexcept { return value; }

    template <typename U>
        requires std::assignable_from<std::optional<T>&, U&&>
    cache_aligned_state_slot& operator=(U&& next_value)
    {
        value = std::forward<U>(next_value);
        return *this;
    }
};

template <typename T>
struct optional_container_state_slot final
{
    std::optional<T> value {};

    [[nodiscard]] std::optional<T>& get() noexcept { return value; }
    [[nodiscard]] const std::optional<T>& get() const noexcept { return value; }

    template <typename U>
        requires std::assignable_from<std::optional<T>&, U&&>
    optional_container_state_slot& operator=(U&& next_value)
    {
        value = std::forward<U>(next_value);
        return *this;
    }
};
}  // namespace shared_framework::runtime
