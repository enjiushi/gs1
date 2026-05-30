#include "runtime/game_runtime.h"
#include "content/defs/faction_defs.h"
#include "content/defs/item_defs.h"
#include "content/defs/plant_defs.h"
#include "content/defs/progression_defs.h"
#include "messages/game_message.h"
#include "site/site_world_access.h"
#include "../system/source/split_state_test_helpers.h"

#include <cassert>
#include <cstdint>
#include <vector>

namespace gs1
{
struct GameRuntimeProjectionTestAccess
{
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

    template <typename Func>
    static void mutate_active_site_run(GameRuntime& runtime, Func&& func)
    {
        auto site_run = active_site_run(runtime);
        assert(site_run.has_value());
        func(site_run.value());
        write_site_run_state_to_state_sets(site_run.value(), runtime.state(), runtime.state_manager());
        runtime.site_world_ = site_run->site_world;
    }

};
}  // namespace gs1

namespace
{
using gs1::GameRuntime;
using gs1::ProgressionEventOccurredMessage;
using gs1::SiteRunState;
using gs1::StartNewCampaignMessage;
using gs1::StartSiteAttemptMessage;
using gs1::TileCoord;

StartNewCampaignMessage make_start_campaign_message()
{
    return StartNewCampaignMessage {42ULL, 30U};
}

StartSiteAttemptMessage make_start_site_attempt_message(std::uint32_t site_id)
{
    return StartSiteAttemptMessage {site_id};
}

Gs1HostMessage make_site_scene_ready_event()
{
    Gs1HostMessage event {};
    event.type = GS1_HOST_EVENT_SITE_SCENE_READY;
    return event;
}

void run_phase2(GameRuntime& runtime)
{
    Gs1Phase2Request request {};
    request.struct_size = sizeof(Gs1Phase2Request);
    Gs1Phase2Result result {};
    assert(runtime.run_phase2(request, result) == GS1_STATUS_OK);
}

Gs1GameStateView read_view(GameRuntime& runtime)
{
    Gs1GameStateView view {};
    view.struct_size = sizeof(Gs1GameStateView);
    assert(runtime.get_game_state_view(view) == GS1_STATUS_OK);
    return view;
}

void bootstrap_site_one(GameRuntime& runtime)
{
    assert(runtime.handle_message(make_start_campaign_message()) == GS1_STATUS_OK);

    auto campaign_view = read_view(runtime);
    assert(campaign_view.has_campaign == 1U);
    assert(campaign_view.campaign != nullptr);
    assert(campaign_view.campaign->site_count > 0U);

    const auto site_id = campaign_view.campaign->sites[0].site_id;
    const auto start_site_status = runtime.handle_message(make_start_site_attempt_message(site_id));
    assert(start_site_status == GS1_STATUS_OK);

    const auto ready_event = make_site_scene_ready_event();
    assert(runtime.submit_host_messages(&ready_event, 1U) == GS1_STATUS_OK);
    run_phase2(runtime);
    (void)read_view(runtime);
}

void campaign_and_site_state_view_exposes_current_runtime_state()
{
    Gs1RuntimeCreateDesc create_desc {};
    create_desc.struct_size = sizeof(Gs1RuntimeCreateDesc);
    create_desc.api_version = gs1::k_api_version;
    create_desc.fixed_step_seconds = 1.0 / 60.0;
    create_desc.adapter_config_json_utf8 = nullptr;

    GameRuntime runtime {create_desc};
    bootstrap_site_one(runtime);

    gs1::GameRuntimeProjectionTestAccess::mutate_active_site_run(
        runtime,
        [](SiteRunState& site_run)
        {
            site_run.inventory.worker_pack_slots[0].occupied = true;
            site_run.inventory.worker_pack_slots[0].item_id = gs1::ItemId {gs1::k_item_medicine_pack};
            site_run.inventory.worker_pack_slots[0].item_quantity = 2U;
            site_run.inventory.worker_pack_slots[0].item_condition = 0.75f;
            site_run.inventory.worker_pack_slots[0].item_freshness = 0.9f;

            auto ecology = gs1::site_world_access::tile_ecology(site_run, TileCoord {0, 0});
            ecology.plant_id = gs1::PlantId {gs1::k_plant_ordos_wormwood};
            ecology.plant_density = 66.0f;
            gs1::site_world_access::set_tile_ecology(site_run, TileCoord {0, 0}, ecology);
        });

    const auto view = read_view(runtime);
    assert(view.app_state == GS1_APP_STATE_SITE_ACTIVE);
    assert(view.has_campaign == 1U);
    assert(view.has_active_site == 1U);
    assert(view.campaign != nullptr);
    assert(view.active_site != nullptr);

    const auto& campaign = *view.campaign;
    const auto& site = *view.active_site;
    assert(campaign.site_count > 0U);
    assert(site.storage_slot_count > 0U);
    assert(site.storage_slots != nullptr);

    bool found_worker_pack_item = false;
    for (std::uint32_t index = 0; index < site.storage_slot_count; ++index)
    {
        const auto& slot = site.storage_slots[index];
        if (slot.storage_id == site.worker_pack_storage_id && slot.slot_index == 0U)
        {
            found_worker_pack_item = true;
            assert(slot.occupied == 1U);
            assert(slot.item_id == gs1::k_item_medicine_pack);
            assert(slot.quantity == 2U);
            break;
        }
    }
    assert(found_worker_pack_item);

    Gs1SiteTileView tile_view {};
    assert(runtime.query_site_tile_view(0U, tile_view) == GS1_STATUS_OK);
    assert(tile_view.tile_x == 0);
    assert(tile_view.tile_y == 0);
    assert(tile_view.plant_id == gs1::k_plant_ordos_wormwood);
    assert(tile_view.plant_density == 66.0f);
}

void progression_events_update_campaign_view_and_progression_entries()
{
    Gs1RuntimeCreateDesc create_desc {};
    create_desc.struct_size = sizeof(Gs1RuntimeCreateDesc);
    create_desc.api_version = gs1::k_api_version;
    create_desc.fixed_step_seconds = 1.0 / 60.0;
    create_desc.adapter_config_json_utf8 = nullptr;

    GameRuntime runtime {create_desc};
    assert(runtime.handle_message(make_start_campaign_message()) == GS1_STATUS_OK);

    assert(runtime.handle_message(ProgressionEventOccurredMessage {
        gs1::k_progression_event_campaign_reputation_reward,
        0U,
        100}) == GS1_STATUS_OK);

    const auto view = read_view(runtime);
    assert(view.has_campaign == 1U);
    assert(view.campaign != nullptr);
    assert(view.campaign->total_reputation == 100U);
    assert(view.campaign->progression_entry_count > 0U);
    assert(view.campaign->progression_entries != nullptr);

    bool found_unlocked_entry = false;
    for (std::uint32_t index = 0; index < view.campaign->progression_entry_count; ++index)
    {
        const auto& entry = view.campaign->progression_entries[index];
        if ((entry.flags & GS1_PROGRESSION_ENTRY_FLAG_UNLOCKED) != 0U)
        {
            found_unlocked_entry = true;
            break;
        }
    }

    assert(found_unlocked_entry);
}

void translated_site_host_messages_mutate_authoritative_runtime_state()
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
    const auto worker_position_before = gs1::site_world_access::worker_position(site_run.value());

