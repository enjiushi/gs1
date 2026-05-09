#pragma once

#include "gs1_godot_adapter_service.h"
#include "gs1_godot_panel_state_reducers.h"

#include <godot_cpp/classes/base_button.hpp>
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/core/binder_common.hpp>
#include <godot_cpp/core/object_id.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <unordered_map>
#include <unordered_set>

class Gs1GodotActionPanelController final
    : public godot::Control
    , public IGs1GodotEngineMessageSubscriber
{
    GDCLASS(Gs1GodotActionPanelController, godot::Control)

public:
    using SubmitUiActionFn = std::function<void(std::int64_t action_type, std::int64_t target_id, std::int64_t arg0, std::int64_t arg1)>;

    Gs1GodotActionPanelController() = default;
    ~Gs1GodotActionPanelController() override = default;

    void _ready() override;
    void _process(double delta) override;
    void _exit_tree() override;

    void cache_ui_references(godot::Control& owner);
    void set_submit_ui_action_callback(SubmitUiActionFn callback);
    void handle_open_protection_pressed();
    void handle_fixed_slot_pressed(std::int64_t button_id);
    void handle_site_control_pressed(std::int64_t button_key);
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

    struct FixedSlotBinding final
    {
        godot::ObjectID object_id {};
        int panel_id {0};
        int slot_id {0};
    };

    void cache_adapter_service();
    [[nodiscard]] godot::Control* resolve_owner_control();
    void submit_ui_action(std::int64_t action_type, std::int64_t target_id, std::int64_t arg0, std::int64_t arg1);
    void cache_fixed_slot_bindings();
    void clear_fixed_slot_actions();
    void bind_fixed_slot_actions(const Gs1RuntimeUiPanelProjection* panel, int panel_id);
    void apply_action_to_button(godot::BaseButton* button, const godot::Dictionary& action, const godot::String& fallback_label);
    void clear_action_from_button(godot::BaseButton* button, bool preserve_text = true);
    void reconcile_site_control_buttons();
    void reconcile_projected_action_buttons(
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
    [[nodiscard]] const Gs1RuntimeUiPanelProjection* find_ui_panel(int panel_id) const;
    [[nodiscard]] const Gs1RuntimeUiPanelSlotActionProjection* find_panel_slot_action(
        const Gs1RuntimeUiPanelProjection& panel,
        int slot_id) const;
    [[nodiscard]] godot::String panel_slot_label(const Gs1RuntimeUiPanelSlotActionProjection& slot_action) const;

    godot::Control* owner_control_ {nullptr};
    godot::VBoxContainer* site_controls_ {nullptr};
    Gs1GodotAdapterService* adapter_service_ {nullptr};
    SubmitUiActionFn submit_ui_action_ {};
    Gs1GodotUiSetupStateReducer ui_setup_state_reducer_ {};
    Gs1GodotUiPanelStateReducer ui_panel_state_reducer_ {};
    std::vector<FixedSlotBinding> fixed_slot_bindings_ {};
    std::unordered_map<std::uint64_t, ProjectedButtonRecord> site_control_buttons_ {};
    bool fixed_slot_bindings_cached_ {false};
};
