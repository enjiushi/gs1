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

std::size_t tile_count_from_dimensions(std::uint32_t width, std::uint32_t height) noexcept
{
    return static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
}

TileCoord coord_from_index(std::uint32_t width, std::size_t index) noexcept
{
    if (width == 0U)
    {
        return {};
    }

    return TileCoord {
        static_cast<std::int32_t>(index % width),
        static_cast<std::int32_t>(index / width)};
}

template <typename Component>
void register_component(flecs::world& world)
{
    world.component<Component>();
}

void register_site_world_types(flecs::world& world)
{
    register_component<TileTag>(world);
    register_component<TileOccupantTag>(world);
    register_component<ActiveEcologyTag>(world);
    register_component<DirtyEcologyTag>(world);
    register_component<DeviceTag>(world);
    register_component<WorkerTag>(world);
    register_component<StorageContainerTag>(world);
    register_component<StorageItemTag>(world);
    register_component<TileIndex>(world);
    register_component<TileCoordComponent>(world);
    register_component<StorageContainerKindComponent>(world);
    register_component<StorageSlotIndex>(world);
    register_component<StorageItemStack>(world);
    register_component<StorageItemLocation>(world);
    world.component<StorageOwnedByDevice>().add(flecs::Exclusive);
    world.component<StorageAtTile>()
        .add(flecs::Exclusive)
        .add(flecs::Symmetric);
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
    register_component<TilePlantWeatherContribution>(world);
    register_component<TileDeviceWeatherContribution>(world);
    register_component<TilePlantSlot>(world);
    register_component<TileGroundCoverSlot>(world);
    register_component<TilePlantDensity>(world);
    register_component<TileGrowthPressure>(world);
    register_component<DirtyEcologyMask>(world);
    register_component<TileEcologyReportState>(world);
    register_component<DeviceStructureId>(world);
    register_component<DeviceIntegrity>(world);
    register_component<DeviceEfficiency>(world);
    register_component<DeviceStoredWater>(world);
    register_component<WorkerTilePosition>(world);
    register_component<WorkerFacing>(world);
    register_component<WorkerVitals>(world);
}

bool has_tile_occupant(PlantId plant_id, std::uint32_t ground_cover_type_id) noexcept
{
    return plant_id.value != 0U || ground_cover_type_id != 0U;
}

void sync_tile_occupant_tag(
    flecs::entity entity,
    PlantId plant_id,
    std::uint32_t ground_cover_type_id)
{
    if (has_tile_occupant(plant_id, ground_cover_type_id))
    {
        entity.add<TileOccupantTag>();
    }
    else
    {
        entity.remove<TileOccupantTag>();
    }
}

void sync_active_ecology_state(
    flecs::entity entity,
    PlantId plant_id,
    std::uint32_t ground_cover_type_id,
    float plant_density)
{
    if (has_tile_occupant(plant_id, ground_cover_type_id))
    {
        entity.add<ActiveEcologyTag>();
        if (!entity.has<TileEcologyReportState>())
        {
            entity.set<TileEcologyReportState>({plant_density, 0U, {0U, 0U, 0U}});
        }
    }
    else
    {
        entity.remove<ActiveEcologyTag>();
        entity.remove<TileEcologyReportState>();
    }
}

SiteWorld::TileStaticData tile_static_from_entity(flecs::entity entity)
{
    const auto terrain = entity.get<TileTerrain>();
    const auto traversable = entity.get<TileTraversable>();
    const auto plantable = entity.get<TilePlantable>();
    const auto reserved = entity.get<TileReservedByStructure>();

    return SiteWorld::TileStaticData {
        terrain.type_id,
        traversable.value,
        plantable.value,
        reserved.value};
}

SiteWorld::TileEcologyData tile_ecology_from_entity(flecs::entity entity)
{
    const auto fertility = entity.get<TileSoilFertility>();
    const auto moisture = entity.get<TileMoisture>();
    const auto salinity = entity.get<TileSoilSalinity>();
    const auto burial = entity.get<TileSandBurial>();
    const auto plant = entity.get<TilePlantSlot>();
    const auto ground_cover = entity.get<TileGroundCoverSlot>();
    const auto density = entity.get<TilePlantDensity>();
    const auto growth = entity.get<TileGrowthPressure>();

    return SiteWorld::TileEcologyData {
        fertility.value,
        moisture.value,
        salinity.value,
        burial.value,
        plant.plant_id,
        ground_cover.ground_cover_type_id,
        density.value,
        growth.value};
}

