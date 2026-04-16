#include "commands/game_command.h"
#include "site/site_projection_update_flags.h"
#include "site/site_world_access.h"
#include "site/systems/camp_durability_system.h"
#include "site/systems/device_maintenance_system.h"
#include "site/systems/device_support_system.h"
#include "site/systems/local_weather_resolve_system.h"
#include "site/systems/modifier_system.h"
#include "site/systems/weather_event_system.h"
#include "site/systems/worker_condition_system.h"
#include "testing/system_test_registry.h"
#include "system_test_fixtures.h"

namespace
{
using gs1::CampDurabilitySystem;
using gs1::DeviceMaintenanceSystem;
using gs1::DeviceSupportSystem;
using gs1::EventPhase;
using gs1::GameCommand;
using gs1::GameCommandQueue;
using gs1::GameCommandType;
using gs1::LocalWeatherResolveSystem;
using gs1::ModifierSystem;
using gs1::SiteRunStartedCommand;
using gs1::TileCoord;
using gs1::WeatherEventSystem;
using gs1::WorkerConditionSystem;
using namespace gs1::testing::fixtures;

template <typename Payload>
GameCommand make_command(gs1::GameCommandType type, const Payload& payload)
{
    GameCommand command {};
    command.type = type;
    command.set_payload(payload);
    return command;
}

void weather_event_seeds_site_one_and_ignores_other_sites(gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_one_run = make_test_site_run(1U, 1101U);
    auto site_two_run = make_test_site_run(2U, 1102U);
    GameCommandQueue queue_one {};
    GameCommandQueue queue_two {};

    auto site_one_context = make_site_context<WeatherEventSystem>(campaign, site_one_run, queue_one);
    auto site_two_context = make_site_context<WeatherEventSystem>(campaign, site_two_run, queue_two);
    const auto started_one = make_command(
        GameCommandType::SiteRunStarted,
        SiteRunStartedCommand {1U, 1U, 101U, 1U, 42ULL});
    const auto started_two = make_command(
        GameCommandType::SiteRunStarted,
        SiteRunStartedCommand {2U, 1U, 102U, 1U, 42ULL});

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        WeatherEventSystem::process_command(site_one_context, started_one) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, site_one_run.weather.forecast_profile_state.forecast_profile_id == 1U);
    GS1_SYSTEM_TEST_CHECK(context, site_one_run.event.active_event_template_id.has_value());
    GS1_SYSTEM_TEST_CHECK(context, site_one_run.event.event_phase == EventPhase::Warning);
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(site_one_run.weather.weather_heat, 15.0f));
    GS1_SYSTEM_TEST_CHECK(
        context,
        (site_one_run.pending_projection_update_flags & gs1::SITE_PROJECTION_UPDATE_WEATHER) != 0U);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        WeatherEventSystem::process_command(site_two_context, started_two) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, !site_two_run.event.active_event_template_id.has_value());
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(site_two_run.weather.weather_heat, 0.0f));
}

void weather_event_run_advances_through_full_lifecycle(gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 1103U);
    GameCommandQueue queue {};
    auto site_context = make_site_context<WeatherEventSystem>(campaign, site_run, queue);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        WeatherEventSystem::process_command(
            site_context,
            make_command(
                GameCommandType::SiteRunStarted,
                SiteRunStartedCommand {1U, 1U, 101U, 1U, 42ULL})) == GS1_STATUS_OK);

    for (int index = 0; index < 10; ++index)
    {
        WeatherEventSystem::run(site_context);
    }
    GS1_SYSTEM_TEST_CHECK(context, site_run.event.event_phase == EventPhase::Build);

    for (int index = 0; index < 40; ++index)
    {
        WeatherEventSystem::run(site_context);
    }
    GS1_SYSTEM_TEST_CHECK(context, site_run.event.event_phase == EventPhase::None);
    GS1_SYSTEM_TEST_CHECK(context, !site_run.event.active_event_template_id.has_value());
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(site_run.weather.weather_heat, 0.0f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(site_run.event.aftermath_relief_resolved, 1.0f));
}

void weather_event_does_not_reseed_when_event_is_already_active(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 1104U);
    GameCommandQueue queue {};
    auto site_context = make_site_context<WeatherEventSystem>(campaign, site_run, queue);

    site_run.weather.forecast_profile_state.forecast_profile_id = 7U;
    site_run.weather.site_weather_bias = 2.5f;
    site_run.weather.weather_heat = 99.0f;
    site_run.event.active_event_template_id = gs1::EventTemplateId {8U};
    site_run.event.event_phase = EventPhase::Peak;
    site_run.event.phase_minutes_remaining = 3.0;

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        WeatherEventSystem::process_command(
            site_context,
            make_command(
                GameCommandType::SiteRunStarted,
                SiteRunStartedCommand {1U, 1U, 101U, 1U, 42ULL})) == GS1_STATUS_OK);

    GS1_SYSTEM_TEST_CHECK(context, site_run.weather.forecast_profile_state.forecast_profile_id == 7U);
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(site_run.weather.site_weather_bias, 2.5f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(site_run.weather.weather_heat, 99.0f));
    GS1_SYSTEM_TEST_CHECK(context, site_run.event.active_event_template_id.has_value());
    GS1_SYSTEM_TEST_CHECK(context, site_run.event.active_event_template_id->value == 8U);
    GS1_SYSTEM_TEST_CHECK(context, site_run.event.event_phase == EventPhase::Peak);
}

