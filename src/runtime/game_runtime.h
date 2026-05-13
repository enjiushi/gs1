#pragma once

#include "campaign/systems/campaign_flow_system.h"
#include "campaign/systems/campaign_time_system.h"
#include "messages/game_message.h"
#include "runtime/game_state.h"
#include "runtime/game_state_view.h"
#include "runtime/runtime_clock.h"
#include "runtime/system_interface.h"
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
inline constexpr std::uint32_t k_api_version = 7;

class GameRuntime final
{
public:
    explicit GameRuntime(Gs1RuntimeCreateDesc create_desc);

    [[nodiscard]] Gs1Status submit_host_messages(
        const Gs1HostMessage* messages,
        std::uint32_t message_count);
    [[nodiscard]] Gs1Status run_phase1(const Gs1Phase1Request& request, Gs1Phase1Result& out_result);
    [[nodiscard]] Gs1Status run_phase2(const Gs1Phase2Request& request, Gs1Phase2Result& out_result);
    [[nodiscard]] Gs1Status pop_runtime_message(Gs1RuntimeMessage& out_message);
    [[nodiscard]] Gs1Status get_game_state_view(Gs1GameStateView& out_view);
    [[nodiscard]] Gs1Status query_site_tile_view(std::uint32_t tile_index, Gs1SiteTileView& out_tile) const;
    [[nodiscard]] Gs1Status get_profiling_snapshot(Gs1RuntimeProfilingSnapshot& out_snapshot) const noexcept;
    void reset_profiling() noexcept;
    [[nodiscard]] Gs1Status set_profiled_system_enabled(
        Gs1RuntimeProfileSystemId system_id,
        bool enabled) noexcept;
    [[nodiscard]] bool profiled_system_enabled(Gs1RuntimeProfileSystemId system_id) const noexcept;

    [[nodiscard]] GameState& state() noexcept { return state_; }
    [[nodiscard]] const GameState& state() const noexcept { return state_; }
    [[nodiscard]] GameMessageQueue& message_queue() noexcept { return state_.message_queue; }
    [[nodiscard]] const GameMessageQueue& message_queue() const noexcept { return state_.message_queue; }
    [[nodiscard]] std::deque<Gs1RuntimeMessage>& runtime_messages() noexcept { return state_.runtime_messages; }
    [[nodiscard]] const std::deque<Gs1RuntimeMessage>& runtime_messages() const noexcept
    {
        return state_.runtime_messages;
    }
    [[nodiscard]] Gs1Status handle_message(const GameMessage& message);
    friend struct GameRuntimeProjectionTestAccess;
    friend class RuntimeInvocation;

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
    [[nodiscard]] Gs1Status dispatch_queued_messages();
    [[nodiscard]] Gs1Status dispatch_subscribed_host_message(const Gs1HostMessage& message);
    [[nodiscard]] Gs1Status dispatch_subscribed_message(const GameMessage& message);
    void run_fixed_step();
    void copy_timing_snapshot(
        const TimingAccumulator& source,
        Gs1RuntimeTimingStats& destination) const noexcept;

private:
    Gs1RuntimeCreateDesc create_desc_ {};
    std::string adapter_config_json_utf8_ {};
    GameState state_ {};
    RuntimeGameStateViewCache state_view_cache_ {};
    std::deque<Gs1HostMessage> host_messages_ {};
    std::vector<std::unique_ptr<IRuntimeSystem>> systems_ {};
    std::vector<IRuntimeSystem*> fixed_step_systems_ {};
    RuntimeHostMessageSubscribers host_message_subscribers_ {};
    RuntimeGameMessageSubscribers message_subscribers_ {};
    TimingAccumulator phase1_timing_ {};
    TimingAccumulator phase2_timing_ {};
    TimingAccumulator fixed_step_timing_ {};
    std::array<ProfiledSystemState, static_cast<std::size_t>(GS1_RUNTIME_PROFILE_SYSTEM_COUNT)>
        profiled_systems_ {};
    bool boot_initialized_ {false};
};

}  // namespace gs1

