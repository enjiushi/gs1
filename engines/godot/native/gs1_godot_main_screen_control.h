#pragma once

#include "host/runtime_projection_state.h"

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

class Gs1GodotMainScreenControl final : public godot::Control
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

protected:
    static void _bind_methods();

private:
    struct ProjectedButtonRecord final
    {
        godot::ObjectID object_id {};
        godot::ObjectID content_root_id {};
        godot::ObjectID marker_label_id {};
        godot::ObjectID icon_texture_id {};
        godot::ObjectID icon_label_id {};
        godot::ObjectID title_label_id {};
        godot::ObjectID subtitle_label_id {};
        godot::ObjectID status_label_id {};
        godot::ObjectID lock_overlay_id {};
        godot::ObjectID lock_label_id {};
    };

    struct FixedSlotBinding final
    {
        godot::ObjectID object_id {};
        int panel_id {0};
        int slot_id {0};
    };

    struct RegionalSiteNodeRecord final
    {
        godot::ObjectID root_id {};
        godot::ObjectID base_id {};
        godot::ObjectID tower_id {};
        godot::ObjectID beacon_id {};
        godot::ObjectID label_id {};
    };

    struct MenuBackdropPlantRecord final
    {
        godot::ObjectID root_id {};
        godot::Vector3 base_position {};
        godot::Vector3 base_rotation_degrees {};
        float sway_scale {1.0F};
    };

    struct TaskTemplateUiCacheEntry final
    {
        int task_tier_id {0};
        int progress_kind {0};
        godot::Ref<godot::Texture2D> icon_texture {};
    };

    struct ModifierUiCacheEntry final
    {
        godot::String label {};
        godot::Ref<godot::Texture2D> icon_texture {};
    };

    struct TechnologyUiCacheEntry final
    {
        godot::String title {};
        godot::String faction_name {};
        godot::String tooltip {};
        godot::Ref<godot::Texture2D> icon_texture {};
    };

    struct UnlockableUiCacheEntry final
    {
        godot::String title {};
        godot::String subtitle {};
        godot::String tooltip {};
        int content_kind {-1};
        godot::Ref<godot::Texture2D> icon_texture {};
    };

    void cache_scene_references();
    void cache_ui_references();
    void wire_static_buttons();
    [[nodiscard]] bool sync_projection_from_runtime();

    void refresh_status();
    void refresh_menu(int app_state);
    void apply_fixed_panel_actions();
    void refresh_regional_map(int app_state, bool projection_changed);
    void refresh_site(int app_state, bool projection_changed);

    void render_inventory(const Gs1RuntimeSiteProjection& site_state);
    void render_tasks(const Gs1RuntimeSiteProjection& site_state);
    void render_phone(const Gs1RuntimeSiteProjection& site_state);
    void render_craft(const Gs1RuntimeSiteProjection& site_state);
    void render_overlay(const Gs1RuntimeSiteProjection& site_state);
    void reconcile_task_rows(const std::vector<Gs1RuntimeTaskProjection>& tasks);
    void reconcile_modifier_rows(const std::vector<Gs1RuntimeModifierProjection>& modifiers);
    void render_projected_ui_buttons(godot::VBoxContainer* container, const std::initializer_list<int>& allowed_action_types);
    void reconcile_phone_listing_buttons(const std::vector<Gs1RuntimePhoneListingProjection>& phone_listings);
    void reconcile_craft_option_buttons(const Gs1RuntimeCraftContextProjection* craft_context);
    void reconcile_projected_action_buttons(
        godot::Node* container,
        std::unordered_map<std::uint64_t, ProjectedButtonRecord>& registry,
        int container_kind,
        const godot::Array& button_specs);
    void reconcile_tech_tree_cards(
        godot::GridContainer* container,
        std::unordered_map<std::uint64_t, ProjectedButtonRecord>& registry,
        const godot::Array& card_specs,
        bool allow_actions);
    [[nodiscard]] godot::String tech_tree_marker_text(const godot::Dictionary& spec) const;
    void render_regional_selection(const Gs1RuntimeRegionalMapSiteProjection* selected_site);
    void render_regional_tech_tree();

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
    [[nodiscard]] godot::String build_regional_map_overview_text(
        const std::vector<Gs1RuntimeRegionalMapSiteProjection>& sites,
        const std::vector<Gs1RuntimeRegionalMapLinkProjection>& links) const;
    [[nodiscard]] const Gs1RuntimeProgressionViewProjection* find_progression_view(int view_id) const;
    [[nodiscard]] const Gs1RuntimeUiPanelProjection* find_ui_panel(int panel_id) const;
    [[nodiscard]] const Gs1RuntimeUiPanelSlotActionProjection* find_panel_slot_action(const Gs1RuntimeUiPanelProjection& panel, int slot_id) const;
    [[nodiscard]] godot::String regional_panel_text_line(const Gs1RuntimeUiPanelTextProjection& line) const;
    [[nodiscard]] godot::String regional_panel_slot_label(const Gs1RuntimeUiPanelSlotActionProjection& slot_action) const;
    [[nodiscard]] godot::String regional_panel_list_primary_text(const Gs1RuntimeUiPanelListItemProjection& item) const;
    [[nodiscard]] godot::String regional_panel_list_secondary_text(const Gs1RuntimeUiPanelListItemProjection& item) const;
    void cache_fixed_slot_bindings();
    void clear_fixed_slot_actions();
    void bind_fixed_slot_actions(const Gs1RuntimeUiPanelProjection* panel, int panel_id);
    void apply_action_to_button(godot::BaseButton* button, const godot::Dictionary& action, const godot::String& fallback_label);
    void clear_action_from_button(godot::BaseButton* button, bool preserve_text = true);
    [[nodiscard]] godot::String regional_site_deployment_summary(const Gs1RuntimeRegionalMapSiteProjection& site) const;
    [[nodiscard]] godot::String regional_support_preview_text(int preview_mask) const;
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
    [[nodiscard]] godot::Button* upsert_button_node(
        godot::Node* container,
        std::unordered_map<std::uint64_t, ProjectedButtonRecord>& registry,
        std::uint64_t stable_key,
        const godot::String& node_name,
        int desired_index);
    void prune_button_registry(
        std::unordered_map<std::uint64_t, ProjectedButtonRecord>& registry,
        const std::unordered_set<std::uint64_t>& desired_keys);
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
    [[nodiscard]] godot::String regional_site_button_text(const Gs1RuntimeRegionalMapSiteProjection& site) const;
    [[nodiscard]] godot::String regional_site_tooltip(const Gs1RuntimeRegionalMapSiteProjection& site) const;
    [[nodiscard]] godot::String regional_site_state_name(int site_state) const;
    [[nodiscard]] godot::String regional_selection_action_label(const godot::String& text, const godot::Dictionary& action) const;
    [[nodiscard]] godot::String regional_unlockable_tooltip_text(const godot::Dictionary& spec) const;
    [[nodiscard]] godot::String regional_tech_tooltip_text(const godot::Dictionary& spec) const;
    [[nodiscard]] godot::String regional_row_requirement_text(const godot::Dictionary& spec) const;
    [[nodiscard]] godot::String regional_card_icon_text(const godot::Dictionary& spec) const;
    [[nodiscard]] godot::String regional_card_title_text(const godot::Dictionary& spec) const;
    [[nodiscard]] godot::String regional_card_subtitle_text(const godot::Dictionary& spec) const;
    [[nodiscard]] godot::String regional_card_status_text(const godot::Dictionary& spec) const;
    [[nodiscard]] godot::Color regional_card_status_color(const godot::Dictionary& spec) const;
    [[nodiscard]] godot::Color regional_card_icon_background_color(const godot::String& icon_text) const;
    [[nodiscard]] godot::Ref<godot::Texture2D> regional_card_icon_texture(const godot::Dictionary& spec) const;
    void ensure_card_content_nodes(godot::Button* button, ProjectedButtonRecord& record);
    void apply_tech_tree_overlay_layout();

    [[nodiscard]] const Gs1RuntimeProjectionState* projection_state() const;
    [[nodiscard]] const Gs1RuntimeSiteProjection* active_site() const;
    [[nodiscard]] const Gs1RuntimeTileProjection* tile_at(const godot::Vector2i& tile_coord) const;
    [[nodiscard]] int find_worker_pack_storage_id() const;
    [[nodiscard]] int find_selected_tile_storage_id();
    [[nodiscard]] godot::String item_name_for(int item_id) const;
    void use_first_usable_item();
    void transfer_first_storage_item_to_pack();
    void plant_first_seed_on_selected_tile();
    [[nodiscard]] godot::String faction_name_for(int faction_id) const;
    [[nodiscard]] godot::String plant_name_for(int plant_id) const;
    [[nodiscard]] godot::String structure_name_for(int structure_id) const;
    [[nodiscard]] godot::String recipe_output_name_for(int recipe_id, int output_item_id) const;
    [[nodiscard]] godot::Ref<godot::Texture2D> load_cached_texture(const godot::String& path) const;
    [[nodiscard]] godot::Ref<godot::Texture2D> fallback_icon_texture(const godot::String& icon_text) const;
    [[nodiscard]] const TaskTemplateUiCacheEntry& task_template_ui_for(std::uint32_t task_template_id) const;
    [[nodiscard]] const ModifierUiCacheEntry& modifier_ui_for(std::uint32_t modifier_id) const;
    [[nodiscard]] const TechnologyUiCacheEntry& technology_ui_for(std::uint32_t tech_node_id) const;
    [[nodiscard]] const UnlockableUiCacheEntry& unlockable_ui_for(std::uint32_t unlock_id) const;

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

    void bind_button(godot::BaseButton* button, const godot::Callable& callback);
    void clamp_selected_tile();
    [[nodiscard]] bool regional_map_ui_contains_screen_point(const godot::Vector2& screen_position) const;
    [[nodiscard]] bool try_select_regional_site_from_screen(const godot::Vector2& screen_position);

    [[nodiscard]] int as_int(const godot::Variant& value, int fallback = 0) const;
    [[nodiscard]] double as_float(const godot::Variant& value, double fallback = 0.0) const;
    [[nodiscard]] bool as_bool(const godot::Variant& value, bool fallback = false) const;
    [[nodiscard]] godot::String app_state_name(int app_state) const;

    void on_start_campaign_pressed();
    void on_continue_campaign_pressed();
    void on_menu_settings_pressed();
    void on_quit_pressed();
    void on_return_to_map_pressed();
    void on_move_north_pressed();
    void on_move_south_pressed();
    void on_move_west_pressed();
    void on_move_east_pressed();
    void on_hover_tile_pressed();
    void on_cancel_action_pressed();
    void on_open_protection_pressed();
    void on_overlay_wind_pressed();
    void on_overlay_heat_pressed();
    void on_overlay_dust_pressed();
    void on_overlay_condition_pressed();
    void on_overlay_clear_pressed();
    void on_open_phone_home_pressed();
    void on_open_phone_tasks_pressed();
    void on_open_phone_buy_pressed();
    void on_open_phone_sell_pressed();
    void on_open_tech_tree_pressed();
    void on_close_phone_pressed();
    void on_open_worker_pack_pressed();
    void on_open_nearest_storage_pressed();
    void on_close_storage_pressed();
    void on_use_selected_item_pressed();
    void on_transfer_selected_item_pressed();
    void on_plant_selected_seed_pressed();
    void on_craft_at_selected_tile_pressed();
    void on_dynamic_regional_site_pressed(int site_id);
    void on_dynamic_phone_listing_pressed(int listing_id);
    void on_dynamic_craft_option_pressed(std::int64_t button_key);
    void on_dynamic_projected_action_pressed(int container_kind, std::int64_t button_key);
    void on_fixed_slot_pressed(std::int64_t button_id);
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
    static constexpr int PROJECTED_ACTION_CONTAINER_SITE_CONTROLS = 0;
    static constexpr int PROJECTED_ACTION_CONTAINER_REGIONAL_MENU = 1;
    static constexpr int PROJECTED_ACTION_CONTAINER_REGIONAL_SELECTION = 2;
    static constexpr int PROJECTED_ACTION_CONTAINER_REGIONAL_TECH_TREE = 3;
    static constexpr int PROJECTED_ACTION_CONTAINER_MAIN_MENU = 4;

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
    int selected_site_id_ {1};
    godot::String last_action_message_;
    std::uint64_t last_projection_revision_ {0};
    godot::Rect2i regional_map_bounds_ {-4, -3, 9, 7};
    int last_rendered_selected_site_id_ {-1};
    int last_app_state_ {-1};
    double menu_backdrop_time_ {0.0};
    bool regional_map_visible_ {false};
    bool site_panel_visible_ {false};
    bool buttons_wired_ {false};
    bool fixed_slot_bindings_cached_ {false};
    bool regional_menu_backdrop_active_ {false};

    std::unordered_map<int, int> cached_storage_lookup_;
    std::unordered_map<int, RegionalSiteNodeRecord> regional_site_nodes_;
    std::unordered_map<int, Gs1RuntimeRegionalMapSiteProjection> regional_site_data_;
    std::unordered_map<std::uint64_t, godot::ObjectID> regional_terrain_tile_nodes_;
    std::unordered_map<std::uint64_t, godot::ObjectID> regional_terrain_grid_line_nodes_;
    std::unordered_map<std::uint64_t, godot::ObjectID> regional_link_nodes_;
    std::unordered_map<std::string, godot::Ref<godot::StandardMaterial3D>> regional_material_cache_;
    std::vector<MenuBackdropPlantRecord> menu_backdrop_plants_;
    std::vector<FixedSlotBinding> fixed_slot_bindings_;
    std::unordered_map<std::uint64_t, ProjectedButtonRecord> phone_listing_buttons_;
    std::unordered_map<std::uint64_t, ProjectedButtonRecord> craft_option_buttons_;
    std::unordered_map<std::uint64_t, ProjectedButtonRecord> task_buttons_;
    std::unordered_map<std::uint64_t, ProjectedButtonRecord> modifier_buttons_;
    std::unordered_map<std::uint64_t, ProjectedButtonRecord> site_control_buttons_;
    std::unordered_map<std::uint64_t, ProjectedButtonRecord> regional_selection_action_buttons_;
    std::unordered_map<std::uint64_t, ProjectedButtonRecord> regional_tech_tree_action_buttons_;
    mutable std::unordered_map<int, godot::String> item_name_cache_;
    mutable std::unordered_map<int, godot::String> plant_name_cache_;
    mutable std::unordered_map<int, godot::String> structure_name_cache_;
    mutable std::unordered_map<int, godot::String> faction_name_cache_;
    mutable std::unordered_map<std::uint64_t, godot::String> recipe_output_name_cache_;
    mutable std::unordered_map<std::uint32_t, TaskTemplateUiCacheEntry> task_template_ui_cache_;
    mutable std::unordered_map<std::uint32_t, ModifierUiCacheEntry> modifier_ui_cache_;
    mutable std::unordered_map<std::uint32_t, TechnologyUiCacheEntry> technology_ui_cache_;
    mutable std::unordered_map<std::uint32_t, UnlockableUiCacheEntry> unlockable_ui_cache_;
    mutable std::unordered_map<std::string, godot::Ref<godot::Texture2D>> texture_cache_;
    mutable std::unordered_map<std::string, godot::Ref<godot::Texture2D>> fallback_icon_texture_cache_;

    godot::RichTextLabel* status_label_ {nullptr};
    godot::PanelContainer* menu_panel_ {nullptr};
    godot::PanelContainer* regional_map_panel_ {nullptr};
    godot::RichTextLabel* regional_map_summary_ {nullptr};
    godot::RichTextLabel* regional_map_graph_ {nullptr};
    godot::VBoxContainer* regional_action_buttons_ {nullptr};
    godot::PanelContainer* regional_selection_panel_ {nullptr};
    godot::Label* regional_selection_title_ {nullptr};
    godot::RichTextLabel* regional_selection_summary_ {nullptr};
    godot::VBoxContainer* regional_selection_actions_ {nullptr};
    godot::Control* regional_tech_tree_overlay_ {nullptr};
    godot::PanelContainer* regional_tech_tree_panel_ {nullptr};
    godot::Label* regional_tech_tree_title_ {nullptr};
    godot::ScrollContainer* regional_tech_tree_summary_ {nullptr};
    godot::GridContainer* regional_tech_tree_actions_ {nullptr};
    godot::PanelContainer* site_panel_ {nullptr};
    godot::Label* site_title_ {nullptr};
    godot::RichTextLabel* site_summary_ {nullptr};
    godot::Label* tile_label_ {nullptr};
    godot::VBoxContainer* site_controls_ {nullptr};
    godot::PanelContainer* inventory_panel_ {nullptr};
    godot::RichTextLabel* inventory_summary_ {nullptr};
    godot::PanelContainer* task_panel_ {nullptr};
    godot::RichTextLabel* task_summary_ {nullptr};
    godot::VBoxContainer* task_rows_ {nullptr};
    godot::VBoxContainer* modifier_rows_ {nullptr};
    godot::PanelContainer* phone_panel_ {nullptr};
    godot::VBoxContainer* phone_listings_ {nullptr};
    godot::PanelContainer* craft_panel_ {nullptr};
    godot::RichTextLabel* craft_summary_ {nullptr};
    godot::VBoxContainer* craft_options_ {nullptr};
    godot::PanelContainer* overlay_panel_ {nullptr};
    godot::Label* phone_state_label_ {nullptr};
    godot::Label* overlay_state_label_ {nullptr};
};
