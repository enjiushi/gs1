#pragma once

#include "site/site_projection_update_flags.h"
#include "site/site_run_state.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace gs1
{
enum class SiteComponent : std::uint8_t
{
    RunMeta = 0,
    Time,
    TileLayout,
    TileEcology,
    TileWeather,
    DeviceCondition,
    DeviceRuntime,
    WorkerMotion,
    WorkerNeeds,
    Camp,
    Inventory,
    Contractor,
    Weather,
    Event,
    TaskBoard,
    Modifier,
    Economy,
    Action,
    Counters,
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
    case SiteComponent::Event:
        return "Event";
    case SiteComponent::TaskBoard:
        return "TaskBoard";
    case SiteComponent::Modifier:
        return "Modifier";
    case SiteComponent::Economy:
        return "Economy";
    case SiteComponent::Action:
        return "Action";
    case SiteComponent::Counters:
        return "Counters";
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

class SiteWorld final
{
public:
    explicit SiteWorld(SiteRunState& site_run) noexcept
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

    [[nodiscard]] const SiteClockState& clock() const noexcept { return site_run_.clock; }
    [[nodiscard]] SiteClockState& clock() noexcept { return site_run_.clock; }

    [[nodiscard]] const TileGridState& tile_grid() const noexcept { return site_run_.tile_grid; }
    [[nodiscard]] TileGridState& tile_grid() noexcept { return site_run_.tile_grid; }

    [[nodiscard]] const WorkerState& worker() const noexcept { return site_run_.worker; }
    [[nodiscard]] WorkerState& worker() noexcept { return site_run_.worker; }

    [[nodiscard]] const CampState& camp() const noexcept { return site_run_.camp; }
    [[nodiscard]] CampState& camp() noexcept { return site_run_.camp; }

    [[nodiscard]] const InventoryState& inventory() const noexcept { return site_run_.inventory; }
    [[nodiscard]] InventoryState& inventory() noexcept { return site_run_.inventory; }

    [[nodiscard]] const ContractorState& contractor() const noexcept { return site_run_.contractor; }
    [[nodiscard]] ContractorState& contractor() noexcept { return site_run_.contractor; }

    [[nodiscard]] const WeatherState& weather() const noexcept { return site_run_.weather; }
    [[nodiscard]] WeatherState& weather() noexcept { return site_run_.weather; }

    [[nodiscard]] const EventState& event() const noexcept { return site_run_.event; }
    [[nodiscard]] EventState& event() noexcept { return site_run_.event; }

    [[nodiscard]] const TaskBoardState& task_board() const noexcept { return site_run_.task_board; }
    [[nodiscard]] TaskBoardState& task_board() noexcept { return site_run_.task_board; }

    [[nodiscard]] const ModifierState& modifier() const noexcept { return site_run_.modifier; }
    [[nodiscard]] ModifierState& modifier() noexcept { return site_run_.modifier; }

    [[nodiscard]] const EconomyState& economy() const noexcept { return site_run_.economy; }
    [[nodiscard]] EconomyState& economy() noexcept { return site_run_.economy; }

    [[nodiscard]] const ActionState& action() const noexcept { return site_run_.site_action; }
    [[nodiscard]] ActionState& action() noexcept { return site_run_.site_action; }

    [[nodiscard]] const SiteCounters& counters() const noexcept { return site_run_.counters; }
    [[nodiscard]] SiteCounters& counters() noexcept { return site_run_.counters; }

    void mark_projection_dirty(std::uint64_t dirty_flags) noexcept
    {
        if ((dirty_flags & SITE_PROJECTION_UPDATE_TILES) != 0U)
        {
            site_run_.pending_full_tile_projection_update = true;
        }

        site_run_.pending_projection_update_flags |= dirty_flags;
    }

    void mark_tile_projection_dirty(TileCoord coord) noexcept
    {
        if (!tile_coord_in_bounds(site_run_.tile_grid, coord))
        {
            return;
        }

        const auto tile_count = site_run_.tile_grid.tile_count();
        if (site_run_.pending_tile_projection_update_mask.size() != tile_count)
        {
            site_run_.pending_tile_projection_update_mask.assign(tile_count, 0U);
            site_run_.pending_tile_projection_updates.clear();
        }

        const auto index = site_run_.tile_grid.to_index(coord);
        if (index < site_run_.pending_tile_projection_update_mask.size() &&
            site_run_.pending_tile_projection_update_mask[index] == 0U)
        {
            site_run_.pending_tile_projection_update_mask[index] = 1U;
            site_run_.pending_tile_projection_updates.push_back(coord);
        }

        site_run_.pending_projection_update_flags |= SITE_PROJECTION_UPDATE_TILES;
    }

private:
    [[nodiscard]] static bool tile_coord_in_bounds(
        const TileGridState& tile_grid,
        TileCoord coord) noexcept
    {
        return coord.x >= 0 &&
            coord.y >= 0 &&
            static_cast<std::uint32_t>(coord.x) < tile_grid.width &&
            static_cast<std::uint32_t>(coord.y) < tile_grid.height;
    }

private:
    SiteRunState& site_run_;
};

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
        site_system_reads_component<SystemTag>(SiteComponent::DeviceCondition) ||
        site_system_reads_component<SystemTag>(SiteComponent::DeviceRuntime);
}

template <typename SystemTag>
[[nodiscard]] consteval bool site_system_owns_any_tile_component() noexcept
{
    return site_system_owns_component<SystemTag>(SiteComponent::TileEcology) ||
        site_system_owns_component<SystemTag>(SiteComponent::TileWeather) ||
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

template <typename SystemTag>
class SiteWorldAccess final
{
public:
    explicit SiteWorldAccess(SiteRunState& site_run) noexcept
        : world_(site_run)
    {
    }

    [[nodiscard]] SiteRunId site_run_id() const noexcept
    {
        static_assert(
            site_system_reads_component<SystemTag>(SiteComponent::RunMeta),
            "System must declare SiteComponent::RunMeta as readable.");
        return world_.site_run_id();
    }

    [[nodiscard]] SiteId site_id() const noexcept
    {
        static_assert(
            site_system_reads_component<SystemTag>(SiteComponent::RunMeta),
            "System must declare SiteComponent::RunMeta as readable.");
        return world_.site_id();
    }

    [[nodiscard]] std::uint32_t site_id_value() const noexcept
    {
        static_assert(
            site_system_reads_component<SystemTag>(SiteComponent::RunMeta),
            "System must declare SiteComponent::RunMeta as readable.");
        return world_.site_id_value();
    }

    [[nodiscard]] std::uint32_t site_archetype_id() const noexcept
    {
        static_assert(
            site_system_reads_component<SystemTag>(SiteComponent::RunMeta),
            "System must declare SiteComponent::RunMeta as readable.");
        return world_.site_archetype_id();
    }

    [[nodiscard]] std::uint32_t attempt_index() const noexcept
    {
        static_assert(
            site_system_reads_component<SystemTag>(SiteComponent::RunMeta),
            "System must declare SiteComponent::RunMeta as readable.");
        return world_.attempt_index();
    }

    [[nodiscard]] std::uint64_t site_attempt_seed() const noexcept
    {
        static_assert(
            site_system_reads_component<SystemTag>(SiteComponent::RunMeta),
            "System must declare SiteComponent::RunMeta as readable.");
        return world_.site_attempt_seed();
    }

    [[nodiscard]] SiteRunStatus run_status() const noexcept
    {
        static_assert(
            site_system_reads_component<SystemTag>(SiteComponent::RunMeta),
            "System must declare SiteComponent::RunMeta as readable.");
        return world_.run_status();
    }

    [[nodiscard]] const SiteClockState& read_time() const noexcept
    {
        static_assert(
            site_system_reads_component<SystemTag>(SiteComponent::Time),
            "System must declare SiteComponent::Time as readable.");
        return world_.clock();
    }

    [[nodiscard]] SiteClockState& own_time() noexcept
    {
        static_assert(
            site_system_owns_component<SystemTag>(SiteComponent::Time),
            "System must declare SiteComponent::Time as owned.");
        return world_.clock();
    }

    [[nodiscard]] const TileGridState& read_tile_grid() const noexcept
    {
        static_assert(
            site_system_reads_any_tile_component<SystemTag>(),
            "System must declare a tile-grid component as readable.");
        return world_.tile_grid();
    }

    [[nodiscard]] TileGridState& own_tile_grid() noexcept
    {
        static_assert(
            site_system_owns_any_tile_component<SystemTag>(),
            "System must declare a tile-grid component as owned.");
        return world_.tile_grid();
    }

    [[nodiscard]] const WorkerState& read_worker_motion() const noexcept
    {
        static_assert(
            site_system_reads_component<SystemTag>(SiteComponent::WorkerMotion),
            "System must declare SiteComponent::WorkerMotion as readable.");
        return world_.worker();
    }

    [[nodiscard]] WorkerState& own_worker_motion() noexcept
    {
        static_assert(
            site_system_owns_component<SystemTag>(SiteComponent::WorkerMotion),
            "System must declare SiteComponent::WorkerMotion as owned.");
        return world_.worker();
    }

    [[nodiscard]] const WorkerState& read_worker_needs() const noexcept
    {
        static_assert(
            site_system_reads_component<SystemTag>(SiteComponent::WorkerNeeds),
            "System must declare SiteComponent::WorkerNeeds as readable.");
        return world_.worker();
    }

    [[nodiscard]] WorkerState& own_worker_needs() noexcept
    {
        static_assert(
            site_system_owns_component<SystemTag>(SiteComponent::WorkerNeeds),
            "System must declare SiteComponent::WorkerNeeds as owned.");
        return world_.worker();
    }

    [[nodiscard]] const CampState& read_camp() const noexcept
    {
        static_assert(
            site_system_reads_component<SystemTag>(SiteComponent::Camp),
            "System must declare SiteComponent::Camp as readable.");
        return world_.camp();
    }

    [[nodiscard]] CampState& own_camp() noexcept
    {
        static_assert(
            site_system_owns_component<SystemTag>(SiteComponent::Camp),
            "System must declare SiteComponent::Camp as owned.");
        return world_.camp();
    }

    [[nodiscard]] const InventoryState& read_inventory() const noexcept
    {
        static_assert(
            site_system_reads_component<SystemTag>(SiteComponent::Inventory),
            "System must declare SiteComponent::Inventory as readable.");
        return world_.inventory();
    }

    [[nodiscard]] InventoryState& own_inventory() noexcept
    {
        static_assert(
            site_system_owns_component<SystemTag>(SiteComponent::Inventory),
            "System must declare SiteComponent::Inventory as owned.");
        return world_.inventory();
    }

    [[nodiscard]] const ContractorState& read_contractor() const noexcept
    {
        static_assert(
            site_system_reads_component<SystemTag>(SiteComponent::Contractor),
            "System must declare SiteComponent::Contractor as readable.");
        return world_.contractor();
    }

    [[nodiscard]] ContractorState& own_contractor() noexcept
    {
        static_assert(
            site_system_owns_component<SystemTag>(SiteComponent::Contractor),
            "System must declare SiteComponent::Contractor as owned.");
        return world_.contractor();
    }

    [[nodiscard]] const WeatherState& read_weather() const noexcept
    {
        static_assert(
            site_system_reads_component<SystemTag>(SiteComponent::Weather),
            "System must declare SiteComponent::Weather as readable.");
        return world_.weather();
    }

    [[nodiscard]] WeatherState& own_weather() noexcept
    {
        static_assert(
            site_system_owns_component<SystemTag>(SiteComponent::Weather),
            "System must declare SiteComponent::Weather as owned.");
        return world_.weather();
    }

    [[nodiscard]] const EventState& read_event() const noexcept
    {
        static_assert(
            site_system_reads_component<SystemTag>(SiteComponent::Event),
            "System must declare SiteComponent::Event as readable.");
        return world_.event();
    }

    [[nodiscard]] EventState& own_event() noexcept
    {
        static_assert(
            site_system_owns_component<SystemTag>(SiteComponent::Event),
            "System must declare SiteComponent::Event as owned.");
        return world_.event();
    }

    [[nodiscard]] const TaskBoardState& read_task_board() const noexcept
    {
        static_assert(
            site_system_reads_component<SystemTag>(SiteComponent::TaskBoard),
            "System must declare SiteComponent::TaskBoard as readable.");
        return world_.task_board();
    }

    [[nodiscard]] TaskBoardState& own_task_board() noexcept
    {
        static_assert(
            site_system_owns_component<SystemTag>(SiteComponent::TaskBoard),
            "System must declare SiteComponent::TaskBoard as owned.");
        return world_.task_board();
    }

    [[nodiscard]] const ModifierState& read_modifier() const noexcept
    {
        static_assert(
            site_system_reads_component<SystemTag>(SiteComponent::Modifier),
            "System must declare SiteComponent::Modifier as readable.");
        return world_.modifier();
    }

    [[nodiscard]] ModifierState& own_modifier() noexcept
    {
        static_assert(
            site_system_owns_component<SystemTag>(SiteComponent::Modifier),
            "System must declare SiteComponent::Modifier as owned.");
        return world_.modifier();
    }

    [[nodiscard]] const EconomyState& read_economy() const noexcept
    {
        static_assert(
            site_system_reads_component<SystemTag>(SiteComponent::Economy),
            "System must declare SiteComponent::Economy as readable.");
        return world_.economy();
    }

    [[nodiscard]] EconomyState& own_economy() noexcept
    {
        static_assert(
            site_system_owns_component<SystemTag>(SiteComponent::Economy),
            "System must declare SiteComponent::Economy as owned.");
        return world_.economy();
    }

    [[nodiscard]] const ActionState& read_action() const noexcept
    {
        static_assert(
            site_system_reads_component<SystemTag>(SiteComponent::Action),
            "System must declare SiteComponent::Action as readable.");
        return world_.action();
    }

    [[nodiscard]] ActionState& own_action() noexcept
    {
        static_assert(
            site_system_owns_component<SystemTag>(SiteComponent::Action),
            "System must declare SiteComponent::Action as owned.");
        return world_.action();
    }

    [[nodiscard]] const SiteCounters& read_counters() const noexcept
    {
        static_assert(
            site_system_reads_component<SystemTag>(SiteComponent::Counters),
            "System must declare SiteComponent::Counters as readable.");
        return world_.counters();
    }

    [[nodiscard]] SiteCounters& own_counters() noexcept
    {
        static_assert(
            site_system_owns_component<SystemTag>(SiteComponent::Counters),
            "System must declare SiteComponent::Counters as owned.");
        return world_.counters();
    }

    void mark_projection_dirty(std::uint64_t dirty_flags) noexcept
    {
        world_.mark_projection_dirty(dirty_flags);
    }

    void mark_tile_projection_dirty(TileCoord coord) noexcept
    {
        world_.mark_tile_projection_dirty(coord);
    }

private:
    SiteWorld world_;
};
}  // namespace gs1
