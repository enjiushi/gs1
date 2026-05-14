#pragma once

#include "messages/game_message.h"
#include "runtime/runtime_state_access.h"
#include "site/inventory_storage.h"
#include "site/site_run_state.h"
#include "site/site_world_access.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <utility>

namespace gs1
{
struct CampaignState;

enum class SiteComponent : std::uint8_t
{
    RunMeta = 0,
    Time,
    TileLayout,
    TileEcology,
    TileExcavation,
    TileWeather,
    TilePlantWeatherContribution,
    TileDeviceWeatherContribution,
    PlantWeatherRuntime,
    DeviceWeatherRuntime,
    DeviceCondition,
    DeviceRuntime,
    WorkerMotion,
    WorkerNeeds,
    Camp,
    Inventory,
    Contractor,
    Weather,
    LocalWeatherRuntime,
    Event,
    TaskBoard,
    Modifier,
    Economy,
    Craft,
    Action,
    Counters,
    Objective,
    Count
};

using SiteComponentMask = std::uint64_t;

[[nodiscard]] constexpr SiteComponentMask site_component_bit(SiteComponent component) noexcept
{
    return SiteComponentMask {1} << static_cast<std::uint8_t>(component);
}

template <typename... Components>
[[nodiscard]] consteval SiteComponentMask site_component_mask_of(Components... components) noexcept
{
    return (SiteComponentMask {0} | ... | site_component_bit(components));
}

[[nodiscard]] constexpr std::string_view site_component_name(SiteComponent component) noexcept
{
    switch (component)
    {
    case SiteComponent::RunMeta:
        return "RunMeta";
    case SiteComponent::Time:
        return "Time";
    case SiteComponent::TileLayout:
        return "TileLayout";
    case SiteComponent::TileEcology:
        return "TileEcology";
    case SiteComponent::TileExcavation:
        return "TileExcavation";
    case SiteComponent::TileWeather:
        return "TileWeather";
    case SiteComponent::TilePlantWeatherContribution:
        return "TilePlantWeatherContribution";
    case SiteComponent::TileDeviceWeatherContribution:
        return "TileDeviceWeatherContribution";
    case SiteComponent::PlantWeatherRuntime:
        return "PlantWeatherRuntime";
    case SiteComponent::DeviceWeatherRuntime:
        return "DeviceWeatherRuntime";
    case SiteComponent::DeviceCondition:
        return "DeviceCondition";
    case SiteComponent::DeviceRuntime:
        return "DeviceRuntime";
    case SiteComponent::WorkerMotion:
        return "WorkerMotion";
    case SiteComponent::WorkerNeeds:
        return "WorkerNeeds";
    case SiteComponent::Camp:
        return "Camp";
    case SiteComponent::Inventory:
        return "Inventory";
    case SiteComponent::Contractor:
        return "Contractor";
    case SiteComponent::Weather:
        return "Weather";
    case SiteComponent::LocalWeatherRuntime:
        return "LocalWeatherRuntime";
    case SiteComponent::Event:
        return "Event";
    case SiteComponent::TaskBoard:
        return "TaskBoard";
    case SiteComponent::Modifier:
        return "Modifier";
    case SiteComponent::Economy:
        return "Economy";
    case SiteComponent::Craft:
        return "Craft";
    case SiteComponent::Action:
        return "Action";
    case SiteComponent::Counters:
        return "Counters";
    case SiteComponent::Objective:
        return "Objective";
    case SiteComponent::Count:
    default:
        return "Unknown";
    }
}

struct SiteSystemAccess final
{
    std::string_view system_name {};
    SiteComponentMask read_components {0};
    SiteComponentMask own_components {0};
};

struct SiteSystemAccessValidationResult final
{
    bool ok {true};
    std::string_view message {};
    std::string_view system_name {};
    SiteComponent component {SiteComponent::Count};
    std::string_view other_system_name {};
};

template <std::size_t SystemCount>
[[nodiscard]] constexpr SiteSystemAccessValidationResult validate_site_system_access_registry(
    const std::array<SiteSystemAccess, SystemCount>& systems) noexcept
{
    std::array<std::string_view, static_cast<std::size_t>(SiteComponent::Count)> owners {};

    for (const auto& system : systems)
    {
        if ((system.own_components & ~system.read_components) != 0U)
        {
            for (std::size_t index = 0; index < static_cast<std::size_t>(SiteComponent::Count); ++index)
            {
                const auto component = static_cast<SiteComponent>(index);
                const auto bit = site_component_bit(component);
                if ((system.own_components & bit) != 0U && (system.read_components & bit) == 0U)
                {
                    return SiteSystemAccessValidationResult {
                        false,
                        "Own components must also be declared as readable.",
                        system.system_name,
                        component,
                        {}};
                }
            }
        }

        for (std::size_t index = 0; index < static_cast<std::size_t>(SiteComponent::Count); ++index)
        {
            const auto component = static_cast<SiteComponent>(index);
            const auto bit = site_component_bit(component);
            if ((system.own_components & bit) == 0U)
            {
                continue;
            }

            auto& owner = owners[index];
            if (!owner.empty() && owner != system.system_name)
            {
                return SiteSystemAccessValidationResult {
                    false,
                    "Component has multiple owning systems.",
                    system.system_name,
                    component,
                    owner};
            }

            owner = system.system_name;
        }
    }

    return {};
}

template <typename SystemTag>
[[nodiscard]] consteval bool site_system_reads_component(SiteComponent component) noexcept
{
    return (SystemTag::access().read_components & site_component_bit(component)) != 0U;
}

template <typename SystemTag>
[[nodiscard]] consteval bool site_system_owns_component(SiteComponent component) noexcept
{
    return (SystemTag::access().own_components & site_component_bit(component)) != 0U;
}

template <typename SystemTag>
[[nodiscard]] consteval bool site_system_reads_any_tile_component() noexcept
{
    return site_system_reads_component<SystemTag>(SiteComponent::TileLayout) ||
        site_system_reads_component<SystemTag>(SiteComponent::TileEcology) ||
        site_system_reads_component<SystemTag>(SiteComponent::TileExcavation) ||
        site_system_reads_component<SystemTag>(SiteComponent::TileWeather) ||
        site_system_reads_component<SystemTag>(SiteComponent::TilePlantWeatherContribution) ||
        site_system_reads_component<SystemTag>(SiteComponent::TileDeviceWeatherContribution) ||
        site_system_reads_component<SystemTag>(SiteComponent::DeviceCondition) ||
        site_system_reads_component<SystemTag>(SiteComponent::DeviceRuntime);
}

template <typename SystemTag>
[[nodiscard]] consteval bool site_system_owns_any_tile_component() noexcept
{
    return site_system_owns_component<SystemTag>(SiteComponent::TileEcology) ||
        site_system_owns_component<SystemTag>(SiteComponent::TileExcavation) ||
        site_system_owns_component<SystemTag>(SiteComponent::TileWeather) ||
        site_system_owns_component<SystemTag>(SiteComponent::TilePlantWeatherContribution) ||
        site_system_owns_component<SystemTag>(SiteComponent::TileDeviceWeatherContribution) ||
        site_system_owns_component<SystemTag>(SiteComponent::DeviceCondition) ||
        site_system_owns_component<SystemTag>(SiteComponent::DeviceRuntime);
}

template <typename SystemTag>
[[nodiscard]] consteval bool site_system_reads_any_worker_component() noexcept
{
    return site_system_reads_component<SystemTag>(SiteComponent::WorkerMotion) ||
        site_system_reads_component<SystemTag>(SiteComponent::WorkerNeeds);
}

template <typename SystemTag>
[[nodiscard]] consteval bool site_system_owns_any_worker_component() noexcept
{
    return site_system_owns_component<SystemTag>(SiteComponent::WorkerMotion) ||
        site_system_owns_component<SystemTag>(SiteComponent::WorkerNeeds);
}

struct SiteMoveDirectionInput final
{
    float world_move_x {0.0f};
    float world_move_y {0.0f};
    float world_move_z {0.0f};
    bool present {false};
};

template <typename SystemTag>
class SiteWorldAccess final
{
public:
    explicit SiteWorldAccess(SiteRunState& site_run) noexcept
        : site_run_(&site_run)
    {
    }

