#pragma once

#include "site/action_state.h"
#include "site/contractor_state.h"
#include "site/craft_state.h"
#include "site/device_weather_contribution_state.h"
#include "site/economy_state.h"
#include "site/event_state.h"
#include "site/inventory_state.h"
#include "site/local_weather_resolve_state.h"
#include "site/modifier_state.h"
#include "site/plant_weather_contribution_state.h"
#include "site/site_objective_state.h"
#include "site/task_board_state.h"
#include "site/weather_state.h"
#include "support/id_types.h"

#include <cstdint>
#include <memory>
#include <mutex>
#include <utility>
#include <unordered_map>
#include <vector>

namespace gs1
{
class SiteWorld;

namespace detail
{
class SiteWorldHandleRegistry final
{
public:
    static SiteWorldHandleRegistry& instance() noexcept
    {
        static SiteWorldHandleRegistry registry {};
        return registry;
    }

    std::uint64_t acquire(std::shared_ptr<SiteWorld> world)
    {
        if (world == nullptr)
        {
            return 0U;
        }

        std::lock_guard<std::mutex> guard {mutex_};
        const std::uint64_t token = next_token_++;
        entries_.emplace(token, Entry {std::move(world), 1U});
        return token;
    }

    void retain(std::uint64_t token) noexcept
    {
        if (token == 0U)
        {
            return;
        }

        std::lock_guard<std::mutex> guard {mutex_};
        const auto it = entries_.find(token);
        if (it != entries_.end())
        {
            ++it->second.ref_count;
        }
    }

    void release(std::uint64_t token) noexcept
    {
        if (token == 0U)
        {
            return;
        }

        std::lock_guard<std::mutex> guard {mutex_};
        const auto it = entries_.find(token);
        if (it == entries_.end())
        {
            return;
        }

        if (it->second.ref_count > 0U)
        {
            --it->second.ref_count;
        }

        if (it->second.ref_count == 0U)
        {
            entries_.erase(it);
        }
    }

    [[nodiscard]] SiteWorld* get(std::uint64_t token) const noexcept
    {
        if (token == 0U)
        {
            return nullptr;
        }

        std::lock_guard<std::mutex> guard {mutex_};
        const auto it = entries_.find(token);
        return it != entries_.end() ? it->second.world.get() : nullptr;
    }

private:
    struct Entry final
    {
        std::shared_ptr<SiteWorld> world {};
        std::uint32_t ref_count {0U};
    };

    mutable std::mutex mutex_ {};
    std::uint64_t next_token_ {1U};
    std::unordered_map<std::uint64_t, Entry> entries_ {};
};
}  // namespace detail

struct SiteWorldHandle final
{
    std::uint64_t token {0U};

    SiteWorldHandle() = default;
    SiteWorldHandle(std::nullptr_t) noexcept {}
    explicit SiteWorldHandle(SiteWorld* world)
    {
        *this = world;
    }
    explicit SiteWorldHandle(std::shared_ptr<SiteWorld> world)
    {
        *this = std::move(world);
    }
    SiteWorldHandle(const SiteWorldHandle& other) noexcept
        : token(other.token)
    {
        detail::SiteWorldHandleRegistry::instance().retain(token);
    }
    SiteWorldHandle(SiteWorldHandle&& other) noexcept
        : token(std::exchange(other.token, 0U))
    {
    }
    ~SiteWorldHandle()
    {
        reset();
    }

    SiteWorldHandle& operator=(const SiteWorldHandle& other) noexcept
    {
        if (this != &other)
        {
            reset();
            token = other.token;
            detail::SiteWorldHandleRegistry::instance().retain(token);
        }
        return *this;
    }

    SiteWorldHandle& operator=(SiteWorldHandle&& other) noexcept
    {
        if (this != &other)
        {
            reset();
            token = std::exchange(other.token, 0U);
        }
        return *this;
    }

    SiteWorldHandle& operator=(std::nullptr_t) noexcept
    {
        reset();
        return *this;
    }

    SiteWorldHandle& operator=(SiteWorld* world)
    {
        return *this = std::shared_ptr<SiteWorld>(world, [](SiteWorld*) {});
    }

    SiteWorldHandle& operator=(std::shared_ptr<SiteWorld> world)
    {
        reset();
        token = detail::SiteWorldHandleRegistry::instance().acquire(std::move(world));
        return *this;
    }

    [[nodiscard]] SiteWorld* get() const noexcept
    {
        return detail::SiteWorldHandleRegistry::instance().get(token);
    }

    [[nodiscard]] SiteWorld* operator->() const noexcept { return get(); }
    [[nodiscard]] SiteWorld& operator*() const noexcept { return *get(); }
    [[nodiscard]] explicit operator bool() const noexcept { return get() != nullptr; }

    [[nodiscard]] bool operator==(std::nullptr_t) const noexcept { return get() == nullptr; }
    [[nodiscard]] bool operator!=(std::nullptr_t) const noexcept { return get() != nullptr; }

    void reset() noexcept
    {
        detail::SiteWorldHandleRegistry::instance().release(token);
        token = 0U;
    }
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
    std::uint32_t tracked_living_plant_count {0};
    float objective_progress_normalized {0.0f};
    float highway_average_sand_cover {0.0f};
    bool all_tracked_living_plants_stable {false};
};

struct SiteHostMoveDirectionState final
{
    float world_move_x {0.0f};
    float world_move_y {0.0f};
    float world_move_z {0.0f};
    bool present {false};
};

struct SiteRunState final
{
    SiteRunId site_run_id {};
    SiteId site_id {};
    std::uint32_t site_archetype_id {0};
    std::uint32_t attempt_index {0};
    std::uint64_t site_attempt_seed {0};
    SiteRunStatus run_status {SiteRunStatus::Active};
    SiteWorldHandle site_world {};
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
    SiteHostMoveDirectionState host_move_direction {};
    SiteCounters counters {};
    SiteObjectiveState objective {};
    LocalWeatherResolveState local_weather_resolve {};
    PlantWeatherContributionState plant_weather_contribution {};
    DeviceWeatherContributionState device_weather_contribution {};
    std::uint32_t result_newly_revealed_site_count {0};
};
}  // namespace gs1
