#pragma once

#include "gs1/types.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

struct Gs1RuntimeUiElementProjection final
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

struct Gs1RuntimeUiSetupProjection final
{
    Gs1UiSetupId setup_id {GS1_UI_SETUP_NONE};
    Gs1UiSetupPresentationType presentation_type {GS1_UI_SETUP_PRESENTATION_NONE};
    std::uint32_t context_id {0};
    std::vector<Gs1RuntimeUiElementProjection> elements {};
};

struct Gs1RuntimeProgressionEntryProjection final
{
    std::uint16_t entry_id {0};
    std::uint16_t reputation_requirement {0};
    std::uint16_t content_id {0};
    std::uint16_t tech_node_id {0};
    std::uint8_t faction_id {0};
    Gs1ProgressionEntryKind entry_kind {GS1_PROGRESSION_ENTRY_NONE};
    std::uint32_t flags {0};
    std::uint8_t content_kind {0};
    std::uint8_t tier_index {0};
    Gs1UiAction action {};
};

struct Gs1RuntimeProgressionViewProjection final
{
    Gs1ProgressionViewId view_id {GS1_PROGRESSION_VIEW_NONE};
    std::uint32_t context_id {0};
    std::vector<Gs1RuntimeProgressionEntryProjection> entries {};
};

struct Gs1RuntimeUiPanelTextProjection final
{
    std::uint16_t line_id {0};
    std::uint32_t flags {0};
    std::uint32_t text_kind {0};
    std::uint32_t primary_id {0};
    std::uint32_t secondary_id {0};
    std::uint32_t quantity {0};
    std::uint32_t aux_value {0};
};

struct Gs1RuntimeUiPanelSlotActionProjection final
{
    Gs1UiPanelSlotId slot_id {GS1_UI_PANEL_SLOT_NONE};
    std::uint32_t flags {0};
    Gs1UiAction action {};
    std::uint32_t label_kind {0};
    std::uint32_t primary_id {0};
    std::uint32_t secondary_id {0};
    std::uint32_t quantity {0};
};

struct Gs1RuntimeUiPanelListItemProjection final
{
    std::uint32_t item_id {0};
    Gs1UiPanelListId list_id {GS1_UI_PANEL_LIST_NONE};
    std::uint32_t flags {0};
    std::uint32_t primary_kind {0};
    std::uint32_t secondary_kind {0};
    std::uint32_t primary_id {0};
    std::uint32_t secondary_id {0};
    std::uint32_t quantity {0};
    std::int32_t map_x {0};
    std::int32_t map_y {0};
};

struct Gs1RuntimeUiPanelListActionProjection final
{
    std::uint32_t item_id {0};
    Gs1UiPanelListId list_id {GS1_UI_PANEL_LIST_NONE};
    Gs1UiPanelListActionRole role {GS1_UI_PANEL_LIST_ACTION_ROLE_NONE};
    std::uint32_t flags {0};
    Gs1UiAction action {};
};

struct Gs1RuntimeUiPanelProjection final
{
    Gs1UiPanelId panel_id {GS1_UI_PANEL_NONE};
    std::uint32_t context_id {0};
    std::vector<Gs1RuntimeUiPanelTextProjection> text_lines {};
    std::vector<Gs1RuntimeUiPanelSlotActionProjection> slot_actions {};
    std::vector<Gs1RuntimeUiPanelListItemProjection> list_items {};
    std::vector<Gs1RuntimeUiPanelListActionProjection> list_actions {};
};

struct Gs1RuntimeRegionalMapSiteProjection final
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

struct Gs1RuntimeRegionalMapLinkProjection final
{
    std::uint32_t from_site_id {0};
    std::uint32_t to_site_id {0};
    std::uint32_t flags {0};
};

struct Gs1RuntimeTileProjection final
{
    std::uint16_t x {0};
    std::uint16_t y {0};
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
    std::uint16_t device_integrity_quantized {0};
    std::uint8_t excavation_depth {0};
    std::uint8_t visible_excavation_depth {0};
};