    explicit SiteWorldAccess(RuntimeInvocation& invocation) noexcept
        : invocation_(&invocation)
    {
        if (!using_split_state_sets())
        {
            auto& active_site_run = runtime_invocation_state_ref<RuntimeActiveSiteRunTag>(invocation);
            if (active_site_run.has_value())
            {
                site_run_ = &active_site_run.value();
            }
        }
    }

    [[nodiscard]] SiteRunId site_run_id() const noexcept
    {
        return using_split_state_sets()
            ? invocation_->state_manager()->query<StateSetId::SiteRunMeta>(*invocation_->owned_state())->site_run_id
            : site_run_->site_run_id;
    }

    [[nodiscard]] SiteId site_id() const noexcept
    {
        return using_split_state_sets()
            ? invocation_->state_manager()->query<StateSetId::SiteRunMeta>(*invocation_->owned_state())->site_id
            : site_run_->site_id;
    }

    [[nodiscard]] std::uint32_t site_id_value() const noexcept
    {
        return site_id().value;
    }

    [[nodiscard]] std::uint32_t site_archetype_id() const noexcept
    {
        return using_split_state_sets()
            ? invocation_->state_manager()->query<StateSetId::SiteRunMeta>(*invocation_->owned_state())
                  ->site_archetype_id
            : site_run_->site_archetype_id;
    }

