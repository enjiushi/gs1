#pragma once

#include "messages/game_message.h"
#include "site/inventory_storage.h"
#include "site/site_projection_update_flags.h"
#include "site/site_run_state.h"
#include "site/site_world_access.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace gs1
{
struct CampaignState;

enum class SiteComponent : std::uint8_t
{
    RunMeta = 0,
    Time,
    TileLayout,
    TileEcology,
    TileWeather,
    TilePlantWeatherContribution,
    TileDeviceWeatherContribution,
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
    PhonePanel,
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
    case SiteComponent::TileWeather:
        return "TileWeather";
    case SiteComponent::TilePlantWeatherContribution:
        return "TilePlantWeatherContribution";
    case SiteComponent::TileDeviceWeatherContribution:
        return "TileDeviceWeatherContribution";
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
    case SiteComponent::PhonePanel:
        return "PhonePanel";
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
        : site_run_(site_run)
    {
    }

    [[nodiscard]] SiteRunId site_run_id() const noexcept { return site_run_.site_run_id; }
    [[nodiscard]] SiteId site_id() const noexcept { return site_run_.site_id; }
    [[nodiscard]] std::uint32_t site_id_value() const noexcept { return site_run_.site_id.value; }
    [[nodiscard]] std::uint32_t site_archetype_id() const noexcept { return site_run_.site_archetype_id; }
    [[nodiscard]] std::uint32_t attempt_index() const noexcept { return site_run_.attempt_index; }
    [[nodiscard]] std::uint64_t site_attempt_seed() const noexcept { return site_run_.site_attempt_seed; }
    [[nodiscard]] SiteRunStatus run_status() const noexcept { return site_run_.run_status; }

    [[nodiscard]] const SiteClockState& read_time() const noexcept { return site_run_.clock; }
    [[nodiscard]] SiteClockState& own_time() noexcept { return site_run_.clock; }

    [[nodiscard]] bool has_world() const noexcept
    {
        return site_world_access::has_world(site_run_);
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

    [[nodiscard]] float last_reported_tile_density_or(
        TileCoord coord,
        float fallback) const noexcept
    {
        static_assert(
            site_system_reads_component<SystemTag>(SiteComponent::TileEcology),
            "System must declare TileEcology as readable.");
        const auto* world = site_world_ptr();
        if (world == nullptr || !world->contains(coord))
        {
            return fallback;
        }

        const auto index = world->tile_index(coord);
        if (site_run_.last_reported_tile_density_valid.size() != world->tile_count() ||
            site_run_.last_reported_tile_density.size() != world->tile_count() ||
            index >= site_run_.last_reported_tile_density_valid.size() ||
            site_run_.last_reported_tile_density_valid[index] == 0U)
        {
            return fallback;
        }

        return site_run_.last_reported_tile_density[index];
    }

    void set_last_reported_tile_density(TileCoord coord, float density) noexcept
    {
        static_assert(
            site_system_owns_component<SystemTag>(SiteComponent::TileEcology),
            "System must declare TileEcology as owned.");
        auto* world = site_world_ptr();
        if (world == nullptr || !world->contains(coord))
        {
            return;
        }

        const auto tile_count = world->tile_count();
        if (site_run_.last_reported_tile_density_valid.size() != tile_count)
        {
            site_run_.last_reported_tile_density_valid.assign(tile_count, 0U);
        }
        if (site_run_.last_reported_tile_density.size() != tile_count)
        {
            site_run_.last_reported_tile_density.assign(tile_count, 0.0f);
        }

        const auto index = world->tile_index(coord);
        if (index >= site_run_.last_reported_tile_density_valid.size() ||
            index >= site_run_.last_reported_tile_density.size())
        {
            return;
        }

        site_run_.last_reported_tile_density[index] = density;
        site_run_.last_reported_tile_density_valid[index] = 1U;
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

    [[nodiscard]] const CampState& read_camp() const noexcept { return site_run_.camp; }
    [[nodiscard]] CampState& own_camp() noexcept { return site_run_.camp; }

    [[nodiscard]] const InventoryState& read_inventory() const noexcept { return site_run_.inventory; }
    [[nodiscard]] InventoryState& own_inventory() noexcept { return site_run_.inventory; }

    [[nodiscard]] const ContractorState& read_contractor() const noexcept { return site_run_.contractor; }
    [[nodiscard]] ContractorState& own_contractor() noexcept { return site_run_.contractor; }

    [[nodiscard]] const WeatherState& read_weather() const noexcept { return site_run_.weather; }
    [[nodiscard]] WeatherState& own_weather() noexcept { return site_run_.weather; }

    [[nodiscard]] const LocalWeatherResolveState& read_local_weather_runtime() const noexcept
    {
        return site_run_.local_weather_resolve;
    }

    [[nodiscard]] LocalWeatherResolveState& own_local_weather_runtime() noexcept
    {
        return site_run_.local_weather_resolve;
    }

    [[nodiscard]] const EventState& read_event() const noexcept { return site_run_.event; }
    [[nodiscard]] EventState& own_event() noexcept { return site_run_.event; }

    [[nodiscard]] const TaskBoardState& read_task_board() const noexcept { return site_run_.task_board; }
    [[nodiscard]] TaskBoardState& own_task_board() noexcept { return site_run_.task_board; }

    [[nodiscard]] const ModifierState& read_modifier() const noexcept { return site_run_.modifier; }
    [[nodiscard]] ModifierState& own_modifier() noexcept { return site_run_.modifier; }

    [[nodiscard]] const EconomyState& read_economy() const noexcept { return site_run_.economy; }
    [[nodiscard]] EconomyState& own_economy() noexcept { return site_run_.economy; }

    [[nodiscard]] const PhonePanelState& read_phone_panel() const noexcept { return site_run_.phone_panel; }
    [[nodiscard]] PhonePanelState& own_phone_panel() noexcept { return site_run_.phone_panel; }

    [[nodiscard]] const CraftState& read_craft() const noexcept { return site_run_.craft; }
    [[nodiscard]] CraftState& own_craft() noexcept { return site_run_.craft; }

    [[nodiscard]] const ActionState& read_action() const noexcept { return site_run_.site_action; }
    [[nodiscard]] ActionState& own_action() noexcept { return site_run_.site_action; }

    [[nodiscard]] const SiteCounters& read_counters() const noexcept { return site_run_.counters; }
    [[nodiscard]] SiteCounters& own_counters() noexcept { return site_run_.counters; }

    [[nodiscard]] const SiteObjectiveState& read_objective() const noexcept { return site_run_.objective; }
    [[nodiscard]] SiteObjectiveState& own_objective() noexcept { return site_run_.objective; }

    void mark_projection_dirty(std::uint64_t dirty_flags) noexcept
    {
        if ((dirty_flags & SITE_PROJECTION_UPDATE_TILES) != 0U)
        {
            site_run_.pending_full_tile_projection_update = true;
        }
        if ((dirty_flags & SITE_PROJECTION_UPDATE_INVENTORY) != 0U)
        {
            site_run_.pending_full_inventory_projection_update = true;
            site_run_.pending_inventory_storage_descriptor_projection_update = true;
            if (site_run_.inventory.opened_device_storage_id != 0U)
            {
                site_run_.pending_opened_inventory_storage_full_projection_update = true;
            }
        }

        site_run_.pending_projection_update_flags |= dirty_flags;
    }

    void mark_tile_projection_dirty(TileCoord coord) noexcept
    {
        if (!site_world_access::tile_coord_in_bounds(site_run_, coord))
        {
            return;
        }

        const auto tile_count = site_world_access::tile_count(site_run_);
        if (site_run_.pending_tile_projection_update_mask.size() != tile_count)
        {
            site_run_.pending_tile_projection_update_mask.assign(tile_count, 0U);
            site_run_.pending_tile_projection_updates.clear();
        }

        const auto index = site_world_access::tile_index(site_run_, coord);
        if (index < site_run_.pending_tile_projection_update_mask.size() &&
            site_run_.pending_tile_projection_update_mask[index] == 0U)
        {
            site_run_.pending_tile_projection_update_mask[index] = 1U;
            site_run_.pending_tile_projection_updates.push_back(coord);
        }

        site_run_.pending_projection_update_flags |= SITE_PROJECTION_UPDATE_TILES;
    }

    void mark_inventory_slot_projection_dirty(
        Gs1InventoryContainerKind container_kind,
        std::uint32_t slot_index) noexcept
    {
        switch (container_kind)
        {
        case GS1_INVENTORY_CONTAINER_WORKER_PACK:
            mark_inventory_slot_projection_dirty_by_storage(
                site_run_.inventory.worker_pack_storage_id,
                slot_index);
            break;
        case GS1_INVENTORY_CONTAINER_DEVICE_STORAGE:
            if (site_run_.inventory.opened_device_storage_id != 0U)
            {
                mark_inventory_slot_projection_dirty_by_storage(
                    site_run_.inventory.opened_device_storage_id,
                    slot_index);
            }
            break;
        default:
            return;
        }
    }

    void mark_inventory_storage_descriptors_projection_dirty() noexcept
    {
        site_run_.pending_inventory_storage_descriptor_projection_update = true;
        site_run_.pending_projection_update_flags |= SITE_PROJECTION_UPDATE_INVENTORY;
    }

    void mark_inventory_view_state_projection_dirty() noexcept
    {
        site_run_.pending_inventory_view_state_projection_update = true;
        site_run_.pending_projection_update_flags |= SITE_PROJECTION_UPDATE_INVENTORY;
    }

    void mark_opened_inventory_storage_full_projection_dirty() noexcept
    {
        if (site_run_.inventory.opened_device_storage_id == 0U)
        {
            return;
        }

        site_run_.pending_opened_inventory_storage_full_projection_update = true;
        site_run_.pending_projection_update_flags |= SITE_PROJECTION_UPDATE_INVENTORY;
    }

    void mark_inventory_slot_projection_dirty_by_storage(
        std::uint32_t storage_id,
        std::uint32_t slot_index) noexcept
    {
        if (storage_id == 0U)
        {
            return;
        }

        std::vector<std::uint32_t>* pending_updates = nullptr;
        std::vector<std::uint8_t>* pending_update_mask = nullptr;
        std::uint32_t slot_count = 0U;

        if (storage_id == site_run_.inventory.worker_pack_storage_id)
        {
            pending_updates = &site_run_.pending_worker_pack_inventory_projection_updates;
            pending_update_mask = &site_run_.pending_worker_pack_inventory_projection_update_mask;
            slot_count = site_run_.inventory.worker_pack_slot_count;
        }
        else if (storage_id == site_run_.inventory.opened_device_storage_id)
        {
            const auto* storage_state =
                inventory_storage::storage_container_state_for_storage_id(site_run_, storage_id);
            if (storage_state == nullptr)
            {
                return;
            }

            pending_updates = &site_run_.pending_opened_inventory_storage_projection_updates;
            pending_update_mask = &site_run_.pending_opened_inventory_storage_projection_update_mask;
            slot_count = static_cast<std::uint32_t>(storage_state->slot_item_instance_ids.size());
        }
        else
        {
            return;
        }

        if (slot_index >= slot_count || pending_updates == nullptr || pending_update_mask == nullptr)
        {
            return;
        }

        if (pending_update_mask->size() != slot_count)
        {
            pending_update_mask->assign(slot_count, 0U);
            pending_updates->clear();
        }

        if ((*pending_update_mask)[slot_index] == 0U)
        {
            (*pending_update_mask)[slot_index] = 1U;
            pending_updates->push_back(slot_index);
        }

        site_run_.pending_projection_update_flags |= SITE_PROJECTION_UPDATE_INVENTORY;
    }

private:
    [[nodiscard]] const SiteWorld* site_world_ptr() const noexcept
    {
        return has_world() ? site_run_.site_world.get() : nullptr;
    }

    [[nodiscard]] SiteWorld* site_world_ptr() noexcept
    {
        return has_world() ? site_run_.site_world.get() : nullptr;
    }

    SiteRunState& site_run_;
};

template <typename SystemTag>
struct SiteSystemContext final
{
    const CampaignState& campaign;
    SiteRunState& site_run;
    SiteWorldAccess<SystemTag> world;
    GameMessageQueue& message_queue;
    double fixed_step_seconds {0.0};
    SiteMoveDirectionInput move_direction {};
};

template <typename SystemTag>
[[nodiscard]] inline SiteSystemContext<SystemTag> make_site_system_context(
    const CampaignState& campaign,
    SiteRunState& site_run,
    GameMessageQueue& message_queue,
    double fixed_step_seconds,
    SiteMoveDirectionInput move_direction)
{
    return SiteSystemContext<SystemTag> {
        campaign,
        site_run,
        SiteWorldAccess<SystemTag> {site_run},
        message_queue,
        fixed_step_seconds,
        move_direction};
}
}  // namespace gs1
