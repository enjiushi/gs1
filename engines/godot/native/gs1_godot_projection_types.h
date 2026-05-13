#pragma once

#include "gs1/types.h"

#include <cstdint>
#include <optional>
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
using Gs1RuntimeModifierProjection = Gs1EngineMessageSiteModifierData;
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
    std::optional<Gs1RuntimeInventoryViewProjection> opened_storage {};
    std::optional<Gs1RuntimeCraftContextProjection> craft_context {};
    std::optional<Gs1RuntimePlacementPreviewProjection> placement_preview {};
    std::vector<Gs1RuntimePlacementPreviewTileProjection> placement_preview_tiles {};
    std::optional<Gs1RuntimePlacementFailureProjection> placement_failure {};
    std::optional<Gs1RuntimeWorkerProjection> worker {};
    std::optional<Gs1RuntimeCampProjection> camp {};
    std::optional<Gs1RuntimeWeatherProjection> weather {};
};
