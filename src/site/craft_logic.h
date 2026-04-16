#pragma once

#include "content/defs/craft_recipe_defs.h"
#include "content/defs/structure_defs.h"
#include "site/craft_state.h"
#include "site/inventory_storage.h"
#include "site/site_world_access.h"

#include <algorithm>
#include <cstdlib>
#include <cstdint>
#include <vector>

namespace gs1::craft_logic
{
inline constexpr int k_craft_cache_radius_tiles = 5;

inline bool tile_in_craft_range(TileCoord source, TileCoord target) noexcept
{
    return std::abs(target.x - source.x) <= k_craft_cache_radius_tiles &&
        std::abs(target.y - source.y) <= k_craft_cache_radius_tiles;
}

inline bool worker_pack_included_for_device(const SiteRunState& site_run, TileCoord device_tile) noexcept
{
    const auto worker_tile = site_world_access::worker_position(site_run).tile_coord;
    return tile_in_craft_range(device_tile, worker_tile);
}

inline std::vector<flecs::entity> collect_nearby_source_containers(
    SiteRunState& site_run,
    TileCoord device_tile)
{
    using namespace site_ecs;

    std::vector<flecs::entity> containers {};
    const auto all_containers = inventory_storage::collect_all_storage_containers(site_run, true);
    const bool include_worker_pack = worker_pack_included_for_device(site_run, device_tile);

    for (const auto& container : all_containers)
    {
        if (!container.is_valid() || !container.has<StorageContainerKindComponent>())
        {
            continue;
        }

        const auto kind = container.get<StorageContainerKindComponent>().kind;
        if (kind == StorageContainerKind::WorkerPack)
        {
            if (include_worker_pack)
            {
                containers.push_back(container);
            }
            continue;
        }

        if (!tile_in_craft_range(device_tile, inventory_storage::container_tile_coord(site_run, container)))
        {
            continue;
        }

        containers.push_back(container);
    }

    return containers;
}

inline std::vector<std::uint32_t> collect_nearby_item_instance_ids(
    SiteRunState& site_run,
    TileCoord device_tile)
{
    return inventory_storage::collect_item_instance_ids_in_containers(
        site_run,
        collect_nearby_source_containers(site_run, device_tile));
}

inline CraftDeviceCacheState* find_device_cache(
    CraftState& craft_state,
    std::uint64_t device_entity_id) noexcept
{
    for (auto& cache : craft_state.device_caches)
    {
        if (cache.device_entity_id == device_entity_id)
        {
            return &cache;
        }
    }

    return nullptr;
}

inline const CraftDeviceCacheState* find_device_cache(
    const CraftState& craft_state,
    std::uint64_t device_entity_id) noexcept
{
    for (const auto& cache : craft_state.device_caches)
    {
        if (cache.device_entity_id == device_entity_id)
        {
            return &cache;
        }
    }

    return nullptr;
}

inline std::vector<std::uint32_t> nearby_item_instance_ids_for_device(
    SiteRunState& site_run,
    const CraftState& craft_state,
    std::uint64_t device_entity_id,
    TileCoord device_tile)
{
    const auto* cache = find_device_cache(craft_state, device_entity_id);
    const bool include_worker_pack = worker_pack_included_for_device(site_run, device_tile);
    const auto membership_revision = site_run.inventory.item_membership_revision;
    if (cache != nullptr &&
        cache->source_membership_revision == membership_revision &&
        cache->worker_pack_included == include_worker_pack)
    {
        return cache->nearby_item_instance_ids;
    }

    return collect_nearby_item_instance_ids(site_run, device_tile);
}

inline std::uint32_t available_cached_item_quantity(
    SiteRunState& site_run,
    const std::vector<std::uint32_t>& item_instance_ids,
    ItemId item_id) noexcept
{
    std::uint32_t total = 0U;
    for (const auto item_instance_id : item_instance_ids)
    {
        const auto item = inventory_storage::entity_from_id(site_run, item_instance_id);
        const auto* stack = inventory_storage::stack_data(site_run, item);
        if (stack != nullptr && stack->item_id == item_id)
        {
            total += stack->quantity;
        }
    }

    return total;
}

inline bool can_satisfy_recipe_ingredients(
    SiteRunState& site_run,
    const std::vector<std::uint32_t>& item_instance_ids,
    const CraftRecipeDef& recipe_def) noexcept
{
    for (std::uint8_t index = 0U; index < recipe_def.ingredient_count; ++index)
    {
        const auto& ingredient = recipe_def.ingredients[index];
        if (available_cached_item_quantity(site_run, item_instance_ids, ingredient.item_id) <
            ingredient.quantity)
        {
            return false;
        }
    }

    return true;
}

inline bool can_store_output_after_recipe_consumption(
    SiteRunState& site_run,
    flecs::entity output_container,
    const std::vector<flecs::entity>& source_containers,
    const CraftRecipeDef& recipe_def)
{
    std::vector<InventorySlot> simulated_slots {};
    const auto slot_count = inventory_storage::slot_count_in_container(site_run, output_container);
    simulated_slots.resize(slot_count);
    for (std::uint32_t slot_index = 0U; slot_index < slot_count; ++slot_index)
    {
        const auto slot_entity = inventory_storage::slot_entity(site_run, output_container, slot_index);
        const auto item_entity = inventory_storage::item_entity_for_slot(site_run, slot_entity);
        inventory_storage::fill_projection_slot_from_entities(simulated_slots[slot_index], item_entity);
    }

    for (std::uint8_t ingredient_index = 0U; ingredient_index < recipe_def.ingredient_count; ++ingredient_index)
    {
        const auto& ingredient = recipe_def.ingredients[ingredient_index];
        std::uint32_t remaining = ingredient.quantity;

        for (const auto& container : source_containers)
        {
            if (remaining == 0U)
            {
                break;
            }

            if (container != output_container)
            {
                const auto available =
                    inventory_storage::available_item_quantity_in_container(site_run, container, ingredient.item_id);
                remaining = available >= remaining ? 0U : remaining - available;
                continue;
            }

            for (auto& slot : simulated_slots)
            {
                if (remaining == 0U)
                {
                    break;
                }

                if (!slot.occupied || slot.item_id != ingredient.item_id || slot.item_quantity == 0U)
                {
                    continue;
                }

                const auto removed = std::min<std::uint32_t>(remaining, slot.item_quantity);
                slot.item_quantity -= removed;
                remaining -= removed;
                if (slot.item_quantity == 0U)
                {
                    slot = InventorySlot {};
                }
            }
        }

        if (remaining != 0U)
        {
            return false;
        }
    }

    auto remaining_output = static_cast<std::uint32_t>(recipe_def.output_quantity);
    const auto max_stack = item_stack_size(recipe_def.output_item_id);
    for (auto& slot : simulated_slots)
    {
        if (!slot.occupied ||
            slot.item_id != recipe_def.output_item_id ||
            slot.item_quantity >= max_stack)
        {
            continue;
        }

        const auto free_capacity = max_stack - slot.item_quantity;
        const auto added = std::min<std::uint32_t>(remaining_output, free_capacity);
        slot.item_quantity += added;
        remaining_output -= added;
        if (remaining_output == 0U)
        {
            return true;
        }
    }

    for (const auto& slot : simulated_slots)
    {
        if (slot.occupied)
        {
            continue;
        }

        remaining_output = remaining_output > max_stack ? remaining_output - max_stack : 0U;
        if (remaining_output == 0U)
        {
            return true;
        }
    }

    return remaining_output == 0U;
}

inline std::vector<const CraftRecipeDef*> recipes_for_station(StructureId structure_id)
{
    std::vector<const CraftRecipeDef*> recipes {};
    for (const auto& recipe_def : k_prototype_craft_recipe_defs)
    {
        if (recipe_def.station_structure_id == structure_id)
        {
            recipes.push_back(&recipe_def);
        }
    }

    std::sort(recipes.begin(), recipes.end(), [](const auto* lhs, const auto* rhs) {
        return lhs->output_item_id.value < rhs->output_item_id.value;
    });
    return recipes;
}
}  // namespace gs1::craft_logic
