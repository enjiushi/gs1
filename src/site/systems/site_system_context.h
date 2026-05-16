#pragma once

#include "messages/game_message.h"
#include "runtime/runtime_state_access.h"
#include "site/inventory_storage.h"
#include "site/site_run_state.h"
#include "site/site_world_access.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <utility>

namespace gs1
{
struct CampaignState;
struct ConstInventoryStateRef;
struct ConstTaskBoardStateRef;
struct ConstActionStateRef;

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

struct ModifierStateRef final
{
    std::vector<ModifierId>& active_nearby_aura_modifier_ids;
    std::vector<ActiveSiteModifierState>& active_site_modifiers;
    ModifierChannelTotals& resolved_channel_totals;
    TerrainFactorModifierState& resolved_terrain_factor_modifiers;
    ActionCostModifierState& resolved_action_cost_modifiers;
    HarvestOutputModifierState& resolved_harvest_output_modifiers;
    VillageTechnologyEffectState& resolved_village_technology_effects;
    BureauTechnologyEffectState& resolved_bureau_technology_effects;
};

struct ConstModifierStateRef final
{
    const std::vector<ModifierId>& active_nearby_aura_modifier_ids;
    const std::vector<ActiveSiteModifierState>& active_site_modifiers;
    const ModifierChannelTotals& resolved_channel_totals;
    const TerrainFactorModifierState& resolved_terrain_factor_modifiers;
    const ActionCostModifierState& resolved_action_cost_modifiers;
    const HarvestOutputModifierState& resolved_harvest_output_modifiers;
    const VillageTechnologyEffectState& resolved_village_technology_effects;
    const BureauTechnologyEffectState& resolved_bureau_technology_effects;
};

struct EconomyStateRef final
{
    std::int32_t& current_cash;
    std::int32_t& phone_delivery_fee;
    std::uint16_t& phone_delivery_minutes;
    std::uint64_t& phone_listing_source_membership_revision;
    std::uint64_t& phone_listing_source_quantity_revision;
    std::uint64_t& phone_listing_source_action_reservation_signature;
    std::uint32_t& phone_buy_stock_refresh_generation;
    std::vector<std::uint32_t>& revealed_site_unlockable_ids;
    std::vector<std::uint32_t>& direct_purchase_unlockable_ids;
    std::vector<PhoneListingState>& available_phone_listings;
};

struct ConstEconomyStateRef final
{
    const std::int32_t& current_cash;
    const std::int32_t& phone_delivery_fee;
    const std::uint16_t& phone_delivery_minutes;
    const std::uint64_t& phone_listing_source_membership_revision;
    const std::uint64_t& phone_listing_source_quantity_revision;
    const std::uint64_t& phone_listing_source_action_reservation_signature;
    const std::uint32_t& phone_buy_stock_refresh_generation;
    const std::vector<std::uint32_t>& revealed_site_unlockable_ids;
    const std::vector<std::uint32_t>& direct_purchase_unlockable_ids;
    const std::vector<PhoneListingState>& available_phone_listings;
};

struct InventoryStateRef final
{
    std::uint32_t& next_storage_id;
    std::uint32_t& worker_pack_slot_count;
    std::uint32_t& worker_pack_storage_id;
    std::uint64_t& worker_pack_container_entity_id;
    std::uint64_t& item_membership_revision;
    std::uint64_t& item_quantity_revision;
    std::vector<StorageContainerEntryState>& storage_containers;
    std::vector<std::uint64_t>& storage_slot_item_instance_ids;
    std::vector<InventorySlot>& worker_pack_slots;
    std::vector<PendingDeliveryEntryState>& pending_deliveries;
    std::vector<InventorySlot>& pending_delivery_item_stacks;

    [[nodiscard]] operator ConstInventoryStateRef() const noexcept;
};

struct ConstInventoryStateRef final
{
    const std::uint32_t& next_storage_id;
    const std::uint32_t& worker_pack_slot_count;
    const std::uint32_t& worker_pack_storage_id;
    const std::uint64_t& worker_pack_container_entity_id;
    const std::uint64_t& item_membership_revision;
    const std::uint64_t& item_quantity_revision;
    const std::vector<StorageContainerEntryState>& storage_containers;
    const std::vector<std::uint64_t>& storage_slot_item_instance_ids;
    const std::vector<InventorySlot>& worker_pack_slots;
    const std::vector<PendingDeliveryEntryState>& pending_deliveries;
    const std::vector<InventorySlot>& pending_delivery_item_stacks;
};

struct TaskBoardStateRef final
{
    std::vector<TaskInstanceEntryState>& visible_tasks;
    std::vector<TaskRewardDraftOption>& reward_draft_options;
    std::vector<TaskTrackedTileState>& tracked_tiles;
    std::vector<TaskInstanceId>& accepted_task_ids;
    std::vector<TaskInstanceId>& completed_task_ids;
    std::vector<TaskInstanceId>& claimed_task_ids;
    std::uint32_t& task_pool_size;
    std::uint32_t& accepted_task_cap;
    std::uint32_t& tracked_tile_width;
    std::uint32_t& tracked_tile_height;
    std::uint32_t& fully_grown_tile_count;
    std::uint32_t& site_completion_tile_threshold;
    std::uint32_t& tracked_living_plant_count;
    std::uint32_t& refresh_generation;
    double& minutes_until_next_refresh;
    TaskTrackedWorkerState& worker;
    std::uint32_t& active_chain_task_chain_id;
    std::uint32_t& active_chain_current_accepted_step_index;
    TaskInstanceId& active_chain_surfaced_follow_up_task_instance_id;
    bool& all_tracked_living_plants_stable;
    bool& has_active_chain_state;
    bool& active_chain_has_current_accepted_step_index;
    bool& active_chain_has_surfaced_follow_up_task_instance_id;
    bool& active_chain_is_broken;