void local_weather_resolve_updates_tiles_and_process_command_is_noop(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(2U, 1201U);
    GameCommandQueue queue {};
    auto site_context = make_site_context<LocalWeatherResolveSystem>(campaign, site_run, queue);

    site_run.weather.weather_heat = 10.0f;
    site_run.weather.weather_wind = 5.0f;
    site_run.weather.weather_dust = 3.0f;

    auto covered = site_run.site_world->tile_at(TileCoord {1, 1});
    covered.ecology.ground_cover_type_id = 4U;
    covered.ecology.plant_density = 0.5f;
    site_run.site_world->set_tile(TileCoord {1, 1}, covered);

    GS1_SYSTEM_TEST_CHECK(context, !LocalWeatherResolveSystem::subscribes_to(GameCommandType::SiteRunStarted));
    GS1_SYSTEM_TEST_REQUIRE(
        context,
        LocalWeatherResolveSystem::process_command(
            site_context,
            make_command(
                GameCommandType::SiteRunStarted,
                SiteRunStartedCommand {1U, 1U, 101U, 1U, 42ULL})) == GS1_STATUS_OK);

    LocalWeatherResolveSystem::run(site_context);

    const auto covered_weather = site_run.site_world->tile_local_weather(TileCoord {1, 1});
    const auto exposed_weather = site_run.site_world->tile_local_weather(TileCoord {2, 1});
#if defined(GS1_ENABLE_LOCAL_WEATHER_RESOLUTION)
    GS1_SYSTEM_TEST_CHECK(context, covered_weather.heat < exposed_weather.heat);
    GS1_SYSTEM_TEST_CHECK(context, covered_weather.wind < exposed_weather.wind);
    GS1_SYSTEM_TEST_CHECK(context, covered_weather.dust < exposed_weather.dust);
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(exposed_weather.dust, 4.25f));
#else
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(covered_weather.heat, 0.0f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(covered_weather.wind, 0.0f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(covered_weather.dust, 0.0f));
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(exposed_weather.dust, 0.0f));
#endif
}

void worker_condition_requested_delta_emits_initial_full_mask_and_clamps(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 1301U);
    GameCommandQueue queue {};
    auto site_context = make_site_context<WorkerConditionSystem>(campaign, site_run, queue);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        WorkerConditionSystem::process_command(
            site_context,
            make_command(
                GameCommandType::WorkerMeterDeltaRequested,
                gs1::WorkerMeterDeltaRequestedCommand {
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
    GS1_SYSTEM_TEST_CHECK(context, queue.front().type == GameCommandType::WorkerMetersChanged);
    GS1_SYSTEM_TEST_CHECK(
        context,
        queue.front().payload_as<gs1::WorkerMetersChangedCommand>().changed_mask ==
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
    GameCommandQueue queue {};
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

void device_support_updates_efficiency_and_water_from_heat(
    gs1::testing::SystemTestExecutionContext& context)
{
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 1401U);
    GameCommandQueue queue {};
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
    GameCommandQueue queue {};
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
    GameCommandQueue queue {};
    auto site_context = make_site_context<CampDurabilitySystem>(campaign, site_run, queue, 300.0);

    site_run.camp.camp_durability = 25.0f;
    site_run.camp.camp_protection_resolved = false;
    site_run.camp.delivery_point_operational = false;
    site_run.camp.shared_camp_storage_access_enabled = false;

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        CampDurabilitySystem::process_command(
            site_context,
            make_command(
                GameCommandType::SiteRunStarted,
                SiteRunStartedCommand {1U, 1501U, 101U, 1U, 42ULL})) == GS1_STATUS_OK);
    GS1_SYSTEM_TEST_CHECK(context, approx_equal(site_run.camp.camp_durability, 100.0f));
    GS1_SYSTEM_TEST_CHECK(context, site_run.camp.camp_protection_resolved);
    GS1_SYSTEM_TEST_CHECK(context, site_run.camp.delivery_point_operational);
    GS1_SYSTEM_TEST_CHECK(context, site_run.camp.shared_camp_storage_access_enabled);

    site_run.weather.weather_heat = 100.0f;
    site_run.weather.weather_wind = 100.0f;
    site_run.weather.weather_dust = 100.0f;
    site_run.event.event_phase = EventPhase::Peak;
    CampDurabilitySystem::run(site_context);

    GS1_SYSTEM_TEST_CHECK(context, site_run.camp.camp_durability < 30.0f);
    GS1_SYSTEM_TEST_CHECK(context, !site_run.camp.camp_protection_resolved);
    GS1_SYSTEM_TEST_CHECK(context, !site_run.camp.delivery_point_operational);
    GS1_SYSTEM_TEST_CHECK(context, !site_run.camp.shared_camp_storage_access_enabled);
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
    GameCommandQueue queue {};
    auto site_context = make_site_context<ModifierSystem>(campaign, site_run, queue);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        ModifierSystem::process_command(
            site_context,
            make_command(
                GameCommandType::SiteRunStarted,
                SiteRunStartedCommand {1U, 1601U, 101U, 1U, 42ULL})) == GS1_STATUS_OK);
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
}  // namespace

GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "weather_event",
    "seeds_site_one_and_ignores_other_sites",
    weather_event_seeds_site_one_and_ignores_other_sites);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "weather_event",
    "run_advances_through_full_lifecycle",
    weather_event_run_advances_through_full_lifecycle);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "weather_event",
    "does_not_reseed_when_event_is_already_active",
    weather_event_does_not_reseed_when_event_is_already_active);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "local_weather_resolve",
    "updates_tiles_and_process_command_is_noop",
    local_weather_resolve_updates_tiles_and_process_command_is_noop);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "worker_condition",
    "requested_delta_emits_initial_full_mask_and_clamps",
    worker_condition_requested_delta_emits_initial_full_mask_and_clamps);
GS1_REGISTER_SOURCE_SYSTEM_TEST(
    "worker_condition",
    "run_drains_resources_and_health_when_starving",
    worker_condition_run_drains_resources_and_health_when_starving);
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
