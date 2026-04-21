#include "messages/game_message.h"
#include "site/systems/ecology_system.h"
#include "site/systems/economy_phone_system.h"
#include "site/systems/inventory_system.h"
#include "site/systems/placement_validation_system.h"
#include "site/systems/task_board_system.h"
#include "testing/system_test_registry.h"
#include "system_test_fixtures.h"

#include <cctype>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace
{
using gs1::EcologySystem;
using gs1::EconomyPhoneSystem;
using gs1::GameMessage;
using gs1::GameMessageQueue;
using gs1::GameMessageType;
using gs1::InventorySystem;
using gs1::PlacementOccupancyLayer;
using gs1::PlacementReservationRejectionReason;
using gs1::PlacementValidationSystem;
using gs1::SiteRunStartedMessage;
using gs1::TaskBoardSystem;
using gs1::TaskRuntimeListKind;
using gs1::TileCoord;
using namespace gs1::testing::fixtures;

std::string trim_copy(std::string_view value)
{
    std::size_t start = 0U;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start])) != 0)
    {
        start += 1U;
    }

    std::size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1U])) != 0)
    {
        end -= 1U;
    }

    return std::string(value.substr(start, end - start));
}

std::map<std::string, std::string, std::less<>> parse_payload_map(std::string_view payload)
{
    std::map<std::string, std::string, std::less<>> values {};
    std::size_t line_start = 0U;
    while (line_start <= payload.size())
    {
        const std::size_t line_end = payload.find('\n', line_start);
        const std::string_view raw_line = line_end == std::string_view::npos
            ? payload.substr(line_start)
            : payload.substr(line_start, line_end - line_start);
        std::string line = trim_copy(raw_line);
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }

        if (!line.empty() && line.front() != '#')
        {
            const std::size_t separator = line.find('=');
            if (separator != std::string::npos)
            {
                values.emplace(
                    trim_copy(std::string_view(line).substr(0U, separator)),
                    trim_copy(std::string_view(line).substr(separator + 1U)));
            }
        }

        if (line_end == std::string_view::npos)
        {
            break;
        }
        line_start = line_end + 1U;
    }

    return values;
}

std::uint32_t parse_u32(
    gs1::testing::SystemTestExecutionContext& context,
    const std::map<std::string, std::string, std::less<>>& values,
    std::string_view key,
    std::uint32_t fallback = 0U)
{
    const auto it = values.find(std::string(key));
    if (it == values.end())
    {
        return fallback;
    }

    try
    {
        return static_cast<std::uint32_t>(std::stoul(it->second));
    }
    catch (...)
    {
        context.fail(__FILE__, static_cast<std::uint32_t>(__LINE__), std::string("Invalid integer for key '") + std::string(key) + "'.");
        return fallback;
    }
}

std::int32_t parse_i32(
    gs1::testing::SystemTestExecutionContext& context,
    const std::map<std::string, std::string, std::less<>>& values,
    std::string_view key,
    std::int32_t fallback = 0)
{
    const auto it = values.find(std::string(key));
    if (it == values.end())
    {
        return fallback;
    }

    try
    {
        return static_cast<std::int32_t>(std::stol(it->second));
    }
    catch (...)
    {
        context.fail(__FILE__, static_cast<std::uint32_t>(__LINE__), std::string("Invalid integer for key '") + std::string(key) + "'.");
        return fallback;
    }
}

float parse_float(
    gs1::testing::SystemTestExecutionContext& context,
    const std::map<std::string, std::string, std::less<>>& values,
    std::string_view key,
    float fallback = 0.0f)
{
    const auto it = values.find(std::string(key));
    if (it == values.end())
    {
        return fallback;
    }

    try
    {
        return std::stof(it->second);
    }
    catch (...)
    {
        context.fail(__FILE__, static_cast<std::uint32_t>(__LINE__), std::string("Invalid float for key '") + std::string(key) + "'.");
        return fallback;
    }
}

