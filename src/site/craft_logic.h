#pragma once

#include "campaign/campaign_state.h"
#include "content/defs/craft_recipe_defs.h"
#include "content/defs/structure_defs.h"
#include "site/craft_state.h"
#include "site/site_run_state.h"

#include <flecs.h>

#include <cstdint>
#include <vector>

namespace gs1::craft_logic
{
inline constexpr int k_craft_cache_radius_tiles = 5;

bool tile_in_craft_range(TileCoord source, TileCoord target) noexcept;
bool worker_pack_included_for_device(TileCoord worker_tile, TileCoord device_tile) noexcept;
bool worker_pack_included_for_device(const SiteRunState& site_run, TileCoord device_tile) noexcept;

std::vector<flecs::entity> collect_nearby_source_containers(
    const InventoryState& inventory,
    const flecs::world* world,
    TileCoord worker_tile,
    TileCoord device_tile);

std::vector<flecs::entity> collect_nearby_source_containers(
    SiteRunState& site_run,
    TileCoord device_tile);

std::vector<std::uint64_t> collect_nearby_item_instance_ids(
    const InventoryState& inventory,
    const flecs::world* world,
    TileCoord worker_tile,
    TileCoord device_tile);

std::vector<std::uint64_t> collect_nearby_item_instance_ids(
    SiteRunState& site_run,
    TileCoord device_tile);

CraftDeviceCacheState* find_device_cache(
    CraftState& craft_state,
    std::uint64_t device_entity_id) noexcept;

const CraftDeviceCacheState* find_device_cache(
    const CraftState& craft_state,
    std::uint64_t device_entity_id) noexcept;

std::vector<std::uint64_t> nearby_item_instance_ids_for_device(
    const InventoryState& inventory,
    flecs::world* world,
    TileCoord worker_tile,
    const CraftState& craft_state,
    std::uint64_t device_entity_id,
    TileCoord device_tile);

std::vector<std::uint64_t> nearby_item_instance_ids_for_device(
    SiteRunState& site_run,
    const CraftState& craft_state,
    std::uint64_t device_entity_id,
    TileCoord device_tile);

std::uint32_t available_cached_item_quantity(
    const InventoryState& inventory,
    flecs::world* world,
    const std::vector<std::uint64_t>& item_instance_ids,
    ItemId item_id) noexcept;

std::uint32_t available_cached_item_quantity(
    SiteRunState& site_run,
    const std::vector<std::uint64_t>& item_instance_ids,
    ItemId item_id) noexcept;

bool recipe_requires_hammer(const CraftRecipeDef& recipe_def) noexcept;

bool can_satisfy_recipe_ingredients(
    const InventoryState& inventory,
    flecs::world* world,
    const std::vector<std::uint64_t>& item_instance_ids,
    const CraftRecipeDef& recipe_def) noexcept;

bool can_satisfy_recipe_ingredients(
    SiteRunState& site_run,
    const std::vector<std::uint64_t>& item_instance_ids,
    const CraftRecipeDef& recipe_def) noexcept;

bool can_satisfy_recipe_requirements(
    const InventoryState& inventory,
    flecs::world* world,
    const std::vector<std::uint64_t>& item_instance_ids,
    const CraftRecipeDef& recipe_def) noexcept;

bool can_satisfy_recipe_requirements(
    SiteRunState& site_run,
    const std::vector<std::uint64_t>& item_instance_ids,
    const CraftRecipeDef& recipe_def) noexcept;

bool can_store_output_after_recipe_consumption(
    const InventoryState& inventory,
    flecs::world* world,
    flecs::entity output_container,
    const std::vector<flecs::entity>& source_containers,
    const CraftRecipeDef& recipe_def);

bool can_store_output_after_recipe_consumption(
    SiteRunState& site_run,
    flecs::entity output_container,
    const std::vector<flecs::entity>& source_containers,
    const CraftRecipeDef& recipe_def);

std::vector<const CraftRecipeDef*> recipes_for_station(
    const CampaignState& campaign,
    StructureId structure_id);

std::vector<const CraftRecipeDef*> recipes_for_station(
    std::span<const FactionProgressState> faction_progress,
    const TechnologyState& technology,
    StructureId structure_id);
}  // namespace gs1::craft_logic
