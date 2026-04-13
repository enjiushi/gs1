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
using gs1::SiteId;
using gs1::StartNewCampaignCommand;
using gs1::StartSiteAttemptCommand;
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

    return 0;
}
