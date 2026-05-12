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
    using type = type_list<
        RuntimeCampaignTag,
        RuntimeActiveSiteRunTag,
        RuntimeFixedStepSecondsTag,
        RuntimeMoveDirectionTag>;
};

namespace
{
struct CraftContext final
{
    const CampaignState& campaign;
    SiteRunState& site_run;
    SiteWorldAccess<CraftSystem> world;
};

bool same_tile_coord(TileCoord lhs, TileCoord rhs) noexcept
{
    return lhs.x == rhs.x && lhs.y == rhs.y;
}

void refresh_phone_cache(
    CraftContext& context,
    std::uint64_t membership_revision)
{
    auto& phone_cache = context.world.own_craft().phone_cache;
    phone_cache.source_membership_revision = membership_revision;
    phone_cache.item_instance_ids =
        inventory_storage::collect_item_instance_ids_in_containers(
            context.site_run,
            inventory_storage::collect_all_storage_containers(context.site_run, true));
}

void refresh_device_caches(
    CraftContext& context,
    std::uint64_t membership_revision,
    TileCoord worker_tile)
{
    auto& craft = context.world.own_craft();
    craft.device_caches.clear();

    auto& ecs_world = context.site_run.site_world->ecs_world();
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
                craft_logic::worker_pack_included_for_device(context.site_run, coord),
                craft_logic::collect_nearby_item_instance_ids(context.site_run, coord)});
        });

    craft.device_cache_source_membership_revision = membership_revision;
    craft.device_cache_worker_tile = worker_tile;
    craft.device_caches_dirty = false;
}

Gs1Status handle_craft_context_requested(
    CraftContext& context,
    const CraftContextRequestedMessage& payload) noexcept
{
    auto& presentation = context.world.own_craft().context_presentation;
    presentation = CraftContextPresentationState {};

    if (!context.world.has_world() || context.site_run.site_world == nullptr)
    {
        context.world.mark_projection_dirty(SITE_PROJECTION_UPDATE_CRAFT_CONTEXT);
        return GS1_STATUS_OK;
    }

    const TileCoord target_tile {payload.tile_x, payload.tile_y};
    if (!context.world.tile_coord_in_bounds(target_tile))
    {
        context.world.mark_projection_dirty(SITE_PROJECTION_UPDATE_CRAFT_CONTEXT);
        return GS1_STATUS_OK;
    }

    const auto tile = context.world.read_tile(target_tile);
    const auto recipes = craft_logic::recipes_for_station(context.campaign, tile.device.structure_id);
    presentation.occupied = true;
    presentation.tile_coord = target_tile;
    if (recipes.empty())
    {
        context.world.mark_projection_dirty(SITE_PROJECTION_UPDATE_CRAFT_CONTEXT);
        return GS1_STATUS_OK;
    }

    const auto device_entity_id = context.site_run.site_world->device_entity_id(target_tile);
    const auto output_container =
        inventory_storage::find_device_storage_container(context.site_run, device_entity_id);
    if (device_entity_id == 0U || !output_container.is_valid())
    {
        context.world.mark_projection_dirty(SITE_PROJECTION_UPDATE_CRAFT_CONTEXT);
        return GS1_STATUS_OK;
    }

    const auto nearby_items =
        craft_logic::nearby_item_instance_ids_for_device(context.site_run, context.world.read_craft(), device_entity_id, target_tile);
    const auto source_containers =
        craft_logic::collect_nearby_source_containers(context.site_run, target_tile);
    for (const auto* recipe_def : recipes)
    {
        if (recipe_def == nullptr)
        {
            continue;
        }

        if (!craft_logic::can_satisfy_recipe_requirements(
                context.site_run,
                nearby_items,
                *recipe_def))
        {
            continue;
        }

        if (!craft_logic::can_store_output_after_recipe_consumption(
                context.site_run,
                output_container,
                source_containers,
                *recipe_def))
        {
            continue;
        }

        presentation.options.push_back(CraftContextOptionState {
            recipe_def->recipe_id.value,
            recipe_def->output_item_id.value});
    }

    context.world.mark_projection_dirty(SITE_PROJECTION_UPDATE_CRAFT_CONTEXT);
    return GS1_STATUS_OK;
}
}  // namespace

bool CraftSystem::subscribes_to_host_message(Gs1HostMessageType type) noexcept
{
    return type == GS1_HOST_EVENT_SITE_CONTEXT_REQUEST;
}

