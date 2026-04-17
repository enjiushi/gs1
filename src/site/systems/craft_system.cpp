#include "site/systems/craft_system.h"

#include "content/defs/structure_defs.h"
#include "site/craft_logic.h"

#include <algorithm>
#include <vector>

namespace gs1
{
bool CraftSystem::subscribes_to(GameMessageType type) noexcept
{
    switch (type)
    {
    case GameMessageType::SiteRunStarted:
    case GameMessageType::SiteDevicePlaced:
        return true;
    default:
        return false;
    }
}

Gs1Status CraftSystem::process_message(
    SiteSystemContext<CraftSystem>& context,
    const GameMessage& message)
{
    switch (message.type)
    {
    case GameMessageType::SiteRunStarted:
        context.world.own_craft() = CraftState {};
        return GS1_STATUS_OK;

    case GameMessageType::SiteDevicePlaced:
        context.world.own_craft().device_caches.clear();
        return GS1_STATUS_OK;

    default:
        return GS1_STATUS_OK;
    }
}

void CraftSystem::run(SiteSystemContext<CraftSystem>& context)
{
    if (!context.world.has_world() || context.site_run.site_world == nullptr)
    {
        return;
    }

    auto& craft = context.world.own_craft();
    std::vector<std::uint64_t> active_device_ids {};
    const auto tile_count = context.world.tile_count();

    for (std::size_t index = 0; index < tile_count; ++index)
    {
        const auto coord = context.world.tile_coord(index);
        const auto tile = context.world.read_tile_at_index(index);
        if (!structure_is_crafting_station(tile.device.structure_id))
        {
            continue;
        }

        const auto device_entity_id = context.site_run.site_world->device_entity_id(coord);
        if (device_entity_id == 0U)
        {
            continue;
        }

        active_device_ids.push_back(device_entity_id);
        const auto membership_revision = context.world.read_inventory().item_membership_revision;
        const bool include_worker_pack =
            craft_logic::worker_pack_included_for_device(context.site_run, coord);

        auto* cache = craft_logic::find_device_cache(craft, device_entity_id);
        if (cache == nullptr)
        {
            craft.device_caches.push_back(CraftDeviceCacheState {});
            cache = &craft.device_caches.back();
            cache->device_entity_id = device_entity_id;
        }

        if (cache->source_membership_revision != membership_revision ||
            cache->worker_pack_included != include_worker_pack)
        {
            cache->source_membership_revision = membership_revision;
            cache->worker_pack_included = include_worker_pack;
            cache->nearby_item_instance_ids =
                craft_logic::collect_nearby_item_instance_ids(context.site_run, coord);
        }
    }

    craft.device_caches.erase(
        std::remove_if(
            craft.device_caches.begin(),
            craft.device_caches.end(),
            [&](const CraftDeviceCacheState& cache) {
                return std::find(active_device_ids.begin(), active_device_ids.end(), cache.device_entity_id) ==
                    active_device_ids.end();
            }),
        craft.device_caches.end());

    const auto membership_revision = context.world.read_inventory().item_membership_revision;
    if (craft.phone_cache.source_membership_revision != membership_revision)
    {
        craft.phone_cache.source_membership_revision = membership_revision;
        craft.phone_cache.item_instance_ids =
            inventory_storage::collect_item_instance_ids_in_containers(
                context.site_run,
                inventory_storage::collect_all_storage_containers(context.site_run, true));
    }
}
}  // namespace gs1