    [[nodiscard]] operator ConstTaskBoardStateRef() const noexcept;
};

struct ConstTaskBoardStateRef final
{
    const std::vector<TaskInstanceEntryState>& visible_tasks;
    const std::vector<TaskRewardDraftOption>& reward_draft_options;
    const std::vector<TaskTrackedTileState>& tracked_tiles;
    const std::vector<TaskInstanceId>& accepted_task_ids;
    const std::vector<TaskInstanceId>& completed_task_ids;
    const std::vector<TaskInstanceId>& claimed_task_ids;
    const std::uint32_t& task_pool_size;
    const std::uint32_t& accepted_task_cap;
    const std::uint32_t& tracked_tile_width;
    const std::uint32_t& tracked_tile_height;
    const std::uint32_t& fully_grown_tile_count;
    const std::uint32_t& site_completion_tile_threshold;
    const std::uint32_t& tracked_living_plant_count;
    const std::uint32_t& refresh_generation;
    const double& minutes_until_next_refresh;
    const TaskTrackedWorkerState& worker;
    const std::uint32_t& active_chain_task_chain_id;
    const std::uint32_t& active_chain_current_accepted_step_index;
    const TaskInstanceId& active_chain_surfaced_follow_up_task_instance_id;
    const bool& all_tracked_living_plants_stable;
    const bool& has_active_chain_state;
    const bool& active_chain_has_current_accepted_step_index;
    const bool& active_chain_has_surfaced_follow_up_task_instance_id;
    const bool& active_chain_is_broken;
};

struct ActionStateRef final
{
    RuntimeActionId& current_action_id;
    ActionKind& action_kind;
    TileCoord& target_tile;
    TileCoord& approach_tile;
    std::uint32_t& primary_subject_id;
    std::uint32_t& secondary_subject_id;
    std::uint32_t& item_id;
    std::uint32_t& placement_reservation_token;
    std::uint16_t& quantity;
    std::uint8_t& request_flags;
    bool& awaiting_placement_reservation;
    bool& reactivate_placement_mode_on_completion;
    double& total_action_minutes;
    double& remaining_action_minutes;
    DeferredWorkerMeterDelta& deferred_meter_delta;
    float& resolved_harvest_density;
    bool& resolved_harvest_outputs_valid;
    double& started_at_world_minute;
    PlacementModeMetaState& placement_mode;
    bool& has_current_action_id;
    bool& has_target_tile;
    bool& has_approach_tile;
    bool& has_started_at_world_minute;
    std::vector<ReservedItemStack>& reserved_input_item_stacks;
    std::vector<ResolvedHarvestOutputStack>& resolved_harvest_outputs;

    [[nodiscard]] operator ConstActionStateRef() const noexcept;
};

struct ConstActionStateRef final
{
    const RuntimeActionId& current_action_id;
    const ActionKind& action_kind;
    const TileCoord& target_tile;
    const TileCoord& approach_tile;
    const std::uint32_t& primary_subject_id;
    const std::uint32_t& secondary_subject_id;
    const std::uint32_t& item_id;
    const std::uint32_t& placement_reservation_token;
    const std::uint16_t& quantity;
    const std::uint8_t& request_flags;
    const bool& awaiting_placement_reservation;
    const bool& reactivate_placement_mode_on_completion;
    const double& total_action_minutes;
    const double& remaining_action_minutes;
    const DeferredWorkerMeterDelta& deferred_meter_delta;
    const float& resolved_harvest_density;
    const bool& resolved_harvest_outputs_valid;
    const double& started_at_world_minute;
    const PlacementModeMetaState& placement_mode;
    const bool& has_current_action_id;
    const bool& has_target_tile;
    const bool& has_approach_tile;
    const bool& has_started_at_world_minute;
    const std::vector<ReservedItemStack>& reserved_input_item_stacks;
    const std::vector<ResolvedHarvestOutputStack>& resolved_harvest_outputs;
};

inline InventoryStateRef::operator ConstInventoryStateRef() const noexcept
{
    return ConstInventoryStateRef {
        next_storage_id,
        worker_pack_slot_count,
        worker_pack_storage_id,
        worker_pack_container_entity_id,
        item_membership_revision,
        item_quantity_revision,
        storage_containers,
        storage_slot_item_instance_ids,
        worker_pack_slots,
        pending_deliveries,
        pending_delivery_item_stacks};
}

inline TaskBoardStateRef::operator ConstTaskBoardStateRef() const noexcept
{
    return ConstTaskBoardStateRef {
        visible_tasks,
        reward_draft_options,
        tracked_tiles,
        accepted_task_ids,
        completed_task_ids,
        claimed_task_ids,
        task_pool_size,
        accepted_task_cap,
        tracked_tile_width,
        tracked_tile_height,
        fully_grown_tile_count,
        site_completion_tile_threshold,
        tracked_living_plant_count,
        refresh_generation,
        minutes_until_next_refresh,
        worker,
        active_chain_task_chain_id,
        active_chain_current_accepted_step_index,
        active_chain_surfaced_follow_up_task_instance_id,
        all_tracked_living_plants_stable,
        has_active_chain_state,
        active_chain_has_current_accepted_step_index,
        active_chain_has_surfaced_follow_up_task_instance_id,
        active_chain_is_broken};
}

inline ActionStateRef::operator ConstActionStateRef() const noexcept
{
    return ConstActionStateRef {
        current_action_id,
        action_kind,
        target_tile,
        approach_tile,
        primary_subject_id,
        secondary_subject_id,
        item_id,
        placement_reservation_token,
        quantity,
        request_flags,
        awaiting_placement_reservation,
        reactivate_placement_mode_on_completion,
        total_action_minutes,
        remaining_action_minutes,
        deferred_meter_delta,
        resolved_harvest_density,
        resolved_harvest_outputs_valid,
        started_at_world_minute,
        placement_mode,
        has_current_action_id,
        has_target_tile,
        has_approach_tile,
        has_started_at_world_minute,
        reserved_input_item_stacks,
        resolved_harvest_outputs};
}

struct SiteObjectiveStateRef final
{
    SiteObjectiveType& type;
    double& time_limit_minutes;
    double& completion_hold_minutes;
    double& completion_hold_progress_minutes;
    double& paused_main_timer_minutes;
    double& last_evaluated_world_time_minutes;
    std::int32_t& target_cash_points;
    SiteObjectiveTargetEdge& target_edge;
    std::uint8_t& target_band_width;
    float& highway_max_average_sand_cover;
    float& last_target_average_sand_level;
    bool& has_hold_baseline;
    std::vector<std::uint32_t>& target_tile_indices;
    std::vector<std::uint8_t>& target_tile_mask;
    std::vector<std::uint32_t>& connection_start_tile_indices;
    std::vector<std::uint8_t>& connection_start_tile_mask;
    std::vector<std::uint32_t>& connection_goal_tile_indices;
    std::vector<std::uint8_t>& connection_goal_tile_mask;
};

struct ConstSiteObjectiveStateRef final
{
    const SiteObjectiveType& type;
    const double& time_limit_minutes;
    const double& completion_hold_minutes;
    const double& completion_hold_progress_minutes;
    const double& paused_main_timer_minutes;
    const double& last_evaluated_world_time_minutes;
    const std::int32_t& target_cash_points;
    const SiteObjectiveTargetEdge& target_edge;
    const std::uint8_t& target_band_width;
    const float& highway_max_average_sand_cover;
    const float& last_target_average_sand_level;
    const bool& has_hold_baseline;
    const std::vector<std::uint32_t>& target_tile_indices;
    const std::vector<std::uint8_t>& target_tile_mask;
    const std::vector<std::uint32_t>& connection_start_tile_indices;
    const std::vector<std::uint8_t>& connection_start_tile_mask;
    const std::vector<std::uint32_t>& connection_goal_tile_indices;
    const std::vector<std::uint8_t>& connection_goal_tile_mask;
};

template <typename SystemTag>
class SiteWorldAccess final
{
public:
    explicit SiteWorldAccess(RuntimeInvocation& invocation) noexcept
        : invocation_(&invocation)
    {
    }