    [[nodiscard]] std::uint32_t attempt_index() const noexcept
    {
        return using_split_state_sets()
            ? invocation_->state_manager()->query<StateSetId::SiteRunMeta>(*invocation_->owned_state())
                  ->attempt_index
            : site_run_->attempt_index;
    }

    [[nodiscard]] std::uint64_t site_attempt_seed() const noexcept
    {
        return using_split_state_sets()
            ? invocation_->state_manager()->query<StateSetId::SiteRunMeta>(*invocation_->owned_state())
                  ->site_attempt_seed
            : site_run_->site_attempt_seed;
    }

    [[nodiscard]] SiteRunStatus run_status() const noexcept
    {
        return using_split_state_sets()
            ? invocation_->state_manager()->query<StateSetId::SiteRunMeta>(*invocation_->owned_state())->run_status
            : site_run_->run_status;
    }

    [[nodiscard]] const SiteClockState& read_time() const noexcept { return site_clock_state(); }
    [[nodiscard]] SiteClockState& own_time() noexcept { return site_clock_state(); }

    [[nodiscard]] bool has_world() const noexcept
    {
        const auto* world = site_world_ptr();
        return world != nullptr && world->initialized();
    }

    [[nodiscard]] std::uint32_t tile_width() const noexcept
    {
        static_assert(
            site_system_reads_any_tile_component<SystemTag>(),
            "System must declare a tile-grid component as readable.");
        const auto* world = site_world_ptr();
        return world != nullptr ? world->width() : 0U;
    }

    [[nodiscard]] std::uint32_t tile_height() const noexcept
    {
        static_assert(
            site_system_reads_any_tile_component<SystemTag>(),
            "System must declare a tile-grid component as readable.");
        const auto* world = site_world_ptr();
        return world != nullptr ? world->height() : 0U;
    }

    [[nodiscard]] std::size_t tile_count() const noexcept
    {
        static_assert(
            site_system_reads_any_tile_component<SystemTag>(),
            "System must declare a tile-grid component as readable.");
        const auto* world = site_world_ptr();
        return world != nullptr ? world->tile_count() : 0U;
    }

    [[nodiscard]] bool tile_coord_in_bounds(TileCoord coord) const noexcept
    {
        static_assert(
            site_system_reads_any_tile_component<SystemTag>(),
            "System must declare a tile-grid component as readable.");
        const auto* world = site_world_ptr();
        return world != nullptr && world->contains(coord);
    }

    [[nodiscard]] TileCoord tile_coord(std::size_t index) const noexcept
    {
        static_assert(
            site_system_reads_any_tile_component<SystemTag>(),
            "System must declare a tile-grid component as readable.");
        const auto* world = site_world_ptr();
        return world != nullptr ? world->tile_coord(index) : TileCoord {};
    }

    [[nodiscard]] std::uint32_t tile_index(TileCoord coord) const noexcept
    {
        static_assert(
            site_system_reads_any_tile_component<SystemTag>(),
            "System must declare a tile-grid component as readable.");
        const auto* world = site_world_ptr();
        return world != nullptr && world->contains(coord) ? world->tile_index(coord) : 0U;
    }

