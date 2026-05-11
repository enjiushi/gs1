#include <algorithm>
#include "smoke_engine_host.h"

#include <cassert>
#include <cmath>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace
{
struct FakeRuntimeState final
{
    std::deque<Gs1RuntimeMessage> engine_messages {};
    std::vector<Gs1HostMessage> submitted_host_events {};
    std::uint32_t last_submitted_host_event_count {0};
    std::uint32_t run_phase1_call_count {0};
    std::uint32_t run_phase2_call_count {0};
};

FakeRuntimeState& fake_runtime(Gs1RuntimeHandle* runtime) noexcept
{
    assert(runtime != nullptr);
    return *reinterpret_cast<FakeRuntimeState*>(runtime);
}

Gs1Status fake_submit_host_messages(
    Gs1RuntimeHandle* runtime,
    const Gs1HostMessage* events,
    std::uint32_t event_count) noexcept
{
    auto& state = fake_runtime(runtime);
    state.last_submitted_host_event_count = event_count;
    for (std::uint32_t index = 0; index < event_count; ++index)
    {
        state.submitted_host_events.push_back(events[index]);
    }
    return GS1_STATUS_OK;
}

Gs1Status fake_run_phase1(
    Gs1RuntimeHandle* runtime,
    const Gs1Phase1Request* request,
    Gs1Phase1Result* out_result) noexcept
{
    assert(request != nullptr);
    assert(out_result != nullptr);

    auto& state = fake_runtime(runtime);
    state.run_phase1_call_count += 1U;

    out_result->struct_size = sizeof(Gs1Phase1Result);
    out_result->fixed_steps_executed = request->real_delta_seconds > 0.0 ? 1U : 0U;
    out_result->runtime_messages_queued = static_cast<std::uint32_t>(state.engine_messages.size());
    out_result->processed_host_message_count = state.last_submitted_host_event_count;
    return GS1_STATUS_OK;
}

Gs1Status fake_run_phase2(
    Gs1RuntimeHandle* runtime,
    const Gs1Phase2Request* request,
    Gs1Phase2Result* out_result) noexcept
{
    assert(request != nullptr);
    assert(out_result != nullptr);

    auto& state = fake_runtime(runtime);
    state.run_phase2_call_count += 1U;

    out_result->struct_size = sizeof(Gs1Phase2Result);
    out_result->processed_host_message_count = 0U;
    out_result->reserved0 = 0U;
    out_result->runtime_messages_queued = static_cast<std::uint32_t>(state.engine_messages.size());
    out_result->reserved1 = 0U;
    return GS1_STATUS_OK;
}

Gs1Status fake_pop_runtime_message(
    Gs1RuntimeHandle* runtime,
    Gs1RuntimeMessage* out_message) noexcept
{
    assert(out_message != nullptr);

    auto& state = fake_runtime(runtime);
    if (state.engine_messages.empty())
    {
        return GS1_STATUS_BUFFER_EMPTY;
    }

    *out_message = state.engine_messages.front();
    state.engine_messages.pop_front();
    return GS1_STATUS_OK;
}

Gs1RuntimeApi make_fake_api() noexcept
{
    Gs1RuntimeApi api {};
    api.submit_host_messages = &fake_submit_host_messages;
    api.run_phase1 = &fake_run_phase1;
    api.run_phase2 = &fake_run_phase2;
    api.pop_runtime_message = &fake_pop_runtime_message;
    return api;
}

Gs1RuntimeMessage make_set_app_state_message(Gs1AppState app_state)
{
    Gs1RuntimeMessage message {};
    message.type = GS1_ENGINE_MESSAGE_SET_APP_STATE;
    (void)message.emplace_payload<Gs1EngineMessageSetAppStateData>(Gs1EngineMessageSetAppStateData {app_state});
    return message;
}

Gs1RuntimeMessage make_log_text_message(Gs1LogLevel level, const char* text)
{
    Gs1RuntimeMessage message {};
    message.type = GS1_ENGINE_MESSAGE_LOG_TEXT;
    auto& payload = message.emplace_payload<Gs1EngineMessageLogTextData>();
    payload.level = level;
    std::memset(payload.text, 0, sizeof(payload.text));
    const auto copy_length = std::min<std::size_t>(std::strlen(text), sizeof(payload.text) - 1U);
    std::memcpy(payload.text, text, copy_length);
    return message;
}

Gs1RuntimeMessage make_begin_site_snapshot_message(
    std::uint32_t site_id,
    std::uint32_t site_archetype_id,
    std::uint16_t width,
    std::uint16_t height,
    Gs1ProjectionMode mode)
{
    Gs1RuntimeMessage message {};
    message.type = GS1_ENGINE_MESSAGE_BEGIN_SITE_SNAPSHOT;
    auto& payload = message.emplace_payload<Gs1EngineMessageSiteSnapshotData>();
    payload.site_id = site_id;
    payload.site_archetype_id = site_archetype_id;
    payload.width = width;
    payload.height = height;
    payload.mode = mode;
    return message;
}

Gs1RuntimeMessage make_site_modifier_list_begin_message(
    Gs1ProjectionMode mode,
    std::uint16_t modifier_count)
{
    Gs1RuntimeMessage message {};
    message.type = GS1_ENGINE_MESSAGE_SITE_MODIFIER_LIST_BEGIN;
    auto& payload = message.emplace_payload<Gs1EngineMessageSiteModifierListData>();
    payload.mode = mode;
    payload.reserved0 = 0U;
    payload.modifier_count = modifier_count;
    return message;
}

Gs1RuntimeMessage make_site_modifier_upsert_message(
    std::uint32_t modifier_id,
    std::uint16_t remaining_game_hours,
    std::uint8_t flags)
{
    Gs1RuntimeMessage message {};
    message.type = GS1_ENGINE_MESSAGE_SITE_MODIFIER_UPSERT;
    auto& payload = message.emplace_payload<Gs1EngineMessageSiteModifierData>();
    payload.modifier_id = modifier_id;
    payload.remaining_game_hours = remaining_game_hours;
    payload.flags = flags;
    payload.reserved0 = 0U;
    return message;
}

Gs1RuntimeMessage make_end_site_snapshot_message()
{
    Gs1RuntimeMessage message {};
    message.type = GS1_ENGINE_MESSAGE_END_SITE_SNAPSHOT;
    return message;
}

Gs1RuntimeMessage make_hud_state_message(float money)
{
    Gs1RuntimeMessage message {};
    message.type = GS1_ENGINE_MESSAGE_HUD_STATE;
    auto& payload = message.emplace_payload<Gs1EngineMessageHudStateData>();
    payload.player_health = 90.0f;
    payload.player_hydration = 80.0f;
    payload.player_nourishment = 75.0f;
    payload.player_energy = 70.0f;
    payload.player_morale = 60.0f;
    payload.current_money = money;
    payload.active_task_count = 2U;
    payload.current_action_kind = GS1_SITE_ACTION_NONE;
    payload.site_completion_normalized = 0.25f;
    payload.warning_code = 0U;
    return message;
}

Gs1RuntimeMessage make_campaign_resources_message(
    float money,
    std::uint32_t reputation,
    std::uint32_t village_reputation,
    std::uint32_t forestry_reputation,
    std::uint32_t university_reputation)
{
    Gs1RuntimeMessage message {};
    message.type = GS1_ENGINE_MESSAGE_CAMPAIGN_RESOURCES;
    auto& payload = message.emplace_payload<Gs1EngineMessageCampaignResourcesData>();
    payload.current_money = money;
    payload.total_reputation = reputation;
    payload.village_reputation = village_reputation;
    payload.forestry_reputation = forestry_reputation;
    payload.university_reputation = university_reputation;
    return message;
}

std::string load_text_file(const std::filesystem::path& path)
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream.is_open())
    {
        const auto alt_path = std::filesystem::path("..") / path;
        stream.open(alt_path, std::ios::binary);
    }
    if (!stream.is_open())
    {
        const auto alt_path = std::filesystem::path("..") / std::filesystem::path("..") / path;
        stream.open(alt_path, std::ios::binary);
    }
    assert(stream.is_open());
    return std::string(
        std::istreambuf_iterator<char>(stream),
        std::istreambuf_iterator<char>());
}

