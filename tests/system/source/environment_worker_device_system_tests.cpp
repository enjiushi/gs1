#include <algorithm>
#include <cmath>

#include "content/defs/technology_defs.h"
#include "messages/game_message.h"
#include "site/site_projection_update_flags.h"
#include "site/site_world_access.h"
#include "site/systems/camp_durability_system.h"
#include "site/systems/device_maintenance_system.h"
#include "site/systems/device_support_system.h"
#include "site/systems/local_weather_resolve_system.h"
#include "site/systems/modifier_system.h"
#include "site/systems/site_flow_system.h"
#include "site/systems/site_time_system.h"
#include "site/systems/weather_event_system.h"
#include "site/systems/worker_condition_system.h"
#include "testing/system_test_registry.h"
#include "system_test_fixtures.h"

namespace
{
using gs1::CampDurabilitySystem;
using gs1::DeviceMaintenanceSystem;
using gs1::DeviceSupportSystem;
using gs1::GameMessage;
using gs1::GameMessageQueue;
using gs1::GameMessageType;
using gs1::LocalWeatherResolveSystem;
using gs1::ModifierSystem;
using gs1::SiteFlowSystem;
using gs1::SiteRunStartedMessage;
using gs1::SiteTimeSystem;
using gs1::TileCoord;
using gs1::WeatherEventSystem;
using gs1::WorkerConditionSystem;
using namespace gs1::testing::fixtures;

template <typename Payload>
GameMessage make_message(gs1::GameMessageType type, const Payload& payload)
{
    GameMessage message {};
    message.type = type;
    message.set_payload(payload);
    return message;
}

void weather_event_site_run_started_keeps_site_one_in_normal_conditions(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_one_run = make_test_site_run(1U, 1101U);
    auto site_two_run = make_test_site_run(2U, 1102U);
    GameMessageQueue queue_one {};
    GameMessageQueue queue_two {};

    auto site_one_context = make_site_context<WeatherEventSystem>(campaign, site_one_run, queue_one);
    auto site_two_context = make_site_context<WeatherEventSystem>(campaign, site_two_run, queue_two);
    const auto started_one = make_message(
        GameMessageType::SiteRunStarted,
        SiteRunStartedMessage {1U, 1U, 101U, 1U, 42ULL});
    const auto started_two = make_message(
        GameMessageType::SiteRunStarted,
        SiteRunStartedMessage {2U, 1U, 102U, 1U, 42ULL});

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        WeatherEventSystem::process_message(site_one_context, started_one) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, site_one_run.weather.forecast_profile_state.forecast_profile_id == 1U);
    GS1_SYSTEM_TEST_CHECK(context, !site_one_run.event.active_event_template_id.has_value());
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(site_one_run.event.start_time_minutes, 0.0));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(site_one_run.event.peak_time_minutes, 0.0));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(site_one_run.event.peak_duration_minutes, 0.0));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(site_one_run.event.end_time_minutes, 0.0));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(site_one_run.weather.weather_heat, 0.0f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(site_one_run.weather.weather_wind, 0.0f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(site_one_run.weather.weather_dust, 0.0f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(site_one_run.event.event_heat_pressure, 0.0f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(site_one_run.event.event_wind_pressure, 0.0f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(site_one_run.event.event_dust_pressure, 0.0f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(site_one_run.weather.weather_wind_direction_degrees, 0.0f));
    GS1_SYSTEM_TEST_CHECK(context, site_one_run.pending_projection_update_flags == 0U);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        WeatherEventSystem::process_message(site_two_context, started_two) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, !site_two_run.event.active_event_template_id.has_value());
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(site_two_run.weather.weather_heat, 0.0f));
}

void weather_event_run_advances_active_event_through_full_lifecycle(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 1103U);
    GameMessageQueue queue {};
    auto site_time_context = make_site_context<SiteTimeSystem>(campaign, site_run, queue, 0.25);
    auto weather_context = make_site_context<WeatherEventSystem>(campaign, site_run, queue, 0.25);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        WeatherEventSystem::process_message(
            weather_context,
            make_message(
                GameMessageType::SiteRunStarted,
                SiteRunStartedMessage {1U, 1U, 101U, 1U, 42ULL})) == GS1_STATUS_OK);

    site_run.event.active_event_template_id = gs1::EventTemplateId {1U};
    site_run.event.start_time_minutes = 5.0;
    site_run.event.peak_time_minutes = 10.0;
    site_run.event.peak_duration_minutes = 5.0;
    site_run.event.end_time_minutes = 20.0;

    for (int index = 0; index < 10; ++index)
    {
        SiteTimeSystem::run(site_time_context);
        WeatherEventSystem::run(weather_context);
    }
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(site_run.weather.weather_heat, 0.0f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(site_run.weather.weather_wind, 0.0f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(site_run.weather.weather_dust, 0.0f));

    for (int index = 0; index < 25; ++index)
    {
        SiteTimeSystem::run(site_time_context);
        WeatherEventSystem::run(weather_context);
    }
    GS1_SYSTEM_TEST_CHECK(context, site_run.weather.weather_heat > 0.0f);
    GS1_SYSTEM_TEST_CHECK(context, site_run.weather.weather_heat < 15.0f);
    GS1_SYSTEM_TEST_CHECK(context, site_run.weather.weather_wind > 0.0f);
    GS1_SYSTEM_TEST_CHECK(context, site_run.weather.weather_wind < 10.0f);
    GS1_SYSTEM_TEST_CHECK(context, site_run.weather.weather_dust > 0.0f);
    GS1_SYSTEM_TEST_CHECK(context, site_run.weather.weather_dust < 5.0f);

    for (int index = 0; index < 120; ++index)
    {
        SiteTimeSystem::run(site_time_context);
        WeatherEventSystem::run(weather_context);
    }
    GS1_SYSTEM_TEST_CHECK(context, !site_run.event.active_event_template_id.has_value());
    GS1_SYSTEM_TEST_CHECK(context, site_run.weather.weather_heat < 0.01f);
    GS1_SYSTEM_TEST_CHECK(context, site_run.weather.weather_wind < 0.01f);
    GS1_SYSTEM_TEST_CHECK(context, site_run.weather.weather_dust < 0.01f);
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(site_run.event.aftermath_relief_resolved, 1.0f));
}

