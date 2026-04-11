#pragma once

#include "campaign/campaign_state.h"
#include "commands/game_command.h"
#include "site/site_run_state.h"
#include "gs1/status.h"
#include "gs1/types.h"
#include "runtime/input_snapshot.h"

#include <deque>
#include <map>
#include <optional>

namespace gs1
{
inline constexpr std::uint32_t k_api_version = 1;
inline constexpr double k_default_fixed_step_seconds = 0.25;

class GameRuntime final
{
public:
    explicit GameRuntime(Gs1RuntimeCreateDesc create_desc);

    [[nodiscard]] Gs1Status submit_host_events(
        const Gs1HostEvent* events,
        std::uint32_t event_count);
    [[nodiscard]] Gs1Status submit_feedback_events(
        const Gs1FeedbackEvent* events,
        std::uint32_t event_count);
    [[nodiscard]] Gs1Status run_phase1(const Gs1Phase1Request& request, Gs1Phase1Result& out_result);
    [[nodiscard]] Gs1Status run_phase2(const Gs1Phase2Request& request, Gs1Phase2Result& out_result);
    [[nodiscard]] Gs1Status pop_engine_command(Gs1EngineCommand& out_command);

    [[nodiscard]] GameCommandQueue& command_queue() noexcept { return command_queue_; }
    [[nodiscard]] Gs1Status handle_command(const GameCommand& command);

private:
    void queue_log_command(const char* message);
    void queue_app_state_command(Gs1AppState app_state);
    void queue_ui_setup_begin_command(
        Gs1UiSetupId setup_id,
        Gs1UiSetupPresentationType presentation_type,
        std::uint32_t element_count,
        std::uint32_t context_id);
    void queue_ui_setup_close_command(
        Gs1UiSetupId setup_id,
        Gs1UiSetupPresentationType presentation_type);
    void queue_close_ui_setup_if_open(Gs1UiSetupId setup_id);
    void queue_close_active_normal_ui_if_open();
    void queue_ui_element_command(
        std::uint32_t element_id,
        Gs1UiElementType element_type,
        std::uint32_t flags,
        const Gs1UiAction& action,
        const char* text);
    void queue_ui_setup_end_command();
    void queue_clear_ui_setup_commands(Gs1UiSetupId setup_id);
    void queue_main_menu_ui_commands();
    void queue_regional_map_selection_ui_commands();
    void queue_site_result_ui_commands(std::uint32_t site_id, Gs1SiteAttemptResult result);
    void queue_regional_map_snapshot_commands();
    void queue_regional_map_site_upsert_command(const SiteMetaState& site);
    void queue_site_snapshot_commands();
    void queue_hud_state_command();
    void queue_site_result_ready_command(
        std::uint32_t site_id,
        Gs1SiteAttemptResult result,
        std::uint32_t newly_revealed_site_count);
    void consume_input_snapshot(const Gs1InputSnapshot* input);
    [[nodiscard]] Gs1Status translate_ui_action_to_command(const Gs1UiAction& action, GameCommand& out_command) const;
    [[nodiscard]] Gs1Status dispatch_host_events(std::uint32_t& out_processed_count);
    [[nodiscard]] Gs1Status dispatch_feedback_events(std::uint32_t& out_processed_count);
    [[nodiscard]] Gs1Status dispatch_queued_commands();
    void rebuild_regional_map_caches();
    void run_fixed_step();
    std::optional<SiteMetaState*> find_site_mut(std::uint32_t site_id);

private:
    Gs1RuntimeCreateDesc create_desc_ {};
    double fixed_step_seconds_ {k_default_fixed_step_seconds};
    Gs1AppState app_state_ {GS1_APP_STATE_BOOT};
    std::optional<CampaignState> campaign_ {};
    std::optional<SiteRunState> active_site_run_ {};
    InputSnapshot input_snapshot_ {};
    std::deque<Gs1HostEvent> host_events_ {};
    std::deque<Gs1FeedbackEvent> feedback_events_ {};
    GameCommandQueue command_queue_ {};
    std::deque<Gs1EngineCommand> engine_commands_ {};
    std::map<Gs1UiSetupId, Gs1UiSetupPresentationType> active_ui_setups_ {};
    std::optional<Gs1UiSetupId> active_normal_ui_setup_ {};
    std::optional<Gs1AppState> last_emitted_app_state_ {};
    bool boot_initialized_ {false};
};

}  // namespace gs1
