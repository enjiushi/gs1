#include <algorithm>
#include <cmath>
#include <cstring>

#include "content/defs/technology_defs.h"
#include "messages/game_message.h"
#include "site/site_projection_update_flags.h"
#include "site/site_world_access.h"
#include "site/systems/camp_durability_system.h"
#include "site/systems/device_maintenance_system.h"
#include "site/systems/device_support_system.h"
#include "site/systems/device_weather_contribution_system.h"
#include "site/systems/ecology_system.h"
#include "site/systems/local_weather_resolve_system.h"
#include "site/systems/modifier_system.h"
#include "site/systems/plant_weather_contribution_system.h"
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
using gs1::DeviceWeatherContributionSystem;
using gs1::EcologySystem;
using gs1::GameMessage;
using gs1::GameMessageQueue;
using gs1::GameMessageType;
using gs1::LocalWeatherResolveSystem;
using gs1::ModifierSystem;
using gs1::PlantWeatherContributionSystem;
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

void run_local_weather_pipeline(
    gs1::CampaignState& campaign,
    gs1::SiteRunState& site_run,
    GameMessageQueue& queue)
{
    auto plant_context =
        make_site_context<PlantWeatherContributionSystem>(campaign, site_run, queue);
    auto device_context =
        make_site_context<DeviceWeatherContributionSystem>(campaign, site_run, queue);
    auto local_weather_context =
        make_site_context<LocalWeatherResolveSystem>(campaign, site_run, queue);

    PlantWeatherContributionSystem::run(plant_context);
    DeviceWeatherContributionSystem::run(device_context);
    LocalWeatherResolveSystem::run(local_weather_context);
}

void prototype_site_run_seeds_site_one_ordos_wormwood_patches_near_camp(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_prototype_site_run(campaign, 1U);

    GS1_SYSTEM_TEST_REQUIRE(context, site_run.site_world != nullptr);
    for (const TileCoord coord : {
             TileCoord {12, 14},
             TileCoord {13, 14},
             TileCoord {12, 15},
             TileCoord {13, 15},
             TileCoord {12, 16},
             TileCoord {13, 16},
             TileCoord {12, 17},
             TileCoord {13, 17}})
    {
        const auto tile = site_run.site_world->tile_at(coord);
        GS1_SYSTEM_TEST_CHECK(context, tile.ecology.plant_id.value == gs1::k_plant_ordos_wormwood);
        GS1_SYSTEM_TEST_CHECK(context, approx_equal(tile.ecology.plant_density, 60.0f));
    }

    GS1_SYSTEM_TEST_CHECK(
        context,
        site_run.site_world->tile_at(site_run.camp.camp_anchor_tile).ecology.plant_id.value == 0U);
    GS1_SYSTEM_TEST_CHECK(
        context,
        site_run.site_world->tile_at(site_run.camp.delivery_box_tile).ecology.plant_id.value == 0U);
    GS1_SYSTEM_TEST_CHECK(
        context,
        site_run.site_world->tile_at(default_starter_workbench_tile(site_run.camp.camp_anchor_tile))
            .ecology.plant_id.value == 0U);
}

