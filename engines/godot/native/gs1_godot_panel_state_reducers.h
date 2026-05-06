#pragma once

#include "gs1_godot_projection_types.h"

#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

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
