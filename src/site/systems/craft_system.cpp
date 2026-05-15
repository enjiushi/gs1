#include "site/systems/craft_system.h"

#include "content/defs/structure_defs.h"
#include "runtime/game_runtime.h"
#include "site/craft_logic.h"
#include "site/site_world_access.h"
#include "site/site_world_components.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4127)
#endif
#include <flecs.h>

#include <algorithm>
#include <utility>
#include <vector>

namespace gs1
{
template <>
struct system_state_tags<CraftSystem>
{
    using type = type_list<RuntimeCampaignFactionProgressTag, RuntimeCampaignTechnologyTag>;
};

namespace
{
bool same_tile_coord(TileCoord lhs, TileCoord rhs) noexcept
{
    return lhs.x == rhs.x && lhs.y == rhs.y;
}

void refresh_phone_cache(
    RuntimeInvocation& invocation,
    std::uint64_t membership_revision)
{
    SiteWorldAccess<CraftSystem> world {invocation};
    auto& phone_cache = world.own_craft().phone_cache;
    phone_cache.source_membership_revision = membership_revision;
    phone_cache.item_instance_ids =
        inventory_storage::collect_item_instance_ids_in_containers(
            world.read_inventory(),
            world.ecs_world_ptr(),
            inventory_storage::collect_all_storage_containers(world.read_inventory(), world.ecs_world_ptr(), true));
}

void refresh_device_caches(
    RuntimeInvocation& invocation,
    std::uint64_t membership_revision,
    TileCoord worker_tile)
{
    SiteWorldAccess<CraftSystem> world {invocation};
    auto& craft = world.own_craft();
    craft.device_caches.clear();

    auto* ecs_world_ptr = world.ecs_world_ptr();
    if (ecs_world_ptr == nullptr)
    {
        return;
    }

    auto& ecs_world = *ecs_world_ptr;
    auto source_query =
        ecs_world.query_builder<
            const site_ecs::TileCoordComponent,
            const site_ecs::DeviceStructureId>()
            .with<site_ecs::DeviceTag>()
            .build();

    source_query.each(
        [&](flecs::entity entity,
            const site_ecs::TileCoordComponent& coord_component,
            const site_ecs::DeviceStructureId& structure_component) {
            if (!structure_is_crafting_station(structure_component.structure_id))
            {
                return;
            }

            const TileCoord coord = coord_component.value;
            craft.device_caches.push_back(CraftDeviceCacheState {
                entity.id(),
                membership_revision,
                craft_logic::worker_pack_included_for_device(world.read_worker().position.tile_coord, coord),
                craft_logic::collect_nearby_item_instance_ids(
                    world.read_inventory(),
                    world.ecs_world_ptr(),
                    world.read_worker().position.tile_coord,
                    coord)});
        });

    craft.device_cache_source_membership_revision = membership_revision;
    craft.device_cache_worker_tile = worker_tile;
    craft.device_caches_dirty = false;
}

Gs1Status handle_craft_context_requested(
    RuntimeInvocation& invocation,
    const CraftContextRequestedMessage& payload) noexcept
{
    auto access = make_game_state_access<CraftSystem>(invocation);
    auto& faction_progress = access.template read<RuntimeCampaignFactionProgressTag>();
    auto& technology = access.template read<RuntimeCampaignTechnologyTag>();

    SiteWorldAccess<CraftSystem> world {invocation};
    auto& context = world.own_craft().context;
    context = CraftContextState {};

    if (!world.has_world() || world.ecs_world_ptr() == nullptr)
    {
        return GS1_STATUS_OK;
    }

    const TileCoord target_tile {payload.tile_x, payload.tile_y};
    if (!world.tile_coord_in_bounds(target_tile))
    {
        return GS1_STATUS_OK;
    }

    const auto tile = world.read_tile(target_tile);
    const auto recipes = craft_logic::recipes_for_station(
        std::span<const FactionProgressState> {faction_progress},
        technology,
        tile.device.structure_id);
    context.occupied = true;
    context.tile_coord = target_tile;
    if (recipes.empty())
    {
        return GS1_STATUS_OK;
    }

    const auto device_entity_id = world.device_entity_id(target_tile);
    const auto output_container =
        inventory_storage::find_device_storage_container(
            world.read_inventory(),
            world.ecs_world_ptr(),
            device_entity_id);
    if (device_entity_id == 0U || !output_container.is_valid())
    {
        return GS1_STATUS_OK;
    }

    const auto nearby_items =
        craft_logic::nearby_item_instance_ids_for_device(
            world.read_inventory(),
            world.ecs_world_ptr(),
            world.read_worker().position.tile_coord,
            world.read_craft(),
            device_entity_id,
            target_tile);
    const auto source_containers =
        craft_logic::collect_nearby_source_containers(
            world.read_inventory(),
            world.ecs_world_ptr(),
            world.read_worker().position.tile_coord,
            target_tile);
    for (const auto* recipe_def : recipes)
    {
        if (recipe_def == nullptr)
        {
            continue;
        }

        if (!craft_logic::can_satisfy_recipe_requirements(
                world.read_inventory(),
                world.ecs_world_ptr(),
                nearby_items,
                *recipe_def))
        {
            continue;
        }

        if (!craft_logic::can_store_output_after_recipe_consumption(
                world.read_inventory(),
                world.ecs_world_ptr(),
                output_container,
                source_containers,
                *recipe_def))
        {
            continue;
        }

        context.options.push_back(CraftContextOptionState {
            recipe_def->recipe_id.value,
            recipe_def->output_item_id.value});
    }

    return GS1_STATUS_OK;
}
}  // namespace

const char* CraftSystem::name() const noexcept
{
    return "CraftSystem";
}

GameMessageSubscriptionSpan CraftSystem::subscribed_game_messages() const noexcept
{
    static constexpr GameMessageType subscriptions[] = {
        GameMessageType::SiteRunStarted,
        GameMessageType::SiteDevicePlaced,
        GameMessageType::SiteDeviceBroken,
        GameMessageType::InventoryCraftContextRequested,
    };
    return subscriptions;
}

HostMessageSubscriptionSpan CraftSystem::subscribed_host_messages() const noexcept
{
    static constexpr Gs1HostMessageType subscriptions[] = {GS1_HOST_EVENT_SITE_CONTEXT_REQUEST};
    return subscriptions;
}

std::optional<Gs1RuntimeProfileSystemId> CraftSystem::profile_system_id() const noexcept
{
    return GS1_RUNTIME_PROFILE_SYSTEM_CRAFT;
}

std::optional<std::uint32_t> CraftSystem::fixed_step_order() const noexcept
{
    return 15U;
}

Gs1Status CraftSystem::process_game_message(
    RuntimeInvocation& invocation,
    const GameMessage& message)
{
    SiteWorldAccess<CraftSystem> world {invocation};

    switch (message.type)
    {
    case GameMessageType::SiteRunStarted:
        world.own_craft() = CraftState {};
        return GS1_STATUS_OK;

    case GameMessageType::SiteDevicePlaced:
    case GameMessageType::SiteDeviceBroken:
        world.own_craft().device_caches_dirty = true;
        return GS1_STATUS_OK;

    case GameMessageType::InventoryCraftContextRequested:
        return handle_craft_context_requested(
            invocation,
            message.payload_as<CraftContextRequestedMessage>());

    default:
        return GS1_STATUS_OK;
    }
}

Gs1Status CraftSystem::process_host_message(
    RuntimeInvocation& invocation,
    const Gs1HostMessage& message)
{
    if (message.type != GS1_HOST_EVENT_SITE_CONTEXT_REQUEST ||
        SiteWorldAccess<CraftSystem> {invocation}.read_action().placement_mode.active)
    {
        return GS1_STATUS_OK;
    }

    return handle_craft_context_requested(
        invocation,
        CraftContextRequestedMessage {
            message.payload.site_context_request.tile_x,
            message.payload.site_context_request.tile_y,
            message.payload.site_context_request.flags});
}

void CraftSystem::run(RuntimeInvocation& invocation)
{
    SiteWorldAccess<CraftSystem> world {invocation};
    if (!world.has_world() || world.ecs_world_ptr() == nullptr)
    {
        return;
    }

    const auto membership_revision = world.read_inventory().item_membership_revision;
    auto& craft = world.own_craft();
    if (craft.phone_cache.source_membership_revision != membership_revision)
    {
        refresh_phone_cache(invocation, membership_revision);
    }

    const TileCoord worker_tile = world.read_worker().position.tile_coord;
    if (!craft.device_caches_dirty &&
        craft.device_cache_source_membership_revision == membership_revision &&
        same_tile_coord(craft.device_cache_worker_tile, worker_tile))
    {
        return;
    }

    refresh_device_caches(invocation, membership_revision, worker_tile);
}
}  // namespace gs1

#ifdef _MSC_VER
#pragma warning(pop)
#endif