    [[nodiscard]] std::uint64_t tile_entity_id(TileCoord coord) const noexcept
    {
        static_assert(
            site_system_reads_any_tile_component<SystemTag>(),
            "System must declare a tile-grid component as readable.");
        const auto* world = site_world_ptr();
        return world != nullptr && world->contains(coord) ? world->tile_entity_id(coord) : 0U;
    }

    [[nodiscard]] std::uint64_t device_entity_id(TileCoord coord) const noexcept
    {
        static_assert(
            site_system_reads_any_tile_component<SystemTag>(),
            "System must declare a tile-grid component as readable.");
        const auto* world = site_world_ptr();
        return world != nullptr && world->contains(coord) ? world->device_entity_id(coord) : 0U;
    }

    [[nodiscard]] std::uint64_t worker_entity_id() const noexcept
    {
        static_assert(
            site_system_reads_any_worker_component<SystemTag>() ||
                site_system_reads_any_tile_component<SystemTag>(),
            "System must declare worker or tile access as readable.");
        const auto* world = site_world_ptr();
        return world != nullptr ? world->worker_entity_id() : 0U;
    }

    [[nodiscard]] flecs::world* ecs_world_ptr() noexcept
    {
        return site_world_ptr() != nullptr ? &site_world_ptr()->ecs_world() : nullptr;
    }

    [[nodiscard]] const flecs::world* ecs_world_ptr() const noexcept
    {
        return site_world_ptr() != nullptr ? &site_world_ptr()->ecs_world() : nullptr;
    }

    [[nodiscard]] SiteWorld::TileEcologyData read_tile_ecology(TileCoord coord) const noexcept
    {
        static_assert(
            site_system_reads_component<SystemTag>(SiteComponent::TileEcology),
            "System must declare TileEcology as readable.");
        const auto* world = site_world_ptr();
        return world != nullptr ? world->tile_ecology(coord) : SiteWorld::TileEcologyData {};
    }

    [[nodiscard]] SiteWorld::TileEcologyData read_tile_ecology_at_index(std::size_t index) const noexcept
    {
        static_assert(
            site_system_reads_component<SystemTag>(SiteComponent::TileEcology),
            "System must declare TileEcology as readable.");
        const auto* world = site_world_ptr();
        return world != nullptr ? world->tile_ecology_at_index(index) : SiteWorld::TileEcologyData {};
    }

    void write_tile_ecology(TileCoord coord, const SiteWorld::TileEcologyData& data)
    {
        static_assert(
            site_system_owns_component<SystemTag>(SiteComponent::TileEcology),
            "System must declare TileEcology as owned.");
        auto* world = site_world_ptr();
        if (world != nullptr)
        {
            world->set_tile_ecology(coord, data);
        }
    }

    void write_tile_ecology_at_index(std::size_t index, const SiteWorld::TileEcologyData& data)
    {
        static_assert(
            site_system_owns_component<SystemTag>(SiteComponent::TileEcology),
            "System must declare TileEcology as owned.");
        auto* world = site_world_ptr();
        if (world != nullptr)
        {
            world->set_tile_ecology_at_index(index, data);
        }
    }

    [[nodiscard]] SiteWorld::TileExcavationData read_tile_excavation(TileCoord coord) const noexcept
    {
        static_assert(
            site_system_reads_component<SystemTag>(SiteComponent::TileExcavation),
            "System must declare TileExcavation as readable.");
        const auto* world = site_world_ptr();
        return world != nullptr ? world->tile_excavation(coord) : SiteWorld::TileExcavationData {};
    }

    [[nodiscard]] SiteWorld::TileExcavationData read_tile_excavation_at_index(std::size_t index) const noexcept
    {
        static_assert(
            site_system_reads_component<SystemTag>(SiteComponent::TileExcavation),
            "System must declare TileExcavation as readable.");
        const auto* world = site_world_ptr();
        return world != nullptr ? world->tile_excavation_at_index(index) : SiteWorld::TileExcavationData {};
    }

    void write_tile_excavation(TileCoord coord, const SiteWorld::TileExcavationData& data)
    {
        static_assert(
            site_system_owns_component<SystemTag>(SiteComponent::TileExcavation),
            "System must declare TileExcavation as owned.");
        auto* world = site_world_ptr();
        if (world != nullptr)
        {
            world->set_tile_excavation(coord, data);
        }
    }

