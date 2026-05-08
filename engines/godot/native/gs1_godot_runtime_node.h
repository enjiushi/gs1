#pragma once

#include "host/runtime_session.h"

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/core/binder_common.hpp>
#include <godot_cpp/variant/string.hpp>

#include <filesystem>
#include <string>
#include <vector>

class IGs1GodotEngineMessageSubscriber
{
public:
    virtual ~IGs1GodotEngineMessageSubscriber() = default;

    [[nodiscard]] virtual bool handles_engine_message(Gs1EngineMessageType type) const noexcept = 0;
    virtual void handle_engine_message(const Gs1EngineMessage& message) = 0;
    virtual void handle_runtime_message_reset() = 0;
};

class Gs1RuntimeNode final : public godot::Node
{
    GDCLASS(Gs1RuntimeNode, godot::Node)

public:
    Gs1RuntimeNode() = default;
    ~Gs1RuntimeNode() override;

    void _ready() override;
    void _process(double delta) override;

    [[nodiscard]] godot::String get_runtime_summary() const;

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
    void subscribe_engine_messages(IGs1GodotEngineMessageSubscriber& subscriber);
    void unsubscribe_engine_messages(IGs1GodotEngineMessageSubscriber& subscriber);

protected:
    static void _bind_methods();

private:
    void ensure_runtime_started();
    void process_frame(double delta_seconds);
    bool drain_projection_messages();
    void notify_runtime_message_reset();
    void dispatch_engine_message(const Gs1EngineMessage& message);
    [[nodiscard]] bool submit_ui_action(const Gs1UiAction& action);
    void refresh_gameplay_dll_path();
    void refresh_project_config_root();
    [[nodiscard]] std::string compute_default_gameplay_dll_path() const;
    [[nodiscard]] std::string compute_default_project_config_root() const;

private:
    std::filesystem::path gameplay_dll_path_ {};
    std::filesystem::path project_config_root_ {};
    Gs1AdapterConfigBlob adapter_config_ {};
    Gs1RuntimeSession runtime_session_ {};
    std::vector<IGs1GodotEngineMessageSubscriber*> engine_message_subscribers_ {};
    std::string last_error_ {};
};

using Gs1GodotRuntimeNode = Gs1RuntimeNode;
