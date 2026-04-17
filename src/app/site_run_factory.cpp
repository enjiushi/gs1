#include "app/site_run_factory.h"

#include "content/defs/structure_defs.h"
#include "content/prototype_content.h"
#include "site/inventory_storage.h"
#include "site/site_world.h"

#include <cstdint>
#include <memory>

namespace gs1
{
namespace
{
constexpr std::uint32_t k_site_width = 32U;
constexpr std::uint32_t k_site_height = 32U;

TileCoord starter_storage_tile(TileCoord camp_anchor_tile) noexcept
{
    return TileCoord {camp_anchor_tile.x - 1, camp_anchor_tile.y};
}

TileCoord starter_workbench_tile(TileCoord camp_anchor_tile) noexcept
{
    return TileCoord {camp_anchor_tile.x + 1, camp_anchor_tile.y};
}
}

SiteRunState SiteRunFactory::create_site_run(
    const CampaignState& campaign,
    const SiteMetaState& site_meta)
{
    SiteRunState run {};
    const auto* site_content = find_prototype_site_content(site_meta.site_id);

    run.site_run_id = SiteRunId{site_meta.attempt_count};
    run.site_id = site_meta.site_id;
    run.attempt_index = site_meta.attempt_count;
    run.site_attempt_seed = campaign.campaign_seed + site_meta.site_id.value + site_meta.attempt_count;
    run.run_status = SiteRunStatus::Active;
    run.clock.day_phase = DayPhase::Dawn;
    run.inventory.worker_pack_slot_count = 6U;
    run.inventory.worker_pack_slots.resize(run.inventory.worker_pack_slot_count);
    run.task_board.accepted_task_cap = 3U;

    if (site_content != nullptr)
    {
        run.site_archetype_id = site_content->site_archetype_id;
        run.camp.camp_anchor_tile = site_content->camp_anchor_tile;
        run.camp.starter_storage_tile = starter_storage_tile(site_content->camp_anchor_tile);
        run.counters.site_completion_tile_threshold = site_content->site_completion_tile_threshold;
    }
    else
    {
        run.camp.starter_storage_tile = starter_storage_tile(run.camp.camp_anchor_tile);
    }

    const auto worker_tile_coord = run.camp.camp_anchor_tile;
    const auto worker_tile_x = static_cast<float>(run.camp.camp_anchor_tile.x);
    const auto worker_tile_y = static_cast<float>(run.camp.camp_anchor_tile.y);
    run.site_world = std::make_shared<SiteWorld>();
    run.site_world->initialize(
        SiteWorld::CreateDesc {
            k_site_width,
            k_site_height,
            worker_tile_coord,
            worker_tile_x,
            worker_tile_y,
            0.0f,
            100.0f,
            100.0f,
            100.0f,
            100.0f,
            100.0f,
            100.0f,
            1.0f,
            false});
    if (run.site_world->contains(run.camp.starter_storage_tile))
    {
        auto tile = run.site_world->tile_at(run.camp.starter_storage_tile);
        tile.device.structure_id = StructureId {k_structure_storage_crate};
        tile.device.device_integrity = 1.0f;
        run.site_world->set_tile(run.camp.starter_storage_tile, tile);
    }
    if (run.site_id.value == 1U)
    {
        const auto workbench_tile = starter_workbench_tile(run.camp.camp_anchor_tile);
        if (run.site_world->contains(workbench_tile))
        {
            auto tile = run.site_world->tile_at(workbench_tile);
            tile.device.structure_id = StructureId {k_structure_workbench};
            tile.device.device_integrity = 1.0f;
            run.site_world->set_tile(workbench_tile, tile);
        }
    }
    run.pending_tile_projection_update_mask.assign(run.site_world->tile_count(), 0U);
    inventory_storage::initialize_site_inventory_storage(run);

    return run;
}
}  // namespace gs1
