#pragma once

#include "gs1_godot_action_panel_controller.h"
#include "gs1_godot_panel_state_reducers.h"
#include "gs1_godot_craft_panel_controller.h"
#include "gs1_godot_phone_panel_controller.h"
#include "gs1_godot_inventory_panel_controller.h"
#include "gs1_godot_overlay_panel_controller.h"
#include "gs1_godot_regional_map_hud_controller.h"
#include "gs1_godot_regional_selection_panel_controller.h"
#include "gs1_godot_regional_summary_panel_controller.h"
#include "gs1_godot_regional_tech_tree_panel_controller.h"
#include "gs1_godot_runtime_node.h"
#include "gs1_godot_site_hud_controller.h"
#include "gs1_godot_site_summary_panel_controller.h"
#include "gs1_godot_status_panel_controller.h"
#include "gs1_godot_task_panel_controller.h"

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/grid_container.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/classes/scroll_container.hpp>
#include <godot_cpp/core/object_id.hpp>
#include <godot_cpp/core/binder_common.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/node_path.hpp>
#include <godot_cpp/variant/rect2i.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace godot
{
class BaseButton;
class CenterContainer;
class ColorRect;
class Color;
class Button;
class Camera3D;
class ImageTexture;
class Label;
class Label3D;
class MarginContainer;
class MeshInstance3D;
class Node;
class Node3D;
class PanelContainer;
class Panel;
class RichTextLabel;
class ScrollContainer;
class StandardMaterial3D;
class Texture2D;
class TextureRect;
class VBoxContainer;
class GridContainer;
class ScrollContainer;
}

class Gs1RuntimeNode;

class Gs1GodotMainScreenControl final : public godot::Control, public IGs1GodotEngineMessageSubscriber
{
    GDCLASS(Gs1GodotMainScreenControl, godot::Control)

public:
    Gs1GodotMainScreenControl() = default;
    ~Gs1GodotMainScreenControl() override = default;

    void _ready() override;
    void _process(double delta) override;
    void _input(const godot::Ref<godot::InputEvent>& event) override;

    void set_runtime_node_path(const godot::NodePath& path);
    [[nodiscard]] godot::NodePath get_runtime_node_path() const;

    void set_site_view_path(const godot::NodePath& path);
    [[nodiscard]] godot::NodePath get_site_view_path() const;

    void set_regional_world_path(const godot::NodePath& path);
    [[nodiscard]] godot::NodePath get_regional_world_path() const;

    void set_regional_camera_path(const godot::NodePath& path);
    [[nodiscard]] godot::NodePath get_regional_camera_path() const;

    void set_regional_terrain_root_path(const godot::NodePath& path);
    [[nodiscard]] godot::NodePath get_regional_terrain_root_path() const;

    void set_regional_link_root_path(const godot::NodePath& path);
    [[nodiscard]] godot::NodePath get_regional_link_root_path() const;

    void set_regional_site_root_path(const godot::NodePath& path);
    [[nodiscard]] godot::NodePath get_regional_site_root_path() const;

    [[nodiscard]] bool handles_engine_message(Gs1EngineMessageType type) const noexcept override;
    void handle_engine_message(const Gs1EngineMessage& message) override;
    void handle_runtime_message_reset() override;
    void disconnect_runtime_subscriptions();
    void apply_bootstrap_app_state(int app_state);

protected:
    static void _bind_methods();

private:
    struct RegionalSiteNodeRecord final
    {
        godot::ObjectID root_id {};
        godot::ObjectID base_id {};
        godot::ObjectID tower_id {};
        godot::ObjectID beacon_id {};
        godot::ObjectID selection_ring_id {};
        godot::ObjectID label_id {};
        godot::ObjectID selected_hint_id {};
    };

    struct MenuBackdropPlantRecord final
    {
        godot::ObjectID root_id {};
        godot::Vector3 base_position {};
        godot::Vector3 base_rotation_degrees {};
        float sway_scale {1.0F};
    };

    void cache_scene_references();
    void cache_ui_references();
    void wire_static_buttons();
    void invalidate_all_ui();
    void update_visibility(int app_state);
    void refresh_regional_map_if_needed(int app_state);
    void refresh_selected_tile_if_needed();
    void mark_regional_map_dirty();
    void mark_regional_visuals_dirty();
    void mark_selected_tile_dirty();

