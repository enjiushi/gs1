#pragma once

#include "host/runtime_dll_loader.h"
#include "smoke_script_directive.h"

#include <cstdint>
#include <filesystem>
#include <map>
#include <mutex>
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

    struct FrameTimingSnapshot final
    {
        double total_update_seconds {0.0};
        double host_update_seconds {0.0};
        double gameplay_dll_seconds {0.0};
    };

    enum LiveStatePatchField : std::uint32_t
    {
        LiveStatePatchField_None = 0U,
        LiveStatePatchField_AppState = 1U << 0,
        LiveStatePatchField_SelectedSiteId = 1U << 1,
        LiveStatePatchField_ScriptFailed = 1U << 2,
        LiveStatePatchField_UiSetups = 1U << 3,
        LiveStatePatchField_ProgressionViews = 1U << 4,
        LiveStatePatchField_RegionalMap = 1U << 5,
        LiveStatePatchField_SiteBootstrap = 1U << 6,
        LiveStatePatchField_SiteState = 1U << 7,
        LiveStatePatchField_Hud = 1U << 8,
        LiveStatePatchField_SiteResult = 1U << 9,
        LiveStatePatchField_SiteAction = 1U << 10,
        LiveStatePatchField_SiteStateWorker = 1U << 11,
        LiveStatePatchField_SiteStateCamp = 1U << 12,
        LiveStatePatchField_SiteStateWeather = 1U << 13,
        LiveStatePatchField_SiteStateInventory = 1U << 14,
        LiveStatePatchField_SiteStateTasks = 1U << 15,
        LiveStatePatchField_SiteStatePhone = 1U << 16,
        LiveStatePatchField_SiteStateCraftContext = 1U << 17,
        LiveStatePatchField_SiteStatePlacementPreview = 1U << 18,
        LiveStatePatchField_SitePlacementFailure = 1U << 19,
        LiveStatePatchField_CampaignResources = 1U << 20,
        LiveStatePatchField_SiteStateProtectionOverlay = 1U << 21,
        LiveStatePatchField_SiteStateModifiers = 1U << 22,
        LiveStatePatchField_AudioCues = 1U << 23
    };

    struct LiveStateSnapshot;

public:
    SmokeEngineHost(
        const Gs1RuntimeApi& api,
        Gs1RuntimeHandle* runtime,
        LogMode log_mode = LogMode::Verbose) noexcept;

    void queue_feedback_event(const Gs1FeedbackEvent& event);
    void update(double delta_seconds);
    void queue_ui_action(const Gs1UiAction& action);
    void queue_site_action_request(const Gs1HostEventSiteActionRequestData& action);
    void queue_site_action_cancel(const Gs1HostEventSiteActionCancelData& action);
    void queue_site_storage_view(const Gs1HostEventSiteStorageViewData& request);
    void queue_site_inventory_slot_tap(const Gs1HostEventSiteInventorySlotTapData& request);
    void queue_site_context_request(const Gs1HostEventSiteContextRequestData& request);
    void queue_site_move_direction(float world_move_x, float world_move_y, float world_move_z);
    [[nodiscard]] std::vector<std::string> consume_pending_live_state_patches();
    [[nodiscard]] LiveStateSnapshot capture_live_state_snapshot() const;
    [[nodiscard]] static std::string build_live_state_json(const LiveStateSnapshot& snapshot);
    [[nodiscard]] static std::string build_live_state_patch_json(
        const LiveStateSnapshot& snapshot,
        std::uint32_t field_mask);
    [[nodiscard]] std::string build_live_state_json() const;

    [[nodiscard]] bool has_inflight_script_directive() const noexcept
    {
        return inflight_script_directive_.has_value();
    }
    [[nodiscard]] bool script_failed() const noexcept { return script_failed_; }
    bool set_inflight_script_directive(SmokeScriptDirective directive);

    [[nodiscard]] const std::vector<std::string>& message_logs() const noexcept { return message_logs_; }
    [[nodiscard]] std::uint32_t phase1_fixed_steps_executed() const noexcept { return phase1_fixed_steps_executed_; }
    [[nodiscard]] std::uint32_t phase2_processed_host_message_count() const noexcept
    {
        return phase2_processed_host_message_count_;
    }
    [[nodiscard]] std::uint64_t frame_number() const noexcept { return frame_number_; }
    [[nodiscard]] FrameTimingSnapshot last_frame_timing() const noexcept { return last_frame_timing_; }
    [[nodiscard]] std::optional<Gs1AppState> current_app_state() const noexcept { return current_app_state_; }
    [[nodiscard]] std::optional<std::uint32_t> selected_site_id() const noexcept { return selected_site_id_; }

    [[nodiscard]] bool saw_message(Gs1EngineMessageType type) const noexcept;