void queued_commands_only_publish_after_update()
{
    FakeRuntimeState runtime_state {};
    runtime_state.engine_messages.push_back(make_set_app_state_message(GS1_APP_STATE_MAIN_MENU));
    runtime_state.engine_messages.push_back(make_log_text_message(GS1_LOG_LEVEL_INFO, "queued ok"));

    const auto api = make_fake_api();
    auto* runtime = reinterpret_cast<Gs1RuntimeHandle*>(&runtime_state);
    SmokeEngineHost host {api, runtime, SmokeEngineHost::LogMode::ActivityOnly};

    Gs1UiAction action {};
    action.type = GS1_UI_ACTION_START_NEW_CAMPAIGN;
    action.target_id = 41U;
    host.queue_ui_action(action);

    const auto before_update = host.capture_live_state_snapshot();
    assert(before_update.frame_number == 0U);
    assert(!before_update.current_app_state.has_value());
    assert(before_update.current_frame_message_entries.empty());
    assert(host.consume_pending_live_state_patches().empty());

    host.update(1.0 / 60.0);

    assert(runtime_state.run_phase1_call_count == 1U);
    assert(runtime_state.run_phase2_call_count == 1U);
    assert(runtime_state.submitted_host_events.size() == 1U);
    assert(runtime_state.submitted_host_events.front().type == GS1_HOST_EVENT_UI_ACTION);
    assert(runtime_state.submitted_host_events.front().payload.ui_action.action.type == GS1_UI_ACTION_START_NEW_CAMPAIGN);

    const auto after_update = host.capture_live_state_snapshot();
    assert(after_update.frame_number == 1U);
    assert(after_update.current_app_state.has_value());
    assert(after_update.current_app_state.value() == GS1_APP_STATE_MAIN_MENU);
    assert(after_update.current_frame_message_entries.size() == 2U);
    assert(after_update.message_log_tail.size() == 2U);
    assert(after_update.message_log_tail.front().find("SET_APP_STATE") != std::string::npos);

    const auto patches = host.consume_pending_live_state_patches();
    assert(patches.size() == 1U);
    assert(!patches.front().empty());

    host.queue_ui_action(action);
    const auto still_published = host.capture_live_state_snapshot();
    assert(still_published.frame_number == 1U);
    assert(still_published.current_app_state == after_update.current_app_state);
}

