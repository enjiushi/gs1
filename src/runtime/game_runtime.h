#pragma once

#include "campaign/campaign_state.h"
#include "messages/game_message.h"
#include "site/site_run_state.h"
#include "gs1/status.h"
#include "gs1/types.h"

#include <array>
#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <vector>

namespace gs1
{
inline constexpr std::uint32_t k_api_version = 4;
inline constexpr double k_default_fixed_step_seconds = 0.25;
inline constexpr std::size_t k_feedback_event_type_count = 4U;

class GameRuntime final
{
public:
    explicit GameRuntime(Gs1RuntimeCreateDesc create_desc);

    [[nodiscard]] Gs1Status submit_host_events(
        const Gs1HostEvent* events,
        std::uint32_t event_count);
    [[nodiscard]] Gs1Status submit_feedback_events(
        const Gs1FeedbackEvent* events,
        std::uint32_t event_count);
    [[nodiscard]] Gs1Status run_phase1(const Gs1Phase1Request& request, Gs1Phase1Result& out_result);
    [[nodiscard]] Gs1Status run_phase2(const Gs1Phase2Request& request, Gs1Phase2Result& out_result);
    [[nodiscard]] Gs1Status pop_engine_message(Gs1EngineMessage& out_message);
    [[nodiscard]] Gs1Status get_profiling_snapshot(Gs1RuntimeProfilingSnapshot& out_snapshot) const noexcept;
    void reset_profiling() noexcept;
    [[nodiscard]] Gs1Status set_profiled_system_enabled(
        Gs1RuntimeProfileSystemId system_id,
        bool enabled) noexcept;
    [[nodiscard]] bool profiled_system_enabled(Gs1RuntimeProfileSystemId system_id) const noexcept;

    [[nodiscard]] GameMessageQueue& message_queue() noexcept { return message_queue_; }
    [[nodiscard]] Gs1Status handle_message(const GameMessage& message);

    friend struct GameRuntimeProjectionTestAccess;
    friend class MessageDispatcher;

private:
    enum class HostEventDispatchStage : std::uint8_t
    {
        Phase1,
        Phase2
    };

    struct Phase1SiteMoveDirection final
    {
        float world_move_x {0.0f};
        float world_move_y {0.0f};
        float world_move_z {0.0f};
        bool present {false};
    };

    enum class MessageSubscriberId : std::uint8_t
    {
        CampaignFlow = 0,
        LoadoutPlanner = 1,
        ActionExecution = 2,
        WeatherEvent = 3,
        WorkerCondition = 4,
        Ecology = 5,
        TaskBoard = 6,
        PlacementValidation = 7,
        LocalWeatherResolve = 8,
        Inventory = 9,
        Craft = 10,
        EconomyPhone = 11,
        PhonePanel = 12,
        CampDurability = 13,
        DeviceSupport = 14,
        DeviceMaintenance = 15,
        Modifier = 16
    };

    enum class FeedbackEventSubscriberId : std::uint8_t
    {
        Reserved = 0
    };

    using MessageSubscriberList = std::vector<MessageSubscriberId>;
    using FeedbackEventSubscriberList = std::vector<FeedbackEventSubscriberId>;

    struct TimingAccumulator final
    {
        std::uint64_t sample_count {0U};
        double last_elapsed_ms {0.0};
        double total_elapsed_ms {0.0};
        double max_elapsed_ms {0.0};
    };

    struct ProfiledSystemState final
    {
        bool enabled {true};
        TimingAccumulator run_timing {};
        TimingAccumulator message_timing {};
    };

