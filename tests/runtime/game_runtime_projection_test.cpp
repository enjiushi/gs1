#include "runtime/game_runtime.h"
#include "content/defs/item_defs.h"
#include "site/site_world_access.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <vector>

namespace
{
using gs1::GameCommand;
using gs1::GameCommandType;
using gs1::GameRuntime;
using gs1::InventoryItemUseRequestedCommand;
using gs1::PhoneListingPurchaseRequestedCommand;
using gs1::SiteId;
using gs1::StartNewCampaignCommand;
using gs1::StartSiteAttemptCommand;
using gs1::TaskAcceptRequestedCommand;
using gs1::TileCoord;
}

namespace gs1
{
struct GameRuntimeProjectionTestAccess
{
    static std::optional<CampaignState>& campaign(GameRuntime& runtime)
    {
        return runtime.campaign_;
    }

    static std::optional<SiteRunState>& active_site_run(GameRuntime& runtime)
    {
        return runtime.active_site_run_;
    }

    static void mark_tile_dirty(GameRuntime& runtime, TileCoord coord)
    {
        runtime.mark_site_tile_projection_dirty(coord);
    }

    static void mark_all_tiles_dirty(GameRuntime& runtime)
    {
        runtime.mark_site_projection_update_dirty(1ULL << 0);
    }

    static void flush_projection(GameRuntime& runtime)
    {
        runtime.flush_site_presentation_if_dirty();
    }
};
}  // namespace gs1

namespace
{

GameCommand make_start_campaign_command()
{
    GameCommand command {};
    command.type = GameCommandType::StartNewCampaign;
    command.set_payload(StartNewCampaignCommand {42ULL, 30U});
    return command;
}

GameCommand make_start_site_attempt_command(std::uint32_t site_id)
{
    GameCommand command {};
    command.type = GameCommandType::StartSiteAttempt;
    command.set_payload(StartSiteAttemptCommand {site_id});
    return command;
}

std::vector<Gs1EngineCommand> drain_engine_commands(GameRuntime& runtime)
{
    std::vector<Gs1EngineCommand> commands {};
    Gs1EngineCommand command {};
    while (runtime.pop_engine_command(command) == GS1_STATUS_OK)
    {
        commands.push_back(command);
    }
    return commands;
}

Gs1HostEvent make_site_action_request_event(
    Gs1SiteActionKind action_kind,
    TileCoord target_tile,
    std::uint32_t primary_subject_id,
    std::uint32_t item_id = 0U)
{
    Gs1HostEvent event {};
    event.type = GS1_HOST_EVENT_SITE_ACTION_REQUEST;
    event.payload.site_action_request.action_kind = action_kind;
    event.payload.site_action_request.flags = item_id != 0U
        ? GS1_SITE_ACTION_REQUEST_FLAG_HAS_ITEM
        : GS1_SITE_ACTION_REQUEST_FLAG_HAS_PRIMARY_SUBJECT;
    event.payload.site_action_request.quantity = 1U;
    event.payload.site_action_request.target_tile_x = target_tile.x;
    event.payload.site_action_request.target_tile_y = target_tile.y;
    event.payload.site_action_request.primary_subject_id = primary_subject_id;
    event.payload.site_action_request.item_id = item_id;
    return event;
}

std::uint64_t pack_inventory_use_arg(
    Gs1InventoryContainerKind container_kind,
    std::uint32_t slot_index,
    std::uint32_t quantity)
{
    return static_cast<std::uint64_t>(container_kind) |
        (static_cast<std::uint64_t>(slot_index & 0xffU) << 8U) |
        (static_cast<std::uint64_t>(quantity & 0xffffU) << 16U);
}

std::uint64_t pack_inventory_transfer_arg(
    Gs1InventoryContainerKind source_container_kind,
    std::uint32_t source_slot_index,
    Gs1InventoryContainerKind destination_container_kind,
    std::uint32_t destination_slot_index,
    std::uint32_t quantity)
{
    return static_cast<std::uint64_t>(source_container_kind) |
        (static_cast<std::uint64_t>(source_slot_index & 0xffU) << 8U) |
        (static_cast<std::uint64_t>(destination_container_kind) << 16U) |
        (static_cast<std::uint64_t>(destination_slot_index & 0xffU) << 24U) |
        (static_cast<std::uint64_t>(quantity & 0xffffU) << 32U);
}

Gs1HostEvent make_ui_action_event(const Gs1UiAction& action)
{
    Gs1HostEvent event {};
    event.type = GS1_HOST_EVENT_UI_ACTION;
    event.payload.ui_action.action = action;
    return event;
}

void run_phase1(GameRuntime& runtime, double real_delta_seconds, Gs1Phase1Result& out_result)
{
    Gs1Phase1Request request {};
    request.struct_size = sizeof(Gs1Phase1Request);
    request.real_delta_seconds = real_delta_seconds;

    out_result = {};
    assert(runtime.run_phase1(request, out_result) == GS1_STATUS_OK);
}

void run_phase1(GameRuntime& runtime, double real_delta_seconds)
{
    Gs1Phase1Result result {};
    run_phase1(runtime, real_delta_seconds, result);
}

void set_tile_sand_burial(gs1::SiteRunState& site_run, TileCoord coord, float sand_burial)
{
    auto ecology = gs1::site_world_access::tile_ecology(site_run, coord);
    ecology.sand_burial = sand_burial;
    gs1::site_world_access::set_tile_ecology(site_run, coord, ecology);
}

std::vector<Gs1EngineCommand> flush_tile_delta_for(
    GameRuntime& runtime,
    TileCoord coord,
    float sand_burial)
{
    auto& site_run = gs1::GameRuntimeProjectionTestAccess::active_site_run(runtime).value();
    set_tile_sand_burial(site_run, coord, sand_burial);
    gs1::GameRuntimeProjectionTestAccess::mark_tile_dirty(runtime, coord);
    gs1::GameRuntimeProjectionTestAccess::flush_projection(runtime);
    return drain_engine_commands(runtime);
}

std::vector<const Gs1EngineCommand*> collect_commands_of_type(
    const std::vector<Gs1EngineCommand>& commands,
    Gs1EngineCommandType type)
{
    std::vector<const Gs1EngineCommand*> matches {};
    for (const auto& command : commands)
    {
        if (command.type == type)
        {
            matches.push_back(&command);
        }
    }
    return matches;
}

std::vector<const Gs1EngineCommand*> collect_inventory_slot_commands(
    const std::vector<Gs1EngineCommand>& commands)
{
    return collect_commands_of_type(commands, GS1_ENGINE_COMMAND_SITE_INVENTORY_SLOT_UPSERT);
}

const Gs1EngineCommand* find_inventory_slot_command(
    const std::vector<Gs1EngineCommand>& commands,
    Gs1InventoryContainerKind container_kind,
    std::uint16_t slot_index)
{
    for (const auto& command : commands)
    {
        if (command.type != GS1_ENGINE_COMMAND_SITE_INVENTORY_SLOT_UPSERT)
        {
            continue;
        }

        const auto& payload = command.payload_as<Gs1EngineCommandInventorySlotData>();
        if (payload.container_kind == container_kind && payload.slot_index == slot_index)
        {
            return &command;
        }
    }

    return nullptr;
}
}  // namespace