struct Gs1RuntimeWorkerProjection final
{
    std::uint64_t entity_id {0};
    float tile_x {0.0f};
    float tile_y {0.0f};
    float facing_degrees {0.0f};
    float health_normalized {0.0f};
    float hydration_normalized {0.0f};
    float energy_normalized {0.0f};
    std::uint8_t flags {0};
    Gs1SiteActionKind current_action_kind {GS1_SITE_ACTION_NONE};
};

struct Gs1RuntimeCampProjection final
{
    std::int32_t tile_x {0};
    std::int32_t tile_y {0};
    float durability_normalized {0.0f};
    std::uint32_t flags {0};
};

struct Gs1RuntimeWeatherProjection final
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

struct Gs1RuntimeInventoryStorageProjection final
{
    std::uint32_t storage_id {0};
    std::uint32_t owner_entity_id {0};
    std::uint16_t slot_count {0};
    std::int16_t tile_x {0};
    std::int16_t tile_y {0};
    Gs1InventoryContainerKind container_kind {GS1_INVENTORY_CONTAINER_WORKER_PACK};
    std::uint32_t flags {0};
};

struct Gs1RuntimeInventorySlotProjection final
{
    std::uint32_t item_id {0};
    std::uint32_t item_instance_id {0};
    float condition {0.0f};
    float freshness {0.0f};
    std::uint32_t storage_id {0};
    std::uint32_t container_owner_id {0};
    std::uint16_t quantity {0};
    std::uint16_t slot_index {0};
    std::int16_t container_tile_x {0};
    std::int16_t container_tile_y {0};
    Gs1InventoryContainerKind container_kind {GS1_INVENTORY_CONTAINER_WORKER_PACK};
    std::uint32_t flags {0};
};

struct Gs1RuntimeInventoryViewProjection final
{
    std::uint32_t storage_id {0};
    std::uint16_t slot_count {0};
    std::vector<Gs1RuntimeInventorySlotProjection> slots {};
};

struct Gs1RuntimeCraftContextOptionProjection final
{
    std::uint32_t recipe_id {0};
    std::uint32_t output_item_id {0};
    std::uint32_t flags {0};
};

struct Gs1RuntimeCraftContextProjection final
{
    std::int32_t tile_x {0};
    std::int32_t tile_y {0};
    std::uint32_t flags {0};
    std::vector<Gs1RuntimeCraftContextOptionProjection> options {};
};

struct Gs1RuntimePlacementPreviewProjection final
{
    std::int32_t tile_x {0};
    std::int32_t tile_y {0};
    std::uint64_t blocked_mask {0ULL};
    std::uint32_t item_id {0};
    std::uint32_t preview_tile_count {0};
    std::uint32_t footprint_width {1U};
    std::uint32_t footprint_height {1U};
    Gs1SiteActionKind action_kind {GS1_SITE_ACTION_NONE};
    std::uint32_t flags {0};
};

struct Gs1RuntimePlacementPreviewTileProjection final
{
    std::int16_t x {0};
    std::int16_t y {0};
    std::uint32_t flags {0};
    float wind_protection {0.0f};
    float heat_protection {0.0f};
    float dust_protection {0.0f};
    float final_wind_protection {0.0f};
    float final_heat_protection {0.0f};
    float final_dust_protection {0.0f};
    float occupant_condition {0.0f};
};

struct Gs1RuntimePlacementFailureProjection final
{
    std::int32_t tile_x {0};
    std::int32_t tile_y {0};
    std::uint64_t blocked_mask {0ULL};
    std::uint32_t sequence_id {0};
    Gs1SiteActionKind action_kind {GS1_SITE_ACTION_NONE};
    std::uint32_t flags {0};
};

struct Gs1RuntimeTaskProjection final
{
    std::uint32_t task_instance_id {0};
    std::uint32_t task_template_id {0};
    std::uint32_t publisher_faction_id {0};
    std::uint16_t current_progress {0};
    std::uint16_t target_progress {0};
    Gs1TaskPresentationListKind list_kind {GS1_TASK_PRESENTATION_LIST_VISIBLE};
    std::uint32_t flags {0};
};

