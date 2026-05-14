#include "site/site_run_state.h"
#include "site/systems/camp_durability_system.h"
#include "content/defs/gameplay_tuning_defs.h"
#include "runtime/game_runtime.h"

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

void refresh_memory_with_current_state(RuntimeInvocation& invocation)
{
    SiteWorldAccess<CampDurabilitySystem> world {invocation};
    if (!world.has_world())
    {
        return;
    }

    auto& memory = g_camp_durability_memory;
    const auto& camp = world.read_camp();
    memory.site_run_id = world.site_run_id();
    memory.last_reported_durability = camp.camp_durability;
    memory.last_protection_resolved = camp.camp_protection_resolved;
    memory.last_delivery_operational = camp.delivery_point_operational;
    memory.last_shared_storage_access_enabled = camp.shared_storage_access_enabled;
}

void ensure_memory_for_run(RuntimeInvocation& invocation)
{
    SiteWorldAccess<CampDurabilitySystem> world {invocation};
    if (!world.has_world())
    {
        return;
    }

    auto& memory = g_camp_durability_memory;
    if (!memory.site_run_id.has_value() ||
        memory.site_run_id->value != world.site_run_id().value)
    {
        refresh_memory_with_current_state(invocation);
    }
}

void apply_projection_if_needed(
    RuntimeInvocation& invocation,
    float durability,
    bool protection_resolved,
    bool delivery_operational,
    bool shared_storage_access_enabled)
{
    SiteWorldAccess<CampDurabilitySystem> world {invocation};
    if (!world.has_world())
    {
        return;
    }

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

    memory.last_reported_durability = durability;
    memory.last_protection_resolved = protection_resolved;
    memory.last_delivery_operational = delivery_operational;
    memory.last_shared_storage_access_enabled = shared_storage_access_enabled;
}
}  // namespace

const char* CampDurabilitySystem::name() const noexcept
{
    return access().system_name.data();
}

GameMessageSubscriptionSpan CampDurabilitySystem::subscribed_game_messages() const noexcept
{
    static constexpr GameMessageType subscriptions[] = {GameMessageType::SiteRunStarted};
    return subscriptions;
}

HostMessageSubscriptionSpan CampDurabilitySystem::subscribed_host_messages() const noexcept
{
    return {};
}

std::optional<Gs1RuntimeProfileSystemId> CampDurabilitySystem::profile_system_id() const noexcept
{
    return GS1_RUNTIME_PROFILE_SYSTEM_CAMP_DURABILITY;
}

std::optional<std::uint32_t> CampDurabilitySystem::fixed_step_order() const noexcept
{
    return 10U;
}

Gs1Status CampDurabilitySystem::process_game_message(
    RuntimeInvocation& invocation,
    const GameMessage& message)
{
    if (message.type != GameMessageType::SiteRunStarted)
    {
        return GS1_STATUS_OK;
    }

    auto access = make_game_state_access<CampDurabilitySystem>(invocation);
    const double fixed_step_seconds = access.template read<RuntimeFixedStepSecondsTag>();
    SiteWorldAccess<CampDurabilitySystem> world {invocation};
    if (!world.has_world())
    {
        return GS1_STATUS_INVALID_STATE;
    }

    auto& camp = world.own_camp();
    camp.camp_durability = camp_durability_tuning().durability_max;
    camp.camp_protection_resolved = true;
    camp.delivery_point_operational = true;
    camp.shared_storage_access_enabled = true;
    refresh_memory_with_current_state(invocation);
    return GS1_STATUS_OK;
}

Gs1Status CampDurabilitySystem::process_host_message(
    RuntimeInvocation& invocation,
    const Gs1HostMessage& message)
{
    (void)message;
    (void)invocation;
    return GS1_STATUS_OK;
}

void CampDurabilitySystem::run(RuntimeInvocation& invocation)
{
    auto access = make_game_state_access<CampDurabilitySystem>(invocation);
    const double fixed_step_seconds = access.template read<RuntimeFixedStepSecondsTag>();
    SiteWorldAccess<CampDurabilitySystem> world {invocation};
    if (!world.has_world())
    {
        return;
    }

    ensure_memory_for_run(invocation);
    if (fixed_step_seconds <= 0.0)
    {
        return;
    }

    auto& camp = world.own_camp();
    const auto& tuning = camp_durability_tuning();
    const auto step_seconds = static_cast<float>(fixed_step_seconds);
    const float wear_rate_per_second =
        tuning.base_wear_per_second +
        compute_weather_intensity(world.read_weather()) * tuning.weather_wear_scale +
        (tuning.event_timeline_wear_per_second *
            compute_event_pressure_ratio(world.read_event()));
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
        invocation,
        durability,
        protection_resolved,
        delivery_operational,
        shared_storage_access_enabled);
}
}  // namespace gs1