    void refresh_regional_map(int app_state);

    void toggle_regional_tech_tree();
    void select_regional_site(int site_id, bool submit_runtime);
    void rebuild_regional_map_world(
        const std::vector<Gs1RuntimeRegionalMapSiteProjection>& sites,
        const std::vector<Gs1RuntimeRegionalMapLinkProjection>& links);
    void clear_regional_projection_world();
    void build_menu_backdrop();
    void reconcile_desert_tiles(const godot::Rect2i& bounds, bool include_grid_lines);
    void reconcile_regional_links(const std::vector<Gs1RuntimeRegionalMapLinkProjection>& links);
    void reconcile_regional_sites(const std::vector<Gs1RuntimeRegionalMapSiteProjection>& sites);
    void position_regional_camera(const godot::Rect2i& bounds);
    void update_regional_site_visuals();
    [[nodiscard]] godot::Vector2i regional_grid_coord(const Gs1RuntimeRegionalMapSiteProjection& site) const;
    [[nodiscard]] godot::Vector3 regional_world_position(const godot::Vector2i& grid) const;
    [[nodiscard]] godot::Color regional_site_state_color(int site_state) const;
    void update_background_presentation(int app_state, double delta);
    [[nodiscard]] godot::Ref<godot::StandardMaterial3D> get_material(
        const godot::String& key,
        const godot::Color& color,
        double roughness,
        double metallic,
        bool emission = false);

    void clear_dynamic_children(godot::Node* container, const godot::String& prefix = godot::String("Dynamic"));
    [[nodiscard]] godot::MeshInstance3D* upsert_regional_mesh_node(
        godot::Node3D* container,
        std::unordered_map<std::uint64_t, godot::ObjectID>& registry,
        std::uint64_t stable_key,
        const godot::String& node_name,
        int desired_index);
    void prune_regional_node_registry(
        std::unordered_map<std::uint64_t, godot::ObjectID>& registry,
        const std::unordered_set<std::uint64_t>& desired_keys);
    void prune_regional_site_registry(const std::unordered_set<int>& desired_site_ids);
    [[nodiscard]] const Gs1RuntimeSiteProjection* active_site() const;
    [[nodiscard]] const Gs1RuntimeTileProjection* tile_at(const godot::Vector2i& tile_coord) const;
    [[nodiscard]] int find_worker_pack_storage_id() const;
    [[nodiscard]] int find_selected_tile_storage_id();
    void use_first_usable_item();
    void transfer_first_storage_item_to_pack();
    void plant_first_seed_on_selected_tile();
    [[nodiscard]] godot::String plant_name_for(int plant_id) const;
    [[nodiscard]] godot::String structure_name_for(int structure_id) const;

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
    void publish_last_action_message();

    void bind_button(godot::BaseButton* button, const godot::Callable& callback);
    void clamp_selected_tile();
    [[nodiscard]] bool regional_map_ui_contains_screen_point(const godot::Vector2& screen_position) const;
    [[nodiscard]] bool try_select_regional_site_from_screen(const godot::Vector2& screen_position);

    [[nodiscard]] int as_int(const godot::Variant& value, int fallback = 0) const;
    [[nodiscard]] double as_float(const godot::Variant& value, double fallback = 0.0) const;
    [[nodiscard]] bool as_bool(const godot::Variant& value, bool fallback = false) const;
    [[nodiscard]] godot::String app_state_name(int app_state) const;

    void on_submit_ui_action_pressed(std::int64_t action_type, std::int64_t target_id = 0, std::int64_t arg0 = 0, std::int64_t arg1 = 0);
    void on_submit_move_pressed(double x, double y, double z);
    void on_submit_selected_tile_context_pressed(int flags = 0);
    void on_submit_site_action_cancel_pressed(int action_id, int flags);
    void on_toggle_regional_tech_tree_pressed();
    void on_menu_settings_pressed();
    void on_quit_pressed();
    void on_open_worker_pack_pressed();
    void on_open_nearest_storage_pressed();
    void on_close_storage_pressed();
    void on_use_selected_item_pressed();
    void on_transfer_selected_item_pressed();
    void on_plant_selected_seed_pressed();
    void on_dynamic_regional_site_pressed(int site_id);

private:
    static constexpr int APP_STATE_BOOT = 0;
    static constexpr int APP_STATE_MAIN_MENU = 1;
    static constexpr int APP_STATE_CAMPAIGN_SETUP = 2;
    static constexpr int APP_STATE_REGIONAL_MAP = 3;
    static constexpr int APP_STATE_SITE_LOADING = 4;
    static constexpr int APP_STATE_SITE_ACTIVE = 5;
    static constexpr int APP_STATE_SITE_PAUSED = 6;
    static constexpr int APP_STATE_SITE_RESULT = 7;
    static constexpr int APP_STATE_CAMPAIGN_END = 8;