void weather_event_site_run_started_applies_site_one_background_conditions(
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
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(site_one_run.weather.weather_heat, 30.0f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(site_one_run.weather.weather_wind, 20.0f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(site_one_run.weather.weather_dust, 10.0f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(site_one_run.event.event_heat_pressure, 0.0f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(site_one_run.event.event_wind_pressure, 0.0f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(site_one_run.event.event_dust_pressure, 0.0f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(site_one_run.weather.weather_wind_direction_degrees, 0.0f));
    GS1_SYSTEM_TEST_CHECK(
        context,
        (site_one_run.pending_projection_update_flags & gs1::SITE_PROJECTION_UPDATE_WEATHER) != 0U);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        WeatherEventSystem::process_message(site_two_context, started_two) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, !site_two_run.event.active_event_template_id.has_value());
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(site_two_run.weather.weather_heat, 0.0f));
}

void site_one_starting_weather_keeps_starter_plant_density_stable(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_prototype_site_run(campaign, 1U);
    GameMessageQueue queue {};
    auto weather_context = make_site_context<WeatherEventSystem>(campaign, site_run, queue);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        WeatherEventSystem::process_message(
            weather_context,
            make_message(
                GameMessageType::SiteRunStarted,
                SiteRunStartedMessage {1U, 1701U, 101U, 1U, 42ULL})) == GS1_STATUS_OK);

    run_local_weather_pipeline(campaign, site_run, queue);

    auto ecology_context = make_site_context<EcologySystem>(campaign, site_run, queue, 60.0);
    EcologySystem::run(ecology_context);

    for (const TileCoord coord : {
             TileCoord {12, 14},
             TileCoord {13, 14},
             TileCoord {12, 15},
             TileCoord {13, 15},
             TileCoord {12, 16},
             TileCoord {13, 16},
             TileCoord {12, 17},
             TileCoord {13, 17}})
    {
        const auto tile = site_run.site_world->tile_at(coord);
        GS1_SYSTEM_TEST_CHECK(context, tile.ecology.plant_density >= 60.0f);
        GS1_SYSTEM_TEST_CHECK(context, tile.ecology.growth_pressure < 55.0f);
    }

    GS1_SYSTEM_TEST_CHECK(
        context,
        (site_run.pending_projection_update_flags & gs1::SITE_PROJECTION_UPDATE_TILES) != 0U);
}

void site_one_small_fixed_steps_keep_density_stable_without_density_reports(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_prototype_site_run(campaign, 1U);
    GameMessageQueue queue {};
    const auto started = make_message(
        GameMessageType::SiteRunStarted,
        SiteRunStartedMessage {1U, 1702U, 101U, 1U, 42ULL});

    auto weather_context = make_site_context<WeatherEventSystem>(campaign, site_run, queue);
    auto local_weather_message_context =
        make_site_context<LocalWeatherResolveSystem>(campaign, site_run, queue);
    auto ecology_message_context = make_site_context<EcologySystem>(campaign, site_run, queue);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        WeatherEventSystem::process_message(weather_context, started) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        LocalWeatherResolveSystem::process_message(local_weather_message_context, started) ==
            GS1_STATUS_OK);
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        EcologySystem::process_message(ecology_message_context, started) == GS1_STATUS_OK);

    queue.clear();

    for (int index = 0; index < 4; ++index)
    {
        run_local_weather_pipeline(campaign, site_run, queue);
        auto ecology_context = make_site_context<EcologySystem>(campaign, site_run, queue, 1.0 / 60.0);
        EcologySystem::run(ecology_context);
    }

    const auto tile = site_run.site_world->tile_at(TileCoord {12, 14});
    GS1_SYSTEM_TEST_CHECK(context, tile.ecology.plant_density >= 60.0f);
    GS1_SYSTEM_TEST_CHECK(context, tile.ecology.growth_pressure < 55.0f);
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
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(site_run.weather.weather_heat, 30.0f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(site_run.weather.weather_wind, 20.0f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(site_run.weather.weather_dust, 10.0f));

    for (int index = 0; index < 25; ++index)
    {
        SiteTimeSystem::run(site_time_context);
        WeatherEventSystem::run(weather_context);
    }
    GS1_SYSTEM_TEST_CHECK(context, site_run.weather.weather_heat > 30.0f);
    GS1_SYSTEM_TEST_CHECK(context, site_run.weather.weather_heat < 45.0f);
    GS1_SYSTEM_TEST_CHECK(context, site_run.weather.weather_wind > 20.0f);
    GS1_SYSTEM_TEST_CHECK(context, site_run.weather.weather_wind < 30.0f);
    GS1_SYSTEM_TEST_CHECK(context, site_run.weather.weather_dust > 10.0f);
    GS1_SYSTEM_TEST_CHECK(context, site_run.weather.weather_dust < 15.0f);

    for (int index = 0; index < 120; ++index)
    {
        SiteTimeSystem::run(site_time_context);
        WeatherEventSystem::run(weather_context);
    }
    GS1_SYSTEM_TEST_CHECK(context, !site_run.event.active_event_template_id.has_value());
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(site_run.weather.weather_heat, 30.0f, 0.01f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(site_run.weather.weather_wind, 20.0f, 0.01f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(site_run.weather.weather_dust, 10.0f, 0.01f));
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
    site_run.weather.weather_heat = 45.0f;
    site_run.weather.weather_wind = 30.0f;
    site_run.weather.weather_dust = 15.0f;
    site_run.weather.weather_wind_direction_degrees = 74.0f;
    site_run.clock.world_time_minutes = 6.0;

    WeatherEventSystem::run(weather_context);

    GS1_SYSTEM_TEST_CHECK(context, site_run.event.event_heat_pressure < 15.0f);
    GS1_SYSTEM_TEST_CHECK(context, site_run.event.event_wind_pressure < 10.0f);
    GS1_SYSTEM_TEST_CHECK(context, site_run.event.event_dust_pressure < 5.0f);
    GS1_SYSTEM_TEST_CHECK(context, site_run.weather.weather_heat < 45.0f);
    GS1_SYSTEM_TEST_CHECK(context, site_run.weather.weather_heat > 30.0f + site_run.event.event_heat_pressure);
    GS1_SYSTEM_TEST_CHECK(context, site_run.weather.weather_wind < 30.0f);
    GS1_SYSTEM_TEST_CHECK(context, site_run.weather.weather_wind > 20.0f + site_run.event.event_wind_pressure);
    GS1_SYSTEM_TEST_CHECK(context, site_run.weather.weather_dust < 15.0f);
    GS1_SYSTEM_TEST_CHECK(context, site_run.weather.weather_dust > 10.0f + site_run.event.event_dust_pressure);
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

void local_weather_resolve_recomputes_full_grid_each_run(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(2U, 1201U, 101U, 8U, 8U);
    GameMessageQueue queue {};
    auto site_context = make_site_context<LocalWeatherResolveSystem>(campaign, site_run, queue);

    site_run.weather.weather_heat = 10.0f;
    site_run.weather.weather_wind = 5.0f;
    site_run.weather.weather_dust = 3.0f;

    GS1_SYSTEM_TEST_CHECK(context, LocalWeatherResolveSystem::subscribes_to(GameMessageType::SiteRunStarted));
    GS1_SYSTEM_TEST_CHECK(context, !LocalWeatherResolveSystem::subscribes_to(GameMessageType::TileEcologyBatchChanged));
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        LocalWeatherResolveSystem::process_message(
            site_context,
            make_message(
                GameMessageType::SiteRunStarted,
                SiteRunStartedMessage {1U, 1U, 101U, 1U, 42ULL})) == GS1_STATUS_OK);

    run_local_weather_pipeline(campaign, site_run, queue);

    const auto first_pass_weather = site_run.site_world->tile_local_weather(TileCoord {0, 0});
    const auto second_tile_weather = site_run.site_world->tile_local_weather(TileCoord {0, 4});
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(first_pass_weather.heat, 10.0f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(first_pass_weather.wind, 5.0f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(first_pass_weather.dust, 3.0f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(second_tile_weather.heat, 10.0f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(second_tile_weather.wind, 5.0f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(second_tile_weather.dust, 3.0f));
}

void local_weather_resolve_combines_owner_specific_contributions_each_run(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(2U, 1202U);
    GameMessageQueue queue {};

    site_run.weather.weather_heat = 10.0f;
    site_run.weather.weather_wind = 5.0f;
    site_run.weather.weather_dust = 3.0f;
    site_run.weather.weather_wind_direction_degrees = 0.0f;

    auto planted_tile = site_run.site_world->tile_at(TileCoord {3, 3});
    planted_tile.ecology.plant_id = gs1::PlantId {1U};
    planted_tile.ecology.plant_density = 50.0f;
    site_run.site_world->set_tile(TileCoord {3, 3}, planted_tile);

    auto device_tile = site_run.site_world->tile_at(TileCoord {4, 3});
    device_tile.device.structure_id = gs1::StructureId {gs1::k_structure_wind_fence};
    device_tile.device.device_integrity = 1.0f;
    device_tile.device.device_efficiency = 0.5f;
    site_run.site_world->set_tile(TileCoord {4, 3}, device_tile);

    run_local_weather_pipeline(campaign, site_run, queue);

    const auto planted_weather = site_run.site_world->tile_local_weather(TileCoord {3, 3});
    const auto downwind_weather = site_run.site_world->tile_local_weather(TileCoord {4, 3});
    const auto upwind_weather = site_run.site_world->tile_local_weather(TileCoord {2, 3});
    const auto far_weather = site_run.site_world->tile_local_weather(TileCoord {7, 7});
    const auto planted_support =
        site_run.site_world->tile_plant_weather_contribution(TileCoord {3, 3});
    const auto device_support =
        site_run.site_world->tile_device_weather_contribution(TileCoord {4, 3});
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(planted_weather.heat, 10.0f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(planted_weather.wind, 5.0f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(planted_weather.dust, 3.0f));
    GS1_SYSTEM_TEST_CHECK(context, downwind_weather.heat < 10.0f);
    GS1_SYSTEM_TEST_CHECK(context, downwind_weather.wind < 5.0f);
    GS1_SYSTEM_TEST_CHECK(context, downwind_weather.dust < 3.0f);
    GS1_SYSTEM_TEST_CHECK(context, upwind_weather.heat < 10.0f);
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(upwind_weather.wind, 5.0f));
    GS1_SYSTEM_TEST_CHECK(context, upwind_weather.dust < 3.0f);
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(far_weather.heat, 10.0f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(far_weather.wind, 5.0f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(far_weather.dust, 3.0f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(planted_support.heat_protection, 0.0f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(planted_support.wind_protection, 0.0f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(planted_support.dust_protection, 0.0f));
    GS1_SYSTEM_TEST_CHECK(context, planted_support.fertility_improve > 0.0f);
    GS1_SYSTEM_TEST_CHECK(context, planted_support.salinity_reduction > 0.0f);
    GS1_SYSTEM_TEST_CHECK(context, device_support.wind_protection > 0.0f);
}

void local_weather_resolve_applies_eight_direction_wind_shadow_for_windbreak_plants(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(2U, 1204U, 101U, 6U, 5U);
    GameMessageQueue queue {};

    site_run.weather.weather_wind = 50.0f;
    site_run.weather.weather_wind_direction_degrees = 0.0f;

    auto tile = site_run.site_world->tile_at(TileCoord {1, 2});
    tile.ecology.plant_id = gs1::PlantId {gs1::k_plant_ordos_wormwood};
    tile.ecology.plant_density = 100.0f;
    site_run.site_world->set_tile(TileCoord {1, 2}, tile);

    run_local_weather_pipeline(campaign, site_run, queue);

    const auto own_weather = site_run.site_world->tile_local_weather(TileCoord {1, 2});
    const auto first_downwind = site_run.site_world->tile_local_weather(TileCoord {2, 2});
    const auto second_downwind = site_run.site_world->tile_local_weather(TileCoord {3, 2});
    const auto upwind = site_run.site_world->tile_local_weather(TileCoord {0, 2});
    const auto first_downwind_support =
        site_run.site_world->tile_plant_weather_contribution(TileCoord {2, 2});
    const auto second_downwind_support =
        site_run.site_world->tile_plant_weather_contribution(TileCoord {3, 2});

    GS1_SYSTEM_TEST_CHECK(context, approx_equal(own_weather.wind, 50.0f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(first_downwind_support.wind_protection, 34.0f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(second_downwind_support.wind_protection, 34.0f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(first_downwind.wind, 16.0f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(second_downwind.wind, 16.0f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(upwind.wind, 50.0f));
}

void local_weather_resolve_applies_half_strength_diagonal_wind_shadow_for_windbreak_plants(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(2U, 1206U, 101U, 6U, 6U);
    GameMessageQueue queue {};

    site_run.weather.weather_wind = 50.0f;
    site_run.weather.weather_wind_direction_degrees = 45.0f;

    auto tile = site_run.site_world->tile_at(TileCoord {2, 2});
    tile.ecology.plant_id = gs1::PlantId {gs1::k_plant_ordos_wormwood};
    tile.ecology.plant_density = 100.0f;
    site_run.site_world->set_tile(TileCoord {2, 2}, tile);

    run_local_weather_pipeline(campaign, site_run, queue);

    const auto horizontal_lane = site_run.site_world->tile_local_weather(TileCoord {3, 2});
    const auto vertical_lane = site_run.site_world->tile_local_weather(TileCoord {2, 3});
    const auto diagonal_lane = site_run.site_world->tile_local_weather(TileCoord {3, 3});
    const auto upwind = site_run.site_world->tile_local_weather(TileCoord {1, 2});
    const auto horizontal_support =
        site_run.site_world->tile_plant_weather_contribution(TileCoord {3, 2});
    const auto vertical_support =
        site_run.site_world->tile_plant_weather_contribution(TileCoord {2, 3});
    const auto diagonal_support =
        site_run.site_world->tile_plant_weather_contribution(TileCoord {3, 3});

    GS1_SYSTEM_TEST_CHECK(context, approx_equal(horizontal_support.wind_protection, 34.0f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(vertical_support.wind_protection, 34.0f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(diagonal_support.wind_protection, 17.0f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(horizontal_lane.wind, 16.0f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(vertical_lane.wind, 16.0f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(diagonal_lane.wind, 33.0f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(upwind.wind, 50.0f));
}

void local_weather_resolve_scales_multitile_plant_ranges_by_footprint(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(2U, 1206U, 101U, 10U, 8U);
    GameMessageQueue queue {};

    site_run.weather.weather_heat = 20.0f;
    site_run.weather.weather_wind = 20.0f;
    site_run.weather.weather_dust = 20.0f;
    site_run.weather.weather_wind_direction_degrees = 0.0f;

    for (const auto coord : {TileCoord {2, 2}, TileCoord {3, 2}, TileCoord {2, 3}, TileCoord {3, 3}})
    {
        auto tile = site_run.site_world->tile_at(coord);
        tile.ecology.plant_id = gs1::PlantId {gs1::k_plant_ordos_wormwood};
        tile.ecology.plant_density = 100.0f;
        site_run.site_world->set_tile(coord, tile);
    }

    run_local_weather_pipeline(campaign, site_run, queue);

    const auto aura_edge = site_run.site_world->tile_local_weather(TileCoord {5, 2});
    const auto aura_out_of_range = site_run.site_world->tile_local_weather(TileCoord {6, 2});
    const auto wind_edge = site_run.site_world->tile_local_weather(TileCoord {7, 2});
    const auto wind_out_of_range = site_run.site_world->tile_local_weather(TileCoord {8, 2});

    GS1_SYSTEM_TEST_CHECK(context, aura_edge.heat < 20.0f);
    GS1_SYSTEM_TEST_CHECK(context, aura_edge.dust < 20.0f);
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(aura_out_of_range.heat, 20.0f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(aura_out_of_range.dust, 20.0f));
    GS1_SYSTEM_TEST_CHECK(context, wind_edge.wind < 20.0f);
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(wind_out_of_range.wind, 20.0f));
}

void local_weather_resolve_respects_authored_plant_protection_ratio(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(2U, 1207U, 101U, 8U, 8U);
    GameMessageQueue queue {};

    site_run.weather.weather_heat = 20.0f;
    site_run.weather.weather_wind = 20.0f;
    site_run.weather.weather_dust = 20.0f;
    site_run.weather.weather_wind_direction_degrees = 0.0f;

    for (const auto coord : {TileCoord {2, 2}, TileCoord {3, 2}, TileCoord {2, 3}, TileCoord {3, 3}})
    {
        auto tile = site_run.site_world->tile_at(coord);
        tile.ecology.plant_id = gs1::PlantId {gs1::k_plant_ningxia_wolfberry};
        tile.ecology.plant_density = 100.0f;
        site_run.site_world->set_tile(coord, tile);
    }

    run_local_weather_pipeline(campaign, site_run, queue);

    const auto nearby_weather = site_run.site_world->tile_local_weather(TileCoord {5, 2});
    const auto nearby_support =
        site_run.site_world->tile_plant_weather_contribution(TileCoord {5, 2});

    GS1_SYSTEM_TEST_CHECK(context, approx_equal(nearby_weather.heat, 20.0f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(nearby_weather.wind, 20.0f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(nearby_weather.dust, 20.0f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(nearby_support.heat_protection, 0.0f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(nearby_support.wind_protection, 0.0f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(nearby_support.dust_protection, 0.0f));
    GS1_SYSTEM_TEST_CHECK(context, nearby_support.fertility_improve > 0.0f);
}

void local_weather_resolve_marks_projected_plant_tiles_dirty_when_wind_changes(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(2U, 1205U, 101U, 5U, 5U);
    GameMessageQueue queue {};

    auto tile = site_run.site_world->tile_at(TileCoord {2, 2});
    tile.ecology.plant_id = gs1::PlantId {gs1::k_plant_ordos_wormwood};
    tile.ecology.plant_density = 80.0f;
    site_run.site_world->set_tile(TileCoord {2, 2}, tile);

    site_run.weather.weather_wind = 10.0f;
    site_run.weather.weather_wind_direction_degrees = 0.0f;
    run_local_weather_pipeline(campaign, site_run, queue);

    site_run.pending_projection_update_flags = 0U;
    site_run.pending_tile_projection_updates.clear();
    site_run.pending_tile_projection_update_mask.assign(site_run.site_world->tile_count(), 0U);

    site_run.weather.weather_wind = 18.0f;
    run_local_weather_pipeline(campaign, site_run, queue);

    GS1_SYSTEM_TEST_CHECK(
        context,
        (site_run.pending_projection_update_flags & gs1::SITE_PROJECTION_UPDATE_TILES) != 0U);
    GS1_SYSTEM_TEST_REQUIRE(context, site_run.pending_tile_projection_updates.size() == 1U);
    GS1_SYSTEM_TEST_CHECK(context, site_run.pending_tile_projection_updates.front().x == 2);
    GS1_SYSTEM_TEST_CHECK(context, site_run.pending_tile_projection_updates.front().y == 2);
}

void local_weather_resolve_applies_device_wind_protection_value_and_range(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(2U, 1203U, 101U, 5U, 5U);
    GameMessageQueue queue {};
    auto device_context = make_site_context<DeviceWeatherContributionSystem>(campaign, site_run, queue);

    site_run.weather.weather_heat = 10.0f;
    site_run.weather.weather_wind = 10.0f;
    site_run.weather.weather_dust = 3.0f;
    site_run.weather.weather_wind_direction_degrees = 0.0f;

    run_local_weather_pipeline(campaign, site_run, queue);

    auto tile = site_run.site_world->tile_at(TileCoord {2, 2});
    tile.device.structure_id = gs1::StructureId {gs1::k_structure_wind_fence};
    tile.device.device_integrity = 1.0f;
    tile.device.device_efficiency = 0.5f;
    site_run.site_world->set_tile(TileCoord {2, 2}, tile);

    GS1_SYSTEM_TEST_CHECK(context, !LocalWeatherResolveSystem::subscribes_to(GameMessageType::SiteDevicePlaced));
    GS1_SYSTEM_TEST_CHECK(context, !LocalWeatherResolveSystem::subscribes_to(GameMessageType::SiteDeviceBroken));
    GS1_SYSTEM_TEST_CHECK(context, !LocalWeatherResolveSystem::subscribes_to(GameMessageType::SiteDeviceRepaired));
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        DeviceWeatherContributionSystem::process_message(
            device_context,
            make_message(
                GameMessageType::SiteDevicePlaced,
                gs1::SiteDevicePlacedMessage {
                    0U,
                    2,
                    2,
                    gs1::k_structure_wind_fence,
                    0U})) == GS1_STATUS_OK);

    run_local_weather_pipeline(campaign, site_run, queue);

    const auto own_weather = site_run.site_world->tile_local_weather(TileCoord {2, 2});
    const auto neighbor_weather = site_run.site_world->tile_local_weather(TileCoord {3, 2});
    const auto upwind_weather = site_run.site_world->tile_local_weather(TileCoord {1, 2});
    const auto out_of_range_weather = site_run.site_world->tile_local_weather(TileCoord {4, 2});
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(own_weather.wind, 0.0f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(neighbor_weather.wind, 0.0f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(upwind_weather.wind, 10.0f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(neighbor_weather.heat, 10.0f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(neighbor_weather.dust, 3.0f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(out_of_range_weather.wind, 10.0f));
}

void ecology_non_growable_checkerboard_ignores_weather_pressure_and_uses_constant_wither_only(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto calm_run = make_test_site_run(2U, 1703U, 101U, 6U, 6U);
    auto harsh_run = make_test_site_run(2U, 1704U, 101U, 6U, 6U);
    GameMessageQueue calm_queue {};
    GameMessageQueue harsh_queue {};
    auto calm_context = make_site_context<EcologySystem>(campaign, calm_run, calm_queue, 60.0);
    auto harsh_context = make_site_context<EcologySystem>(campaign, harsh_run, harsh_queue, 60.0);

    for (auto* run : {&calm_run, &harsh_run})
    {
        auto tile = run->site_world->tile_at(TileCoord {2, 2});
        tile.ecology.plant_id = gs1::PlantId {gs1::k_plant_straw_checkerboard};
        tile.ecology.plant_density = 100.0f;
        tile.ecology.moisture = 0.0f;
        tile.ecology.soil_fertility = 0.0f;
        tile.ecology.soil_salinity = 0.0f;
        tile.local_weather.heat = 0.0f;
        tile.local_weather.wind = 0.0f;
        tile.local_weather.dust = 0.0f;
        run->site_world->set_tile(TileCoord {2, 2}, tile);
    }

    auto harsh_tile = harsh_run.site_world->tile_at(TileCoord {2, 2});
    harsh_tile.local_weather.heat = 100.0f;
    harsh_tile.local_weather.wind = 100.0f;
    harsh_tile.local_weather.dust = 100.0f;
    harsh_run.site_world->set_tile(TileCoord {2, 2}, harsh_tile);

    EcologySystem::run(calm_context);
    EcologySystem::run(harsh_context);

    const auto calm = calm_run.site_world->tile_at(TileCoord {2, 2});
    const auto harsh = harsh_run.site_world->tile_at(TileCoord {2, 2});
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(calm.ecology.growth_pressure, 0.0f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(harsh.ecology.growth_pressure, 0.0f));
    GS1_SYSTEM_TEST_CHECK(context, calm.ecology.plant_density < 100.0f);
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(calm.ecology.plant_density, harsh.ecology.plant_density, 0.01f));
}

void worker_condition_requested_delta_recomputes_cap_and_clamps_energy(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 1301U);
    GameMessageQueue queue {};
    auto site_context = make_site_context<WorkerConditionSystem>(campaign, site_run, queue);

    auto worker = gs1::site_world_access::worker_conditions(site_run);
    worker.health = 80.0f;
    worker.hydration = 60.0f;
    worker.nourishment = 90.0f;
    worker.energy = 10.0f;
    gs1::site_world_access::set_worker_conditions(site_run, worker);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        WorkerConditionSystem::process_message(
            site_context,
            make_message(
                GameMessageType::WorkerMeterDeltaRequested,
                gs1::WorkerMeterDeltaRequestedMessage {
                    1U,
                    gs1::WORKER_METER_CHANGED_HEALTH |
                        gs1::WORKER_METER_CHANGED_ENERGY,
                    -10.0f,
                    0.0f,
                    0.0f,
                    0.0f,
                    100.0f,
                    0.0f,
                    0.0f})) == GS1_STATUS_OK);

    const auto updated = gs1::site_world_access::worker_conditions(site_run);
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(updated.health, 70.0f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(updated.energy_cap, 60.0f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(updated.energy, 60.0f));
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

void worker_condition_run_applies_passive_decay_and_derived_value_floors(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 1302U);
    GameMessageQueue queue {};
    auto site_context = make_site_context<WorkerConditionSystem>(campaign, site_run, queue, 10.0);

    auto worker = gs1::site_world_access::worker_conditions(site_run);
    worker.health = 10.0f;
    worker.hydration = 10.0f;
    worker.nourishment = 20.0f;
    worker.energy = 80.0f;
    worker.morale = 5.0f;
    gs1::site_world_access::set_worker_conditions(site_run, worker);

    auto tile = site_run.site_world->tile_at(TileCoord {2, 2});
    tile.local_weather.heat = 100.0f;
    tile.local_weather.wind = 100.0f;
    tile.local_weather.dust = 100.0f;
    site_run.site_world->set_tile(TileCoord {2, 2}, tile);

    WorkerConditionSystem::run(site_context);

    const auto updated = gs1::site_world_access::worker_conditions(site_run);
    GS1_SYSTEM_TEST_CHECK(context, updated.health < 10.0f);
    GS1_SYSTEM_TEST_CHECK(context, updated.hydration < 10.0f);
    GS1_SYSTEM_TEST_CHECK(context, updated.nourishment < 20.0f);
    GS1_SYSTEM_TEST_CHECK(context, updated.morale < 5.0f);
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(updated.energy_cap, 50.0f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(updated.energy, 50.0f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(updated.work_efficiency, 0.4f));
    GS1_SYSTEM_TEST_CHECK(context, queue.size() == 1U);
}

void worker_condition_run_recovers_morale_in_calm_weather(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 13021U);
    GameMessageQueue queue {};
    auto site_context = make_site_context<WorkerConditionSystem>(campaign, site_run, queue, 10.0);

    auto worker = gs1::site_world_access::worker_conditions(site_run);
    worker.morale = 40.0f;
    gs1::site_world_access::set_worker_conditions(site_run, worker);

    auto tile = site_run.site_world->tile_at(TileCoord {2, 2});
    tile.local_weather.heat = 0.0f;
    tile.local_weather.wind = 0.0f;
    tile.local_weather.dust = 0.0f;
    site_run.site_world->set_tile(TileCoord {2, 2}, tile);

    WorkerConditionSystem::run(site_context);

    const auto updated = gs1::site_world_access::worker_conditions(site_run);
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(updated.morale, 45.55556f, 0.01f));
    GS1_SYSTEM_TEST_CHECK(context, queue.size() == 1U);
}

void worker_condition_run_applies_morale_support_to_background_resolution(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 13022U);
    GameMessageQueue queue {};
    auto site_context = make_site_context<WorkerConditionSystem>(campaign, site_run, queue, 10.0);

    auto worker = gs1::site_world_access::worker_conditions(site_run);
    worker.morale = 40.0f;
    gs1::site_world_access::set_worker_conditions(site_run, worker);

    auto tile = site_run.site_world->tile_at(TileCoord {2, 2});
    tile.local_weather.heat = 0.0f;
    tile.local_weather.wind = 0.0f;
    tile.local_weather.dust = 0.0f;
    site_run.site_world->set_tile(TileCoord {2, 2}, tile);

    site_run.modifier.resolved_channel_totals.morale = 1.0f;

    WorkerConditionSystem::run(site_context);

    const auto updated = gs1::site_world_access::worker_conditions(site_run);
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(updated.morale, 53.88889f, 0.01f));
    GS1_SYSTEM_TEST_CHECK(context, queue.size() == 1U);
}

void worker_condition_run_scales_background_energy_recovery_from_hydration_and_nourishment(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    std::uint32_t next_site_run_id = 13023U;
    const auto run_sample = [&](float hydration, float nourishment) {
        auto site_run = make_test_site_run(1U, next_site_run_id++);
        GameMessageQueue queue {};
        auto site_context =
            make_site_context<WorkerConditionSystem>(campaign, site_run, queue, 10.0);

        auto worker = gs1::site_world_access::worker_conditions(site_run);
        worker.health = 100.0f;
        worker.hydration = hydration;
        worker.nourishment = nourishment;
        worker.energy = 0.0f;
        gs1::site_world_access::set_worker_conditions(site_run, worker);

        auto tile = site_run.site_world->tile_at(TileCoord {2, 2});
        tile.local_weather.heat = 0.0f;
        tile.local_weather.wind = 0.0f;
        tile.local_weather.dust = 0.0f;
        site_run.site_world->set_tile(TileCoord {2, 2}, tile);

        WorkerConditionSystem::run(site_context);

        GS1_SYSTEM_TEST_CHECK(context, queue.size() == 1U);
        return gs1::site_world_access::worker_conditions(site_run);
    };

    const auto full_support = run_sample(100.0f, 100.0f);
    const auto medium_support = run_sample(50.0f, 80.0f);
    const auto no_support = run_sample(0.0f, 100.0f);

    GS1_SYSTEM_TEST_CHECK(context, approx_equal(full_support.energy, 33.312f, 0.02f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(medium_support.energy, 19.97867f, 0.02f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(no_support.energy, 6.66667f, 0.02f));
    GS1_SYSTEM_TEST_CHECK(context, full_support.energy > medium_support.energy);
    GS1_SYSTEM_TEST_CHECK(context, medium_support.energy > no_support.energy);
}

void worker_condition_run_softens_environmental_decay_when_sheltered(
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
    exposed_tile.local_weather.heat = 80.0f;
    exposed_tile.local_weather.wind = 80.0f;
    exposed_tile.local_weather.dust = 80.0f;
    exposed_run.site_world->set_tile(TileCoord {2, 2}, exposed_tile);

    auto sheltered_tile = sheltered_run.site_world->tile_at(TileCoord {2, 2});
    sheltered_tile.local_weather.heat = 80.0f;
    sheltered_tile.local_weather.wind = 80.0f;
    sheltered_tile.local_weather.dust = 80.0f;
    sheltered_run.site_world->set_tile(TileCoord {2, 2}, sheltered_tile);

    auto exposed_worker = gs1::site_world_access::worker_conditions(exposed_run);
    exposed_worker.hydration = 90.0f;
    exposed_worker.nourishment = 90.0f;
    exposed_worker.health = 90.0f;
    exposed_worker.energy = 90.0f;
    exposed_worker.morale = 70.0f;
    gs1::site_world_access::set_worker_conditions(exposed_run, exposed_worker);

    auto sheltered_worker = gs1::site_world_access::worker_conditions(sheltered_run);
    sheltered_worker.hydration = 90.0f;
    sheltered_worker.nourishment = 90.0f;
    sheltered_worker.health = 90.0f;
    sheltered_worker.energy = 90.0f;
    sheltered_worker.morale = 70.0f;
    sheltered_worker.is_sheltered = true;
    gs1::site_world_access::set_worker_conditions(sheltered_run, sheltered_worker);

    WorkerConditionSystem::run(exposed_context);
    WorkerConditionSystem::run(sheltered_context);

    const auto exposed = gs1::site_world_access::worker_conditions(exposed_run);
    const auto sheltered = gs1::site_world_access::worker_conditions(sheltered_run);
    GS1_SYSTEM_TEST_CHECK(context, exposed.hydration < 90.0f);
    GS1_SYSTEM_TEST_CHECK(context, exposed.nourishment < 90.0f);
    GS1_SYSTEM_TEST_CHECK(context, exposed.health < 90.0f);
    GS1_SYSTEM_TEST_CHECK(context, exposed.energy < 90.0f);
    GS1_SYSTEM_TEST_CHECK(context, exposed.morale < 70.0f);
    GS1_SYSTEM_TEST_CHECK(context, exposed.hydration < sheltered.hydration);
    GS1_SYSTEM_TEST_CHECK(context, exposed.nourishment < sheltered.nourishment);
    GS1_SYSTEM_TEST_CHECK(context, exposed.health < sheltered.health);
    GS1_SYSTEM_TEST_CHECK(context, exposed.energy < sheltered.energy);
    GS1_SYSTEM_TEST_CHECK(context, sheltered.morale > 70.0f);
    GS1_SYSTEM_TEST_CHECK(context, exposed.morale < sheltered.morale);
    GS1_SYSTEM_TEST_CHECK(context, exposed_queue.size() == 1U);
    GS1_SYSTEM_TEST_CHECK(context, sheltered_queue.size() == 1U);
}

void worker_condition_run_matches_requested_weather_decay_timings(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();

    auto heat_health_run = make_test_site_run(1U, 1305U);
    GameMessageQueue heat_health_queue {};
    auto heat_health_context =
        make_site_context<WorkerConditionSystem>(campaign, heat_health_run, heat_health_queue, 600.0);
    auto heat_health_tile = heat_health_run.site_world->tile_at(TileCoord {2, 2});
    heat_health_tile.local_weather.heat = 100.0f;
    heat_health_run.site_world->set_tile(TileCoord {2, 2}, heat_health_tile);

    WorkerConditionSystem::run(heat_health_context);

    const auto heat_health = gs1::site_world_access::worker_conditions(heat_health_run);
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(heat_health.health, 0.0f, 0.01f));

    auto heat_hydration_run = make_test_site_run(1U, 1306U);
    GameMessageQueue heat_hydration_queue {};
    auto heat_hydration_context = make_site_context<WorkerConditionSystem>(
        campaign,
        heat_hydration_run,
        heat_hydration_queue,
        300.0);
    auto heat_hydration_tile = heat_hydration_run.site_world->tile_at(TileCoord {2, 2});
    heat_hydration_tile.local_weather.heat = 100.0f;
    heat_hydration_run.site_world->set_tile(TileCoord {2, 2}, heat_hydration_tile);

    WorkerConditionSystem::run(heat_hydration_context);

    const auto heat_hydration = gs1::site_world_access::worker_conditions(heat_hydration_run);
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(heat_hydration.hydration, 0.0f, 0.01f));

    auto wind_health_run = make_test_site_run(1U, 1307U);
    GameMessageQueue wind_health_queue {};
    auto wind_health_context =
        make_site_context<WorkerConditionSystem>(campaign, wind_health_run, wind_health_queue, 600.0);
    auto wind_health_tile = wind_health_run.site_world->tile_at(TileCoord {2, 2});
    wind_health_tile.local_weather.wind = 100.0f;
    wind_health_run.site_world->set_tile(TileCoord {2, 2}, wind_health_tile);

    WorkerConditionSystem::run(wind_health_context);

    const auto wind_health = gs1::site_world_access::worker_conditions(wind_health_run);
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(wind_health.health, 0.0f, 0.01f));
}

void worker_condition_run_uses_requested_weather_decay_ratios(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    std::uint32_t next_site_run_id = 1310U;
    const auto run_sample = [&](float heat, float wind, float dust) {
        auto site_run = make_test_site_run(1U, next_site_run_id++);
        GameMessageQueue queue {};
        auto site_context =
            make_site_context<WorkerConditionSystem>(campaign, site_run, queue, 60.0);
        auto tile = site_run.site_world->tile_at(TileCoord {2, 2});
        tile.local_weather.heat = heat;
        tile.local_weather.wind = wind;
        tile.local_weather.dust = dust;
        site_run.site_world->set_tile(TileCoord {2, 2}, tile);
        WorkerConditionSystem::run(site_context);
        return gs1::site_world_access::worker_conditions(site_run);
    };

    const auto neutral = run_sample(0.0f, 0.0f, 0.0f);
    const auto heat_only = run_sample(100.0f, 0.0f, 0.0f);
    const auto wind_only = run_sample(0.0f, 100.0f, 0.0f);
    const auto dust_only = run_sample(0.0f, 0.0f, 100.0f);

    GS1_SYSTEM_TEST_CHECK(context, approx_equal(neutral.hydration, 99.52f, 0.01f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(neutral.nourishment, 99.88f, 0.01f));

    GS1_SYSTEM_TEST_CHECK(context, approx_equal(heat_only.health, 90.0f, 0.01f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(wind_only.health, 90.0f, 0.01f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(dust_only.health, 95.0f, 0.01f));

    GS1_SYSTEM_TEST_CHECK(context, approx_equal(heat_only.hydration, 80.0f, 0.01f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(wind_only.hydration, 97.08f, 0.01f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(dust_only.hydration, 97.08f, 0.01f));

    GS1_SYSTEM_TEST_CHECK(context, approx_equal(heat_only.nourishment, 99.784f, 0.01f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(wind_only.nourishment, 99.544f, 0.01f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(dust_only.nourishment, 99.832f, 0.01f));
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
    campaign.technology_state.purchased_nodes.push_back(
        gs1::TechnologyPurchaseRecord {
            gs1::TechNodeId {gs1::k_tech_node_t1_field_briefing},
            gs1::FactionId {gs1::k_faction_village_committee},
            10});
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
        gs1::TechNodeId {gs1::k_tech_node_t1_field_briefing});
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
    "site_run_factory",
    "prototype_site_run_seeds_site_one_ordos_wormwood_patches_near_camp",
    prototype_site_run_seeds_site_one_ordos_wormwood_patches_near_camp);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "weather_event",
    "site_run_started_applies_site_one_background_conditions",
    weather_event_site_run_started_applies_site_one_background_conditions);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "ecology",
    "site_one_starting_weather_keeps_starter_plant_density_stable",
    site_one_starting_weather_keeps_starter_plant_density_stable);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "ecology",
    "site_one_small_fixed_steps_keep_density_stable_without_density_reports",
    site_one_small_fixed_steps_keep_density_stable_without_density_reports);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "ecology",
    "non_growable_checkerboard_ignores_weather_pressure_and_uses_constant_wither_only",
    ecology_non_growable_checkerboard_ignores_weather_pressure_and_uses_constant_wither_only);
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
    "recomputes_full_grid_each_run",
    local_weather_resolve_recomputes_full_grid_each_run);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "local_weather_resolve",
    "combines_owner_specific_contributions_each_run",
    local_weather_resolve_combines_owner_specific_contributions_each_run);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "local_weather_resolve",
    "applies_eight_direction_wind_shadow_for_windbreak_plants",
    local_weather_resolve_applies_eight_direction_wind_shadow_for_windbreak_plants);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "local_weather_resolve",
    "applies_half_strength_diagonal_wind_shadow_for_windbreak_plants",
    local_weather_resolve_applies_half_strength_diagonal_wind_shadow_for_windbreak_plants);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "local_weather_resolve",
    "scales_multitile_plant_ranges_by_footprint",
    local_weather_resolve_scales_multitile_plant_ranges_by_footprint);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "local_weather_resolve",
    "respects_authored_plant_protection_ratio",
    local_weather_resolve_respects_authored_plant_protection_ratio);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "local_weather_resolve",
    "marks_projected_plant_tiles_dirty_when_wind_changes",
    local_weather_resolve_marks_projected_plant_tiles_dirty_when_wind_changes);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "local_weather_resolve",
    "applies_device_wind_protection_value_and_range",
    local_weather_resolve_applies_device_wind_protection_value_and_range);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "worker_condition",
    "requested_delta_recomputes_cap_and_clamps_energy",
    worker_condition_requested_delta_recomputes_cap_and_clamps_energy);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "worker_condition",
    "run_applies_passive_decay_and_derived_value_floors",
    worker_condition_run_applies_passive_decay_and_derived_value_floors);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "worker_condition",
    "run_recovers_morale_in_calm_weather",
    worker_condition_run_recovers_morale_in_calm_weather);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "worker_condition",
    "run_applies_morale_support_to_background_resolution",
    worker_condition_run_applies_morale_support_to_background_resolution);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "worker_condition",
    "run_scales_background_energy_recovery_from_hydration_and_nourishment",
    worker_condition_run_scales_background_energy_recovery_from_hydration_and_nourishment);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "worker_condition",
    "run_softens_environmental_decay_when_sheltered",
    worker_condition_run_softens_environmental_decay_when_sheltered);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "worker_condition",
    "run_matches_requested_weather_decay_timings",
    worker_condition_run_matches_requested_weather_decay_timings);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "worker_condition",
    "run_uses_requested_weather_decay_ratios",
    worker_condition_run_uses_requested_weather_decay_ratios);
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