SiteWorld::TileLocalWeatherData tile_local_weather_from_entity(flecs::entity entity)
{
    const auto heat = entity.get<TileHeat>();
    const auto wind = entity.get<TileWind>();
    const auto dust = entity.get<TileDust>();

    return SiteWorld::TileLocalWeatherData {
        heat.value,
        wind.value,
        dust.value};
}

SiteWorld::TileWeatherContributionData tile_plant_weather_contribution_from_entity(flecs::entity entity)
{
    const auto contribution = entity.get<TilePlantWeatherContribution>();

    return SiteWorld::TileWeatherContributionData {
        contribution.heat_protection,
        contribution.wind_protection,
        contribution.dust_protection,
        contribution.fertility_improve,
        contribution.salinity_reduction,
        contribution.irrigation};
}

SiteWorld::TileWeatherContributionData tile_device_weather_contribution_from_entity(flecs::entity entity)
{
    const auto contribution = entity.get<TileDeviceWeatherContribution>();

    return SiteWorld::TileWeatherContributionData {
        contribution.heat_protection,
        contribution.wind_protection,
        contribution.dust_protection,
        contribution.fertility_improve,
        contribution.salinity_reduction,
        contribution.irrigation};
}

SiteWorld::TileDeviceData tile_device_from_entity(flecs::entity entity)
{
    const auto structure = entity.get<DeviceStructureId>();
    const auto integrity = entity.get<DeviceIntegrity>();
    const auto efficiency = entity.get<DeviceEfficiency>();
    const auto stored_water = entity.get<DeviceStoredWater>();

    return SiteWorld::TileDeviceData {
        structure.structure_id,
        integrity.value,
        efficiency.value,
        stored_water.value};
}

SiteWorld::TileData tile_data_from_entity(flecs::entity entity)
{
    return SiteWorld::TileData {
        tile_static_from_entity(entity),
        tile_ecology_from_entity(entity),
        tile_local_weather_from_entity(entity),
        tile_plant_weather_contribution_from_entity(entity),
        tile_device_weather_contribution_from_entity(entity),
        SiteWorld::TileDeviceData {StructureId {}, 1.0f, 1.0f, 0.0f}};
}

SiteWorld::TileData default_tile_data() noexcept
{
    return SiteWorld::TileData {
        SiteWorld::TileStaticData {0U, true, true, false},
        SiteWorld::TileEcologyData {20.0f, 20.0f, 0.0f, 0.0f, PlantId {}, 0U, 0.0f, 0.0f},
        SiteWorld::TileLocalWeatherData {0.0f, 0.0f, 0.0f},
        SiteWorld::TileWeatherContributionData {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f},
        SiteWorld::TileWeatherContributionData {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f},
        SiteWorld::TileDeviceData {StructureId {}, 1.0f, 1.0f, 0.0f}};
}

SiteWorld::TileDeviceData default_device_data() noexcept
{
    return SiteWorld::TileDeviceData {StructureId {}, 1.0f, 1.0f, 0.0f};
}

void apply_tile_static_to_entity(flecs::entity entity, const SiteWorld::TileStaticData& data)
{
    entity
        .set<TileTerrain>({data.terrain_type_id})
        .set<TileTraversable>({data.traversable})
        .set<TilePlantable>({data.plantable})
        .set<TileReservedByStructure>({data.reserved_by_structure});
}

