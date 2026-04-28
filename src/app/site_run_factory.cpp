#include "app/site_run_factory.h"

#include "campaign/systems/technology_system.h"
#include "content/defs/plant_defs.h"
#include "content/defs/structure_defs.h"
#include "content/prototype_content.h"
#include "site/inventory_storage.h"
#include "site/site_world.h"
#include "site/tile_footprint.h"
#include "support/site_objective_types.h"

#include <algorithm>
#include <cstdint>
#include <memory>

namespace gs1
{
namespace
{
constexpr std::uint32_t k_site_width = 32U;
constexpr std::uint32_t k_site_height = 32U;
constexpr std::uint32_t k_highway_terrain_type_id = 9001U;
constexpr FactionId k_village_faction {k_faction_village_committee};

std::uint32_t resolved_worker_pack_slot_count(const CampaignState& campaign) noexcept
{
    std::uint32_t slot_count = 8U;
    if (TechnologySystem::node_purchased(
            campaign,
            TechNodeId {base_technology_node_id(k_village_faction, 21U)}))
    {
        slot_count += 2U;
    }

    if (TechnologySystem::node_purchased(
            campaign,
            TechNodeId {base_technology_node_id(k_village_faction, 22U)}))
    {
        slot_count += 2U;
    }

    return slot_count;
}

TileCoord starter_workbench_tile(TileCoord camp_anchor_tile) noexcept
{
    return TileCoord {camp_anchor_tile.x + 1, camp_anchor_tile.y};
}

TileCoord delivery_box_tile(TileCoord camp_anchor_tile) noexcept
{
    return TileCoord {camp_anchor_tile.x - 1, camp_anchor_tile.y};
}

bool is_in_target_edge_band(
    TileCoord coord,
    std::uint32_t width,
    std::uint32_t height,
    SiteObjectiveTargetEdge edge,
    std::uint8_t band_width) noexcept
{
    const auto x = static_cast<std::uint32_t>(coord.x);
    const auto y = static_cast<std::uint32_t>(coord.y);
    const auto thickness = std::max<std::uint32_t>(1U, band_width);
    switch (edge)
    {
    case SiteObjectiveTargetEdge::North:
        return y < thickness;
    case SiteObjectiveTargetEdge::East:
        return x >= width - std::min(width, thickness);
    case SiteObjectiveTargetEdge::South:
        return y >= height - std::min(height, thickness);
    case SiteObjectiveTargetEdge::West:
        return x < thickness;
    default:
        return false;
    }
}

void initialize_site_objective(
    SiteRunState& run,
    const PrototypeSiteContent& site_content)
{
    run.objective.type = site_content.objective_type;
    run.objective.time_limit_minutes = site_content.site_time_limit_minutes;
    run.objective.target_edge = site_content.objective_target_edge;
    run.objective.target_band_width = site_content.objective_target_band_width;
    run.objective.highway_max_average_sand_cover = site_content.highway_max_average_sand_cover;
    run.counters.site_completion_tile_threshold = site_content.site_completion_tile_threshold;
    run.counters.objective_progress_normalized =
        run.objective.type == SiteObjectiveType::HighwayProtection ? 1.0f : 0.0f;
    run.counters.highway_average_sand_cover = 0.0f;
}

void apply_highway_objective_layout(SiteRunState& run)
{
    if (run.site_world == nullptr ||
        run.objective.type != SiteObjectiveType::HighwayProtection)
    {
        return;
    }

    const auto width = run.site_world->width();
    const auto height = run.site_world->height();
    const auto tile_count = run.site_world->tile_count();
    run.objective.target_tile_indices.clear();
    run.objective.target_tile_mask.assign(tile_count, 0U);

    for (std::size_t index = 0; index < tile_count; ++index)
    {
        const auto coord = run.site_world->tile_coord(index);
        if (!is_in_target_edge_band(
                coord,
                width,
                height,
                run.objective.target_edge,
                run.objective.target_band_width))
        {
            continue;
        }

        auto tile = run.site_world->tile_at_index(index);
        tile.static_data.terrain_type_id = k_highway_terrain_type_id;
        tile.static_data.traversable = true;
        tile.static_data.plantable = false;
        tile.ecology.soil_fertility = 0.0f;
        tile.ecology.soil_salinity = 0.0f;
        tile.ecology.sand_burial = 0.0f;
        tile.ecology.plant_id = PlantId {};
        tile.ecology.ground_cover_type_id = 0U;
        tile.ecology.plant_density = 0.0f;
        tile.ecology.growth_pressure = 0.0f;
        run.site_world->set_tile_at_index(index, tile);
        run.objective.target_tile_indices.push_back(static_cast<std::uint32_t>(index));
        run.objective.target_tile_mask[index] = 1U;
    }
}

void seed_starting_plants(
    SiteRunState& run,
    const PrototypeSiteContent& site_content)
{
    if (run.site_world == nullptr)
    {
        return;
    }

    for (const auto& starting_plant : site_content.starting_plants)
    {
        const auto footprint = resolve_plant_tile_footprint(starting_plant.plant_id);
        const float initial_density = std::clamp(starting_plant.initial_density, 0.0f, 100.0f);
        for_each_tile_in_footprint(
            starting_plant.anchor_tile,
            footprint,
            [&](TileCoord coord) {
                if (!run.site_world->contains(coord))
                {
                    return;
                }

                auto tile = run.site_world->tile_at(coord);
                tile.ecology.plant_id = starting_plant.plant_id;
                tile.ecology.ground_cover_type_id = 0U;
                tile.ecology.plant_density = initial_density;
                tile.ecology.growth_pressure = 0.0f;
                run.site_world->set_tile(coord, tile);
            });
    }
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
    run.inventory.worker_pack_slot_count = resolved_worker_pack_slot_count(campaign);
    run.inventory.worker_pack_slots.resize(run.inventory.worker_pack_slot_count);
    run.task_board.accepted_task_cap = 3U;

    if (site_content != nullptr)
    {
        run.site_archetype_id = site_content->site_archetype_id;
        run.camp.camp_anchor_tile = site_content->camp_anchor_tile;
        run.camp.delivery_box_tile = delivery_box_tile(site_content->camp_anchor_tile);
        run.camp.starter_storage_tile = run.camp.delivery_box_tile;
        initialize_site_objective(run, *site_content);
    }
    else
    {
        run.camp.delivery_box_tile = delivery_box_tile(run.camp.camp_anchor_tile);
        run.camp.starter_storage_tile = run.camp.delivery_box_tile;
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
    if (site_content != nullptr)
    {
        seed_starting_plants(run, *site_content);
    }
    apply_highway_objective_layout(run);
    if (run.site_world->contains(run.camp.delivery_box_tile))
    {
        auto tile = run.site_world->tile_at(run.camp.delivery_box_tile);
        tile.device.structure_id = StructureId {k_structure_storage_crate};
        tile.device.device_integrity = 1.0f;
        run.site_world->set_tile(run.camp.delivery_box_tile, tile);
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
    run.last_projected_tile_states.assign(run.site_world->tile_count(), ProjectedSiteTileState {});
    inventory_storage::initialize_site_inventory_storage(run);

    return run;
}
}  // namespace gs1