struct Gs1RuntimePhoneListingProjection final
{
    std::uint32_t listing_id {0};
    std::uint32_t item_or_unlockable_id {0};
    float price {0.0f};
    std::uint32_t related_site_id {0};
    std::uint16_t quantity {0};
    std::uint16_t cart_quantity {0};
    Gs1PhoneListingPresentationKind listing_kind {GS1_PHONE_LISTING_PRESENTATION_BUY_ITEM};
    std::uint32_t flags {0};
};

struct Gs1RuntimePhonePanelProjection final
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

struct Gs1RuntimeProtectionOverlayProjection final
{
    Gs1SiteProtectionOverlayMode mode {GS1_SITE_PROTECTION_OVERLAY_NONE};
};

struct Gs1RuntimeModifierProjection final
{
    std::uint32_t modifier_id {0};
    std::uint16_t remaining_game_hours {0};
    std::uint32_t flags {0};
};

struct Gs1RuntimeHudProjection final
{
    float player_health {0.0f};
    float player_hydration {0.0f};
    float player_nourishment {0.0f};
    float player_energy {0.0f};
    float player_morale {0.0f};
    float current_money {0.0f};
    float site_completion_normalized {0.0f};
    std::uint16_t active_task_count {0};
    std::uint16_t warning_code {0};
    Gs1SiteActionKind current_action_kind {GS1_SITE_ACTION_NONE};
};

struct Gs1RuntimeCampaignResourcesProjection final
{
    float current_money {0.0f};
    std::int32_t total_reputation {0};
    std::int32_t village_reputation {0};
    std::int32_t forestry_reputation {0};
    std::int32_t university_reputation {0};
};

struct Gs1RuntimeSiteActionProjection final
{
    std::uint32_t action_id {0};
    std::int32_t target_tile_x {0};
    std::int32_t target_tile_y {0};
    Gs1SiteActionKind action_kind {GS1_SITE_ACTION_NONE};
    std::uint32_t flags {0};
    float progress_normalized {0.0f};
    float duration_minutes {0.0f};
};

struct Gs1RuntimeSiteResultProjection final
{
    std::uint32_t site_id {0};
    Gs1SiteAttemptResult result {GS1_SITE_ATTEMPT_RESULT_NONE};
    std::uint16_t newly_revealed_site_count {0};
};

struct Gs1RuntimeOneShotCueProjection final
{
    std::uint64_t sequence_id {0};
    std::uint32_t subject_id {0};
    std::uint32_t arg0 {0};
    std::uint32_t arg1 {0};
    float world_x {0.0f};
    float world_y {0.0f};
    Gs1OneShotCueKind cue_kind {GS1_ONE_SHOT_CUE_NONE};
};

struct Gs1RuntimeSiteProjection final
{
    std::uint32_t site_id {0};
    std::uint32_t site_archetype_id {0};
    std::uint16_t width {0};
    std::uint16_t height {0};
    std::vector<Gs1RuntimeTileProjection> tiles {};
    std::vector<Gs1RuntimeInventoryStorageProjection> inventory_storages {};
    std::vector<Gs1RuntimeInventorySlotProjection> worker_pack_slots {};
    std::vector<Gs1RuntimeTaskProjection> tasks {};
    std::vector<Gs1RuntimeModifierProjection> active_modifiers {};
    std::vector<Gs1RuntimePhoneListingProjection> phone_listings {};
    bool worker_pack_open {false};
    Gs1RuntimePhonePanelProjection phone_panel {};
    Gs1RuntimeProtectionOverlayProjection protection_overlay {};
    std::optional<Gs1RuntimeInventoryViewProjection> opened_storage {};
    std::optional<Gs1RuntimeCraftContextProjection> craft_context {};
    std::optional<Gs1RuntimePlacementPreviewProjection> placement_preview {};
    std::vector<Gs1RuntimePlacementPreviewTileProjection> placement_preview_tiles {};
    std::optional<Gs1RuntimePlacementFailureProjection> placement_failure {};
    std::optional<Gs1RuntimeWorkerProjection> worker {};
    std::optional<Gs1RuntimeCampProjection> camp {};
    std::optional<Gs1RuntimeWeatherProjection> weather {};
};