    void write_tile_excavation_at_index(std::size_t index, const SiteWorld::TileExcavationData& data)
    {
        static_assert(
            site_system_owns_component<SystemTag>(SiteComponent::TileExcavation),
            "System must declare TileExcavation as owned.");
        auto* world = site_world_ptr();
        if (world != nullptr)
        {
            world->set_tile_excavation_at_index(index, data);
        }
    }

    [[nodiscard]] SiteWorld::TileLocalWeatherData read_tile_local_weather(TileCoord coord) const noexcept
    {
        static_assert(
            site_system_reads_component<SystemTag>(SiteComponent::TileWeather),
            "System must declare TileWeather as readable.");
        const auto* world = site_world_ptr();
        return world != nullptr ? world->tile_local_weather(coord) : SiteWorld::TileLocalWeatherData {};
    }

    [[nodiscard]] SiteWorld::TileLocalWeatherData read_tile_local_weather_at_index(
        std::size_t index) const noexcept
    {
        static_assert(
            site_system_reads_component<SystemTag>(SiteComponent::TileWeather),
            "System must declare TileWeather as readable.");
        const auto* world = site_world_ptr();
        return world != nullptr
            ? world->tile_local_weather_at_index(index)
            : SiteWorld::TileLocalWeatherData {};
    }

    void write_tile_local_weather(TileCoord coord, const SiteWorld::TileLocalWeatherData& data)
    {
        static_assert(
            site_system_owns_component<SystemTag>(SiteComponent::TileWeather),
            "System must declare TileWeather as owned.");
        auto* world = site_world_ptr();
        if (world != nullptr)
        {
            world->set_tile_local_weather(coord, data);
        }
    }

    void write_tile_local_weather_at_index(std::size_t index, const SiteWorld::TileLocalWeatherData& data)
    {
        static_assert(
            site_system_owns_component<SystemTag>(SiteComponent::TileWeather),
            "System must declare TileWeather as owned.");
        auto* world = site_world_ptr();
        if (world != nullptr)
        {
            world->set_tile_local_weather_at_index(index, data);
        }
    }

    [[nodiscard]] SiteWorld::TileWeatherContributionData read_tile_plant_weather_contribution(
        TileCoord coord) const noexcept
    {
        static_assert(
            site_system_reads_component<SystemTag>(SiteComponent::TilePlantWeatherContribution),
            "System must declare TilePlantWeatherContribution as readable.");
        const auto* world = site_world_ptr();
        return world != nullptr
            ? world->tile_plant_weather_contribution(coord)
            : SiteWorld::TileWeatherContributionData {};
    }

    [[nodiscard]] SiteWorld::TileWeatherContributionData read_tile_plant_weather_contribution_at_index(
        std::size_t index) const noexcept
    {
        static_assert(
            site_system_reads_component<SystemTag>(SiteComponent::TilePlantWeatherContribution),
            "System must declare TilePlantWeatherContribution as readable.");
        const auto* world = site_world_ptr();
        return world != nullptr
            ? world->tile_plant_weather_contribution_at_index(index)
            : SiteWorld::TileWeatherContributionData {};
    }

    void write_tile_plant_weather_contribution(
        TileCoord coord,
        const SiteWorld::TileWeatherContributionData& data)
    {
        static_assert(
            site_system_owns_component<SystemTag>(SiteComponent::TilePlantWeatherContribution),
            "System must declare TilePlantWeatherContribution as owned.");
        auto* world = site_world_ptr();
        if (world != nullptr)
        {
            world->set_tile_plant_weather_contribution(coord, data);
        }
    }

    void write_tile_plant_weather_contribution_at_index(
        std::size_t index,
        const SiteWorld::TileWeatherContributionData& data)
    {
        static_assert(
            site_system_owns_component<SystemTag>(SiteComponent::TilePlantWeatherContribution),
            "System must declare TilePlantWeatherContribution as owned.");
        auto* world = site_world_ptr();
        if (world != nullptr)
        {
            world->set_tile_plant_weather_contribution_at_index(index, data);
        }
    }