bool parse_bool(
    const std::map<std::string, std::string, std::less<>>& values,
    std::string_view key,
    bool fallback = false)
{
    const auto it = values.find(std::string(key));
    if (it == values.end())
    {
        return fallback;
    }

    return it->second == "true" || it->second == "1" || it->second == "yes";
}

std::string parse_string(
    const std::map<std::string, std::string, std::less<>>& values,
    std::string_view key,
    std::string fallback = {})
{
    const auto it = values.find(std::string(key));
    return it == values.end() ? fallback : it->second;
}

Gs1Status parse_status(
    const std::map<std::string, std::string, std::less<>>& values,
    std::string_view key,
    Gs1Status fallback = GS1_STATUS_OK)
{
    const auto it = values.find(std::string(key));
    if (it == values.end())
    {
        return fallback;
    }

    if (it->second == "ok")
    {
        return GS1_STATUS_OK;
    }
    if (it->second == "invalid_argument")
    {
        return GS1_STATUS_INVALID_ARGUMENT;
    }
    if (it->second == "invalid_state")
    {
        return GS1_STATUS_INVALID_STATE;
    }
    if (it->second == "not_found")
    {
        return GS1_STATUS_NOT_FOUND;
    }

    return fallback;
}

PlacementOccupancyLayer parse_occupancy_layer(
    const std::map<std::string, std::string, std::less<>>& values,
    std::string_view key,
    PlacementOccupancyLayer fallback = PlacementOccupancyLayer::GroundCover)
{
    const auto it = values.find(std::string(key));
    if (it == values.end())
    {
        return fallback;
    }

    if (it->second == "ground_cover")
    {
        return PlacementOccupancyLayer::GroundCover;
    }
    if (it->second == "structure")
    {
        return PlacementOccupancyLayer::Structure;
    }
    if (it->second == "none")
    {
        return PlacementOccupancyLayer::None;
    }

    return fallback;
}

PlacementReservationRejectionReason parse_rejection_reason(
    const std::map<std::string, std::string, std::less<>>& values,
    std::string_view key,
    PlacementReservationRejectionReason fallback = PlacementReservationRejectionReason::None)
{
    const auto it = values.find(std::string(key));
    if (it == values.end())
    {
        return fallback;
    }

    if (it->second == "out_of_bounds")
    {
        return PlacementReservationRejectionReason::OutOfBounds;
    }
    if (it->second == "terrain_blocked")
    {
        return PlacementReservationRejectionReason::TerrainBlocked;
    }
    if (it->second == "occupied")
    {
        return PlacementReservationRejectionReason::Occupied;
    }
    if (it->second == "reserved")
    {
        return PlacementReservationRejectionReason::Reserved;
    }

    return fallback;
}

template <typename Payload>
GameMessage make_message(gs1::GameMessageType type, const Payload& payload)
{
    GameMessage message {};
    message.type = type;
    message.set_payload(payload);
    return message;
}

