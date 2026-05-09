#pragma once

#include "gs1_godot_projection_types.h"

#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

struct Gs1GodotUiPanelStateReducerMessageFamily final
{
    Gs1EngineMessageType begin_message {GS1_ENGINE_MESSAGE_BEGIN_UI_PANEL};
    Gs1EngineMessageType text_upsert_message {GS1_ENGINE_MESSAGE_UI_PANEL_TEXT_UPSERT};
    Gs1EngineMessageType slot_action_upsert_message {GS1_ENGINE_MESSAGE_UI_PANEL_SLOT_ACTION_UPSERT};
    Gs1EngineMessageType list_item_upsert_message {GS1_ENGINE_MESSAGE_UI_PANEL_LIST_ITEM_UPSERT};
    Gs1EngineMessageType list_action_upsert_message {GS1_ENGINE_MESSAGE_UI_PANEL_LIST_ACTION_UPSERT};
    Gs1EngineMessageType end_message {GS1_ENGINE_MESSAGE_END_UI_PANEL};
    Gs1EngineMessageType close_message {GS1_ENGINE_MESSAGE_CLOSE_UI_PANEL};
};

inline constexpr Gs1GodotUiPanelStateReducerMessageFamily k_gs1_action_panel_ui_panel_message_family {};
inline constexpr Gs1GodotUiPanelStateReducerMessageFamily k_gs1_regional_selection_ui_panel_message_family {
    GS1_ENGINE_MESSAGE_BEGIN_REGIONAL_SELECTION_UI_PANEL,
    GS1_ENGINE_MESSAGE_REGIONAL_SELECTION_UI_PANEL_TEXT_UPSERT,
    GS1_ENGINE_MESSAGE_REGIONAL_SELECTION_UI_PANEL_SLOT_ACTION_UPSERT,
    GS1_ENGINE_MESSAGE_REGIONAL_SELECTION_UI_PANEL_LIST_ITEM_UPSERT,
    GS1_ENGINE_MESSAGE_REGIONAL_SELECTION_UI_PANEL_LIST_ACTION_UPSERT,
    GS1_ENGINE_MESSAGE_END_REGIONAL_SELECTION_UI_PANEL,
    GS1_ENGINE_MESSAGE_CLOSE_REGIONAL_SELECTION_UI_PANEL};

struct Gs1GodotRegionalMapStateReducerMessageFamily final
{
    Gs1EngineMessageType begin_message {GS1_ENGINE_MESSAGE_BEGIN_REGIONAL_MAP_SNAPSHOT};
    Gs1EngineMessageType site_upsert_message {GS1_ENGINE_MESSAGE_REGIONAL_MAP_SITE_UPSERT};
    Gs1EngineMessageType site_remove_message {GS1_ENGINE_MESSAGE_REGIONAL_MAP_SITE_REMOVE};
    Gs1EngineMessageType link_upsert_message {GS1_ENGINE_MESSAGE_REGIONAL_MAP_LINK_UPSERT};
    Gs1EngineMessageType link_remove_message {GS1_ENGINE_MESSAGE_REGIONAL_MAP_LINK_REMOVE};
    Gs1EngineMessageType end_message {GS1_ENGINE_MESSAGE_END_REGIONAL_MAP_SNAPSHOT};
};

inline constexpr Gs1GodotRegionalMapStateReducerMessageFamily k_gs1_regional_map_scene_message_family {};
inline constexpr Gs1GodotRegionalMapStateReducerMessageFamily k_gs1_regional_map_hud_message_family {
    GS1_ENGINE_MESSAGE_BEGIN_REGIONAL_MAP_HUD_SNAPSHOT,
    GS1_ENGINE_MESSAGE_REGIONAL_MAP_HUD_SITE_UPSERT,
    GS1_ENGINE_MESSAGE_REGIONAL_MAP_HUD_SITE_REMOVE,
    GS1_ENGINE_MESSAGE_REGIONAL_MAP_HUD_LINK_UPSERT,
    GS1_ENGINE_MESSAGE_REGIONAL_MAP_HUD_LINK_REMOVE,
    GS1_ENGINE_MESSAGE_END_REGIONAL_MAP_HUD_SNAPSHOT};
