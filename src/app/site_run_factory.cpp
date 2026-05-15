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
#include <cmath>
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
constexpr float k_min_seeded_fertility = 18.0f;
constexpr float k_max_seeded_fertility = 88.0f;
constexpr float k_min_starting_patch_fertility = 55.0f;

std::uint64_t splitmix64(std::uint64_t value) noexcept
{
    value += 0x9e3779b97f4a7c15ULL;
    value = (value ^ (value >> 30U)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27U)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31U);
}

float hashed_unit_value(std::uint64_t seed, std::int32_t x, std::int32_t y) noexcept
{
    const std::uint64_t mixed =
        seed ^
        (static_cast<std::uint64_t>(static_cast<std::uint32_t>(x)) * 0x632be59bd9b4e019ULL) ^
        (static_cast<std::uint64_t>(static_cast<std::uint32_t>(y)) * 0x85157af5d3f9d65dULL);
    const std::uint64_t hashed = splitmix64(mixed);
    return static_cast<float>(hashed & 0xffffffULL) / static_cast<float>(0xffffffULL);
}

float smoothstep5(float t) noexcept
{
    return t * t * t * (t * ((t * 6.0f) - 15.0f) + 10.0f);
}

float lerp(float lhs, float rhs, float t) noexcept
{
    return lhs + ((rhs - lhs) * t);
}

float value_noise_2d(std::uint64_t seed, float x, float y) noexcept
{
    const auto x0 = static_cast<std::int32_t>(std::floor(x));
    const auto y0 = static_cast<std::int32_t>(std::floor(y));
    const auto x1 = x0 + 1;
    const auto y1 = y0 + 1;
    const float tx = smoothstep5(x - static_cast<float>(x0));
    const float ty = smoothstep5(y - static_cast<float>(y0));

    const float v00 = hashed_unit_value(seed, x0, y0);
    const float v10 = hashed_unit_value(seed, x1, y0);
    const float v01 = hashed_unit_value(seed, x0, y1);
    const float v11 = hashed_unit_value(seed, x1, y1);

    return lerp(lerp(v00, v10, tx), lerp(v01, v11, tx), ty);
}

float fractal_value_noise_2d(
    std::uint64_t seed,
    float x,
    float y,
    std::uint32_t octave_count,
    float lacunarity,
    float gain) noexcept
{
    float amplitude = 1.0f;
    float frequency = 1.0f;
    float total = 0.0f;
    float weight_sum = 0.0f;
    for (std::uint32_t octave = 0U; octave < octave_count; ++octave)
    {
        total += value_noise_2d(seed + octave, x * frequency, y * frequency) * amplitude;
        weight_sum += amplitude;
        frequency *= lacunarity;
        amplitude *= gain;
    }

    if (weight_sum <= 0.0f)
    {
        return 0.0f;
    }

    return total / weight_sum;
}

float seeded_tile_fertility(
    std::uint64_t site_seed,
    TileCoord coord,
    std::uint32_t width,
    std::uint32_t height,
    TileCoord camp_anchor_tile) noexcept
{
    const float sample_x = static_cast<float>(coord.x);
    const float sample_y = static_cast<float>(coord.y);
    const float macro =
        fractal_value_noise_2d(site_seed + 101U, sample_x * 0.11f, sample_y * 0.11f, 4U, 2.0f, 0.5f);
    const float detail =
        fractal_value_noise_2d(site_seed + 211U, sample_x * 0.24f, sample_y * 0.24f, 3U, 2.15f, 0.55f);
    const float ridge =
        1.0f - std::fabs((value_noise_2d(site_seed + 307U, sample_x * 0.18f, sample_y * 0.18f) * 2.0f) - 1.0f);

    const float width_scale = std::max(1.0f, static_cast<float>(width));
    const float height_scale = std::max(1.0f, static_cast<float>(height));
    const float camp_dx = (sample_x - static_cast<float>(camp_anchor_tile.x)) / width_scale;
    const float camp_dy = (sample_y - static_cast<float>(camp_anchor_tile.y)) / height_scale;
    const float camp_bias = std::clamp(1.0f - std::sqrt((camp_dx * camp_dx) + (camp_dy * camp_dy)) * 1.75f, 0.0f, 1.0f);

    const float fertility_unit = std::clamp(
        (macro * 0.58f) +
            (detail * 0.26f) +
            (ridge * 0.10f) +
            (camp_bias * 0.06f),
        0.0f,
        1.0f);
    return std::clamp(
        lerp(k_min_seeded_fertility, k_max_seeded_fertility, fertility_unit),
        0.0f,
        100.0f);
}

