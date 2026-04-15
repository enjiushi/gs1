#include "runtime/game_runtime.h"
#include "content/defs/plant_defs.h"
#include "site/site_world_access.h"
#include "site/site_world_components.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <optional>
#include <vector>

namespace
{
using gs1::CampaignState;
using gs1::GameCommand;
using gs1::GameCommandType;
using gs1::GameRuntime;
using gs1::InventoryItemUseRequestedCommand;
using gs1::SiteRunState;
using gs1::StartNewCampaignCommand;
using gs1::StartSiteAttemptCommand;
using gs1::TaskAcceptRequestedCommand;
using gs1::TaskRuntimeListKind;
using gs1::TileCoord;

constexpr float k_float_epsilon = 0.001f;

bool approx_equal(float lhs, float rhs, float epsilon = k_float_epsilon)
{
    return std::fabs(lhs - rhs) <= epsilon;
}
}  // namespace

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

    static Gs1AppState app_state(const GameRuntime& runtime)
    {
        return runtime.app_state_;
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

GameCommand make_accept_task_command(std::uint32_t task_instance_id)
{
    GameCommand command {};
    command.type = GameCommandType::TaskAcceptRequested;
    command.set_payload(TaskAcceptRequestedCommand {task_instance_id});
    return command;
}

GameCommand make_inventory_use_command(
    std::uint32_t item_id,
    std::uint32_t quantity,
    Gs1InventoryContainerKind container_kind,
    std::uint32_t slot_index)
{
    GameCommand command {};
    command.type = GameCommandType::InventoryItemUseRequested;
    command.set_payload(InventoryItemUseRequestedCommand {
        item_id,
        static_cast<std::uint16_t>(quantity),
        container_kind,
        static_cast<std::uint8_t>(slot_index)});
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

void run_phase1(GameRuntime& runtime, double real_delta_seconds, Gs1Phase1Result& out_result)
{
    Gs1Phase1Request request {};
    request.struct_size = sizeof(Gs1Phase1Request);
    request.real_delta_seconds = real_delta_seconds;

    out_result = {};
    assert(runtime.run_phase1(request, out_result) == GS1_STATUS_OK);
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

const Gs1EngineCommand* find_task_command(
    const std::vector<Gs1EngineCommand>& commands,
    std::uint32_t task_instance_id)
{
    for (const auto& command : commands)
    {
        if (command.type != GS1_ENGINE_COMMAND_SITE_TASK_UPSERT)
        {
            continue;
        }

        const auto& payload = command.payload_as<Gs1EngineCommandTaskData>();
        if (payload.task_instance_id == task_instance_id)
        {
            return &command;
        }
    }

    return nullptr;
}

const Gs1EngineCommand* find_tile_command(
    const std::vector<Gs1EngineCommand>& commands,
    TileCoord coord)
{
    for (const auto& command : commands)
    {
        if (command.type != GS1_ENGINE_COMMAND_SITE_TILE_UPSERT)
        {
            continue;
        }

        const auto& payload = command.payload_as<Gs1EngineCommandSiteTileData>();
        if (payload.x == static_cast<std::uint32_t>(coord.x) &&
            payload.y == static_cast<std::uint32_t>(coord.y))
        {
            return &command;
        }
    }

    return nullptr;
}

void bootstrap_site_one(GameRuntime& runtime)
{
    assert(runtime.handle_command(make_start_campaign_command()) == GS1_STATUS_OK);
    auto& campaign = gs1::GameRuntimeProjectionTestAccess::campaign(runtime);
    assert(campaign.has_value());
    assert(!campaign->sites.empty());

    const auto site_id = campaign->sites.front().site_id.value;
    assert(runtime.handle_command(make_start_site_attempt_command(site_id)) == GS1_STATUS_OK);
    assert(gs1::GameRuntimeProjectionTestAccess::active_site_run(runtime).has_value());
}

void set_worker_hydration(SiteRunState& site_run, float hydration)
{
    assert(site_run.site_world != nullptr);
    auto entity = site_run.site_world->ecs_world().entity(site_run.site_world->worker_entity_id());
    assert(entity.is_alive());

    auto& vitals = entity.get_mut<gs1::site_ecs::WorkerVitals>();
    vitals.hydration = hydration;
    entity.modified<gs1::site_ecs::WorkerVitals>();
}

void seed_plant_tile(SiteRunState& site_run, TileCoord coord, std::uint32_t plant_id, float density)
{
    assert(site_run.site_world != nullptr);
    auto entity = site_run.site_world->ecs_world().entity(site_run.site_world->tile_entity_id(coord));
    assert(entity.is_alive());

    auto& burial = entity.get_mut<gs1::site_ecs::TileSandBurial>();
    auto& plant = entity.get_mut<gs1::site_ecs::TilePlantSlot>();
    auto& ground_cover = entity.get_mut<gs1::site_ecs::TileGroundCoverSlot>();
    auto& plant_density = entity.get_mut<gs1::site_ecs::TilePlantDensity>();

    burial.value = 0.0f;
    plant.plant_id = gs1::PlantId {plant_id};
    ground_cover.ground_cover_type_id = 0U;
    plant_density.value = density;
    entity.add<gs1::site_ecs::TileOccupantTag>();
    entity.modified<gs1::site_ecs::TileSandBurial>();
    entity.modified<gs1::site_ecs::TilePlantSlot>();
    entity.modified<gs1::site_ecs::TileGroundCoverSlot>();
    entity.modified<gs1::site_ecs::TilePlantDensity>();
}

void inventory_item_use_updates_worker_and_projection()
{
    Gs1RuntimeCreateDesc create_desc {};
    create_desc.struct_size = sizeof(Gs1RuntimeCreateDesc);
    create_desc.api_version = gs1::k_api_version;
    create_desc.fixed_step_seconds = 1.0 / 60.0;

    GameRuntime runtime {create_desc};
    bootstrap_site_one(runtime);

    auto& site_run = gs1::GameRuntimeProjectionTestAccess::active_site_run(runtime).value();
    assert(site_run.inventory.worker_pack_slots[0].occupied);
    assert(site_run.inventory.worker_pack_slots[0].item_id.value == 1U);
    assert(site_run.inventory.worker_pack_slots[0].item_quantity == 2U);

    drain_engine_commands(runtime);

    set_worker_hydration(site_run, 80.0f);
    assert(approx_equal(gs1::site_world_access::worker_conditions(site_run).hydration, 80.0f));

    assert(runtime.handle_command(make_inventory_use_command(
               1U,
               1U,
               GS1_INVENTORY_CONTAINER_WORKER_PACK,
               0U)) == GS1_STATUS_OK);

    assert(site_run.inventory.worker_pack_slots[0].occupied);
    assert(site_run.inventory.worker_pack_slots[0].item_quantity == 1U);

    const auto worker_conditions = gs1::site_world_access::worker_conditions(site_run);
    assert(approx_equal(worker_conditions.hydration, 92.0f));

    gs1::GameRuntimeProjectionTestAccess::flush_projection(runtime);
    const auto commands = drain_engine_commands(runtime);

    const auto snapshot_commands =
        collect_commands_of_type(commands, GS1_ENGINE_COMMAND_BEGIN_SITE_SNAPSHOT);
    assert(snapshot_commands.size() == 1U);
    const auto worker_commands =
        collect_commands_of_type(commands, GS1_ENGINE_COMMAND_SITE_WORKER_UPDATE);
    const auto hud_commands = collect_commands_of_type(commands, GS1_ENGINE_COMMAND_HUD_STATE);
    assert(worker_commands.size() == 1U);
    assert(hud_commands.size() == 1U);

    const auto* slot_command =
        find_inventory_slot_command(commands, GS1_INVENTORY_CONTAINER_WORKER_PACK, 0U);
    assert(slot_command != nullptr);
    {
        const auto& payload = slot_command->payload_as<Gs1EngineCommandInventorySlotData>();
        assert(payload.item_id == 1U);
        assert(payload.quantity == 1U);
        assert(payload.flags == 1U);
    }

    {
        const auto& payload = worker_commands.front()->payload_as<Gs1EngineCommandWorkerData>();
        assert(approx_equal(payload.hydration_normalized, 0.92f));
        assert(approx_equal(payload.health_normalized, 1.0f));
    }

    {
        const auto& payload = hud_commands.front()->payload_as<Gs1EngineCommandHudStateData>();
        assert(approx_equal(payload.player_hydration, 92.0f));
        assert(payload.current_money == 45);
        assert(payload.active_task_count == 0U);
    }
}

void ecology_growth_completes_task_and_site_attempt()
{
    Gs1RuntimeCreateDesc create_desc {};
    create_desc.struct_size = sizeof(Gs1RuntimeCreateDesc);
    create_desc.api_version = gs1::k_api_version;
    create_desc.fixed_step_seconds = 60.0;

    GameRuntime runtime {create_desc};
    bootstrap_site_one(runtime);

    auto& site_run = gs1::GameRuntimeProjectionTestAccess::active_site_run(runtime).value();
    assert(site_run.counters.site_completion_tile_threshold == 10U);
    assert(site_run.task_board.visible_tasks.size() == 1U);
    assert(site_run.task_board.visible_tasks.front().target_amount == 10U);

    drain_engine_commands(runtime);

    assert(runtime.handle_command(make_accept_task_command(1U)) == GS1_STATUS_OK);
    assert(site_run.task_board.accepted_task_ids.size() == 1U);

    gs1::GameRuntimeProjectionTestAccess::flush_projection(runtime);
    drain_engine_commands(runtime);

    const std::vector<TileCoord> seeded_tiles {
        TileCoord {0, 0},
        TileCoord {1, 0},
        TileCoord {2, 0},
        TileCoord {3, 0},
        TileCoord {4, 0},
        TileCoord {0, 1},
        TileCoord {1, 1},
        TileCoord {2, 1},
        TileCoord {3, 1},
        TileCoord {4, 1}};
    for (const auto coord : seeded_tiles)
    {
        seed_plant_tile(site_run, coord, gs1::k_plant_wind_reed, 0.9f);
    }

    Gs1Phase1Result result {};
    run_phase1(runtime, 60.0, result);
    assert(result.fixed_steps_executed == 1U);

    assert(site_run.counters.fully_grown_tile_count == 10U);
    assert(site_run.run_status == gs1::SiteRunStatus::Completed);
    assert(gs1::GameRuntimeProjectionTestAccess::app_state(runtime) == GS1_APP_STATE_SITE_RESULT);

    auto& campaign = gs1::GameRuntimeProjectionTestAccess::campaign(runtime).value();
    assert(campaign.app_state == GS1_APP_STATE_SITE_RESULT);
    assert(campaign.sites[0].site_state == GS1_SITE_STATE_COMPLETED);
    assert(campaign.sites[1].site_state == GS1_SITE_STATE_AVAILABLE);
    assert(site_run.result_newly_revealed_site_count == 1U);
    assert(site_run.task_board.accepted_task_ids.empty());
    assert(site_run.task_board.completed_task_ids.size() == 1U);
    assert(site_run.task_board.visible_tasks.front().runtime_list_kind == TaskRuntimeListKind::Completed);
    assert(site_run.task_board.visible_tasks.front().current_progress_amount == 10U);

    const auto tile_state = gs1::site_world_access::tile_ecology(site_run, seeded_tiles.front());
    assert(approx_equal(tile_state.plant_density, 1.0f));
    assert(tile_state.plant_id.value == gs1::k_plant_wind_reed);
    assert(tile_state.ground_cover_type_id == 0U);

    const auto phase1_commands = drain_engine_commands(runtime);
    const auto* grown_tile_command = find_tile_command(phase1_commands, seeded_tiles.front());
    assert(grown_tile_command != nullptr);
    {
        const auto& payload = grown_tile_command->payload_as<Gs1EngineCommandSiteTileData>();
        assert(payload.plant_type_id == gs1::k_plant_wind_reed);
        assert(payload.ground_cover_type_id == 0U);
        assert(approx_equal(payload.plant_density, 1.0f));
    }

    const auto app_state_commands =
        collect_commands_of_type(phase1_commands, GS1_ENGINE_COMMAND_SET_APP_STATE);
    const auto site_result_commands =
        collect_commands_of_type(phase1_commands, GS1_ENGINE_COMMAND_SITE_RESULT_READY);
    assert(!app_state_commands.empty());
    assert(site_result_commands.size() == 1U);

    bool saw_site_result_state = false;
    for (const auto* command : app_state_commands)
    {
        if (command->payload_as<Gs1EngineCommandSetAppStateData>().app_state == GS1_APP_STATE_SITE_RESULT)
        {
            saw_site_result_state = true;
            break;
        }
    }
    assert(saw_site_result_state);

    {
        const auto& payload = site_result_commands.front()->payload_as<Gs1EngineCommandSiteResultData>();
        assert(payload.site_id == 1U);
        assert(payload.result == GS1_SITE_ATTEMPT_RESULT_COMPLETED);
        assert(payload.newly_revealed_site_count == 1U);
    }

    gs1::GameRuntimeProjectionTestAccess::flush_projection(runtime);
    const auto follow_up_commands = drain_engine_commands(runtime);

    const auto* task_command = find_task_command(follow_up_commands, 1U);
    assert(task_command != nullptr);
    {
        const auto& payload = task_command->payload_as<Gs1EngineCommandTaskData>();
        assert(payload.current_progress == 10U);
        assert(payload.target_progress == 10U);
        assert(payload.list_kind == GS1_TASK_PRESENTATION_LIST_COMPLETED);
    }

    const auto follow_up_hud_commands =
        collect_commands_of_type(follow_up_commands, GS1_ENGINE_COMMAND_HUD_STATE);
    assert(follow_up_hud_commands.size() == 1U);
    {
        const auto& payload = follow_up_hud_commands.front()->payload_as<Gs1EngineCommandHudStateData>();
        assert(payload.active_task_count == 0U);
        assert(approx_equal(payload.site_completion_normalized, 1.0f));
    }
}
}  // namespace

int main()
{
    inventory_item_use_updates_worker_and_projection();
    ecology_growth_completes_task_and_site_attempt();
    return 0;
}
