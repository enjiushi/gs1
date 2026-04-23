#include "site/site_projection_update_flags.h"
#include "site/site_run_state.h"
#include "site/systems/camp_durability_system.h"
#include "content/defs/gameplay_tuning_defs.h"

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

constexpr float k_durability_dirty_threshold = 0.25f;

CampDurabilityMemory g_camp_durability_memory {};

const CampDurabilityTuning& camp_durability_tuning() noexcept
{
    return gameplay_tuning_def().camp_durability;
}

float compute_weather_intensity(const WeatherState& weather) noexcept
{
    return weather.weather_heat + weather.weather_wind + weather.weather_dust;
}

float compute_event_pressure_ratio(const EventState& event) noexcept
{
    const auto& tuning = camp_durability_tuning();
    const float total_pressure =
        std::max(event.event_heat_pressure, 0.0f) +
        std::max(event.event_wind_pressure, 0.0f) +
        std::max(event.event_dust_pressure, 0.0f);
    return std::clamp(total_pressure / tuning.peak_event_pressure_total, 0.0f, 1.0f);
}

void refresh_memory_with_current_state(SiteSystemContext<CampDurabilitySystem>& context)
{
    auto& memory = g_camp_durability_memory;
    const auto& camp = context.world.read_camp();
    memory.site_run_id = context.world.site_run_id();
    memory.last_reported_durability = camp.camp_durability;
    memory.last_protection_resolved = camp.camp_protection_resolved;
    memory.last_delivery_operational = camp.delivery_point_operational;
    memory.last_shared_storage_access_enabled = camp.shared_storage_access_enabled;
}

void ensure_memory_for_run(SiteSystemContext<CampDurabilitySystem>& context)
{
    auto& memory = g_camp_durability_memory;
    if (!memory.site_run_id.has_value() ||
        memory.site_run_id->value != context.world.site_run_id().value)
    {
        refresh_memory_with_current_state(context);
    }
}

void apply_projection_if_needed(
    SiteSystemContext<CampDurabilitySystem>& context,
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

    context.world.mark_projection_dirty(SITE_PROJECTION_UPDATE_CAMP);
    memory.last_reported_durability = durability;
    memory.last_protection_resolved = protection_resolved;
    memory.last_delivery_operational = delivery_operational;
    memory.last_shared_storage_access_enabled = shared_storage_access_enabled;
}
}  // namespace

bool CampDurabilitySystem::subscribes_to(GameMessageType type) noexcept
{
    return type == GameMessageType::SiteRunStarted;
}

Gs1Status CampDurabilitySystem::process_message(
    SiteSystemContext<CampDurabilitySystem>& context,
    const GameMessage& message)
{
    if (message.type != GameMessageType::SiteRunStarted)
    {
        return GS1_STATUS_OK;
    }

    auto& camp = context.world.own_camp();
    camp.camp_durability = camp_durability_tuning().durability_max;
    camp.camp_protection_resolved = true;
    camp.delivery_point_operational = true;
    camp.shared_storage_access_enabled = true;
    refresh_memory_with_current_state(context);
    return GS1_STATUS_OK;
}

void CampDurabilitySystem::run(SiteSystemContext<CampDurabilitySystem>& context)
{
    ensure_memory_for_run(context);
    if (context.fixed_step_seconds <= 0.0)
    {
        return;
    }

    auto& camp = context.world.own_camp();
    const auto& tuning = camp_durability_tuning();
    const auto step_seconds = static_cast<float>(context.fixed_step_seconds);
    const float wear_rate_per_second =
        tuning.base_wear_per_second +
        compute_weather_intensity(context.world.read_weather()) * tuning.weather_wear_scale +
        (tuning.event_timeline_wear_per_second *
            compute_event_pressure_ratio(context.world.read_event()));
    const float wear_amount = wear_rate_per_second * step_seconds;
    const float previous = camp.camp_durability;
    camp.camp_durability =
        std::clamp(previous - wear_amount, 0.0f, tuning.durability_max);

    const float durability = camp.camp_durability;
    const bool protection_resolved = durability >= tuning.protection_threshold;
    const bool delivery_operational = durability >= tuning.delivery_threshold;
    const bool shared_storage_access_enabled = durability >= tuning.shared_storage_threshold;

    camp.camp_protection_resolved = protection_resolved;
    camp.delivery_point_operational = delivery_operational;
    camp.shared_storage_access_enabled = shared_storage_access_enabled;

    apply_projection_if_needed(
        context,
        durability,
        protection_resolved,
        delivery_operational,
        shared_storage_access_enabled);
}
}  // namespace gs1