void apply_tile_ecology_to_entity(flecs::entity entity, const SiteWorld::TileEcologyData& data)
{
    entity
        .set<TileSoilFertility>({data.soil_fertility})
        .set<TileMoisture>({data.moisture})
        .set<TileSoilSalinity>({data.soil_salinity})
        .set<TileSandBurial>({data.sand_burial})
        .set<TilePlantSlot>({data.plant_id})
        .set<TileGroundCoverSlot>({data.ground_cover_type_id})
        .set<TilePlantDensity>({data.plant_density})
        .set<TileGrowthPressure>({data.growth_pressure});

    sync_tile_occupant_tag(entity, data.plant_id, data.ground_cover_type_id);
    sync_active_ecology_state(entity, data.plant_id, data.ground_cover_type_id, data.plant_density);
}

void apply_tile_local_weather_to_entity(
    flecs::entity entity,
    const SiteWorld::TileLocalWeatherData& data)
{
    entity
        .set<TileHeat>({data.heat})
        .set<TileWind>({data.wind})
        .set<TileDust>({data.dust});
}

void apply_tile_plant_weather_contribution_to_entity(
    flecs::entity entity,
    const SiteWorld::TileWeatherContributionData& data)
{
    entity.set<TilePlantWeatherContribution>({
        data.heat_protection,
        data.wind_protection,
        data.dust_protection,
        data.fertility_improve,
        data.salinity_reduction,
        data.irrigation});
}

void apply_tile_device_weather_contribution_to_entity(
    flecs::entity entity,
    const SiteWorld::TileWeatherContributionData& data)
{
    entity.set<TileDeviceWeatherContribution>({
        data.heat_protection,
        data.wind_protection,
        data.dust_protection,
        data.fertility_improve,
        data.salinity_reduction,
        data.irrigation});
}

void apply_device_to_entity(flecs::entity entity, const SiteWorld::TileDeviceData& data)
{
    entity
        .set<DeviceStructureId>({data.structure_id})
        .set<DeviceIntegrity>({data.device_integrity})
        .set<DeviceEfficiency>({data.device_efficiency})
        .set<DeviceStoredWater>({data.device_stored_water});
}

SiteWorld::WorkerPositionData worker_position_from_entity(flecs::entity entity)
{
    const auto position = entity.get<WorkerTilePosition>();
    const auto facing = entity.get<WorkerFacing>();

    return SiteWorld::WorkerPositionData {
        position.tile_coord,
        position.tile_x,
        position.tile_y,
        facing.degrees};
}

SiteWorld::WorkerConditionData worker_conditions_from_entity(flecs::entity entity)
{
    const auto vitals = entity.get<WorkerVitals>();

    return SiteWorld::WorkerConditionData {
        vitals.health,
        vitals.hydration,
        vitals.nourishment,
        vitals.energy_cap,
        vitals.energy,
        vitals.morale,
        vitals.work_efficiency,
        vitals.is_sheltered};
}

SiteWorld::WorkerData worker_data_from_entity(flecs::entity entity)
{
    return SiteWorld::WorkerData {
        worker_position_from_entity(entity),
        worker_conditions_from_entity(entity)};
}

void apply_worker_position_to_entity(flecs::entity entity, const SiteWorld::WorkerPositionData& data)
{
    entity
        .set<WorkerTilePosition>({
            data.tile_coord,
            data.tile_x,
            data.tile_y})
        .set<WorkerFacing>({data.facing_degrees});
}

void apply_worker_conditions_to_entity(flecs::entity entity, const SiteWorld::WorkerConditionData& data)
{
    entity.set<WorkerVitals>({
        data.health,
        data.hydration,
        data.nourishment,
        data.energy_cap,
        data.energy,
        data.morale,
        data.work_efficiency,
        data.is_sheltered});
}
}  // namespace

