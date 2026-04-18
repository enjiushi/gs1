#pragma once

#include "app/campaign_factory.h"
#include "app/site_run_factory.h"
#include "campaign/campaign_state.h"
#include "campaign/systems/campaign_system_context.h"
#include "messages/game_message.h"
#include "content/defs/structure_defs.h"
#include "site/inventory_storage.h"
#include "site/site_run_state.h"
#include "site/site_world.h"
#include "site/site_world_components.h"
#include "site/systems/site_system_context.h"

#include <flecs.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace gs1::testing::fixtures
{
inline constexpr std::uint64_t k_default_campaign_seed = 42ULL;
inline constexpr std::uint32_t k_default_campaign_days = 30U;
inline constexpr double k_default_fixed_step_seconds = 1.0;

inline TileCoord default_starter_workbench_tile(TileCoord camp_anchor_tile) noexcept
{
    return TileCoord {camp_anchor_tile.x + 1, camp_anchor_tile.y};
}

inline TileCoord default_starter_storage_tile(TileCoord camp_anchor_tile) noexcept
{
    return TileCoord {camp_anchor_tile.x - 1, camp_anchor_tile.y};
}

inline TileCoord default_delivery_box_tile(
    TileCoord camp_anchor_tile,
    std::uint32_t width,
    std::uint32_t height) noexcept
{
    (void)height;
    const auto max_x = static_cast<std::int32_t>(width == 0U ? 0U : width - 1U);
    return TileCoord {std::min(camp_anchor_tile.x + 10, max_x), camp_anchor_tile.y};
}

inline bool approx_equal(float lhs, float rhs, float epsilon = 0.001f) noexcept
{
    return std::fabs(lhs - rhs) <= epsilon;
}

inline bool approx_equal(double lhs, double rhs, double epsilon = 0.001) noexcept
{
    return std::fabs(lhs - rhs) <= epsilon;
}

inline CampaignState make_campaign(
    std::uint64_t seed = k_default_campaign_seed,
    std::uint32_t days = k_default_campaign_days)
{
    return CampaignFactory::create_prototype_campaign(seed, days);
}

inline SiteMetaState* find_site_meta(CampaignState& campaign, std::uint32_t site_id) noexcept
{
    for (auto& site : campaign.sites)
    {
        if (site.site_id.value == site_id)
        {
            return &site;
        }
    }

    return nullptr;
}

inline const SiteMetaState* find_site_meta(const CampaignState& campaign, std::uint32_t site_id) noexcept
{
    for (const auto& site : campaign.sites)
    {
        if (site.site_id.value == site_id)
        {
            return &site;
        }
    }

    return nullptr;
}

inline SiteRunState make_prototype_site_run(CampaignState& campaign, std::uint32_t site_id = 1U)
{
    auto* site = find_site_meta(campaign, site_id);
    if (site == nullptr)
    {
        return {};
    }

    return SiteRunFactory::create_site_run(campaign, *site);
}

inline SiteRunState make_test_site_run(
    std::uint32_t site_id = 1U,
    std::uint32_t site_run_id = 1U,
    std::uint32_t site_archetype_id = 101U,
    std::uint32_t width = 8U,
    std::uint32_t height = 8U,
    TileCoord camp_anchor_tile = TileCoord {2, 2},
    TileCoord worker_tile = TileCoord {2, 2},
    float worker_health = 100.0f)
{
    SiteRunState site_run {};
    site_run.site_run_id = SiteRunId {site_run_id};
    site_run.site_id = SiteId {site_id};
    site_run.site_archetype_id = site_archetype_id;
    site_run.attempt_index = site_run_id;
    site_run.site_attempt_seed =
        1000ULL + static_cast<std::uint64_t>(site_id) + static_cast<std::uint64_t>(site_run_id);
    site_run.run_status = SiteRunStatus::Active;
    site_run.clock.day_phase = DayPhase::Dawn;
    site_run.camp.camp_anchor_tile = camp_anchor_tile;
    site_run.camp.starter_storage_tile = default_starter_storage_tile(camp_anchor_tile);
    site_run.camp.delivery_box_tile = default_delivery_box_tile(camp_anchor_tile, width, height);
    site_run.inventory.worker_pack_slot_count = 8U;
    site_run.inventory.worker_pack_slots.resize(site_run.inventory.worker_pack_slot_count);
    site_run.task_board.accepted_task_cap = 3U;
    site_run.counters.site_completion_tile_threshold = 3U;

    site_run.site_world = std::make_shared<SiteWorld>();
    site_run.site_world->initialize(
        SiteWorld::CreateDesc {
            width,
            height,
            worker_tile,
            static_cast<float>(worker_tile.x),
            static_cast<float>(worker_tile.y),
            0.0f,
            worker_health,
            100.0f,
            100.0f,
            100.0f,
            100.0f,
            100.0f,
            1.0f,
            false});
    if (site_run.site_world->contains(site_run.camp.starter_storage_tile))
    {
        auto tile = site_run.site_world->tile_at(site_run.camp.starter_storage_tile);
        tile.device.structure_id = gs1::StructureId {gs1::k_structure_storage_crate};
        tile.device.device_integrity = 1.0f;
        site_run.site_world->set_tile(site_run.camp.starter_storage_tile, tile);
    }
    if (site_run.site_world->contains(site_run.camp.delivery_box_tile))
    {
        auto tile = site_run.site_world->tile_at(site_run.camp.delivery_box_tile);
        tile.device.structure_id = gs1::StructureId {gs1::k_structure_storage_crate};
        tile.device.device_integrity = 1.0f;
        site_run.site_world->set_tile(site_run.camp.delivery_box_tile, tile);
    }
    if (site_id == 1U && site_run.site_world->contains(default_starter_workbench_tile(camp_anchor_tile)))
    {
        auto tile = site_run.site_world->tile_at(default_starter_workbench_tile(camp_anchor_tile));
        tile.device.structure_id = gs1::StructureId {gs1::k_structure_workbench};
        tile.device.device_integrity = 1.0f;
        site_run.site_world->set_tile(default_starter_workbench_tile(camp_anchor_tile), tile);
    }
    site_run.pending_tile_projection_update_mask.assign(site_run.site_world->tile_count(), 0U);
    inventory_storage::initialize_site_inventory_storage(site_run);
    return site_run;
}

template <typename SystemTag>
inline SiteSystemContext<SystemTag> make_site_context(
    CampaignState& campaign,
    SiteRunState& site_run,
    GameMessageQueue& message_queue,
    double fixed_step_seconds = k_default_fixed_step_seconds,
    SiteMoveDirectionInput move_direction = {})
{
    return make_site_system_context<SystemTag>(
        campaign,
        site_run,
        message_queue,
        fixed_step_seconds,
        move_direction);
}

inline CampaignSystemContext make_campaign_context(CampaignState& campaign) noexcept
{
    return CampaignSystemContext {campaign};
}

inline flecs::entity tile_entity(SiteRunState& site_run, TileCoord coord)
{
    if (site_run.site_world == nullptr || !site_run.site_world->contains(coord))
    {
        return {};
    }

    return site_run.site_world->ecs_world().entity(site_run.site_world->tile_entity_id(coord));
}

inline flecs::entity device_entity(SiteRunState& site_run, TileCoord coord)
{
    if (site_run.site_world == nullptr || !site_run.site_world->contains(coord))
    {
        return {};
    }

    const auto entity_id = site_run.site_world->device_entity_id(coord);
    return entity_id == 0U ? flecs::entity {} : site_run.site_world->ecs_world().entity(entity_id);
}

inline flecs::entity worker_entity(SiteRunState& site_run)
{
    if (site_run.site_world == nullptr)
    {
        return {};
    }

    const auto entity_id = site_run.site_world->worker_entity_id();
    return entity_id == 0U ? flecs::entity {} : site_run.site_world->ecs_world().entity(entity_id);
}

inline flecs::entity starter_storage_container(SiteRunState& site_run)
{
    return gs1::inventory_storage::starter_storage_container(site_run);
}

template <typename Component, typename Func>
inline bool with_tile_component_mut(SiteRunState& site_run, TileCoord coord, Func&& func)
{
    auto entity = tile_entity(site_run, coord);
    if (!entity)
    {
        return false;
    }

    auto& component = entity.get_mut<Component>();
    func(component);
    entity.modified<Component>();
    return true;
}

template <typename Component, typename Func>
inline bool with_device_component_mut(SiteRunState& site_run, TileCoord coord, Func&& func)
{
    auto entity = device_entity(site_run, coord);
    if (!entity)
    {
        return false;
    }

    auto& component = entity.get_mut<Component>();
    func(component);
    entity.modified<Component>();
    return true;
}

template <typename Component, typename Func>
inline bool with_worker_component_mut(SiteRunState& site_run, Func&& func)
{
    auto entity = worker_entity(site_run);
    if (!entity)
    {
        return false;
    }

    auto& component = entity.get_mut<Component>();
    func(component);
    entity.modified<Component>();
    return true;
}

inline std::size_t count_messages(
    const GameMessageQueue& message_queue,
    GameMessageType type) noexcept
{
    std::size_t count = 0U;
    for (const auto& message : message_queue)
    {
        if (message.type == type)
        {
            count += 1U;
        }
    }

    return count;
}

inline const GameMessage* first_message(
    const GameMessageQueue& message_queue,
    GameMessageType type) noexcept
{
    for (const auto& message : message_queue)
    {
        if (message.type == type)
        {
            return &message;
        }
    }

    return nullptr;
}

template <typename Payload>
inline const Payload* first_message_payload(
    const GameMessageQueue& message_queue,
    GameMessageType type) noexcept
{
    const auto* message = first_message(message_queue, type);
    return message == nullptr ? nullptr : &message->payload_as<Payload>();
}
}  // namespace gs1::testing::fixtures
