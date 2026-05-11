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
    gs1::SiteSystemContext<gs1::LocalWeatherResolveSystem>& context,
    gs1::TileCoord coord,
    const BaseLocalWeather& base_weather,
    const gs1::SiteWorld::TileLocalWeatherData& resolved_weather,
    const gs1::SiteWorld::TileWeatherContributionData& total_contribution,
    float plant_density)
{
    if (!context.world.has_world() ||
        context.world.site_id_value() != 1U ||
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
    context.message_queue.push_back(message);

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
    context.message_queue.push_back(message);
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
    gs1::SiteSystemContext<gs1::LocalWeatherResolveSystem>& context,
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
    context.message_queue.push_back(message);
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
    gs1::SiteSystemContext<gs1::LocalWeatherResolveSystem>& context)
{
    const auto& weather = context.world.read_weather();
    const auto& modifiers = context.world.read_modifier().resolved_channel_totals;
    return BaseLocalWeather {
        std::clamp(weather.weather_heat * (1.0f + modifiers.heat), 0.0f, 100.0f),
        std::clamp(weather.weather_wind * (1.0f + modifiers.wind), 0.0f, 100.0f),
        std::clamp(weather.weather_dust * (1.0f + modifiers.dust), 0.0f, 100.0f)};
}
}  // namespace

namespace gs1
{
bool LocalWeatherResolveSystem::subscribes_to(GameMessageType type) noexcept
{
    return type == GameMessageType::SiteRunStarted;
}

Gs1Status LocalWeatherResolveSystem::process_message(
    SiteSystemContext<LocalWeatherResolveSystem>& context,
    const GameMessage& message)
{
    if (!context.world.has_world())
    {
        return GS1_STATUS_OK;
    }

    if (message.type == GameMessageType::SiteRunStarted)
    {
        context.world.own_local_weather_runtime().emit_full_snapshot_on_next_run = true;
    }

    return GS1_STATUS_OK;
}

void LocalWeatherResolveSystem::run(SiteSystemContext<LocalWeatherResolveSystem>& context)
{
    if (!context.world.has_world())
    {
        return;
    }

    auto& runtime = context.world.own_local_weather_runtime();
    ensure_local_weather_runtime_buffers(runtime, context.world.tile_count());

    const bool emit_full_snapshot = runtime.emit_full_snapshot_on_next_run;
    runtime.emit_full_snapshot_on_next_run = false;

    const auto base_weather = compute_base_local_weather(context);
    auto& ecs_world = context.site_run.site_world->ecs_world();
    auto tile_query =
        ecs_world.query_builder<
            const site_ecs::TileIndex,
            const site_ecs::TileCoordComponent,
            const site_ecs::TilePlantSlot,
            const site_ecs::TilePlantDensity,
            const site_ecs::TileMoisture,
            site_ecs::TileHeat,
            site_ecs::TileWind,
            site_ecs::TileDust,
            const site_ecs::TilePlantWeatherContribution,
            const site_ecs::TileDeviceWeatherContribution>()
            .with<site_ecs::TileTag>()
            .build();

    // Query only the components this system actually reads or owns.
    tile_query.each(
        [&](flecs::entity,
            const site_ecs::TileIndex& index_component,
            const site_ecs::TileCoordComponent& coord_component,
            const site_ecs::TilePlantSlot& plant_slot,
            const site_ecs::TilePlantDensity& density_component,
            const site_ecs::TileMoisture& moisture_component,
            site_ecs::TileHeat& heat_component,
            site_ecs::TileWind& wind_component,
            site_ecs::TileDust& dust_component,
            const site_ecs::TilePlantWeatherContribution& plant_contribution_component,
            const site_ecs::TileDeviceWeatherContribution& device_contribution_component) {
            const std::size_t index = index_component.value;
            if (index >= runtime.last_total_weather_contributions.size())
            {
                return;
            }

            const auto total_contribution = clamp_weather_contribution(
                sum_weather_contributions(
                    weather_contribution_from_component(plant_contribution_component),
                    weather_contribution_from_component(device_contribution_component)));
            const auto resolved_weather = resolve_local_weather(base_weather, total_contribution);
            const auto previous_total_contribution = runtime.last_total_weather_contributions[index];
            const SiteWorld::TileLocalWeatherData previous_local_weather {
                heat_component.value,
                wind_component.value,
                dust_component.value};

            const bool heat_changed =
                std::fabs(previous_local_weather.heat - resolved_weather.heat) >
                k_weather_contribution_epsilon;
            const bool wind_changed =
                std::fabs(previous_local_weather.wind - resolved_weather.wind) >
                k_weather_contribution_epsilon;
            const bool dust_changed =
                std::fabs(previous_local_weather.dust - resolved_weather.dust) >
                k_weather_contribution_epsilon;
            const bool local_weather_changed = heat_changed || wind_changed || dust_changed;
            const bool support_changed =
                weather_contribution_changed(previous_total_contribution, total_contribution);

            if (local_weather_changed)
            {
                heat_component.value = resolved_weather.heat;
                wind_component.value = resolved_weather.wind;
                dust_component.value = resolved_weather.dust;
            }

            if ((emit_full_snapshot || local_weather_changed || support_changed))
            {
                const SiteWorld::TileLocalWeatherData emitted_local_weather = local_weather_changed
                    ? resolved_weather
                    : previous_local_weather;

                emit_site_tile_state_changed(
                    context,
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
                context.world.read_time().world_time_minutes <= k_site_one_probe_window_minutes)
            {
                emit_site_one_local_weather_probe_log(
                    context,
                    coord_component.value,
                    base_weather,
                    resolved_weather,
                    total_contribution,
                    density_component.value);
            }

            if ((wind_changed &&
                    ecology_has_projected_plant_visual(plant_slot.plant_id, density_component.value)) ||
                support_changed)
            {
                context.world.mark_tile_projection_dirty(coord_component.value);
            }

            runtime.last_total_weather_contributions[index] = total_contribution;
        });
}
GS1_IMPLEMENT_RUNTIME_SITE_MESSAGE_SYSTEM(
    LocalWeatherResolveSystem,
    GS1_RUNTIME_PROFILE_SYSTEM_LOCAL_WEATHER_RESOLVE,
    8U)
}  // namespace gs1

#ifdef _MSC_VER
#pragma warning(pop)
#endif