    [[nodiscard]] SiteRunId site_run_id() const noexcept
    {
        return invocation_->state_manager()->query<StateSetId::SiteRunMeta>(*invocation_->owned_state())->site_run_id;
    }

    [[nodiscard]] SiteId site_id() const noexcept
    {
        return invocation_->state_manager()->query<StateSetId::SiteRunMeta>(*invocation_->owned_state())->site_id;
    }

    [[nodiscard]] std::uint32_t site_id_value() const noexcept
    {
        return site_id().value;
    }

    [[nodiscard]] std::uint32_t site_archetype_id() const noexcept
    {
        return invocation_->state_manager()->query<StateSetId::SiteRunMeta>(*invocation_->owned_state())
            ->site_archetype_id;
    }

    [[nodiscard]] std::uint32_t attempt_index() const noexcept
    {
        return invocation_->state_manager()->query<StateSetId::SiteRunMeta>(*invocation_->owned_state())
            ->attempt_index;
    }

    [[nodiscard]] std::uint64_t site_attempt_seed() const noexcept
    {
        return invocation_->state_manager()->query<StateSetId::SiteRunMeta>(*invocation_->owned_state())
            ->site_attempt_seed;
    }

    [[nodiscard]] SiteRunStatus run_status() const noexcept
    {
        return invocation_->state_manager()->query<StateSetId::SiteRunMeta>(*invocation_->owned_state())->run_status;
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

    [[nodiscard]] ConstInventoryStateRef read_inventory() const noexcept
    {
        return ConstInventoryStateRef {
            inventory_meta_state().next_storage_id,
            inventory_meta_state().worker_pack_slot_count,
            inventory_meta_state().worker_pack_storage_id,
            inventory_meta_state().worker_pack_container_entity_id,
            inventory_meta_state().item_membership_revision,
            inventory_meta_state().item_quantity_revision,
            inventory_storage_containers_state(),
            inventory_storage_slot_item_ids_state(),
            inventory_worker_pack_slots_state(),
            inventory_pending_deliveries_state(),
            inventory_pending_delivery_item_stacks_state()};
    }
    [[nodiscard]] InventoryStateRef own_inventory() noexcept
    {
        return InventoryStateRef {
            inventory_meta_state().next_storage_id,
            inventory_meta_state().worker_pack_slot_count,
            inventory_meta_state().worker_pack_storage_id,
            inventory_meta_state().worker_pack_container_entity_id,
            inventory_meta_state().item_membership_revision,
            inventory_meta_state().item_quantity_revision,
            inventory_storage_containers_state(),
            inventory_storage_slot_item_ids_state(),
            inventory_worker_pack_slots_state(),
            inventory_pending_deliveries_state(),
            inventory_pending_delivery_item_stacks_state()};
    }

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

    [[nodiscard]] const LocalWeatherResolveMetaState& read_local_weather_runtime_meta() const noexcept
    {
        return local_weather_runtime_meta_state();
    }
    [[nodiscard]] LocalWeatherResolveMetaState& own_local_weather_runtime_meta() noexcept
    {
        return local_weather_runtime_meta_state();
    }
    [[nodiscard]] const std::vector<SiteWorld::TileWeatherContributionData>&
    read_local_weather_last_total_contributions() const noexcept
    {
        return local_weather_last_total_contributions_state();
    }
    [[nodiscard]] std::vector<SiteWorld::TileWeatherContributionData>&
    own_local_weather_last_total_contributions() noexcept
    {
        return local_weather_last_total_contributions_state();
    }

    [[nodiscard]] const PlantWeatherContributionMetaState& read_plant_weather_runtime_meta() const noexcept
    {
        return plant_weather_runtime_meta_state();
    }
    [[nodiscard]] PlantWeatherContributionMetaState& own_plant_weather_runtime_meta() noexcept
    {
        return plant_weather_runtime_meta_state();
    }
    [[nodiscard]] const std::vector<std::uint32_t>& read_plant_weather_dirty_tile_indices() const noexcept
    {
        return plant_weather_dirty_tile_indices_state();
    }
    [[nodiscard]] std::vector<std::uint32_t>& own_plant_weather_dirty_tile_indices() noexcept
    {
        return plant_weather_dirty_tile_indices_state();
    }
    [[nodiscard]] const std::vector<std::uint8_t>& read_plant_weather_dirty_tile_mask() const noexcept
    {
        return plant_weather_dirty_tile_mask_state();
    }
    [[nodiscard]] std::vector<std::uint8_t>& own_plant_weather_dirty_tile_mask() noexcept
    {
        return plant_weather_dirty_tile_mask_state();
    }

    [[nodiscard]] const DeviceWeatherContributionMetaState& read_device_weather_runtime_meta() const noexcept
    {
        return device_weather_runtime_meta_state();
    }
    [[nodiscard]] DeviceWeatherContributionMetaState& own_device_weather_runtime_meta() noexcept
    {
        return device_weather_runtime_meta_state();
    }
    [[nodiscard]] const std::vector<std::uint32_t>& read_device_weather_dirty_tile_indices() const noexcept
    {
        return device_weather_dirty_tile_indices_state();
    }
    [[nodiscard]] std::vector<std::uint32_t>& own_device_weather_dirty_tile_indices() noexcept
    {
        return device_weather_dirty_tile_indices_state();
    }
    [[nodiscard]] const std::vector<std::uint8_t>& read_device_weather_dirty_tile_mask() const noexcept
    {
        return device_weather_dirty_tile_mask_state();
    }
    [[nodiscard]] std::vector<std::uint8_t>& own_device_weather_dirty_tile_mask() noexcept
    {
        return device_weather_dirty_tile_mask_state();
    }

    [[nodiscard]] const EventState& read_event() const noexcept { return event_state(); }
    [[nodiscard]] EventState& own_event() noexcept { return event_state(); }

    [[nodiscard]] ConstTaskBoardStateRef read_task_board() const noexcept
    {
        return ConstTaskBoardStateRef {
            task_board_visible_tasks_state(),
            task_board_reward_draft_options_state(),
            task_board_tracked_tiles_state(),
            task_board_accepted_task_ids_state(),
            task_board_completed_task_ids_state(),
            task_board_claimed_task_ids_state(),
            task_board_meta_state().task_pool_size,
            task_board_meta_state().accepted_task_cap,
            task_board_meta_state().tracked_tile_width,
            task_board_meta_state().tracked_tile_height,
            task_board_meta_state().fully_grown_tile_count,
            task_board_meta_state().site_completion_tile_threshold,
            task_board_meta_state().tracked_living_plant_count,
            task_board_meta_state().refresh_generation,
            task_board_meta_state().minutes_until_next_refresh,
            task_board_meta_state().worker,
            task_board_meta_state().active_chain_task_chain_id,
            task_board_meta_state().active_chain_current_accepted_step_index,
            task_board_meta_state().active_chain_surfaced_follow_up_task_instance_id,
            task_board_meta_state().all_tracked_living_plants_stable,
            task_board_meta_state().has_active_chain_state,
            task_board_meta_state().active_chain_has_current_accepted_step_index,
            task_board_meta_state().active_chain_has_surfaced_follow_up_task_instance_id,
            task_board_meta_state().active_chain_is_broken};
    }
    [[nodiscard]] TaskBoardStateRef own_task_board() noexcept
    {
        return TaskBoardStateRef {
            task_board_visible_tasks_state(),
            task_board_reward_draft_options_state(),
            task_board_tracked_tiles_state(),
            task_board_accepted_task_ids_state(),
            task_board_completed_task_ids_state(),
            task_board_claimed_task_ids_state(),
            task_board_meta_state().task_pool_size,
            task_board_meta_state().accepted_task_cap,
            task_board_meta_state().tracked_tile_width,
            task_board_meta_state().tracked_tile_height,
            task_board_meta_state().fully_grown_tile_count,
            task_board_meta_state().site_completion_tile_threshold,
            task_board_meta_state().tracked_living_plant_count,
            task_board_meta_state().refresh_generation,
            task_board_meta_state().minutes_until_next_refresh,
            task_board_meta_state().worker,
            task_board_meta_state().active_chain_task_chain_id,
            task_board_meta_state().active_chain_current_accepted_step_index,
            task_board_meta_state().active_chain_surfaced_follow_up_task_instance_id,
            task_board_meta_state().all_tracked_living_plants_stable,
            task_board_meta_state().has_active_chain_state,
            task_board_meta_state().active_chain_has_current_accepted_step_index,
            task_board_meta_state().active_chain_has_surfaced_follow_up_task_instance_id,
            task_board_meta_state().active_chain_is_broken};
    }

    [[nodiscard]] const ModifierChannelTotals& read_modifier_channel_totals() const noexcept
    {
        return modifier_meta_state().resolved_channel_totals;
    }
    [[nodiscard]] ModifierChannelTotals& own_modifier_channel_totals() noexcept
    {
        return modifier_meta_state().resolved_channel_totals;
    }
    [[nodiscard]] const TerrainFactorModifierState& read_modifier_terrain_factor_modifiers() const noexcept
    {
        return modifier_meta_state().resolved_terrain_factor_modifiers;
    }
    [[nodiscard]] TerrainFactorModifierState& own_modifier_terrain_factor_modifiers() noexcept
    {
        return modifier_meta_state().resolved_terrain_factor_modifiers;
    }
    [[nodiscard]] const ActionCostModifierState& read_modifier_action_cost_modifiers() const noexcept
    {
        return modifier_meta_state().resolved_action_cost_modifiers;
    }
    [[nodiscard]] ActionCostModifierState& own_modifier_action_cost_modifiers() noexcept
    {
        return modifier_meta_state().resolved_action_cost_modifiers;
    }
    [[nodiscard]] const HarvestOutputModifierState& read_modifier_harvest_output_modifiers() const noexcept
    {
        return modifier_meta_state().resolved_harvest_output_modifiers;
    }
    [[nodiscard]] HarvestOutputModifierState& own_modifier_harvest_output_modifiers() noexcept
    {
        return modifier_meta_state().resolved_harvest_output_modifiers;
    }
    [[nodiscard]] const VillageTechnologyEffectState& read_modifier_village_technology_effects() const noexcept
    {
        return modifier_meta_state().resolved_village_technology_effects;
    }
    [[nodiscard]] VillageTechnologyEffectState& own_modifier_village_technology_effects() noexcept
    {
        return modifier_meta_state().resolved_village_technology_effects;
    }
    [[nodiscard]] const BureauTechnologyEffectState& read_modifier_bureau_technology_effects() const noexcept
    {
        return modifier_meta_state().resolved_bureau_technology_effects;
    }
    [[nodiscard]] BureauTechnologyEffectState& own_modifier_bureau_technology_effects() noexcept
    {
        return modifier_meta_state().resolved_bureau_technology_effects;
    }
    [[nodiscard]] const std::vector<ModifierId>& read_modifier_nearby_aura_ids() const noexcept
    {
        return modifier_nearby_aura_ids_state();
    }
    [[nodiscard]] std::vector<ModifierId>& own_modifier_nearby_aura_ids() noexcept
    {
        return modifier_nearby_aura_ids_state();
    }
    [[nodiscard]] const std::vector<ActiveSiteModifierState>& read_active_site_modifiers() const noexcept
    {
        return active_site_modifiers_state();
    }
    [[nodiscard]] std::vector<ActiveSiteModifierState>& own_active_site_modifiers() noexcept
    {
        return active_site_modifiers_state();
    }
    [[nodiscard]] ConstModifierStateRef read_modifier() const noexcept
    {
        return ConstModifierStateRef {
            modifier_nearby_aura_ids_state(),
            active_site_modifiers_state(),
            modifier_meta_state().resolved_channel_totals,
            modifier_meta_state().resolved_terrain_factor_modifiers,
            modifier_meta_state().resolved_action_cost_modifiers,
            modifier_meta_state().resolved_harvest_output_modifiers,
            modifier_meta_state().resolved_village_technology_effects,
            modifier_meta_state().resolved_bureau_technology_effects};
    }
    [[nodiscard]] ModifierStateRef own_modifier() noexcept
    {
        return ModifierStateRef {
            modifier_nearby_aura_ids_state(),
            active_site_modifiers_state(),
            modifier_meta_state().resolved_channel_totals,
            modifier_meta_state().resolved_terrain_factor_modifiers,
            modifier_meta_state().resolved_action_cost_modifiers,
            modifier_meta_state().resolved_harvest_output_modifiers,
            modifier_meta_state().resolved_village_technology_effects,
            modifier_meta_state().resolved_bureau_technology_effects};
    }

    [[nodiscard]] const EconomyMetaState& read_economy_meta() const noexcept { return economy_meta_state(); }
    [[nodiscard]] EconomyMetaState& own_economy_meta() noexcept { return economy_meta_state(); }
    [[nodiscard]] const std::vector<std::uint32_t>& read_economy_revealed_unlockable_ids() const noexcept
    {
        return economy_revealed_unlockable_ids_state();
    }
    [[nodiscard]] std::vector<std::uint32_t>& own_economy_revealed_unlockable_ids() noexcept
    {
        return economy_revealed_unlockable_ids_state();
    }
    [[nodiscard]] const std::vector<std::uint32_t>& read_economy_direct_purchase_unlockable_ids() const noexcept
    {
        return economy_direct_purchase_unlockable_ids_state();
    }
    [[nodiscard]] std::vector<std::uint32_t>& own_economy_direct_purchase_unlockable_ids() noexcept
    {
        return economy_direct_purchase_unlockable_ids_state();
    }
    [[nodiscard]] const std::vector<PhoneListingState>& read_economy_phone_listings() const noexcept
    {
        return economy_phone_listings_state();
    }
    [[nodiscard]] std::vector<PhoneListingState>& own_economy_phone_listings() noexcept
    {
        return economy_phone_listings_state();
    }
    [[nodiscard]] ConstEconomyStateRef read_economy() const noexcept
    {
        return ConstEconomyStateRef {
            economy_meta_state().current_cash,
            economy_meta_state().phone_delivery_fee,
            economy_meta_state().phone_delivery_minutes,
            economy_meta_state().phone_listing_source_membership_revision,
            economy_meta_state().phone_listing_source_quantity_revision,
            economy_meta_state().phone_listing_source_action_reservation_signature,
            economy_meta_state().phone_buy_stock_refresh_generation,
            economy_revealed_unlockable_ids_state(),
            economy_direct_purchase_unlockable_ids_state(),
            economy_phone_listings_state()};
    }
    [[nodiscard]] EconomyStateRef own_economy() noexcept
    {
        return EconomyStateRef {
            economy_meta_state().current_cash,
            economy_meta_state().phone_delivery_fee,
            economy_meta_state().phone_delivery_minutes,
            economy_meta_state().phone_listing_source_membership_revision,
            economy_meta_state().phone_listing_source_quantity_revision,
            economy_meta_state().phone_listing_source_action_reservation_signature,
            economy_meta_state().phone_buy_stock_refresh_generation,
            economy_revealed_unlockable_ids_state(),
            economy_direct_purchase_unlockable_ids_state(),
            economy_phone_listings_state()};
    }

    [[nodiscard]] const CraftDeviceCacheRuntimeState& read_craft_device_cache_runtime() const noexcept
    {
        return craft_device_cache_runtime_state();
    }
    [[nodiscard]] CraftDeviceCacheRuntimeState& own_craft_device_cache_runtime() noexcept
    {
        return craft_device_cache_runtime_state();
    }

    [[nodiscard]] const std::vector<CraftDeviceCacheEntryState>& read_craft_device_caches() const noexcept
    {
        return craft_device_caches_state();
    }
    [[nodiscard]] std::vector<CraftDeviceCacheEntryState>& own_craft_device_caches() noexcept
    {
        return craft_device_caches_state();
    }

    [[nodiscard]] const std::vector<std::uint64_t>& read_craft_nearby_items() const noexcept
    {
        return craft_nearby_items_state();
    }
    [[nodiscard]] std::vector<std::uint64_t>& own_craft_nearby_items() noexcept
    {
        return craft_nearby_items_state();
    }

    [[nodiscard]] const PhoneInventoryCacheMetaState& read_craft_phone_cache_meta() const noexcept
    {
        return craft_phone_cache_meta_state();
    }
    [[nodiscard]] PhoneInventoryCacheMetaState& own_craft_phone_cache_meta() noexcept
    {
        return craft_phone_cache_meta_state();
    }

    [[nodiscard]] const std::vector<std::uint64_t>& read_craft_phone_items() const noexcept
    {
        return craft_phone_items_state();
    }
    [[nodiscard]] std::vector<std::uint64_t>& own_craft_phone_items() noexcept
    {
        return craft_phone_items_state();
    }

    [[nodiscard]] const CraftContextMetaState& read_craft_context_meta() const noexcept
    {
        return craft_context_meta_state();
    }
    [[nodiscard]] CraftContextMetaState& own_craft_context_meta() noexcept
    {
        return craft_context_meta_state();
    }

    [[nodiscard]] const std::vector<CraftContextOptionState>& read_craft_context_options() const noexcept
    {
        return craft_context_options_state();
    }
    [[nodiscard]] std::vector<CraftContextOptionState>& own_craft_context_options() noexcept
    {
        return craft_context_options_state();
    }

    [[nodiscard]] ConstActionStateRef read_action() const noexcept
    {
        return ConstActionStateRef {
            action_meta_state().current_action_id,
            action_meta_state().action_kind,
            action_meta_state().target_tile,
            action_meta_state().approach_tile,
            action_meta_state().primary_subject_id,
            action_meta_state().secondary_subject_id,
            action_meta_state().item_id,
            action_meta_state().placement_reservation_token,
            action_meta_state().quantity,
            action_meta_state().request_flags,
            action_meta_state().awaiting_placement_reservation,
            action_meta_state().reactivate_placement_mode_on_completion,
            action_meta_state().total_action_minutes,
            action_meta_state().remaining_action_minutes,
            action_meta_state().deferred_meter_delta,
            action_meta_state().resolved_harvest_density,
            action_meta_state().resolved_harvest_outputs_valid,
            action_meta_state().started_at_world_minute,
            action_meta_state().placement_mode,
            action_meta_state().has_current_action_id,
            action_meta_state().has_target_tile,
            action_meta_state().has_approach_tile,
            action_meta_state().has_started_at_world_minute,
            action_reserved_input_item_stacks_state(),
            action_resolved_harvest_outputs_state()};
    }
    [[nodiscard]] ActionStateRef own_action() noexcept
    {
        return ActionStateRef {
            action_meta_state().current_action_id,
            action_meta_state().action_kind,
            action_meta_state().target_tile,
            action_meta_state().approach_tile,
            action_meta_state().primary_subject_id,
            action_meta_state().secondary_subject_id,
            action_meta_state().item_id,
            action_meta_state().placement_reservation_token,
            action_meta_state().quantity,
            action_meta_state().request_flags,
            action_meta_state().awaiting_placement_reservation,
            action_meta_state().reactivate_placement_mode_on_completion,
            action_meta_state().total_action_minutes,
            action_meta_state().remaining_action_minutes,
            action_meta_state().deferred_meter_delta,
            action_meta_state().resolved_harvest_density,
            action_meta_state().resolved_harvest_outputs_valid,
            action_meta_state().started_at_world_minute,
            action_meta_state().placement_mode,
            action_meta_state().has_current_action_id,
            action_meta_state().has_target_tile,
            action_meta_state().has_approach_tile,
            action_meta_state().has_started_at_world_minute,
            action_reserved_input_item_stacks_state(),
            action_resolved_harvest_outputs_state()};
    }

    [[nodiscard]] const SiteCounters& read_counters() const noexcept { return counters_state(); }
    [[nodiscard]] SiteCounters& own_counters() noexcept { return counters_state(); }

    [[nodiscard]] const SiteObjectiveMetaState& read_objective_meta() const noexcept
    {
        return objective_meta_state();
    }
    [[nodiscard]] SiteObjectiveMetaState& own_objective_meta() noexcept
    {
        return objective_meta_state();
    }
    [[nodiscard]] const std::vector<std::uint32_t>& read_objective_target_tile_indices() const noexcept
    {
        return objective_target_tile_indices_state();
    }
    [[nodiscard]] std::vector<std::uint32_t>& own_objective_target_tile_indices() noexcept
    {
        return objective_target_tile_indices_state();
    }
    [[nodiscard]] const std::vector<std::uint8_t>& read_objective_target_tile_mask() const noexcept
    {
        return objective_target_tile_mask_state();
    }
    [[nodiscard]] std::vector<std::uint8_t>& own_objective_target_tile_mask() noexcept
    {
        return objective_target_tile_mask_state();
    }
    [[nodiscard]] const std::vector<std::uint32_t>& read_objective_connection_start_tile_indices()
        const noexcept
    {
        return objective_connection_start_tile_indices_state();
    }
    [[nodiscard]] std::vector<std::uint32_t>& own_objective_connection_start_tile_indices() noexcept
    {
        return objective_connection_start_tile_indices_state();
    }
    [[nodiscard]] const std::vector<std::uint8_t>& read_objective_connection_start_tile_mask() const noexcept
    {
        return objective_connection_start_tile_mask_state();
    }
    [[nodiscard]] std::vector<std::uint8_t>& own_objective_connection_start_tile_mask() noexcept
    {
        return objective_connection_start_tile_mask_state();
    }
    [[nodiscard]] const std::vector<std::uint32_t>& read_objective_connection_goal_tile_indices()
        const noexcept
    {
        return objective_connection_goal_tile_indices_state();
    }
    [[nodiscard]] std::vector<std::uint32_t>& own_objective_connection_goal_tile_indices() noexcept
    {
        return objective_connection_goal_tile_indices_state();
    }
    [[nodiscard]] const std::vector<std::uint8_t>& read_objective_connection_goal_tile_mask() const noexcept
    {
        return objective_connection_goal_tile_mask_state();
    }
    [[nodiscard]] std::vector<std::uint8_t>& own_objective_connection_goal_tile_mask() noexcept
    {
        return objective_connection_goal_tile_mask_state();
    }
    [[nodiscard]] ConstSiteObjectiveStateRef read_objective() const noexcept
    {
        return ConstSiteObjectiveStateRef {
            objective_meta_state().type,
            objective_meta_state().time_limit_minutes,
            objective_meta_state().completion_hold_minutes,
            objective_meta_state().completion_hold_progress_minutes,
            objective_meta_state().paused_main_timer_minutes,
            objective_meta_state().last_evaluated_world_time_minutes,
            objective_meta_state().target_cash_points,
            objective_meta_state().target_edge,
            objective_meta_state().target_band_width,
            objective_meta_state().highway_max_average_sand_cover,
            objective_meta_state().last_target_average_sand_level,
            objective_meta_state().has_hold_baseline,
            objective_target_tile_indices_state(),
            objective_target_tile_mask_state(),
            objective_connection_start_tile_indices_state(),
            objective_connection_start_tile_mask_state(),
            objective_connection_goal_tile_indices_state(),
            objective_connection_goal_tile_mask_state()};
    }
    [[nodiscard]] SiteObjectiveStateRef own_objective() noexcept
    {
        return SiteObjectiveStateRef {
            objective_meta_state().type,
            objective_meta_state().time_limit_minutes,
            objective_meta_state().completion_hold_minutes,
            objective_meta_state().completion_hold_progress_minutes,
            objective_meta_state().paused_main_timer_minutes,
            objective_meta_state().last_evaluated_world_time_minutes,
            objective_meta_state().target_cash_points,
            objective_meta_state().target_edge,
            objective_meta_state().target_band_width,
            objective_meta_state().highway_max_average_sand_cover,
            objective_meta_state().last_target_average_sand_level,
            objective_meta_state().has_hold_baseline,
            objective_target_tile_indices_state(),
            objective_target_tile_mask_state(),
            objective_connection_start_tile_indices_state(),
            objective_connection_start_tile_mask_state(),
            objective_connection_goal_tile_indices_state(),
            objective_connection_goal_tile_mask_state()};
    }

private:
    [[nodiscard]] SiteClockState& site_clock_state() const
    {
        return invocation_->state_manager()->state<StateSetId::SiteClock>(*invocation_->owned_state()).value();
    }

    [[nodiscard]] CampState& camp_state() const
    {
        return invocation_->state_manager()->state<StateSetId::SiteCamp>(*invocation_->owned_state()).value();
    }

    [[nodiscard]] InventoryMetaState& inventory_meta_state() const
    {
        return invocation_->state_manager()->state<StateSetId::SiteInventoryMeta>(*invocation_->owned_state())
            .value();
    }

    [[nodiscard]] WeatherState& weather_state() const
    {
        return invocation_->state_manager()->state<StateSetId::SiteWeather>(*invocation_->owned_state()).value();
    }

    [[nodiscard]] LocalWeatherResolveMetaState& local_weather_runtime_meta_state() const
    {
        return invocation_->state_manager()
            ->state<StateSetId::SiteLocalWeatherResolveMeta>(*invocation_->owned_state())
            .value();
    }

    [[nodiscard]] std::vector<SiteWorld::TileWeatherContributionData>&
    local_weather_last_total_contributions_state() const
    {
        return invocation_->state_manager()
            ->state<StateSetId::SiteLocalWeatherResolveLastTotalContributions>(*invocation_->owned_state())
            .value();
    }

    [[nodiscard]] PlantWeatherContributionMetaState& plant_weather_runtime_meta_state() const
    {
        return invocation_->state_manager()
            ->state<StateSetId::SitePlantWeatherContributionMeta>(*invocation_->owned_state())
            .value();
    }

    [[nodiscard]] std::vector<std::uint32_t>& plant_weather_dirty_tile_indices_state() const
    {
        return invocation_->state_manager()
            ->state<StateSetId::SitePlantWeatherContributionDirtyTileIndices>(*invocation_->owned_state())
            .value();
    }

    [[nodiscard]] std::vector<std::uint8_t>& plant_weather_dirty_tile_mask_state() const
    {
        return invocation_->state_manager()
            ->state<StateSetId::SitePlantWeatherContributionDirtyTileMask>(*invocation_->owned_state())
            .value();
    }

    [[nodiscard]] DeviceWeatherContributionMetaState& device_weather_runtime_meta_state() const
    {
        return invocation_->state_manager()
            ->state<StateSetId::SiteDeviceWeatherContributionMeta>(*invocation_->owned_state())
            .value();
    }

    [[nodiscard]] std::vector<std::uint32_t>& device_weather_dirty_tile_indices_state() const
    {
        return invocation_->state_manager()
            ->state<StateSetId::SiteDeviceWeatherContributionDirtyTileIndices>(*invocation_->owned_state())
            .value();
    }

    [[nodiscard]] std::vector<std::uint8_t>& device_weather_dirty_tile_mask_state() const
    {
        return invocation_->state_manager()
            ->state<StateSetId::SiteDeviceWeatherContributionDirtyTileMask>(*invocation_->owned_state())
            .value();
    }

    [[nodiscard]] EventState& event_state() const
    {
        return invocation_->state_manager()->state<StateSetId::SiteEvent>(*invocation_->owned_state()).value();
    }

    [[nodiscard]] TaskBoardMetaState& task_board_meta_state() const
    {
        return invocation_->state_manager()->state<StateSetId::SiteTaskBoardMeta>(*invocation_->owned_state())
            .value();
    }

    [[nodiscard]] ModifierMetaState& modifier_meta_state() const
    {
        return invocation_->state_manager()->state<StateSetId::SiteModifierMeta>(*invocation_->owned_state())
            .value();
    }

    [[nodiscard]] std::vector<ModifierId>& modifier_nearby_aura_ids_state() const
    {
        return invocation_->state_manager()
            ->state<StateSetId::SiteModifierNearbyAuraIds>(*invocation_->owned_state())
            .value();
    }

    [[nodiscard]] std::vector<ActiveSiteModifierState>& active_site_modifiers_state() const
    {
        return invocation_->state_manager()
            ->state<StateSetId::SiteModifierActiveSiteModifiers>(*invocation_->owned_state())
            .value();
    }

    [[nodiscard]] EconomyMetaState& economy_meta_state() const
    {
        return invocation_->state_manager()->state<StateSetId::SiteEconomyMeta>(*invocation_->owned_state())
            .value();
    }

    [[nodiscard]] std::vector<std::uint32_t>& economy_revealed_unlockable_ids_state() const
    {
        return invocation_->state_manager()
            ->state<StateSetId::SiteEconomyRevealedUnlockableIds>(*invocation_->owned_state())
            .value();
    }

    [[nodiscard]] std::vector<std::uint32_t>& economy_direct_purchase_unlockable_ids_state() const
    {
        return invocation_->state_manager()
            ->state<StateSetId::SiteEconomyDirectPurchaseUnlockableIds>(*invocation_->owned_state())
            .value();
    }

    [[nodiscard]] std::vector<PhoneListingState>& economy_phone_listings_state() const
    {
        return invocation_->state_manager()
            ->state<StateSetId::SiteEconomyPhoneListings>(*invocation_->owned_state())
            .value();
    }

    [[nodiscard]] CraftDeviceCacheRuntimeState& craft_device_cache_runtime_state() const
    {
        return invocation_->state_manager()
            ->state<StateSetId::SiteCraftDeviceCacheRuntime>(*invocation_->owned_state())
            .value();
    }

    [[nodiscard]] std::vector<CraftDeviceCacheEntryState>& craft_device_caches_state() const
    {
        return invocation_->state_manager()
            ->state<StateSetId::SiteCraftDeviceCaches>(*invocation_->owned_state())
            .value();
    }

    [[nodiscard]] std::vector<std::uint64_t>& craft_nearby_items_state() const
    {
        return invocation_->state_manager()
            ->state<StateSetId::SiteCraftNearbyItems>(*invocation_->owned_state())
            .value();
    }

    [[nodiscard]] PhoneInventoryCacheMetaState& craft_phone_cache_meta_state() const
    {
        return invocation_->state_manager()
            ->state<StateSetId::SiteCraftPhoneCacheMeta>(*invocation_->owned_state())
            .value();
    }

    [[nodiscard]] std::vector<std::uint64_t>& craft_phone_items_state() const
    {
        return invocation_->state_manager()->state<StateSetId::SiteCraftPhoneItems>(*invocation_->owned_state())
            .value();
    }

    [[nodiscard]] CraftContextMetaState& craft_context_meta_state() const
    {
        return invocation_->state_manager()->state<StateSetId::SiteCraftContextMeta>(*invocation_->owned_state())
            .value();
    }

    [[nodiscard]] std::vector<CraftContextOptionState>& craft_context_options_state() const
    {
        return invocation_->state_manager()
            ->state<StateSetId::SiteCraftContextOptions>(*invocation_->owned_state())
            .value();
    }

    [[nodiscard]] ActionMetaState& action_meta_state() const
    {
        return invocation_->state_manager()->state<StateSetId::SiteActionMeta>(*invocation_->owned_state()).value();
    }

    [[nodiscard]] std::vector<StorageContainerEntryState>& inventory_storage_containers_state() const
    {
        return invocation_->state_manager()
            ->state<StateSetId::SiteInventoryStorageContainers>(*invocation_->owned_state())
            .value();
    }

    [[nodiscard]] std::vector<std::uint64_t>& inventory_storage_slot_item_ids_state() const
    {
        return invocation_->state_manager()
            ->state<StateSetId::SiteInventoryStorageSlotItemIds>(*invocation_->owned_state())
            .value();
    }

    [[nodiscard]] std::vector<InventorySlot>& inventory_worker_pack_slots_state() const
    {
        return invocation_->state_manager()
            ->state<StateSetId::SiteInventoryWorkerPackSlots>(*invocation_->owned_state())
            .value();
    }

    [[nodiscard]] std::vector<PendingDeliveryEntryState>& inventory_pending_deliveries_state() const
    {
        return invocation_->state_manager()
            ->state<StateSetId::SiteInventoryPendingDeliveries>(*invocation_->owned_state())
            .value();
    }

    [[nodiscard]] std::vector<InventorySlot>& inventory_pending_delivery_item_stacks_state() const
    {
        return invocation_->state_manager()
            ->state<StateSetId::SiteInventoryPendingDeliveryItemStacks>(*invocation_->owned_state())
            .value();
    }

    [[nodiscard]] std::vector<TaskInstanceEntryState>& task_board_visible_tasks_state() const
    {
        return invocation_->state_manager()
            ->state<StateSetId::SiteTaskBoardVisibleTasks>(*invocation_->owned_state())
            .value();
    }

    [[nodiscard]] std::vector<TaskRewardDraftOption>& task_board_reward_draft_options_state() const
    {
        return invocation_->state_manager()
            ->state<StateSetId::SiteTaskBoardRewardDraftOptions>(*invocation_->owned_state())
            .value();
    }

    [[nodiscard]] std::vector<TaskTrackedTileState>& task_board_tracked_tiles_state() const
    {
        return invocation_->state_manager()
            ->state<StateSetId::SiteTaskBoardTrackedTiles>(*invocation_->owned_state())
            .value();
    }

    [[nodiscard]] std::vector<TaskInstanceId>& task_board_accepted_task_ids_state() const
    {
        return invocation_->state_manager()
            ->state<StateSetId::SiteTaskBoardAcceptedTaskIds>(*invocation_->owned_state())
            .value();
    }

    [[nodiscard]] std::vector<TaskInstanceId>& task_board_completed_task_ids_state() const
    {
        return invocation_->state_manager()
            ->state<StateSetId::SiteTaskBoardCompletedTaskIds>(*invocation_->owned_state())
            .value();
    }

    [[nodiscard]] std::vector<TaskInstanceId>& task_board_claimed_task_ids_state() const
    {
        return invocation_->state_manager()
            ->state<StateSetId::SiteTaskBoardClaimedTaskIds>(*invocation_->owned_state())
            .value();
    }

    [[nodiscard]] std::vector<ReservedItemStack>& action_reserved_input_item_stacks_state() const
    {
        return invocation_->state_manager()
            ->state<StateSetId::SiteActionReservedInputItemStacks>(*invocation_->owned_state())
            .value();
    }

    [[nodiscard]] std::vector<ResolvedHarvestOutputStack>& action_resolved_harvest_outputs_state() const
    {
        return invocation_->state_manager()
            ->state<StateSetId::SiteActionResolvedHarvestOutputs>(*invocation_->owned_state())
            .value();
    }

    [[nodiscard]] SiteCounters& counters_state() const
    {
        return invocation_->state_manager()->state<StateSetId::SiteCounters>(*invocation_->owned_state()).value();
    }

    [[nodiscard]] SiteObjectiveMetaState& objective_meta_state() const
    {
        return invocation_->state_manager()->state<StateSetId::SiteObjectiveMeta>(*invocation_->owned_state())
            .value();
    }

    [[nodiscard]] std::vector<std::uint32_t>& objective_target_tile_indices_state() const
    {
        return invocation_->state_manager()
            ->state<StateSetId::SiteObjectiveTargetTileIndices>(*invocation_->owned_state())
            .value();
    }

    [[nodiscard]] std::vector<std::uint8_t>& objective_target_tile_mask_state() const
    {
        return invocation_->state_manager()
            ->state<StateSetId::SiteObjectiveTargetTileMask>(*invocation_->owned_state())
            .value();
    }

    [[nodiscard]] std::vector<std::uint32_t>& objective_connection_start_tile_indices_state() const
    {
        return invocation_->state_manager()
            ->state<StateSetId::SiteObjectiveConnectionStartTileIndices>(*invocation_->owned_state())
            .value();
    }

    [[nodiscard]] std::vector<std::uint8_t>& objective_connection_start_tile_mask_state() const
    {
        return invocation_->state_manager()
            ->state<StateSetId::SiteObjectiveConnectionStartTileMask>(*invocation_->owned_state())
            .value();
    }

    [[nodiscard]] std::vector<std::uint32_t>& objective_connection_goal_tile_indices_state() const
    {
        return invocation_->state_manager()
            ->state<StateSetId::SiteObjectiveConnectionGoalTileIndices>(*invocation_->owned_state())
            .value();
    }

    [[nodiscard]] std::vector<std::uint8_t>& objective_connection_goal_tile_mask_state() const
    {
        return invocation_->state_manager()
            ->state<StateSetId::SiteObjectiveConnectionGoalTileMask>(*invocation_->owned_state())
            .value();
    }

    [[nodiscard]] const SiteWorld* site_world_ptr() const noexcept
    {
        return invocation_->site_world();
    }

    [[nodiscard]] SiteWorld* site_world_ptr() noexcept
    {
        return const_cast<SiteWorld*>(std::as_const(*this).site_world_ptr());
    }

    RuntimeInvocation* invocation_ {nullptr};
};

}  // namespace gs1