void inventory_regression_runner(
    gs1::testing::SystemTestExecutionContext& context,
    const gs1::testing::SystemTestAssetDocument& document)
{
    const auto values = parse_payload_map(document.payload_text);
    const std::string scenario = parse_string(values, "scenario");
    auto campaign = make_campaign();
    const std::uint32_t site_id = parse_u32(context, values, "site_id", 1U);
    auto site_run = make_test_site_run(site_id, 2001U);
    GameMessageQueue queue {};
    auto site_context = make_site_context<InventorySystem>(campaign, site_run, queue);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        InventorySystem::process_message(
            site_context,
            make_message(
                GameMessageType::SiteRunStarted,
                SiteRunStartedMessage {site_id, 1U, 101U, 1U, 42ULL})) == GS1_STATUS_OK);

    if (scenario == "use_item" || scenario == "transfer")
    {
        site_run.inventory.worker_pack_slots[0].occupied = true;
        site_run.inventory.worker_pack_slots[0].item_id = gs1::ItemId {gs1::k_item_water_container};
        site_run.inventory.worker_pack_slots[0].item_quantity = 2U;
        site_run.inventory.worker_pack_slots[0].item_condition = 1.0f;
        site_run.inventory.worker_pack_slots[0].item_freshness = 1.0f;
    }

    if (scenario == "use_item")
    {
        const auto status = InventorySystem::process_message(
            site_context,
            make_message(
                GameMessageType::InventoryItemUseRequested,
                gs1::InventoryItemUseRequestedMessage {
                    parse_u32(context, values, "item_id"),
                    site_run.inventory.worker_pack_storage_id,
                    static_cast<std::uint16_t>(parse_u32(context, values, "quantity", 1U)),
                    static_cast<std::uint16_t>(parse_u32(context, values, "slot", 0U))}));
        GS1_SYSTEM_TEST_CHECK(context, status == parse_status(values, "expect_status", GS1_STATUS_OK));
        GS1_SYSTEM_TEST_CHECK(
            context,
            site_run.inventory.worker_pack_slots[parse_u32(context, values, "slot", 0U)].item_quantity ==
                parse_u32(context, values, "expect_remaining_quantity"));
        GS1_SYSTEM_TEST_CHECK(
            context,
            count_messages(queue, GameMessageType::WorkerMeterDeltaRequested) ==
                (parse_bool(values, "expect_meter_message", true) ? 1U : 0U));
        return;
    }

    if (scenario == "transfer")
    {
        const std::uint32_t source_slot = parse_u32(context, values, "source_slot", 0U);
        const std::uint32_t destination_slot = parse_u32(context, values, "destination_slot", 1U);
        if (parse_bool(values, "prefill_destination_with_same_item", false))
        {
            site_run.inventory.worker_pack_slots[destination_slot] = site_run.inventory.worker_pack_slots[source_slot];
            site_run.inventory.worker_pack_slots[destination_slot].item_quantity =
                parse_u32(context, values, "prefill_destination_quantity", 1U);
        }

        const auto status = InventorySystem::process_message(
            site_context,
            make_message(
                GameMessageType::InventoryTransferRequested,
                gs1::InventoryTransferRequestedMessage {
                    site_run.inventory.worker_pack_storage_id,
                    site_run.inventory.worker_pack_storage_id,
                    static_cast<std::uint16_t>(source_slot),
                    static_cast<std::uint16_t>(destination_slot),
                    static_cast<std::uint16_t>(parse_u32(context, values, "quantity", 1U)),
                    0U,
                    0U}));
        GS1_SYSTEM_TEST_CHECK(context, status == parse_status(values, "expect_status", GS1_STATUS_OK));
        GS1_SYSTEM_TEST_CHECK(
            context,
            site_run.inventory.worker_pack_slots[source_slot].item_quantity ==
                parse_u32(context, values, "expect_source_quantity"));
        GS1_SYSTEM_TEST_CHECK(
            context,
            site_run.inventory.worker_pack_slots[destination_slot].item_quantity ==
                parse_u32(context, values, "expect_destination_quantity"));
        GS1_SYSTEM_TEST_CHECK(
            context,
            site_run.inventory.worker_pack_slots[destination_slot].occupied ==
                parse_bool(values, "expect_destination_occupied", true));
        return;
    }

    GS1_SYSTEM_TEST_FAIL(context, "Unknown inventory asset regression scenario.");
}

