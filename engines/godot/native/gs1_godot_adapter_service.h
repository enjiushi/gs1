#pragma once

#include "gs1_godot_debug_http_protocol.h"
#include "gs1_godot_debug_http_server.h"
#include "host/runtime_session.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <unordered_set>
#include <vector>

class IGs1GodotEngineMessageSubscriber
{
public:
    virtual ~IGs1GodotEngineMessageSubscriber() = default;

    [[nodiscard]] virtual bool handles_engine_message(Gs1EngineMessageType type) const noexcept = 0;
    virtual void handle_engine_message(const Gs1EngineMessage& message) = 0;
    virtual void handle_runtime_message_reset() = 0;
};

enum class Gs1GodotPhoneSection : std::uint8_t
{
    Home = 0,
    Tasks = 1,
    Buy = 2,
    Sell = 3,
    Hire = 4,
    Cart = 5
};

enum class Gs1GodotProtectionOverlayMode : std::uint8_t
{
    None = 0,
    Wind = 1,
    Heat = 2,
    Dust = 3,
    OccupantCondition = 4
};

inline constexpr std::uint32_t GS1_GODOT_PHONE_BADGE_LAUNCHER = 1U << 0U;
inline constexpr std::uint32_t GS1_GODOT_PHONE_BADGE_TASKS = 1U << 1U;
inline constexpr std::uint32_t GS1_GODOT_PHONE_BADGE_BUY = 1U << 2U;
inline constexpr std::uint32_t GS1_GODOT_PHONE_BADGE_SELL = 1U << 3U;
inline constexpr std::uint32_t GS1_GODOT_PHONE_BADGE_HIRE = 1U << 4U;

struct Gs1GodotPhoneUiSessionState final
{
    bool open {false};
    bool notification_state_initialized {false};
    Gs1GodotPhoneSection active_section {Gs1GodotPhoneSection::Home};
    std::uint32_t badge_flags {0U};
    std::uint64_t task_snapshot_signature {0U};
    std::uint64_t buy_snapshot_signature {0U};
    std::uint64_t sell_snapshot_signature {0U};
    std::uint64_t service_snapshot_signature {0U};
};

struct Gs1GodotRegionalTechUiSessionState final
{
    bool open {false};
    std::uint32_t selected_faction_id {1U};
};

struct Gs1GodotProtectionUiSessionState final
{
    bool selector_open {false};
    Gs1GodotProtectionOverlayMode overlay_mode {Gs1GodotProtectionOverlayMode::None};
};

struct Gs1GodotInventoryUiSessionState final
{
    bool worker_pack_open {false};
    std::uint32_t opened_storage_id {0U};
};

struct Gs1GodotUiSessionState final
{
    Gs1GodotPhoneUiSessionState phone {};
    Gs1GodotRegionalTechUiSessionState regional_tech {};
    Gs1GodotProtectionUiSessionState protection {};
    Gs1GodotInventoryUiSessionState inventory {};
    Gs1AppState app_state {GS1_APP_STATE_BOOT};
    std::uint64_t revision {0U};
};

class Gs1GodotAdapterService final
{
public:
    Gs1GodotAdapterService() = default;
    ~Gs1GodotAdapterService();

    void begin_frame(double delta_seconds);
    void finish_frame();
    void begin_engine_message_buffering();
    void flush_buffered_engine_messages();
    void clear_buffered_engine_messages() noexcept;

    void subscribe(Gs1EngineMessageType type, IGs1GodotEngineMessageSubscriber& subscriber);
    void subscribe_matching_messages(IGs1GodotEngineMessageSubscriber& subscriber);
    void unsubscribe(Gs1EngineMessageType type, IGs1GodotEngineMessageSubscriber& subscriber);
    void unsubscribe_matching_messages(IGs1GodotEngineMessageSubscriber& subscriber);
    void unsubscribe_all(IGs1GodotEngineMessageSubscriber& subscriber);