public:
    struct ActiveUiElement final
    {
        std::uint32_t element_id {0};
        Gs1UiElementType element_type {GS1_UI_ELEMENT_NONE};
        std::uint32_t flags {0};
        Gs1UiAction action {};
        std::uint32_t content_kind {0};
        std::uint32_t primary_id {0};
        std::uint32_t secondary_id {0};
        std::uint32_t quantity {0};
    };

    struct ActiveUiSetup final
    {
        Gs1UiSetupId setup_id {GS1_UI_SETUP_NONE};
        Gs1UiSetupPresentationType presentation_type {GS1_UI_SETUP_PRESENTATION_NONE};
        std::uint32_t context_id {0};
        std::vector<ActiveUiElement> elements {};
    };

    struct ProgressionEntryProjection final
    {
        std::uint32_t entry_id {0};
        std::uint32_t reputation_requirement {0};
        std::uint32_t content_id {0};
        std::uint32_t tech_node_id {0};
        std::uint32_t faction_id {0};
        Gs1ProgressionEntryKind entry_kind {GS1_PROGRESSION_ENTRY_NONE};
        std::uint32_t flags {0};
        std::uint32_t content_kind {0};
        std::uint32_t tier_index {0};
        Gs1UiAction action {};
    };

    struct ActiveProgressionView final
    {
        Gs1ProgressionViewId view_id {GS1_PROGRESSION_VIEW_NONE};
        std::uint32_t context_id {0};
        std::vector<ProgressionEntryProjection> entries {};
    };

    struct PendingProgressionView final
    {
        Gs1ProgressionViewId view_id {GS1_PROGRESSION_VIEW_NONE};
        std::uint32_t context_id {0};
        std::vector<ProgressionEntryProjection> entries {};
    };

    struct PendingUiSetup final
    {
        Gs1UiSetupId setup_id {GS1_UI_SETUP_NONE};
        Gs1UiSetupPresentationType presentation_type {GS1_UI_SETUP_PRESENTATION_NONE};
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
        float local_wind {0.0f};
        float wind_protection {0.0f};
        float heat_protection {0.0f};
        float dust_protection {0.0f};
        float moisture {0.0f};
        float soil_fertility {0.0f};
        float soil_salinity {0.0f};
        float device_integrity {0.0f};
        std::uint32_t excavation_depth {0};
        std::uint32_t visible_excavation_depth {0};
    };

    struct SiteWorkerProjection final
    {
        std::uint64_t entity_id {0};
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
        float wind_direction_degrees {0.0f};
        std::uint32_t event_template_id {0};
        float event_start_time_minutes {0.0f};
        float event_peak_time_minutes {0.0f};
        float event_peak_duration_minutes {0.0f};
        float event_end_time_minutes {0.0f};
    };

    struct SiteInventoryStorageProjection final
    {
        std::uint32_t storage_id {0};
        std::uint32_t owner_entity_id {0};
        std::uint32_t slot_count {0};
        std::int32_t tile_x {0};
        std::int32_t tile_y {0};
        Gs1InventoryContainerKind container_kind {GS1_INVENTORY_CONTAINER_WORKER_PACK};
        std::uint32_t flags {0};
    };

    struct SiteInventorySlotProjection final
    {
        std::uint32_t item_id {0};
        std::uint32_t item_instance_id {0};
        std::uint32_t storage_id {0};
        float condition {0.0f};
        float freshness {0.0f};
        std::uint32_t container_owner_id {0};
        std::uint32_t quantity {0};
        std::uint32_t slot_index {0};
        std::int32_t container_tile_x {0};
        std::int32_t container_tile_y {0};
        Gs1InventoryContainerKind container_kind {GS1_INVENTORY_CONTAINER_WORKER_PACK};
        std::uint32_t flags {0};
    };

    struct SiteInventoryViewProjection final
    {
        std::uint32_t storage_id {0};
        std::uint32_t slot_count {0};
        std::vector<SiteInventorySlotProjection> slots {};
    };

    struct SiteCraftContextOptionProjection final
    {
        std::uint32_t recipe_id {0};
        std::uint32_t output_item_id {0};
        std::uint32_t flags {0};
    };

    struct SiteCraftContextProjection final
    {
        std::int32_t tile_x {0};
        std::int32_t tile_y {0};
        std::uint32_t flags {0};
        std::vector<SiteCraftContextOptionProjection> options {};
    };

    struct SitePlacementPreviewProjection final
    {
        std::int32_t tile_x {0};
        std::int32_t tile_y {0};
        std::uint64_t blocked_mask {0ULL};
        std::uint32_t item_id {0};
        std::uint32_t action_kind {0};
        std::uint32_t flags {0};
        std::uint32_t footprint_width {1U};
        std::uint32_t footprint_height {1U};
    };

    struct SitePlacementPreviewTileProjection final
    {
        std::int32_t x {0};
        std::int32_t y {0};
        std::uint32_t flags {0U};
        float wind_protection {0.0f};
        float heat_protection {0.0f};
        float dust_protection {0.0f};
        float final_wind_protection {0.0f};
        float final_heat_protection {0.0f};
        float final_dust_protection {0.0f};
        float occupant_condition {0.0f};
    };

    struct SitePlacementFailureProjection final
    {
        std::int32_t tile_x {0};
        std::int32_t tile_y {0};
        std::uint64_t blocked_mask {0ULL};
        std::uint32_t action_kind {0};
        std::uint32_t sequence_id {0};
        std::uint32_t flags {0};
    };

    struct SiteTaskProjection final
    {
        std::uint32_t task_instance_id {0};
        std::uint32_t task_template_id {0};
        std::uint32_t publisher_faction_id {0};
        std::uint32_t current_progress {0};
        std::uint32_t target_progress {0};
        Gs1TaskPresentationListKind list_kind {GS1_TASK_PRESENTATION_LIST_VISIBLE};
        std::uint32_t flags {0};
    };

    struct SitePhoneListingProjection final
    {
        std::uint32_t listing_id {0};
        std::uint32_t item_or_unlockable_id {0};
        float price {0.0f};
        std::uint32_t related_site_id {0};
        std::uint32_t quantity {0};
        std::uint32_t cart_quantity {0};
        Gs1PhoneListingPresentationKind listing_kind {GS1_PHONE_LISTING_PRESENTATION_BUY_ITEM};
        std::uint32_t flags {0};
    };

    struct SitePhonePanelProjection final
    {
        Gs1PhonePanelSection active_section {GS1_PHONE_PANEL_SECTION_HOME};
        std::uint32_t visible_task_count {0};
        std::uint32_t accepted_task_count {0};
        std::uint32_t completed_task_count {0};
        std::uint32_t claimed_task_count {0};
        std::uint32_t buy_listing_count {0};
        std::uint32_t sell_listing_count {0};
        std::uint32_t service_listing_count {0};
        std::uint32_t cart_item_count {0};
        std::uint32_t flags {0};
    };

    struct SiteProtectionOverlayProjection final
    {
        Gs1SiteProtectionOverlayMode mode {GS1_SITE_PROTECTION_OVERLAY_NONE};
    };

    struct SiteModifierProjection final
    {
        std::uint32_t modifier_id {0};
        std::uint16_t remaining_game_hours {0};
        std::uint32_t flags {0};
    };

    struct SiteSnapshotProjection final
    {
        std::uint32_t site_id {0};
        std::uint32_t site_archetype_id {0};
        std::uint32_t width {0};
        std::uint32_t height {0};
        std::vector<SiteTileProjection> tiles {};
        std::vector<SiteInventoryStorageProjection> inventory_storages {};
        std::vector<SiteInventorySlotProjection> worker_pack_slots {};
        std::vector<SiteTaskProjection> tasks {};
        std::vector<SiteModifierProjection> active_modifiers {};
        std::vector<SitePhoneListingProjection> phone_listings {};
        bool worker_pack_open {false};
        SitePhonePanelProjection phone_panel {};
        SiteProtectionOverlayProjection protection_overlay {};
        std::optional<SiteInventoryViewProjection> opened_storage {};
        std::optional<SiteCraftContextProjection> craft_context {};
        std::optional<SitePlacementPreviewProjection> placement_preview {};
        std::vector<SitePlacementPreviewTileProjection> placement_preview_tiles {};
        std::optional<SitePlacementFailureProjection> placement_failure {};
        std::optional<SiteWorkerProjection> worker {};
        std::optional<SiteCampProjection> camp {};
        std::optional<SiteWeatherProjection> weather {};
    };

    struct HudProjection final
    {
        float player_health {0.0f};
        float player_hydration {0.0f};
        float player_nourishment {0.0f};
        float player_energy {0.0f};
        float player_morale {0.0f};
        float current_money {0.0f};
        std::uint32_t active_task_count {0};
        std::uint32_t warning_code {0};
        std::uint32_t current_action_kind {0};
        float site_completion_normalized {0.0f};
    };

    struct CampaignResourcesProjection final
    {
        float current_money {0.0f};
        std::int32_t total_reputation {0};
        std::int32_t village_reputation {0};
        std::int32_t forestry_reputation {0};
        std::int32_t university_reputation {0};
    };

    struct SiteActionProjection final
    {
        std::uint32_t action_id {0};
        std::int32_t target_tile_x {0};
        std::int32_t target_tile_y {0};
        std::uint32_t action_kind {0};
        std::uint32_t flags {0};
        float progress_normalized {0.0f};
        float duration_minutes {0.0f};
    };

    struct SiteResultProjection final
    {
        std::uint32_t site_id {0};
        Gs1SiteAttemptResult result {GS1_SITE_ATTEMPT_RESULT_NONE};
        std::uint32_t newly_revealed_site_count {0};
    };

    struct OneShotCueProjection final
    {
        std::uint64_t sequence_id {0};
        std::uint64_t frame_number {0};
        std::uint32_t subject_id {0};
        std::uint32_t arg0 {0};
        std::uint32_t arg1 {0};
        float world_x {0.0f};
        float world_y {0.0f};
        Gs1OneShotCueKind cue_kind {GS1_ONE_SHOT_CUE_NONE};
    };