void economy_phone_regression_runner(
    gs1::testing::SystemTestExecutionContext& context,
    const gs1::testing::SystemTestAssetDocument& document)
{
    const auto values = parse_payload_map(document.payload_text);
    const std::string scenario = parse_string(values, "scenario");
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(1U, 2002U);
    GameMessageQueue queue {};
    auto site_context = make_site_context<EconomyPhoneSystem>(campaign, site_run, queue);

    GS1_SYSTEM_TEST_REQUIRE(
        context,
        EconomyPhoneSystem::process_message(
            site_context,
            make_message(
                GameMessageType::SiteRunStarted,
                SiteRunStartedMessage {1U, 1U, 101U, 1U, 42ULL})) == GS1_STATUS_OK);

    if (scenario == "purchase_unlockable_listing")
    {
        const auto status = EconomyPhoneSystem::process_message(
            site_context,
            make_message(
                GameMessageType::PhoneListingPurchaseRequested,
                gs1::PhoneListingPurchaseRequestedMessage {
                    parse_u32(context, values, "listing_id"),
                    static_cast<std::uint16_t>(parse_u32(context, values, "quantity", 1U)),
                    0U}));
        GS1_SYSTEM_TEST_CHECK(context, status == GS1_STATUS_OK);
        GS1_SYSTEM_TEST_CHECK(context, site_run.economy.money == static_cast<std::int32_t>(parse_u32(context, values, "expect_money")));
        GS1_SYSTEM_TEST_CHECK(
            context,
            site_run.economy.direct_purchase_unlockable_ids.size() ==
                parse_u32(context, values, "expect_direct_unlockable_count"));
        return;
    }

    if (scenario == "purchase_listing")
    {
        const auto status = EconomyPhoneSystem::process_message(
            site_context,
            make_message(
                GameMessageType::PhoneListingPurchaseRequested,
                gs1::PhoneListingPurchaseRequestedMessage {
                    parse_u32(context, values, "listing_id"),
                    static_cast<std::uint16_t>(parse_u32(context, values, "quantity", 1U)),
                    0U}));
        GS1_SYSTEM_TEST_CHECK(context, status == parse_status(values, "expect_status", GS1_STATUS_OK));
        GS1_SYSTEM_TEST_CHECK(
            context,
            site_run.economy.money == static_cast<std::int32_t>(parse_u32(context, values, "expect_money", static_cast<std::uint32_t>(site_run.economy.money))));
        return;
    }

    if (scenario == "hire_contractor")
    {
        if (values.contains("override_money"))
        {
            site_run.economy.money = static_cast<std::int32_t>(parse_u32(context, values, "override_money"));
        }

        const auto status = EconomyPhoneSystem::process_message(
            site_context,
            make_message(
                GameMessageType::ContractorHireRequested,
                gs1::ContractorHireRequestedMessage {
                    parse_u32(context, values, "listing_id"),
                    parse_u32(context, values, "work_units", 1U)}));
        GS1_SYSTEM_TEST_CHECK(context, status == parse_status(values, "expect_status", GS1_STATUS_OK));
        GS1_SYSTEM_TEST_CHECK(
            context,
            site_run.economy.money == static_cast<std::int32_t>(parse_u32(context, values, "expect_money", static_cast<std::uint32_t>(site_run.economy.money))));
        return;
    }

    GS1_SYSTEM_TEST_FAIL(context, "Unknown economy_phone asset regression scenario.");
}

