#include "site/systems/local_weather_resolve_system.h"

#include "runtime/game_runtime.h"
#include "site/site_run_state.h"
#include "site/site_world_components.h"
#include "site/weather_contribution_logic.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4127)
#endif
#include <flecs.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>

namespace
{
constexpr std::uint32_t k_site_tile_state_changed_local_weather = 1U << 0U;
constexpr std::uint32_t k_site_tile_state_changed_support = 1U << 1U;
constexpr double k_site_one_probe_window_minutes = 0.25;

struct BaseLocalWeather final
{
    float heat {0.0f};
    float wind {0.0f};
    float dust {0.0f};
};

bool is_site_one_probe_tile(gs1::TileCoord coord) noexcept
{
    return (coord.x == 12 && coord.y >= 14 && coord.y <= 17) ||
        (coord.x == 13 && coord.y >= 14 && coord.y <= 17);
}

void emit_site_one_local_weather_probe_log(
    const gs1::SiteWorldAccess<gs1::LocalWeatherResolveSystem>& world,
    gs1::GameMessageQueue& message_queue,
    gs1::TileCoord coord,
    const BaseLocalWeather& base_weather,
    const gs1::SiteWorld::TileLocalWeatherData& resolved_weather,
    const gs1::SiteWorld::TileWeatherContributionData& total_contribution,
    float plant_density)
{
    if (!world.has_world() ||
        world.site_id_value() != 1U ||
        !is_site_one_probe_tile(coord))
    {
        return;
    }

    gs1::GameMessage message {};
    gs1::PresentLogMessage payload {};
    payload.level = GS1_LOG_LEVEL_DEBUG;
    std::snprintf(
        payload.text,
        sizeof(payload.text),
        "S1 lw (%d,%d) b%.0f/%.0f/%.0f l%.0f/%.0f/%.0f d%.1f",
        coord.x,
        coord.y,
        base_weather.heat,
        base_weather.wind,
        base_weather.dust,
        resolved_weather.heat,
        resolved_weather.wind,
        resolved_weather.dust,
        plant_density);
    message.type = gs1::GameMessageType::PresentLog;
    message.set_payload(payload);
    message_queue.push_back(message);

    payload.level = GS1_LOG_LEVEL_DEBUG;
    std::snprintf(
        payload.text,
        sizeof(payload.text),
        "S1 sup (%d,%d) w%.1f h%.1f z%.1f",
        coord.x,
        coord.y,
        total_contribution.wind_protection,
        total_contribution.heat_protection,
        total_contribution.dust_protection);
    message.set_payload(payload);
    message_queue.push_back(message);
}

void ensure_local_weather_runtime_buffers(
    gs1::LocalWeatherResolveState& state,
    std::size_t tile_count)
{
    if (state.last_total_weather_contributions.size() == tile_count)
    {
        return;
    }

    state.last_total_weather_contributions.assign(tile_count, gs1::zero_weather_contribution());
}

bool ecology_has_projected_plant_visual(
    gs1::PlantId plant_id,
    float plant_density) noexcept
{
    return plant_id.value != 0U ||
        plant_density > gs1::k_weather_contribution_density_epsilon_raw;
}

template <typename ContributionComponent>
gs1::SiteWorld::TileWeatherContributionData weather_contribution_from_component(
    const ContributionComponent& component) noexcept
{
    return gs1::SiteWorld::TileWeatherContributionData {
        component.heat_protection,
        component.wind_protection,
        component.dust_protection,
        component.fertility_improve,
        component.salinity_reduction,
        component.irrigation};
}

void emit_site_tile_state_changed(
    gs1::GameMessageQueue& message_queue,
    gs1::TileCoord coord,
    gs1::PlantId plant_id,
    float plant_density,
    float moisture,
    const gs1::SiteWorld::TileLocalWeatherData& local_weather,
    const gs1::SiteWorld::TileWeatherContributionData& total_contribution,
    bool local_weather_changed,
    bool support_changed)
{
    std::uint32_t changed_mask = 0U;
    if (local_weather_changed)
    {
        changed_mask |= k_site_tile_state_changed_local_weather;
    }
    if (support_changed)
    {
        changed_mask |= k_site_tile_state_changed_support;
    }
    if (changed_mask == 0U)
    {
        return;
    }

    gs1::GameMessage message {};
    message.type = gs1::GameMessageType::SiteTileStateChanged;
    message.set_payload(gs1::SiteTileStateChangedMessage {
        coord.x,
        coord.y,
        changed_mask,
        plant_id.value,
        plant_density,
        moisture,
        local_weather.heat,
        local_weather.dust,
        total_contribution.wind_protection,
        total_contribution.heat_protection,
        total_contribution.dust_protection});
    message_queue.push_back(message);
}

gs1::SiteWorld::TileLocalWeatherData resolve_local_weather(
    const BaseLocalWeather& base_weather,
    const gs1::SiteWorld::TileWeatherContributionData& total_contribution) noexcept
{
    return gs1::SiteWorld::TileLocalWeatherData {
        std::clamp(base_weather.heat - total_contribution.heat_protection, 0.0f, 100.0f),
        std::clamp(base_weather.wind - total_contribution.wind_protection, 0.0f, 100.0f),
        std::clamp(base_weather.dust - total_contribution.dust_protection, 0.0f, 100.0f)};
}

BaseLocalWeather compute_base_local_weather(
    const gs1::SiteWorldAccess<gs1::LocalWeatherResolveSystem>& world)
{
    const auto& weather = world.read_weather();
    const auto& modifiers = world.read_modifier().resolved_channel_totals;
    return BaseLocalWeather {
        std::clamp(weather.weather_heat * (1.0f + modifiers.heat), 0.0f, 100.0f),
        std::clamp(weather.weather_wind * (1.0f + modifiers.wind), 0.0f, 100.0f),
        std::clamp(weather.weather_dust * (1.0f + modifiers.dust), 0.0f, 100.0f)};
}

Gs1Status process_message(
    gs1::RuntimeInvocation& invocation,
    const gs1::GameMessage& message)
{
    auto access = gs1::make_game_state_access<gs1::LocalWeatherResolveSystem>(invocation);
    auto& site_run = access.template read<gs1::RuntimeActiveSiteRunTag>();
    if (!site_run.has_value())
    {
        return GS1_STATUS_INVALID_STATE;
    }

    gs1::SiteWorldAccess<gs1::LocalWeatherResolveSystem> world {*site_run};
    if (!world.has_world())
    {
        return GS1_STATUS_OK;
    }

    if (message.type == gs1::GameMessageType::SiteRunStarted)
    {
        world.own_local_weather_runtime().emit_full_snapshot_on_next_run = true;
    }

    return GS1_STATUS_OK;
}

void run_system(gs1::RuntimeInvocation& invocation)
{
    auto access = gs1::make_game_state_access<gs1::LocalWeatherResolveSystem>(invocation);
    auto& site_run = access.template read<gs1::RuntimeActiveSiteRunTag>();
    if (!site_run.has_value())
    {
        return;
    }

    gs1::SiteWorldAccess<gs1::LocalWeatherResolveSystem> world {*site_run};
    if (!world.has_world())
    {
        return;
    }

    auto& runtime = world.own_local_weather_runtime();
    ensure_local_weather_runtime_buffers(runtime, world.tile_count());

    const bool emit_full_snapshot = runtime.emit_full_snapshot_on_next_run;
    runtime.emit_full_snapshot_on_next_run = false;

    const auto base_weather = compute_base_local_weather(world);
    auto& ecs_world = site_run->site_world->ecs_world();
    auto& message_queue = invocation.game_message_queue();
    auto tile_query =
        ecs_world.query_builder<
            const gs1::site_ecs::TileIndex,
            const gs1::site_ecs::TileCoordComponent,
            const gs1::site_ecs::TilePlantSlot,
            const gs1::site_ecs::TilePlantDensity,
            const gs1::site_ecs::TileMoisture,
            gs1::site_ecs::TileHeat,
            gs1::site_ecs::TileWind,
            gs1::site_ecs::TileDust,
            const gs1::site_ecs::TilePlantWeatherContribution,
            const gs1::site_ecs::TileDeviceWeatherContribution>()
            .with<gs1::site_ecs::TileTag>()
            .build();

    // Query only the components this system actually reads or owns.
    tile_query.each(
        [&](flecs::entity,
            const gs1::site_ecs::TileIndex& index_component,
            const gs1::site_ecs::TileCoordComponent& coord_component,
            const gs1::site_ecs::TilePlantSlot& plant_slot,
            const gs1::site_ecs::TilePlantDensity& density_component,
            const gs1::site_ecs::TileMoisture& moisture_component,
            gs1::site_ecs::TileHeat& heat_component,
            gs1::site_ecs::TileWind& wind_component,
            gs1::site_ecs::TileDust& dust_component,
            const gs1::site_ecs::TilePlantWeatherContribution& plant_contribution_component,
            const gs1::site_ecs::TileDeviceWeatherContribution& device_contribution_component) {
            const std::size_t index = index_component.value;
            if (index >= runtime.last_total_weather_contributions.size())
            {
                return;
            }

            const auto total_contribution = gs1::clamp_weather_contribution(
                gs1::sum_weather_contributions(
                    weather_contribution_from_component(plant_contribution_component),
                    weather_contribution_from_component(device_contribution_component)));
            const auto resolved_weather = resolve_local_weather(base_weather, total_contribution);
            const auto previous_total_contribution = runtime.last_total_weather_contributions[index];
            const gs1::SiteWorld::TileLocalWeatherData previous_local_weather {
                heat_component.value,
                wind_component.value,
                dust_component.value};

            const bool heat_changed =
                std::fabs(previous_local_weather.heat - resolved_weather.heat) >
                gs1::k_weather_contribution_epsilon;
            const bool wind_changed =
                std::fabs(previous_local_weather.wind - resolved_weather.wind) >
                gs1::k_weather_contribution_epsilon;
            const bool dust_changed =
                std::fabs(previous_local_weather.dust - resolved_weather.dust) >
                gs1::k_weather_contribution_epsilon;
            const bool local_weather_changed = heat_changed || wind_changed || dust_changed;
            const bool support_changed =
                gs1::weather_contribution_changed(previous_total_contribution, total_contribution);

            if (local_weather_changed)
            {
                heat_component.value = resolved_weather.heat;
                wind_component.value = resolved_weather.wind;
                dust_component.value = resolved_weather.dust;
            }

            if ((emit_full_snapshot || local_weather_changed || support_changed))
            {
                const gs1::SiteWorld::TileLocalWeatherData emitted_local_weather = local_weather_changed
                    ? resolved_weather
                    : previous_local_weather;

                emit_site_tile_state_changed(
                    message_queue,
                    coord_component.value,
                    plant_slot.plant_id,
                    density_component.value,
                    moisture_component.value,
                    emitted_local_weather,
                    total_contribution,
                    emit_full_snapshot || local_weather_changed,
                    emit_full_snapshot || support_changed);
            }

            if ((emit_full_snapshot || local_weather_changed || support_changed) &&
                world.read_time().world_time_minutes <= k_site_one_probe_window_minutes)
            {
                emit_site_one_local_weather_probe_log(
                    world,
                    message_queue,
                    coord_component.value,
                    base_weather,
                    resolved_weather,
                    total_contribution,
                    density_component.value);
            }

            runtime.last_total_weather_contributions[index] = total_contribution;
        });
}
}  // namespace

