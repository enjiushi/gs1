#pragma once

#include "runtime_dll_loader.h"
#include "smoke_script_directive.h"

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

class SmokeEngineHost final
{
public:
    enum class LogMode : std::uint32_t
    {
        Verbose = 0,
        ActivityOnly = 1
    };

public:
    SmokeEngineHost(
        const Gs1RuntimeApi& api,
        Gs1RuntimeHandle* runtime,
        LogMode log_mode = LogMode::Verbose) noexcept;

    void queue_feedback_event(const Gs1FeedbackEvent& event);
    void update(double delta_seconds);
    void update(double delta_seconds, const Gs1InputSnapshot* input_override);
    void queue_ui_action(const Gs1UiAction& action);
    [[nodiscard]] std::string build_live_state_json() const;

    [[nodiscard]] bool has_inflight_script_directive() const noexcept
    {
        return inflight_script_directive_.has_value();
    }
    [[nodiscard]] bool script_failed() const noexcept { return script_failed_; }
    bool set_inflight_script_directive(SmokeScriptDirective directive);

    [[nodiscard]] const std::vector<std::string>& command_logs() const noexcept { return command_logs_; }
    [[nodiscard]] std::uint32_t phase1_fixed_steps_executed() const noexcept { return phase1_fixed_steps_executed_; }
    [[nodiscard]] std::uint32_t phase2_processed_host_event_count() const noexcept
    {
        return phase2_processed_host_event_count_;
    }
    [[nodiscard]] std::uint64_t frame_number() const noexcept { return frame_number_; }
    [[nodiscard]] std::optional<Gs1AppState> current_app_state() const noexcept { return current_app_state_; }
    [[nodiscard]] std::optional<std::uint32_t> selected_site_id() const noexcept { return selected_site_id_; }

    [[nodiscard]] bool saw_command(Gs1EngineCommandType type) const noexcept;

private:
    struct ActiveUiElement final
    {
        std::uint32_t element_id {0};
        Gs1UiElementType element_type {GS1_UI_ELEMENT_NONE};
        std::uint32_t flags {0};
        Gs1UiAction action {};
        std::string text {};
    };

    struct ActiveUiSetup final
    {
        Gs1UiSetupId setup_id {GS1_UI_SETUP_NONE};
        std::uint32_t context_id {0};
        std::vector<ActiveUiElement> elements {};
    };

    struct PendingUiSetup final
    {
        Gs1UiSetupId setup_id {GS1_UI_SETUP_NONE};
        std::uint32_t context_id {0};
        std::vector<ActiveUiElement> elements {};
    };

    struct RegionalMapSiteProjection final
    {
        std::uint32_t site_id {0};
        Gs1SiteState site_state {GS1_SITE_STATE_LOCKED};
        std::uint32_t site_archetype_id {0};
        std::uint32_t flags {0};
        std::int32_t map_x {0};
        std::int32_t map_y {0};
        std::uint32_t support_package_id {0};
        std::uint32_t support_preview_mask {0};
    };

    struct RegionalMapLinkProjection final
    {
        std::uint32_t from_site_id {0};
        std::uint32_t to_site_id {0};
        std::uint32_t flags {0};
    };

    struct SiteTileProjection final
    {
        std::uint32_t x {0};
        std::uint32_t y {0};
        std::uint32_t terrain_type_id {0};
        std::uint32_t plant_type_id {0};
        std::uint32_t structure_type_id {0};
        std::uint32_t ground_cover_type_id {0};
        float plant_density {0.0f};
        float sand_burial {0.0f};
    };

    struct SiteWorkerProjection final
    {
        float tile_x {0.0f};
        float tile_y {0.0f};
        float facing_degrees {0.0f};
        float health_normalized {0.0f};
        float hydration_normalized {0.0f};
        float energy_normalized {0.0f};
        std::uint32_t flags {0};
        std::uint32_t current_action_kind {0};
    };

    struct SiteCampProjection final
    {
        std::int32_t tile_x {0};
        std::int32_t tile_y {0};
        float durability_normalized {0.0f};
        std::uint32_t flags {0};
    };

    struct SiteWeatherProjection final
    {
        float heat {0.0f};
        float wind {0.0f};
        float dust {0.0f};
        std::uint32_t event_template_id {0};
        Gs1WeatherEventPhase event_phase {GS1_WEATHER_EVENT_PHASE_NONE};
        float phase_minutes_remaining {0.0f};
    };

    struct SiteSnapshotProjection final
    {
        std::uint32_t site_id {0};
        std::uint32_t site_archetype_id {0};
        std::uint32_t width {0};
        std::uint32_t height {0};
        std::vector<SiteTileProjection> tiles {};
        std::optional<SiteWorkerProjection> worker {};
        std::optional<SiteCampProjection> camp {};
        std::optional<SiteWeatherProjection> weather {};
    };

