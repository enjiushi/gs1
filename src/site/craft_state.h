#pragma once

#include "support/id_types.h"

#include <cstdint>
#include <vector>

namespace gs1
{
struct CraftDeviceCacheState final
{
    std::uint64_t device_entity_id {0U};
    std::uint64_t source_membership_revision {0U};
    bool worker_pack_included {false};
    std::vector<std::uint32_t> nearby_item_instance_ids {};
};

struct PhoneInventoryCacheState final
{
    std::uint64_t source_membership_revision {0U};
    std::vector<std::uint32_t> item_instance_ids {};
};

struct CraftContextOptionState final
{
    std::uint32_t recipe_id {0U};
    std::uint32_t output_item_id {0U};
};

struct CraftContextPresentationState final
{
    bool occupied {false};
    TileCoord tile_coord {};
    std::vector<CraftContextOptionState> options {};
};

struct CraftState final
{
    std::vector<CraftDeviceCacheState> device_caches {};
    PhoneInventoryCacheState phone_cache {};
    CraftContextPresentationState context_presentation {};
};
}  // namespace gs1