void weather_event_does_not_reseed_when_event_is_already_active(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 1104U);
    GameMessageQueue queue {};
    auto site_context = make_site_context<WeatherEventSystem>(campaign, site_run, queue);

    site_run.weather.forecast_profile_state.forecast_profile_id = 7U;
    site_run.weather.site_weather_bias = 2.5f;
    site_run.weather.weather_heat = 99.0f;
    site_run.event.active_event_template_id = gs1::EventTemplateId {8U};
    site_run.event.start_time_minutes = 1.0;
    site_run.event.peak_time_minutes = 4.0;
    site_run.event.peak_duration_minutes = 2.0;
    site_run.event.end_time_minutes = 9.0;

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        WeatherEventSystem::process_message(
            site_context,
            make_message(
                GameMessageType::SiteRunStarted,
                SiteRunStartedMessage {1U, 1U, 101U, 1U, 42ULL})) == GS1_STATUS_OK);

    GS1_SYSTEM_TEST_CHECK(context, site_run.weather.forecast_profile_state.forecast_profile_id == 7U);
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(site_run.weather.site_weather_bias, 2.5f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(site_run.weather.weather_heat, 99.0f));
    GS1_SYSTEM_TEST_CHECK(context, site_run.event.active_event_template_id.has_value());
    GS1_SYSTEM_TEST_CHECK(context, site_run.event.active_event_template_id->value == 8U);
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(site_run.event.start_time_minutes, 1.0));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(site_run.event.peak_time_minutes, 4.0));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(site_run.event.peak_duration_minutes, 2.0));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(site_run.event.end_time_minutes, 9.0));
}

void weather_event_timeline_progress_tracks_site_clock_progress(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 1105U);
    GameMessageQueue queue {};
    auto site_time_context = make_site_context<SiteTimeSystem>(campaign, site_run, queue, 0.25);
    auto weather_context = make_site_context<WeatherEventSystem>(campaign, site_run, queue, 0.25);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        WeatherEventSystem::process_message(
            weather_context,
            make_message(
                GameMessageType::SiteRunStarted,
                SiteRunStartedMessage {1U, 1U, 101U, 1U, 42ULL})) == GS1_STATUS_OK);

    site_run.event.active_event_template_id = gs1::EventTemplateId {1U};
    site_run.event.start_time_minutes = 0.0;
    site_run.event.peak_time_minutes = 5.0;
    site_run.event.peak_duration_minutes = 3.0;
    site_run.event.end_time_minutes = 10.0;

    const double initial_world_minutes = site_run.clock.world_time_minutes;

    for (int index = 0; index < 10; ++index)
    {
        SiteTimeSystem::run(site_time_context);
        WeatherEventSystem::run(weather_context);
    }

    const double world_minutes_elapsed =
        site_run.clock.world_time_minutes - initial_world_minutes;
    GS1_SYSTEM_TEST_CHECK(context, std::fabs(world_minutes_elapsed - 2.0) <= 0.0001);
    GS1_SYSTEM_TEST_CHECK(context, std::fabs(site_run.event.event_heat_pressure - 6.0f) <= 0.0001f);
    GS1_SYSTEM_TEST_CHECK(context, std::fabs(site_run.event.event_wind_pressure - 4.0f) <= 0.0001f);
    GS1_SYSTEM_TEST_CHECK(context, std::fabs(site_run.event.event_dust_pressure - 2.0f) <= 0.0001f);
}

