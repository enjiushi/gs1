#pragma once

#include "gs1_godot_adapter_service.h"
#include "gs1_godot_panel_state_reducers.h"
#include "gs1_godot_projection_types.h"

#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/center_container.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/panel_container.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/core/binder_common.hpp>
#include <godot_cpp/core/object_id.hpp>
#include <godot_cpp/variant/array.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <unordered_map>
#include <unordered_set>

class Gs1GodotOverlayPanelController final
    : public godot::Control
    , public IGs1GodotEngineMessageSubscriber
{
    GDCLASS(Gs1GodotOverlayPanelController, godot::Control)

public:
    using SubmitUiActionFn = std::function<void(std::int64_t action_type, std::int64_t target_id, std::int64_t arg0, std::int64_t arg1)>;

    Gs1GodotOverlayPanelController() = default;
    ~Gs1GodotOverlayPanelController() override = default;

    void _ready() override;
    void _process(double delta) override;
    void _exit_tree() override;

    void cache_ui_references(godot::Control& owner);
    void set_submit_ui_action_callback(SubmitUiActionFn callback);
    void handle_overlay_mode_pressed(std::int64_t mode);
    void handle_projected_button_pressed(std::int64_t button_key);
    [[nodiscard]] bool handles_engine_message(Gs1EngineMessageType type) const noexcept override;
    void handle_engine_message(const Gs1EngineMessage& message) override;
    void handle_runtime_message_reset() override;

protected:
    static void _bind_methods();

private:
    struct ProjectedButtonRecord final
    {
        godot::ObjectID object_id {};
    };

    void cache_adapter_service();
    [[nodiscard]] godot::Control* resolve_owner_control();
    void submit_ui_action(std::int64_t action_type, std::int64_t target_id, std::int64_t arg0, std::int64_t arg1);
    void update_overlay_state_label();
    void update_protection_selector(const Gs1RuntimeUiSetupProjection* setup);
    void update_site_result(const Gs1RuntimeUiSetupProjection* setup);
    void reconcile_projected_buttons(
        godot::Node* container,
        std::unordered_map<std::uint64_t, ProjectedButtonRecord>& registry,
        const godot::Array& button_specs);
    [[nodiscard]] godot::Button* upsert_button_node(
        godot::Node* container,
        std::unordered_map<std::uint64_t, ProjectedButtonRecord>& registry,
        std::uint64_t stable_key,
        const godot::String& node_name,
        int desired_index);
    void prune_button_registry(
        std::unordered_map<std::uint64_t, ProjectedButtonRecord>& registry,
        const std::unordered_set<std::uint64_t>& desired_keys);
    [[nodiscard]] godot::String protection_label(const Gs1RuntimeUiElementProjection& element) const;
    [[nodiscard]] godot::String site_result_text(const Gs1RuntimeUiElementProjection& element) const;

    godot::Control* owner_control_ {nullptr};
    Gs1GodotAdapterService* adapter_service_ {nullptr};
    godot::Control* panel_ {nullptr};
    godot::Label* overlay_state_label_ {nullptr};
    godot::CenterContainer* protection_selector_overlay_ {nullptr};
    godot::PanelContainer* protection_selector_panel_ {nullptr};
    godot::Label* protection_selector_title_ {nullptr};
    godot::VBoxContainer* protection_selector_buttons_ {nullptr};
    godot::CenterContainer* site_result_overlay_ {nullptr};
    godot::PanelContainer* site_result_panel_ {nullptr};
    godot::Label* site_result_title_ {nullptr};
    godot::Label* site_result_summary_ {nullptr};
    godot::VBoxContainer* site_result_actions_ {nullptr};
    SubmitUiActionFn submit_ui_action_ {};
    Gs1GodotUiSetupStateReducer ui_setup_state_reducer_ {};
    std::optional<Gs1RuntimeProtectionOverlayProjection> overlay_state_ {};
    std::unordered_map<std::uint64_t, ProjectedButtonRecord> protection_selector_buttons_registry_ {};
    std::unordered_map<std::uint64_t, ProjectedButtonRecord> site_result_buttons_registry_ {};
};
