#include "runtime/game_runtime.h"

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
    std::uint32_t primary_subject_id)
{
    Gs1HostEvent event {};
    event.type = GS1_HOST_EVENT_SITE_ACTION_REQUEST;
    event.payload.site_action_request.action_kind = action_kind;
    event.payload.site_action_request.flags = GS1_SITE_ACTION_REQUEST_FLAG_HAS_PRIMARY_SUBJECT;
    event.payload.site_action_request.quantity = 1U;
    event.payload.site_action_request.target_tile_x = target_tile.x;
    event.payload.site_action_request.target_tile_y = target_tile.y;
    event.payload.site_action_request.primary_subject_id = primary_subject_id;
    return event;
}

void run_phase1(GameRuntime& runtime, double real_delta_seconds)
{
    Gs1Phase1Request request {};
    request.struct_size = sizeof(Gs1Phase1Request);
    request.real_delta_seconds = real_delta_seconds;

    Gs1Phase1Result result {};
    assert(runtime.run_phase1(request, result) == GS1_STATUS_OK);
}

std::vector<Gs1EngineCommand> flush_tile_delta_for(
    GameRuntime& runtime,
    TileCoord coord,
    float sand_burial)
{
    auto& site_run = gs1::GameRuntimeProjectionTestAccess::active_site_run(runtime).value();
    const auto index = site_run.tile_grid.to_index(coord);
    site_run.tile_grid.tile_sand_burial[index] = sand_burial;
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
    assert(bootstrap_site_run.inventory.worker_pack_slots.size() == 8U);
    assert(bootstrap_site_run.inventory.worker_pack_slots[0].occupied);
    assert(bootstrap_site_run.inventory.worker_pack_slots[0].item_id.value == 1U);
    assert(bootstrap_site_run.task_board.visible_tasks.size() == 1U);
    assert(bootstrap_site_run.economy.money == 45);
    assert(bootstrap_site_run.economy.available_phone_listings.size() == 4U);

    const auto bootstrap_commands = drain_engine_commands(runtime);
    assert(!collect_commands_of_type(bootstrap_commands, GS1_ENGINE_COMMAND_SITE_INVENTORY_SLOT_UPSERT).empty());
    assert(!collect_commands_of_type(bootstrap_commands, GS1_ENGINE_COMMAND_SITE_TASK_UPSERT).empty());
    assert(!collect_commands_of_type(bootstrap_commands, GS1_ENGINE_COMMAND_SITE_PHONE_LISTING_UPSERT).empty());

    GameCommand accept_task {};
    accept_task.type = GameCommandType::TaskAcceptRequested;
    accept_task.set_payload(TaskAcceptRequestedCommand {1U});
    assert(runtime.handle_command(accept_task) == GS1_STATUS_OK);
    assert(bootstrap_site_run.task_board.accepted_task_ids.size() == 1U);

    bootstrap_site_run.worker.player_hydration = 80.0f;
    GameCommand use_item {};
    use_item.type = GameCommandType::InventoryItemUseRequested;
    use_item.set_payload(InventoryItemUseRequestedCommand {
        1U,
        1U,
        GS1_INVENTORY_CONTAINER_WORKER_PACK,
        0U});
    assert(runtime.handle_command(use_item) == GS1_STATUS_OK);
    assert(bootstrap_site_run.inventory.worker_pack_slots[0].item_quantity == 1U);
    assert(bootstrap_site_run.worker.player_hydration > 80.0f);

    GameCommand buy_listing {};
    buy_listing.type = GameCommandType::PhoneListingPurchaseRequested;
    buy_listing.set_payload(PhoneListingPurchaseRequestedCommand {1U, 1U, 0U});
    assert(runtime.handle_command(buy_listing) == GS1_STATUS_OK);
    assert(bootstrap_site_run.economy.money == 40);
    assert(bootstrap_site_run.economy.available_phone_listings[0].quantity == 4U);

    gs1::GameRuntimeProjectionTestAccess::flush_projection(runtime);
    drain_engine_commands(runtime);

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
    const auto repeated_index = site_run.tile_grid.to_index(repeated_coord);
    site_run.tile_grid.tile_sand_burial[repeated_index] = 0.5f;
    gs1::GameRuntimeProjectionTestAccess::mark_tile_dirty(runtime, repeated_coord);
    gs1::GameRuntimeProjectionTestAccess::mark_tile_dirty(runtime, repeated_coord);
    site_run.tile_grid.tile_sand_burial[site_run.tile_grid.to_index(TileCoord {1, 1})] = 0.75f;
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

    site_run.tile_grid.tile_sand_burial[site_run.tile_grid.to_index(TileCoord {0, 0})] = 0.9f;
    gs1::GameRuntimeProjectionTestAccess::mark_all_tiles_dirty(runtime);
    gs1::GameRuntimeProjectionTestAccess::flush_projection(runtime);
    const auto fallback_commands = drain_engine_commands(runtime);
    const auto fallback_tiles = collect_commands_of_type(fallback_commands, GS1_ENGINE_COMMAND_SITE_TILE_UPSERT);
    assert(!fallback_tiles.empty());
    assert(fallback_tiles.size() == site_run.tile_grid.tile_count());

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

    const TileCoord action_target {5, 4};
    auto action_event = make_site_action_request_event(GS1_SITE_ACTION_PLANT, action_target, 1U);
    assert(action_runtime.submit_host_events(&action_event, 1U) == GS1_STATUS_OK);
    run_phase1(action_runtime, 60.0);

    auto& action_site_run =
        gs1::GameRuntimeProjectionTestAccess::active_site_run(action_runtime).value();
    const auto action_tile_index = action_site_run.tile_grid.to_index(action_target);
    assert(action_site_run.tile_grid.ground_cover_type_ids[action_tile_index] == 1U);
    assert(action_site_run.tile_grid.tile_plant_density[action_tile_index] >= 0.25f);

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
                payload.ground_cover_type_id == 1U &&
                payload.plant_density >= 0.25f;
        });
    assert(projected_action_tile != action_tile_commands.end());

    return 0;
}