namespace gs1
{
const char* LocalWeatherResolveSystem::name() const noexcept
{
    return access().system_name.data();
}

GameMessageSubscriptionSpan LocalWeatherResolveSystem::subscribed_game_messages() const noexcept
{
    static constexpr GameMessageType subscriptions[] = {GameMessageType::SiteRunStarted};
    return subscriptions;
}

HostMessageSubscriptionSpan LocalWeatherResolveSystem::subscribed_host_messages() const noexcept
{
    return {};
}

std::optional<Gs1RuntimeProfileSystemId> LocalWeatherResolveSystem::profile_system_id() const noexcept
{
    return GS1_RUNTIME_PROFILE_SYSTEM_LOCAL_WEATHER_RESOLVE;
}

std::optional<std::uint32_t> LocalWeatherResolveSystem::fixed_step_order() const noexcept
{
    return 8U;
}

Gs1Status LocalWeatherResolveSystem::process_game_message(
    RuntimeInvocation& invocation,
    const GameMessage& message)
{
    auto access = make_game_state_access<LocalWeatherResolveSystem>(invocation);
    auto& site_run = access.template read<RuntimeActiveSiteRunTag>();
    if (!site_run.has_value())
    {
        return GS1_STATUS_INVALID_STATE;
    }

    return process_message(invocation, message);
}

Gs1Status LocalWeatherResolveSystem::process_host_message(
    RuntimeInvocation& invocation,
    const Gs1HostMessage& message)
{
    (void)message;
    (void)invocation;
    return GS1_STATUS_OK;
}

void LocalWeatherResolveSystem::run(RuntimeInvocation& invocation)
{
    auto access = make_game_state_access<LocalWeatherResolveSystem>(invocation);
    auto& site_run = access.template read<RuntimeActiveSiteRunTag>();
    if (!site_run.has_value())
    {
        return;
    }

    run_system(invocation);
}
}  // namespace gs1

#ifdef _MSC_VER
#pragma warning(pop)
#endif

