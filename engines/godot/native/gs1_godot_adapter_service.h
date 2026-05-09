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

class Gs1GodotAdapterService final
{
public:
    Gs1GodotAdapterService() = default;
    ~Gs1GodotAdapterService();

    void process_frame(double delta_seconds);
    void begin_engine_message_buffering();
    void flush_buffered_engine_messages();
    void clear_buffered_engine_messages() noexcept;

    void subscribe(Gs1EngineMessageType type, IGs1GodotEngineMessageSubscriber& subscriber);
    void subscribe_matching_messages(IGs1GodotEngineMessageSubscriber& subscriber);
    void unsubscribe(Gs1EngineMessageType type, IGs1GodotEngineMessageSubscriber& subscriber);
    void unsubscribe_matching_messages(IGs1GodotEngineMessageSubscriber& subscriber);
    void unsubscribe_all(IGs1GodotEngineMessageSubscriber& subscriber);

    [[nodiscard]] bool submit_ui_action(std::int64_t action_type, std::int64_t target_id = 0, std::int64_t arg0 = 0, std::int64_t arg1 = 0);
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

    [[nodiscard]] const std::string& last_error() const noexcept { return last_error_; }
    [[nodiscard]] bool is_running() const noexcept { return runtime_session_.is_running(); }

private:
    static constexpr std::size_t k_message_bucket_count = 256U;

    void ensure_debug_http_server_initialized();
    void ensure_runtime_started();
    [[nodiscard]] bool drain_debug_http_commands();
    bool drain_projection_messages();
    void notify_runtime_message_reset();
    void dispatch_engine_message(Gs1EngineMessage&& message);
    void dispatch_or_buffer_engine_message(Gs1EngineMessage&& message);
    [[nodiscard]] bool submit_ui_action(const Gs1UiAction& action);
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
    std::mutex pending_debug_http_commands_mutex_ {};
    std::vector<Gs1GodotDebugHttpCommand> pending_debug_http_commands_ {};
    std::vector<Gs1EngineMessage> buffered_engine_messages_ {};
    std::string last_error_ {};
};