void task_board_regression_runner(
    gs1::testing::SystemTestExecutionContext& context,
    const gs1::testing::SystemTestAssetDocument& document)
{
    const auto values = parse_payload_map(document.payload_text);
    const std::string scenario = parse_string(values, "scenario");
    auto campaign = make_campaign();
    const std::uint32_t site_id = parse_u32(context, values, "site_id", 2U);
    auto site_run = make_test_site_run(site_id, 2003U);
    GameMessageQueue queue {};
    auto site_context = make_site_context<TaskBoardSystem>(campaign, site_run, queue);

    if (parse_bool(values, "prepopulate_board", false))
    {
        gs1::TaskInstanceState task {};
        task.task_instance_id = gs1::TaskInstanceId {91U};
        site_run.task_board.visible_tasks.push_back(task);
        site_run.task_board.accepted_task_ids.push_back(task.task_instance_id);
        site_run.task_board.completed_task_ids.push_back(gs1::TaskInstanceId {92U});
        site_run.task_board.task_pool_size = 3U;
        site_run.task_board.minutes_until_next_refresh = 8.0;
    }

    if (scenario == "reset_non_site")
    {
        GS1_SYSTEM_TEST_REQUIRE(
            context,
            TaskBoardSystem::process_message(
                site_context,
                make_message(
                    GameMessageType::SiteRunStarted,
                    SiteRunStartedMessage {site_id, 1U, 102U, 1U, 42ULL})) == GS1_STATUS_OK);
        GS1_SYSTEM_TEST_CHECK(context, site_run.task_board.visible_tasks.size() == parse_u32(context, values, "expect_visible"));
        GS1_SYSTEM_TEST_CHECK(context, site_run.task_board.accepted_task_ids.size() == parse_u32(context, values, "expect_accepted"));
        GS1_SYSTEM_TEST_CHECK(context, site_run.task_board.completed_task_ids.size() == parse_u32(context, values, "expect_completed"));
        return;
    }

    if (scenario == "accept_and_complete_site_one")
    {
        GS1_SYSTEM_TEST_REQUIRE(
            context,
            TaskBoardSystem::process_message(
                site_context,
                make_message(
                    GameMessageType::SiteRunStarted,
                    SiteRunStartedMessage {1U, 1U, 101U, 1U, 42ULL})) == GS1_STATUS_OK);
        GS1_SYSTEM_TEST_REQUIRE(context, !site_run.task_board.visible_tasks.empty());
        const auto task_id = site_run.task_board.visible_tasks.front().task_instance_id.value;
        GS1_SYSTEM_TEST_REQUIRE(
            context,
            TaskBoardSystem::process_message(
                site_context,
                make_message(
                    GameMessageType::TaskAcceptRequested,
                    gs1::TaskAcceptRequestedMessage {task_id})) == GS1_STATUS_OK);
        GS1_SYSTEM_TEST_REQUIRE(
            context,
            TaskBoardSystem::process_message(
                site_context,
                make_message(
                    GameMessageType::RestorationProgressChanged,
                    gs1::RestorationProgressChangedMessage {
                        site_run.task_board.visible_tasks.front().target_amount,
                        site_run.task_board.visible_tasks.front().target_amount,
                        1.0f})) == GS1_STATUS_OK);
        GS1_SYSTEM_TEST_CHECK(
            context,
            site_run.task_board.visible_tasks.front().runtime_list_kind ==
                (parse_bool(values, "expect_completed_runtime_list", true)
                    ? TaskRuntimeListKind::Completed
                    : TaskRuntimeListKind::Accepted));
        GS1_SYSTEM_TEST_CHECK(
            context,
            site_run.task_board.completed_task_ids.size() ==
                parse_u32(context, values, "expect_completed_count", 1U));
        return;
    }

    GS1_SYSTEM_TEST_FAIL(context, "Unknown task_board asset regression scenario.");
}