void move_direction_commands_coalesce_before_the_frame_drains_them()
{
    FakeRuntimeState runtime_state {};
    const auto api = make_fake_api();
    auto* runtime = reinterpret_cast<Gs1RuntimeHandle*>(&runtime_state);
    SmokeEngineHost host {api, runtime, SmokeEngineHost::LogMode::ActivityOnly};

    host.queue_site_move_direction(1.0f, 2.0f, 3.0f);
    host.queue_site_move_direction(4.0f, 5.0f, 6.0f);

    host.update(0.0);

    assert(runtime_state.submitted_host_events.size() == 1U);
    const auto& move_event = runtime_state.submitted_host_events.front();
    assert(move_event.type == GS1_HOST_EVENT_SITE_MOVE_DIRECTION);
    assert(move_event.payload.site_move_direction.world_move_x == 4.0f);
    assert(move_event.payload.site_move_direction.world_move_y == 5.0f);
    assert(move_event.payload.site_move_direction.world_move_z == 6.0f);
}

void update_records_frame_timing_breakdown()
{
    FakeRuntimeState runtime_state {};
    runtime_state.engine_messages.push_back(make_set_app_state_message(GS1_APP_STATE_MAIN_MENU));

    const auto api = make_fake_api();
    auto* runtime = reinterpret_cast<Gs1RuntimeHandle*>(&runtime_state);
    SmokeEngineHost host {api, runtime, SmokeEngineHost::LogMode::ActivityOnly};

    host.update(1.0 / 60.0);

    const auto timing = host.last_frame_timing();
    assert(timing.total_update_seconds >= 0.0);
    assert(timing.host_update_seconds >= 0.0);
    assert(timing.gameplay_dll_seconds >= 0.0);
    assert((timing.total_update_seconds + 1e-9) >= timing.host_update_seconds);
    assert((timing.total_update_seconds + 1e-9) >= timing.gameplay_dll_seconds);
    const double reconstructed_total = timing.host_update_seconds + timing.gameplay_dll_seconds;
    assert(std::abs(reconstructed_total - timing.total_update_seconds) < 0.01);
}

