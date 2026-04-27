#include "site/systems/worker_condition_system.h"

#include "content/defs/gameplay_tuning_defs.h"
#include "runtime/runtime_clock.h"
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

struct WorkerMeterDeltas final
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
constexpr float kEnergyCapMin = 50.0f;
constexpr float kEnergyCapMax = 100.0f;
constexpr float kMoraleMax = 100.0f;
constexpr float kMeterMin = 0.0f;
constexpr float kWorkEfficiencyMax = 1.0f;
constexpr float kWorkEfficiencyMin = 0.4f;
constexpr float kNormalizedMeterMax = 100.0f;
constexpr float kMeterChangeThreshold = 0.01f;
constexpr float kMoraleWeatherNeutralPoint = 50.0f;
constexpr float kMoraleWeatherHalfBand = 50.0f;

constexpr std::uint32_t kWorkerMetersChangedInitialMask =
    WORKER_METER_CHANGED_HEALTH |
    WORKER_METER_CHANGED_HYDRATION |
    WORKER_METER_CHANGED_NOURISHMENT |
    WORKER_METER_CHANGED_ENERGY_CAP |
    WORKER_METER_CHANGED_ENERGY |
    WORKER_METER_CHANGED_MORALE |
    WORKER_METER_CHANGED_WORK_EFFICIENCY;

WorkerConditionMemory g_worker_memory {};

float clamp_meter(float value, float max_value) noexcept
{
    return std::clamp(value, kMeterMin, max_value);
}

float normalize_meter(float value) noexcept
{
    return std::clamp(value / kNormalizedMeterMax, 0.0f, 1.0f);
}

const WorkerConditionTuning& worker_condition_tuning() noexcept
{
    return gameplay_tuning_def().worker_condition;
}

float resolve_factor(float base) noexcept
{
    const auto& tuning = worker_condition_tuning();
    return std::clamp(
        (base * tuning.factor_weight_default) + tuning.factor_bias_default,
        tuning.factor_min,
        tuning.factor_max);
}

float resolve_factor(float base, float bias) noexcept
{
    const auto& tuning = worker_condition_tuning();
    return std::clamp(
        (base * tuning.factor_weight_default) + bias,
        tuning.factor_min,
        tuning.factor_max);
}

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

