#pragma once

#include "support/id_types.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <type_traits>

namespace flecs
{
struct world;
}

namespace gs1
{
class SiteWorld final
{
public:
    struct TileStaticData final
    {
        std::uint32_t terrain_type_id;
        bool traversable;
        bool plantable;
        bool reserved_by_structure;
    };

    struct TileEcologyData final
    {
        float soil_fertility;
        float moisture;
        float soil_salinity;
        float sand_burial;
        PlantId plant_id;
        std::uint32_t ground_cover_type_id;
        float plant_density;
        float growth_pressure;
    };

    struct TileLocalWeatherData final
    {
        float heat;
        float wind;
        float dust;
    };

    struct TileDeviceData final
    {
        StructureId structure_id;
        float device_integrity;
        float device_efficiency;
        float device_stored_water;
    };

    struct TileData final
    {
        TileStaticData static_data;
        TileEcologyData ecology;
        TileLocalWeatherData local_weather;
        TileDeviceData device;
    };

    struct WorkerPositionData final
    {
        TileCoord tile_coord;
        float tile_x;
        float tile_y;
        float facing_degrees;
    };

    struct WorkerConditionData final
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

    struct WorkerData final
    {
        WorkerPositionData position;
        WorkerConditionData conditions;
    };

    struct CreateDesc final
    {
        std::uint32_t width;
        std::uint32_t height;
        TileCoord worker_tile_coord;
        float worker_tile_x;
        float worker_tile_y;
        float worker_facing_degrees;
        float worker_health;
        float worker_hydration;
        float worker_nourishment;
        float worker_energy_cap;
        float worker_energy;
        float worker_morale;
        float worker_work_efficiency;
        bool worker_is_sheltered;
    };

    SiteWorld();
    ~SiteWorld();

    SiteWorld(const SiteWorld&) = delete;
    SiteWorld& operator=(const SiteWorld&) = delete;
    SiteWorld(SiteWorld&&) noexcept;
    SiteWorld& operator=(SiteWorld&&) noexcept;

    void initialize(const CreateDesc& desc);

    [[nodiscard]] bool initialized() const noexcept;
    [[nodiscard]] std::uint32_t width() const noexcept;
    [[nodiscard]] std::uint32_t height() const noexcept;
    [[nodiscard]] std::size_t tile_count() const noexcept;
    [[nodiscard]] bool contains(TileCoord coord) const noexcept;
    [[nodiscard]] std::uint32_t tile_index(TileCoord coord) const noexcept;
    [[nodiscard]] TileCoord tile_coord(std::size_t index) const noexcept;
    [[nodiscard]] std::uint64_t tile_entity_id(TileCoord coord) const noexcept;
    [[nodiscard]] std::uint64_t device_entity_id(TileCoord coord) const noexcept;
    [[nodiscard]] std::uint64_t worker_entity_id() const noexcept;
    [[nodiscard]] flecs::world& ecs_world() noexcept;
    [[nodiscard]] const flecs::world& ecs_world() const noexcept;
    [[nodiscard]] TileStaticData tile_static_data(TileCoord coord) const noexcept;
    [[nodiscard]] TileStaticData tile_static_data_at_index(std::size_t index) const noexcept;
    [[nodiscard]] TileEcologyData tile_ecology(TileCoord coord) const noexcept;
    [[nodiscard]] TileEcologyData tile_ecology_at_index(std::size_t index) const noexcept;
    void set_tile_ecology(TileCoord coord, const TileEcologyData& data);
    void set_tile_ecology_at_index(std::size_t index, const TileEcologyData& data);
    [[nodiscard]] TileLocalWeatherData tile_local_weather(TileCoord coord) const noexcept;
    [[nodiscard]] TileLocalWeatherData tile_local_weather_at_index(std::size_t index) const noexcept;
    void set_tile_local_weather(TileCoord coord, const TileLocalWeatherData& data);
    void set_tile_local_weather_at_index(std::size_t index, const TileLocalWeatherData& data);
    [[nodiscard]] TileDeviceData tile_device(TileCoord coord) const noexcept;
    [[nodiscard]] TileDeviceData tile_device_at_index(std::size_t index) const noexcept;
    void set_tile_device(TileCoord coord, const TileDeviceData& data);
    void set_tile_device_at_index(std::size_t index, const TileDeviceData& data);
    [[nodiscard]] TileData tile_at(TileCoord coord) const noexcept;
    [[nodiscard]] TileData tile_at_index(std::size_t index) const noexcept;
    void set_tile(TileCoord coord, const TileData& data);
    void set_tile_at_index(std::size_t index, const TileData& data);
    [[nodiscard]] WorkerPositionData worker_position() const;
    void set_worker_position(const WorkerPositionData& data);
    [[nodiscard]] WorkerConditionData worker_conditions() const;
    void set_worker_conditions(const WorkerConditionData& data);
    [[nodiscard]] WorkerData worker() const;
    void set_worker(const WorkerData& data);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_ {};
};

#define GS1_REQUIRE_TRIVIAL(Type) \
    static_assert(std::is_trivial_v<Type>, #Type " must remain trivial."); \
    static_assert(std::is_trivially_copyable_v<Type>, #Type " must remain trivially copyable.")

GS1_REQUIRE_TRIVIAL(SiteWorld::TileStaticData);
GS1_REQUIRE_TRIVIAL(SiteWorld::TileEcologyData);
GS1_REQUIRE_TRIVIAL(SiteWorld::TileLocalWeatherData);
GS1_REQUIRE_TRIVIAL(SiteWorld::TileDeviceData);
GS1_REQUIRE_TRIVIAL(SiteWorld::TileData);
GS1_REQUIRE_TRIVIAL(SiteWorld::WorkerPositionData);
GS1_REQUIRE_TRIVIAL(SiteWorld::WorkerConditionData);
GS1_REQUIRE_TRIVIAL(SiteWorld::WorkerData);
GS1_REQUIRE_TRIVIAL(SiteWorld::CreateDesc);

#undef GS1_REQUIRE_TRIVIAL
}  // namespace gs1
