#pragma once

#include "campaign/campaign_state.h"
#include "messages/game_message.h"
#include "runtime/message_queue.h"
#include "runtime/site_protection_presentation_state.h"
#include "site/site_run_state.h"
#include "gs1/status.h"
#include "gs1/types.h"

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <unordered_set>
#include <vector>

namespace gs1
{
struct GamePresentationRuntimeContext final
{
    Gs1AppState& app_state;
    std::optional<CampaignState>& campaign;
    std::optional<SiteRunState>& active_site_run;
    SiteProtectionPresentationState& protection_presentation;
    GameMessageQueue& message_queue;
    std::deque<Gs1RuntimeMessage>& engine_messages;
    double fixed_step_seconds {0.0};
};

class GamePresentationCoordinator final
{
public:
    [[nodiscard]] static bool subscribes_to_host_message(Gs1HostMessageType type) noexcept;
    [[nodiscard]] Gs1Status process_host_message(
        GamePresentationRuntimeContext& context,
        const Gs1HostMessage& message);
    void on_message_processed(
        GamePresentationRuntimeContext& context,
        const GameMessage& message);
    void activate_loaded_site_scene(GamePresentationRuntimeContext& context);
    void mark_site_projection_update_dirty(
        GamePresentationRuntimeContext& context,
        std::uint64_t dirty_flags) noexcept;
    void mark_site_tile_projection_dirty(
        GamePresentationRuntimeContext& context,
        TileCoord coord) noexcept;
    void flush_site_presentation_if_dirty(GamePresentationRuntimeContext& context);

    [[nodiscard]] bool site_protection_selector_open() const noexcept
    {
        return protection_presentation_state().selector_open;
    }

    [[nodiscard]] Gs1SiteProtectionOverlayMode site_protection_overlay_mode() const noexcept
    {
        return protection_presentation_state().overlay_mode;
    }

private:
    [[nodiscard]] GamePresentationRuntimeContext& context() noexcept { return *active_context_; }
    [[nodiscard]] const GamePresentationRuntimeContext& context() const noexcept { return *active_context_; }
    [[nodiscard]] Gs1AppState& app_state() noexcept { return context().app_state; }
    [[nodiscard]] std::optional<CampaignState>& campaign() noexcept { return context().campaign; }
    [[nodiscard]] std::optional<SiteRunState>& active_site_run() noexcept { return context().active_site_run; }
    [[nodiscard]] SiteProtectionPresentationState& protection_presentation_state() noexcept
    {
        return context().protection_presentation;
    }
    [[nodiscard]] const SiteProtectionPresentationState& protection_presentation_state() const noexcept
    {
        return context().protection_presentation;
    }
    [[nodiscard]] GameMessageQueue& message_queue() noexcept { return context().message_queue; }
    [[nodiscard]] std::deque<Gs1RuntimeMessage>& engine_messages() noexcept { return context().engine_messages; }
    [[nodiscard]] double fixed_step_seconds() const noexcept { return context().fixed_step_seconds; }

    struct CampaignUnlockSnapshot final
    {
        std::vector<std::uint32_t> unlocked_reputation_unlock_ids {};
        std::vector<std::uint32_t> unlocked_technology_node_ids {};
    };