inline constexpr Gs1GodotRegionalMapStateReducerMessageFamily k_gs1_regional_summary_message_family {
    GS1_ENGINE_MESSAGE_BEGIN_REGIONAL_SUMMARY_SNAPSHOT,
    GS1_ENGINE_MESSAGE_REGIONAL_SUMMARY_SITE_UPSERT,
    GS1_ENGINE_MESSAGE_REGIONAL_SUMMARY_SITE_REMOVE,
    GS1_ENGINE_MESSAGE_REGIONAL_SUMMARY_LINK_UPSERT,
    GS1_ENGINE_MESSAGE_REGIONAL_SUMMARY_LINK_REMOVE,
    GS1_ENGINE_MESSAGE_END_REGIONAL_SUMMARY_SNAPSHOT};
inline constexpr Gs1GodotRegionalMapStateReducerMessageFamily k_gs1_regional_selection_message_family {
    GS1_ENGINE_MESSAGE_BEGIN_REGIONAL_SELECTION_SNAPSHOT,
    GS1_ENGINE_MESSAGE_REGIONAL_SELECTION_SITE_UPSERT,
    GS1_ENGINE_MESSAGE_REGIONAL_SELECTION_SITE_REMOVE,
    GS1_ENGINE_MESSAGE_REGIONAL_SELECTION_LINK_UPSERT,
    GS1_ENGINE_MESSAGE_REGIONAL_SELECTION_LINK_REMOVE,
    GS1_ENGINE_MESSAGE_END_REGIONAL_SELECTION_SNAPSHOT};

class Gs1GodotStatusStateReducer final
{
public:
    struct State final
    {
        std::optional<Gs1AppState> current_app_state {};
        std::optional<Gs1RuntimeCampaignResourcesProjection> campaign_resources {};
        std::optional<Gs1RuntimeHudProjection> hud {};
        std::optional<Gs1RuntimeSiteActionProjection> site_action {};
        std::optional<Gs1RuntimeSiteResultProjection> site_result {};
        std::vector<Gs1RuntimeOneShotCueProjection> recent_one_shot_cues {};
    };

    void reset() noexcept;
    void apply_engine_message(const Gs1EngineMessage& message);
    [[nodiscard]] const State& state() const noexcept { return state_; }

private:
    State state_ {};
    std::uint64_t next_one_shot_cue_sequence_id_ {1U};
};

class Gs1GodotUiSetupStateReducer final
{
public:
    void reset() noexcept;
    void apply_engine_message(const Gs1EngineMessage& message);
    [[nodiscard]] const std::vector<Gs1RuntimeUiSetupProjection>& setups() const noexcept { return setups_; }

private:
    struct PendingUiSetup final
    {
        Gs1UiSetupId setup_id {GS1_UI_SETUP_NONE};
        Gs1UiSetupPresentationType presentation_type {GS1_UI_SETUP_PRESENTATION_NONE};
        std::uint32_t context_id {0};
        std::vector<Gs1RuntimeUiElementProjection> elements {};
    };

    void rebuild_indices() noexcept;

    std::vector<Gs1RuntimeUiSetupProjection> setups_ {};
    std::optional<PendingUiSetup> pending_ {};
    std::unordered_map<std::uint16_t, std::size_t> setup_indices_ {};
    std::unordered_map<std::uint32_t, std::size_t> pending_element_indices_ {};
};

class Gs1GodotUiPanelStateReducer final
{
public:
    explicit Gs1GodotUiPanelStateReducer(
        Gs1GodotUiPanelStateReducerMessageFamily family = k_gs1_action_panel_ui_panel_message_family) noexcept
        : family_(family)
    {
    }

    void reset() noexcept;
    void apply_engine_message(const Gs1EngineMessage& message);
    [[nodiscard]] const Gs1RuntimeUiPanelProjection* find_panel(Gs1UiPanelId panel_id) const noexcept;

private:
    struct PendingUiPanel final
    {
        Gs1UiPanelId panel_id {GS1_UI_PANEL_NONE};
        std::uint32_t context_id {0};
        std::vector<Gs1RuntimeUiPanelTextProjection> text_lines {};
        std::vector<Gs1RuntimeUiPanelSlotActionProjection> slot_actions {};
        std::vector<Gs1RuntimeUiPanelListItemProjection> list_items {};
        std::vector<Gs1RuntimeUiPanelListActionProjection> list_actions {};
    };