    static constexpr int UI_ACTION_START_NEW_CAMPAIGN = 1;
    static constexpr int UI_ACTION_SELECT_DEPLOYMENT_SITE = 2;
    static constexpr int UI_ACTION_START_SITE_ATTEMPT = 3;
    static constexpr int UI_ACTION_RETURN_TO_REGIONAL_MAP = 4;
    static constexpr int UI_ACTION_CLEAR_DEPLOYMENT_SITE_SELECTION = 5;
    static constexpr int UI_ACTION_ACCEPT_TASK = 6;
    static constexpr int UI_ACTION_CLAIM_TASK_REWARD = 7;
    static constexpr int UI_ACTION_BUY_PHONE_LISTING = 8;
    static constexpr int UI_ACTION_USE_INVENTORY_ITEM = 10;
    static constexpr int UI_ACTION_TRANSFER_INVENTORY_ITEM = 11;
    static constexpr int UI_ACTION_SET_PHONE_PANEL_SECTION = 17;
    static constexpr int UI_ACTION_OPEN_REGIONAL_MAP_TECH_TREE = 18;
    static constexpr int UI_ACTION_CLOSE_REGIONAL_MAP_TECH_TREE = 19;
    static constexpr int UI_ACTION_CLAIM_TECHNOLOGY_NODE = 20;
    static constexpr int UI_ACTION_SELECT_TECH_TREE_FACTION_TAB = 21;
    static constexpr int UI_ACTION_CLOSE_PHONE_PANEL = 22;
    static constexpr int UI_ACTION_REFUND_TECHNOLOGY_NODE = 23;
    static constexpr int UI_ACTION_OPEN_SITE_PROTECTION_SELECTOR = 24;
    static constexpr int UI_ACTION_CLOSE_SITE_PROTECTION_UI = 25;
    static constexpr int UI_ACTION_SET_SITE_PROTECTION_OVERLAY_MODE = 26;

    static constexpr int SITE_ACTION_PLANT = 1;
    static constexpr int SITE_ACTION_CRAFT = 6;

    static constexpr int SITE_ACTION_REQUEST_FLAG_HAS_ITEM = 4;
    static constexpr int SITE_ACTION_CANCEL_FLAG_CURRENT_ACTION = 1;
    static constexpr int SITE_ACTION_CANCEL_FLAG_PLACEMENT_MODE = 2;

    static constexpr int INVENTORY_VIEW_EVENT_OPEN_SNAPSHOT = 1;
    static constexpr int INVENTORY_VIEW_EVENT_CLOSE = 2;

    static constexpr int PHONE_PANEL_SECTION_HOME = 0;
    static constexpr int PHONE_PANEL_SECTION_TASKS = 1;
    static constexpr int PHONE_PANEL_SECTION_BUY = 2;
    static constexpr int PHONE_PANEL_SECTION_SELL = 3;

    static constexpr int PROTECTION_OVERLAY_NONE = 0;
    static constexpr int PROTECTION_OVERLAY_WIND = 1;
    static constexpr int PROTECTION_OVERLAY_HEAT = 2;
    static constexpr int PROTECTION_OVERLAY_DUST = 3;
    static constexpr int PROTECTION_OVERLAY_OCCUPANT_CONDITION = 4;

    static constexpr int CONTAINER_WORKER_PACK = 0;
    static constexpr int CONTAINER_DEVICE_STORAGE = 1;
    static constexpr double REGIONAL_TILE_SIZE = 4.5;
    static constexpr int REGIONAL_WORLD_PADDING = 2;
    static constexpr double REGIONAL_PICK_DISTANCE = 400.0;

