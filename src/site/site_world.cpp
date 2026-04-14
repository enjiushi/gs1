#include "site/site_world.h"

#include "site/site_world_components.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4127)
#endif
#include <flecs.h>

#include <cassert>
#include <cstddef>
#include <utility>
#include <vector>

namespace gs1
{
namespace
{
using namespace site_ecs;

TileCoord coord_from_index(const TileGridState& tile_grid, std::size_t index) noexcept
{
    return TileCoord {
        static_cast<std::int32_t>(index % tile_grid.width),
        static_cast<std::int32_t>(index / tile_grid.width)};
}

template <typename Component>
void register_component(flecs::world& world)
{
    world.component<Component>();
}

void register_site_world_types(flecs::world& world)
{
    register_component<TileTag>(world);
    register_component<WorkerTag>(world);
    register_component<TileIndex>(world);
    register_component<TileCoordComponent>(world);
    register_component<TileTerrain>(world);
    register_component<TileTraversable>(world);
    register_component<TilePlantable>(world);
    register_component<TileReservedByStructure>(world);
    register_component<TileSoilFertility>(world);
    register_component<TileMoisture>(world);
    register_component<TileSoilSalinity>(world);
    register_component<TileSandBurial>(world);
    register_component<TileHeat>(world);
    register_component<TileWind>(world);
    register_component<TileDust>(world);
    register_component<TilePlantSlot>(world);
    register_component<TileGroundCoverSlot>(world);
    register_component<TilePlantDensity>(world);
    register_component<TileGrowthPressure>(world);
    register_component<TileStructureSlot>(world);
    register_component<TileDeviceIntegrity>(world);
    register_component<TileDeviceEfficiency>(world);
    register_component<TileDeviceStoredWater>(world);
    register_component<SiteIdentityResource>(world);
    register_component<TileDomainResource>(world);
    register_component<CampResource>(world);
    register_component<WorkerTilePosition>(world);
    register_component<WorkerFacing>(world);
    register_component<WorkerVitals>(world);
    register_component<WorkerActionReference>(world);
}
}  // namespace

struct SiteWorld::Impl final
{
    flecs::world world {};
    std::uint32_t width {0};
    std::uint32_t height {0};
    std::vector<flecs::entity_t> tile_entities {};
    flecs::entity_t worker_entity {0};
    bool initialized {false};
};

SiteWorld::SiteWorld()
    : impl_(std::make_unique<Impl>())
{
    register_site_world_types(impl_->world);
}

SiteWorld::~SiteWorld() = default;
SiteWorld::SiteWorld(SiteWorld&&) noexcept = default;
SiteWorld& SiteWorld::operator=(SiteWorld&&) noexcept = default;

void SiteWorld::initialize(const CreateDesc& desc, const TileGridState& tile_grid)
{
    impl_ = std::make_unique<Impl>();
    register_site_world_types(impl_->world);

    impl_->width = tile_grid.width;
    impl_->height = tile_grid.height;
    const auto tile_count = tile_grid.tile_count();
    impl_->tile_entities.reserve(tile_count);

    impl_->world.set<SiteIdentityResource>({
        desc.site_id,
        desc.site_run_id,
        desc.site_archetype_id});
    impl_->world.set<TileDomainResource>({
        tile_grid.width,
        tile_grid.height});
    impl_->world.set<CampResource>({
        desc.camp_anchor_tile,
        desc.camp_durability,
        desc.camp_protection_resolved,
        desc.delivery_point_operational,
        desc.shared_camp_storage_access_enabled});

    for (std::size_t index = 0; index < tile_count; ++index)
    {
        const auto coord = coord_from_index(tile_grid, index);
        auto tile_entity = impl_->world.entity();
        tile_entity
            .add<TileTag>()
            .set<TileIndex>({static_cast<std::uint32_t>(index)})
            .set<TileCoordComponent>({coord})
            .set<TileTerrain>({tile_grid.terrain_type_ids[index]})
            .set<TileTraversable>({tile_grid.tile_traversable[index] != 0U})
            .set<TilePlantable>({tile_grid.tile_plantable[index] != 0U})
            .set<TileReservedByStructure>({tile_grid.tile_reserved_by_structure[index] != 0U})
            .set<TileSoilFertility>({tile_grid.tile_soil_fertility[index]})
            .set<TileMoisture>({tile_grid.tile_moisture[index]})
            .set<TileSoilSalinity>({tile_grid.tile_soil_salinity[index]})
            .set<TileSandBurial>({tile_grid.tile_sand_burial[index]})
            .set<TileHeat>({tile_grid.tile_heat[index]})
            .set<TileWind>({tile_grid.tile_wind[index]})
            .set<TileDust>({tile_grid.tile_dust[index]})
            .set<TilePlantSlot>({tile_grid.plant_type_ids[index]})
            .set<TileGroundCoverSlot>({tile_grid.ground_cover_type_ids[index]})
            .set<TilePlantDensity>({tile_grid.tile_plant_density[index]})
            .set<TileGrowthPressure>({tile_grid.growth_pressure[index]})
            .set<TileStructureSlot>({tile_grid.structure_type_ids[index]})
            .set<TileDeviceIntegrity>({tile_grid.device_integrity[index]})
            .set<TileDeviceEfficiency>({tile_grid.device_efficiency[index]})
            .set<TileDeviceStoredWater>({tile_grid.device_stored_water[index]});

        impl_->tile_entities.push_back(tile_entity.id());
    }

    auto worker_entity = impl_->world.entity("Worker");
    worker_entity
        .add<WorkerTag>()
        .set<WorkerTilePosition>({
            desc.worker_tile_coord,
            desc.worker_tile_x,
            desc.worker_tile_y})
        .set<WorkerFacing>({desc.worker_facing_degrees})
        .set<WorkerVitals>({
            desc.worker_health,
            desc.worker_hydration,
            desc.worker_nourishment,
            desc.worker_energy_cap,
            desc.worker_energy,
            desc.worker_morale,
            desc.worker_work_efficiency,
            desc.worker_is_sheltered})
        .set<WorkerActionReference>({
            desc.worker_current_action_id,
            desc.worker_has_current_action_id});

    impl_->worker_entity = worker_entity.id();
    impl_->initialized = true;
}

bool SiteWorld::initialized() const noexcept
{
    return impl_ != nullptr && impl_->initialized;
}

std::uint32_t SiteWorld::width() const noexcept
{
    return impl_ != nullptr ? impl_->width : 0U;
}

std::uint32_t SiteWorld::height() const noexcept
{
    return impl_ != nullptr ? impl_->height : 0U;
}

std::size_t SiteWorld::tile_count() const noexcept
{
    return impl_ != nullptr ? impl_->tile_entities.size() : 0U;
}

bool SiteWorld::contains(TileCoord coord) const noexcept
{
    return impl_ != nullptr &&
        coord.x >= 0 &&
        coord.y >= 0 &&
        static_cast<std::uint32_t>(coord.x) < impl_->width &&
        static_cast<std::uint32_t>(coord.y) < impl_->height;
}

std::uint32_t SiteWorld::tile_index(TileCoord coord) const noexcept
{
    assert(contains(coord));
    return static_cast<std::uint32_t>(
        static_cast<std::uint32_t>(coord.y) * impl_->width +
        static_cast<std::uint32_t>(coord.x));
}

std::uint64_t SiteWorld::tile_entity_id(TileCoord coord) const noexcept
{
    if (!contains(coord))
    {
        return 0U;
    }

    const auto index = tile_index(coord);
    return index < impl_->tile_entities.size() ? impl_->tile_entities[index] : 0U;
}

std::uint64_t SiteWorld::worker_entity_id() const noexcept
{
    return impl_ != nullptr ? impl_->worker_entity : 0U;
}
}  // namespace gs1

#ifdef _MSC_VER
#pragma warning(pop)
#endif