    [[nodiscard]] SiteWorld::TileWeatherContributionData read_tile_device_weather_contribution(
        TileCoord coord) const noexcept
    {
        static_assert(
            site_system_reads_component<SystemTag>(SiteComponent::TileDeviceWeatherContribution),
            "System must declare TileDeviceWeatherContribution as readable.");
        const auto* world = site_world_ptr();
        return world != nullptr
            ? world->tile_device_weather_contribution(coord)
            : SiteWorld::TileWeatherContributionData {};
    }

    [[nodiscard]] SiteWorld::TileWeatherContributionData read_tile_device_weather_contribution_at_index(
        std::size_t index) const noexcept
    {
        static_assert(
            site_system_reads_component<SystemTag>(SiteComponent::TileDeviceWeatherContribution),
            "System must declare TileDeviceWeatherContribution as readable.");
        const auto* world = site_world_ptr();
        return world != nullptr
            ? world->tile_device_weather_contribution_at_index(index)
            : SiteWorld::TileWeatherContributionData {};
    }

    void write_tile_device_weather_contribution(
        TileCoord coord,
        const SiteWorld::TileWeatherContributionData& data)
    {
        static_assert(
            site_system_owns_component<SystemTag>(SiteComponent::TileDeviceWeatherContribution),
            "System must declare TileDeviceWeatherContribution as owned.");
        auto* world = site_world_ptr();
        if (world != nullptr)
        {
            world->set_tile_device_weather_contribution(coord, data);
        }
    }

    void write_tile_device_weather_contribution_at_index(
        std::size_t index,
        const SiteWorld::TileWeatherContributionData& data)
    {
        static_assert(
            site_system_owns_component<SystemTag>(SiteComponent::TileDeviceWeatherContribution),
            "System must declare TileDeviceWeatherContribution as owned.");
        auto* world = site_world_ptr();
        if (world != nullptr)
        {
            world->set_tile_device_weather_contribution_at_index(index, data);
        }
    }

    [[nodiscard]] SiteWorld::TileData read_tile(TileCoord coord) const noexcept
    {
        static_assert(
            site_system_reads_any_tile_component<SystemTag>(),
            "System must declare a tile-grid component as readable.");
        const auto* world = site_world_ptr();
        return world != nullptr ? world->tile_at(coord) : SiteWorld::TileData {};
    }

    [[nodiscard]] SiteWorld::TileData read_tile_at_index(std::size_t index) const noexcept
    {
        static_assert(
            site_system_reads_any_tile_component<SystemTag>(),
            "System must declare a tile-grid component as readable.");
        const auto* world = site_world_ptr();
        return world != nullptr ? world->tile_at_index(index) : SiteWorld::TileData {};
    }

    void write_tile(TileCoord coord, const SiteWorld::TileData& data)
    {
        static_assert(
            site_system_owns_any_tile_component<SystemTag>(),
            "System must declare a tile-grid component as owned.");
        auto* world = site_world_ptr();
        if (world != nullptr)
        {
            world->set_tile(coord, data);
        }
    }

    void write_tile_at_index(std::size_t index, const SiteWorld::TileData& data)
    {
        static_assert(
            site_system_owns_any_tile_component<SystemTag>(),
            "System must declare a tile-grid component as owned.");
        auto* world = site_world_ptr();
        if (world != nullptr)
        {
            world->set_tile_at_index(index, data);
        }
    }

    [[nodiscard]] SiteWorld::WorkerData read_worker() const
    {
        static_assert(
            site_system_reads_any_worker_component<SystemTag>(),
            "System must declare a worker component as readable.");
        const auto* world = site_world_ptr();
        return world != nullptr ? world->worker() : SiteWorld::WorkerData {};
    }

    void write_worker(const SiteWorld::WorkerData& data)
    {
        static_assert(
            site_system_owns_any_worker_component<SystemTag>(),
            "System must declare a worker component as owned.");
        auto* world = site_world_ptr();
        if (world != nullptr)
        {
            world->set_worker(data);
        }
    }

    [[nodiscard]] const CampState& read_camp() const noexcept { return camp_state(); }
    [[nodiscard]] CampState& own_camp() noexcept { return camp_state(); }

    [[nodiscard]] const InventoryState& read_inventory() const noexcept { return inventory_state(); }
    [[nodiscard]] InventoryState& own_inventory() noexcept { return inventory_state(); }