public:
    struct LiveStateSnapshot final
    {
        std::uint64_t frame_number {0};
        std::optional<Gs1AppState> current_app_state {};
        std::optional<std::uint32_t> selected_site_id {};
        bool script_failed {false};
        std::vector<std::string> current_frame_message_entries {};
        std::vector<std::string> message_log_tail {};
        std::vector<ActiveUiSetup> active_ui_setups {};
        std::vector<ActiveProgressionView> active_progression_views {};
        std::vector<RegionalMapSiteProjection> regional_map_sites {};
        std::vector<RegionalMapLinkProjection> regional_map_links {};
        std::optional<SiteSnapshotProjection> active_site_snapshot {};
        std::optional<CampaignResourcesProjection> campaign_resources {};
        std::optional<HudProjection> hud_state {};
        std::optional<SiteActionProjection> site_action {};
        std::optional<SiteResultProjection> site_result {};
        std::vector<OneShotCueProjection> recent_one_shot_cues {};
    };

private:
    void drain_incoming_commands();
    void queue_pre_phase1_ui_action_if_ready();
    void queue_between_phase_ui_action_if_ready();
    void queue_between_phase_site_scene_ready_if_needed();
    void submit_host_messages(
        std::vector<Gs1HostMessage>& events,
        const char* stage_label);
    void apply_inflight_script_directive();
    void resolve_inflight_script_directive();
    void clear_inflight_script_directive();
    void fail_inflight_script_directive(const std::string& message);
    void flush_engine_messages(const char* stage_label);
    [[nodiscard]] bool try_queue_ui_action_from_directive(std::vector<Gs1HostMessage>& destination);
    [[nodiscard]] bool resolve_available_ui_action(
        const Gs1UiAction& requested_action,
        Gs1UiAction& out_action) const;
    void apply_ui_setup_begin(const Gs1RuntimeMessage& message);
    void apply_ui_setup_close(const Gs1RuntimeMessage& message);
    void apply_ui_element_upsert(const Gs1RuntimeMessage& message);
    void apply_ui_setup_end();
    void apply_progression_view_begin(const Gs1RuntimeMessage& message);
    void apply_progression_view_close(const Gs1RuntimeMessage& message);
    void apply_progression_entry_upsert(const Gs1RuntimeMessage& message);
    void apply_progression_view_end();
    void apply_regional_map_snapshot_begin(const Gs1RuntimeMessage& message);
    void apply_regional_map_site_upsert(const Gs1RuntimeMessage& message);
    void apply_regional_map_site_remove(const Gs1RuntimeMessage& message);
    void apply_regional_map_link_upsert(const Gs1RuntimeMessage& message);
    void apply_regional_map_link_remove(const Gs1RuntimeMessage& message);
    void apply_site_snapshot_begin(const Gs1RuntimeMessage& message);
    void apply_site_tile_upsert(const Gs1RuntimeMessage& message);
    void apply_site_worker_update(const Gs1RuntimeMessage& message);
    void apply_site_camp_update(const Gs1RuntimeMessage& message);
    void apply_site_weather_update(const Gs1RuntimeMessage& message);
    void apply_site_inventory_storage_upsert(const Gs1RuntimeMessage& message);
    void apply_site_inventory_slot_upsert(const Gs1RuntimeMessage& message);
    void apply_site_inventory_view_state(const Gs1RuntimeMessage& message);
    void apply_site_craft_context_begin(const Gs1RuntimeMessage& message);
    void apply_site_craft_context_option_upsert(const Gs1RuntimeMessage& message);
    void apply_site_craft_context_end();
    void apply_site_placement_preview(const Gs1RuntimeMessage& message);
    void apply_site_placement_preview_tile_upsert(const Gs1RuntimeMessage& message);
    void apply_site_placement_failure(const Gs1RuntimeMessage& message);
    void apply_site_task_upsert(const Gs1RuntimeMessage& message);
    void apply_site_modifier_list_begin(const Gs1RuntimeMessage& message);
    void apply_site_modifier_upsert(const Gs1RuntimeMessage& message);
    void apply_site_phone_panel_state(const Gs1RuntimeMessage& message);
    void apply_site_protection_overlay_state(const Gs1RuntimeMessage& message);
    void apply_site_phone_listing_remove(const Gs1RuntimeMessage& message);
    void apply_site_phone_listing_upsert(const Gs1RuntimeMessage& message);
    void apply_site_snapshot_end();
    void apply_campaign_resources(const Gs1RuntimeMessage& message);
    void apply_hud_state(const Gs1RuntimeMessage& message);
    void apply_site_action_update(const Gs1RuntimeMessage& message);
    void apply_site_result_ready(const Gs1RuntimeMessage& message);
    void apply_one_shot_cue(const Gs1RuntimeMessage& message);
    void publish_live_state_snapshot();
    void queue_live_state_patch(std::uint32_t field_mask);
    [[nodiscard]] LiveStateSnapshot capture_frame_live_state_snapshot() const;
    static void write_json_string(std::string& destination, std::string_view value);
    [[nodiscard]] std::vector<ActiveUiSetup> snapshot_active_ui_setups() const;
    [[nodiscard]] std::vector<ActiveProgressionView> snapshot_active_progression_views() const;
    [[nodiscard]] std::vector<RegionalMapSiteProjection> snapshot_regional_map_sites() const;
    [[nodiscard]] std::vector<RegionalMapLinkProjection> snapshot_regional_map_links() const;
    [[nodiscard]] static std::uint64_t make_regional_map_link_key(
        std::uint32_t from_site_id,
        std::uint32_t to_site_id) noexcept;
    [[nodiscard]] static std::string describe_message(const Gs1RuntimeMessage& message);
    static Gs1HostMessage make_ui_action_event(const Gs1UiAction& action) noexcept;
    static Gs1HostMessage make_site_scene_ready_event() noexcept;
    static Gs1HostMessage make_site_move_direction_event(
        float world_move_x,
        float world_move_y,
        float world_move_z) noexcept;