    gs1::GameRuntimeProjectionTestAccess::mutate_active_site_run(
        runtime,
        [](SiteRunState& active_site_run)
        {
            auto ecology = gs1::site_world_access::tile_ecology(active_site_run, TileCoord {0, 0});
            ecology.plant_id = gs1::PlantId {gs1::k_plant_ordos_wormwood};
            ecology.plant_density = 80.0f;
            gs1::site_world_access::set_tile_ecology(active_site_run, TileCoord {0, 0}, ecology);
        });

    {
        Gs1HostMessage move_event {};
        move_event.type = GS1_HOST_EVENT_SITE_MOVE_DIRECTION;
        move_event.payload.site_move_direction.world_move_x = 1.0f;
        move_event.payload.site_move_direction.world_move_y = 0.0f;
        move_event.payload.site_move_direction.world_move_z = 0.0f;
        assert(runtime.submit_host_messages(&move_event, 1U) == GS1_STATUS_OK);

        Gs1Phase1Request request {};
        request.struct_size = sizeof(Gs1Phase1Request);
        request.real_delta_seconds = 1.0 / 60.0;
        Gs1Phase1Result result {};
        assert(runtime.run_phase1(request, result) == GS1_STATUS_OK);
        assert(result.processed_host_message_count == 1U);
        assert(result.fixed_steps_executed == 1U);
    }