    godot::NodePath runtime_node_path_ {"Runtime"};
    godot::NodePath site_view_path_ {"SiteView"};
    godot::NodePath regional_world_path_ {"RegionalMapWorld"};
    godot::NodePath regional_camera_path_ {"RegionalMapWorld/RegionalMapCamera"};
    godot::NodePath regional_terrain_root_path_ {"RegionalMapWorld/TerrainRoot"};
    godot::NodePath regional_link_root_path_ {"RegionalMapWorld/LinkRoot"};
    godot::NodePath regional_site_root_path_ {"RegionalMapWorld/SiteRoot"};

    Gs1RuntimeNode* runtime_node_ {nullptr};
    godot::Node3D* site_view_ {nullptr};
    godot::Node3D* regional_map_world_ {nullptr};
    godot::Camera3D* regional_camera_ {nullptr};
    godot::Node3D* regional_terrain_root_ {nullptr};
    godot::Node3D* regional_link_root_ {nullptr};
    godot::Node3D* regional_site_root_ {nullptr};

    godot::Vector2i selected_tile_ {0, 0};
    godot::String last_action_message_;
    Gs1GodotStatusPanelController status_panel_controller_ {};
    Gs1GodotInventoryPanelController inventory_panel_controller_ {};
    Gs1GodotCraftPanelController craft_panel_controller_ {};
    Gs1GodotActionPanelController action_panel_controller_ {};
    Gs1GodotOverlayPanelController overlay_panel_controller_ {};
    Gs1GodotPhonePanelController phone_panel_controller_ {};
    Gs1GodotRegionalMapHudController regional_map_hud_controller_ {};
    Gs1GodotRegionalSelectionPanelController regional_selection_panel_controller_ {};
    Gs1GodotRegionalSummaryPanelController regional_summary_panel_controller_ {};
    Gs1GodotRegionalTechTreePanelController regional_tech_tree_panel_controller_ {};
    Gs1GodotSiteHudController site_hud_controller_ {};
    Gs1GodotSiteSummaryPanelController site_summary_panel_controller_ {};
    Gs1GodotTaskPanelController task_panel_controller_ {};
    Gs1GodotRegionalMapStateReducer regional_map_state_reducer_ {};
    Gs1GodotSiteStateReducer site_state_reducer_ {};
    godot::Rect2i regional_map_bounds_ {-4, -3, 9, 7};
    int last_app_state_ {-1};
    int last_visible_app_state_ {-1};
    int last_tile_label_x_ {-1};
    int last_tile_label_y_ {-1};
    double menu_backdrop_time_ {0.0};
    bool regional_map_visible_ {false};
    bool site_panel_visible_ {false};
    bool buttons_wired_ {false};
    bool regional_menu_backdrop_active_ {false};
    bool regional_map_dirty_ {true};
    bool regional_visuals_dirty_ {true};
    bool selected_tile_dirty_ {true};

    std::unordered_map<int, int> cached_storage_lookup_;
    std::unordered_map<int, RegionalSiteNodeRecord> regional_site_nodes_;
    std::unordered_map<int, Gs1RuntimeRegionalMapSiteProjection> regional_site_data_;
    std::unordered_map<std::uint64_t, godot::ObjectID> regional_terrain_tile_nodes_;
    std::unordered_map<std::uint64_t, godot::ObjectID> regional_terrain_grid_line_nodes_;
    std::unordered_map<std::uint64_t, godot::ObjectID> regional_link_nodes_;
    std::unordered_map<std::string, godot::Ref<godot::StandardMaterial3D>> regional_material_cache_;
    std::vector<MenuBackdropPlantRecord> menu_backdrop_plants_;
    mutable std::unordered_map<int, godot::String> plant_name_cache_;
    mutable std::unordered_map<int, godot::String> structure_name_cache_;

    godot::PanelContainer* menu_panel_ {nullptr};
    godot::PanelContainer* regional_map_panel_ {nullptr};
    godot::PanelContainer* regional_selection_panel_ {nullptr};
    godot::Control* regional_tech_tree_overlay_ {nullptr};
    godot::PanelContainer* site_panel_ {nullptr};
    godot::Label* tile_label_ {nullptr};
    godot::PanelContainer* inventory_panel_ {nullptr};
    godot::PanelContainer* task_panel_ {nullptr};
    godot::PanelContainer* phone_panel_ {nullptr};
    godot::PanelContainer* craft_panel_ {nullptr};
    godot::PanelContainer* overlay_panel_ {nullptr};
};
