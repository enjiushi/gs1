#pragma once

#include "site/action_state.h"
#include "site/contractor_state.h"
#include "site/economy_state.h"
#include "site/event_state.h"
#include "site/inventory_state.h"
#include "site/modifier_state.h"
#include "site/task_board_state.h"
#include "site/tile_grid_state.h"
#include "site/weather_state.h"
#include "support/id_types.h"

#include <cstdint>
#include <optional>

namespace gs1
{
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

struct WorkerState final
{
    TileCoord tile_coord {};
    float facing_degrees {0.0f};
    float player_health {100.0f};
    float player_hydration {100.0f};
    float player_nourishment {100.0f};
    float player_energy_cap {100.0f};
    float player_energy {100.0f};
    float player_morale {100.0f};
    float player_work_efficiency {1.0f};
    bool is_sheltered {false};
    std::optional<RuntimeActionId> current_action_id {};
};

struct CampState final
{
    TileCoord camp_anchor_tile {};
    float camp_durability {100.0f};
    bool camp_protection_resolved {true};
    bool delivery_point_operational {true};
    bool shared_camp_storage_access_enabled {true};
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
    SiteClockState clock {};
    TileGridState tile_grid {};
    WorkerState worker {};
    CampState camp {};
    InventoryState inventory {};
    ContractorState contractor {};
    WeatherState weather {};
    EventState event {};
    TaskBoardState task_board {};
    ModifierState modifier {};
    EconomyState economy {};
    ActionState site_action {};
    SiteCounters counters {};
    std::uint64_t pending_presentation_dirty_flags {0};
};
}  // namespace gs1
