#pragma once

#include "support/id_types.h"

#include <cstdint>

namespace gs1::site_ecs
{
struct TileTag final
{
};

struct WorkerTag final
{
};

struct TileIndex final
{
    std::uint32_t value {0};
};

struct TileCoordComponent final
{
    TileCoord value {};
};

struct TileTerrain final
{
    std::uint32_t type_id {0};
};

struct TileTraversable final
{
    bool value {false};
};

struct TilePlantable final
{
    bool value {false};
};

struct TileReservedByStructure final
{
    bool value {false};
};

struct TileSoilFertility final
{
    float value {0.0f};
};

struct TileMoisture final
{
    float value {0.0f};
};

struct TileSoilSalinity final
{
    float value {0.0f};
};

struct TileSandBurial final
{
    float value {0.0f};
};

struct TileHeat final
{
    float value {0.0f};
};

struct TileWind final
{
    float value {0.0f};
};

struct TileDust final
{
    float value {0.0f};
};

struct TilePlantSlot final
{
    PlantId plant_id {};
};

struct TileGroundCoverSlot final
{
    std::uint32_t ground_cover_type_id {0};
};

struct TilePlantDensity final
{
    float value {0.0f};
};

struct TileGrowthPressure final
{
    float value {0.0f};
};

struct TileStructureSlot final
{
    StructureId structure_id {};
};

struct TileDeviceIntegrity final
{
    float value {1.0f};
};

struct TileDeviceEfficiency final
{
    float value {1.0f};
};

struct TileDeviceStoredWater final
{
    float value {0.0f};
};

struct SiteIdentityResource final
{
    SiteId site_id {};
    SiteRunId site_run_id {};
    std::uint32_t site_archetype_id {0};
};

struct TileDomainResource final
{
    std::uint32_t width {0};
    std::uint32_t height {0};
};

struct CampResource final
{
    TileCoord anchor_tile {};
    float durability {100.0f};
    bool protection_resolved {true};
    bool delivery_point_operational {true};
    bool shared_storage_access_enabled {true};
};

struct WorkerTilePosition final
{
    TileCoord tile_coord {};
    float tile_x {0.0f};
    float tile_y {0.0f};
};

struct WorkerFacing final
{
    float degrees {0.0f};
};

struct WorkerVitals final
{
    float health {100.0f};
    float hydration {100.0f};
    float nourishment {100.0f};
    float energy_cap {100.0f};
    float energy {100.0f};
    float morale {100.0f};
    float work_efficiency {1.0f};
    bool is_sheltered {false};
};

struct WorkerActionReference final
{
    std::uint32_t runtime_action_id {0};
    bool has_runtime_action_id {false};
};
}  // namespace gs1::site_ecs