struct Gs1RuntimeProjectionState final
{
    std::optional<Gs1AppState> current_app_state {};
    std::optional<std::uint32_t> selected_site_id {};
    std::vector<Gs1RuntimeUiSetupProjection> active_ui_setups {};
    std::vector<Gs1RuntimeProgressionViewProjection> active_progression_views {};
    std::vector<Gs1RuntimeUiPanelProjection> active_ui_panels {};
    std::vector<Gs1RuntimeRegionalMapSiteProjection> regional_map_sites {};
    std::vector<Gs1RuntimeRegionalMapLinkProjection> regional_map_links {};
    std::optional<Gs1RuntimeSiteProjection> active_site {};
    std::optional<Gs1RuntimeHudProjection> hud {};
    std::optional<Gs1RuntimeCampaignResourcesProjection> campaign_resources {};
    std::optional<Gs1RuntimeSiteActionProjection> site_action {};
    std::optional<Gs1RuntimeSiteResultProjection> site_result {};
    std::vector<Gs1RuntimeOneShotCueProjection> recent_one_shot_cues {};
};

class Gs1RuntimeProjectionCache final
{
public:
    void reset() noexcept;
    void apply_engine_message(const Gs1EngineMessage& message);

    [[nodiscard]] const Gs1RuntimeProjectionState& state() const noexcept { return state_; }

private:
    struct PendingUiSetup final
    {
        Gs1UiSetupId setup_id {GS1_UI_SETUP_NONE};
        Gs1UiSetupPresentationType presentation_type {GS1_UI_SETUP_PRESENTATION_NONE};
        std::uint32_t context_id {0};
        std::vector<Gs1RuntimeUiElementProjection> elements {};
    };

    struct PendingUiPanel final
    {
        Gs1UiPanelId panel_id {GS1_UI_PANEL_NONE};
        std::uint32_t context_id {0};
        std::vector<Gs1RuntimeUiPanelTextProjection> text_lines {};
        std::vector<Gs1RuntimeUiPanelSlotActionProjection> slot_actions {};
        std::vector<Gs1RuntimeUiPanelListItemProjection> list_items {};
        std::vector<Gs1RuntimeUiPanelListActionProjection> list_actions {};
    };

    struct PendingProgressionView final
    {
        Gs1ProgressionViewId view_id {GS1_PROGRESSION_VIEW_NONE};
        std::uint32_t context_id {0};
        std::vector<Gs1RuntimeProgressionEntryProjection> entries {};
    };

    struct PendingRegionalMapState final
    {
        std::vector<Gs1RuntimeRegionalMapSiteProjection> sites {};
        std::vector<Gs1RuntimeRegionalMapLinkProjection> links {};
    };