void weather_event_timeline_ramp_down_smooths_weather_toward_lower_target(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 1106U);
    GameMessageQueue queue {};
    auto weather_context = make_site_context<WeatherEventSystem>(campaign, site_run, queue);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        WeatherEventSystem::process_message(
            weather_context,
            make_message(
                GameMessageType::SiteRunStarted,
                SiteRunStartedMessage {1U, 1U, 101U, 1U, 42ULL})) == GS1_STATUS_OK);

    site_run.event.active_event_template_id = gs1::EventTemplateId {1U};
    site_run.event.start_time_minutes = 0.0;
    site_run.event.peak_time_minutes = 2.0;
    site_run.event.peak_duration_minutes = 1.0;
    site_run.event.end_time_minutes = 8.0;
    site_run.event.event_heat_pressure = 15.0f;
    site_run.event.event_wind_pressure = 10.0f;
    site_run.event.event_dust_pressure = 5.0f;
    site_run.weather.weather_heat = 15.0f;
    site_run.weather.weather_wind = 10.0f;
    site_run.weather.weather_dust = 5.0f;
    site_run.weather.weather_wind_direction_degrees = 74.0f;
    site_run.clock.world_time_minutes = 6.0;

    WeatherEventSystem::run(weather_context);

    GS1_SYSTEM_TEST_CHECK(context, site_run.event.event_heat_pressure < 15.0f);
    GS1_SYSTEM_TEST_CHECK(context, site_run.event.event_wind_pressure < 10.0f);
    GS1_SYSTEM_TEST_CHECK(context, site_run.event.event_dust_pressure < 5.0f);
    GS1_SYSTEM_TEST_CHECK(context, site_run.weather.weather_heat < 15.0f);
    GS1_SYSTEM_TEST_CHECK(context, site_run.weather.weather_heat > site_run.event.event_heat_pressure);
    GS1_SYSTEM_TEST_CHECK(context, site_run.weather.weather_wind < 10.0f);
    GS1_SYSTEM_TEST_CHECK(context, site_run.weather.weather_wind > site_run.event.event_wind_pressure);
    GS1_SYSTEM_TEST_CHECK(context, site_run.weather.weather_dust < 5.0f);
    GS1_SYSTEM_TEST_CHECK(context, site_run.weather.weather_dust > site_run.event.event_dust_pressure);
}

void weather_event_highway_objective_schedules_repeating_waves_with_one_sided_wind(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(5U, 1107U);
    GameMessageQueue queue {};
    auto weather_context = make_site_context<WeatherEventSystem>(campaign, site_run, queue);

    configure_highway_protection_objective(
        site_run,
        gs1::SiteObjectiveTargetEdge::East,
        30.0,
        0.5f);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        WeatherEventSystem::process_message(
            weather_context,
            make_message(
                GameMessageType::SiteRunStarted,
                SiteRunStartedMessage {5U, 1U, 105U, 1U, 42ULL})) == GS1_STATUS_OK);

    GS1_SYSTEM_TEST_CHECK(context, site_run.weather.forecast_profile_state.forecast_profile_id == 2U);
    GS1_SYSTEM_TEST_CHECK(context, site_run.event.minutes_until_next_wave >= 4.0);
    GS1_SYSTEM_TEST_CHECK(context, site_run.event.minutes_until_next_wave <= 8.0);

    for (int index = 0; index < 40; ++index)
    {
        WeatherEventSystem::run(weather_context);
    }

    GS1_SYSTEM_TEST_REQUIRE(context, site_run.event.active_event_template_id.has_value());
    GS1_SYSTEM_TEST_CHECK(context, site_run.event.wave_sequence_index == 1U);
    GS1_SYSTEM_TEST_CHECK(context, site_run.event.peak_time_minutes > site_run.event.start_time_minutes);
    GS1_SYSTEM_TEST_CHECK(context, site_run.event.end_time_minutes > site_run.event.peak_time_minutes);
    GS1_SYSTEM_TEST_CHECK(context, site_run.weather.weather_wind_direction_degrees < 90.0f ||
        site_run.weather.weather_wind_direction_degrees > 270.0f);
}

