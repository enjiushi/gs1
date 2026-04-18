#include "site/systems/worker_condition_system.h"

#include "site/site_projection_update_flags.h"
#include "site/site_run_state.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>

namespace gs1
{
namespace
{
struct WorkerMeterSnapshot final
{
    float health {0.0f};
    float hydration {0.0f};
    float nourishment {0.0f};
    float energy_cap {0.0f};
    float energy {0.0f};
    float morale {0.0f};
    float work_efficiency {0.0f};
};

struct WorkerConditionMemory final
{
    std::optional<SiteRunId> site_run_id {};
    std::optional<WorkerMeterSnapshot> last_reported_snapshot {};
};

constexpr float kHealthMax = 100.0f;
constexpr float kHydrationMax = 100.0f;
constexpr float kNourishmentMax = 100.0f;
constexpr float kEnergyCapMax = 200.0f;
constexpr float kMoraleMax = 100.0f;
constexpr float kMeterMin = 0.0f;
constexpr float kWorkEfficiencyMax = 2.0f;
constexpr float kWorkEfficiencyMin = 0.0f;

constexpr float kHydrationDrainPerSecond = 0.03f;
constexpr float kNourishmentDrainPerSecond = 0.02f;
constexpr float kEnergyDrainPerSecond = 0.05f;
constexpr float kMoraleDrainPerSecond = 0.01f;
constexpr float kStarvationHealthDrainPerSecond = 0.02f;
constexpr float kMeterChangeThreshold = 0.01f;
constexpr float kWindHydrationDrainPerUnitPerSecond = 0.0012f;
constexpr float kWindEnergyDrainPerUnitPerSecond = 0.0010f;
constexpr float kWindMoraleDrainPerUnitPerSecond = 0.0006f;
constexpr float kShelteredWindExposureScale = 0.2f;

constexpr std::uint32_t kWorkerMetersChangedInitialMask =
    WORKER_METER_CHANGED_HEALTH |
    WORKER_METER_CHANGED_HYDRATION |
    WORKER_METER_CHANGED_NOURISHMENT |
    WORKER_METER_CHANGED_ENERGY_CAP |
    WORKER_METER_CHANGED_ENERGY |
    WORKER_METER_CHANGED_MORALE |
    WORKER_METER_CHANGED_WORK_EFFICIENCY;

WorkerConditionMemory g_worker_memory {};

WorkerMeterSnapshot make_snapshot(const SiteWorld::WorkerConditionData& worker) noexcept
{
    return WorkerMeterSnapshot {
        worker.health,
        worker.hydration,
        worker.nourishment,
        worker.energy_cap,
        worker.energy,
        worker.morale,
        worker.work_efficiency};
}

bool worker_conditions_changed(
    const SiteWorld::WorkerConditionData& previous,
    const SiteWorld::WorkerConditionData& current) noexcept
{
    return previous.health != current.health ||
        previous.hydration != current.hydration ||
        previous.nourishment != current.nourishment ||
        previous.energy_cap != current.energy_cap ||
        previous.energy != current.energy ||
        previous.morale != current.morale ||
        previous.work_efficiency != current.work_efficiency ||
        previous.is_sheltered != current.is_sheltered;
}

std::uint32_t compute_change_mask(
    const WorkerMeterSnapshot& previous,
    const WorkerMeterSnapshot& current) noexcept
{
    std::uint32_t mask = WORKER_METER_CHANGED_NONE;
    if (std::fabs(previous.health - current.health) > kMeterChangeThreshold)
    {
        mask |= WORKER_METER_CHANGED_HEALTH;
    }

    if (std::fabs(previous.hydration - current.hydration) > kMeterChangeThreshold)
    {
        mask |= WORKER_METER_CHANGED_HYDRATION;
    }

    if (std::fabs(previous.nourishment - current.nourishment) > kMeterChangeThreshold)
    {
        mask |= WORKER_METER_CHANGED_NOURISHMENT;
    }

    if (std::fabs(previous.energy_cap - current.energy_cap) > kMeterChangeThreshold)
    {
        mask |= WORKER_METER_CHANGED_ENERGY_CAP;
    }

    if (std::fabs(previous.energy - current.energy) > kMeterChangeThreshold)
    {
        mask |= WORKER_METER_CHANGED_ENERGY;
    }

    if (std::fabs(previous.morale - current.morale) > kMeterChangeThreshold)
    {
        mask |= WORKER_METER_CHANGED_MORALE;
    }

    if (std::fabs(previous.work_efficiency - current.work_efficiency) > kMeterChangeThreshold)
    {
        mask |= WORKER_METER_CHANGED_WORK_EFFICIENCY;
    }

    return mask;
}

void ensure_memory_for_run(SiteSystemContext<WorkerConditionSystem>& context) noexcept
{
    auto& memory = g_worker_memory;
    if (!memory.site_run_id.has_value() ||
        memory.site_run_id->value != context.world.site_run_id().value)
    {
        memory.site_run_id = context.world.site_run_id();
        memory.last_reported_snapshot.reset();
    }
}

void emit_worker_meters_changed_if_needed(SiteSystemContext<WorkerConditionSystem>& context) noexcept
{
    ensure_memory_for_run(context);
    if (!context.world.has_world())
    {
        return;
    }

    auto& memory = g_worker_memory;
    const auto snapshot = make_snapshot(context.world.read_worker().conditions);
    const auto previous_snapshot = memory.last_reported_snapshot.value_or(snapshot);
    const auto mask = compute_change_mask(previous_snapshot, snapshot);
    const auto is_initial = !memory.last_reported_snapshot.has_value();
    const auto actual_mask = is_initial ? kWorkerMetersChangedInitialMask : mask;

    if (is_initial || actual_mask != WORKER_METER_CHANGED_NONE)
    {
        GameMessage message {};
        message.type = GameMessageType::WorkerMetersChanged;
        message.set_payload(WorkerMetersChangedMessage {
            actual_mask,
            snapshot.health,
            snapshot.hydration,
            snapshot.nourishment,
            snapshot.energy_cap,
            snapshot.energy,
            snapshot.morale,
            snapshot.work_efficiency});
        context.message_queue.push_back(message);
        context.world.mark_projection_dirty(
            SITE_PROJECTION_UPDATE_WORKER | SITE_PROJECTION_UPDATE_HUD);
        memory.last_reported_snapshot = snapshot;
    }
}

void apply_passive_decay(SiteWorld::WorkerConditionData& worker, float step_seconds) noexcept
{
    worker.hydration = std::clamp(
        worker.hydration - kHydrationDrainPerSecond * step_seconds,
        kMeterMin,
        kHydrationMax);
    worker.nourishment = std::clamp(
        worker.nourishment - kNourishmentDrainPerSecond * step_seconds,
        kMeterMin,
        kNourishmentMax);
    worker.energy = std::clamp(
        worker.energy - kEnergyDrainPerSecond * step_seconds,
        kMeterMin,
        worker.energy_cap);
    worker.morale = std::clamp(
        worker.morale - kMoraleDrainPerSecond * step_seconds,
        kMeterMin,
        kMoraleMax);

    if (worker.hydration <= 0.0f || worker.nourishment <= 0.0f)
    {
        worker.health = std::clamp(
            worker.health - kStarvationHealthDrainPerSecond * step_seconds,
            kMeterMin,
            kHealthMax);
    }
}

void apply_wind_exposure_decay(
    SiteWorld::WorkerConditionData& worker,
    const SiteWorld::TileLocalWeatherData& local_weather,
    float step_seconds) noexcept
{
    const float wind = std::max(local_weather.wind, 0.0f);
    if (wind <= 0.0f || step_seconds <= 0.0f)
    {
        return;
    }

    const float exposure_scale = worker.is_sheltered ? kShelteredWindExposureScale : 1.0f;
    worker.hydration = std::clamp(
        worker.hydration - wind * kWindHydrationDrainPerUnitPerSecond * exposure_scale * step_seconds,
        kMeterMin,
        kHydrationMax);
    worker.energy = std::clamp(
        worker.energy - wind * kWindEnergyDrainPerUnitPerSecond * exposure_scale * step_seconds,
        kMeterMin,
        worker.energy_cap);
    worker.morale = std::clamp(
        worker.morale - wind * kWindMoraleDrainPerUnitPerSecond * exposure_scale * step_seconds,
        kMeterMin,
        kMoraleMax);
}

void apply_worker_meter_delta(
    SiteWorld::WorkerConditionData& worker,
    const WorkerMeterDeltaRequestedMessage& payload) noexcept
{
    worker.health = std::clamp(
        worker.health + payload.health_delta,
        kMeterMin,
        kHealthMax);
    worker.hydration = std::clamp(
        worker.hydration + payload.hydration_delta,
        kMeterMin,
        kHydrationMax);
    worker.nourishment = std::clamp(
        worker.nourishment + payload.nourishment_delta,
        kMeterMin,
        kNourishmentMax);
    worker.energy_cap = std::clamp(
        worker.energy_cap + payload.energy_cap_delta,
        kMeterMin,
        kEnergyCapMax);
    worker.energy = std::clamp(
        worker.energy + payload.energy_delta,
        kMeterMin,
        worker.energy_cap);
    worker.morale = std::clamp(
        worker.morale + payload.morale_delta,
        kMeterMin,
        kMoraleMax);
    worker.work_efficiency = std::clamp(
        worker.work_efficiency + payload.work_efficiency_delta,
        kWorkEfficiencyMin,
        kWorkEfficiencyMax);
}
}  // namespace

bool WorkerConditionSystem::subscribes_to(GameMessageType type) noexcept
{
    return type == GameMessageType::WorkerMeterDeltaRequested;
}

Gs1Status WorkerConditionSystem::process_message(
    SiteSystemContext<WorkerConditionSystem>& context,
    const GameMessage& message)
{
    if (message.type != GameMessageType::WorkerMeterDeltaRequested)
    {
        return GS1_STATUS_OK;
    }

    if (!context.world.has_world())
    {
        return GS1_STATUS_OK;
    }

    auto worker = context.world.read_worker();
    const auto previous = worker.conditions;
    apply_worker_meter_delta(
        worker.conditions,
        message.payload_as<WorkerMeterDeltaRequestedMessage>());
    const bool modified = worker_conditions_changed(previous, worker.conditions);
    if (modified)
    {
        context.world.write_worker(worker);
        emit_worker_meters_changed_if_needed(context);
    }
    return GS1_STATUS_OK;
}

void WorkerConditionSystem::run(SiteSystemContext<WorkerConditionSystem>& context)
{
    if (!context.world.has_world())
    {
        return;
    }

    const auto step_seconds = static_cast<float>(context.fixed_step_seconds);
    auto worker = context.world.read_worker();
    const auto previous = worker.conditions;
    apply_passive_decay(worker.conditions, step_seconds);
    apply_wind_exposure_decay(
        worker.conditions,
        context.world.read_tile_local_weather(worker.position.tile_coord),
        step_seconds);
    const bool modified = worker_conditions_changed(previous, worker.conditions);
    if (modified)
    {
        context.world.write_worker(worker);
        emit_worker_meters_changed_if_needed(context);
    }
}
}  // namespace gs1
