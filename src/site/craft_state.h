#pragma once

#include "support/id_types.h"

#include <cstdint>
#include <vector>

namespace gs1
{
struct CraftDeviceCacheEntryState
{
    std::uint64_t device_entity_id {0U};
    std::uint64_t source_membership_revision {0U};
    bool worker_pack_included {false};
    std::uint32_t nearby_item_offset {0U};
    std::uint32_t nearby_item_count {0U};
};

struct CraftDeviceCacheRuntimeState
{
    std::uint64_t device_cache_source_membership_revision {0U};
    TileCoord device_cache_worker_tile {};
    bool device_caches_dirty {true};
};

struct PhoneInventoryCacheMetaState
{
    std::uint64_t source_membership_revision {0U};
};

struct CraftContextOptionState final
{
    std::uint32_t recipe_id {0U};
    std::uint32_t output_item_id {0U};
};

struct CraftContextMetaState
{
    bool occupied {false};
    TileCoord tile_coord {};
};

struct CraftDeviceCacheState final : CraftDeviceCacheEntryState
{
    std::vector<std::uint64_t> nearby_item_instance_ids {};
};

struct PhoneInventoryCacheState final : PhoneInventoryCacheMetaState
{
    std::vector<std::uint64_t> item_instance_ids {};
};

struct CraftContextState final : CraftContextMetaState
{
    std::vector<CraftContextOptionState> options {};
};

struct CraftState final : CraftDeviceCacheRuntimeState
{
    std::vector<CraftDeviceCacheState> device_caches {};
    PhoneInventoryCacheState phone_cache {};
    CraftContextState context {};
};
}  // namespace gs1
