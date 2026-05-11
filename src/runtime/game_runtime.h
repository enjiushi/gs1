#pragma once

#include "app/game_presentation_coordinator.h"
#include "campaign/campaign_state.h"
#include "campaign/systems/campaign_flow_system.h"
#include "campaign/systems/campaign_system_context.h"
#include "campaign/systems/campaign_time_system.h"
#include "messages/game_message.h"
#include "runtime/runtime_clock.h"
#include "runtime/system_interface.h"
#include "site/site_run_state.h"
#include "site/systems/site_system_context.h"
#include "gs1/status.h"
#include "gs1/types.h"

#include <array>
#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace gs1
{
inline constexpr std::uint32_t k_api_version = 6;

class GameRuntime final
{
public:
    explicit GameRuntime(Gs1RuntimeCreateDesc create_desc);

    [[nodiscard]] Gs1Status submit_host_messages(
        const Gs1HostMessage* messages,
        std::uint32_t message_count);
    [[nodiscard]] Gs1Status submit_feedback_events(
        const Gs1FeedbackEvent* events,
        std::uint32_t event_count);
    [[nodiscard]] Gs1Status run_phase1(const Gs1Phase1Request& request, Gs1Phase1Result& out_result);
    [[nodiscard]] Gs1Status run_phase2(const Gs1Phase2Request& request, Gs1Phase2Result& out_result);
    [[nodiscard]] Gs1Status pop_runtime_message(Gs1RuntimeMessage& out_message);
    [[nodiscard]] Gs1Status get_profiling_snapshot(Gs1RuntimeProfilingSnapshot& out_snapshot) const noexcept;
    void reset_profiling() noexcept;
    [[nodiscard]] Gs1Status set_profiled_system_enabled(
        Gs1RuntimeProfileSystemId system_id,
        bool enabled) noexcept;
    [[nodiscard]] bool profiled_system_enabled(Gs1RuntimeProfileSystemId system_id) const noexcept;

    [[nodiscard]] GameMessageQueue& message_queue() noexcept { return message_queue_; }
    [[nodiscard]] Gs1Status handle_message(const GameMessage& message);

    friend struct GameRuntimeProjectionTestAccess;
    friend class MessageDispatcher;
    friend class GameRuntimeTempBridge;

private:
    struct TimingAccumulator final
    {
        std::uint64_t sample_count {0U};
        double last_elapsed_ms {0.0};
        double total_elapsed_ms {0.0};
        double max_elapsed_ms {0.0};
    };

    struct ProfiledSystemState final
    {
        bool enabled {true};
        TimingAccumulator run_timing {};
        TimingAccumulator message_timing {};
    };

    void initialize_system_registry();
    [[nodiscard]] Gs1Status dispatch_host_messages(std::uint32_t& out_processed_count);
    [[nodiscard]] Gs1Status dispatch_feedback_events(std::uint32_t& out_processed_count);
    [[nodiscard]] Gs1Status dispatch_queued_messages();
    [[nodiscard]] Gs1Status dispatch_subscribed_host_message(const Gs1HostMessage& message);
    [[nodiscard]] Gs1Status dispatch_subscribed_message(const GameMessage& message);
    [[nodiscard]] Gs1Status dispatch_subscribed_feedback_event(const Gs1FeedbackEvent& event);
    void run_fixed_step();
    void copy_timing_snapshot(
        const TimingAccumulator& source,
        Gs1RuntimeTimingStats& destination) const noexcept;

private:
    Gs1RuntimeCreateDesc create_desc_ {};
    std::string adapter_config_json_utf8_ {};
    double fixed_step_seconds_ {k_default_fixed_step_seconds};
    Gs1AppState app_state_ {GS1_APP_STATE_BOOT};
    std::optional<CampaignState> campaign_ {};
    std::optional<SiteRunState> active_site_run_ {};
    std::deque<Gs1HostMessage> host_messages_ {};
    std::deque<Gs1FeedbackEvent> feedback_events_ {};
    GameMessageQueue message_queue_ {};
    std::vector<std::unique_ptr<IRuntimeSystem>> systems_ {};
    std::vector<IRuntimeSystem*> fixed_step_systems_ {};
    RuntimeHostMessageSubscribers host_message_subscribers_ {};
    RuntimeGameMessageSubscribers message_subscribers_ {};
    RuntimeFeedbackEventSubscribers feedback_event_subscribers_ {};
    std::deque<Gs1RuntimeMessage> runtime_messages_ {};
    TimingAccumulator phase1_timing_ {};
    TimingAccumulator phase2_timing_ {};
    TimingAccumulator fixed_step_timing_ {};
    std::array<ProfiledSystemState, static_cast<std::size_t>(GS1_RUNTIME_PROFILE_SYSTEM_COUNT)>
        profiled_systems_ {};
    GamePresentationCoordinator presentation_ {};
    bool boot_initialized_ {false};
};

template <typename Fn>
Gs1Status GameRuntimeTempBridge::with_campaign_flow_message_context(Fn&& fn)
{
    CampaignFlowMessageContext context {
        runtime_.campaign_,
        runtime_.active_site_run_,
        runtime_.app_state_,
        runtime_.message_queue_};
    return fn(context);
}

template <typename Fn>
Gs1Status GameRuntimeTempBridge::with_campaign_context(Fn&& fn)
{
    if (!runtime_.campaign_.has_value())
    {
        return GS1_STATUS_OK;
    }

    CampaignSystemContext context {*runtime_.campaign_};
    return fn(context);
}

template <typename Fn>
void GameRuntimeTempBridge::with_campaign_fixed_step_context(Fn&& fn)
{
    if (!runtime_.campaign_.has_value())
    {
        return;
    }

    CampaignFixedStepContext context {*runtime_.campaign_, runtime_.fixed_step_seconds_};
    fn(context);
}

template <typename SystemTag, typename Fn>
Gs1Status GameRuntimeTempBridge::with_site_context(
    Fn&& fn,
    SiteMoveDirectionInput move_direction,
    bool missing_context_is_ok)
{
    if (!runtime_.campaign_.has_value() || !runtime_.active_site_run_.has_value())
    {
        return missing_context_is_ok ? GS1_STATUS_OK : GS1_STATUS_INVALID_STATE;
    }

    auto context = make_site_system_context<SystemTag>(
        *runtime_.campaign_,
        *runtime_.active_site_run_,
        runtime_.message_queue_,
        runtime_.fixed_step_seconds_,
        move_direction);
    return fn(context);
}

template <typename Fn>
Gs1Status GameRuntimeTempBridge::with_presentation_context(Fn&& fn)
{
    GamePresentationRuntimeContext context {
        runtime_.app_state_,
        runtime_.campaign_,
        runtime_.active_site_run_,
        runtime_.message_queue_,
        runtime_.runtime_messages_,
        runtime_.fixed_step_seconds_};
    return fn(context);
}

}  // namespace gs1