bool action_is_executing(const ActionState& action_state) noexcept
{
    return action_state.current_action_id.has_value() &&
        action_state.started_at_world_minute.has_value();
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

WorkerMeterDeltas deltas_from_message(const WorkerMeterDeltaRequestedMessage& payload) noexcept
{
    return WorkerMeterDeltas {
        payload.health_delta,
        payload.hydration_delta,
        payload.nourishment_delta,
        payload.energy_cap_delta,
        payload.energy_delta,
        payload.morale_delta,
        payload.work_efficiency_delta};
}

float resolve_meter_units_per_real_second(float full_change_real_minutes) noexcept
{
    return full_change_real_minutes <= 0.0f
        ? 0.0f
        : kNormalizedMeterMax / (full_change_real_minutes * 60.0f);
}

float resolve_energy_background_speed_factor(
    const SiteWorld::WorkerConditionData& current) noexcept
{
    const auto& tuning = worker_condition_tuning();
    const float minimum_support =
        std::min(normalize_meter(current.hydration), normalize_meter(current.nourishment));
    const float minimum_speed = tuning.energy_background_min_speed_factor;
    return std::clamp(
        minimum_speed + ((1.0f - minimum_speed) * minimum_support),
        minimum_speed,
        1.0f);
}

float resolve_energy_background_delta(
    const SiteWorld::WorkerConditionData& current,
    float step_real_seconds) noexcept
{
    if (step_real_seconds <= 0.0f)
    {
        return 0.0f;
    }

    return step_real_seconds *
        resolve_energy_background_speed_factor(current) *
        resolve_meter_units_per_real_second(
            worker_condition_tuning().energy_background_increase_real_minutes);
}

void accumulate_passive_deltas(
    WorkerMeterDeltas& deltas,
    const SiteWorld::WorkerConditionData& previous,
    const SiteWorld::TileLocalWeatherData& local_weather,
    const ModifierChannelTotals& modifiers,
    float step_game_minutes,
    float step_real_seconds) noexcept
{
    if (step_game_minutes <= 0.0f && step_real_seconds <= 0.0f)
    {
        return;
    }

    const auto& tuning = worker_condition_tuning();
    const float exposure_scale = previous.is_sheltered ? tuning.sheltered_exposure_scale : 1.0f;
    const float heat = normalize_meter(std::max(local_weather.heat, 0.0f)) * exposure_scale;
    const float wind = normalize_meter(std::max(local_weather.wind, 0.0f)) * exposure_scale;
    const float dust = normalize_meter(std::max(local_weather.dust, 0.0f)) * exposure_scale;
    const float max_weather =
        std::max(
            std::max(std::max(local_weather.heat, 0.0f), std::max(local_weather.wind, 0.0f)),
            std::max(local_weather.dust, 0.0f)) * exposure_scale;

    deltas.hydration -= step_game_minutes * (
        resolve_factor(tuning.hydration_base_loss_per_game_minute) +
        heat * resolve_factor(tuning.heat_to_hydration_factor) +
        wind * resolve_factor(tuning.wind_to_hydration_factor) +
        dust * resolve_factor(tuning.dust_to_hydration_factor));
    deltas.nourishment -= step_game_minutes * (
        resolve_factor(tuning.nourishment_base_loss_per_game_minute) +
        wind * resolve_factor(tuning.wind_to_nourishment_factor) +
        heat * resolve_factor(tuning.heat_to_nourishment_factor) +
        dust * resolve_factor(tuning.dust_to_nourishment_factor));
    const float morale_weather_factor = std::clamp(
        (kMoraleWeatherNeutralPoint - max_weather) / kMoraleWeatherHalfBand,
        -1.0f,
        1.0f);
    const float morale_increase_per_real_second =
        resolve_meter_units_per_real_second(tuning.morale_background_increase_real_minutes);
    const float morale_decrease_per_real_second =
        resolve_meter_units_per_real_second(tuning.morale_background_decrease_real_minutes);
    const float morale_support_per_real_second =
        resolve_meter_units_per_real_second(tuning.morale_support_real_minutes);
    deltas.morale += step_real_seconds *
        (morale_weather_factor >= 0.0f
            ? morale_weather_factor * morale_increase_per_real_second
            : morale_weather_factor * morale_decrease_per_real_second);
    deltas.morale += step_real_seconds *
        std::clamp(modifiers.morale, -1.0f, 1.0f) *
        morale_support_per_real_second;
    deltas.health -= step_game_minutes * (
        heat * resolve_factor(tuning.heat_to_health_factor) +
        wind * resolve_factor(tuning.wind_to_health_factor) +
        dust * resolve_factor(tuning.dust_to_health_factor));
}

float resolve_energy_cap(
    const SiteWorld::WorkerConditionData& current,
    const ModifierChannelTotals& modifiers,
    float direct_delta) noexcept
{
    const auto& tuning = worker_condition_tuning();
    const float normalized_health =
        normalize_meter(current.health) * resolve_factor(tuning.health_energy_cap_factor);
    const float normalized_hydration =
        normalize_meter(current.hydration) * resolve_factor(tuning.hydration_energy_cap_factor);
    const float normalized_nourishment =
        normalize_meter(current.nourishment) * resolve_factor(tuning.nourishment_energy_cap_factor);
    const float energy_cap_factor =
        resolve_factor(tuning.energy_cap_factor, modifiers.energy_cap);
    const float raw_cap = std::min(
                              normalized_health,
                              std::min(normalized_hydration, normalized_nourishment)) *
        energy_cap_factor *
        kNormalizedMeterMax;
    return std::clamp(raw_cap + direct_delta, kEnergyCapMin, kEnergyCapMax);
}

float resolve_work_efficiency(
    const SiteWorld::WorkerConditionData& current,
    const ModifierChannelTotals& modifiers,
    float direct_delta) noexcept
{
    const auto& tuning = worker_condition_tuning();
    const float normalized_health =
        normalize_meter(current.health) * resolve_factor(tuning.health_efficiency_factor);
    const float normalized_hydration =
        normalize_meter(current.hydration) * resolve_factor(tuning.hydration_efficiency_factor);
    const float normalized_nourishment =
        normalize_meter(current.nourishment) * resolve_factor(tuning.nourishment_efficiency_factor);
    const float normalized_morale =
        normalize_meter(current.morale) * resolve_factor(tuning.morale_efficiency_factor);
    const float work_efficiency_factor =
        resolve_factor(tuning.work_efficiency_factor, modifiers.work_efficiency);
    const float raw_efficiency = std::min(
                                     normalized_health,
                                     std::min(
                                         normalized_hydration,
                                         std::min(normalized_nourishment, normalized_morale))) *
        work_efficiency_factor;
    return std::clamp(raw_efficiency + direct_delta, kWorkEfficiencyMin, kWorkEfficiencyMax);
}

SiteWorld::WorkerConditionData resolve_worker_conditions(
    const SiteWorld::WorkerConditionData& previous,
    const WorkerMeterDeltas& deltas,
    const ModifierChannelTotals& modifiers,
    float passive_energy_step_real_seconds) noexcept
{
    SiteWorld::WorkerConditionData current = previous;
    current.health = clamp_meter(previous.health + deltas.health, kHealthMax);
    current.hydration = clamp_meter(previous.hydration + deltas.hydration, kHydrationMax);
    current.nourishment = clamp_meter(previous.nourishment + deltas.nourishment, kNourishmentMax);
    current.morale = clamp_meter(previous.morale + deltas.morale, kMoraleMax);
    current.energy_cap = resolve_energy_cap(current, modifiers, deltas.energy_cap);
    const float passive_energy_delta =
        resolve_energy_background_delta(current, passive_energy_step_real_seconds);
    current.energy = std::clamp(
        previous.energy + deltas.energy + passive_energy_delta,
        kMeterMin,
        current.energy_cap);
    current.work_efficiency =
        resolve_work_efficiency(current, modifiers, deltas.work_efficiency);
    return current;
}
}  // namespace

