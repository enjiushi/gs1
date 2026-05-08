#pragma once

#include "gs1_godot_regional_map_hud_controller.h"
#include "gs1_godot_regional_selection_panel_controller.h"
#include "gs1_godot_regional_summary_panel_controller.h"
#include "gs1_godot_regional_tech_tree_panel_controller.h"
#include "gs1_godot_runtime_node.h"

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/core/binder_common.hpp>
#include <godot_cpp/variant/node_path.hpp>

#include <cstdint>

class Gs1GodotRegionalMapUiController final
    : public godot::Control
    , public IGs1GodotEngineMessageSubscriber
{
    GDCLASS(Gs1GodotRegionalMapUiController, godot::Control)

public:
    Gs1GodotRegionalMapUiController() = default;
    ~Gs1GodotRegionalMapUiController() override = default;

    void _ready() override;
    void _process(double delta) override;

    void set_runtime_node_path(const godot::NodePath& path);
    [[nodiscard]] godot::NodePath get_runtime_node_path() const;
    void set_ui_root_path(const godot::NodePath& path);
    [[nodiscard]] godot::NodePath get_ui_root_path() const;

    void disconnect_runtime_subscriptions();
    void apply_bootstrap_app_state(int app_state);
    [[nodiscard]] bool handles_engine_message(Gs1EngineMessageType type) const noexcept override;
    void handle_engine_message(const Gs1EngineMessage& message) override;
    void handle_runtime_message_reset() override;

protected:
    static void _bind_methods();

private:
    void cache_runtime_reference();
    void cache_ui_references();
    [[nodiscard]] godot::Control* resolve_ui_root();
    void refresh_visibility();
    void submit_ui_action(std::int64_t action_type, std::int64_t target_id = 0, std::int64_t arg0 = 0, std::int64_t arg1 = 0);

private:
    godot::NodePath runtime_node_path_ {};
    godot::NodePath ui_root_path_ {".."};
    Gs1RuntimeNode* runtime_node_ {nullptr};
    godot::Control* ui_root_ {nullptr};
    Gs1GodotRegionalMapHudController regional_map_hud_controller_ {};
    Gs1GodotRegionalSelectionPanelController regional_selection_panel_controller_ {};
    Gs1GodotRegionalSummaryPanelController regional_summary_panel_controller_ {};
    Gs1GodotRegionalTechTreePanelController regional_tech_tree_panel_controller_ {};
    int last_app_state_ {0};
    bool selection_visible_ {false};
    bool tech_tree_visible_ {false};
};