    void initialize_subscription_tables();
    void queue_log_message(const char* message);
    void queue_app_state_message(Gs1AppState app_state);
    void queue_ui_setup_begin_message(
        Gs1UiSetupId setup_id,
        Gs1UiSetupPresentationType presentation_type,
        std::uint32_t element_count,
        std::uint32_t context_id);
    void queue_ui_setup_close_message(
        Gs1UiSetupId setup_id,
        Gs1UiSetupPresentationType presentation_type);
    void queue_close_ui_setup_if_open(Gs1UiSetupId setup_id);
    void queue_close_active_normal_ui_if_open();
    void queue_ui_element_message(
        std::uint32_t element_id,
        Gs1UiElementType element_type,
        std::uint32_t flags,
        const Gs1UiAction& action,
        const char* text);
    void queue_ui_setup_end_message();
    void queue_clear_ui_setup_messages(Gs1UiSetupId setup_id);
    void queue_main_menu_ui_messages();
    void queue_regional_map_selection_ui_messages();
    void queue_site_result_ui_messages(std::uint32_t site_id, Gs1SiteAttemptResult result);
    void queue_regional_map_snapshot_messages();
    void queue_regional_map_site_upsert_message(const SiteMetaState& site);
    void queue_site_snapshot_begin_message(Gs1ProjectionMode mode);
    void queue_site_snapshot_end_message();
    void queue_site_tile_upsert_message(std::uint32_t x, std::uint32_t y);
    void queue_all_site_tile_upsert_messages();
    void queue_pending_site_tile_upsert_messages();
    void queue_site_worker_update_message();
    void queue_site_camp_update_message();
    void queue_site_weather_update_message();
    void queue_site_inventory_storage_upsert_message(std::uint32_t storage_id);
    void queue_all_site_inventory_storage_upsert_messages();
    void queue_site_inventory_slot_upsert_message(
        Gs1InventoryContainerKind container_kind,
        std::uint32_t slot_index,
        std::uint32_t storage_id = 0U,
        std::uint64_t container_owner_id = 0U,
        TileCoord container_tile = TileCoord {});
    void queue_site_inventory_view_state_message(
        std::uint32_t storage_id,
        Gs1InventoryViewEventKind event_kind,
        std::uint32_t slot_count = 0U);
    void queue_all_site_inventory_slot_upsert_messages();
    void queue_pending_site_inventory_slot_upsert_messages();
    void queue_site_craft_context_messages();
    void queue_site_placement_preview_message();
    void queue_site_placement_failure_message(const PlacementModeCommitRejectedMessage& payload);
    void queue_site_task_upsert_message(std::size_t task_index);
    void queue_all_site_task_upsert_messages();
    void queue_site_phone_panel_state_message();
    void queue_site_phone_listing_remove_message(std::uint32_t listing_id);
    void queue_site_phone_listing_upsert_message(std::size_t listing_index);
    void queue_all_site_phone_listing_upsert_messages();
    void queue_site_bootstrap_messages();
    void queue_site_delta_messages(std::uint64_t dirty_flags);
    void queue_site_action_update_message();
    void queue_hud_state_message();
    void queue_site_result_ready_message(
        std::uint32_t site_id,
        Gs1SiteAttemptResult result,
        std::uint32_t newly_revealed_site_count);
    void mark_site_projection_update_dirty(std::uint64_t dirty_flags) noexcept;
    void mark_site_tile_projection_dirty(TileCoord coord) noexcept;
    void clear_pending_site_tile_projection_updates() noexcept;
    void clear_pending_site_inventory_projection_updates() noexcept;
    void flush_site_presentation_if_dirty();
    [[nodiscard]] Gs1Status translate_ui_action_to_message(const Gs1UiAction& action, GameMessage& out_message) const;
    [[nodiscard]] Gs1Status translate_site_action_request_to_message(
        const Gs1HostEventSiteActionRequestData& action,
        GameMessage& out_message) const;
    [[nodiscard]] Gs1Status translate_site_action_cancel_to_message(
        const Gs1HostEventSiteActionCancelData& action,
        GameMessage& out_message) const;
    [[nodiscard]] Gs1Status translate_site_storage_view_to_message(
        const Gs1HostEventSiteStorageViewData& request,
        GameMessage& out_message) const;
    [[nodiscard]] Gs1Status translate_site_context_request_to_message(
        const Gs1HostEventSiteContextRequestData& request,
        GameMessage& out_message) const;
    [[nodiscard]] Gs1Status dispatch_host_events(
        HostEventDispatchStage stage,
        std::uint32_t& out_processed_count);
    [[nodiscard]] Gs1Status dispatch_feedback_events(std::uint32_t& out_processed_count);
    [[nodiscard]] Gs1Status dispatch_queued_messages();
    [[nodiscard]] Gs1Status dispatch_subscribed_message(const GameMessage& message);
    [[nodiscard]] Gs1Status dispatch_subscribed_feedback_event(const Gs1FeedbackEvent& event);
    [[nodiscard]] Gs1Status cancel_pending_device_storage_open_request();
    void sync_after_processed_message(const GameMessage& message);
    void run_fixed_step();
    void copy_timing_snapshot(
        const TimingAccumulator& source,
        Gs1RuntimeTimingStats& destination) const noexcept;

private:
    Gs1RuntimeCreateDesc create_desc_ {};
    double fixed_step_seconds_ {k_default_fixed_step_seconds};
    Gs1AppState app_state_ {GS1_APP_STATE_BOOT};
    std::optional<CampaignState> campaign_ {};
    std::optional<SiteRunState> active_site_run_ {};
    Phase1SiteMoveDirection phase1_site_move_direction_ {};
    std::deque<Gs1HostEvent> host_events_ {};
    std::deque<Gs1FeedbackEvent> feedback_events_ {};
    GameMessageQueue message_queue_ {};
    std::array<MessageSubscriberList, k_game_message_type_count> message_subscribers_ {};
    std::array<FeedbackEventSubscriberList, k_feedback_event_type_count> feedback_event_subscribers_ {};
    std::deque<Gs1EngineMessage> engine_messages_ {};
    TimingAccumulator phase1_timing_ {};
    TimingAccumulator phase2_timing_ {};
    TimingAccumulator fixed_step_timing_ {};
    std::array<ProfiledSystemState, static_cast<std::size_t>(GS1_RUNTIME_PROFILE_SYSTEM_COUNT)>
        profiled_systems_ {};
    std::map<Gs1UiSetupId, Gs1UiSetupPresentationType> active_ui_setups_ {};
    std::optional<Gs1UiSetupId> active_normal_ui_setup_ {};
    std::optional<Gs1AppState> last_emitted_app_state_ {};
    std::vector<std::uint32_t> last_emitted_phone_listing_ids_ {};
    bool boot_initialized_ {false};
};

}  // namespace gs1
