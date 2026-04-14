#include "site/systems/worker_condition_system.h"

#include "site/site_projection_update_flags.h"
#include "site/site_run_state.h"
#include "site/site_world_access.h"

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

constexpr std::uint32_t kWorkerMetersChangedInitialMask =
    WORKER_METER_CHANGED_HEALTH |
    WORKER_METER_CHANGED_HYDRATION |
    WORKER_METER_CHANGED_NOURISHMENT |
    WORKER_METER_CHANGED_ENERGY_CAP |
    WORKER_METER_CHANGED_ENERGY |
    WORKER_METER_CHANGED_MORALE |
    WORKER_METER_CHANGED_WORK_EFFICIENCY;

WorkerConditionMemory g_worker_memory {};

WorkerMeterSnapshot make_snapshot(const site_ecs::WorkerVitals& worker) noexcept
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

bool worker_vitals_changed(
    const site_ecs::WorkerVitals& previous,
    const site_ecs::WorkerVitals& current) noexcept
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
    auto& memory = g_worker_memory;
    WorkerMeterSnapshot snapshot {};
    const bool has_worker = site_world_access::worker_condition::read_worker(
        context.site_run,
        [&](const site_ecs::WorkerVitals& worker) {
            snapshot = make_snapshot(worker);
        });
    if (!has_worker)
    {
        return;
    }

    const auto previous_snapshot = memory.last_reported_snapshot.value_or(snapshot);
    const auto mask = compute_change_mask(previous_snapshot, snapshot);
    const auto is_initial = !memory.last_reported_snapshot.has_value();
    const auto actual_mask = is_initial ? kWorkerMetersChangedInitialMask : mask;

    if (is_initial || actual_mask != WORKER_METER_CHANGED_NONE)
    {
        GameCommand command {};
        command.type = GameCommandType::WorkerMetersChanged;
        command.set_payload(WorkerMetersChangedCommand {
            actual_mask,
            snapshot.health,
            snapshot.hydration,
            snapshot.nourishment,
            snapshot.energy_cap,
            snapshot.energy,
            snapshot.morale,
            snapshot.work_efficiency});
        context.command_queue.push_back(command);
        context.world.mark_projection_dirty(
            SITE_PROJECTION_UPDATE_WORKER | SITE_PROJECTION_UPDATE_HUD);
        memory.last_reported_snapshot = snapshot;
    }
}

void apply_passive_decay(site_ecs::WorkerVitals& worker, float step_seconds) noexcept
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

void apply_worker_meter_delta(
    site_ecs::WorkerVitals& worker,
    const WorkerMeterDeltaRequestedCommand& payload) noexcept
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

bool WorkerConditionSystem::subscribes_to(GameCommandType type) noexcept
{
    return type == GameCommandType::WorkerMeterDeltaRequested;
}

Gs1Status WorkerConditionSystem::process_command(
    SiteSystemContext<WorkerConditionSystem>& context,
    const GameCommand& command)
{
    if (command.type != GameCommandType::WorkerMeterDeltaRequested)
    {
        return GS1_STATUS_OK;
    }

    const bool modified = site_world_access::worker_condition::with_worker_mut(
        context.site_run,
        [&](site_ecs::WorkerVitals& worker) {
            const auto previous = worker;
            apply_worker_meter_delta(worker, command.payload_as<WorkerMeterDeltaRequestedCommand>());
            return worker_vitals_changed(previous, worker);
        });
    if (modified)
    {
        emit_worker_meters_changed_if_needed(context);
    }
    return GS1_STATUS_OK;
}

void WorkerConditionSystem::run(SiteSystemContext<WorkerConditionSystem>& context)
{
    const auto step_seconds = static_cast<float>(context.fixed_step_seconds);
    const bool modified = site_world_access::worker_condition::with_worker_mut(
        context.site_run,
        [&](site_ecs::WorkerVitals& worker) {
            const auto previous = worker;
            apply_passive_decay(worker, step_seconds);
            return worker_vitals_changed(previous, worker);
        });
    if (modified)
    {
        emit_worker_meters_changed_if_needed(context);
    }
}
}  // namespace gs1
