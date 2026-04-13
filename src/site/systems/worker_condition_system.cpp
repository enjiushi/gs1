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

constexpr std::uint32_t kWorkerMetersChangedInitialMask =
    WORKER_METER_CHANGED_HEALTH |
    WORKER_METER_CHANGED_HYDRATION |
    WORKER_METER_CHANGED_NOURISHMENT |
    WORKER_METER_CHANGED_ENERGY_CAP |
    WORKER_METER_CHANGED_ENERGY |
    WORKER_METER_CHANGED_MORALE |
    WORKER_METER_CHANGED_WORK_EFFICIENCY;

WorkerConditionMemory g_worker_memory {};

WorkerMeterSnapshot make_snapshot(const WorkerState& worker) noexcept
{
    return WorkerMeterSnapshot {
        worker.player_health,
        worker.player_hydration,
        worker.player_nourishment,
        worker.player_energy_cap,
        worker.player_energy,
        worker.player_morale,
        worker.player_work_efficiency};
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

void ensure_memory_for_run(SiteSystemContext& context) noexcept
{
    auto& memory = g_worker_memory;
    if (!memory.site_run_id.has_value() ||
        memory.site_run_id->value != context.site_run.site_run_id.value)
    {
        memory.site_run_id = context.site_run.site_run_id;
        memory.last_reported_snapshot.reset();
    }
}

void emit_worker_meters_changed_if_needed(SiteSystemContext& context) noexcept
{
    ensure_memory_for_run(context);
    auto& memory = g_worker_memory;
    const auto snapshot = make_snapshot(context.site_run.worker);
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
        context.site_run.pending_projection_update_flags |=
            SITE_PROJECTION_UPDATE_WORKER | SITE_PROJECTION_UPDATE_HUD;
        memory.last_reported_snapshot = snapshot;
    }
}

void apply_passive_decay(WorkerState& worker, float step_seconds) noexcept
{
    worker.player_hydration = std::clamp(
        worker.player_hydration - kHydrationDrainPerSecond * step_seconds,
        kMeterMin,
        kHydrationMax);
    worker.player_nourishment = std::clamp(
        worker.player_nourishment - kNourishmentDrainPerSecond * step_seconds,
        kMeterMin,
        kNourishmentMax);
    worker.player_energy = std::clamp(
        worker.player_energy - kEnergyDrainPerSecond * step_seconds,
        kMeterMin,
        worker.player_energy_cap);
    worker.player_morale = std::clamp(
        worker.player_morale - kMoraleDrainPerSecond * step_seconds,
        kMeterMin,
        kMoraleMax);

    if (worker.player_hydration <= 0.0f || worker.player_nourishment <= 0.0f)
    {
        worker.player_health = std::clamp(
            worker.player_health - kStarvationHealthDrainPerSecond * step_seconds,
            kMeterMin,
            kHealthMax);
    }
}

void apply_worker_meter_delta(
    WorkerState& worker,
    const WorkerMeterDeltaRequestedCommand& payload) noexcept
{
    worker.player_health = std::clamp(
        worker.player_health + payload.health_delta,
        kMeterMin,
        kHealthMax);
    worker.player_hydration = std::clamp(
        worker.player_hydration + payload.hydration_delta,
        kMeterMin,
        kHydrationMax);
    worker.player_nourishment = std::clamp(
        worker.player_nourishment + payload.nourishment_delta,
        kMeterMin,
        kNourishmentMax);
    worker.player_energy_cap = std::clamp(
        worker.player_energy_cap + payload.energy_cap_delta,
        kMeterMin,
        kEnergyCapMax);
    worker.player_energy = std::clamp(
        worker.player_energy + payload.energy_delta,
        kMeterMin,
        worker.player_energy_cap);
    worker.player_morale = std::clamp(
        worker.player_morale + payload.morale_delta,
        kMeterMin,
        kMoraleMax);
    worker.player_work_efficiency = std::clamp(
        worker.player_work_efficiency + payload.work_efficiency_delta,
        kWorkEfficiencyMin,
        kWorkEfficiencyMax);
}
}  // namespace

bool WorkerConditionSystem::subscribes_to(GameCommandType type) noexcept
{
    return type == GameCommandType::WorkerMeterDeltaRequested;
}

Gs1Status WorkerConditionSystem::process_command(
    SiteSystemContext& context,
    const GameCommand& command)
{
    if (command.type != GameCommandType::WorkerMeterDeltaRequested)
    {
        return GS1_STATUS_OK;
    }

    apply_worker_meter_delta(
        context.site_run.worker,
        command.payload_as<WorkerMeterDeltaRequestedCommand>());
    emit_worker_meters_changed_if_needed(context);
    return GS1_STATUS_OK;
}

void WorkerConditionSystem::run(SiteSystemContext& context)
{
    const auto step_seconds = static_cast<float>(context.fixed_step_seconds);
    apply_passive_decay(context.site_run.worker, step_seconds);
    emit_worker_meters_changed_if_needed(context);
}
}  // namespace gs1