    [[nodiscard]] bool submit_gameplay_action(std::int64_t action_type, std::int64_t target_id = 0, std::int64_t arg0 = 0, std::int64_t arg1 = 0);
    [[nodiscard]] bool submit_move_direction(double world_move_x, double world_move_y, double world_move_z);
    [[nodiscard]] bool submit_site_context_request(std::int64_t tile_x, std::int64_t tile_y, std::int64_t flags);
    [[nodiscard]] bool submit_site_action_request(
        std::int64_t action_kind,
        std::int64_t flags,
        std::int64_t quantity,
        std::int64_t tile_x,
        std::int64_t tile_y,
        std::int64_t primary_subject_id,
        std::int64_t secondary_subject_id,
        std::int64_t item_id);
    [[nodiscard]] bool submit_site_action_cancel(std::int64_t action_id, std::int64_t flags);
    [[nodiscard]] bool submit_site_storage_view(std::int64_t storage_id, std::int64_t event_kind);
    [[nodiscard]] bool submit_site_inventory_slot_tap(
        std::int64_t storage_id,
        std::int64_t container_kind,
        std::int64_t slot_index,
        std::int64_t item_instance_id);
    [[nodiscard]] bool submit_site_scene_ready();
    [[nodiscard]] bool get_game_state_view(Gs1GameStateView& out_view);
    [[nodiscard]] bool query_site_tile_view(std::uint32_t tile_index, Gs1SiteTileView& out_tile);
    [[nodiscard]] const Gs1GodotUiSessionState& ui_session_state() const noexcept
    {
        return ui_session_state_;
    }

    [[nodiscard]] const std::string& last_error() const noexcept { return last_error_; }
    [[nodiscard]] bool is_running() const noexcept { return runtime_session_.is_running(); }

private:
    static constexpr std::size_t k_message_bucket_count = 256U;

    void ensure_debug_http_server_initialized();
    void ensure_runtime_started();
    void fail_runtime_session(const char* fallback_error_message);
    [[nodiscard]] bool drain_debug_http_commands();
    bool drain_projection_messages();
    bool poll_gameplay_state_notifications();
    void notify_runtime_message_reset();
    void dispatch_engine_message(Gs1EngineMessage&& message);
    void dispatch_or_buffer_engine_message(Gs1EngineMessage&& message);
    [[nodiscard]] bool submit_gameplay_action(const Gs1GameplayAction& action);
    [[nodiscard]] bool handle_local_gameplay_action(const Gs1GameplayAction& action);
    [[nodiscard]] bool handle_local_storage_view(
        std::uint32_t storage_id,
        Gs1InventoryViewEventKind event_kind);
    void reset_ui_session_state() noexcept;
    void bump_ui_session_revision(std::uint32_t dirty_flags);
    void sync_phone_badges_from_view(const Gs1GameStateView& view);
    void sync_ui_session_from_view(const Gs1GameStateView& view);
    [[nodiscard]] bool queue_debug_http_command(std::string_view path, const std::string& body, std::string& out_error);
    void refresh_gameplay_dll_path();
    void refresh_project_config_root();
    [[nodiscard]] std::string compute_default_gameplay_dll_path() const;
    [[nodiscard]] std::string compute_default_project_config_root() const;
    void remember_subscriber(IGs1GodotEngineMessageSubscriber& subscriber);
    [[nodiscard]] static std::size_t bucket_index(Gs1EngineMessageType type) noexcept;

private:
    std::filesystem::path gameplay_dll_path_ {};
    std::filesystem::path project_config_root_ {};
    Gs1AdapterConfigBlob adapter_config_ {};
    Gs1RuntimeSession runtime_session_ {};
    std::array<std::vector<IGs1GodotEngineMessageSubscriber*>, k_message_bucket_count> subscribers_by_message_ {};
    std::unordered_set<IGs1GodotEngineMessageSubscriber*> known_subscribers_ {};
    Gs1GodotDebugHttpServer debug_http_server_ {};
    bool debug_http_server_checked_ {false};
    bool engine_message_buffering_active_ {false};
    bool phase2_pending_ {false};
    double pending_phase1_delta_seconds_ {1.0 / 60.0};
    std::mutex pending_debug_http_commands_mutex_ {};
    std::vector<Gs1GodotDebugHttpCommand> pending_debug_http_commands_ {};
    std::vector<Gs1EngineMessage> buffered_engine_messages_ {};
    Gs1GodotUiSessionState ui_session_state_ {};
    Gs1AppState last_dispatched_gameplay_app_state_ {GS1_APP_STATE_BOOT};
    bool has_dispatched_gameplay_app_state_ {false};
    std::string last_error_ {};
};