    void apply_set_app_state(const Gs1EngineMessageSetAppStateData& payload);
    void apply_regional_map_snapshot_begin(const Gs1EngineMessageRegionalMapSnapshotData& payload);
    void apply_regional_map_site_upsert(const Gs1EngineMessageRegionalMapSiteData& payload);
    void apply_regional_map_site_remove(const Gs1EngineMessageRegionalMapSiteData& payload);
    void apply_regional_map_link_upsert(const Gs1EngineMessageRegionalMapLinkData& payload);
    void apply_regional_map_link_remove(const Gs1EngineMessageRegionalMapLinkData& payload);
    void apply_regional_map_snapshot_end();
    void apply_ui_setup_begin(const Gs1EngineMessageUiSetupData& payload);
    void apply_ui_element_upsert(const Gs1EngineMessageUiElementData& payload);
    void apply_ui_setup_end();
    void apply_ui_setup_close(const Gs1EngineMessageCloseUiSetupData& payload);
    void apply_progression_view_begin(const Gs1EngineMessageProgressionViewData& payload);
    void apply_progression_entry_upsert(const Gs1EngineMessageProgressionEntryData& payload);
    void apply_progression_view_end();
    void apply_progression_view_close(const Gs1EngineMessageCloseProgressionViewData& payload);
    void apply_ui_panel_begin(const Gs1EngineMessageUiPanelData& payload);
    void apply_ui_panel_text_upsert(const Gs1EngineMessageUiPanelTextData& payload);
    void apply_ui_panel_slot_action_upsert(const Gs1EngineMessageUiPanelSlotActionData& payload);
    void apply_ui_panel_list_item_upsert(const Gs1EngineMessageUiPanelListItemData& payload);
    void apply_ui_panel_list_action_upsert(const Gs1EngineMessageUiPanelListActionData& payload);
    void apply_ui_panel_end();
    void apply_ui_panel_close(const Gs1EngineMessageCloseUiPanelData& payload);
    void apply_site_snapshot_begin(const Gs1EngineMessageSiteSnapshotData& payload);
    void apply_site_tile_upsert(const Gs1EngineMessageSiteTileData& payload);
    void apply_site_worker_update(const Gs1EngineMessageWorkerData& payload);
    void apply_site_camp_update(const Gs1EngineMessageCampData& payload);
    void apply_site_weather_update(const Gs1EngineMessageWeatherData& payload);
    void apply_site_inventory_storage_upsert(const Gs1EngineMessageInventoryStorageData& payload);
    void apply_site_inventory_slot_upsert(const Gs1EngineMessageInventorySlotData& payload);
    void apply_site_inventory_view_state(const Gs1EngineMessageInventoryViewData& payload);
    void apply_site_craft_context_begin(const Gs1EngineMessageCraftContextData& payload);
    void apply_site_craft_context_option_upsert(const Gs1EngineMessageCraftContextOptionData& payload);
    void apply_site_craft_context_end();
    void apply_site_placement_preview(const Gs1EngineMessagePlacementPreviewData& payload);
    void apply_site_placement_preview_tile_upsert(const Gs1EngineMessagePlacementPreviewTileData& payload);
    void apply_site_placement_failure(const Gs1EngineMessagePlacementFailureData& payload);
    void apply_site_task_upsert(const Gs1EngineMessageTaskData& payload);
    void apply_site_task_remove(const Gs1EngineMessageTaskData& payload);
    void apply_site_phone_listing_upsert(const Gs1EngineMessagePhoneListingData& payload);
    void apply_site_phone_listing_remove(const Gs1EngineMessagePhoneListingData& payload);
    void apply_site_phone_panel_state(const Gs1EngineMessagePhonePanelData& payload);
    void apply_site_modifier_list_begin(const Gs1EngineMessageSiteModifierListData& payload);
    void apply_site_modifier_upsert(const Gs1EngineMessageSiteModifierData& payload);
    void apply_site_protection_overlay_state(const Gs1EngineMessageSiteProtectionOverlayData& payload);
    void apply_site_snapshot_end();
    void apply_hud_state(const Gs1EngineMessageHudStateData& payload);
    void apply_campaign_resources(const Gs1EngineMessageCampaignResourcesData& payload);
    void apply_site_action_update(const Gs1EngineMessageSiteActionData& payload);
    void apply_site_result_ready(const Gs1EngineMessageSiteResultData& payload);
    void apply_one_shot_cue(const Gs1EngineMessageOneShotCueData& payload);
    [[nodiscard]] std::size_t site_tile_capacity(const Gs1RuntimeSiteProjection& site) const noexcept;
    [[nodiscard]] std::optional<std::uint32_t> site_tile_index(
        const Gs1RuntimeSiteProjection& site,
        std::uint16_t x,
        std::uint16_t y) const noexcept;

private:
    Gs1RuntimeProjectionState state_ {};
    std::optional<PendingUiSetup> pending_ui_setup_ {};
    std::optional<PendingProgressionView> pending_progression_view_ {};
    std::optional<PendingUiPanel> pending_ui_panel_ {};
    std::optional<PendingRegionalMapState> pending_regional_map_ {};
    std::optional<Gs1RuntimeSiteProjection> pending_site_ {};
    std::uint64_t next_one_shot_cue_sequence_id_ {1U};
};