void local_weather_resolve_spreads_full_refresh_over_multiple_runs(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(2U, 1201U, 101U, 8U, 8U);
    GameMessageQueue queue {};
    auto site_context = make_site_context<LocalWeatherResolveSystem>(campaign, site_run, queue);

    site_run.weather.weather_heat = 10.0f;
    site_run.weather.weather_wind = 5.0f;
    site_run.weather.weather_dust = 3.0f;

    GS1_SYSTEM_TEST_CHECK(context, !LocalWeatherResolveSystem::subscribes_to(GameMessageType::SiteRunStarted));
    GS1_SYSTEM_TEST_CHECK(context, LocalWeatherResolveSystem::subscribes_to(GameMessageType::TileEcologyChanged));
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        LocalWeatherResolveSystem::process_message(
            site_context,
            make_message(
                GameMessageType::SiteRunStarted,
                SiteRunStartedMessage {1U, 1U, 101U, 1U, 42ULL})) == GS1_STATUS_OK);

    LocalWeatherResolveSystem::run(site_context);

    const auto first_pass_weather = site_run.site_world->tile_local_weather(TileCoord {0, 0});
    const auto deferred_weather = site_run.site_world->tile_local_weather(TileCoord {0, 4});
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(first_pass_weather.heat, 10.0f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(first_pass_weather.wind, 5.0f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(first_pass_weather.dust, 4.25f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(deferred_weather.heat, 0.0f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(deferred_weather.wind, 0.0f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(deferred_weather.dust, 0.0f));

    LocalWeatherResolveSystem::run(site_context);
    const auto second_pass_weather = site_run.site_world->tile_local_weather(TileCoord {0, 4});
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(second_pass_weather.heat, 10.0f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(second_pass_weather.wind, 5.0f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(second_pass_weather.dust, 4.25f));
}

void local_weather_resolve_refreshes_dirty_neighborhood_from_tile_ecology_changed(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(2U, 1202U);
    GameMessageQueue queue {};
    auto site_context = make_site_context<LocalWeatherResolveSystem>(campaign, site_run, queue);

    site_run.weather.weather_heat = 10.0f;
    site_run.weather.weather_wind = 5.0f;
    site_run.weather.weather_dust = 3.0f;
    site_run.weather.weather_wind_direction_degrees = 0.0f;

    for (int iteration = 0; iteration < 8; ++iteration)
    {
        LocalWeatherResolveSystem::run(site_context);
    }

    auto planted_tile = site_run.site_world->tile_at(TileCoord {3, 3});
    planted_tile.ecology.plant_id = gs1::PlantId {1U};
    planted_tile.ecology.plant_density = 0.5f;
    site_run.site_world->set_tile(TileCoord {3, 3}, planted_tile);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        LocalWeatherResolveSystem::process_message(
            site_context,
            make_message(
                GameMessageType::TileEcologyChanged,
                gs1::TileEcologyChangedMessage {
                    3,
                    3,
                    gs1::TILE_ECOLOGY_CHANGED_OCCUPANCY |
                        gs1::TILE_ECOLOGY_CHANGED_DENSITY,
                    1U,
                    0U,
                    0.5f,
                    0.0f})) == GS1_STATUS_OK);

    LocalWeatherResolveSystem::run(site_context);

    const auto planted_weather = site_run.site_world->tile_local_weather(TileCoord {3, 3});
    const auto downwind_weather = site_run.site_world->tile_local_weather(TileCoord {4, 3});
    const auto upwind_weather = site_run.site_world->tile_local_weather(TileCoord {2, 3});
    const auto far_weather = site_run.site_world->tile_local_weather(TileCoord {7, 7});
    GS1_SYSTEM_TEST_CHECK(context, planted_weather.heat < 10.0f);
    GS1_SYSTEM_TEST_CHECK(context, planted_weather.wind < 5.0f);
    GS1_SYSTEM_TEST_CHECK(context, planted_weather.dust < 4.25f);
    GS1_SYSTEM_TEST_CHECK(context, downwind_weather.heat < 10.0f);
    GS1_SYSTEM_TEST_CHECK(context, downwind_weather.wind < 5.0f);
    GS1_SYSTEM_TEST_CHECK(context, downwind_weather.dust < 4.25f);
    GS1_SYSTEM_TEST_CHECK(context, upwind_weather.heat < 10.0f);
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(upwind_weather.wind, 5.0f));
    GS1_SYSTEM_TEST_CHECK(context, upwind_weather.dust < 4.25f);
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(far_weather.heat, 10.0f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(far_weather.wind, 5.0f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(far_weather.dust, 4.25f));
}

void local_weather_resolve_applies_directional_wind_shadow_falloff_for_windbreak_plants(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(2U, 1204U, 101U, 6U, 5U);
    GameMessageQueue queue {};
    auto site_context = make_site_context<LocalWeatherResolveSystem>(campaign, site_run, queue);

    site_run.weather.weather_wind = 12.0f;
    site_run.weather.weather_wind_direction_degrees = 0.0f;

    auto tile = site_run.site_world->tile_at(TileCoord {1, 2});
    tile.ecology.plant_id = gs1::PlantId {gs1::k_plant_wind_reed};
    tile.ecology.plant_density = 1.0f;
    site_run.site_world->set_tile(TileCoord {1, 2}, tile);

    LocalWeatherResolveSystem::run(site_context);

    const auto own_weather = site_run.site_world->tile_local_weather(TileCoord {1, 2});
    const auto first_downwind = site_run.site_world->tile_local_weather(TileCoord {2, 2});
    const auto second_downwind = site_run.site_world->tile_local_weather(TileCoord {3, 2});
    const auto upwind = site_run.site_world->tile_local_weather(TileCoord {0, 2});

    GS1_SYSTEM_TEST_CHECK(context, own_weather.wind < 12.0f);
    GS1_SYSTEM_TEST_CHECK(context, first_downwind.wind < 12.0f);
    GS1_SYSTEM_TEST_CHECK(context, second_downwind.wind < 12.0f);
    GS1_SYSTEM_TEST_CHECK(context, first_downwind.wind < second_downwind.wind);
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(upwind.wind, 12.0f));
}

void local_weather_resolve_applies_device_wind_protection_value_and_range(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(2U, 1203U, 101U, 5U, 5U);
    GameMessageQueue queue {};
    auto site_context = make_site_context<LocalWeatherResolveSystem>(campaign, site_run, queue);

    site_run.weather.weather_heat = 10.0f;
    site_run.weather.weather_wind = 10.0f;
    site_run.weather.weather_dust = 3.0f;
    site_run.weather.weather_wind_direction_degrees = 0.0f;

    LocalWeatherResolveSystem::run(site_context);

    auto tile = site_run.site_world->tile_at(TileCoord {2, 2});
    tile.device.structure_id = gs1::StructureId {gs1::k_structure_wind_fence};
    tile.device.device_integrity = 1.0f;
    tile.device.device_efficiency = 0.5f;
    site_run.site_world->set_tile(TileCoord {2, 2}, tile);

    GS1_SYSTEM_TEST_CHECK(context, LocalWeatherResolveSystem::subscribes_to(GameMessageType::SiteDevicePlaced));
    GS1_SYSTEM_TEST_CHECK(context, LocalWeatherResolveSystem::subscribes_to(GameMessageType::SiteDeviceBroken));
    GS1_SYSTEM_TEST_CHECK(context, LocalWeatherResolveSystem::subscribes_to(GameMessageType::SiteDeviceRepaired));
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        LocalWeatherResolveSystem::process_message(
            site_context,
            make_message(
                GameMessageType::SiteDevicePlaced,
                gs1::SiteDevicePlacedMessage {
                    1U,
                    2,
                    2,
                    gs1::k_structure_wind_fence,
                    0U})) == GS1_STATUS_OK);

    LocalWeatherResolveSystem::run(site_context);

    const auto own_weather = site_run.site_world->tile_local_weather(TileCoord {2, 2});
    const auto neighbor_weather = site_run.site_world->tile_local_weather(TileCoord {3, 2});
    const auto upwind_weather = site_run.site_world->tile_local_weather(TileCoord {1, 2});
    const auto out_of_range_weather = site_run.site_world->tile_local_weather(TileCoord {4, 2});
    GS1_SYSTEM_TEST_CHECK(context, own_weather.wind < 10.0f);
    GS1_SYSTEM_TEST_CHECK(context, neighbor_weather.wind < 10.0f);
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(upwind_weather.wind, 10.0f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(neighbor_weather.heat, 10.0f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(neighbor_weather.dust, 4.25f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(out_of_range_weather.wind, 10.0f));
}

void worker_condition_requested_delta_emits_initial_full_mask_and_clamps(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 1301U);
    GameMessageQueue queue {};
    auto site_context = make_site_context<WorkerConditionSystem>(campaign, site_run, queue);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        WorkerConditionSystem::process_message(
            site_context,
            make_message(
                GameMessageType::WorkerMeterDeltaRequested,
                gs1::WorkerMeterDeltaRequestedMessage {
                    1U,
                    gs1::WORKER_METER_CHANGED_HEALTH |
                        gs1::WORKER_METER_CHANGED_ENERGY_CAP |
                        gs1::WORKER_METER_CHANGED_ENERGY,
                    -150.0f,
                    0.0f,
                    0.0f,
                    150.0f,
                    500.0f,
                    0.0f,
                    0.0f})) == GS1_STATUS_OK);

    const auto worker = gs1::site_world_access::worker_conditions(site_run);
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(worker.health, 0.0f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(worker.energy_cap, 200.0f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(worker.energy, 200.0f));
    GS1_SYSTEM_TEST_REQUIRE(context, queue.size() == 1U);
    GS1_SYSTEM_TEST_CHECK(context, queue.front().type == GameMessageType::WorkerMetersChanged);
    GS1_SYSTEM_TEST_CHECK(
        context,
        queue.front().payload_as<gs1::WorkerMetersChangedMessage>().changed_mask ==
            (gs1::WORKER_METER_CHANGED_HEALTH |
                gs1::WORKER_METER_CHANGED_HYDRATION |
                gs1::WORKER_METER_CHANGED_NOURISHMENT |
                gs1::WORKER_METER_CHANGED_ENERGY_CAP |
                gs1::WORKER_METER_CHANGED_ENERGY |
                gs1::WORKER_METER_CHANGED_MORALE |
                gs1::WORKER_METER_CHANGED_WORK_EFFICIENCY));
}

void worker_condition_run_drains_resources_and_health_when_starving(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 1302U);
    GameMessageQueue queue {};
    auto site_context = make_site_context<WorkerConditionSystem>(campaign, site_run, queue, 100.0);

    auto worker = gs1::site_world_access::worker_conditions(site_run);
    worker.health = 10.0f;
    worker.hydration = 0.0f;
    worker.nourishment = 0.0f;
    worker.energy = 50.0f;
    worker.morale = 50.0f;
    gs1::site_world_access::set_worker_conditions(site_run, worker);

    WorkerConditionSystem::run(site_context);

    const auto updated = gs1::site_world_access::worker_conditions(site_run);
    GS1_SYSTEM_TEST_CHECK(context, updated.health < 10.0f);
    GS1_SYSTEM_TEST_CHECK(context, updated.energy < 50.0f);
    GS1_SYSTEM_TEST_CHECK(context, updated.morale < 50.0f);
    GS1_SYSTEM_TEST_CHECK(context, queue.size() == 1U);
}

void worker_condition_run_applies_wind_exposure_and_shelter_softens_it(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto exposed_run = make_test_site_run(1U, 1303U);
    auto sheltered_run = make_test_site_run(1U, 1304U);
    GameMessageQueue exposed_queue {};
    GameMessageQueue sheltered_queue {};
    auto exposed_context =
        make_site_context<WorkerConditionSystem>(campaign, exposed_run, exposed_queue, 10.0);
    auto sheltered_context =
        make_site_context<WorkerConditionSystem>(campaign, sheltered_run, sheltered_queue, 10.0);

    auto exposed_tile = exposed_run.site_world->tile_at(TileCoord {2, 2});
    exposed_tile.local_weather.wind = 80.0f;
    exposed_run.site_world->set_tile(TileCoord {2, 2}, exposed_tile);

    auto sheltered_tile = sheltered_run.site_world->tile_at(TileCoord {2, 2});
    sheltered_tile.local_weather.wind = 80.0f;
    sheltered_run.site_world->set_tile(TileCoord {2, 2}, sheltered_tile);

    auto exposed_worker = gs1::site_world_access::worker_conditions(exposed_run);
    exposed_worker.hydration = 50.0f;
    exposed_worker.energy = 60.0f;
    exposed_worker.morale = 70.0f;
    gs1::site_world_access::set_worker_conditions(exposed_run, exposed_worker);

    auto sheltered_worker = gs1::site_world_access::worker_conditions(sheltered_run);
    sheltered_worker.hydration = 50.0f;
    sheltered_worker.energy = 60.0f;
    sheltered_worker.morale = 70.0f;
    sheltered_worker.is_sheltered = true;
    gs1::site_world_access::set_worker_conditions(sheltered_run, sheltered_worker);

    WorkerConditionSystem::run(exposed_context);
    WorkerConditionSystem::run(sheltered_context);

    const auto exposed = gs1::site_world_access::worker_conditions(exposed_run);
    const auto sheltered = gs1::site_world_access::worker_conditions(sheltered_run);
    GS1_SYSTEM_TEST_CHECK(context, exposed.hydration < 50.0f);
    GS1_SYSTEM_TEST_CHECK(context, exposed.energy < 60.0f);
    GS1_SYSTEM_TEST_CHECK(context, exposed.morale < 70.0f);
    GS1_SYSTEM_TEST_CHECK(context, exposed.hydration < sheltered.hydration);
    GS1_SYSTEM_TEST_CHECK(context, exposed.energy < sheltered.energy);
    GS1_SYSTEM_TEST_CHECK(context, exposed.morale < sheltered.morale);
    GS1_SYSTEM_TEST_CHECK(context, exposed_queue.size() == 1U);
    GS1_SYSTEM_TEST_CHECK(context, sheltered_queue.size() == 1U);
}

void device_support_updates_efficiency_and_water_from_heat(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 1401U);
    GameMessageQueue queue {};
    auto site_context = make_site_context<DeviceSupportSystem>(campaign, site_run, queue, 10.0);

    auto tile = site_run.site_world->tile_at(TileCoord {2, 2});
    tile.local_weather.heat = 10.0f;
    tile.device.structure_id = gs1::StructureId {9U};
    tile.device.device_integrity = 0.4f;
    tile.device.device_efficiency = 1.0f;
    tile.device.device_stored_water = 1.0f;
    site_run.site_world->set_tile(TileCoord {2, 2}, tile);

    DeviceSupportSystem::run(site_context);

    const auto device = site_run.site_world->tile_device(TileCoord {2, 2});
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(device.device_efficiency, 0.4f));
    GS1_SYSTEM_TEST_CHECK(context, device.device_stored_water < 1.0f);
    GS1_SYSTEM_TEST_CHECK(context, device.device_stored_water > 0.0f);
}

