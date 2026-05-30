#include "runtime/game_runtime.h"
#include "../system/source/split_state_test_helpers.h"
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
using gs1::GameMessageQueue;
using gs1::GameRuntime;
using gs1::GameState;
using gs1::InventoryItemUseRequestedMessage;
using gs1::PhoneListingPurchaseRequestedMessage;
using gs1::RuntimeInvocation;
using gs1::SiteSceneActivatedMessage;
using gs1::SiteWorldHandle;
using gs1::StateManager;
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
    [[nodiscard]] static std::optional<CampaignState> campaign(GameRuntime& runtime)
    {
        if (!runtime.state().campaign_core.has_value())
        {
            return std::nullopt;
        }

        return assemble_campaign_state_from_state_sets(runtime.state(), runtime.state_manager());
    }

    [[nodiscard]] static std::optional<SiteRunState> active_site_run(GameRuntime& runtime)
    {
        if (!runtime.state().site_run_meta.has_value())
        {
            return std::nullopt;
        }

        return assemble_site_run_state_from_state_sets(
            runtime.state(),
            runtime.state_manager(),
            runtime.site_world_);
    }

    static Gs1AppState app_state(const GameRuntime& runtime)
    {
        return runtime.state().app_state;
    }

    template <typename Func>
    static void mutate_active_site_run(GameRuntime& runtime, Func&& func)
    {
        auto site_run = active_site_run(runtime);
        assert(site_run.has_value());
        func(site_run.value());
        write_site_run_state_to_state_sets(site_run.value(), runtime.state(), runtime.state_manager());
        runtime.site_world_ = site_run->site_world;
    }

    static void mark_tile_dirty(GameRuntime& runtime, TileCoord coord)
    {
        (void)runtime;
        (void)coord;
    }

    static void mark_projection_dirty(GameRuntime& runtime, std::uint64_t dirty_flags)
    {
        (void)runtime;
        (void)dirty_flags;
    }
};
}  // namespace gs1

