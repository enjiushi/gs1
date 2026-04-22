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

    enum LiveStatePatchField : std::uint32_t
    {
        LiveStatePatchField_None = 0U,
        LiveStatePatchField_AppState = 1U << 0,
        LiveStatePatchField_SelectedSiteId = 1U << 1,
        LiveStatePatchField_ScriptFailed = 1U << 2,
        LiveStatePatchField_UiSetups = 1U << 3,
        LiveStatePatchField_RegionalMap = 1U << 4,
        LiveStatePatchField_SiteBootstrap = 1U << 5,
        LiveStatePatchField_SiteState = 1U << 6,
        LiveStatePatchField_Hud = 1U << 7,
        LiveStatePatchField_SiteResult = 1U << 8,
        LiveStatePatchField_SiteAction = 1U << 9,
        LiveStatePatchField_SiteStateWorker = 1U << 10,
        LiveStatePatchField_SiteStateCamp = 1U << 11,
        LiveStatePatchField_SiteStateWeather = 1U << 12,
        LiveStatePatchField_SiteStateInventory = 1U << 13,
        LiveStatePatchField_SiteStateTasks = 1U << 14,
        LiveStatePatchField_SiteStatePhone = 1U << 15,
        LiveStatePatchField_SiteStateCraftContext = 1U << 16,
        LiveStatePatchField_SiteStatePlacementPreview = 1U << 17,
        LiveStatePatchField_SitePlacementFailure = 1U << 18,
        LiveStatePatchField_CampaignResources = 1U << 19
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
    [[nodiscard]] std::uint32_t phase2_processed_host_event_count() const noexcept
    {
        return phase2_processed_host_event_count_;
    }
    [[nodiscard]] std::uint64_t frame_number() const noexcept { return frame_number_; }
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
        std::string text {};
    };

    struct ActiveUiSetup final
    {
        Gs1UiSetupId setup_id {GS1_UI_SETUP_NONE};
        Gs1UiSetupPresentationType presentation_type {GS1_UI_SETUP_PRESENTATION_NONE};
        std::uint32_t context_id {0};
        std::vector<ActiveUiElement> elements {};
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
        float moisture {0.0f};
        float soil_fertility {0.0f};
        float soil_salinity {0.0f};
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
        std::int32_t price {0};
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
        std::vector<SitePhoneListingProjection> phone_listings {};
        SitePhonePanelProjection phone_panel {};
        std::optional<SiteInventoryViewProjection> opened_storage {};
        std::optional<SiteCraftContextProjection> craft_context {};
        std::optional<SitePlacementPreviewProjection> placement_preview {};
        std::optional<SitePlacementFailureProjection> placement_failure {};
        std::optional<SiteWorkerProjection> worker {};
        std::optional<SiteCampProjection> camp {};
        std::optional<SiteWeatherProjection> weather {};
    };

    struct HudProjection final
    {
        float player_health {0.0f};
        float player_hydration {0.0f};
        float player_energy {0.0f};
        float player_morale {0.0f};
        std::int32_t current_money {0};
        std::uint32_t active_task_count {0};
        std::uint32_t warning_code {0};
        std::uint32_t current_action_kind {0};
        float site_completion_normalized {0.0f};
    };

    struct CampaignResourcesProjection final
    {
        std::int32_t current_money {0};
        std::int32_t total_reputation {0};
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
        std::vector<RegionalMapSiteProjection> regional_map_sites {};
        std::vector<RegionalMapLinkProjection> regional_map_links {};
        std::optional<SiteSnapshotProjection> active_site_snapshot {};
        std::optional<CampaignResourcesProjection> campaign_resources {};
        std::optional<HudProjection> hud_state {};
        std::optional<SiteActionProjection> site_action {};
        std::optional<SiteResultProjection> site_result {};
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
    void flush_engine_messages(const char* stage_label);
    [[nodiscard]] bool try_queue_ui_action_from_directive(std::vector<Gs1HostEvent>& destination);
    [[nodiscard]] bool resolve_available_ui_action(
        const Gs1UiAction& requested_action,
        Gs1UiAction& out_action) const;
    void apply_ui_setup_begin(const Gs1EngineMessage& message);
    void apply_ui_setup_close(const Gs1EngineMessage& message);
    void apply_ui_element_upsert(const Gs1EngineMessage& message);
    void apply_ui_setup_end();
    void apply_regional_map_snapshot_begin(const Gs1EngineMessage& message);
    void apply_regional_map_site_upsert(const Gs1EngineMessage& message);
    void apply_regional_map_site_remove(const Gs1EngineMessage& message);
    void apply_regional_map_link_upsert(const Gs1EngineMessage& message);
    void apply_regional_map_link_remove(const Gs1EngineMessage& message);
    void apply_site_snapshot_begin(const Gs1EngineMessage& message);
    void apply_site_tile_upsert(const Gs1EngineMessage& message);
    void apply_site_worker_update(const Gs1EngineMessage& message);
    void apply_site_camp_update(const Gs1EngineMessage& message);
    void apply_site_weather_update(const Gs1EngineMessage& message);
    void apply_site_inventory_storage_upsert(const Gs1EngineMessage& message);
    void apply_site_inventory_slot_upsert(const Gs1EngineMessage& message);
    void apply_site_inventory_view_state(const Gs1EngineMessage& message);
    void apply_site_craft_context_begin(const Gs1EngineMessage& message);
    void apply_site_craft_context_option_upsert(const Gs1EngineMessage& message);
    void apply_site_craft_context_end();
    void apply_site_placement_preview(const Gs1EngineMessage& message);
    void apply_site_placement_failure(const Gs1EngineMessage& message);
    void apply_site_task_upsert(const Gs1EngineMessage& message);
    void apply_site_phone_panel_state(const Gs1EngineMessage& message);
    void apply_site_phone_listing_remove(const Gs1EngineMessage& message);
    void apply_site_phone_listing_upsert(const Gs1EngineMessage& message);
    void apply_site_snapshot_end();
    void apply_campaign_resources(const Gs1EngineMessage& message);
    void apply_hud_state(const Gs1EngineMessage& message);
    void apply_site_action_update(const Gs1EngineMessage& message);
    void apply_site_result_ready(const Gs1EngineMessage& message);
    void queue_live_state_patch(std::uint32_t field_mask);
    static void write_json_string(std::string& destination, std::string_view value);
    [[nodiscard]] std::vector<ActiveUiSetup> snapshot_active_ui_setups() const;
    [[nodiscard]] std::vector<RegionalMapSiteProjection> snapshot_regional_map_sites() const;
    [[nodiscard]] std::vector<RegionalMapLinkProjection> snapshot_regional_map_links() const;
    [[nodiscard]] static std::uint64_t make_regional_map_link_key(
        std::uint32_t from_site_id,
        std::uint32_t to_site_id) noexcept;
    [[nodiscard]] static std::string describe_message(const Gs1EngineMessage& message);
    static Gs1HostEvent make_ui_action_event(const Gs1UiAction& action) noexcept;
    static Gs1HostEvent make_site_move_direction_event(
        float world_move_x,
        float world_move_y,
        float world_move_z) noexcept;

private:
    const Gs1RuntimeApi* api_ {nullptr};
    Gs1RuntimeHandle* runtime_ {nullptr};
    LogMode log_mode_ {LogMode::Verbose};
    std::vector<Gs1HostEvent> pending_pre_phase1_host_events_ {};
    std::vector<Gs1HostEvent> pending_between_phase_host_events_ {};
    std::vector<Gs1FeedbackEvent> pending_feedback_events_ {};
    std::vector<std::string> message_logs_ {};
    std::vector<std::string> current_frame_message_entries_ {};
    std::vector<std::string> pending_live_state_patches_ {};
    std::vector<Gs1EngineMessageType> seen_messages_ {};
    std::map<Gs1UiSetupId, ActiveUiSetup> active_ui_setups_ {};
    std::optional<PendingUiSetup> pending_ui_setup_ {};
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
