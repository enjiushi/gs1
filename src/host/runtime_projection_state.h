#pragma once

#include "gs1/types.h"

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
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

using Gs1RuntimeRegionalMapSiteProjection = Gs1EngineMessageRegionalMapSiteData;
using Gs1RuntimeRegionalMapLinkProjection = Gs1EngineMessageRegionalMapLinkData;
using Gs1RuntimeTileProjection = Gs1EngineMessageSiteTileData;
using Gs1RuntimeWorkerProjection = Gs1EngineMessageWorkerData;
using Gs1RuntimeCampProjection = Gs1EngineMessageCampData;
using Gs1RuntimeWeatherProjection = Gs1EngineMessageWeatherData;
using Gs1RuntimeInventoryStorageProjection = Gs1EngineMessageInventoryStorageData;
using Gs1RuntimeInventorySlotProjection = Gs1EngineMessageInventorySlotData;

struct Gs1RuntimeInventoryViewProjection final
{
    std::uint32_t storage_id {0};
    std::uint16_t slot_count {0};
    std::vector<Gs1RuntimeInventorySlotProjection> slots {};
};

using Gs1RuntimeCraftContextOptionProjection = Gs1EngineMessageCraftContextOptionData;

struct Gs1RuntimeCraftContextProjection final
{
    std::int32_t tile_x {0};
    std::int32_t tile_y {0};
    std::uint32_t flags {0};
    std::vector<Gs1RuntimeCraftContextOptionProjection> options {};
};

using Gs1RuntimePlacementPreviewProjection = Gs1EngineMessagePlacementPreviewData;
using Gs1RuntimePlacementPreviewTileProjection = Gs1EngineMessagePlacementPreviewTileData;
using Gs1RuntimePlacementFailureProjection = Gs1EngineMessagePlacementFailureData;
using Gs1RuntimeTaskProjection = Gs1EngineMessageTaskData;
using Gs1RuntimePhoneListingProjection = Gs1EngineMessagePhoneListingData;
using Gs1RuntimePhonePanelProjection = Gs1EngineMessagePhonePanelData;
using Gs1RuntimeProtectionOverlayProjection = Gs1EngineMessageSiteProtectionOverlayData;
using Gs1RuntimeModifierProjection = Gs1EngineMessageSiteModifierData;
using Gs1RuntimeHudProjection = Gs1EngineMessageHudStateData;
using Gs1RuntimeCampaignResourcesProjection = Gs1EngineMessageCampaignResourcesData;
using Gs1RuntimeSiteActionProjection = Gs1EngineMessageSiteActionData;
using Gs1RuntimeSiteResultProjection = Gs1EngineMessageSiteResultData;

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
    [[nodiscard]] const Gs1RuntimeProgressionViewProjection* find_progression_view(Gs1ProgressionViewId view_id) const noexcept;
    [[nodiscard]] const Gs1RuntimeUiPanelProjection* find_ui_panel(Gs1UiPanelId panel_id) const noexcept;

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
    void rebuild_ui_setup_indices() noexcept;
    void rebuild_progression_view_indices() noexcept;
    void rebuild_ui_panel_indices() noexcept;

private:
    Gs1RuntimeProjectionState state_ {};
    std::optional<PendingUiSetup> pending_ui_setup_ {};
    std::optional<PendingProgressionView> pending_progression_view_ {};
    std::optional<PendingUiPanel> pending_ui_panel_ {};
    std::optional<PendingRegionalMapState> pending_regional_map_ {};
    std::optional<Gs1RuntimeSiteProjection> pending_site_ {};
    std::unordered_map<std::uint32_t, std::size_t> pending_ui_setup_element_indices_ {};
    std::unordered_map<std::uint16_t, std::size_t> pending_progression_entry_indices_ {};
    std::unordered_map<std::uint16_t, std::size_t> pending_ui_panel_text_line_indices_ {};
    std::unordered_map<std::uint16_t, std::size_t> pending_ui_panel_slot_action_indices_ {};
    std::unordered_map<std::uint64_t, std::size_t> pending_ui_panel_list_item_indices_ {};
    std::unordered_map<std::uint64_t, std::size_t> pending_ui_panel_list_action_indices_ {};
    std::unordered_map<std::uint32_t, std::size_t> pending_inventory_storage_indices_ {};
    std::unordered_map<std::uint64_t, std::size_t> pending_worker_pack_slot_indices_ {};
    std::unordered_map<std::uint64_t, std::size_t> pending_opened_storage_slot_indices_ {};
    std::unordered_map<std::uint32_t, std::size_t> pending_task_indices_ {};
    std::unordered_map<std::uint32_t, std::size_t> pending_phone_listing_indices_ {};
    std::unordered_map<std::uint32_t, std::size_t> pending_modifier_indices_ {};
    std::unordered_map<std::uint16_t, std::size_t> ui_setup_indices_ {};
    std::unordered_map<std::uint16_t, std::size_t> progression_view_indices_ {};
    std::unordered_map<std::uint16_t, std::size_t> ui_panel_indices_ {};
    std::uint64_t next_one_shot_cue_sequence_id_ {1U};
};
