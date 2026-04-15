#pragma once

#include "commands/game_command.h"
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

    [[nodiscard]] const CampState& read_camp() const noexcept { return site_run_.camp; }
    [[nodiscard]] CampState& own_camp() noexcept { return site_run_.camp; }

    [[nodiscard]] const InventoryState& read_inventory() const noexcept { return site_run_.inventory; }
    [[nodiscard]] InventoryState& own_inventory() noexcept { return site_run_.inventory; }

    [[nodiscard]] const ContractorState& read_contractor() const noexcept { return site_run_.contractor; }
    [[nodiscard]] ContractorState& own_contractor() noexcept { return site_run_.contractor; }

    [[nodiscard]] const WeatherState& read_weather() const noexcept { return site_run_.weather; }
    [[nodiscard]] WeatherState& own_weather() noexcept { return site_run_.weather; }

    [[nodiscard]] const EventState& read_event() const noexcept { return site_run_.event; }
    [[nodiscard]] EventState& own_event() noexcept { return site_run_.event; }

    [[nodiscard]] const TaskBoardState& read_task_board() const noexcept { return site_run_.task_board; }
    [[nodiscard]] TaskBoardState& own_task_board() noexcept { return site_run_.task_board; }

    [[nodiscard]] const ModifierState& read_modifier() const noexcept { return site_run_.modifier; }
    [[nodiscard]] ModifierState& own_modifier() noexcept { return site_run_.modifier; }

    [[nodiscard]] const EconomyState& read_economy() const noexcept { return site_run_.economy; }
    [[nodiscard]] EconomyState& own_economy() noexcept { return site_run_.economy; }

    [[nodiscard]] const ActionState& read_action() const noexcept { return site_run_.site_action; }
    [[nodiscard]] ActionState& own_action() noexcept { return site_run_.site_action; }

    [[nodiscard]] const SiteCounters& read_counters() const noexcept { return site_run_.counters; }
    [[nodiscard]] SiteCounters& own_counters() noexcept { return site_run_.counters; }

    void mark_projection_dirty(std::uint64_t dirty_flags) noexcept
    {
        if ((dirty_flags & SITE_PROJECTION_UPDATE_TILES) != 0U)
        {
            site_run_.pending_full_tile_projection_update = true;
        }
        if ((dirty_flags & SITE_PROJECTION_UPDATE_INVENTORY) != 0U)
        {
            site_run_.pending_full_inventory_projection_update = true;
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
        std::vector<std::uint32_t>* pending_updates = nullptr;
        std::vector<std::uint8_t>* pending_update_mask = nullptr;
        std::uint32_t slot_count = 0U;

        switch (container_kind)
        {
        case GS1_INVENTORY_CONTAINER_WORKER_PACK:
            pending_updates = &site_run_.pending_worker_pack_inventory_projection_updates;
            pending_update_mask = &site_run_.pending_worker_pack_inventory_projection_update_mask;
            slot_count = site_run_.inventory.worker_pack_slot_count;
            break;
        case GS1_INVENTORY_CONTAINER_CAMP_STORAGE:
            pending_updates = &site_run_.pending_camp_storage_inventory_projection_updates;
            pending_update_mask = &site_run_.pending_camp_storage_inventory_projection_update_mask;
            slot_count = site_run_.inventory.camp_storage_slot_count;
            break;
        default:
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
    SiteRunState& site_run_;
};

template <typename SystemTag>
struct SiteSystemContext final
{
    const CampaignState& campaign;
    SiteRunState& site_run;
    SiteWorldAccess<SystemTag> world;
    GameCommandQueue& command_queue;
    double fixed_step_seconds {0.0};
    SiteMoveDirectionInput move_direction {};
};

template <typename SystemTag>
[[nodiscard]] inline SiteSystemContext<SystemTag> make_site_system_context(
    const CampaignState& campaign,
    SiteRunState& site_run,
    GameCommandQueue& command_queue,
    double fixed_step_seconds,
    SiteMoveDirectionInput move_direction)
{
    return SiteSystemContext<SystemTag> {
        campaign,
        site_run,
        SiteWorldAccess<SystemTag> {site_run},
        command_queue,
        fixed_step_seconds,
        move_direction};
}
}  // namespace gs1
