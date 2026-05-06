#pragma once

#include "gs1_godot_projection_types.h"

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

struct Gs1GodotMainScreenProjectionState final
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

class Gs1GodotMainScreenProjectionCache final
{
public:
    void reset() noexcept;
    void apply_engine_message(const Gs1EngineMessage& message);

    [[nodiscard]] const Gs1GodotMainScreenProjectionState& state() const noexcept { return state_; }
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
    Gs1GodotMainScreenProjectionState state_ {};
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