int main()
{
    Gs1RuntimeCreateDesc create_desc {};
    create_desc.struct_size = sizeof(Gs1RuntimeCreateDesc);
    create_desc.api_version = gs1::k_api_version;
    create_desc.fixed_step_seconds = 1.0 / 60.0;

    GameRuntime runtime {create_desc};

    assert(runtime.handle_command(make_start_campaign_command()) == GS1_STATUS_OK);
    assert(gs1::GameRuntimeProjectionTestAccess::campaign(runtime).has_value());

    const auto first_site_id = gs1::GameRuntimeProjectionTestAccess::campaign(runtime)->sites.front().site_id.value;
    assert(runtime.handle_command(make_start_site_attempt_command(first_site_id)) == GS1_STATUS_OK);
    assert(gs1::GameRuntimeProjectionTestAccess::active_site_run(runtime).has_value());

    auto& bootstrap_site_run = gs1::GameRuntimeProjectionTestAccess::active_site_run(runtime).value();
    assert(gs1::site_world_access::width(bootstrap_site_run) == 32U);
    assert(gs1::site_world_access::height(bootstrap_site_run) == 32U);
    assert(bootstrap_site_run.inventory.worker_pack_slots.size() == 6U);
    assert(bootstrap_site_run.inventory.worker_pack_slots[0].occupied);
    assert(bootstrap_site_run.inventory.worker_pack_slots[0].item_id.value == 1U);
    assert(bootstrap_site_run.task_board.visible_tasks.size() == 1U);
    assert(bootstrap_site_run.economy.money == 45);
    assert(bootstrap_site_run.economy.available_phone_listings.size() == 10U);

    const auto bootstrap_commands = drain_engine_commands(runtime);
    assert(!collect_commands_of_type(bootstrap_commands, GS1_ENGINE_COMMAND_SITE_INVENTORY_SLOT_UPSERT).empty());
    assert(!collect_commands_of_type(bootstrap_commands, GS1_ENGINE_COMMAND_SITE_TASK_UPSERT).empty());
    assert(!collect_commands_of_type(bootstrap_commands, GS1_ENGINE_COMMAND_SITE_PHONE_LISTING_UPSERT).empty());

    GameCommand accept_task {};
    accept_task.type = GameCommandType::TaskAcceptRequested;
    accept_task.set_payload(TaskAcceptRequestedCommand {1U});
    assert(runtime.handle_command(accept_task) == GS1_STATUS_OK);
    assert(bootstrap_site_run.task_board.accepted_task_ids.size() == 1U);

    auto worker = gs1::site_world_access::worker_conditions(bootstrap_site_run);
    worker.hydration = 80.0f;
    gs1::site_world_access::set_worker_conditions(bootstrap_site_run, worker);
    GameCommand use_item {};
    use_item.type = GameCommandType::InventoryItemUseRequested;
    use_item.set_payload(InventoryItemUseRequestedCommand {
        1U,
        1U,
        GS1_INVENTORY_CONTAINER_WORKER_PACK,
        0U});
    assert(runtime.handle_command(use_item) == GS1_STATUS_OK);
    assert(bootstrap_site_run.inventory.worker_pack_slots[0].item_quantity == 1U);
    assert(gs1::site_world_access::worker_conditions(bootstrap_site_run).hydration > 80.0f);

    GameCommand buy_listing {};
    buy_listing.type = GameCommandType::PhoneListingPurchaseRequested;
    buy_listing.set_payload(PhoneListingPurchaseRequestedCommand {1U, 1U, 0U});
    assert(runtime.handle_command(buy_listing) == GS1_STATUS_OK);
    assert(bootstrap_site_run.economy.money == 40);
    assert(bootstrap_site_run.economy.available_phone_listings[0].quantity == 5U);
    gs1::GameRuntimeProjectionTestAccess::flush_projection(runtime);
    drain_engine_commands(runtime);

    Gs1Phase1Result delivery_result {};
    run_phase1(runtime, 3.0, delivery_result);
    assert(bootstrap_site_run.inventory.camp_storage_slots[4].occupied);
    assert(bootstrap_site_run.inventory.camp_storage_slots[4].item_id.value == gs1::k_item_water_container);
    gs1::GameRuntimeProjectionTestAccess::flush_projection(runtime);
    const auto delivery_commands = drain_engine_commands(runtime);
    const auto delivery_inventory_commands = collect_inventory_slot_commands(delivery_commands);
    assert(delivery_inventory_commands.size() == 1U);
    {
        const auto& payload = delivery_inventory_commands.front()->payload_as<Gs1EngineCommandInventorySlotData>();
        assert(payload.container_kind == GS1_INVENTORY_CONTAINER_CAMP_STORAGE);
        assert(payload.slot_index == 4U);
        assert(payload.item_id == gs1::k_item_water_container);
        assert(payload.quantity == 1U);
    }

    const auto first_commands = flush_tile_delta_for(runtime, TileCoord {3, 2}, 0.25f);
    const auto first_tiles = collect_commands_of_type(first_commands, GS1_ENGINE_COMMAND_SITE_TILE_UPSERT);
    assert(first_commands.size() == 3U);
    assert(first_commands.front().type == GS1_ENGINE_COMMAND_BEGIN_SITE_SNAPSHOT);
    assert(first_commands.back().type == GS1_ENGINE_COMMAND_END_SITE_SNAPSHOT);
    assert(first_tiles.size() == 1U);
    {
        const auto& payload = first_tiles.front()->payload_as<Gs1EngineCommandSiteTileData>();
        assert(payload.x == 3U);
        assert(payload.y == 2U);
        assert(payload.sand_burial == 0.25f);
    }

    auto& site_run = gs1::GameRuntimeProjectionTestAccess::active_site_run(runtime).value();
    const auto repeated_coord = TileCoord {5, 4};
    set_tile_sand_burial(site_run, repeated_coord, 0.5f);
    gs1::GameRuntimeProjectionTestAccess::mark_tile_dirty(runtime, repeated_coord);
    gs1::GameRuntimeProjectionTestAccess::mark_tile_dirty(runtime, repeated_coord);
    set_tile_sand_burial(site_run, TileCoord {1, 1}, 0.75f);
    gs1::GameRuntimeProjectionTestAccess::mark_tile_dirty(runtime, TileCoord {1, 1});
    gs1::GameRuntimeProjectionTestAccess::flush_projection(runtime);

    const auto second_commands = drain_engine_commands(runtime);
    const auto second_tiles = collect_commands_of_type(second_commands, GS1_ENGINE_COMMAND_SITE_TILE_UPSERT);
    assert(second_commands.size() == 4U);
    assert(second_tiles.size() == 2U);
    {
        const auto& first_tile = second_tiles[0]->payload_as<Gs1EngineCommandSiteTileData>();
        const auto& second_tile = second_tiles[1]->payload_as<Gs1EngineCommandSiteTileData>();
        assert(first_tile.x == 1U && first_tile.y == 1U);
        assert(first_tile.sand_burial == 0.75f);
        assert(second_tile.x == 5U && second_tile.y == 4U);
        assert(second_tile.sand_burial == 0.5f);
    }

    set_tile_sand_burial(site_run, TileCoord {0, 0}, 0.9f);
    gs1::GameRuntimeProjectionTestAccess::mark_all_tiles_dirty(runtime);
    gs1::GameRuntimeProjectionTestAccess::flush_projection(runtime);
    const auto fallback_commands = drain_engine_commands(runtime);
    const auto fallback_tiles = collect_commands_of_type(fallback_commands, GS1_ENGINE_COMMAND_SITE_TILE_UPSERT);
    assert(!fallback_tiles.empty());
    assert(fallback_tiles.size() == gs1::site_world_access::tile_count(site_run));

    Gs1RuntimeCreateDesc ui_desc {};
    ui_desc.struct_size = sizeof(Gs1RuntimeCreateDesc);
    ui_desc.api_version = gs1::k_api_version;
    ui_desc.fixed_step_seconds = 1.0 / 60.0;

    GameRuntime ui_runtime {ui_desc};
    assert(ui_runtime.handle_command(make_start_campaign_command()) == GS1_STATUS_OK);
    const auto ui_site_id =
        gs1::GameRuntimeProjectionTestAccess::campaign(ui_runtime)->sites.front().site_id.value;
    assert(ui_runtime.handle_command(make_start_site_attempt_command(ui_site_id)) == GS1_STATUS_OK);
    auto& ui_site_run = gs1::GameRuntimeProjectionTestAccess::active_site_run(ui_runtime).value();
    drain_engine_commands(ui_runtime);

    auto ui_worker = gs1::site_world_access::worker_conditions(ui_site_run);
    ui_worker.hydration = 70.0f;
    gs1::site_world_access::set_worker_conditions(ui_site_run, ui_worker);

    Gs1UiAction transfer_action {};
    transfer_action.type = GS1_UI_ACTION_TRANSFER_INVENTORY_ITEM;
    transfer_action.arg0 = pack_inventory_transfer_arg(
        GS1_INVENTORY_CONTAINER_CAMP_STORAGE,
        0U,
        GS1_INVENTORY_CONTAINER_WORKER_PACK,
        3U,
        2U);
    auto transfer_event = make_ui_action_event(transfer_action);

    Gs1Phase1Result transfer_result {};
    assert(ui_runtime.submit_host_events(&transfer_event, 1U) == GS1_STATUS_OK);
    run_phase1(ui_runtime, 0.0, transfer_result);
    assert(transfer_result.processed_host_event_count == 1U);
    assert(ui_site_run.inventory.camp_storage_slots[0].occupied);
    assert(ui_site_run.inventory.camp_storage_slots[0].item_id.value == gs1::k_item_wind_reed_seed_bundle);
    assert(ui_site_run.inventory.camp_storage_slots[0].item_quantity == 6U);
    assert(ui_site_run.inventory.worker_pack_slots[3].occupied);
    assert(ui_site_run.inventory.worker_pack_slots[3].item_id.value == gs1::k_item_wind_reed_seed_bundle);
    assert(ui_site_run.inventory.worker_pack_slots[3].item_quantity == 2U);
    const auto transfer_commands = drain_engine_commands(ui_runtime);
    const auto transfer_inventory_commands = collect_inventory_slot_commands(transfer_commands);
    assert(transfer_inventory_commands.size() == 2U);
    assert(find_inventory_slot_command(transfer_commands, GS1_INVENTORY_CONTAINER_CAMP_STORAGE, 0U) != nullptr);
    assert(find_inventory_slot_command(transfer_commands, GS1_INVENTORY_CONTAINER_WORKER_PACK, 3U) != nullptr);

    Gs1UiAction use_action {};
    use_action.type = GS1_UI_ACTION_USE_INVENTORY_ITEM;
    use_action.target_id = gs1::k_item_water_container;
    use_action.arg0 = pack_inventory_use_arg(
        GS1_INVENTORY_CONTAINER_WORKER_PACK,
        0U,
        1U);
    auto use_event = make_ui_action_event(use_action);

    Gs1Phase1Result use_result {};
    assert(ui_runtime.submit_host_events(&use_event, 1U) == GS1_STATUS_OK);
    run_phase1(ui_runtime, 0.0, use_result);
    assert(use_result.processed_host_event_count == 1U);
    assert(ui_site_run.inventory.worker_pack_slots[0].occupied);
    assert(ui_site_run.inventory.worker_pack_slots[0].item_quantity == 1U);
    assert(gs1::site_world_access::worker_conditions(ui_site_run).hydration == 82.0f);
    const auto use_commands = drain_engine_commands(ui_runtime);
    const auto use_inventory_commands = collect_inventory_slot_commands(use_commands);
    assert(use_inventory_commands.size() == 1U);
    assert(find_inventory_slot_command(use_commands, GS1_INVENTORY_CONTAINER_WORKER_PACK, 0U) != nullptr);
    assert(!collect_commands_of_type(use_commands, GS1_ENGINE_COMMAND_SITE_WORKER_UPDATE).empty());
    assert(!collect_commands_of_type(use_commands, GS1_ENGINE_COMMAND_HUD_STATE).empty());

    Gs1RuntimeCreateDesc action_desc {};
    action_desc.struct_size = sizeof(Gs1RuntimeCreateDesc);
    action_desc.api_version = gs1::k_api_version;
    action_desc.fixed_step_seconds = 60.0;

    GameRuntime action_runtime {action_desc};
    assert(action_runtime.handle_command(make_start_campaign_command()) == GS1_STATUS_OK);
    const auto action_site_id =
        gs1::GameRuntimeProjectionTestAccess::campaign(action_runtime)->sites.front().site_id.value;
    assert(action_runtime.handle_command(make_start_site_attempt_command(action_site_id)) == GS1_STATUS_OK);
    assert(gs1::GameRuntimeProjectionTestAccess::active_site_run(action_runtime).has_value());
    drain_engine_commands(action_runtime);
    auto& action_site_run =
        gs1::GameRuntimeProjectionTestAccess::active_site_run(action_runtime).value();
    action_site_run.inventory.worker_pack_slots[3].occupied = true;
    action_site_run.inventory.worker_pack_slots[3].item_id = gs1::ItemId {gs1::k_item_wind_reed_seed_bundle};
    action_site_run.inventory.worker_pack_slots[3].item_quantity = 2U;
    action_site_run.inventory.worker_pack_slots[3].item_condition = 1.0f;
    action_site_run.inventory.worker_pack_slots[3].item_freshness = 1.0f;

    const TileCoord action_target {5, 4};
    auto action_event = make_site_action_request_event(
        GS1_SITE_ACTION_PLANT,
        action_target,
        0U,
        gs1::k_item_wind_reed_seed_bundle);
    assert(action_runtime.submit_host_events(&action_event, 1U) == GS1_STATUS_OK);
    gs1::SiteWorld::TileEcologyData action_ecology {};
    for (int step = 0; step < 10; ++step)
    {
        run_phase1(action_runtime, 60.0);
        action_ecology = gs1::site_world_access::tile_ecology(action_site_run, action_target);
        if (action_ecology.plant_id.value == gs1::k_plant_wind_reed)
        {
            break;
        }
    }

    assert(action_ecology.plant_id.value == gs1::k_plant_wind_reed);
    assert(action_ecology.plant_density >= 0.2f);
    assert(action_site_run.inventory.worker_pack_slots[3].item_quantity == 1U);

    run_phase1(action_runtime, 0.0);
    const auto action_commands = drain_engine_commands(action_runtime);
    const auto action_tile_commands =
        collect_commands_of_type(action_commands, GS1_ENGINE_COMMAND_SITE_TILE_UPSERT);
    const auto projected_action_tile = std::find_if(
        action_tile_commands.begin(),
        action_tile_commands.end(),
        [&](const Gs1EngineCommand* command) {
            const auto& payload = command->payload_as<Gs1EngineCommandSiteTileData>();
            return payload.x == static_cast<std::uint32_t>(action_target.x) &&
                payload.y == static_cast<std::uint32_t>(action_target.y) &&
                payload.plant_type_id == gs1::k_plant_wind_reed &&
                payload.plant_density >= 0.2f;
        });
    assert(projected_action_tile != action_tile_commands.end());

    return 0;
}