void device_maintenance_applies_weather_and_burial_wear(gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 1402U);
    GameMessageQueue queue {};
    auto site_context = make_site_context<DeviceMaintenanceSystem>(campaign, site_run, queue, 10.0);

    site_run.weather.weather_heat = 20.0f;
    site_run.weather.weather_wind = 10.0f;
    site_run.weather.weather_dust = 5.0f;

    auto tile = site_run.site_world->tile_at(TileCoord {3, 3});
    tile.ecology.sand_burial = 1.0f;
    tile.device.structure_id = gs1::StructureId {8U};
    tile.device.device_integrity = 1.0f;
    site_run.site_world->set_tile(TileCoord {3, 3}, tile);

    DeviceMaintenanceSystem::run(site_context);

    const auto device = site_run.site_world->tile_device(TileCoord {3, 3});
    GS1_SYSTEM_TEST_CHECK(context, device.device_integrity < 1.0f);
}

void camp_durability_resets_and_crosses_service_thresholds(gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 1501U);
    GameMessageQueue queue {};
    auto site_context = make_site_context<CampDurabilitySystem>(campaign, site_run, queue, 300.0);

    site_run.camp.camp_durability = 25.0f;
    site_run.camp.camp_protection_resolved = false;
    site_run.camp.delivery_point_operational = false;
    site_run.camp.shared_storage_access_enabled = false;

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        CampDurabilitySystem::process_message(
            site_context,
            make_message(
                GameMessageType::SiteRunStarted,
                SiteRunStartedMessage {1U, 1501U, 101U, 1U, 42ULL})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(site_run.camp.camp_durability, 100.0f));
    GS1_SYSTEM_TEST_CHECK(context, site_run.camp.camp_protection_resolved);
    GS1_SYSTEM_TEST_CHECK(context, site_run.camp.delivery_point_operational);
    GS1_SYSTEM_TEST_CHECK(context, site_run.camp.shared_storage_access_enabled);

    site_run.weather.weather_heat = 100.0f;
    site_run.weather.weather_wind = 100.0f;
    site_run.weather.weather_dust = 100.0f;
    site_run.event.event_heat_pressure = 15.0f;
    site_run.event.event_wind_pressure = 10.0f;
    site_run.event.event_dust_pressure = 5.0f;
    CampDurabilitySystem::run(site_context);

    GS1_SYSTEM_TEST_CHECK(context, site_run.camp.camp_durability < 30.0f);
    GS1_SYSTEM_TEST_CHECK(context, !site_run.camp.camp_protection_resolved);
    GS1_SYSTEM_TEST_CHECK(context, !site_run.camp.delivery_point_operational);
    GS1_SYSTEM_TEST_CHECK(context, !site_run.camp.shared_storage_access_enabled);
    GS1_SYSTEM_TEST_CHECK(
        context,
        (site_run.pending_projection_update_flags & gs1::SITE_PROJECTION_UPDATE_CAMP) != 0U);
}

