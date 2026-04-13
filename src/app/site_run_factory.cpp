#include "app/site_run_factory.h"

#include "content/prototype_content.h"

namespace gs1
{
namespace
{
void initialize_tile_grid(TileGridState& grid, std::uint32_t width, std::uint32_t height)
{
    grid.width = width;
    grid.height = height;

    const auto tile_count = grid.tile_count();

    grid.terrain_type_ids.assign(tile_count, 0U);
    grid.tile_traversable.assign(tile_count, 1U);
    grid.tile_plantable.assign(tile_count, 1U);
    grid.tile_reserved_by_structure.assign(tile_count, 0U);

    grid.tile_soil_fertility.assign(tile_count, 0.5f);
    grid.tile_moisture.assign(tile_count, 0.5f);
    grid.tile_soil_salinity.assign(tile_count, 0.0f);
    grid.tile_sand_burial.assign(tile_count, 0.0f);

    grid.tile_heat.assign(tile_count, 0.0f);
    grid.tile_wind.assign(tile_count, 0.0f);
    grid.tile_dust.assign(tile_count, 0.0f);

    grid.plant_type_ids.assign(tile_count, PlantId{});
    grid.ground_cover_type_ids.assign(tile_count, 0U);
    grid.tile_plant_density.assign(tile_count, 0.0f);
    grid.growth_pressure.assign(tile_count, 0.0f);

    grid.structure_type_ids.assign(tile_count, StructureId{});
    grid.device_integrity.assign(tile_count, 1.0f);
    grid.device_efficiency.assign(tile_count, 1.0f);
    grid.device_stored_water.assign(tile_count, 0.0f);
}
}  // namespace

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
    run.worker.player_health = 100.0f;
    run.worker.player_hydration = 100.0f;
    run.worker.player_nourishment = 100.0f;
    run.worker.player_energy_cap = 100.0f;
    run.worker.player_energy = 100.0f;
    run.worker.player_morale = 100.0f;
    run.worker.player_work_efficiency = 1.0f;
    run.inventory.worker_pack_slot_count = 8U;
    run.inventory.camp_storage_slot_count = 24U;
    run.inventory.worker_pack_slots.resize(run.inventory.worker_pack_slot_count);
    run.inventory.camp_storage_slots.resize(run.inventory.camp_storage_slot_count);
    run.task_board.accepted_task_cap = 3U;

    initialize_tile_grid(run.tile_grid, 12U, 12U);

    if (site_content != nullptr)
    {
        run.site_archetype_id = site_content->site_archetype_id;
        run.camp.camp_anchor_tile = site_content->camp_anchor_tile;
        run.counters.site_completion_tile_threshold = site_content->site_completion_tile_threshold;
    }

    run.worker.tile_coord = run.camp.camp_anchor_tile;
    run.worker.tile_position_x = static_cast<float>(run.camp.camp_anchor_tile.x);
    run.worker.tile_position_y = static_cast<float>(run.camp.camp_anchor_tile.y);

    return run;
}
}  // namespace gs1