void modifier_patch_publishes_remaining_game_hours()
{
    FakeRuntimeState runtime_state {};
    runtime_state.engine_messages.push_back(
        make_begin_site_snapshot_message(1U, 101U, 32U, 32U, GS1_PROJECTION_MODE_SNAPSHOT));
    runtime_state.engine_messages.push_back(
        make_site_modifier_list_begin_message(GS1_PROJECTION_MODE_SNAPSHOT, 1U));
    runtime_state.engine_messages.push_back(
        make_site_modifier_upsert_message(3003U, 8U, GS1_SITE_MODIFIER_FLAG_TIMED));
    runtime_state.engine_messages.push_back(make_end_site_snapshot_message());

    const auto api = make_fake_api();
    auto* runtime = reinterpret_cast<Gs1RuntimeHandle*>(&runtime_state);
    SmokeEngineHost host {api, runtime, SmokeEngineHost::LogMode::ActivityOnly};

    host.update(0.0);
    const auto initial_snapshot = host.capture_live_state_snapshot();
    assert(initial_snapshot.active_site_snapshot.has_value());
    assert(initial_snapshot.active_site_snapshot->active_modifiers.size() == 1U);
    assert(initial_snapshot.active_site_snapshot->active_modifiers.front().modifier_id == 3003U);
    assert(initial_snapshot.active_site_snapshot->active_modifiers.front().remaining_game_hours == 8U);
    (void)host.consume_pending_live_state_patches();

    runtime_state.engine_messages.push_back(
        make_begin_site_snapshot_message(1U, 101U, 32U, 32U, GS1_PROJECTION_MODE_DELTA));
    runtime_state.engine_messages.push_back(
        make_site_modifier_list_begin_message(GS1_PROJECTION_MODE_DELTA, 1U));
    runtime_state.engine_messages.push_back(
        make_site_modifier_upsert_message(3003U, 7U, GS1_SITE_MODIFIER_FLAG_TIMED));
    runtime_state.engine_messages.push_back(make_end_site_snapshot_message());

    host.update(0.0);
    const auto updated_snapshot = host.capture_live_state_snapshot();
    assert(updated_snapshot.active_site_snapshot.has_value());
    assert(updated_snapshot.active_site_snapshot->active_modifiers.size() == 1U);
    assert(updated_snapshot.active_site_snapshot->active_modifiers.front().remaining_game_hours == 7U);

    const auto patches = host.consume_pending_live_state_patches();
    assert(patches.size() == 1U);
    assert(patches.front().find("\"siteStatePatch\"") != std::string::npos);
    assert(patches.front().find("\"activeModifiers\":[") != std::string::npos);
    assert(patches.front().find("\"modifierId\":3003") != std::string::npos);
    assert(patches.front().find("\"remainingGameHours\":7") != std::string::npos);
}

void repeated_hud_and_campaign_messages_publish_one_coalesced_patch_per_frame()
{
    FakeRuntimeState runtime_state {};
    runtime_state.engine_messages.push_back(make_hud_state_message(10.0f));
    runtime_state.engine_messages.push_back(make_campaign_resources_message(10.0f, 5U, 200U, 400U, 600U));
    runtime_state.engine_messages.push_back(make_hud_state_message(12.5f));
    runtime_state.engine_messages.push_back(make_campaign_resources_message(12.5f, 6U, 300U, 500U, 700U));

    const auto api = make_fake_api();
    auto* runtime = reinterpret_cast<Gs1RuntimeHandle*>(&runtime_state);
    SmokeEngineHost host {api, runtime, SmokeEngineHost::LogMode::ActivityOnly};

    host.update(0.0);

    const auto snapshot = host.capture_live_state_snapshot();
    assert(snapshot.hud_state.has_value());
    assert(snapshot.campaign_resources.has_value());
    assert(std::abs(snapshot.hud_state->player_nourishment - 75.0f) < 0.0001f);
    assert(std::abs(snapshot.hud_state->current_money - 12.5f) < 0.0001f);
    assert(std::abs(snapshot.campaign_resources->current_money - 12.5f) < 0.0001f);
    assert(snapshot.campaign_resources->total_reputation == 6U);
    assert(snapshot.campaign_resources->village_reputation == 300U);
    assert(snapshot.campaign_resources->forestry_reputation == 500U);
    assert(snapshot.campaign_resources->university_reputation == 700U);

    const auto patches = host.consume_pending_live_state_patches();
    assert(patches.size() == 1U);
    assert(patches.front().find("\"hud\":") != std::string::npos);
    assert(patches.front().find("\"campaignResources\":") != std::string::npos);
    assert(patches.front().find("\"playerNourishment\":75.000000") != std::string::npos);
    assert(patches.front().find("\"currentMoney\":12.500000") != std::string::npos);
    assert(patches.front().find("\"totalReputation\":6") != std::string::npos);
    assert(patches.front().find("\"villageReputation\":300") != std::string::npos);
    assert(patches.front().find("\"forestryReputation\":500") != std::string::npos);
    assert(patches.front().find("\"universityReputation\":700") != std::string::npos);
}