bool CraftSystem::subscribes_to(GameMessageType type) noexcept
{
    switch (type)
    {
    case GameMessageType::SiteRunStarted:
    case GameMessageType::SiteDevicePlaced:
    case GameMessageType::SiteDeviceBroken:
    case GameMessageType::InventoryCraftContextRequested:
        return true;
    default:
        return false;
    }
}

const char* CraftSystem::name() const noexcept
{
    return "CraftSystem";
}

GameMessageSubscriptionSpan CraftSystem::subscribed_game_messages() const noexcept
{
    return runtime_subscription_list<GameMessageType, k_game_message_type_count, &CraftSystem::subscribes_to>();
}

HostMessageSubscriptionSpan CraftSystem::subscribed_host_messages() const noexcept
{
    return runtime_subscription_list<
        Gs1HostMessageType,
        k_runtime_host_message_type_count,
        &CraftSystem::subscribes_to_host_message>();
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
    auto access = make_game_state_access<CraftSystem>(invocation);
    auto& campaign = access.template read<RuntimeCampaignTag>();
    auto& site_run = access.template read<RuntimeActiveSiteRunTag>();
    if (!campaign.has_value() || !site_run.has_value())
    {
        return GS1_STATUS_INVALID_STATE;
    }

    CraftContext context {
        *campaign,
        *site_run,
        SiteWorldAccess<CraftSystem> {*site_run}};

    switch (message.type)
    {
    case GameMessageType::SiteRunStarted:
        context.world.own_craft() = CraftState {};
        return GS1_STATUS_OK;

    case GameMessageType::SiteDevicePlaced:
    case GameMessageType::SiteDeviceBroken:
        context.world.own_craft().device_caches_dirty = true;
        return GS1_STATUS_OK;

    case GameMessageType::InventoryCraftContextRequested:
        return handle_craft_context_requested(
            context,
            message.payload_as<CraftContextRequestedMessage>());

    default:
        return GS1_STATUS_OK;
    }
}

Gs1Status CraftSystem::process_host_message(
    RuntimeInvocation& invocation,
    const Gs1HostMessage& message)
{
    auto access = make_game_state_access<CraftSystem>(invocation);
    auto& campaign = access.template read<RuntimeCampaignTag>();
    auto& site_run = access.template read<RuntimeActiveSiteRunTag>();
    if (!campaign.has_value() || !site_run.has_value())
    {
        return GS1_STATUS_INVALID_STATE;
    }

    CraftContext context {
        *campaign,
        *site_run,
        SiteWorldAccess<CraftSystem> {*site_run}};

    if (message.type != GS1_HOST_EVENT_SITE_CONTEXT_REQUEST ||
        context.site_run.site_action.placement_mode.active)
    {
        return GS1_STATUS_OK;
    }

    return handle_craft_context_requested(
        context,
        CraftContextRequestedMessage {
            message.payload.site_context_request.tile_x,
            message.payload.site_context_request.tile_y,
            message.payload.site_context_request.flags});
}

void CraftSystem::run(RuntimeInvocation& invocation)
{
    auto access = make_game_state_access<CraftSystem>(invocation);
    auto& campaign = access.template read<RuntimeCampaignTag>();
    auto& site_run = access.template read<RuntimeActiveSiteRunTag>();
    if (!campaign.has_value() || !site_run.has_value())
    {
        return;
    }

    CraftContext context {
        *campaign,
        *site_run,
        SiteWorldAccess<CraftSystem> {*site_run}};

    if (!context.world.has_world() || context.site_run.site_world == nullptr)
    {
        return;
    }

    const auto membership_revision = context.world.read_inventory().item_membership_revision;
    auto& craft = context.world.own_craft();
    if (craft.phone_cache.source_membership_revision != membership_revision)
    {
        refresh_phone_cache(context, membership_revision);
    }

    const TileCoord worker_tile = site_world_access::worker_position(context.site_run).tile_coord;
    if (!craft.device_caches_dirty &&
        craft.device_cache_source_membership_revision == membership_revision &&
        same_tile_coord(craft.device_cache_worker_tile, worker_tile))
    {
        return;
    }

    refresh_device_caches(context, membership_revision, worker_tile);
}
}  // namespace gs1

#ifdef _MSC_VER
#pragma warning(pop)
#endif