struct SiteWorld::Impl final
{
    flecs::world world {};
    std::uint32_t width {0};
    std::uint32_t height {0};
    flecs::entity_t tile_entity_base {0};
    std::vector<flecs::entity_t> device_entities_by_tile_index {};
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

void SiteWorld::initialize(const CreateDesc& desc)
{
    impl_ = std::make_unique<Impl>();
    register_site_world_types(impl_->world);

    impl_->width = desc.width;
    impl_->height = desc.height;
    const auto count = tile_count_from_dimensions(desc.width, desc.height);
    const auto initial_tile = default_tile_data();
    impl_->device_entities_by_tile_index.assign(count, 0U);

    for (std::size_t index = 0; index < count; ++index)
    {
        const auto coord = coord_from_index(desc.width, index);
        flecs::entity tile_entity = index == 0
            ? impl_->world.entity()
            : [&]() {
                ecs_entity_desc_t entity_desc {};
                entity_desc.id =
                    impl_->tile_entity_base + static_cast<flecs::entity_t>(index);
                return impl_->world.entity(ecs_entity_init(impl_->world.c_ptr(), &entity_desc));
            }();

        if (index == 0)
        {
            impl_->tile_entity_base = tile_entity.id();
        }

        tile_entity
            .add<TileTag>()
            .set<TileIndex>({static_cast<std::uint32_t>(index)})
            .set<TileCoordComponent>({coord})
            .set<TileTerrain>({initial_tile.static_data.terrain_type_id})
            .set<TileTraversable>({initial_tile.static_data.traversable})
            .set<TilePlantable>({initial_tile.static_data.plantable})
            .set<TileReservedByStructure>({initial_tile.static_data.reserved_by_structure})
            .set<TileSoilFertility>({initial_tile.ecology.soil_fertility})
            .set<TileMoisture>({initial_tile.ecology.moisture})
            .set<TileSoilSalinity>({initial_tile.ecology.soil_salinity})
            .set<TileSandBurial>({initial_tile.ecology.sand_burial})
            .set<TileHeat>({initial_tile.local_weather.heat})
            .set<TileWind>({initial_tile.local_weather.wind})
            .set<TileDust>({initial_tile.local_weather.dust})
            .set<TilePlantWeatherContribution>({
                initial_tile.plant_weather_contribution.heat_protection,
                initial_tile.plant_weather_contribution.wind_protection,
                initial_tile.plant_weather_contribution.dust_protection,
                initial_tile.plant_weather_contribution.fertility_improve,
                initial_tile.plant_weather_contribution.salinity_reduction,
                initial_tile.plant_weather_contribution.irrigation})
            .set<TileDeviceWeatherContribution>({
                initial_tile.device_weather_contribution.heat_protection,
                initial_tile.device_weather_contribution.wind_protection,
                initial_tile.device_weather_contribution.dust_protection,
                initial_tile.device_weather_contribution.fertility_improve,
                initial_tile.device_weather_contribution.salinity_reduction,
                initial_tile.device_weather_contribution.irrigation})
            .set<TilePlantSlot>({initial_tile.ecology.plant_id})
            .set<TileGroundCoverSlot>({initial_tile.ecology.ground_cover_type_id})
            .set<TilePlantDensity>({initial_tile.ecology.plant_density})
            .set<TileGrowthPressure>({initial_tile.ecology.growth_pressure});

        sync_tile_occupant_tag(
            tile_entity,
            initial_tile.ecology.plant_id,
            initial_tile.ecology.ground_cover_type_id);

        if (initial_tile.device.structure_id.value != 0U)
        {
            auto device_entity = impl_->world.entity();
            device_entity
                .add<DeviceTag>()
                .set<TileCoordComponent>({coord});
            apply_device_to_entity(device_entity, initial_tile.device);
            impl_->device_entities_by_tile_index[index] = device_entity.id();
        }
    }

    ecs_entity_desc_t worker_desc {};
    worker_desc.id = count > 0
        ? impl_->tile_entity_base + static_cast<flecs::entity_t>(count)
        : impl_->world.entity().id();
    worker_desc.name = "Worker";
    worker_desc.sep = "::";
    worker_desc.root_sep = "::";
    auto worker_entity = impl_->world.entity(ecs_entity_init(impl_->world.c_ptr(), &worker_desc));
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
            desc.worker_is_sheltered});

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
    return impl_ != nullptr
        ? tile_count_from_dimensions(impl_->width, impl_->height)
        : 0U;
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

TileCoord SiteWorld::tile_coord(std::size_t index) const noexcept
{
    return impl_ != nullptr ? coord_from_index(impl_->width, index) : TileCoord {};
}

