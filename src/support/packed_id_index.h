#pragma once

#include <cstddef>
#include <functional>
#include <span>
#include <unordered_map>
#include <vector>
#include <utility>

namespace gs1
{
template <typename Key, typename Index = std::size_t, typename Hash = std::hash<Key>, typename KeyEqual = std::equal_to<Key>>
class PackedIdIndex final
{
public:
    using map_type = std::unordered_map<Key, Index, Hash, KeyEqual>;
    using iterator = typename map_type::iterator;
    using const_iterator = typename map_type::const_iterator;

    [[nodiscard]] std::pair<iterator, bool> emplace(const Key& key, const Index& index)
    {
        return entries_.emplace(key, index);
    }

    [[nodiscard]] iterator find(const Key& key) noexcept
    {
        return entries_.find(key);
    }

    [[nodiscard]] const_iterator find(const Key& key) const noexcept
    {
        return entries_.find(key);
    }

    [[nodiscard]] iterator end() noexcept
    {
        return entries_.end();
    }

    [[nodiscard]] const_iterator end() const noexcept
    {
        return entries_.end();
    }

    [[nodiscard]] bool contains(const Key& key) const noexcept
    {
        return entries_.contains(key);
    }

    [[nodiscard]] Index& at(const Key& key)
    {
        return entries_.at(key);
    }

    [[nodiscard]] const Index& at(const Key& key) const
    {
        return entries_.at(key);
    }

    void clear() noexcept
    {
        entries_.clear();
    }

    void reserve(std::size_t count)
    {
        entries_.reserve(count);
    }

    [[nodiscard]] std::size_t size() const noexcept
    {
        return entries_.size();
    }

    template <typename Value>
    [[nodiscard]] Value* find_value(std::span<Value> values, const Key& key) noexcept
    {
        const auto found = entries_.find(key);
        if (found == entries_.end() || static_cast<std::size_t>(found->second) >= values.size())
        {
            return nullptr;
        }

        return &values[static_cast<std::size_t>(found->second)];
    }

    template <typename Value>
    [[nodiscard]] const Value* find_value(std::span<const Value> values, const Key& key) const noexcept
    {
        const auto found = entries_.find(key);
        if (found == entries_.end() || static_cast<std::size_t>(found->second) >= values.size())
        {
            return nullptr;
        }

        return &values[static_cast<std::size_t>(found->second)];
    }

    template <typename Value>
    [[nodiscard]] Value* find_value(std::vector<Value>& values, const Key& key) noexcept
    {
        return find_value(std::span<Value> {values}, key);
    }

    template <typename Value>
    [[nodiscard]] const Value* find_value(const std::vector<Value>& values, const Key& key) const noexcept
    {
        return find_value(std::span<const Value> {values}, key);
    }

private:
    map_type entries_ {};
};
}  // namespace gs1