    site_run = gs1::GameRuntimeProjectionTestAccess::active_site_run(runtime);
    assert(site_run.has_value());
    const auto worker_position_after = gs1::site_world_access::worker_position(site_run.value());
    assert(worker_position_after.tile_x > worker_position_before.tile_x);

    {
        Gs1HostMessage context_event {};
        context_event.type = GS1_HOST_EVENT_SITE_CONTEXT_REQUEST;
        context_event.payload.site_context_request.tile_x = 1;
        context_event.payload.site_context_request.tile_y = 0;
        assert(runtime.submit_host_messages(&context_event, 1U) == GS1_STATUS_OK);
        run_phase2(runtime);
    }

    site_run = gs1::GameRuntimeProjectionTestAccess::active_site_run(runtime);
    assert(site_run.has_value());
    assert(site_run->craft.context.occupied);
    assert(site_run->craft.context.tile_coord.x == 1);
    assert(site_run->craft.context.tile_coord.y == 0);

    {
        Gs1HostMessage tap_event {};
        tap_event.type = GS1_HOST_EVENT_SITE_INVENTORY_SLOT_TAP;
        tap_event.payload.site_inventory_slot_tap.storage_id = site_run->inventory.worker_pack_storage_id;
        tap_event.payload.site_inventory_slot_tap.slot_index = 0U;
        tap_event.payload.site_inventory_slot_tap.container_kind = GS1_INVENTORY_CONTAINER_WORKER_PACK;
        assert(runtime.submit_host_messages(&tap_event, 1U) == GS1_STATUS_OK);
        run_phase2(runtime);
    }

    site_run = gs1::GameRuntimeProjectionTestAccess::active_site_run(runtime);
    assert(site_run.has_value());

    {
        Gs1HostMessage action_event {};
        action_event.type = GS1_HOST_EVENT_SITE_ACTION_REQUEST;
        action_event.payload.site_action_request.action_kind = GS1_SITE_ACTION_WATER;
        action_event.payload.site_action_request.target_tile_x = 0;
        action_event.payload.site_action_request.target_tile_y = 0;
        action_event.payload.site_action_request.quantity = 1U;
        assert(runtime.submit_host_messages(&action_event, 1U) == GS1_STATUS_OK);
        run_phase2(runtime);
    }

    site_run = gs1::GameRuntimeProjectionTestAccess::active_site_run(runtime);
    assert(site_run.has_value());
    assert(site_run->site_action.current_action_id.has_value());
    assert(site_run->site_action.action_kind == gs1::ActionKind::Water);

    {
        Gs1HostMessage cancel_event {};
        cancel_event.type = GS1_HOST_EVENT_SITE_ACTION_CANCEL;
        cancel_event.payload.site_action_cancel.flags = GS1_SITE_ACTION_CANCEL_FLAG_CURRENT_ACTION;
        assert(runtime.submit_host_messages(&cancel_event, 1U) == GS1_STATUS_OK);
        run_phase2(runtime);
    }

    site_run = gs1::GameRuntimeProjectionTestAccess::active_site_run(runtime);
    assert(site_run.has_value());
    assert(!site_run->site_action.current_action_id.has_value());
    assert(site_run->site_action.action_kind == gs1::ActionKind::None);
}
}  // namespace

int main()
{
    campaign_and_site_state_view_exposes_current_runtime_state();
    progression_events_update_campaign_view_and_progression_entries();
    translated_site_host_messages_mutate_authoritative_runtime_state();
    return 0;
}