    [[nodiscard]] const ContractorState& read_contractor() const noexcept { return contractor_state(); }
    [[nodiscard]] ContractorState& own_contractor() noexcept { return contractor_state(); }

    [[nodiscard]] const WeatherState& read_weather() const noexcept { return weather_state(); }
    [[nodiscard]] WeatherState& own_weather() noexcept { return weather_state(); }

    [[nodiscard]] const SiteWorld* read_site_world() const noexcept
    {
        return site_world_ptr();
    }

    [[nodiscard]] SiteWorld* own_site_world() noexcept
    {
        return site_world_ptr();
    }

    [[nodiscard]] const LocalWeatherResolveState& read_local_weather_runtime() const noexcept
    {
        return local_weather_runtime_state();
    }

    [[nodiscard]] LocalWeatherResolveState& own_local_weather_runtime() noexcept
    {
        return local_weather_runtime_state();
    }

    [[nodiscard]] const PlantWeatherContributionState& read_plant_weather_runtime() const noexcept
    {
        return plant_weather_runtime_state();
    }

    [[nodiscard]] PlantWeatherContributionState& own_plant_weather_runtime() noexcept
    {
        return plant_weather_runtime_state();
    }

    [[nodiscard]] const DeviceWeatherContributionState& read_device_weather_runtime() const noexcept
    {
        return device_weather_runtime_state();
    }

    [[nodiscard]] DeviceWeatherContributionState& own_device_weather_runtime() noexcept
    {
        return device_weather_runtime_state();
    }

    [[nodiscard]] const EventState& read_event() const noexcept { return event_state(); }
    [[nodiscard]] EventState& own_event() noexcept { return event_state(); }

    [[nodiscard]] const TaskBoardState& read_task_board() const noexcept { return task_board_state(); }
    [[nodiscard]] TaskBoardState& own_task_board() noexcept { return task_board_state(); }

    [[nodiscard]] const ModifierState& read_modifier() const noexcept { return modifier_state(); }
    [[nodiscard]] ModifierState& own_modifier() noexcept { return modifier_state(); }

    [[nodiscard]] const EconomyState& read_economy() const noexcept { return economy_state(); }
    [[nodiscard]] EconomyState& own_economy() noexcept { return economy_state(); }

    [[nodiscard]] const CraftState& read_craft() const noexcept { return craft_state(); }
    [[nodiscard]] CraftState& own_craft() noexcept { return craft_state(); }

    [[nodiscard]] const ActionState& read_action() const noexcept { return action_state(); }
    [[nodiscard]] ActionState& own_action() noexcept { return action_state(); }

    [[nodiscard]] const SiteCounters& read_counters() const noexcept { return counters_state(); }
    [[nodiscard]] SiteCounters& own_counters() noexcept { return counters_state(); }

    [[nodiscard]] const SiteObjectiveState& read_objective() const noexcept { return objective_state(); }
    [[nodiscard]] SiteObjectiveState& own_objective() noexcept { return objective_state(); }

private:
    [[nodiscard]] bool using_split_state_sets() const noexcept
    {
        return invocation_ != nullptr &&
            invocation_->owned_state() != nullptr &&
            invocation_->state_manager() != nullptr;
    }

    [[nodiscard]] SiteRunState& aggregate_site_run() const
    {
        return *site_run_;
    }

    [[nodiscard]] SiteClockState& site_clock_state() const
    {
        if (using_split_state_sets())
        {
            return invocation_->state_manager()->state<StateSetId::SiteClock>(*invocation_->owned_state()).value();
        }
        return aggregate_site_run().clock;
    }

    [[nodiscard]] CampState& camp_state() const
    {
        if (using_split_state_sets())
        {
            return invocation_->state_manager()->state<StateSetId::SiteCamp>(*invocation_->owned_state()).value();
        }
        return aggregate_site_run().camp;
    }

    [[nodiscard]] InventoryState& inventory_state() const
    {
        if (using_split_state_sets())
        {
            return invocation_->state_manager()->state<StateSetId::SiteInventory>(*invocation_->owned_state()).value();
        }
        return aggregate_site_run().inventory;
    }

    [[nodiscard]] ContractorState& contractor_state() const
    {
        if (using_split_state_sets())
        {
            return invocation_->state_manager()->state<StateSetId::SiteContractor>(*invocation_->owned_state())
                .value();
        }
        return aggregate_site_run().contractor;
    }

