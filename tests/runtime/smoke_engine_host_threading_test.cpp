#include <algorithm>
#include "smoke_engine_host.h"

#include <cassert>
#include <cstring>
#include <deque>
#include <string>
#include <vector>

namespace
{
struct FakeRuntimeState final
{
    std::deque<Gs1EngineMessage> engine_messages {};
    std::vector<Gs1HostEvent> submitted_host_events {};
    std::vector<Gs1FeedbackEvent> submitted_feedback_events {};
    std::uint32_t last_submitted_host_event_count {0};
    std::uint32_t last_submitted_feedback_event_count {0};
    std::uint32_t run_phase1_call_count {0};
    std::uint32_t run_phase2_call_count {0};
};

FakeRuntimeState& fake_runtime(Gs1RuntimeHandle* runtime) noexcept
{
    assert(runtime != nullptr);
    return *reinterpret_cast<FakeRuntimeState*>(runtime);
}

Gs1Status fake_submit_host_events(
    Gs1RuntimeHandle* runtime,
    const Gs1HostEvent* events,
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

Gs1Status fake_submit_feedback_events(
    Gs1RuntimeHandle* runtime,
    const Gs1FeedbackEvent* events,
    std::uint32_t event_count) noexcept
{
    auto& state = fake_runtime(runtime);
    state.last_submitted_feedback_event_count = event_count;
    for (std::uint32_t index = 0; index < event_count; ++index)
    {
        state.submitted_feedback_events.push_back(events[index]);
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
    out_result->engine_messages_queued = static_cast<std::uint32_t>(state.engine_messages.size());
    out_result->processed_host_event_count = state.last_submitted_host_event_count;
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
    out_result->processed_host_event_count = 0U;
    out_result->processed_feedback_event_count = state.last_submitted_feedback_event_count;
    out_result->engine_messages_queued = static_cast<std::uint32_t>(state.engine_messages.size());
    out_result->reserved = 0U;
    return GS1_STATUS_OK;
}

Gs1Status fake_pop_engine_message(
    Gs1RuntimeHandle* runtime,
    Gs1EngineMessage* out_message) noexcept
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
    api.submit_host_events = &fake_submit_host_events;
    api.submit_feedback_events = &fake_submit_feedback_events;
    api.run_phase1 = &fake_run_phase1;
    api.run_phase2 = &fake_run_phase2;
    api.pop_engine_message = &fake_pop_engine_message;
    return api;
}

Gs1EngineMessage make_set_app_state_message(Gs1AppState app_state)
{
    Gs1EngineMessage message {};
    message.type = GS1_ENGINE_MESSAGE_SET_APP_STATE;
    (void)message.emplace_payload<Gs1EngineMessageSetAppStateData>(Gs1EngineMessageSetAppStateData {app_state});
    return message;
}

Gs1EngineMessage make_log_text_message(Gs1LogLevel level, const char* text)
{
    Gs1EngineMessage message {};
    message.type = GS1_ENGINE_MESSAGE_LOG_TEXT;
    auto& payload = message.emplace_payload<Gs1EngineMessageLogTextData>();
    payload.level = level;
    std::memset(payload.text, 0, sizeof(payload.text));
    const auto copy_length = std::min<std::size_t>(std::strlen(text), sizeof(payload.text) - 1U);
    std::memcpy(payload.text, text, copy_length);
    return message;
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

    Gs1FeedbackEvent feedback {};
    feedback.type = GS1_FEEDBACK_EVENT_TRACE_HIT;
    feedback.data.site_id = 7U;
    feedback.data.subject_id = 8U;
    feedback.data.other_id = 9U;
    feedback.data.code = 10U;
    host.queue_feedback_event(feedback);

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
    assert(runtime_state.submitted_feedback_events.size() == 1U);
    assert(runtime_state.submitted_feedback_events.front().type == GS1_FEEDBACK_EVENT_TRACE_HIT);

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
}  // namespace

int main()
{
    queued_commands_only_publish_after_update();
    move_direction_commands_coalesce_before_the_frame_drains_them();
    return 0;
}