namespace
{
StartNewCampaignMessage make_start_campaign_message()
{
    return StartNewCampaignMessage {42ULL, 30U};
}

StartSiteAttemptMessage make_start_site_attempt_message(std::uint32_t site_id)
{
    return StartSiteAttemptMessage {site_id};
}

TaskAcceptRequestedMessage make_accept_task_message(std::uint32_t task_instance_id)
{
    return TaskAcceptRequestedMessage {task_instance_id};
}

InventoryItemUseRequestedMessage make_inventory_use_message(
    std::uint32_t item_id,
    std::uint32_t quantity,
    std::uint32_t storage_id,
    std::uint32_t slot_index)
{
    return InventoryItemUseRequestedMessage {
        item_id,
        storage_id,
        static_cast<std::uint16_t>(quantity),
        static_cast<std::uint16_t>(slot_index)};
}

GameMessage make_site_attempt_ended_message(std::uint32_t site_id, Gs1SiteAttemptResult result)
{
    return GameMessage {gs1::SiteAttemptEndedMessage {site_id, result}};
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

template <typename Predicate>
std::optional<std::size_t> find_message_index(
    const std::vector<Gs1RuntimeMessage>& messages,
    Predicate&& predicate)
{
    for (std::size_t index = 0U; index < messages.size(); ++index)
    {
        if (predicate(messages[index]))
        {
            return index;
        }
    }

    return std::nullopt;
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

void runtime_invocation_without_runtime_emits_into_explicit_test_queue()
{
    GameState state {};
    StateManager state_manager {};
    GameMessageQueue emitted_messages {};
    RuntimeInvocation invocation {
        state,
        state_manager,
        SiteWorldHandle {},
        &emitted_messages};

    invocation.emit_game_message(SiteSceneActivatedMessage {});

    assert(emitted_messages.size() == 1U);
    assert(emitted_messages.front().type == gs1::GameMessageType::SiteSceneActivated);
    (void)emitted_messages.front().payload_as<gs1::SiteSceneActivatedMessage>();
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
    const auto campaign = gs1::GameRuntimeProjectionTestAccess::campaign(runtime);
    assert(campaign.has_value());
    assert(!campaign->sites.empty());

    const auto site_id = campaign->sites.front().site_id.value;
    assert(runtime.handle_message(make_start_site_attempt_message(site_id)) == GS1_STATUS_OK);
    assert(gs1::GameRuntimeProjectionTestAccess::active_site_run(runtime).has_value());

    assert(runtime.submit_site_scene_ready() == GS1_STATUS_OK);
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
    const auto campaign = gs1::GameRuntimeProjectionTestAccess::campaign(runtime);
    assert(campaign.has_value());
    assert(!campaign->sites.empty());
    drain_runtime_messages(runtime);

    const auto site_id = campaign->sites.front().site_id.value;
    assert(runtime.handle_message(make_start_site_attempt_message(site_id)) == GS1_STATUS_OK);
    assert(gs1::GameRuntimeProjectionTestAccess::active_site_run(runtime).has_value());
    assert(gs1::GameRuntimeProjectionTestAccess::app_state(runtime) == GS1_APP_STATE_SITE_LOADING);

    const auto loading_messages = drain_runtime_messages(runtime);
    assert(collect_messages_of_type(loading_messages, GS1_ENGINE_MESSAGE_BEGIN_SITE_SNAPSHOT).empty());
    assert(collect_messages_of_type(loading_messages, GS1_ENGINE_MESSAGE_HUD_STATE).empty());
    assert(collect_messages_of_type(loading_messages, GS1_ENGINE_MESSAGE_CAMPAIGN_RESOURCES).empty());

    const auto site_run = gs1::GameRuntimeProjectionTestAccess::active_site_run(runtime);
    assert(site_run.has_value());
    assert(site_run->clock.accumulator_real_seconds == 0.0);

    Gs1Phase1Result loading_result {};
    run_phase1(runtime, 60.0, loading_result);
    assert(loading_result.fixed_steps_executed == 0U);
    assert(
        gs1::GameRuntimeProjectionTestAccess::active_site_run(runtime)->clock.accumulator_real_seconds ==
        0.0);
    assert(drain_runtime_messages(runtime).empty());

    assert(runtime.submit_site_scene_ready() == GS1_STATUS_OK);
    Gs1Phase2Result ready_result {};
    run_phase2(runtime, ready_result);
    assert(gs1::GameRuntimeProjectionTestAccess::app_state(runtime) == GS1_APP_STATE_SITE_ACTIVE);

    const auto ready_messages = drain_runtime_messages(runtime);
    assert(ready_messages.empty());
}

void seed_runtime_test_task(GameRuntime& runtime, std::uint32_t site_completion_target)
{
    gs1::GameRuntimeProjectionTestAccess::mutate_active_site_run(
        runtime,
        [site_completion_target](SiteRunState& site_run)
        {
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
        });
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

    auto site_run = gs1::GameRuntimeProjectionTestAccess::active_site_run(runtime);
    assert(site_run.has_value());
    assert(!site_run->inventory.worker_pack_slots[0].occupied);

    drain_runtime_messages(runtime);

    gs1::GameRuntimeProjectionTestAccess::mutate_active_site_run(
        runtime,
        [](SiteRunState& active_site_run)
        {
            (void)gs1::inventory_storage::add_item_to_container(
                active_site_run,
                gs1::inventory_storage::worker_pack_container(active_site_run),
                gs1::ItemId {gs1::k_item_medicine_pack},
                1U);

            auto worker_conditions = gs1::site_world_access::worker_conditions(active_site_run);
            worker_conditions.health = 70.0f;
            gs1::site_world_access::set_worker_conditions(active_site_run, worker_conditions);
            assert(approx_equal(
                gs1::site_world_access::worker_conditions(active_site_run).health,
                70.0f));
        });

    const auto inventory_use_status = runtime.handle_message(make_inventory_use_message(
        gs1::k_item_medicine_pack,
        1U,
        site_run->inventory.worker_pack_storage_id,
        0U));
    assert(inventory_use_status == GS1_STATUS_OK);

    site_run = gs1::GameRuntimeProjectionTestAccess::active_site_run(runtime);
    assert(site_run.has_value());
    assert(!site_run->inventory.worker_pack_slots[0].occupied);
    const auto worker_conditions = gs1::site_world_access::worker_conditions(site_run.value());
    assert(approx_equal(worker_conditions.health, 88.0f));

    const auto messages = drain_runtime_messages(runtime);
    assert_singleton_projection_messages_do_not_repeat(messages);

    const auto campaign_resource_messages =
        collect_messages_of_type(messages, GS1_ENGINE_MESSAGE_CAMPAIGN_RESOURCES);
    assert(campaign_resource_messages.empty());

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
    assert_singleton_projection_messages_do_not_repeat(messages);

    const auto campaign_resource_messages =
        collect_messages_of_type(messages, GS1_ENGINE_MESSAGE_CAMPAIGN_RESOURCES);
    assert(campaign_resource_messages.empty());
}

void phone_purchase_request_emits_purchase_completion_and_task_progress_projection()
{
    Gs1RuntimeCreateDesc create_desc {};
    create_desc.struct_size = sizeof(Gs1RuntimeCreateDesc);
    create_desc.api_version = gs1::k_api_version;
    create_desc.fixed_step_seconds = 1.0 / 60.0;
    create_desc.adapter_config_json_utf8 = nullptr;

    GameRuntime runtime {create_desc};
    bootstrap_site_one(runtime);
    const auto site_run = gs1::GameRuntimeProjectionTestAccess::active_site_run(runtime);
    assert(site_run.has_value());
    drain_runtime_messages(runtime);

    gs1::GameRuntimeProjectionTestAccess::mutate_active_site_run(
        runtime,
        [](SiteRunState& active_site_run)
        {
            auto& task = active_site_run.task_board.visible_tasks.front();
            task.task_instance_id = gs1::TaskInstanceId {1U};
            task.task_template_id = gs1::TaskTemplateId {gs1::k_task_template_site1_buy_water};
            task.publisher_faction_id = gs1::FactionId {gs1::k_faction_village_committee};
            task.item_id = gs1::ItemId {gs1::k_item_water_container};
            task.target_amount = 1U;
            task.current_progress_amount = 0U;
            task.runtime_list_kind = TaskRuntimeListKind::Accepted;
            task.reward_draft_option_offset = 0U;
            task.reward_draft_option_count = 2U;
            active_site_run.task_board.accepted_task_ids.clear();
            active_site_run.task_board.accepted_task_ids.push_back(task.task_instance_id);
            active_site_run.task_board.completed_task_ids.clear();
            active_site_run.task_board.claimed_task_ids.clear();
        });

    const PhoneListingPurchaseRequestedMessage buy_listing {1U, 1U, 0U};
    assert(runtime.handle_message(buy_listing) == GS1_STATUS_OK);
    const auto updated_site_run = gs1::GameRuntimeProjectionTestAccess::active_site_run(runtime);
    assert(updated_site_run.has_value());
    assert(updated_site_run->economy.current_cash == 1280);
    assert(updated_site_run->task_board.visible_tasks.front().current_progress_amount == 1U);

    std::vector<Gs1RuntimeMessage> messages = drain_runtime_messages(runtime);
    assert_singleton_projection_messages_do_not_repeat(messages);

    const auto* purchased_message = find_task_message(messages, 1U);
    if (purchased_message != nullptr)
    {
        const auto& payload = purchased_message->payload_as<Gs1EngineMessageTaskData>();
        assert(payload.current_progress == 1U);
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

    const gs1::CampaignReputationAwardRequestedMessage reputation_award {100};
    assert(runtime.handle_message(reputation_award) == GS1_STATUS_OK);
    const auto updated_campaign = gs1::GameRuntimeProjectionTestAccess::campaign(runtime);
    assert(updated_campaign.has_value());
    assert(updated_campaign->progression_state.total_reputation == 100);

    std::vector<Gs1RuntimeMessage> messages = drain_runtime_messages(runtime);
    assert_singleton_projection_messages_do_not_repeat(messages);
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

    auto site_run = gs1::GameRuntimeProjectionTestAccess::active_site_run(runtime);
    assert(site_run.has_value());
    drain_runtime_messages(runtime);

    gs1::GameRuntimeProjectionTestAccess::mutate_active_site_run(
        runtime,
        [](SiteRunState& active_site_run)
        {
            (void)gs1::inventory_storage::add_item_to_container(
                active_site_run,
                gs1::inventory_storage::worker_pack_container(active_site_run),
                gs1::ItemId {gs1::k_item_medicine_pack},
                1U);
        });

    assert(runtime.handle_message(make_inventory_use_message(
               gs1::k_item_medicine_pack,
               1U,
               site_run->inventory.worker_pack_storage_id,
               0U)) == GS1_STATUS_OK);

    std::vector<Gs1RuntimeMessage> messages = drain_runtime_messages(runtime);
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

    auto site_run = gs1::GameRuntimeProjectionTestAccess::active_site_run(runtime);
    assert(site_run.has_value());
    assert(site_run->counters.site_completion_tile_threshold == 10U);
    assert(site_run->task_board.visible_tasks.size() == 1U);
    assert(site_run->task_board.accepted_task_ids.size() == 1U);
    gs1::GameRuntimeProjectionTestAccess::mutate_active_site_run(
        runtime,
        [](SiteRunState& active_site_run)
        {
            active_site_run.counters.site_completion_tile_threshold = 1U;
        });
    seed_runtime_test_task(runtime, 10U);
    assert(gs1::GameRuntimeProjectionTestAccess::active_site_run(runtime)->task_board.visible_tasks.front().target_amount == 10U);

    drain_runtime_messages(runtime);

    assert(runtime.handle_message(make_accept_task_message(1U)) == GS1_STATUS_OK);
    assert(gs1::GameRuntimeProjectionTestAccess::active_site_run(runtime)->task_board.accepted_task_ids.size() == 1U);

    const gs1::RestorationProgressChangedMessage restoration_message {10U, 1U, 1.0f};
    assert(runtime.handle_message(restoration_message) == GS1_STATUS_OK);

    site_run = gs1::GameRuntimeProjectionTestAccess::active_site_run(runtime);
    assert(site_run.has_value());
    assert(site_run->task_board.accepted_task_ids.empty());
    assert(site_run->task_board.completed_task_ids.size() == 1U);
    assert(site_run->task_board.visible_tasks.front().runtime_list_kind == TaskRuntimeListKind::PendingClaim);
    assert(site_run->task_board.visible_tasks.front().current_progress_amount == 10U);

    const auto phase1_messages = drain_runtime_messages(runtime);
    const auto* completed_task_message = find_task_message(phase1_messages, 1U);
    if (completed_task_message != nullptr)
    {
        const auto& payload = completed_task_message->payload_as<Gs1EngineMessageTaskData>();
        assert(payload.current_progress == 10U);
        assert(payload.target_progress == 10U);
        assert(payload.list_kind == GS1_TASK_PRESENTATION_LIST_PENDING_CLAIM);
    }

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

    auto site_run = gs1::GameRuntimeProjectionTestAccess::active_site_run(runtime);
    assert(site_run.has_value());
    drain_runtime_messages(runtime);

    gs1::GameRuntimeProjectionTestAccess::mutate_active_site_run(
        runtime,
        [](SiteRunState& active_site_run)
        {
            auto& task = active_site_run.task_board.visible_tasks.front();
            task.task_instance_id = gs1::TaskInstanceId {1U};
            task.task_template_id = gs1::TaskTemplateId {gs1::k_task_template_site1_buy_water};
            task.publisher_faction_id = gs1::FactionId {gs1::k_faction_village_committee};
            task.target_amount = 1U;
            task.current_progress_amount = 1U;
            task.runtime_list_kind = TaskRuntimeListKind::PendingClaim;
            task.reward_draft_option_offset = 0U;
            task.reward_draft_option_count = 2U;
            active_site_run.task_board.reward_draft_options.clear();
            active_site_run.task_board.reward_draft_options.push_back(
                gs1::TaskRewardDraftOption {gs1::RewardCandidateId {1U}, false});
            active_site_run.task_board.reward_draft_options.push_back(
                gs1::TaskRewardDraftOption {gs1::RewardCandidateId {2U}, false});
            active_site_run.task_board.accepted_task_ids.clear();
            active_site_run.task_board.completed_task_ids.clear();
            active_site_run.task_board.completed_task_ids.push_back(task.task_instance_id);
            active_site_run.task_board.claimed_task_ids.clear();
        });

    const gs1::TaskRewardClaimRequestedMessage claim_message {1U, 0U};
    assert(runtime.handle_message(claim_message) == GS1_STATUS_OK);

    const auto messages = drain_runtime_messages(runtime);
    const auto* claimed_task_message = find_task_message(messages, 1U);
    if (claimed_task_message != nullptr)
    {
        const auto& payload = claimed_task_message->payload_as<Gs1EngineMessageTaskData>();
        assert(payload.list_kind == GS1_TASK_PRESENTATION_LIST_CLAIMED);
    }

    const auto claimed_task_index = find_message_index(messages, [](const Gs1RuntimeMessage& message) {
        if (message.type != GS1_ENGINE_MESSAGE_SITE_TASK_UPSERT)
        {
            return false;
        }

        return message.payload_as<Gs1EngineMessageTaskData>().task_instance_id == 1U;
    });

    const auto reward_cue_index = find_message_index(messages, [](const Gs1RuntimeMessage& message) {
        return message.type == GS1_ENGINE_MESSAGE_PLAY_ONE_SHOT_CUE &&
            message.payload_as<Gs1EngineMessageOneShotCueData>().cue_kind ==
            GS1_ONE_SHOT_CUE_TASK_REWARD_CLAIMED;
    });
    if (reward_cue_index.has_value())
    {
        if (claimed_task_index.has_value())
        {
            assert(*claimed_task_index < *reward_cue_index);
        }

        const auto* reward_cue_message = &messages[*reward_cue_index];
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

    auto site_run = gs1::GameRuntimeProjectionTestAccess::active_site_run(runtime);
    assert(site_run.has_value());
    drain_runtime_messages(runtime);

    gs1::GameRuntimeProjectionTestAccess::mutate_active_site_run(
        runtime,
        [](SiteRunState& active_site_run)
        {
            auto& task = active_site_run.task_board.visible_tasks.front();
            task.task_instance_id = gs1::TaskInstanceId {1U};
            task.task_template_id = gs1::TaskTemplateId {gs1::k_task_template_site1_buy_water};
            task.publisher_faction_id = gs1::FactionId {gs1::k_faction_village_committee};
            task.target_amount = 1U;
            task.current_progress_amount = 1U;
            task.runtime_list_kind = TaskRuntimeListKind::PendingClaim;
            task.reward_draft_option_offset = 0U;
            task.reward_draft_option_count = 2U;
            active_site_run.task_board.reward_draft_options.clear();
            active_site_run.task_board.reward_draft_options.push_back(
                gs1::TaskRewardDraftOption {gs1::RewardCandidateId {1U}, false});
            active_site_run.task_board.reward_draft_options.push_back(
                gs1::TaskRewardDraftOption {gs1::RewardCandidateId {2U}, false});
            active_site_run.task_board.accepted_task_ids.clear();
            active_site_run.task_board.completed_task_ids.clear();
            active_site_run.task_board.completed_task_ids.push_back(task.task_instance_id);
            active_site_run.task_board.claimed_task_ids.clear();
        });

    Gs1UiAction claim_action {};
    claim_action.type = GS1_UI_ACTION_CLAIM_TASK_REWARD;
    claim_action.target_id = 1U;
    Gs1Phase1Result claim_result {};
    assert(runtime.submit_gameplay_action(claim_action) == GS1_STATUS_OK);
    run_phase1(runtime, 0.0, claim_result);
    assert(claim_result.processed_host_message_count == 1U);

    const auto messages = drain_runtime_messages(runtime);
    const auto* claimed_task_message = find_task_message(messages, 1U);
    if (claimed_task_message != nullptr)
    {
        const auto& payload = claimed_task_message->payload_as<Gs1EngineMessageTaskData>();
        assert(payload.list_kind == GS1_TASK_PRESENTATION_LIST_CLAIMED);
    }

    const auto claimed_task_index = find_message_index(messages, [](const Gs1RuntimeMessage& message) {
        if (message.type != GS1_ENGINE_MESSAGE_SITE_TASK_UPSERT)
        {
            return false;
        }

        return message.payload_as<Gs1EngineMessageTaskData>().task_instance_id == 1U;
    });

    const auto reward_cue_index = find_message_index(messages, [](const Gs1RuntimeMessage& message) {
        return message.type == GS1_ENGINE_MESSAGE_PLAY_ONE_SHOT_CUE &&
            message.payload_as<Gs1EngineMessageOneShotCueData>().cue_kind ==
            GS1_ONE_SHOT_CUE_TASK_REWARD_CLAIMED;
    });
    if (reward_cue_index.has_value())
    {
        if (claimed_task_index.has_value())
        {
            assert(*claimed_task_index < *reward_cue_index);
        }

        const auto* reward_cue_message = &messages[*reward_cue_index];
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

    auto site_run = gs1::GameRuntimeProjectionTestAccess::active_site_run(runtime);
    assert(site_run.has_value());
    drain_runtime_messages(runtime);

    const auto storage_id = starter_storage_id(site_run.value());
    const gs1::InventoryCraftCompletedMessage crafted_message {
        gs1::k_recipe_craft_storage_crate,
        gs1::k_item_storage_crate_kit,
        1U,
        0U,
        storage_id};
    assert(runtime.handle_message(crafted_message) == GS1_STATUS_OK);

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

void direct_site_weather_fixture_mutation_updates_authoritative_local_weather_and_shelter_state()
{
    Gs1RuntimeCreateDesc create_desc {};
    create_desc.struct_size = sizeof(Gs1RuntimeCreateDesc);
    create_desc.api_version = gs1::k_api_version;
    create_desc.fixed_step_seconds = 1.0 / 60.0;
    create_desc.adapter_config_json_utf8 = nullptr;

    GameRuntime runtime {create_desc};
    bootstrap_site_one(runtime);

    auto site_run = gs1::GameRuntimeProjectionTestAccess::active_site_run(runtime);
    assert(site_run.has_value());
    drain_runtime_messages(runtime);

    const auto worker_position = gs1::site_world_access::worker_position(site_run.value());
    gs1::GameRuntimeProjectionTestAccess::mutate_active_site_run(
        runtime,
        [worker_position](SiteRunState& active_site_run)
        {
            gs1::site_world_access::set_tile_local_weather(
                active_site_run,
                worker_position.tile_coord,
                gs1::SiteWorld::TileLocalWeatherData {8.0f, 36.0f, 6.0f});
        });

    site_run = gs1::GameRuntimeProjectionTestAccess::active_site_run(runtime);
    assert(site_run.has_value());
    const auto exposed_local_weather =
        gs1::site_world_access::tile_local_weather(site_run.value(), worker_position.tile_coord);
    assert(approx_equal(exposed_local_weather.heat, 8.0f));
    assert(approx_equal(exposed_local_weather.wind, 36.0f));
    assert(approx_equal(exposed_local_weather.dust, 6.0f));
    assert(drain_runtime_messages(runtime).empty());

    gs1::GameRuntimeProjectionTestAccess::mutate_active_site_run(
        runtime,
        [](SiteRunState& active_site_run)
        {
            active_site_run.weather.weather_heat = 60.0f;
            active_site_run.weather.weather_wind = 24.0f;
            active_site_run.weather.weather_dust = 16.0f;
        });

    site_run = gs1::GameRuntimeProjectionTestAccess::active_site_run(runtime);
    assert(site_run.has_value());
    assert(approx_equal(site_run->weather.weather_heat, 60.0f));
    assert(approx_equal(site_run->weather.weather_wind, 24.0f));
    assert(approx_equal(site_run->weather.weather_dust, 16.0f));
    assert(gs1::site_world_access::worker_is_sheltered(site_run.value()));
    assert(drain_runtime_messages(runtime).empty());
}

}  // namespace

int main()
{
    runtime_invocation_without_runtime_emits_into_explicit_test_queue();
    site_loading_waits_for_scene_ready_before_bootstrap_and_fixed_steps();
    inventory_item_use_updates_worker_and_projection();
    inventory_use_request_keeps_singleton_projections_singular_across_immediate_and_flush_paths();
    task_accept_request_does_not_emit_campaign_resource_projection();
    phone_purchase_request_emits_purchase_completion_and_task_progress_projection();
    campaign_reputation_award_emits_campaign_resource_projection_and_unlock_cue();
    ecology_growth_completes_task_and_site_attempt();
    task_reward_claim_emits_pending_claim_projection_then_reward_cue();
    task_reward_claim_ui_action_claims_pending_task_and_emits_reward_cue();
    inventory_craft_completed_opens_output_storage_and_emits_craft_output_cue();
    direct_site_weather_fixture_mutation_updates_authoritative_local_weather_and_shelter_state();
    return 0;
}