bool WorkerConditionSystem::subscribes_to(GameMessageType type) noexcept
{
    return type == GameMessageType::SiteRunStarted ||
        type == GameMessageType::WorkerMeterDeltaRequested;
}

Gs1Status WorkerConditionSystem::process_message(
    SiteSystemContext<WorkerConditionSystem>& context,
    const GameMessage& message)
{
    if (message.type == GameMessageType::SiteRunStarted)
    {
        emit_worker_meters_changed_if_needed(context);
        return GS1_STATUS_OK;
    }

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
    const auto deltas = deltas_from_message(message.payload_as<WorkerMeterDeltaRequestedMessage>());
    worker.conditions = resolve_worker_conditions(
        previous,
        deltas,
        context.world.read_modifier().resolved_channel_totals,
        0.0f);
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

    auto worker = context.world.read_worker();
    const auto previous = worker.conditions;
    WorkerMeterDeltas deltas {};
    const float step_real_seconds = static_cast<float>(std::max(0.0, context.fixed_step_seconds));
    accumulate_passive_deltas(
        deltas,
        previous,
        context.world.read_tile_local_weather(worker.position.tile_coord),
        context.world.read_modifier().resolved_channel_totals,
        static_cast<float>(runtime_minutes_from_real_seconds(context.fixed_step_seconds)),
        step_real_seconds);
    const float passive_energy_step_real_seconds =
        action_is_executing(context.world.read_action()) ? 0.0f : step_real_seconds;
    worker.conditions = resolve_worker_conditions(
        previous,
        deltas,
        context.world.read_modifier().resolved_channel_totals,
        passive_energy_step_real_seconds);
    const bool modified = worker_conditions_changed(previous, worker.conditions);
    if (modified)
    {
        context.world.write_worker(worker);
        emit_worker_meters_changed_if_needed(context);
    }
}
}  // namespace gs1