    [[nodiscard]] WeatherState& weather_state() const
    {
        if (using_split_state_sets())
        {
            return invocation_->state_manager()->state<StateSetId::SiteWeather>(*invocation_->owned_state()).value();
        }
        return aggregate_site_run().weather;
    }

    [[nodiscard]] LocalWeatherResolveState& local_weather_runtime_state() const
    {
        if (using_split_state_sets())
        {
            return invocation_->state_manager()
                ->state<StateSetId::SiteLocalWeatherResolve>(*invocation_->owned_state())
                .value();
        }
        return aggregate_site_run().local_weather_resolve;
    }

    [[nodiscard]] PlantWeatherContributionState& plant_weather_runtime_state() const
    {
        if (using_split_state_sets())
        {
            return invocation_->state_manager()
                ->state<StateSetId::SitePlantWeatherContribution>(*invocation_->owned_state())
                .value();
        }
        return aggregate_site_run().plant_weather_contribution;
    }

    [[nodiscard]] DeviceWeatherContributionState& device_weather_runtime_state() const
    {
        if (using_split_state_sets())
        {
            return invocation_->state_manager()
                ->state<StateSetId::SiteDeviceWeatherContribution>(*invocation_->owned_state())
                .value();
        }
        return aggregate_site_run().device_weather_contribution;
    }

    [[nodiscard]] EventState& event_state() const
    {
        if (using_split_state_sets())
        {
            return invocation_->state_manager()->state<StateSetId::SiteEvent>(*invocation_->owned_state()).value();
        }
        return aggregate_site_run().event;
    }

    [[nodiscard]] TaskBoardState& task_board_state() const
    {
        if (using_split_state_sets())
        {
            return invocation_->state_manager()->state<StateSetId::SiteTaskBoard>(*invocation_->owned_state())
                .value();
        }
        return aggregate_site_run().task_board;
    }

    [[nodiscard]] ModifierState& modifier_state() const
    {
        if (using_split_state_sets())
        {
            return invocation_->state_manager()->state<StateSetId::SiteModifier>(*invocation_->owned_state())
                .value();
        }
        return aggregate_site_run().modifier;
    }

    [[nodiscard]] EconomyState& economy_state() const
    {
        if (using_split_state_sets())
        {
            return invocation_->state_manager()->state<StateSetId::SiteEconomy>(*invocation_->owned_state())
                .value();
        }
        return aggregate_site_run().economy;
    }

    [[nodiscard]] CraftState& craft_state() const
    {
        if (using_split_state_sets())
        {
            return invocation_->state_manager()->state<StateSetId::SiteCraft>(*invocation_->owned_state()).value();
        }
        return aggregate_site_run().craft;
    }

    [[nodiscard]] ActionState& action_state() const
    {
        if (using_split_state_sets())
        {
            return invocation_->state_manager()->state<StateSetId::SiteAction>(*invocation_->owned_state()).value();
        }
        return aggregate_site_run().site_action;
    }

    [[nodiscard]] SiteCounters& counters_state() const
    {
        if (using_split_state_sets())
        {
            return invocation_->state_manager()->state<StateSetId::SiteCounters>(*invocation_->owned_state())
                .value();
        }
        return aggregate_site_run().counters;
    }

    [[nodiscard]] SiteObjectiveState& objective_state() const
    {
        if (using_split_state_sets())
        {
            return invocation_->state_manager()->state<StateSetId::SiteObjective>(*invocation_->owned_state())
                .value();
        }
        return aggregate_site_run().objective;
    }

    [[nodiscard]] const SiteWorld* site_world_ptr() const noexcept
    {
        if (using_split_state_sets())
        {
            const auto& world_state =
                invocation_->state_manager()->query<StateSetId::SiteWorld>(*invocation_->owned_state());
            return world_state.has_value() ? world_state->site_world.get() : nullptr;
        }

        return site_run_ != nullptr ? site_run_->site_world.get() : nullptr;
    }

    [[nodiscard]] SiteWorld* site_world_ptr() noexcept
    {
        return const_cast<SiteWorld*>(std::as_const(*this).site_world_ptr());
    }

    RuntimeInvocation* invocation_ {nullptr};
    SiteRunState* site_run_ {nullptr};
};

}  // namespace gs1
