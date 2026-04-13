#include "site/site_projection_update_flags.h"
#include "site/site_run_state.h"
#include "site/systems/camp_durability_system.h"

#include <algorithm>
#include <cmath>
#include <optional>

namespace gs1
{
namespace
{
struct CampDurabilityMemory final
{
    std::optional<SiteRunId> site_run_id {};
    float last_reported_durability {0.0f};
    bool last_protection_resolved {true};
    bool last_delivery_operational {true};
    bool last_shared_storage_access_enabled {true};
};

constexpr float k_camp_durability_max = 100.0f;
constexpr float k_base_wear_per_second = 0.01f;
constexpr float k_weather_wear_scale = 0.00075f;
constexpr float k_event_phase_wear_per_second = 0.04f;
constexpr float k_durability_dirty_threshold = 0.25f;

constexpr float k_protection_threshold = 70.0f;
constexpr float k_delivery_threshold = 50.0f;
constexpr float k_shared_storage_threshold = 30.0f;

CampDurabilityMemory g_camp_durability_memory {};

float compute_weather_intensity(const WeatherState& weather) noexcept
{
    return weather.weather_heat + weather.weather_wind + weather.weather_dust;
}

float resolve_event_phase_scale(EventPhase phase) noexcept
{
    switch (phase)
    {
    case EventPhase::Warning:
        return 0.25f;
    case EventPhase::Build:
        return 0.5f;
    case EventPhase::Peak:
        return 1.0f;
    case EventPhase::Decay:
        return 0.5f;
    case EventPhase::Aftermath:
        return 0.1f;
    default:
        return 0.0f;
    }
}

float compute_event_phase_wear(EventPhase phase) noexcept
{
    return k_event_phase_wear_per_second * resolve_event_phase_scale(phase);
}

void refresh_memory_with_current_state(SiteSystemContext& context)
{
    auto& memory = g_camp_durability_memory;
    memory.site_run_id = context.site_run.site_run_id;
    memory.last_reported_durability = context.site_run.camp.camp_durability;
    memory.last_protection_resolved = context.site_run.camp.camp_protection_resolved;
    memory.last_delivery_operational = context.site_run.camp.delivery_point_operational;
    memory.last_shared_storage_access_enabled =
        context.site_run.camp.shared_camp_storage_access_enabled;
}

void ensure_memory_for_run(SiteSystemContext& context)
{
    auto& memory = g_camp_durability_memory;
    if (!memory.site_run_id.has_value() ||
        memory.site_run_id->value != context.site_run.site_run_id.value)
    {
        refresh_memory_with_current_state(context);
    }
}

void apply_projection_if_needed(
    SiteSystemContext& context,
    float durability,
    bool protection_resolved,
    bool delivery_operational,
    bool shared_storage_access_enabled)
{
    auto& memory = g_camp_durability_memory;
    const bool durability_changed =
        std::fabs(memory.last_reported_durability - durability) >= k_durability_dirty_threshold;
    const bool service_flags_changed =
        memory.last_protection_resolved != protection_resolved ||
        memory.last_delivery_operational != delivery_operational ||
        memory.last_shared_storage_access_enabled != shared_storage_access_enabled;

    if (!durability_changed && !service_flags_changed)
    {
        return;
    }

    context.site_run.pending_projection_update_flags |= SITE_PROJECTION_UPDATE_CAMP;
    memory.last_reported_durability = durability;
    memory.last_protection_resolved = protection_resolved;
    memory.last_delivery_operational = delivery_operational;
    memory.last_shared_storage_access_enabled = shared_storage_access_enabled;
}
}  // namespace

bool CampDurabilitySystem::subscribes_to(GameCommandType type) noexcept
{
    return type == GameCommandType::SiteRunStarted;
}

Gs1Status CampDurabilitySystem::process_command(
    SiteSystemContext& context,
    const GameCommand& command)
{
    if (command.type != GameCommandType::SiteRunStarted)
    {
        return GS1_STATUS_OK;
    }

    auto& camp = context.site_run.camp;
    camp.camp_durability = k_camp_durability_max;
    camp.camp_protection_resolved = true;
    camp.delivery_point_operational = true;
    camp.shared_camp_storage_access_enabled = true;
    refresh_memory_with_current_state(context);
    return GS1_STATUS_OK;
}

void CampDurabilitySystem::run(SiteSystemContext& context)
{
    ensure_memory_for_run(context);
    if (context.fixed_step_seconds <= 0.0)
    {
        return;
    }

    auto& camp = context.site_run.camp;
    const auto step_seconds = static_cast<float>(context.fixed_step_seconds);
    const float wear_rate_per_second =
        k_base_wear_per_second +
        compute_weather_intensity(context.site_run.weather) * k_weather_wear_scale +
        compute_event_phase_wear(context.site_run.event.event_phase);
    const float wear_amount = wear_rate_per_second * step_seconds;
    const float previous = camp.camp_durability;
    camp.camp_durability =
        std::clamp(previous - wear_amount, 0.0f, k_camp_durability_max);

    const float durability = camp.camp_durability;
    const bool protection_resolved = durability >= k_protection_threshold;
    const bool delivery_operational = durability >= k_delivery_threshold;
    const bool shared_storage_access_enabled = durability >= k_shared_storage_threshold;

    camp.camp_protection_resolved = protection_resolved;
    camp.delivery_point_operational = delivery_operational;
    camp.shared_camp_storage_access_enabled = shared_storage_access_enabled;

    apply_projection_if_needed(
        context,
        durability,
        protection_resolved,
        delivery_operational,
        shared_storage_access_enabled);
}
}  // namespace gs1