void placement_validation_regression_runner(
    gs1::testing::SystemTestExecutionContext& context,
    const gs1::testing::SystemTestAssetDocument& document)
{
    const auto values = parse_payload_map(document.payload_text);
    const std::string scenario = parse_string(values, "scenario");
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(parse_u32(context, values, "site_id", 1U), 2004U);
    GameMessageQueue queue {};
    auto site_context = make_site_context<PlacementValidationSystem>(campaign, site_run, queue);

    if (scenario == "request")
    {
        const TileCoord coord {
            parse_i32(context, values, "x", 0),
            parse_i32(context, values, "y", 0)};
        if (site_run.site_world->contains(coord))
        {
            auto tile = site_run.site_world->tile_at(coord);
            tile.static_data.traversable = parse_bool(values, "tile_traversable", tile.static_data.traversable);
            tile.static_data.plantable = parse_bool(values, "tile_plantable", tile.static_data.plantable);
            tile.static_data.reserved_by_structure =
                parse_bool(values, "tile_reserved_by_structure", tile.static_data.reserved_by_structure);
            tile.ecology.plant_id = gs1::PlantId {parse_u32(context, values, "tile_plant_id", tile.ecology.plant_id.value)};
            tile.ecology.ground_cover_type_id =
                parse_u32(context, values, "tile_ground_cover_type_id", tile.ecology.ground_cover_type_id);
            tile.device.structure_id =
                gs1::StructureId {parse_u32(context, values, "tile_structure_id", tile.device.structure_id.value)};
            site_run.site_world->set_tile(coord, tile);
        }

        if (parse_bool(values, "pre_reserve_same_tile", false))
        {
            const auto occupancy_layer = parse_occupancy_layer(values, "layer");
            const auto subject_kind =
                occupancy_layer == PlacementOccupancyLayer::Structure
                ? gs1::PlacementReservationSubjectKind::StructureType
                : gs1::PlacementReservationSubjectKind::GroundCoverType;
            GS1_SYSTEM_TEST_REQUIRE(
                context,
                PlacementValidationSystem::process_message(
                    site_context,
                    make_message(
                        GameMessageType::PlacementReservationRequested,
                        gs1::PlacementReservationRequestedMessage {
                            parse_u32(context, values, "pre_action_id", 90U),
                            coord.x,
                            coord.y,
                            occupancy_layer,
                            subject_kind,
                            {0U, 0U},
                            1U})) == GS1_STATUS_OK);
            queue.clear();
        }

        const auto occupancy_layer = parse_occupancy_layer(values, "layer");
        const auto subject_kind =
            occupancy_layer == PlacementOccupancyLayer::Structure
            ? gs1::PlacementReservationSubjectKind::StructureType
            : gs1::PlacementReservationSubjectKind::GroundCoverType;
        GS1_SYSTEM_TEST_REQUIRE(
            context,
            PlacementValidationSystem::process_message(
                site_context,
                make_message(
                    GameMessageType::PlacementReservationRequested,
                    gs1::PlacementReservationRequestedMessage {
                        parse_u32(context, values, "action_id", 91U),
                        coord.x,
                        coord.y,
                        occupancy_layer,
                        subject_kind,
                        {0U, 0U},
                        1U})) == GS1_STATUS_OK);
        GS1_SYSTEM_TEST_REQUIRE(context, queue.size() == 1U);
        const std::string expect_result = parse_string(values, "expect_result", "accepted");
        if (expect_result == "accepted")
        {
            GS1_SYSTEM_TEST_CHECK(context, queue.front().type == GameMessageType::PlacementReservationAccepted);
        }
        else
        {
            GS1_SYSTEM_TEST_CHECK(context, queue.front().type == GameMessageType::PlacementReservationRejected);
            GS1_SYSTEM_TEST_CHECK(
                context,
                queue.front().payload_as<gs1::PlacementReservationRejectedMessage>().reason_code ==
                    parse_rejection_reason(values, "expect_reason"));
        }
        return;
    }

    GS1_SYSTEM_TEST_FAIL(context, "Unknown placement_validation asset regression scenario.");
}

