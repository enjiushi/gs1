#pragma once

#include "app/game_presentation_coordinator.h"
#include "campaign/campaign_state.h"
#include "messages/game_message.h"
#include "runtime/runtime_clock.h"
#include "site/site_run_state.h"
#include "gs1/status.h"
#include "gs1/types.h"

#include <array>
#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <vector>

namespace gs1
{
inline constexpr std::uint32_t k_api_version = 6;
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
        FactionReputation = 2,
        Technology = 3,
        ActionExecution = 4,
        WeatherEvent = 5,
        WorkerCondition = 6,
        Ecology = 7,
        PlantWeatherContribution = 8,
        DeviceWeatherContribution = 9,
        TaskBoard = 10,
        PlacementValidation = 11,
        LocalWeatherResolve = 12,
        Inventory = 13,
        Craft = 14,
        EconomyPhone = 15,
        PhonePanel = 16,
        CampDurability = 17,
        DeviceSupport = 18,
        DeviceMaintenance = 19,
        Modifier = 20
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
    [[nodiscard]] GamePresentationRuntimeContext make_presentation_context() noexcept;
    void activate_loaded_site_scene();
    void mark_site_projection_update_dirty(std::uint64_t dirty_flags) noexcept;
    void mark_site_tile_projection_dirty(TileCoord coord) noexcept;
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
    [[nodiscard]] Gs1Status translate_site_inventory_slot_tap_to_message(
        const Gs1HostEventSiteInventorySlotTapData& request,
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
    std::string adapter_config_json_utf8_ {};
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
    GamePresentationCoordinator presentation_ {};
    bool boot_initialized_ {false};
};

}  // namespace gs1
