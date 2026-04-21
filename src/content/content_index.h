#pragma once

#include "support/id_types.h"

#include <cstddef>
#include <unordered_map>

namespace gs1
{
struct ContentIndex final
{
    std::unordered_map<std::uint32_t, std::size_t> site_by_id {};
    std::unordered_map<std::uint32_t, std::size_t> item_by_id {};
    std::unordered_map<std::uint32_t, std::size_t> plant_by_id {};
    std::unordered_map<std::uint32_t, std::size_t> structure_by_id {};
    std::unordered_map<std::uint32_t, std::size_t> craft_recipe_by_id {};
    std::unordered_map<std::uint32_t, std::size_t> task_template_by_id {};
};
}  // namespace gs1