void ecology_regression_runner(
    gs1::testing::SystemTestExecutionContext& context,
    const gs1::testing::SystemTestAssetDocument& document)
{
    const auto values = parse_payload_map(document.payload_text);
    const std::string scenario = parse_string(values, "scenario");
    auto campaign = make_campaign();
    auto site_run = make_test_site_run(parse_u32(context, values, "site_id", 1U), 2005U);
    GameMessageQueue queue {};
    auto site_context = make_site_context<EcologySystem>(
        campaign,
        site_run,
        queue,
        static_cast<double>(parse_float(context, values, "step_seconds", 1.0f)));

    const TileCoord coord {
        parse_i32(context, values, "x", 1),
        parse_i32(context, values, "y", 1)};
    if (site_run.site_world->contains(coord))
    {
        auto tile = site_run.site_world->tile_at(coord);
        tile.ecology.plant_id = gs1::PlantId {parse_u32(context, values, "tile_plant_id", tile.ecology.plant_id.value)};
        tile.ecology.ground_cover_type_id =
            parse_u32(context, values, "tile_ground_cover_type_id", tile.ecology.ground_cover_type_id);
        tile.ecology.plant_density = parse_float(context, values, "tile_density", tile.ecology.plant_density);
        tile.ecology.moisture = parse_float(context, values, "tile_moisture", tile.ecology.moisture);
        tile.ecology.sand_burial = parse_float(context, values, "tile_sand_burial", tile.ecology.sand_burial);
        site_run.site_world->set_tile(coord, tile);
    }

    if (scenario == "water")
    {
        GS1_SYSTEM_TEST_REQUIRE(
            context,
            EcologySystem::process_message(
                site_context,
                make_message(
                    GameMessageType::SiteTileWatered,
                    gs1::SiteTileWateredMessage {
                        parse_u32(context, values, "source_id", 1U),
                        coord.x,
                        coord.y,
                        parse_float(context, values, "water_amount", 1.0f),
                        0U})) == GS1_STATUS_OK);
        const auto tile = site_run.site_world->tile_at(coord);
        if (values.contains("expect_moisture"))
        {
            GS1_SYSTEM_TEST_CHECK(
                context,
                approx_equal(
                    tile.ecology.moisture,
                    parse_float(context, values, "expect_moisture")));
        }
        else
        {
            GS1_SYSTEM_TEST_CHECK(
                context,
                approx_equal(
                    tile.ecology.plant_density,
                    parse_float(context, values, "expect_density")));
        }
        GS1_SYSTEM_TEST_CHECK(
            context,
            count_messages(queue, GameMessageType::TileEcologyChanged) ==
                parse_u32(context, values, "expect_tile_ecology_changed_count", 1U));
        return;
    }

    if (scenario == "clear_burial")
    {
        GS1_SYSTEM_TEST_REQUIRE(
            context,
            EcologySystem::process_message(
                site_context,
                make_message(
                    GameMessageType::SiteTileBurialCleared,
                    gs1::SiteTileBurialClearedMessage {
                        parse_u32(context, values, "source_id", 1U),
                        coord.x,
                        coord.y,
                        parse_float(context, values, "cleared_amount", 0.5f),
                        0U})) == GS1_STATUS_OK);
        const auto tile = site_run.site_world->tile_at(coord);
        GS1_SYSTEM_TEST_CHECK(context, approx_equal(tile.ecology.sand_burial, parse_float(context, values, "expect_sand_burial")));
        GS1_SYSTEM_TEST_CHECK(
            context,
            count_messages(queue, GameMessageType::TileEcologyChanged) ==
                parse_u32(context, values, "expect_tile_ecology_changed_count", 1U));
        return;
    }

    if (scenario == "run_growth")
    {
        site_run.counters.site_completion_tile_threshold =
            parse_u32(context, values, "site_completion_tile_threshold", 1U);
        EcologySystem::run(site_context);
        const auto tile = site_run.site_world->tile_at(coord);
        GS1_SYSTEM_TEST_CHECK(context, approx_equal(tile.ecology.plant_density, parse_float(context, values, "expect_density")));
        GS1_SYSTEM_TEST_CHECK(
            context,
            site_run.counters.fully_grown_tile_count ==
                parse_u32(context, values, "expect_fully_grown_count"));
        GS1_SYSTEM_TEST_CHECK(
            context,
            count_messages(queue, GameMessageType::RestorationProgressChanged) ==
                parse_u32(context, values, "expect_progress_message_count", 1U));
        return;
    }

    GS1_SYSTEM_TEST_FAIL(context, "Unknown ecology asset regression scenario.");
}
}  // namespace

GS1_REGISTER_SYSTEM_TEST_ASSET_RUNNER("inventory_regression", inventory_regression_runner);
GS1_REGISTER_SYSTEM_TEST_ASSET_RUNNER("economy_phone_regression", economy_phone_regression_runner);
GS1_REGISTER_SYSTEM_TEST_ASSET_RUNNER("task_board_regression", task_board_regression_runner);
GS1_REGISTER_SYSTEM_TEST_ASSET_RUNNER("placement_validation_regression", placement_validation_regression_runner);
GS1_REGISTER_SYSTEM_TEST_ASSET_RUNNER("ecology_regression", ecology_regression_runner);