std::uint64_t SiteWorld::tile_entity_id(TileCoord coord) const noexcept
{
    if (!contains(coord))
    {
        return 0U;
    }

    return impl_->tile_entity_base + static_cast<flecs::entity_t>(tile_index(coord));
}

std::uint64_t SiteWorld::device_entity_id(TileCoord coord) const noexcept
{
    if (!contains(coord) || impl_ == nullptr)
    {
        return 0U;
    }

    const auto index = tile_index(coord);
    return index < impl_->device_entities_by_tile_index.size()
        ? impl_->device_entities_by_tile_index[index]
        : 0U;
}

std::uint64_t SiteWorld::worker_entity_id() const noexcept
{
    return impl_ != nullptr ? impl_->worker_entity : 0U;
}

flecs::world& SiteWorld::ecs_world() noexcept
{
    return impl_->world;
}

const flecs::world& SiteWorld::ecs_world() const noexcept
{
    return impl_->world;
}

SiteWorld::TileStaticData SiteWorld::tile_static_data(TileCoord coord) const noexcept
{
    if (!contains(coord))
    {
        return {};
    }

    return tile_static_data_at_index(tile_index(coord));
}

SiteWorld::TileStaticData SiteWorld::tile_static_data_at_index(std::size_t index) const noexcept
{
    if (impl_ == nullptr || index >= tile_count())
    {
        return {};
    }

    return tile_static_from_entity(
        impl_->world.entity(impl_->tile_entity_base + static_cast<flecs::entity_t>(index)));
}

SiteWorld::TileEcologyData SiteWorld::tile_ecology(TileCoord coord) const noexcept
{
    if (!contains(coord))
    {
        return {};
    }

    return tile_ecology_at_index(tile_index(coord));
}

SiteWorld::TileEcologyData SiteWorld::tile_ecology_at_index(std::size_t index) const noexcept
{
    if (impl_ == nullptr || index >= tile_count())
    {
        return {};
    }

    return tile_ecology_from_entity(
        impl_->world.entity(impl_->tile_entity_base + static_cast<flecs::entity_t>(index)));
}

void SiteWorld::set_tile_ecology(TileCoord coord, const TileEcologyData& data)
{
    if (!contains(coord))
    {
        return;
    }

    set_tile_ecology_at_index(tile_index(coord), data);
}

void SiteWorld::set_tile_ecology_at_index(std::size_t index, const TileEcologyData& data)
{
    if (impl_ == nullptr || index >= tile_count())
    {
        return;
    }

    apply_tile_ecology_to_entity(
        impl_->world.entity(impl_->tile_entity_base + static_cast<flecs::entity_t>(index)),
        data);
}

SiteWorld::TileLocalWeatherData SiteWorld::tile_local_weather(TileCoord coord) const noexcept
{
    if (!contains(coord))
    {
        return {};
    }

    return tile_local_weather_at_index(tile_index(coord));
}

SiteWorld::TileLocalWeatherData SiteWorld::tile_local_weather_at_index(std::size_t index) const noexcept
{
    if (impl_ == nullptr || index >= tile_count())
    {
        return {};
    }

    return tile_local_weather_from_entity(
        impl_->world.entity(impl_->tile_entity_base + static_cast<flecs::entity_t>(index)));
}

void SiteWorld::set_tile_local_weather(TileCoord coord, const TileLocalWeatherData& data)
{
    if (!contains(coord))
    {
        return;
    }

    set_tile_local_weather_at_index(tile_index(coord), data);
}

void SiteWorld::set_tile_local_weather_at_index(std::size_t index, const TileLocalWeatherData& data)
{
    if (impl_ == nullptr || index >= tile_count())
    {
        return;
    }

    apply_tile_local_weather_to_entity(
        impl_->world.entity(impl_->tile_entity_base + static_cast<flecs::entity_t>(index)),
        data);
}

SiteWorld::TileWeatherContributionData SiteWorld::tile_plant_weather_contribution(
    TileCoord coord) const noexcept
{
    if (!contains(coord))
    {
        return {};
    }

    return tile_plant_weather_contribution_at_index(tile_index(coord));
}

