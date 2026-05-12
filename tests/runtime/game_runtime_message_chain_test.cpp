#include "runtime/game_runtime.h"
#include "content/defs/faction_defs.h"
#include "content/defs/item_defs.h"
#include "content/defs/task_defs.h"
#include "site/inventory_storage.h"
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
using gs1::PhoneListingPurchaseRequestedMessage;
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
    static GamePresentationRuntimeContext presentation_context(GameRuntime& runtime)
    {
        return GamePresentationRuntimeContext {
            runtime.state_.app_state,
            runtime.state_.campaign,
            runtime.state_.active_site_run,
            runtime.state_.site_protection_presentation,
            runtime.state_.ui_presentation,
            runtime.state_.presentation_runtime,
            runtime.state_.message_queue,
            runtime.state_.runtime_messages,
            runtime.state_.fixed_step_seconds};
    }

    static std::optional<CampaignState>& campaign(GameRuntime& runtime)
    {
        return runtime.state_.campaign;
    }

    static std::optional<SiteRunState>& active_site_run(GameRuntime& runtime)
    {
        return runtime.state_.active_site_run;
    }

    static Gs1AppState app_state(const GameRuntime& runtime)
    {
        return runtime.state_.app_state;
    }

    static void flush_projection(GameRuntime& runtime)
    {
        auto context = presentation_context(runtime);
        runtime.presentation_.flush_site_presentation_if_dirty(context);
    }

    static void mark_tile_dirty(GameRuntime& runtime, TileCoord coord)
    {
        auto context = presentation_context(runtime);
        runtime.presentation_.mark_site_tile_projection_dirty(context, coord);
    }

    static void mark_projection_dirty(GameRuntime& runtime, std::uint64_t dirty_flags)
    {
        auto context = presentation_context(runtime);
        runtime.presentation_.mark_site_projection_update_dirty(context, dirty_flags);
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

Gs1HostMessage make_ui_action_event(const Gs1UiAction& action)
{
    Gs1HostMessage event {};
    event.type = GS1_HOST_EVENT_UI_ACTION;
    event.payload.ui_action.action = action;
    return event;
}

Gs1HostMessage make_site_scene_ready_event()
{
    Gs1HostMessage event {};
    event.type = GS1_HOST_EVENT_SITE_SCENE_READY;
    return event;
}

std::vector<Gs1RuntimeMessage> drain_runtime_messages(GameRuntime& runtime)
{
    std::vector<Gs1RuntimeMessage> messages {};
    Gs1RuntimeMessage message {};
    while (runtime.pop_runtime_message(message) == GS1_STATUS_OK)
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

void run_phase2(GameRuntime& runtime, Gs1Phase2Result& out_result)
{
    Gs1Phase2Request request {};
    request.struct_size = sizeof(Gs1Phase2Request);

    out_result = {};
    assert(runtime.run_phase2(request, out_result) == GS1_STATUS_OK);
}

std::vector<const Gs1RuntimeMessage*> collect_messages_of_type(
    const std::vector<Gs1RuntimeMessage>& messages,
    Gs1EngineMessageType type)
{
    std::vector<const Gs1RuntimeMessage*> matches {};
    for (const auto& message : messages)
    {
        if (message.type == type)
        {
            matches.push_back(&message);
        }
    }
    return matches;
}

void append_messages(std::vector<Gs1RuntimeMessage>& destination, std::vector<Gs1RuntimeMessage>&& source)
{
    destination.insert(destination.end(), source.begin(), source.end());
}

void assert_singleton_projection_messages_do_not_repeat(const std::vector<Gs1RuntimeMessage>& messages)
{
    constexpr Gs1EngineMessageType k_singleton_projection_types[] = {
        GS1_ENGINE_MESSAGE_HUD_STATE,
        GS1_ENGINE_MESSAGE_CAMPAIGN_RESOURCES,
        GS1_ENGINE_MESSAGE_SITE_ACTION_UPDATE,
        GS1_ENGINE_MESSAGE_SITE_WORKER_UPDATE,
        GS1_ENGINE_MESSAGE_SITE_PHONE_PANEL_STATE,
        GS1_ENGINE_MESSAGE_SITE_PROTECTION_OVERLAY_STATE,
        GS1_ENGINE_MESSAGE_SITE_RESULT_READY};

    for (const auto type : k_singleton_projection_types)
    {
        const auto matches = collect_messages_of_type(messages, type);
        assert(matches.size() <= 1U);
    }
}

const Gs1RuntimeMessage* find_inventory_slot_message(
    const std::vector<Gs1RuntimeMessage>& messages,
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

const Gs1RuntimeMessage* find_task_message(
    const std::vector<Gs1RuntimeMessage>& messages,
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

const Gs1RuntimeMessage* find_one_shot_cue_message(
    const std::vector<Gs1RuntimeMessage>& messages,
    Gs1OneShotCueKind cue_kind)
{
    for (const auto& message : messages)
    {
        if (message.type != GS1_ENGINE_MESSAGE_PLAY_ONE_SHOT_CUE)
        {
            continue;
        }

        const auto& payload = message.payload_as<Gs1EngineMessageOneShotCueData>();
        if (payload.cue_kind == cue_kind)
        {
            return &message;
        }
    }

    return nullptr;
}

const Gs1RuntimeMessage* find_inventory_view_message(
    const std::vector<Gs1RuntimeMessage>& messages,
    std::uint32_t storage_id,
    Gs1InventoryViewEventKind event_kind)
{
    for (const auto& message : messages)
    {
        if (message.type != GS1_ENGINE_MESSAGE_SITE_INVENTORY_VIEW_STATE)
        {
            continue;
        }

        const auto& payload = message.payload_as<Gs1EngineMessageInventoryViewData>();
        if (payload.storage_id == storage_id && payload.event_kind == event_kind)
        {
            return &message;
        }
    }

    return nullptr;
}

std::uint32_t starter_storage_id(gs1::SiteRunState& site_run)
{
    return gs1::inventory_storage::storage_id_for_container(
        site_run,
        gs1::inventory_storage::starter_storage_container(site_run));
}

const Gs1RuntimeMessage* find_tile_message(
    const std::vector<Gs1RuntimeMessage>& messages,
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

    const auto ready_event = make_site_scene_ready_event();
    assert(runtime.submit_host_messages(&ready_event, 1U) == GS1_STATUS_OK);
    Gs1Phase2Result ready_result {};
    run_phase2(runtime, ready_result);
    assert(gs1::GameRuntimeProjectionTestAccess::app_state(runtime) == GS1_APP_STATE_SITE_ACTIVE);
}

void site_loading_waits_for_scene_ready_before_bootstrap_and_fixed_steps()
{
    Gs1RuntimeCreateDesc create_desc {};
    create_desc.struct_size = sizeof(Gs1RuntimeCreateDesc);
    create_desc.api_version = gs1::k_api_version;
    create_desc.fixed_step_seconds = 60.0;
    create_desc.adapter_config_json_utf8 = nullptr;

    GameRuntime runtime {create_desc};
    assert(runtime.handle_message(make_start_campaign_message()) == GS1_STATUS_OK);
    auto& campaign = gs1::GameRuntimeProjectionTestAccess::campaign(runtime);
    assert(campaign.has_value());
    assert(!campaign->sites.empty());
    drain_runtime_messages(runtime);

    const auto site_id = campaign->sites.front().site_id.value;
    assert(runtime.handle_message(make_start_site_attempt_message(site_id)) == GS1_STATUS_OK);
    assert(gs1::GameRuntimeProjectionTestAccess::active_site_run(runtime).has_value());
    assert(gs1::GameRuntimeProjectionTestAccess::app_state(runtime) == GS1_APP_STATE_SITE_LOADING);

    const auto loading_messages = drain_runtime_messages(runtime);
    const auto loading_app_state_messages =
        collect_messages_of_type(loading_messages, GS1_ENGINE_MESSAGE_SET_APP_STATE);
    assert(loading_app_state_messages.size() == 1U);
    assert(
        loading_app_state_messages.front()->payload_as<Gs1EngineMessageSetAppStateData>().app_state ==
        GS1_APP_STATE_SITE_LOADING);
    assert(collect_messages_of_type(loading_messages, GS1_ENGINE_MESSAGE_BEGIN_SITE_SNAPSHOT).empty());
    assert(collect_messages_of_type(loading_messages, GS1_ENGINE_MESSAGE_HUD_STATE).empty());
    assert(collect_messages_of_type(loading_messages, GS1_ENGINE_MESSAGE_CAMPAIGN_RESOURCES).empty());

    auto& site_run = gs1::GameRuntimeProjectionTestAccess::active_site_run(runtime).value();
    assert(site_run.clock.accumulator_real_seconds == 0.0);

    Gs1Phase1Result loading_result {};
    run_phase1(runtime, 60.0, loading_result);
    assert(loading_result.fixed_steps_executed == 0U);
    assert(site_run.clock.accumulator_real_seconds == 0.0);
    assert(drain_runtime_messages(runtime).empty());

    const auto ready_event = make_site_scene_ready_event();
    assert(runtime.submit_host_messages(&ready_event, 1U) == GS1_STATUS_OK);
    Gs1Phase2Result ready_result {};
    run_phase2(runtime, ready_result);
    assert(gs1::GameRuntimeProjectionTestAccess::app_state(runtime) == GS1_APP_STATE_SITE_ACTIVE);

    const auto ready_messages = drain_runtime_messages(runtime);
    const auto ready_app_state_messages =
        collect_messages_of_type(ready_messages, GS1_ENGINE_MESSAGE_SET_APP_STATE);
    assert(ready_app_state_messages.size() == 1U);
    assert(
        ready_app_state_messages.front()->payload_as<Gs1EngineMessageSetAppStateData>().app_state ==
        GS1_APP_STATE_SITE_ACTIVE);
    assert(!collect_messages_of_type(ready_messages, GS1_ENGINE_MESSAGE_BEGIN_SITE_SNAPSHOT).empty());
    assert(!collect_messages_of_type(ready_messages, GS1_ENGINE_MESSAGE_HUD_STATE).empty());
    assert(!collect_messages_of_type(ready_messages, GS1_ENGINE_MESSAGE_CAMPAIGN_RESOURCES).empty());
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
    create_desc.adapter_config_json_utf8 = nullptr;

    GameRuntime runtime {create_desc};
    bootstrap_site_one(runtime);

    auto& site_run = gs1::GameRuntimeProjectionTestAccess::active_site_run(runtime).value();
    assert(!site_run.inventory.worker_pack_slots[0].occupied);

    drain_runtime_messages(runtime);

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
    const auto messages = drain_runtime_messages(runtime);
    assert_singleton_projection_messages_do_not_repeat(messages);

    const auto snapshot_messages =
        collect_messages_of_type(messages, GS1_ENGINE_MESSAGE_BEGIN_SITE_SNAPSHOT);
    assert(snapshot_messages.size() == 1U);
    const auto worker_messages =
        collect_messages_of_type(messages, GS1_ENGINE_MESSAGE_SITE_WORKER_UPDATE);
    const auto hud_messages = collect_messages_of_type(messages, GS1_ENGINE_MESSAGE_HUD_STATE);
    const auto campaign_resource_messages =
        collect_messages_of_type(messages, GS1_ENGINE_MESSAGE_CAMPAIGN_RESOURCES);
    assert(worker_messages.size() == 1U);
    assert(hud_messages.size() == 1U);
    assert(campaign_resource_messages.empty());

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
        const auto& payload = hud_messages.back()->payload_as<Gs1EngineMessageHudStateData>();
        assert(approx_equal(payload.player_health, 88.0f));
        assert(approx_equal(payload.player_nourishment, 100.0f));
        assert(approx_equal(payload.player_morale, 100.0f));
        assert(approx_equal(payload.current_money, 20.0f));
        assert(payload.active_task_count == 1U);
    }
}

void task_accept_request_does_not_emit_campaign_resource_projection()
{
    Gs1RuntimeCreateDesc create_desc {};
    create_desc.struct_size = sizeof(Gs1RuntimeCreateDesc);
    create_desc.api_version = gs1::k_api_version;
    create_desc.fixed_step_seconds = 1.0 / 60.0;
    create_desc.adapter_config_json_utf8 = nullptr;

    GameRuntime runtime {create_desc};
    bootstrap_site_one(runtime);
    seed_runtime_test_task(runtime, 10U);
    drain_runtime_messages(runtime);

    assert(runtime.handle_message(make_accept_task_message(1U)) == GS1_STATUS_OK);

    std::vector<Gs1RuntimeMessage> messages = drain_runtime_messages(runtime);
    gs1::GameRuntimeProjectionTestAccess::flush_projection(runtime);
    append_messages(messages, drain_runtime_messages(runtime));
    assert_singleton_projection_messages_do_not_repeat(messages);

    const auto campaign_resource_messages =
        collect_messages_of_type(messages, GS1_ENGINE_MESSAGE_CAMPAIGN_RESOURCES);
    assert(campaign_resource_messages.empty());
}

void phone_purchase_request_emits_single_hud_and_campaign_resource_projection()
{
    Gs1RuntimeCreateDesc create_desc {};
    create_desc.struct_size = sizeof(Gs1RuntimeCreateDesc);
    create_desc.api_version = gs1::k_api_version;
    create_desc.fixed_step_seconds = 1.0 / 60.0;
    create_desc.adapter_config_json_utf8 = nullptr;

    GameRuntime runtime {create_desc};
    bootstrap_site_one(runtime);
    auto& site_run = gs1::GameRuntimeProjectionTestAccess::active_site_run(runtime).value();
    drain_runtime_messages(runtime);

    GameMessage buy_listing {};
    buy_listing.type = GameMessageType::PhoneListingPurchaseRequested;
    buy_listing.set_payload(PhoneListingPurchaseRequestedMessage {1U, 1U, 0U});
    assert(runtime.handle_message(buy_listing) == GS1_STATUS_OK);
    assert(site_run.economy.current_cash == 1280);

    std::vector<Gs1RuntimeMessage> messages = drain_runtime_messages(runtime);
    gs1::GameRuntimeProjectionTestAccess::flush_projection(runtime);
    append_messages(messages, drain_runtime_messages(runtime));
    assert_singleton_projection_messages_do_not_repeat(messages);

    const auto hud_messages = collect_messages_of_type(messages, GS1_ENGINE_MESSAGE_HUD_STATE);
    const auto campaign_resource_messages =
        collect_messages_of_type(messages, GS1_ENGINE_MESSAGE_CAMPAIGN_RESOURCES);
    assert(hud_messages.size() == 1U);
    assert(campaign_resource_messages.size() == 1U);

    {
        const auto& payload = hud_messages.front()->payload_as<Gs1EngineMessageHudStateData>();
        assert(approx_equal(payload.current_money, 12.8f));
    }

    {
        const auto& payload =
            campaign_resource_messages.front()->payload_as<Gs1EngineMessageCampaignResourcesData>();
        assert(approx_equal(payload.current_money, 12.8f));
        assert(payload.total_reputation == 0);
        assert(payload.village_reputation == 0);
        assert(payload.forestry_reputation == 0);
        assert(payload.university_reputation == 0);
    }
}

void campaign_reputation_award_emits_campaign_resource_projection_and_unlock_cue()
{
    Gs1RuntimeCreateDesc create_desc {};
    create_desc.struct_size = sizeof(Gs1RuntimeCreateDesc);
    create_desc.api_version = gs1::k_api_version;
    create_desc.fixed_step_seconds = 1.0 / 60.0;
    create_desc.adapter_config_json_utf8 = nullptr;

    GameRuntime runtime {create_desc};
    bootstrap_site_one(runtime);
    drain_runtime_messages(runtime);

    GameMessage reputation_award {};
    reputation_award.type = GameMessageType::CampaignReputationAwardRequested;
    reputation_award.set_payload(gs1::CampaignReputationAwardRequestedMessage {100});
    assert(runtime.handle_message(reputation_award) == GS1_STATUS_OK);

    std::vector<Gs1RuntimeMessage> messages = drain_runtime_messages(runtime);
    gs1::GameRuntimeProjectionTestAccess::flush_projection(runtime);
    append_messages(messages, drain_runtime_messages(runtime));
    assert_singleton_projection_messages_do_not_repeat(messages);

    const auto campaign_resource_messages =
        collect_messages_of_type(messages, GS1_ENGINE_MESSAGE_CAMPAIGN_RESOURCES);
    assert(campaign_resource_messages.size() == 1U);
    {
        const auto& payload =
            campaign_resource_messages.front()->payload_as<Gs1EngineMessageCampaignResourcesData>();
        assert(payload.total_reputation == 100);
    }

    const auto* unlock_cue_message =
        find_one_shot_cue_message(messages, GS1_ONE_SHOT_CUE_CAMPAIGN_UNLOCKED);
    assert(unlock_cue_message != nullptr);
    {
        const auto& payload = unlock_cue_message->payload_as<Gs1EngineMessageOneShotCueData>();
        assert(payload.arg1 == 1U || payload.arg1 == 2U);
        assert(payload.arg0 != 0U);
    }
}

void inventory_use_request_keeps_singleton_projections_singular_across_immediate_and_flush_paths()
{
    Gs1RuntimeCreateDesc create_desc {};
    create_desc.struct_size = sizeof(Gs1RuntimeCreateDesc);
    create_desc.api_version = gs1::k_api_version;
    create_desc.fixed_step_seconds = 1.0 / 60.0;
    create_desc.adapter_config_json_utf8 = nullptr;

    GameRuntime runtime {create_desc};
    bootstrap_site_one(runtime);

    auto& site_run = gs1::GameRuntimeProjectionTestAccess::active_site_run(runtime).value();
    drain_runtime_messages(runtime);

    site_run.inventory.worker_pack_slots[4].occupied = true;
    site_run.inventory.worker_pack_slots[4].item_id = gs1::ItemId {gs1::k_item_medicine_pack};
    site_run.inventory.worker_pack_slots[4].item_quantity = 1U;
    site_run.inventory.worker_pack_slots[4].item_condition = 1.0f;
    site_run.inventory.worker_pack_slots[4].item_freshness = 1.0f;

    assert(runtime.handle_message(make_inventory_use_message(
               gs1::k_item_medicine_pack,
               1U,
               site_run.inventory.worker_pack_storage_id,
               4U)) == GS1_STATUS_OK);

    std::vector<Gs1RuntimeMessage> messages = drain_runtime_messages(runtime);
    gs1::GameRuntimeProjectionTestAccess::flush_projection(runtime);
    append_messages(messages, drain_runtime_messages(runtime));
    assert_singleton_projection_messages_do_not_repeat(messages);
}

void ecology_growth_completes_task_and_site_attempt()
{
    Gs1RuntimeCreateDesc create_desc {};
    create_desc.struct_size = sizeof(Gs1RuntimeCreateDesc);
    create_desc.api_version = gs1::k_api_version;
    create_desc.fixed_step_seconds = 60.0;
    create_desc.adapter_config_json_utf8 = nullptr;

    GameRuntime runtime {create_desc};
    bootstrap_site_one(runtime);

    auto& site_run = gs1::GameRuntimeProjectionTestAccess::active_site_run(runtime).value();
    assert(site_run.counters.site_completion_tile_threshold == 10U);
    assert(site_run.task_board.visible_tasks.size() == 1U);
    assert(site_run.task_board.accepted_task_ids.size() == 1U);
    site_run.counters.site_completion_tile_threshold = 1U;
    seed_runtime_test_task(runtime, 10U);
    assert(site_run.task_board.visible_tasks.front().target_amount == 10U);

    drain_runtime_messages(runtime);

    assert(runtime.handle_message(make_accept_task_message(1U)) == GS1_STATUS_OK);
    assert(site_run.task_board.accepted_task_ids.size() == 1U);

    GameMessage restoration_message {};
    restoration_message.type = GameMessageType::RestorationProgressChanged;
    restoration_message.set_payload(gs1::RestorationProgressChangedMessage {10U, 1U, 1.0f});
    assert(runtime.handle_message(restoration_message) == GS1_STATUS_OK);

    assert(site_run.task_board.accepted_task_ids.empty());
    assert(site_run.task_board.completed_task_ids.size() == 1U);
    assert(site_run.task_board.visible_tasks.front().runtime_list_kind == TaskRuntimeListKind::PendingClaim);
    assert(site_run.task_board.visible_tasks.front().current_progress_amount == 10U);

    gs1::GameRuntimeProjectionTestAccess::flush_projection(runtime);
    const auto phase1_messages = drain_runtime_messages(runtime);
    const auto* completed_task_message = find_task_message(phase1_messages, 1U);
    assert(completed_task_message != nullptr);
    {
        const auto& payload = completed_task_message->payload_as<Gs1EngineMessageTaskData>();
        assert(payload.current_progress == 10U);
        assert(payload.target_progress == 10U);
        assert(payload.list_kind == GS1_TASK_PRESENTATION_LIST_PENDING_CLAIM);
    }

    gs1::GameRuntimeProjectionTestAccess::flush_projection(runtime);
    (void)drain_runtime_messages(runtime);
}

void task_reward_claim_emits_pending_claim_projection_then_reward_cue()
{
    Gs1RuntimeCreateDesc create_desc {};
    create_desc.struct_size = sizeof(Gs1RuntimeCreateDesc);
    create_desc.api_version = gs1::k_api_version;
    create_desc.fixed_step_seconds = 60.0;
    create_desc.adapter_config_json_utf8 = nullptr;

    GameRuntime runtime {create_desc};
    bootstrap_site_one(runtime);

    auto& site_run = gs1::GameRuntimeProjectionTestAccess::active_site_run(runtime).value();
    drain_runtime_messages(runtime);

    auto& task = site_run.task_board.visible_tasks.front();
    task.task_instance_id = gs1::TaskInstanceId {1U};
    task.task_template_id = gs1::TaskTemplateId {gs1::k_task_template_site1_buy_water};
    task.publisher_faction_id = gs1::FactionId {gs1::k_faction_village_committee};
    task.target_amount = 1U;
    task.current_progress_amount = 1U;
    task.runtime_list_kind = TaskRuntimeListKind::PendingClaim;
    task.reward_draft_options.clear();
    task.reward_draft_options.push_back(gs1::TaskRewardDraftOption {gs1::RewardCandidateId {1U}, false});
    task.reward_draft_options.push_back(gs1::TaskRewardDraftOption {gs1::RewardCandidateId {2U}, false});
    site_run.task_board.accepted_task_ids.clear();
    site_run.task_board.completed_task_ids.clear();
    site_run.task_board.completed_task_ids.push_back(task.task_instance_id);
    site_run.task_board.claimed_task_ids.clear();

    GameMessage claim_message {};
    claim_message.type = GameMessageType::TaskRewardClaimRequested;
    claim_message.set_payload(gs1::TaskRewardClaimRequestedMessage {1U, 0U});
    assert(runtime.handle_message(claim_message) == GS1_STATUS_OK);

    const auto messages = drain_runtime_messages(runtime);
    const auto* claimed_task_message = find_task_message(messages, 1U);
    assert(claimed_task_message != nullptr);
    {
        const auto& payload = claimed_task_message->payload_as<Gs1EngineMessageTaskData>();
        assert(payload.list_kind == GS1_TASK_PRESENTATION_LIST_CLAIMED);
    }

    const auto* reward_cue_message =
        find_one_shot_cue_message(messages, GS1_ONE_SHOT_CUE_TASK_REWARD_CLAIMED);
    assert(reward_cue_message != nullptr);
    {
        const auto& payload = reward_cue_message->payload_as<Gs1EngineMessageOneShotCueData>();
        assert(payload.subject_id == 1U);
        assert(payload.arg0 == gs1::k_task_template_site1_buy_water);
        assert(payload.arg1 == 2U);
    }
}

void task_reward_claim_ui_action_claims_pending_task_and_emits_reward_cue()
{
    Gs1RuntimeCreateDesc create_desc {};
    create_desc.struct_size = sizeof(Gs1RuntimeCreateDesc);
    create_desc.api_version = gs1::k_api_version;
    create_desc.fixed_step_seconds = 60.0;
    create_desc.adapter_config_json_utf8 = nullptr;

    GameRuntime runtime {create_desc};
    bootstrap_site_one(runtime);

    auto& site_run = gs1::GameRuntimeProjectionTestAccess::active_site_run(runtime).value();
    drain_runtime_messages(runtime);

    auto& task = site_run.task_board.visible_tasks.front();
    task.task_instance_id = gs1::TaskInstanceId {1U};
    task.task_template_id = gs1::TaskTemplateId {gs1::k_task_template_site1_buy_water};
    task.publisher_faction_id = gs1::FactionId {gs1::k_faction_village_committee};
    task.target_amount = 1U;
    task.current_progress_amount = 1U;
    task.runtime_list_kind = TaskRuntimeListKind::PendingClaim;
    task.reward_draft_options.clear();
    task.reward_draft_options.push_back(gs1::TaskRewardDraftOption {gs1::RewardCandidateId {1U}, false});
    task.reward_draft_options.push_back(gs1::TaskRewardDraftOption {gs1::RewardCandidateId {2U}, false});
    site_run.task_board.accepted_task_ids.clear();
    site_run.task_board.completed_task_ids.clear();
    site_run.task_board.completed_task_ids.push_back(task.task_instance_id);
    site_run.task_board.claimed_task_ids.clear();

    Gs1UiAction claim_action {};
    claim_action.type = GS1_UI_ACTION_CLAIM_TASK_REWARD;
    claim_action.target_id = 1U;
    const auto claim_event = make_ui_action_event(claim_action);

    Gs1Phase1Result claim_result {};
    assert(runtime.submit_host_messages(&claim_event, 1U) == GS1_STATUS_OK);
    run_phase1(runtime, 0.0, claim_result);
    assert(claim_result.processed_host_message_count == 1U);

    const auto messages = drain_runtime_messages(runtime);
    const auto* claimed_task_message = find_task_message(messages, 1U);
    assert(claimed_task_message != nullptr);
    {
        const auto& payload = claimed_task_message->payload_as<Gs1EngineMessageTaskData>();
        assert(payload.list_kind == GS1_TASK_PRESENTATION_LIST_CLAIMED);
    }

    const auto* reward_cue_message =
        find_one_shot_cue_message(messages, GS1_ONE_SHOT_CUE_TASK_REWARD_CLAIMED);
    assert(reward_cue_message != nullptr);
    {
        const auto& payload = reward_cue_message->payload_as<Gs1EngineMessageOneShotCueData>();
        assert(payload.subject_id == 1U);
        assert(payload.arg0 == gs1::k_task_template_site1_buy_water);
        assert(payload.arg1 == 2U);
    }
}

void inventory_craft_completed_opens_output_storage_and_emits_craft_output_cue()
{
    Gs1RuntimeCreateDesc create_desc {};
    create_desc.struct_size = sizeof(Gs1RuntimeCreateDesc);
    create_desc.api_version = gs1::k_api_version;
    create_desc.fixed_step_seconds = 1.0 / 60.0;
    create_desc.adapter_config_json_utf8 = nullptr;

    GameRuntime runtime {create_desc};
    bootstrap_site_one(runtime);

    auto& site_run = gs1::GameRuntimeProjectionTestAccess::active_site_run(runtime).value();
    drain_runtime_messages(runtime);

    const auto storage_id = starter_storage_id(site_run);
    GameMessage crafted_message {};
    crafted_message.type = GameMessageType::InventoryCraftCompleted;
    crafted_message.set_payload(gs1::InventoryCraftCompletedMessage {
        gs1::k_recipe_craft_storage_crate,
        gs1::k_item_storage_crate_kit,
        1U,
        0U,
        storage_id});
    assert(runtime.handle_message(crafted_message) == GS1_STATUS_OK);

    assert(site_run.inventory.opened_device_storage_id == storage_id);

    const auto messages = drain_runtime_messages(runtime);
    const auto* craft_cue_message =
        find_one_shot_cue_message(messages, GS1_ONE_SHOT_CUE_CRAFT_OUTPUT_STORED);
    assert(craft_cue_message != nullptr);
    {
        const auto& payload = craft_cue_message->payload_as<Gs1EngineMessageOneShotCueData>();
        assert(payload.subject_id == storage_id);
        assert(payload.arg0 == gs1::k_item_storage_crate_kit);
        assert(payload.arg1 == 1U);
    }
}

void site_weather_changes_emit_hud_wind_warning_codes()
{
    constexpr std::uint16_t k_hud_warning_wind_watch = 1U;
    constexpr std::uint16_t k_hud_warning_wind_exposure = 2U;

    Gs1RuntimeCreateDesc create_desc {};
    create_desc.struct_size = sizeof(Gs1RuntimeCreateDesc);
    create_desc.api_version = gs1::k_api_version;
    create_desc.fixed_step_seconds = 1.0 / 60.0;
    create_desc.adapter_config_json_utf8 = nullptr;

    GameRuntime runtime {create_desc};
    bootstrap_site_one(runtime);

    auto& site_run = gs1::GameRuntimeProjectionTestAccess::active_site_run(runtime).value();
    drain_runtime_messages(runtime);

    const auto worker_position = gs1::site_world_access::worker_position(site_run);
    gs1::site_world_access::set_tile_local_weather(
        site_run,
        worker_position.tile_coord,
        gs1::SiteWorld::TileLocalWeatherData {8.0f, 36.0f, 6.0f});
    gs1::GameRuntimeProjectionTestAccess::mark_projection_dirty(runtime, gs1::SITE_PROJECTION_UPDATE_WEATHER);
    gs1::GameRuntimeProjectionTestAccess::flush_projection(runtime);

    const auto weather_messages = drain_runtime_messages(runtime);
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

    const auto sheltered_messages = drain_runtime_messages(runtime);
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
    site_loading_waits_for_scene_ready_before_bootstrap_and_fixed_steps();
    inventory_item_use_updates_worker_and_projection();
    inventory_use_request_keeps_singleton_projections_singular_across_immediate_and_flush_paths();
    task_accept_request_does_not_emit_campaign_resource_projection();
    phone_purchase_request_emits_single_hud_and_campaign_resource_projection();
    campaign_reputation_award_emits_campaign_resource_projection_and_unlock_cue();
    ecology_growth_completes_task_and_site_attempt();
    task_reward_claim_emits_pending_claim_projection_then_reward_cue();
    task_reward_claim_ui_action_claims_pending_task_and_emits_reward_cue();
    inventory_craft_completed_opens_output_storage_and_emits_craft_output_cue();
    site_weather_changes_emit_hud_wind_warning_codes();
    return 0;
}


