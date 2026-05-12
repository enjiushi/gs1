#include "runtime/game_runtime.h"
#include "content/defs/item_defs.h"

#include <cassert>
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
}

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

    static void flush_projection(GameRuntime& runtime)
    {
        auto context = presentation_context(runtime);
        runtime.presentation_.flush_site_presentation_if_dirty(context);
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

Gs1HostMessage make_site_scene_ready_event()
{
    Gs1HostMessage event {};
    event.type = GS1_HOST_EVENT_SITE_SCENE_READY;
    return event;
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

const Gs1RuntimeMessage* find_site_modifier_message(
    const std::vector<Gs1RuntimeMessage>& messages,
    std::uint32_t modifier_id)
{
    for (const auto& message : messages)
    {
        if (message.type != GS1_ENGINE_MESSAGE_SITE_MODIFIER_UPSERT)
        {
            continue;
        }

        const auto& payload = message.payload_as<Gs1EngineMessageSiteModifierData>();
        if (payload.modifier_id == modifier_id)
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
}

void timed_modifier_projection_only_republishes_on_game_hour_boundaries()
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

    site_run.inventory.worker_pack_slots[0].occupied = true;
    site_run.inventory.worker_pack_slots[0].item_id = gs1::ItemId {gs1::k_item_focus_tonic};
    site_run.inventory.worker_pack_slots[0].item_quantity = 1U;
    site_run.inventory.worker_pack_slots[0].item_condition = 1.0f;
    site_run.inventory.worker_pack_slots[0].item_freshness = 1.0f;

    assert(runtime.handle_message(make_inventory_use_message(
               gs1::k_item_focus_tonic,
               1U,
               site_run.inventory.worker_pack_storage_id,
               0U)) == GS1_STATUS_OK);
    gs1::GameRuntimeProjectionTestAccess::flush_projection(runtime);

    const auto activation_messages = drain_runtime_messages(runtime);
    const auto* activation_modifier_message =
        find_site_modifier_message(activation_messages, 3003U);
    assert(activation_modifier_message != nullptr);
    assert(
        activation_modifier_message->payload_as<Gs1EngineMessageSiteModifierData>()
            .remaining_game_hours == 16U);

    Gs1Phase1Result first_tick_result {};
    run_phase1(runtime, 60.0, first_tick_result);
    const auto first_tick_messages = drain_runtime_messages(runtime);
    assert(find_site_modifier_message(first_tick_messages, 3003U) == nullptr);

    Gs1Phase1Result second_tick_result {};
    run_phase1(runtime, 60.0, second_tick_result);
    const auto second_tick_messages = drain_runtime_messages(runtime);
    const auto* hour_boundary_modifier_message =
        find_site_modifier_message(second_tick_messages, 3003U);
    assert(hour_boundary_modifier_message != nullptr);
    assert(
        hour_boundary_modifier_message->payload_as<Gs1EngineMessageSiteModifierData>()
            .remaining_game_hours == 15U);
}
}  // namespace

int main()
{
    timed_modifier_projection_only_republishes_on_game_hour_boundaries();
    return 0;
}


