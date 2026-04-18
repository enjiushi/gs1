#pragma once

#include "site/action_state.h"
#include "site/contractor_state.h"
#include "site/craft_state.h"
#include "site/economy_state.h"
#include "site/event_state.h"
#include "site/inventory_state.h"
#include "site/local_weather_resolve_state.h"
#include "site/modifier_state.h"
#include "site/task_board_state.h"
#include "site/weather_state.h"
#include "support/id_types.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace gs1
{
class SiteWorld;

struct PendingInventorySlotProjectionUpdate final
{
    std::uint32_t storage_id {0};
    std::uint32_t slot_index {0};
};

enum class SiteRunStatus : std::uint32_t
{
    Active = 0,
    Completed = 1,
    Failed = 2
};

enum class DayPhase : std::uint32_t
{
    Dawn = 0,
    Day = 1,
    Dusk = 2,
    Night = 3
};

struct SiteClockState final
{
    double world_time_minutes {0.0};
    std::uint32_t day_index {0};
    DayPhase day_phase {DayPhase::Dawn};
    double ecology_pulse_accumulator {0.0};
    double task_refresh_accumulator {0.0};
    double delivery_accumulator {0.0};
    double accumulator_real_seconds {0.0};
};

struct CampState final
{
    TileCoord camp_anchor_tile {};
    TileCoord starter_storage_tile {};
    TileCoord delivery_box_tile {};
    float camp_durability {100.0f};
    bool camp_protection_resolved {true};
    bool delivery_point_operational {true};
    bool shared_storage_access_enabled {true};
};

struct SiteCounters final
{
    std::uint32_t fully_grown_tile_count {0};
    std::uint32_t site_completion_tile_threshold {0};
};

struct SiteRunState final
{
    SiteRunId site_run_id {};
    SiteId site_id {};
    std::uint32_t site_archetype_id {0};
    std::uint32_t attempt_index {0};
    std::uint64_t site_attempt_seed {0};
    SiteRunStatus run_status {SiteRunStatus::Active};
    std::shared_ptr<SiteWorld> site_world {};
    SiteClockState clock {};
    CampState camp {};
    InventoryState inventory {};
    ContractorState contractor {};
    WeatherState weather {};
    EventState event {};
    TaskBoardState task_board {};
    ModifierState modifier {};
    EconomyState economy {};
    CraftState craft {};
    ActionState site_action {};
    SiteCounters counters {};
    LocalWeatherResolveState local_weather_resolve {};
    std::uint32_t result_newly_revealed_site_count {0};
    std::uint64_t pending_projection_update_flags {0};
    bool pending_full_tile_projection_update {false};
    std::vector<TileCoord> pending_tile_projection_updates {};
    std::vector<std::uint8_t> pending_tile_projection_update_mask {};
    bool pending_full_inventory_projection_update {false};
    std::vector<std::uint32_t> pending_worker_pack_inventory_projection_updates {};
    std::vector<std::uint8_t> pending_worker_pack_inventory_projection_update_mask {};
    bool pending_inventory_storage_descriptor_projection_update {false};
    bool pending_inventory_view_state_projection_update {false};
    bool pending_opened_inventory_storage_full_projection_update {false};
    std::vector<std::uint32_t> pending_opened_inventory_storage_projection_updates {};
    std::vector<std::uint8_t> pending_opened_inventory_storage_projection_update_mask {};
};
}  // namespace gs1