    void queue_log_message(const char* message, Gs1LogLevel level = GS1_LOG_LEVEL_INFO);
    void queue_app_state_message(Gs1AppState app_state);
    void queue_ui_setup_begin_message(
        Gs1UiSetupId setup_id,
        Gs1UiSetupPresentationType presentation_type,
        std::uint32_t element_count,
        std::uint32_t context_id);
    void queue_ui_setup_close_message(
        Gs1UiSetupId setup_id,
        Gs1UiSetupPresentationType presentation_type);
    void queue_ui_panel_begin_message(
        Gs1UiPanelId panel_id,
        std::uint32_t context_id,
        std::uint32_t text_line_count,
        std::uint32_t slot_action_count,
        std::uint32_t list_item_count,
        std::uint32_t list_action_count);
    void queue_ui_panel_close_message(Gs1UiPanelId panel_id);
    void queue_ui_surface_visibility_message(Gs1UiSurfaceId surface_id, bool visible);
    void queue_progression_view_begin_message(
        Gs1ProgressionViewId view_id,
        std::uint32_t entry_count,
        std::uint32_t context_id);
    void queue_progression_view_close_message(Gs1ProgressionViewId view_id);
    void queue_close_ui_setup_if_open(Gs1UiSetupId setup_id);
    void queue_close_ui_panel_if_open(Gs1UiPanelId panel_id);
    void queue_close_progression_view_if_open(Gs1ProgressionViewId view_id);
    void queue_close_active_normal_ui_if_open();
    void queue_close_site_inventory_panels_if_open();
    void queue_close_site_phone_panel_if_open();
    void queue_ui_element_message(
        std::uint32_t element_id,
        Gs1UiElementType element_type,
        std::uint32_t flags,
        const Gs1UiAction& action,
        std::uint8_t content_kind,
        std::uint32_t primary_id = 0U,
        std::uint32_t secondary_id = 0U,
        std::uint32_t quantity = 0U);
    void queue_progression_entry_message(const Gs1EngineMessageProgressionEntryData& payload);
    void queue_ui_panel_text_message(
        std::uint16_t line_id,
        std::uint32_t flags,
        std::uint8_t text_kind,
        std::uint32_t primary_id = 0U,
        std::uint32_t secondary_id = 0U,
        std::uint32_t quantity = 0U,
        std::uint32_t aux_value = 0U);
    void queue_ui_panel_slot_action_message(
        Gs1UiPanelSlotId slot_id,
        std::uint32_t flags,
        const Gs1UiAction& action,
        std::uint8_t label_kind,
        std::uint32_t primary_id = 0U,
        std::uint32_t secondary_id = 0U,
        std::uint32_t quantity = 0U);
    void queue_ui_panel_list_item_message(
        Gs1UiPanelListId list_id,
        std::uint32_t item_id,
        std::uint32_t flags,
        std::uint8_t primary_kind,
        std::uint8_t secondary_kind,
        std::uint32_t primary_id = 0U,
        std::uint32_t secondary_id = 0U,
        std::uint32_t quantity = 0U,
        std::int32_t map_x = 0,
        std::int32_t map_y = 0);
    void queue_ui_panel_list_action_message(
        Gs1UiPanelListId list_id,
        std::uint32_t item_id,
        Gs1UiPanelListActionRole role,
        std::uint32_t flags,
        const Gs1UiAction& action);
    void queue_ui_setup_end_message();
    void queue_progression_view_end_message();
    void queue_ui_panel_end_message();
    void queue_clear_ui_setup_messages(Gs1UiSetupId setup_id);
    void queue_clear_ui_panel_messages(Gs1UiPanelId panel_id);
    void queue_main_menu_ui_messages();
    void queue_regional_map_menu_ui_messages();
    void queue_regional_map_selection_ui_messages();
    void queue_regional_map_tech_tree_ui_messages();
    void queue_site_result_ui_messages(
        std::uint32_t site_id,
        Gs1SiteAttemptResult result);
    void queue_site_protection_selector_ui_messages();
    void queue_regional_map_snapshot_messages();
    void queue_regional_map_site_upsert_message(
        const SiteMetaState& site,
        Gs1EngineMessageType message_type = GS1_ENGINE_MESSAGE_REGIONAL_MAP_SITE_UPSERT);
    void queue_site_snapshot_begin_message(
        Gs1ProjectionMode mode,
        Gs1EngineMessageType message_type = GS1_ENGINE_MESSAGE_BEGIN_SITE_SNAPSHOT);
    void queue_site_snapshot_end_message(Gs1EngineMessageType message_type = GS1_ENGINE_MESSAGE_END_SITE_SNAPSHOT);
    void queue_site_tile_upsert_message(
        std::uint32_t x,
        std::uint32_t y,
        Gs1EngineMessageType message_type = GS1_ENGINE_MESSAGE_SITE_TILE_UPSERT);
    void queue_all_site_tile_upsert_messages(
        Gs1EngineMessageType message_type = GS1_ENGINE_MESSAGE_SITE_TILE_UPSERT);
    void queue_pending_site_tile_upsert_messages(
        Gs1EngineMessageType message_type = GS1_ENGINE_MESSAGE_SITE_TILE_UPSERT);
    void queue_site_worker_update_message(
        Gs1EngineMessageType message_type = GS1_ENGINE_MESSAGE_SITE_WORKER_UPDATE);
    void queue_site_camp_update_message();
    void queue_site_weather_update_message();
    void queue_site_inventory_storage_upsert_message(
        std::uint32_t storage_id,
        Gs1EngineMessageType message_type = GS1_ENGINE_MESSAGE_SITE_INVENTORY_STORAGE_UPSERT);
    void queue_all_site_inventory_storage_upsert_messages(
        Gs1EngineMessageType message_type = GS1_ENGINE_MESSAGE_SITE_INVENTORY_STORAGE_UPSERT);
    void queue_site_inventory_slot_upsert_message(
        Gs1InventoryContainerKind container_kind,
        std::uint32_t slot_index,
        std::uint32_t storage_id = 0U,
        std::uint64_t container_owner_id = 0U,
        TileCoord container_tile = TileCoord {},
        Gs1EngineMessageType message_type = GS1_ENGINE_MESSAGE_SITE_INVENTORY_SLOT_UPSERT);
    void queue_site_inventory_view_state_message(
        std::uint32_t storage_id,
        Gs1InventoryViewEventKind event_kind,
        std::uint32_t slot_count = 0U,
        Gs1EngineMessageType message_type = GS1_ENGINE_MESSAGE_SITE_INVENTORY_VIEW_STATE);
    void queue_all_site_inventory_slot_upsert_messages(
        Gs1EngineMessageType message_type = GS1_ENGINE_MESSAGE_SITE_INVENTORY_SLOT_UPSERT);
    void queue_pending_site_inventory_slot_upsert_messages(
        Gs1EngineMessageType storage_message_type = GS1_ENGINE_MESSAGE_SITE_INVENTORY_STORAGE_UPSERT,
        Gs1EngineMessageType slot_message_type = GS1_ENGINE_MESSAGE_SITE_INVENTORY_SLOT_UPSERT,
        Gs1EngineMessageType view_state_message_type = GS1_ENGINE_MESSAGE_SITE_INVENTORY_VIEW_STATE);
    void queue_site_craft_context_messages();
    void queue_site_placement_preview_message();
    void queue_site_placement_preview_tile_upsert_message(const Gs1EngineMessagePlacementPreviewTileData& payload);
    void queue_site_placement_failure_message(const PlacementModeCommitRejectedMessage& payload);
    void queue_site_task_upsert_message(
        std::size_t task_index,
        Gs1EngineMessageType message_type = GS1_ENGINE_MESSAGE_SITE_TASK_UPSERT);
    void queue_all_site_task_upsert_messages(
        Gs1EngineMessageType message_type = GS1_ENGINE_MESSAGE_SITE_TASK_UPSERT);
    void queue_site_modifier_list_begin_message(
        Gs1ProjectionMode mode,
        Gs1EngineMessageType message_type = GS1_ENGINE_MESSAGE_SITE_MODIFIER_LIST_BEGIN);
    void queue_site_modifier_upsert_message(
        std::size_t modifier_index,
        Gs1EngineMessageType message_type = GS1_ENGINE_MESSAGE_SITE_MODIFIER_UPSERT);
    void queue_all_site_modifier_upsert_messages(
        Gs1ProjectionMode mode,
        Gs1EngineMessageType begin_message_type = GS1_ENGINE_MESSAGE_SITE_MODIFIER_LIST_BEGIN,
        Gs1EngineMessageType upsert_message_type = GS1_ENGINE_MESSAGE_SITE_MODIFIER_UPSERT);
    void queue_site_plant_visual_upsert_message(
        TileCoord coord,
        Gs1EngineMessageType message_type = GS1_ENGINE_MESSAGE_SITE_PLANT_VISUAL_UPSERT);
    void queue_site_plant_visual_remove_message(
        std::uint64_t visual_id,
        Gs1EngineMessageType message_type = GS1_ENGINE_MESSAGE_SITE_PLANT_VISUAL_REMOVE);
    void queue_all_site_plant_visual_messages(
        Gs1EngineMessageType message_type = GS1_ENGINE_MESSAGE_SITE_PLANT_VISUAL_UPSERT);
    void queue_pending_site_plant_visual_messages(
        Gs1EngineMessageType upsert_message_type = GS1_ENGINE_MESSAGE_SITE_PLANT_VISUAL_UPSERT,
        Gs1EngineMessageType remove_message_type = GS1_ENGINE_MESSAGE_SITE_PLANT_VISUAL_REMOVE);
    void queue_site_device_visual_upsert_message(
        TileCoord coord,
        Gs1EngineMessageType message_type = GS1_ENGINE_MESSAGE_SITE_DEVICE_VISUAL_UPSERT);
    void queue_site_device_visual_remove_message(
        std::uint64_t visual_id,
        Gs1EngineMessageType message_type = GS1_ENGINE_MESSAGE_SITE_DEVICE_VISUAL_REMOVE);
    void queue_all_site_device_visual_messages(
        Gs1EngineMessageType message_type = GS1_ENGINE_MESSAGE_SITE_DEVICE_VISUAL_UPSERT);
    void queue_pending_site_device_visual_messages(
        Gs1EngineMessageType upsert_message_type = GS1_ENGINE_MESSAGE_SITE_DEVICE_VISUAL_UPSERT,
        Gs1EngineMessageType remove_message_type = GS1_ENGINE_MESSAGE_SITE_DEVICE_VISUAL_REMOVE);
    void queue_site_phone_panel_state_message();
    void queue_site_protection_overlay_state_message();
    void queue_site_phone_listing_remove_message(
        std::uint32_t listing_id,
        Gs1EngineMessageType message_type = GS1_ENGINE_MESSAGE_SITE_PHONE_LISTING_REMOVE);
    void queue_site_phone_listing_upsert_message(
        std::size_t listing_index,
        Gs1EngineMessageType message_type = GS1_ENGINE_MESSAGE_SITE_PHONE_LISTING_UPSERT);
    void queue_all_site_phone_listing_upsert_messages(
        Gs1EngineMessageType upsert_message_type = GS1_ENGINE_MESSAGE_SITE_PHONE_LISTING_UPSERT,
        Gs1EngineMessageType remove_message_type = GS1_ENGINE_MESSAGE_SITE_PHONE_LISTING_REMOVE);
    void queue_site_bootstrap_messages();
    void queue_site_ready_bootstrap_messages();
    void queue_site_delta_messages(std::uint64_t dirty_flags);
    void queue_site_action_update_message();
    void queue_hud_state_message();
    void queue_campaign_resources_message();
    void queue_site_result_ready_message(
        std::uint32_t site_id,
        Gs1SiteAttemptResult result,
        std::uint32_t newly_revealed_site_count);
    void queue_task_reward_claimed_cue_message(
        std::uint32_t task_instance_id,
        std::uint32_t task_template_id,
        std::uint32_t reward_candidate_count);
    void queue_craft_output_stored_cue_message(
        std::uint32_t storage_id,
        std::uint32_t item_id,
        std::uint32_t quantity);
    void queue_campaign_unlock_cue_message(
        std::uint32_t subject_id,
        std::uint32_t detail_id,
        std::uint32_t detail_kind);
    void sync_campaign_unlock_presentations();
    void close_site_protection_ui() noexcept;
    void clear_pending_site_tile_projection_updates() noexcept;
    void clear_pending_site_inventory_projection_updates() noexcept;

private:
    GamePresentationRuntimeContext* active_context_ {nullptr};
    std::map<Gs1UiSetupId, Gs1UiSetupPresentationType> active_ui_setups_ {};
    std::unordered_set<Gs1UiPanelId> active_ui_panels_ {};
    std::optional<Gs1UiSetupId> active_normal_ui_setup_ {};
    std::optional<Gs1AppState> last_emitted_app_state_ {};
    std::vector<std::uint32_t> last_emitted_phone_listing_ids_ {};
    CampaignUnlockSnapshot last_campaign_unlock_snapshot_ {};
};
}  // namespace gs1

