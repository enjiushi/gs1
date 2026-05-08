#pragma once

#include "gs1_godot_action_panel_controller.h"
#include "gs1_godot_craft_panel_controller.h"
#include "gs1_godot_input_dispatcher.h"
#include "gs1_godot_inventory_panel_controller.h"
#include "gs1_godot_overlay_panel_controller.h"
#include "gs1_godot_panel_state_reducers.h"
#include "gs1_godot_phone_panel_controller.h"
#include "gs1_godot_regional_tech_tree_panel_controller.h"
#include "gs1_godot_runtime_node.h"
#include "gs1_godot_site_hud_controller.h"
#include "gs1_godot_site_summary_panel_controller.h"
#include "gs1_godot_status_panel_controller.h"
#include "gs1_godot_task_panel_controller.h"

#include <godot_cpp/classes/base_button.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/classes/rich_text_label.hpp>
#include <godot_cpp/core/binder_common.hpp>
#include <godot_cpp/variant/callable.hpp>
#include <godot_cpp/variant/node_path.hpp>
#include <godot_cpp/variant/vector2i.hpp>

#include <cstdint>
#include <string>
#include <unordered_map>

class Gs1GodotSiteSessionUiController final
    : public godot::Control
    , public IGs1GodotEngineMessageSubscriber
    , public IGs1GodotInputSubscriber
{
    GDCLASS(Gs1GodotSiteSessionUiController, godot::Control)

public:
    Gs1GodotSiteSessionUiController() = default;
    ~Gs1GodotSiteSessionUiController() override = default;

    void _ready() override;
    void _process(double delta) override;

    void set_runtime_node_path(const godot::NodePath& path);
    [[nodiscard]] godot::NodePath get_runtime_node_path() const;
    void set_ui_root_path(const godot::NodePath& path);
    [[nodiscard]] godot::NodePath get_ui_root_path() const;

    void attach_input_dispatcher(Gs1GodotInputDispatcher& dispatcher);
    void detach_input_dispatcher();
    void disconnect_runtime_subscriptions();
    void apply_bootstrap_app_state(int app_state);

    [[nodiscard]] bool handles_engine_message(Gs1EngineMessageType type) const noexcept override;
    void handle_engine_message(const Gs1EngineMessage& message) override;
    void handle_runtime_message_reset() override;
    void handle_input_event(const godot::Ref<godot::InputEvent>& event) override;

protected:
    static void _bind_methods();

private:
    void cache_runtime_reference();
    void cache_ui_references();
    [[nodiscard]] godot::Control* resolve_ui_root();
    void wire_static_buttons();
    void bind_button(godot::BaseButton* button, const godot::Callable& callback);
    void refresh_visibility();
    void refresh_selected_tile_if_needed();
    void mark_selected_tile_dirty();
    void clamp_selected_tile();
    [[nodiscard]] const Gs1RuntimeSiteProjection* active_site() const;
    [[nodiscard]] const Gs1RuntimeTileProjection* tile_at(const godot::Vector2i& tile_coord) const;
    [[nodiscard]] int find_worker_pack_storage_id() const;
    [[nodiscard]] int find_selected_tile_storage_id();
    void plant_first_seed_on_selected_tile();
    [[nodiscard]] godot::String plant_name_for(int plant_id) const;
    [[nodiscard]] godot::String structure_name_for(int structure_id) const;
    void publish_last_action_message(const godot::String& message);
    void submit_ui_action(std::int64_t action_type, std::int64_t target_id = 0, std::int64_t arg0 = 0, std::int64_t arg1 = 0);
    void submit_move(double x, double y, double z);
    void submit_site_context_request(int tile_x, int tile_y, int flags);
    void submit_site_action_request(
        int action_kind,
        int flags,
        int quantity,
        int tile_x,
        int tile_y,
        int primary_subject_id,
        int secondary_subject_id,
        int item_id);
    void submit_site_action_cancel(int action_id, int flags);
    void submit_storage_view(int storage_id, int event_kind);
    void on_return_to_map_pressed();
    void on_submit_move_pressed(double x, double y, double z);
    void on_submit_selected_tile_context_pressed(int flags = 0);
    void on_submit_site_action_cancel_pressed(int action_id, int flags);
    void on_open_nearest_storage_pressed();
    void on_close_storage_pressed();
    void on_plant_selected_seed_pressed();

private:
    static constexpr int APP_STATE_BOOT = 0;
    static constexpr int APP_STATE_SITE_LOADING = 4;
    static constexpr int APP_STATE_SITE_ACTIVE = 5;
    static constexpr int APP_STATE_SITE_PAUSED = 6;
    static constexpr int APP_STATE_SITE_RESULT = 7;

    static constexpr std::int64_t UI_ACTION_RETURN_TO_REGIONAL_MAP = 4;
    static constexpr int SITE_ACTION_PLANT = 1;
    static constexpr int SITE_ACTION_CRAFT = 6;
    static constexpr int SITE_ACTION_REQUEST_FLAG_HAS_ITEM = 4;
    static constexpr int SITE_ACTION_CANCEL_FLAG_CURRENT_ACTION = 1;
    static constexpr int SITE_ACTION_CANCEL_FLAG_PLACEMENT_MODE = 2;
    static constexpr int INVENTORY_VIEW_EVENT_OPEN_SNAPSHOT = 1;
    static constexpr int INVENTORY_VIEW_EVENT_CLOSE = 2;
    static constexpr int CONTAINER_WORKER_PACK = 0;

    static constexpr int KEY_UP = 4194320;
    static constexpr int KEY_DOWN = 4194321;
    static constexpr int KEY_LEFT = 4194319;
    static constexpr int KEY_RIGHT = 4194322;

    godot::NodePath runtime_node_path_ {};
    godot::NodePath ui_root_path_ {".."};
    Gs1RuntimeNode* runtime_node_ {nullptr};
    godot::Control* ui_root_ {nullptr};
    Gs1GodotInputDispatcher* input_dispatcher_ {nullptr};
    int current_app_state_ {APP_STATE_BOOT};
    godot::Vector2i selected_tile_ {0, 0};
    int last_tile_label_x_ {-1};
    int last_tile_label_y_ {-1};
    bool selected_tile_dirty_ {true};
    bool buttons_wired_ {false};

    std::unordered_map<int, int> cached_storage_lookup_ {};
    mutable std::unordered_map<int, godot::String> plant_name_cache_ {};
    mutable std::unordered_map<int, godot::String> structure_name_cache_ {};

    Gs1GodotStatusPanelController status_panel_controller_ {};
    Gs1GodotInventoryPanelController inventory_panel_controller_ {};
    Gs1GodotCraftPanelController craft_panel_controller_ {};
    Gs1GodotActionPanelController action_panel_controller_ {};
    Gs1GodotOverlayPanelController overlay_panel_controller_ {};
    Gs1GodotPhonePanelController phone_panel_controller_ {};
    Gs1GodotRegionalTechTreePanelController regional_tech_tree_panel_controller_ {};
    Gs1GodotSiteHudController site_hud_controller_ {};
    Gs1GodotSiteSummaryPanelController site_summary_panel_controller_ {};
    Gs1GodotTaskPanelController task_panel_controller_ {};
    Gs1GodotSiteStateReducer site_state_reducer_ {};

    godot::Control* site_panel_ {nullptr};
    godot::Label* tile_label_ {nullptr};
};