    void rebuild_indices() noexcept;

    std::vector<Gs1RuntimeUiPanelProjection> panels_ {};
    std::optional<PendingUiPanel> pending_ {};
    std::unordered_map<std::uint16_t, std::size_t> panel_indices_ {};
    std::unordered_map<std::uint16_t, std::size_t> pending_text_line_indices_ {};
    std::unordered_map<std::uint16_t, std::size_t> pending_slot_action_indices_ {};
    std::unordered_map<std::uint64_t, std::size_t> pending_list_item_indices_ {};
    std::unordered_map<std::uint64_t, std::size_t> pending_list_action_indices_ {};
    Gs1GodotUiPanelStateReducerMessageFamily family_ {};
};

class Gs1GodotProgressionViewStateReducer final
{
public:
    void reset() noexcept;
    void apply_engine_message(const Gs1EngineMessage& message);
    [[nodiscard]] const Gs1RuntimeProgressionViewProjection* find_view(Gs1ProgressionViewId view_id) const noexcept;

private:
    struct PendingProgressionView final
    {
        Gs1ProgressionViewId view_id {GS1_PROGRESSION_VIEW_NONE};
        std::uint32_t context_id {0};
        std::vector<Gs1RuntimeProgressionEntryProjection> entries {};
    };

    void rebuild_indices() noexcept;

    std::vector<Gs1RuntimeProgressionViewProjection> views_ {};
    std::optional<PendingProgressionView> pending_ {};
    std::unordered_map<std::uint16_t, std::size_t> view_indices_ {};
    std::unordered_map<std::uint16_t, std::size_t> pending_entry_indices_ {};
};

class Gs1GodotRegionalMapStateReducer final
{
public:
    struct State final
    {
        std::optional<std::uint32_t> selected_site_id {};
        std::vector<Gs1RuntimeRegionalMapSiteProjection> sites {};
        std::vector<Gs1RuntimeRegionalMapLinkProjection> links {};
    };

    explicit Gs1GodotRegionalMapStateReducer(
        Gs1GodotRegionalMapStateReducerMessageFamily family = k_gs1_regional_map_scene_message_family) noexcept
        : family_(family)
    {
    }

    void reset() noexcept;
    void apply_engine_message(const Gs1EngineMessage& message);
    [[nodiscard]] const State& state() const noexcept { return state_; }

private:
    struct PendingRegionalMapState final
    {
        std::vector<Gs1RuntimeRegionalMapSiteProjection> sites {};
        std::vector<Gs1RuntimeRegionalMapLinkProjection> links {};
    };

    State state_ {};
    std::optional<PendingRegionalMapState> pending_ {};
    Gs1GodotRegionalMapStateReducerMessageFamily family_ {};
};

class Gs1GodotSiteStateReducer final
{
public:
    void reset() noexcept;
    void apply_engine_message(const Gs1EngineMessage& message);
    [[nodiscard]] const std::optional<Gs1RuntimeSiteProjection>& state() const noexcept { return site_; }

private:
    [[nodiscard]] std::size_t site_tile_capacity(const Gs1RuntimeSiteProjection& site) const noexcept;
    [[nodiscard]] std::optional<std::uint32_t> site_tile_index(const Gs1RuntimeSiteProjection& site, std::uint16_t x, std::uint16_t y) const noexcept;

    std::optional<Gs1RuntimeSiteProjection> site_ {};
    std::optional<Gs1RuntimeSiteProjection> pending_ {};
    std::unordered_map<std::uint32_t, std::size_t> pending_inventory_storage_indices_ {};
    std::unordered_map<std::uint64_t, std::size_t> pending_worker_pack_slot_indices_ {};
    std::unordered_map<std::uint64_t, std::size_t> pending_opened_storage_slot_indices_ {};
    std::unordered_map<std::uint32_t, std::size_t> pending_task_indices_ {};
    std::unordered_map<std::uint32_t, std::size_t> pending_phone_listing_indices_ {};
    std::unordered_map<std::uint32_t, std::size_t> pending_modifier_indices_ {};
};
