#pragma once

#include "content/defs/craft_recipe_defs.h"
#include "content/defs/excavation_defs.h"
#include "content/defs/task_defs.h"
#include "content/defs/technology_defs.h"
#include "support/id_types.h"
#include "support/packed_id_index.h"

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace gs1
{
[[nodiscard]] constexpr std::uint64_t make_craft_recipe_station_output_key(
    StructureId station_structure_id,
    ItemId output_item_id) noexcept
{
    return (static_cast<std::uint64_t>(station_structure_id.value) << 32U) |
        static_cast<std::uint64_t>(output_item_id.value);
}

[[nodiscard]] constexpr std::uint64_t make_site_onboarding_task_seed_key(
    SiteId site_id,
    TaskTemplateId task_template_id) noexcept
{
    return (static_cast<std::uint64_t>(site_id.value) << 32U) |
        static_cast<std::uint64_t>(task_template_id.value);
}

[[nodiscard]] constexpr std::uint64_t make_reputation_unlock_kind_content_key(
    ReputationUnlockKind unlock_kind,
    std::uint32_t content_id) noexcept
{
    return (static_cast<std::uint64_t>(static_cast<std::uint8_t>(unlock_kind)) << 32U) |
        static_cast<std::uint64_t>(content_id);
}

[[nodiscard]] constexpr std::uint64_t make_faction_technology_node_key(
    FactionId faction_id,
    std::uint8_t tier_index) noexcept
{
    return (static_cast<std::uint64_t>(faction_id.value) << 32U) |
        static_cast<std::uint64_t>(tier_index);
}

[[nodiscard]] constexpr std::uint64_t make_technology_granted_content_key(
    TechnologyGrantedContentKind granted_content_kind,
    std::uint32_t granted_content_id) noexcept
{
    return (static_cast<std::uint64_t>(static_cast<std::uint8_t>(granted_content_kind)) << 32U) |
        static_cast<std::uint64_t>(granted_content_id);
}

struct ContentIndex final
{
    PackedIdIndex<std::uint32_t> site_by_id {};
    PackedIdIndex<std::uint32_t> item_by_id {};
    PackedIdIndex<std::uint32_t> plant_by_id {};
    PackedIdIndex<std::uint32_t> structure_by_id {};
    PackedIdIndex<std::uint32_t> craft_recipe_by_id {};
    PackedIdIndex<std::uint64_t> craft_recipe_by_station_output {};
    std::unordered_map<std::uint32_t, std::vector<std::size_t>> craft_recipe_indices_by_output_item {};
    PackedIdIndex<std::uint32_t> excavation_depth_by_value {};
    PackedIdIndex<std::uint32_t> modifier_by_id {};
    PackedIdIndex<std::uint32_t> task_template_by_id {};
    PackedIdIndex<std::uint64_t> site_onboarding_task_seed_by_site_and_template {};
    PackedIdIndex<std::uint32_t> site_onboarding_task_seed_by_task_template {};
    PackedIdIndex<std::uint32_t> reward_candidate_by_id {};
    PackedIdIndex<std::uint32_t> site_action_by_kind {};
    PackedIdIndex<std::uint32_t> technology_tier_by_index {};
    PackedIdIndex<std::uint32_t> reputation_unlock_by_id {};
    PackedIdIndex<std::uint64_t> reputation_unlock_by_kind_and_content {};
    PackedIdIndex<std::uint32_t> technology_node_by_id {};
    PackedIdIndex<std::uint64_t> faction_technology_node_by_key {};
    PackedIdIndex<std::uint64_t> technology_node_by_granted_content {};
};
}  // namespace gs1