std::uint32_t resolved_worker_pack_slot_count(
    std::span<const FactionProgressState> faction_progress) noexcept
{
    std::uint32_t slot_count = 8U;
    if (TechnologySystem::node_purchased(
            faction_progress,
            TechNodeId {base_technology_node_id(k_village_faction, 4U)}))
    {
        slot_count += 2U;
    }

    if (TechnologySystem::node_purchased(
            faction_progress,
            TechNodeId {base_technology_node_id(k_village_faction, 11U)}))
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

float resolve_initial_objective_progress(
    const PrototypeSiteContent& site_content) noexcept
{
    switch (site_content.objective_type)
    {
    case SiteObjectiveType::HighwayProtection:
        return 1.0f;

    case SiteObjectiveType::CashTargetSurvival:
        if (site_content.objective_target_cash_points <= 0)
        {
            return 0.0f;
        }

        return static_cast<float>(std::clamp(
            static_cast<double>(get_prototype_campaign_content().starting_site_cash) /
                static_cast<double>(site_content.objective_target_cash_points),
            0.0,
            1.0));

    case SiteObjectiveType::DenseRestoration:
    case SiteObjectiveType::GreenWallConnection:
    case SiteObjectiveType::PureSurvival:
    default:
        return 0.0f;
    }
}

void initialize_site_objective(
    SiteRunState& run,
    const PrototypeSiteContent& site_content)
{
    run.objective.type = site_content.objective_type;
    run.objective.time_limit_minutes = site_content.site_time_limit_minutes;
    run.objective.target_cash_points = site_content.objective_target_cash_points;
    run.objective.target_edge = site_content.objective_target_edge;
    run.objective.target_band_width = site_content.objective_target_band_width;
    run.objective.highway_max_average_sand_cover = site_content.highway_max_average_sand_cover;
    run.counters.site_completion_tile_threshold = site_content.site_completion_tile_threshold;
    run.counters.objective_progress_normalized = resolve_initial_objective_progress(site_content);
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

void seed_site_fertility(
    SiteRunState& run,
    const PrototypeSiteContent* site_content)
{
    if (run.site_world == nullptr)
    {
        return;
    }

    const TileCoord camp_anchor_tile =
        site_content != nullptr ? site_content->camp_anchor_tile : run.camp.camp_anchor_tile;
    const auto width = run.site_world->width();
    const auto height = run.site_world->height();
    const auto tile_count = run.site_world->tile_count();
    for (std::size_t index = 0; index < tile_count; ++index)
    {
        const TileCoord coord = run.site_world->tile_coord(index);
        auto tile = run.site_world->tile_at_index(index);
        tile.ecology.soil_fertility =
            seeded_tile_fertility(run.site_attempt_seed, coord, width, height, camp_anchor_tile);
        run.site_world->set_tile_at_index(index, tile);
    }

    if (site_content == nullptr)
    {
        return;
    }

    for (const auto& starting_plant : site_content->starting_plants)
    {
        const auto footprint = resolve_plant_tile_footprint(starting_plant.plant_id);
        for_each_tile_in_footprint(
            starting_plant.anchor_tile,
            footprint,
            [&](TileCoord coord) {
                if (!run.site_world->contains(coord))
                {
                    return;
                }

                auto tile = run.site_world->tile_at(coord);
                tile.ecology.soil_fertility =
                    std::max(tile.ecology.soil_fertility, k_min_starting_patch_fertility);
                run.site_world->set_tile(coord, tile);
            });
    }
}
}

SiteRunState SiteRunFactory::create_site_run(
    std::uint64_t campaign_seed,
    std::span<const FactionProgressState> faction_progress,
    const SiteMetaState& site_meta)
{
    SiteRunState run {};
    const auto* site_content = find_prototype_site_content(site_meta.site_id);

    run.site_run_id = SiteRunId{site_meta.attempt_count};
    run.site_id = site_meta.site_id;
    run.attempt_index = site_meta.attempt_count;
    run.site_attempt_seed = campaign_seed + site_meta.site_id.value + site_meta.attempt_count;
    run.run_status = SiteRunStatus::Active;
    run.clock.day_phase = DayPhase::Dawn;
    run.inventory.worker_pack_slot_count = resolved_worker_pack_slot_count(faction_progress);
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
    auto site_world = std::make_shared<SiteWorld>();
    site_world->initialize(
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
    run.site_world = std::move(site_world);
    seed_site_fertility(run, site_content);
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
        tile.device.fixed_integrity = true;
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
            tile.device.fixed_integrity = false;
            run.site_world->set_tile(workbench_tile, tile);
        }
    }
    inventory_storage::initialize_site_inventory_storage(run);

    return run;
}

SiteRunState SiteRunFactory::create_site_run(
    const CampaignState& campaign,
    const SiteMetaState& site_meta)
{
    return create_site_run(
        campaign.campaign_seed,
        std::span<const FactionProgressState> {campaign.faction_progress},
        site_meta);
}
}  // namespace gs1
