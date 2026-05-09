#pragma once

#include "gs1_godot_adapter_service.h"
#include "gs1_godot_panel_state_reducers.h"

#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/rich_text_label.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/core/binder_common.hpp>
#include <godot_cpp/core/object_id.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <unordered_map>
#include <unordered_set>

class Gs1GodotRegionalSelectionPanelController final
    : public godot::Control
    , public IGs1GodotEngineMessageSubscriber
{
    GDCLASS(Gs1GodotRegionalSelectionPanelController, godot::Control)

public:
    using SubmitUiActionFn = std::function<void(std::int64_t action_type, std::int64_t target_id, std::int64_t arg0, std::int64_t arg1)>;

    Gs1GodotRegionalSelectionPanelController() = default;
    ~Gs1GodotRegionalSelectionPanelController() override = default;

    void _ready() override;
    void _process(double delta) override;
    void _exit_tree() override;

    void cache_ui_references(godot::Control& owner);
    void set_submit_ui_action_callback(SubmitUiActionFn callback);
    void handle_action_pressed(std::int64_t button_key);
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
    void rebuild_selection_panel();
    void reconcile_action_buttons(const godot::Array& button_specs);
    [[nodiscard]] godot::Button* upsert_button_node(
        godot::Node* container,
        std::unordered_map<std::uint64_t, ProjectedButtonRecord>& registry,
        std::uint64_t stable_key,
        const godot::String& node_name,
        int desired_index);
    void prune_button_registry(
        std::unordered_map<std::uint64_t, ProjectedButtonRecord>& registry,
        const std::unordered_set<std::uint64_t>& desired_keys);
    [[nodiscard]] godot::String item_name_for(int item_id) const;
    [[nodiscard]] godot::String faction_name_for(int faction_id) const;
    [[nodiscard]] godot::String panel_text_line(const Gs1RuntimeUiPanelTextProjection& line) const;
    [[nodiscard]] godot::String panel_slot_label(const Gs1RuntimeUiPanelSlotActionProjection& slot_action) const;
    [[nodiscard]] godot::String selection_action_label(const godot::String& text, const godot::Dictionary& action) const;
    [[nodiscard]] godot::String site_button_text(const Gs1RuntimeRegionalMapSiteProjection& site) const;
    [[nodiscard]] godot::String site_tooltip(const Gs1RuntimeRegionalMapSiteProjection& site) const;
    [[nodiscard]] godot::String site_state_name(int site_state) const;
    [[nodiscard]] godot::String site_deployment_summary(const Gs1RuntimeRegionalMapSiteProjection& site) const;
    [[nodiscard]] godot::String support_preview_text(int preview_mask) const;
    [[nodiscard]] godot::Vector2i regional_grid_coord(const Gs1RuntimeRegionalMapSiteProjection& site) const;
    void refresh_support_summary(const Gs1RuntimeUiPanelProjection* selection_panel);
    void refresh_loadout_summary(const Gs1RuntimeUiPanelProjection* selection_panel);

    godot::Control* owner_control_ {nullptr};
    Gs1GodotAdapterService* adapter_service_ {nullptr};
    godot::Control* panel_ {nullptr};
    godot::Label* title_ {nullptr};
    godot::RichTextLabel* summary_ {nullptr};
    godot::RichTextLabel* support_summary_ {nullptr};
    godot::RichTextLabel* loadout_summary_ {nullptr};
    godot::VBoxContainer* actions_ {nullptr};
    SubmitUiActionFn submit_ui_action_ {};
    Gs1GodotUiPanelStateReducer ui_panel_state_reducer_ {k_gs1_regional_selection_ui_panel_message_family};
    Gs1GodotRegionalMapStateReducer regional_map_state_reducer_ {k_gs1_regional_selection_message_family};
    int selected_site_id_ {0};
    std::unordered_map<std::uint64_t, ProjectedButtonRecord> action_buttons_ {};
    mutable std::unordered_map<int, godot::String> item_name_cache_ {};
    mutable std::unordered_map<int, godot::String> faction_name_cache_ {};
};