void modifier_imports_campaign_aura_and_reacts_to_camp_changes(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    campaign.loadout_planner_state.active_nearby_aura_modifier_ids = {
        gs1::ModifierId {1U},
        gs1::ModifierId {2U}};
    auto site_run = make_test_site_run(1U, 1601U);
    GameMessageQueue queue {};
    auto site_context = make_site_context<ModifierSystem>(campaign, site_run, queue);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ModifierSystem::process_message(
            site_context,
            make_message(
                GameMessageType::SiteRunStarted,
                SiteRunStartedMessage {1U, 1601U, 101U, 1U, 42ULL})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, site_run.modifier.active_nearby_aura_modifier_ids.size() == 2U);
    const auto initial_morale = site_run.modifier.resolved_channel_totals.morale;

    site_run.camp.camp_durability = 0.0f;
    site_run.modifier.active_run_modifier_ids.push_back(gs1::ModifierId {4U});
    site_run.pending_projection_update_flags = 0U;
    ModifierSystem::run(site_context);

    GS1_SYSTEM_TEST_CHECK(context, site_run.modifier.resolved_channel_totals.morale < initial_morale);
    GS1_SYSTEM_TEST_CHECK(
        context,
        (site_run.pending_projection_update_flags & gs1::SITE_PROJECTION_UPDATE_HUD) != 0U);
}