SiteWorld::TileWeatherContributionData SiteWorld::tile_plant_weather_contribution_at_index(
    std::size_t index) const noexcept
{
    if (impl_ == nullptr || index >= tile_count())
    {
        return {};
    }

    return tile_plant_weather_contribution_from_entity(
        impl_->world.entity(impl_->tile_entity_base + static_cast<flecs::entity_t>(index)));
}

void SiteWorld::set_tile_plant_weather_contribution(
    TileCoord coord,
    const TileWeatherContributionData& data)
{
    if (!contains(coord))
    {
        return;
    }

    set_tile_plant_weather_contribution_at_index(tile_index(coord), data);
}

void SiteWorld::set_tile_plant_weather_contribution_at_index(
    std::size_t index,
    const TileWeatherContributionData& data)
{
    if (impl_ == nullptr || index >= tile_count())
    {
        return;
    }

    apply_tile_plant_weather_contribution_to_entity(
        impl_->world.entity(impl_->tile_entity_base + static_cast<flecs::entity_t>(index)),
        data);
}

SiteWorld::TileWeatherContributionData SiteWorld::tile_device_weather_contribution(
    TileCoord coord) const noexcept
{
    if (!contains(coord))
    {
        return {};
    }

    return tile_device_weather_contribution_at_index(tile_index(coord));
}

SiteWorld::TileWeatherContributionData SiteWorld::tile_device_weather_contribution_at_index(
    std::size_t index) const noexcept
{
    if (impl_ == nullptr || index >= tile_count())
    {
        return {};
    }

    return tile_device_weather_contribution_from_entity(
        impl_->world.entity(impl_->tile_entity_base + static_cast<flecs::entity_t>(index)));
}

void SiteWorld::set_tile_device_weather_contribution(
    TileCoord coord,
    const TileWeatherContributionData& data)
{
    if (!contains(coord))
    {
        return;
    }

    set_tile_device_weather_contribution_at_index(tile_index(coord), data);
}

void SiteWorld::set_tile_device_weather_contribution_at_index(
    std::size_t index,
    const TileWeatherContributionData& data)
{
    if (impl_ == nullptr || index >= tile_count())
    {
        return;
    }

    apply_tile_device_weather_contribution_to_entity(
        impl_->world.entity(impl_->tile_entity_base + static_cast<flecs::entity_t>(index)),
        data);
}

SiteWorld::TileWeatherContributionData SiteWorld::tile_total_weather_contribution(
    TileCoord coord) const noexcept
{
    if (!contains(coord))
    {
        return {};
    }

    return tile_total_weather_contribution_at_index(tile_index(coord));
}

SiteWorld::TileWeatherContributionData SiteWorld::tile_total_weather_contribution_at_index(
    std::size_t index) const noexcept
{
    if (impl_ == nullptr || index >= tile_count())
    {
        return {};
    }

    const auto plant = tile_plant_weather_contribution_at_index(index);
    const auto device = tile_device_weather_contribution_at_index(index);
    return TileWeatherContributionData {
        plant.heat_protection + device.heat_protection,
        plant.wind_protection + device.wind_protection,
        plant.dust_protection + device.dust_protection,
        plant.fertility_improve + device.fertility_improve,
        plant.salinity_reduction + device.salinity_reduction,
        plant.irrigation + device.irrigation};
}

SiteWorld::TileDeviceData SiteWorld::tile_device(TileCoord coord) const noexcept
{
    if (!contains(coord))
    {
        return {};
    }

    return tile_device_at_index(tile_index(coord));
}

SiteWorld::TileDeviceData SiteWorld::tile_device_at_index(std::size_t index) const noexcept
{
    if (impl_ == nullptr || index >= tile_count())
    {
        return {};
    }

    const auto entity_id = impl_->device_entities_by_tile_index[index];
    if (entity_id == 0U)
    {
        return default_device_data();
    }

    return tile_device_from_entity(impl_->world.entity(entity_id));
}

void SiteWorld::set_tile_device(TileCoord coord, const TileDeviceData& data)
{
    if (!contains(coord))
    {
        return;
    }

    set_tile_device_at_index(tile_index(coord), data);
}

