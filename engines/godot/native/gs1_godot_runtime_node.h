#pragma once

#include "host/runtime_message_pump.h"
#include "host/runtime_projection_state.h"
#include "host/runtime_session.h"

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/core/binder_common.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>

#include <filesystem>
#include <string>
#include <vector>

class Gs1RuntimeNode final : public godot::Node
{
    GDCLASS(Gs1RuntimeNode, godot::Node)

public:
    Gs1RuntimeNode() = default;
    ~Gs1RuntimeNode() override;

    void _ready() override;
    void _process(double delta) override;

    [[nodiscard]] godot::String get_runtime_summary() const;
    [[nodiscard]] godot::Dictionary get_projection() const;
    [[nodiscard]] std::uint64_t projection_revision() const noexcept { return projection_revision_; }
    [[nodiscard]] bool has_active_site() const;
    [[nodiscard]] int get_site_width() const;
    [[nodiscard]] int get_site_height() const;
    [[nodiscard]] double get_worker_tile_x() const;
    [[nodiscard]] double get_worker_tile_y() const;
    [[nodiscard]] int get_tile_count() const;
    [[nodiscard]] godot::Dictionary get_tile_at_index(int index) const;

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

    [[nodiscard]] const std::string& last_error() const noexcept { return last_error_; }
    [[nodiscard]] const Gs1RuntimeProjectionState& projection_state() const noexcept { return projection_cache_.state(); }
    [[nodiscard]] std::vector<Gs1EngineMessage> take_drained_messages();

protected:
    static void _bind_methods();

private:
    void ensure_runtime_started();
    void process_frame(double delta_seconds);
    bool drain_projection_messages();
    [[nodiscard]] bool submit_ui_action(const Gs1UiAction& action);
    void refresh_gameplay_dll_path();
    void refresh_project_config_root();
    [[nodiscard]] std::string compute_default_gameplay_dll_path() const;
    [[nodiscard]] std::string compute_default_project_config_root() const;

    [[nodiscard]] godot::Dictionary build_projection_dictionary() const;
    [[nodiscard]] godot::Dictionary build_regional_map_dictionary() const;
    [[nodiscard]] godot::Dictionary build_campaign_resources_dictionary() const;
    [[nodiscard]] godot::Dictionary build_hud_dictionary() const;
    [[nodiscard]] godot::Dictionary build_site_action_dictionary() const;
    [[nodiscard]] godot::Dictionary build_site_result_dictionary() const;
    [[nodiscard]] godot::Array build_ui_setups_array() const;
    [[nodiscard]] godot::Array build_progression_views_array() const;
    [[nodiscard]] godot::Array build_ui_panels_array() const;
    [[nodiscard]] godot::Dictionary build_active_site_dictionary() const;
    [[nodiscard]] godot::Array build_recent_one_shot_cues_array() const;
    [[nodiscard]] godot::Array build_inventory_slot_array(const std::vector<Gs1RuntimeInventorySlotProjection>& slots) const;

private:
    std::filesystem::path gameplay_dll_path_ {};
    std::filesystem::path project_config_root_ {};
    Gs1AdapterConfigBlob adapter_config_ {};
    Gs1RuntimeSession runtime_session_ {};
    Gs1RuntimeProjectionCache projection_cache_ {};
    Gs1RuntimeMessagePump message_pump_ {};
    std::vector<Gs1EngineMessage> drained_messages_ {};
    std::uint64_t projection_revision_ {0};
    std::string last_error_ {};
};

using Gs1GodotRuntimeNode = Gs1RuntimeNode;