private:
    const Gs1RuntimeApi* api_ {nullptr};
    Gs1RuntimeHandle* runtime_ {nullptr};
    LogMode log_mode_ {LogMode::Verbose};
    mutable std::mutex incoming_commands_mutex_ {};
    std::vector<Gs1HostMessage> incoming_pre_phase1_host_events_ {};
    std::vector<Gs1FeedbackEvent> incoming_feedback_events_ {};
    mutable std::mutex published_state_mutex_ {};
    LiveStateSnapshot published_live_state_snapshot_ {};
    std::vector<std::string> published_live_state_patches_ {};
    std::vector<Gs1HostMessage> frame_pre_phase1_host_events_ {};
    std::vector<Gs1HostMessage> pending_between_phase_host_events_ {};
    std::vector<Gs1FeedbackEvent> frame_feedback_events_ {};
    std::vector<std::string> message_logs_ {};
    std::vector<std::string> current_frame_message_entries_ {};
    std::uint32_t frame_live_state_patch_mask_ {0U};
    std::vector<Gs1EngineMessageType> seen_messages_ {};
    std::map<Gs1UiSetupId, ActiveUiSetup> active_ui_setups_ {};
    std::optional<PendingUiSetup> pending_ui_setup_ {};
    std::map<Gs1ProgressionViewId, ActiveProgressionView> active_progression_views_ {};
    std::optional<PendingProgressionView> pending_progression_view_ {};
    std::map<std::uint32_t, RegionalMapSiteProjection> regional_map_sites_ {};
    std::map<std::uint64_t, RegionalMapLinkProjection> regional_map_links_ {};
    std::uint32_t pending_regional_map_patch_mask_ {0U};
    std::optional<SiteSnapshotProjection> pending_site_snapshot_ {};
    std::uint32_t pending_site_snapshot_patch_mask_ {0U};
    std::optional<SiteSnapshotProjection> active_site_snapshot_ {};
    std::optional<CampaignResourcesProjection> campaign_resources_ {};
    std::optional<HudProjection> hud_state_ {};
    std::optional<SiteActionProjection> site_action_ {};
    std::optional<SiteResultProjection> site_result_ {};
    std::vector<OneShotCueProjection> recent_one_shot_cues_ {};
    std::uint64_t next_one_shot_cue_sequence_id_ {0};
    std::uint32_t phase1_fixed_steps_executed_ {0};
    std::uint32_t phase1_processed_host_message_count_ {0};
    std::uint32_t phase2_processed_host_message_count_ {0};
    std::uint64_t frame_number_ {0};
    double current_frame_gameplay_dll_seconds_ {0.0};
    FrameTimingSnapshot last_frame_timing_ {};
    std::optional<Gs1AppState> current_app_state_ {};
    std::optional<std::uint32_t> selected_site_id_ {};
    bool pending_site_scene_ready_ack_ {false};
    std::optional<SmokeScriptDirective> inflight_script_directive_ {};
    bool inflight_script_directive_started_ {false};
    bool script_failed_ {false};
};