void modifier_imports_campaign_assistant_and_technology_run_modifiers(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    campaign.faction_progress[0].has_unlocked_assistant_package = true;
    campaign.faction_progress[0].unlocked_assistant_package_id = 1001U;
    campaign.technology_state.purchased_node_ids.push_back(
        gs1::TechNodeId {gs1::k_tech_node_village_t1_relief_protocol});
    auto site_run = make_test_site_run(1U, 1602U);
    GameMessageQueue queue {};
    auto site_context = make_site_context<ModifierSystem>(campaign, site_run, queue);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ModifierSystem::process_message(
            site_context,
            make_message(
                GameMessageType::SiteRunStarted,
                SiteRunStartedMessage {1U, 1602U, 101U, 1U, 42ULL})) == GS1_STATUS_OK);

    const auto* imported_modifier = gs1::find_technology_node_def(
        gs1::TechNodeId {gs1::k_tech_node_village_t1_relief_protocol});
    GS1_SYSTEM_TEST_REQUIRE(context, imported_modifier != nullptr);
    GS1_SYSTEM_TEST_CHECK(
        context,
        std::find(
            site_run.modifier.active_run_modifier_ids.begin(),
            site_run.modifier.active_run_modifier_ids.end(),
            gs1::ModifierId {1001U}) != site_run.modifier.active_run_modifier_ids.end());
    GS1_SYSTEM_TEST_CHECK(
        context,
        std::find(
            site_run.modifier.active_run_modifier_ids.begin(),
            site_run.modifier.active_run_modifier_ids.end(),
            imported_modifier->linked_modifier_id) != site_run.modifier.active_run_modifier_ids.end());
    GS1_SYSTEM_TEST_CHECK(
        context,
        std::fabs(site_run.modifier.resolved_channel_totals.nourishment) > 0.0001f ||
            std::fabs(site_run.modifier.resolved_channel_totals.morale) > 0.0001f ||
            std::fabs(site_run.modifier.resolved_channel_totals.work_efficiency) > 0.0001f);
}
}  // namespace

GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "weather_event",
    "site_run_started_keeps_site_one_in_normal_conditions",
    weather_event_site_run_started_keeps_site_one_in_normal_conditions);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "weather_event",
    "run_advances_active_event_through_full_lifecycle",
    weather_event_run_advances_active_event_through_full_lifecycle);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "weather_event",
    "does_not_reseed_when_event_is_already_active",
    weather_event_does_not_reseed_when_event_is_already_active);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "weather_event",
    "timeline_progress_tracks_site_clock_progress",
    weather_event_timeline_progress_tracks_site_clock_progress);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "weather_event",
    "timeline_ramp_down_smooths_weather_toward_lower_target",
    weather_event_timeline_ramp_down_smooths_weather_toward_lower_target);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "weather_event",
    "highway_objective_schedules_repeating_waves_with_one_sided_wind",
    weather_event_highway_objective_schedules_repeating_waves_with_one_sided_wind);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "local_weather_resolve",
    "spreads_full_refresh_over_multiple_runs",
    local_weather_resolve_spreads_full_refresh_over_multiple_runs);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "local_weather_resolve",
    "refreshes_dirty_neighborhood_from_tile_ecology_changed",
    local_weather_resolve_refreshes_dirty_neighborhood_from_tile_ecology_changed);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "local_weather_resolve",
    "applies_directional_wind_shadow_falloff_for_windbreak_plants",
    local_weather_resolve_applies_directional_wind_shadow_falloff_for_windbreak_plants);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "local_weather_resolve",
    "applies_device_wind_protection_value_and_range",
    local_weather_resolve_applies_device_wind_protection_value_and_range);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "worker_condition",
    "requested_delta_emits_initial_full_mask_and_clamps",
    worker_condition_requested_delta_emits_initial_full_mask_and_clamps);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "worker_condition",
    "run_drains_resources_and_health_when_starving",
    worker_condition_run_drains_resources_and_health_when_starving);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "worker_condition",
    "run_applies_wind_exposure_and_shelter_softens_it",
    worker_condition_run_applies_wind_exposure_and_shelter_softens_it);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "device_support",
    "updates_efficiency_and_water_from_heat",
    device_support_updates_efficiency_and_water_from_heat);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "device_maintenance",
    "applies_weather_and_burial_wear",
    device_maintenance_applies_weather_and_burial_wear);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "camp_durability",
    "resets_and_crosses_service_thresholds",
    camp_durability_resets_and_crosses_service_thresholds);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "modifier",
    "imports_campaign_aura_and_reacts_to_camp_changes",
    modifier_imports_campaign_aura_and_reacts_to_camp_changes);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "modifier",
    "imports_campaign_assistant_and_technology_run_modifiers",
    modifier_imports_campaign_assistant_and_technology_run_modifiers);
