#pragma once

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

struct TileHeatProtection final
{
    float value;
};

struct TileWindProtection final
{
    float value;
};

struct TileDustProtection final
{
    float value;
};

struct TileFertilityImprove final
{
    float value;
};

struct TileSalinityReduction final
{
    float value;
};

struct TileIrrigation final
{
    float value;
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
GS1_REQUIRE_TRIVIAL(TileHeatProtection);
GS1_REQUIRE_TRIVIAL(TileWindProtection);
GS1_REQUIRE_TRIVIAL(TileDustProtection);
GS1_REQUIRE_TRIVIAL(TileFertilityImprove);
GS1_REQUIRE_TRIVIAL(TileSalinityReduction);
GS1_REQUIRE_TRIVIAL(TileIrrigation);
GS1_REQUIRE_TRIVIAL(TilePlantSlot);
GS1_REQUIRE_TRIVIAL(TileGroundCoverSlot);
GS1_REQUIRE_TRIVIAL(TilePlantDensity);
GS1_REQUIRE_TRIVIAL(TileGrowthPressure);
GS1_REQUIRE_TRIVIAL(DeviceStructureId);
GS1_REQUIRE_TRIVIAL(DeviceIntegrity);
GS1_REQUIRE_TRIVIAL(DeviceEfficiency);
GS1_REQUIRE_TRIVIAL(DeviceStoredWater);
GS1_REQUIRE_TRIVIAL(WorkerTilePosition);
GS1_REQUIRE_TRIVIAL(WorkerFacing);
GS1_REQUIRE_TRIVIAL(WorkerVitals);

#undef GS1_REQUIRE_TRIVIAL
}  // namespace gs1::site_ecs