void visual_smoke_assets_keep_hidden_regional_map_panels_collapsed()
{
    const auto html = load_text_file(std::filesystem::path("tests/smoke/visual_smoke_live.html"));
    const auto viewer = load_text_file(std::filesystem::path("tests/smoke/visual_smoke_live_viewer.js"));

    assert(html.find(".site-vitals-money[hidden]") != std::string::npos);
    assert(html.find(".site-vitals-reputation[hidden]") != std::string::npos);
    assert(html.find(".site-task-panel[hidden]") != std::string::npos);
    assert(html.find(".modifier-strip[hidden]") != std::string::npos);
    assert(html.find("id=\"site-tech-tree-actions\"") != std::string::npos);
    assert(html.find("--shell-height: 100dvh;") != std::string::npos);
    assert(html.find("@media (max-width: 1366px), (max-height: 900px)") != std::string::npos);
    assert(html.find("@media (max-height: 820px)") != std::string::npos);
    assert(html.find("width: var(--phone-width);") != std::string::npos);

    assert(viewer.find("siteVitalsMoney.hidden = !showRegionalMapCash;") != std::string::npos);
    assert(viewer.find("playerNourishment") != std::string::npos);
    assert(viewer.find("{ label: \"Nourishment\", percent: clampMeterPercent(nourishment), className: \"nourishment\" }") != std::string::npos);
    assert(viewer.find("if (state.appState !== \"SITE_ACTIVE\") {") != std::string::npos);
    assert(viewer.find("renderSiteTaskPanel(null);") != std::string::npos);
    assert(viewer.find("renderSiteModifiers(null);") != std::string::npos);
    assert(viewer.find("postInventorySlotTap(slot).catch(() => {") != std::string::npos);
    assert(viewer.find("Click to act on this stack") != std::string::npos);
    assert(viewer.find("Click to select and use/store") == std::string::npos);
    assert(viewer.find("siteTechTreeActions.appendChild(") != std::string::npos);
    assert(viewer.find("function getViewportDimensions() {") != std::string::npos);
    assert(viewer.find("const gameViewBounds = gameView.getBoundingClientRect();") != std::string::npos);
    assert(viewer.find("window.visualViewport.addEventListener(\"resize\", scheduleFitRenderer);") != std::string::npos);
    assert(viewer.find("const viewResizeObserver = new ResizeObserver(function () {") != std::string::npos);
    assert(viewer.find("function resolveTileContextPanelTop(") != std::string::npos);
    assert(viewer.find("return parentElement.offsetTop + hoveredElement.offsetTop;") != std::string::npos);
    assert(viewer.find("clampTileContextPanelTop(preferredTop, estimatedHeight, viewportHeight)") != std::string::npos);
}
}  // namespace

int main()
{
    queued_commands_only_publish_after_update();
    move_direction_commands_coalesce_before_the_frame_drains_them();
    update_records_frame_timing_breakdown();
    modifier_patch_publishes_remaining_game_hours();
    repeated_hud_and_campaign_messages_publish_one_coalesced_patch_per_frame();
    visual_smoke_assets_keep_hidden_regional_map_panels_collapsed();
    return 0;
}