    struct HudProjection final
    {
        float player_health {0.0f};
        float player_hydration {0.0f};
        float player_energy {0.0f};
        std::int32_t current_money {0};
        std::uint32_t active_task_count {0};
        std::uint32_t warning_code {0};
        std::uint32_t current_action_kind {0};
        float site_completion_normalized {0.0f};
    };

    struct SiteResultProjection final
    {
        std::uint32_t site_id {0};
        Gs1SiteAttemptResult result {GS1_SITE_ATTEMPT_RESULT_NONE};
        std::uint32_t newly_revealed_site_count {0};
    };

private:
    void queue_pre_phase1_ui_action_if_ready();
    void queue_between_phase_ui_action_if_ready();
    void submit_host_events(
        std::vector<Gs1HostEvent>& events,
        const char* stage_label);
    void apply_inflight_script_directive();
    void resolve_inflight_script_directive();
    void clear_inflight_script_directive();
    void fail_inflight_script_directive(const std::string& message);
    void flush_engine_commands(const char* stage_label);
    [[nodiscard]] bool try_queue_ui_action_from_directive(std::vector<Gs1HostEvent>& destination);
    [[nodiscard]] bool resolve_available_ui_action(
        const Gs1UiAction& requested_action,
        Gs1UiAction& out_action) const;
    void apply_ui_setup_begin(const Gs1EngineCommand& command);
    void apply_ui_element_upsert(const Gs1EngineCommand& command);
    void apply_ui_setup_end();
    void apply_regional_map_snapshot_begin(const Gs1EngineCommand& command);
    void apply_regional_map_site_upsert(const Gs1EngineCommand& command);
    void apply_regional_map_site_remove(const Gs1EngineCommand& command);
    void apply_regional_map_link_upsert(const Gs1EngineCommand& command);
    void apply_regional_map_link_remove(const Gs1EngineCommand& command);
    void apply_site_snapshot_begin(const Gs1EngineCommand& command);
    void apply_site_tile_upsert(const Gs1EngineCommand& command);
    void apply_site_worker_update(const Gs1EngineCommand& command);
    void apply_site_camp_update(const Gs1EngineCommand& command);
    void apply_site_weather_update(const Gs1EngineCommand& command);
    void apply_site_snapshot_end();
    void apply_hud_state(const Gs1EngineCommand& command);
    void apply_site_result_ready(const Gs1EngineCommand& command);
    static void write_json_string(std::string& destination, std::string_view value);
    [[nodiscard]] std::vector<ActiveUiSetup> snapshot_active_ui_setups() const;
    [[nodiscard]] std::vector<RegionalMapSiteProjection> snapshot_regional_map_sites() const;
    [[nodiscard]] std::vector<RegionalMapLinkProjection> snapshot_regional_map_links() const;
    [[nodiscard]] static std::uint64_t make_regional_map_link_key(
        std::uint32_t from_site_id,
        std::uint32_t to_site_id) noexcept;
    [[nodiscard]] static std::string describe_command(const Gs1EngineCommand& command);
    static Gs1HostEvent make_ui_action_event(const Gs1UiAction& action) noexcept;

private:
    const Gs1RuntimeApi* api_ {nullptr};
    Gs1RuntimeHandle* runtime_ {nullptr};
    LogMode log_mode_ {LogMode::Verbose};
    std::vector<Gs1HostEvent> pending_pre_phase1_host_events_ {};
    std::vector<Gs1HostEvent> pending_between_phase_host_events_ {};
    std::vector<Gs1FeedbackEvent> pending_feedback_events_ {};
    std::vector<std::string> command_logs_ {};
    std::vector<std::string> current_frame_command_entries_ {};
    std::vector<Gs1EngineCommandType> seen_commands_ {};
    std::map<Gs1UiSetupId, ActiveUiSetup> active_ui_setups_ {};
    std::optional<PendingUiSetup> pending_ui_setup_ {};
    std::map<std::uint32_t, RegionalMapSiteProjection> regional_map_sites_ {};
    std::map<std::uint64_t, RegionalMapLinkProjection> regional_map_links_ {};
    std::optional<SiteSnapshotProjection> pending_site_snapshot_ {};
    std::optional<SiteSnapshotProjection> active_site_snapshot_ {};
    std::optional<HudProjection> hud_state_ {};
    std::optional<SiteResultProjection> site_result_ {};
    std::uint32_t phase1_fixed_steps_executed_ {0};
    std::uint32_t phase1_processed_host_event_count_ {0};
    std::uint32_t phase2_processed_host_event_count_ {0};
    std::uint64_t frame_number_ {0};
    std::optional<Gs1AppState> current_app_state_ {};
    std::optional<std::uint32_t> selected_site_id_ {};
    std::optional<SmokeScriptDirective> inflight_script_directive_ {};
    bool inflight_script_directive_started_ {false};
    bool script_failed_ {false};
};
