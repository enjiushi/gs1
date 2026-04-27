#pragma once

#include "support/excavation_types.h"
#include "support/id_types.h"

#include <cstdint>
#include <type_traits>

namespace gs1::site_ecs
{
struct TileTag final
{
};

struct TileOccupantTag final
{
};

struct ActiveEcologyTag final
{
};

struct DirtyEcologyTag final
{
};

struct DeviceTag final
{
};

struct WorkerTag final
{
};

struct StorageContainerTag final
{
};

struct StorageItemTag final
{
};

struct TileIndex final
{
    std::uint32_t value;
};

struct TileCoordComponent final
{
    TileCoord value;
};

enum class StorageContainerKind : std::uint8_t
{
    WorkerPack = 0,
    DeviceStorage = 1
};

struct StorageContainerKindComponent final
{
    StorageContainerKind kind;
};

struct StorageSlotIndex final
{
    std::uint16_t value;
};

struct StorageItemStack final
{
    ItemId item_id;
    std::uint32_t quantity;
    float condition;
    float freshness;
};

struct StorageItemLocation final
{
    std::uint64_t container_entity_id;
    std::uint16_t slot_index;
    std::uint16_t reserved0;
};

struct StorageOwnedByDevice final
{
};

struct StorageAtTile final
{
};

struct TileTerrain final
{
    std::uint32_t type_id;
};

struct TileTraversable final
{
    bool value;
};

struct TilePlantable final
{
    bool value;
};

struct TileReservedByStructure final
{
    bool value;
};

struct TileSoilFertility final
{
    float value;
};

struct TileMoisture final
{
    float value;
};

struct TileSoilSalinity final
{
    float value;
};

struct TileSandBurial final
{
    float value;
};

struct TileHeat final
{
    float value;
};

struct TileWind final
{
    float value;
};

struct TileDust final
{
    float value;
};

struct TilePlantWeatherContribution final
{
    float heat_protection;
    float wind_protection;
    float dust_protection;
    float fertility_improve;
    float salinity_reduction;
    float irrigation;
};

struct TileDeviceWeatherContribution final
{
    float heat_protection;
    float wind_protection;
    float dust_protection;
    float fertility_improve;
    float salinity_reduction;
    float irrigation;
};

struct TilePlantSlot final
{
    PlantId plant_id;
};

struct TileGroundCoverSlot final
{
    std::uint32_t ground_cover_type_id;
};

struct TilePlantDensity final
{
    float value;
};

struct TileGrowthPressure final
{
    float value;
};

struct TileExcavationState final
{
    ExcavationDepth depth;
    std::uint8_t reserved0[3];
};

struct DirtyEcologyMask final
{
    std::uint32_t value;
};

struct TileEcologyReportState final
{
    float last_reported_density;
    std::uint8_t valid;
    std::uint8_t reserved0[3];
};

struct DeviceStructureId final
{
    StructureId structure_id;
};

struct DeviceIntegrity final
{
    float value;
};

struct DeviceEfficiency final
{
    float value;
};

struct DeviceStoredWater final
{
    float value;
};

struct WorkerTilePosition final
{
    TileCoord tile_coord;
    float tile_x;
    float tile_y;
};

struct WorkerFacing final
{
    float degrees;
};

struct WorkerVitals final
{
    float health;
    float hydration;
    float nourishment;
    float energy_cap;
    float energy;
    float morale;
    float work_efficiency;
    bool is_sheltered;
};

#define GS1_REQUIRE_TRIVIAL(Type) \
    static_assert(std::is_trivial_v<Type>, #Type " must remain trivial."); \
    static_assert(std::is_trivially_copyable_v<Type>, #Type " must remain trivially copyable.")

GS1_REQUIRE_TRIVIAL(TileTag);
GS1_REQUIRE_TRIVIAL(TileOccupantTag);
GS1_REQUIRE_TRIVIAL(ActiveEcologyTag);
GS1_REQUIRE_TRIVIAL(DirtyEcologyTag);
GS1_REQUIRE_TRIVIAL(DeviceTag);
GS1_REQUIRE_TRIVIAL(WorkerTag);
GS1_REQUIRE_TRIVIAL(StorageContainerTag);
GS1_REQUIRE_TRIVIAL(StorageItemTag);
GS1_REQUIRE_TRIVIAL(TileIndex);
GS1_REQUIRE_TRIVIAL(TileCoordComponent);
GS1_REQUIRE_TRIVIAL(StorageContainerKindComponent);
GS1_REQUIRE_TRIVIAL(StorageSlotIndex);
GS1_REQUIRE_TRIVIAL(StorageItemStack);
GS1_REQUIRE_TRIVIAL(StorageItemLocation);
GS1_REQUIRE_TRIVIAL(StorageOwnedByDevice);
GS1_REQUIRE_TRIVIAL(StorageAtTile);
GS1_REQUIRE_TRIVIAL(TileTerrain);
GS1_REQUIRE_TRIVIAL(TileTraversable);
GS1_REQUIRE_TRIVIAL(TilePlantable);
GS1_REQUIRE_TRIVIAL(TileReservedByStructure);
GS1_REQUIRE_TRIVIAL(TileSoilFertility);
GS1_REQUIRE_TRIVIAL(TileMoisture);
GS1_REQUIRE_TRIVIAL(TileSoilSalinity);
GS1_REQUIRE_TRIVIAL(TileSandBurial);
GS1_REQUIRE_TRIVIAL(TileHeat);
GS1_REQUIRE_TRIVIAL(TileWind);
GS1_REQUIRE_TRIVIAL(TileDust);
GS1_REQUIRE_TRIVIAL(TilePlantWeatherContribution);
GS1_REQUIRE_TRIVIAL(TileDeviceWeatherContribution);
GS1_REQUIRE_TRIVIAL(TilePlantSlot);
GS1_REQUIRE_TRIVIAL(TileGroundCoverSlot);
GS1_REQUIRE_TRIVIAL(TilePlantDensity);
GS1_REQUIRE_TRIVIAL(TileGrowthPressure);
GS1_REQUIRE_TRIVIAL(TileExcavationState);
GS1_REQUIRE_TRIVIAL(DirtyEcologyMask);
GS1_REQUIRE_TRIVIAL(TileEcologyReportState);
GS1_REQUIRE_TRIVIAL(DeviceStructureId);
GS1_REQUIRE_TRIVIAL(DeviceIntegrity);
GS1_REQUIRE_TRIVIAL(DeviceEfficiency);
GS1_REQUIRE_TRIVIAL(DeviceStoredWater);
GS1_REQUIRE_TRIVIAL(WorkerTilePosition);
GS1_REQUIRE_TRIVIAL(WorkerFacing);
GS1_REQUIRE_TRIVIAL(WorkerVitals);

#undef GS1_REQUIRE_TRIVIAL
}  // namespace gs1::site_ecs
