#include "runtime/game_runtime.h"
#include "content/defs/faction_defs.h"
#include "content/defs/item_defs.h"
#include "content/defs/task_defs.h"
#include "site/site_projection_update_flags.h"
#include "content/defs/plant_defs.h"
#include "site/task_board_state.h"
#include "site/site_world_access.h"
#include "site/site_world_components.h"
#include "support/currency.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <optional>
#include <vector>

namespace
{
using gs1::CampaignState;
using gs1::GameMessage;
using gs1::GameMessageType;
using gs1::GameRuntime;
using gs1::InventoryItemUseRequestedMessage;
using gs1::SiteRunState;
using gs1::StartNewCampaignMessage;
using gs1::StartSiteAttemptMessage;
using gs1::TaskAcceptRequestedMessage;
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

    static void mark_tile_dirty(GameRuntime& runtime, TileCoord coord)
    {
        runtime.mark_site_tile_projection_dirty(coord);
    }

    static void mark_projection_dirty(GameRuntime& runtime, std::uint64_t dirty_flags)
    {
        runtime.mark_site_projection_update_dirty(dirty_flags);
    }
};
}  // namespace gs1

namespace
{
GameMessage make_start_campaign_message()
{
    GameMessage message {};
    message.type = GameMessageType::StartNewCampaign;
    message.set_payload(StartNewCampaignMessage {42ULL, 30U});
    return message;
}

GameMessage make_start_site_attempt_message(std::uint32_t site_id)
{
    GameMessage message {};
    message.type = GameMessageType::StartSiteAttempt;
    message.set_payload(StartSiteAttemptMessage {site_id});
    return message;
}

GameMessage make_accept_task_message(std::uint32_t task_instance_id)
{
    GameMessage message {};
    message.type = GameMessageType::TaskAcceptRequested;
    message.set_payload(TaskAcceptRequestedMessage {task_instance_id});
    return message;
}

GameMessage make_inventory_use_message(
    std::uint32_t item_id,
    std::uint32_t quantity,
    std::uint32_t storage_id,
    std::uint32_t slot_index)
{
    GameMessage message {};
    message.type = GameMessageType::InventoryItemUseRequested;
    message.set_payload(InventoryItemUseRequestedMessage {
        item_id,
        storage_id,
        static_cast<std::uint16_t>(quantity),
        static_cast<std::uint16_t>(slot_index)});
    return message;
}

std::vector<Gs1EngineMessage> drain_engine_messages(GameRuntime& runtime)
{
    std::vector<Gs1EngineMessage> messages {};
    Gs1EngineMessage message {};
    while (runtime.pop_engine_message(message) == GS1_STATUS_OK)
    {
        messages.push_back(message);
    }
    return messages;
}

void run_phase1(GameRuntime& runtime, double real_delta_seconds, Gs1Phase1Result& out_result)
{
    Gs1Phase1Request request {};
    request.struct_size = sizeof(Gs1Phase1Request);
    request.real_delta_seconds = real_delta_seconds;

    out_result = {};
    assert(runtime.run_phase1(request, out_result) == GS1_STATUS_OK);
}

std::vector<const Gs1EngineMessage*> collect_messages_of_type(
    const std::vector<Gs1EngineMessage>& messages,
    Gs1EngineMessageType type)
{
    std::vector<const Gs1EngineMessage*> matches {};
    for (const auto& message : messages)
    {
        if (message.type == type)
        {
            matches.push_back(&message);
        }
    }
    return matches;
}

const Gs1EngineMessage* find_inventory_slot_message(
    const std::vector<Gs1EngineMessage>& messages,
    std::uint32_t storage_id,
    std::uint16_t slot_index)
{
    for (const auto& message : messages)
    {
        if (message.type != GS1_ENGINE_MESSAGE_SITE_INVENTORY_SLOT_UPSERT)
        {
            continue;
        }

        const auto& payload = message.payload_as<Gs1EngineMessageInventorySlotData>();
        if (payload.storage_id == storage_id && payload.slot_index == slot_index)
        {
            return &message;
        }
    }

    return nullptr;
}

const Gs1EngineMessage* find_task_message(
    const std::vector<Gs1EngineMessage>& messages,
    std::uint32_t task_instance_id)
{
    for (const auto& message : messages)
    {
        if (message.type != GS1_ENGINE_MESSAGE_SITE_TASK_UPSERT)
        {
            continue;
        }

        const auto& payload = message.payload_as<Gs1EngineMessageTaskData>();
        if (payload.task_instance_id == task_instance_id)
        {
            return &message;
        }
    }

    return nullptr;
}

const Gs1EngineMessage* find_tile_message(
    const std::vector<Gs1EngineMessage>& messages,
    TileCoord coord)
{
    for (const auto& message : messages)
    {
        if (message.type != GS1_ENGINE_MESSAGE_SITE_TILE_UPSERT)
        {
            continue;
        }

        const auto& payload = message.payload_as<Gs1EngineMessageSiteTileData>();
        if (payload.x == static_cast<std::uint32_t>(coord.x) &&
            payload.y == static_cast<std::uint32_t>(coord.y))
        {
            return &message;
        }
    }

    return nullptr;
}

void bootstrap_site_one(GameRuntime& runtime)
{
    assert(runtime.handle_message(make_start_campaign_message()) == GS1_STATUS_OK);
    auto& campaign = gs1::GameRuntimeProjectionTestAccess::campaign(runtime);
    assert(campaign.has_value());
    assert(!campaign->sites.empty());

    const auto site_id = campaign->sites.front().site_id.value;
    assert(runtime.handle_message(make_start_site_attempt_message(site_id)) == GS1_STATUS_OK);
    assert(gs1::GameRuntimeProjectionTestAccess::active_site_run(runtime).has_value());
}

void seed_runtime_test_task(GameRuntime& runtime, std::uint32_t site_completion_target)
{
    auto& site_run = gs1::GameRuntimeProjectionTestAccess::active_site_run(runtime).value();
    site_run.task_board.visible_tasks.clear();
    site_run.task_board.accepted_task_ids.clear();
    site_run.task_board.completed_task_ids.clear();
    site_run.task_board.claimed_task_ids.clear();
    site_run.task_board.task_pool_size = 1U;
    site_run.task_board.visible_tasks.push_back(gs1::TaskInstanceState {
        gs1::TaskInstanceId {1U},
        gs1::TaskTemplateId {gs1::k_task_template_site1_restore_patch},
        gs1::FactionId {gs1::k_faction_forestry_bureau},
        1U,
        site_completion_target,
        0U,
        0U});
    gs1::GameRuntimeProjectionTestAccess::mark_projection_dirty(
        runtime,
        gs1::SITE_PROJECTION_UPDATE_TASKS |
            gs1::SITE_PROJECTION_UPDATE_PHONE |
            gs1::SITE_PROJECTION_UPDATE_HUD);
    gs1::GameRuntimeProjectionTestAccess::flush_projection(runtime);
}

void set_worker_hydration(SiteRunState& site_run, float hydration)
{
    auto vitals = gs1::site_world_access::worker_conditions(site_run);
    vitals.hydration = hydration;
    gs1::site_world_access::set_worker_conditions(site_run, vitals);
}

void seed_plant_tile(SiteRunState& site_run, TileCoord coord, std::uint32_t plant_id, float density)
{
    auto ecology = gs1::site_world_access::tile_ecology(site_run, coord);
    ecology.sand_burial = 0.0f;
    ecology.plant_id = gs1::PlantId {plant_id};
    ecology.ground_cover_type_id = 0U;
    ecology.plant_density = (density > 0.0f && density <= 1.0f) ? density * 100.0f : density;
    gs1::site_world_access::set_tile_ecology(site_run, coord, ecology);
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
    assert(!site_run.inventory.worker_pack_slots[0].occupied);

    drain_engine_messages(runtime);

    site_run.inventory.worker_pack_slots[4].occupied = true;
    site_run.inventory.worker_pack_slots[4].item_id = gs1::ItemId {gs1::k_item_medicine_pack};
    site_run.inventory.worker_pack_slots[4].item_quantity = 1U;
    site_run.inventory.worker_pack_slots[4].item_condition = 1.0f;
    site_run.inventory.worker_pack_slots[4].item_freshness = 1.0f;

    auto worker_conditions = gs1::site_world_access::worker_conditions(site_run);
    worker_conditions.health = 70.0f;
    gs1::site_world_access::set_worker_conditions(site_run, worker_conditions);
    assert(approx_equal(gs1::site_world_access::worker_conditions(site_run).health, 70.0f));

    assert(runtime.handle_message(make_inventory_use_message(
               gs1::k_item_medicine_pack,
               1U,
               site_run.inventory.worker_pack_storage_id,
               4U)) == GS1_STATUS_OK);

    assert(!site_run.inventory.worker_pack_slots[4].occupied);

    worker_conditions = gs1::site_world_access::worker_conditions(site_run);
    assert(approx_equal(worker_conditions.health, 88.0f));

    gs1::GameRuntimeProjectionTestAccess::flush_projection(runtime);
    const auto messages = drain_engine_messages(runtime);

    const auto snapshot_messages =
        collect_messages_of_type(messages, GS1_ENGINE_MESSAGE_BEGIN_SITE_SNAPSHOT);
    assert(snapshot_messages.size() == 1U);
    const auto worker_messages =
        collect_messages_of_type(messages, GS1_ENGINE_MESSAGE_SITE_WORKER_UPDATE);
    const auto hud_messages = collect_messages_of_type(messages, GS1_ENGINE_MESSAGE_HUD_STATE);
    assert(worker_messages.size() == 1U);
    assert(hud_messages.size() == 1U);

    const auto* slot_message =
        find_inventory_slot_message(messages, site_run.inventory.worker_pack_storage_id, 4U);
    assert(slot_message != nullptr);
    {
        const auto& payload = slot_message->payload_as<Gs1EngineMessageInventorySlotData>();
        assert(payload.item_id == 0U);
        assert(payload.quantity == 0U);
        assert(payload.flags == 0U);
    }

    {
        const auto& payload = worker_messages.front()->payload_as<Gs1EngineMessageWorkerData>();
        assert(approx_equal(payload.health_normalized, 0.88f));
        assert(approx_equal(payload.hydration_normalized, 1.0f));
    }

    {
        const auto& payload = hud_messages.front()->payload_as<Gs1EngineMessageHudStateData>();
        assert(approx_equal(payload.player_health, 88.0f));
        assert(approx_equal(payload.player_morale, 100.0f));
        assert(approx_equal(payload.current_money, 45.0f));
        assert(payload.active_task_count == 1U);
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
    assert(site_run.task_board.accepted_task_ids.size() == 1U);
    site_run.counters.site_completion_tile_threshold = 1U;
    seed_runtime_test_task(runtime, 10U);
    assert(site_run.task_board.visible_tasks.front().target_amount == 10U);

    drain_engine_messages(runtime);

    assert(runtime.handle_message(make_accept_task_message(1U)) == GS1_STATUS_OK);
    assert(site_run.task_board.accepted_task_ids.size() == 1U);

    GameMessage restoration_message {};
    restoration_message.type = GameMessageType::RestorationProgressChanged;
    restoration_message.set_payload(gs1::RestorationProgressChangedMessage {10U, 1U, 1.0f});
    assert(runtime.handle_message(restoration_message) == GS1_STATUS_OK);

    assert(site_run.task_board.accepted_task_ids.empty());
    assert(site_run.task_board.completed_task_ids.size() == 1U);
    assert(site_run.task_board.visible_tasks.front().runtime_list_kind == TaskRuntimeListKind::Completed);
    assert(site_run.task_board.visible_tasks.front().current_progress_amount == 10U);

    gs1::GameRuntimeProjectionTestAccess::flush_projection(runtime);
    const auto phase1_messages = drain_engine_messages(runtime);
    const auto* completed_task_message = find_task_message(phase1_messages, 1U);
    assert(completed_task_message != nullptr);
    {
        const auto& payload = completed_task_message->payload_as<Gs1EngineMessageTaskData>();
        assert(payload.current_progress == 10U);
        assert(payload.target_progress == 10U);
        assert(payload.list_kind == GS1_TASK_PRESENTATION_LIST_COMPLETED);
    }

    gs1::GameRuntimeProjectionTestAccess::flush_projection(runtime);
    (void)drain_engine_messages(runtime);
}

void site_weather_changes_emit_hud_wind_warning_codes()
{
    constexpr std::uint16_t k_hud_warning_wind_watch = 1U;
    constexpr std::uint16_t k_hud_warning_wind_exposure = 2U;

    Gs1RuntimeCreateDesc create_desc {};
    create_desc.struct_size = sizeof(Gs1RuntimeCreateDesc);
    create_desc.api_version = gs1::k_api_version;
    create_desc.fixed_step_seconds = 1.0 / 60.0;

    GameRuntime runtime {create_desc};
    bootstrap_site_one(runtime);

    auto& site_run = gs1::GameRuntimeProjectionTestAccess::active_site_run(runtime).value();
    drain_engine_messages(runtime);

    const auto worker_position = gs1::site_world_access::worker_position(site_run);
    gs1::site_world_access::set_tile_local_weather(
        site_run,
        worker_position.tile_coord,
        gs1::SiteWorld::TileLocalWeatherData {8.0f, 36.0f, 6.0f});
    gs1::GameRuntimeProjectionTestAccess::mark_projection_dirty(runtime, gs1::SITE_PROJECTION_UPDATE_WEATHER);
    gs1::GameRuntimeProjectionTestAccess::flush_projection(runtime);

    const auto weather_messages = drain_engine_messages(runtime);
    const auto weather_hud_messages =
        collect_messages_of_type(weather_messages, GS1_ENGINE_MESSAGE_HUD_STATE);
    assert(weather_hud_messages.size() == 1U);
    {
        const auto& payload = weather_hud_messages.front()->payload_as<Gs1EngineMessageHudStateData>();
        assert(payload.warning_code == k_hud_warning_wind_exposure);
    }

    auto worker_conditions = gs1::site_world_access::worker_conditions(site_run);
    worker_conditions.is_sheltered = true;
    gs1::site_world_access::set_worker_conditions(site_run, worker_conditions);
    gs1::GameRuntimeProjectionTestAccess::mark_projection_dirty(runtime, gs1::SITE_PROJECTION_UPDATE_WORKER);
    gs1::GameRuntimeProjectionTestAccess::flush_projection(runtime);

    const auto sheltered_messages = drain_engine_messages(runtime);
    const auto sheltered_hud_messages =
        collect_messages_of_type(sheltered_messages, GS1_ENGINE_MESSAGE_HUD_STATE);
    assert(sheltered_hud_messages.size() == 1U);
    {
        const auto& payload = sheltered_hud_messages.front()->payload_as<Gs1EngineMessageHudStateData>();
        assert(payload.warning_code == k_hud_warning_wind_watch);
    }
}

}  // namespace

int main()
{
    inventory_item_use_updates_worker_and_projection();
    ecology_growth_completes_task_and_site_attempt();
    site_weather_changes_emit_hud_wind_warning_codes();
    return 0;
}