void SiteWorld::set_tile_device_at_index(std::size_t index, const TileDeviceData& data)
{
    if (impl_ == nullptr || index >= tile_count())
    {
        return;
    }

    auto& entity_id = impl_->device_entities_by_tile_index[index];
    if (data.structure_id.value == 0U)
    {
        if (entity_id != 0U)
        {
            impl_->world.entity(entity_id).destruct();
            entity_id = 0U;
        }
        return;
    }

    const auto coord = tile_coord(index);
    auto entity = entity_id == 0U
        ? impl_->world.entity()
        : impl_->world.entity(entity_id);
    if (entity_id == 0U)
    {
        entity_id = entity.id();
        entity.add<DeviceTag>();
    }

    entity.set<TileCoordComponent>({coord});
    apply_device_to_entity(entity, data);
}

SiteWorld::TileData SiteWorld::tile_at(TileCoord coord) const noexcept
{
    if (!contains(coord))
    {
        return {};
    }

    return tile_at_index(tile_index(coord));
}

SiteWorld::TileData SiteWorld::tile_at_index(std::size_t index) const noexcept
{
    if (impl_ == nullptr || index >= tile_count())
    {
        return {};
    }

    auto tile = tile_data_from_entity(
        impl_->world.entity(impl_->tile_entity_base + static_cast<flecs::entity_t>(index)));
    tile.device = tile_device_at_index(index);
    return tile;
}

void SiteWorld::set_tile(TileCoord coord, const TileData& data)
{
    if (!contains(coord))
    {
        return;
    }

    set_tile_at_index(tile_index(coord), data);
}

void SiteWorld::set_tile_at_index(std::size_t index, const TileData& data)
{
    if (impl_ == nullptr || index >= tile_count())
    {
        return;
    }

    const auto entity = impl_->world.entity(
        impl_->tile_entity_base + static_cast<flecs::entity_t>(index));
    apply_tile_static_to_entity(entity, data.static_data);
    apply_tile_ecology_to_entity(entity, data.ecology);
    apply_tile_local_weather_to_entity(entity, data.local_weather);
    apply_tile_plant_weather_contribution_to_entity(entity, data.plant_weather_contribution);
    apply_tile_device_weather_contribution_to_entity(entity, data.device_weather_contribution);
    set_tile_device_at_index(index, data.device);
}

SiteWorld::WorkerPositionData SiteWorld::worker_position() const
{
    if (impl_ == nullptr || impl_->worker_entity == 0U)
    {
        return {};
    }

    return worker_position_from_entity(impl_->world.entity(impl_->worker_entity));
}

void SiteWorld::set_worker_position(const WorkerPositionData& data)
{
    if (impl_ == nullptr || impl_->worker_entity == 0U)
    {
        return;
    }

    apply_worker_position_to_entity(impl_->world.entity(impl_->worker_entity), data);
}

SiteWorld::WorkerConditionData SiteWorld::worker_conditions() const
{
    if (impl_ == nullptr || impl_->worker_entity == 0U)
    {
        return {};
    }

    return worker_conditions_from_entity(impl_->world.entity(impl_->worker_entity));
}

void SiteWorld::set_worker_conditions(const WorkerConditionData& data)
{
    if (impl_ == nullptr || impl_->worker_entity == 0U)
    {
        return;
    }

    apply_worker_conditions_to_entity(impl_->world.entity(impl_->worker_entity), data);
}

SiteWorld::WorkerData SiteWorld::worker() const
{
    if (impl_ == nullptr || impl_->worker_entity == 0U)
    {
        return {};
    }

    return worker_data_from_entity(impl_->world.entity(impl_->worker_entity));
}

void SiteWorld::set_worker(const WorkerData& data)
{
    if (impl_ == nullptr || impl_->worker_entity == 0U)
    {
        return;
    }

    const auto entity = impl_->world.entity(impl_->worker_entity);
    apply_worker_position_to_entity(entity, data.position);
    apply_worker_conditions_to_entity(entity, data.conditions);
}
}  // namespace gs1

#ifdef _MSC_VER
#pragma warning(pop)
#endif
