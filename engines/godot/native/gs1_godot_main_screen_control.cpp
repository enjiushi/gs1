#include "godot_progression_resources.h"
#include "gs1_godot_main_screen_control.h"
#include "gs1_godot_runtime_node.h"

#include "content/defs/faction_defs.h"
#include "content/defs/technology_defs.h"

#include <godot_cpp/classes/base_button.hpp>
#include <godot_cpp/classes/base_material3d.hpp>
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/color_rect.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/grid_container.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/label3d.hpp>
#include <godot_cpp/classes/margin_container.hpp>
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/panel.hpp>
#include <godot_cpp/classes/packed_scene.hpp>
#include <godot_cpp/classes/panel_container.hpp>
#include <godot_cpp/classes/rich_text_label.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/scroll_container.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/texture_rect.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/core/object.hpp>
#include <godot_cpp/core/property_info.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/callable.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/variant.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cmath>
#include <limits>
#include <type_traits>
#include <vector>

using namespace godot;

namespace
{
constexpr std::uint8_t k_unlockable_content_kind_plant = 0U;
constexpr std::uint8_t k_unlockable_content_kind_item = 1U;
constexpr std::uint8_t k_unlockable_content_kind_structure_recipe = 2U;
constexpr std::uint8_t k_unlockable_content_kind_recipe = 3U;

constexpr std::int64_t k_content_resource_kind_plant = 0;
constexpr std::int64_t k_content_resource_kind_item = 1;
constexpr std::int64_t k_content_resource_kind_structure = 2;
constexpr std::int64_t k_content_resource_kind_recipe = 3;

constexpr int k_mouse_button_left = 1;
constexpr int k_key_up = 4194320;
constexpr int k_key_down = 4194321;
constexpr int k_key_left = 4194319;
constexpr int k_key_right = 4194322;

String bool_text(bool value, const String& when_true, const String& when_false)
{
    return value ? when_true : when_false;
}

Button* make_button()
{
    return memnew(Button);
}

MeshInstance3D* make_mesh_instance3d()
{
    return memnew(MeshInstance3D);
}

Node3D* make_node3d()
{
    return memnew(Node3D);
}

Label3D* make_label3d()
{
    return memnew(Label3D);
}

String string_from_view(std::string_view value)
{
    return String::utf8(value.data(), static_cast<int>(value.size()));
}

const gs1::TechnologyNodeDef* find_tech_tree_node_def(const Dictionary& action)
{
    const Variant raw_target_id = action.get("target_id", Variant());
    if (raw_target_id.get_type() != Variant::INT)
    {
        return nullptr;
    }

    const auto target_id = static_cast<std::uint32_t>(static_cast<std::int64_t>(raw_target_id));
    if (target_id == 0U)
    {
        return nullptr;
    }

    return gs1::find_technology_node_def(gs1::TechNodeId {target_id});
}

constexpr std::uint64_t k_fnv_offset_basis = 1469598103934665603ULL;
constexpr std::uint64_t k_fnv_prime = 1099511628211ULL;

void hash_bytes(std::uint64_t& hash, const void* data, std::size_t size)
{
    const auto* bytes = static_cast<const unsigned char*>(data);
    for (std::size_t index = 0; index < size; ++index)
    {
        hash ^= static_cast<std::uint64_t>(bytes[index]);
        hash *= k_fnv_prime;
    }
}

template <typename T>
void hash_value(std::uint64_t& hash, const T& value)
{
    static_assert(std::is_trivially_copyable_v<T>);
    hash_bytes(hash, &value, sizeof(T));
}

constexpr std::uint64_t pack_u32_pair(std::uint32_t high, std::uint32_t low) noexcept
{
    return (static_cast<std::uint64_t>(high) << 32U) | static_cast<std::uint64_t>(low);
}

std::uint64_t pack_i32_pair(int high, int low) noexcept
{
    return pack_u32_pair(static_cast<std::uint32_t>(high), static_cast<std::uint32_t>(low));
}

std::uint64_t make_regional_link_key(std::uint32_t from_site_id, std::uint32_t to_site_id) noexcept
{
    if (from_site_id > to_site_id)
    {
        std::swap(from_site_id, to_site_id);
    }
    return pack_u32_pair(from_site_id, to_site_id);
}

std::uint64_t make_projected_button_key(int setup_id, int element_id) noexcept
{
    return pack_u32_pair(static_cast<std::uint32_t>(setup_id), static_cast<std::uint32_t>(element_id));
}

std::uint64_t make_panel_list_row_key(int list_id, std::int64_t item_id) noexcept
{
    return pack_u32_pair(static_cast<std::uint32_t>(list_id), static_cast<std::uint32_t>(item_id));
}

std::uint64_t make_craft_option_key(int tile_x, int tile_y, int output_item_id) noexcept
{
    std::uint64_t hash = k_fnv_offset_basis;
    hash_value(hash, tile_x);
    hash_value(hash, tile_y);
    hash_value(hash, output_item_id);
    return hash;
}

template <typename T>
T* resolve_object(const ObjectID object_id)
{
    return Object::cast_to<T>(ObjectDB::get_instance(object_id));
}

Ref<PackedScene> load_packed_scene(const String& path)
{
    if (ResourceLoader* loader = ResourceLoader::get_singleton())
    {
        return loader->load(path);
    }
    return Ref<PackedScene> {};
}

Ref<Texture2D> load_texture_2d(const String& path)
{
    if (path.is_empty())
    {
        return Ref<Texture2D> {};
    }

    if (ResourceLoader* loader = ResourceLoader::get_singleton())
    {
        return loader->load(path);
    }
    return Ref<Texture2D> {};
}

Ref<ImageTexture> make_solid_icon_texture(const Color& base_color)
{
    Ref<Image> image = Image::create_empty(48, 48, false, Image::FORMAT_RGBA8);
    if (image.is_null())
    {
        return Ref<ImageTexture> {};
    }

    image->fill(base_color);
    image->fill_rect(Rect2i(4, 4, 40, 40), Color(base_color.r * 0.72f, base_color.g * 0.72f, base_color.b * 0.72f, 1.0f));
    image->fill_rect(Rect2i(8, 8, 32, 32), Color(base_color.r * 1.10f, base_color.g * 1.10f, base_color.b * 1.10f, 1.0f));
    image->fill_rect(Rect2i(12, 12, 24, 24), Color(base_color.r * 0.56f, base_color.g * 0.56f, base_color.b * 0.56f, 1.0f));
    return ImageTexture::create_from_image(image);
}

const gs1::ReputationUnlockDef* find_unlockable_def_by_title(const String& title)
{
    const CharString title_utf8 = title.utf8();
    const std::string_view title_view {title_utf8.get_data(), static_cast<std::size_t>(title_utf8.length())};
    for (const auto& unlock_def : gs1::all_reputation_unlock_defs())
    {
        if (unlock_def.display_name == title_view)
        {
            return &unlock_def;
        }
    }
    return nullptr;
}

std::uint8_t unlockable_content_kind_from_def(const gs1::ReputationUnlockDef& unlock_def)
{
    switch (unlock_def.unlock_kind)
    {
    case gs1::ReputationUnlockKind::Plant:
        return k_unlockable_content_kind_plant;
    case gs1::ReputationUnlockKind::Item:
        return k_unlockable_content_kind_item;
    case gs1::ReputationUnlockKind::StructureRecipe:
        return k_unlockable_content_kind_structure_recipe;
    case gs1::ReputationUnlockKind::Recipe:
        return k_unlockable_content_kind_recipe;
    default:
        return k_unlockable_content_kind_item;
    }
}

String trimmed_unlockable_title(const String& raw_text)
{
    const String trimmed = raw_text.strip_edges();
    const int open_paren = trimmed.find("(");
    const int need_index = trimmed.find(" Need ");
    return (open_paren > 0 ? trimmed.substr(0, open_paren)
                           : (need_index > 0 ? trimmed.substr(0, need_index) : trimmed))
        .strip_edges();
}
}

void Gs1GodotMainScreenControl::_bind_methods()
{
    ClassDB::bind_method(D_METHOD("set_runtime_node_path", "path"), &Gs1GodotMainScreenControl::set_runtime_node_path);
    ClassDB::bind_method(D_METHOD("get_runtime_node_path"), &Gs1GodotMainScreenControl::get_runtime_node_path);
    ClassDB::bind_method(D_METHOD("set_site_view_path", "path"), &Gs1GodotMainScreenControl::set_site_view_path);
    ClassDB::bind_method(D_METHOD("get_site_view_path"), &Gs1GodotMainScreenControl::get_site_view_path);
    ClassDB::bind_method(D_METHOD("set_regional_world_path", "path"), &Gs1GodotMainScreenControl::set_regional_world_path);
    ClassDB::bind_method(D_METHOD("get_regional_world_path"), &Gs1GodotMainScreenControl::get_regional_world_path);
    ClassDB::bind_method(D_METHOD("set_regional_camera_path", "path"), &Gs1GodotMainScreenControl::set_regional_camera_path);
    ClassDB::bind_method(D_METHOD("get_regional_camera_path"), &Gs1GodotMainScreenControl::get_regional_camera_path);
    ClassDB::bind_method(D_METHOD("set_regional_terrain_root_path", "path"), &Gs1GodotMainScreenControl::set_regional_terrain_root_path);
    ClassDB::bind_method(D_METHOD("get_regional_terrain_root_path"), &Gs1GodotMainScreenControl::get_regional_terrain_root_path);
    ClassDB::bind_method(D_METHOD("set_regional_link_root_path", "path"), &Gs1GodotMainScreenControl::set_regional_link_root_path);
    ClassDB::bind_method(D_METHOD("get_regional_link_root_path"), &Gs1GodotMainScreenControl::get_regional_link_root_path);
    ClassDB::bind_method(D_METHOD("set_regional_site_root_path", "path"), &Gs1GodotMainScreenControl::set_regional_site_root_path);
    ClassDB::bind_method(D_METHOD("get_regional_site_root_path"), &Gs1GodotMainScreenControl::get_regional_site_root_path);

    ClassDB::bind_method(D_METHOD("on_start_campaign_pressed"), &Gs1GodotMainScreenControl::on_start_campaign_pressed);
    ClassDB::bind_method(D_METHOD("on_continue_campaign_pressed"), &Gs1GodotMainScreenControl::on_continue_campaign_pressed);
    ClassDB::bind_method(D_METHOD("on_menu_settings_pressed"), &Gs1GodotMainScreenControl::on_menu_settings_pressed);
    ClassDB::bind_method(D_METHOD("on_quit_pressed"), &Gs1GodotMainScreenControl::on_quit_pressed);
    ClassDB::bind_method(D_METHOD("on_return_to_map_pressed"), &Gs1GodotMainScreenControl::on_return_to_map_pressed);
    ClassDB::bind_method(D_METHOD("on_move_north_pressed"), &Gs1GodotMainScreenControl::on_move_north_pressed);
    ClassDB::bind_method(D_METHOD("on_move_south_pressed"), &Gs1GodotMainScreenControl::on_move_south_pressed);
    ClassDB::bind_method(D_METHOD("on_move_west_pressed"), &Gs1GodotMainScreenControl::on_move_west_pressed);
    ClassDB::bind_method(D_METHOD("on_move_east_pressed"), &Gs1GodotMainScreenControl::on_move_east_pressed);
    ClassDB::bind_method(D_METHOD("on_hover_tile_pressed"), &Gs1GodotMainScreenControl::on_hover_tile_pressed);
    ClassDB::bind_method(D_METHOD("on_cancel_action_pressed"), &Gs1GodotMainScreenControl::on_cancel_action_pressed);
    ClassDB::bind_method(D_METHOD("on_open_protection_pressed"), &Gs1GodotMainScreenControl::on_open_protection_pressed);
    ClassDB::bind_method(D_METHOD("on_overlay_wind_pressed"), &Gs1GodotMainScreenControl::on_overlay_wind_pressed);
    ClassDB::bind_method(D_METHOD("on_overlay_heat_pressed"), &Gs1GodotMainScreenControl::on_overlay_heat_pressed);
    ClassDB::bind_method(D_METHOD("on_overlay_dust_pressed"), &Gs1GodotMainScreenControl::on_overlay_dust_pressed);
    ClassDB::bind_method(D_METHOD("on_overlay_condition_pressed"), &Gs1GodotMainScreenControl::on_overlay_condition_pressed);
    ClassDB::bind_method(D_METHOD("on_overlay_clear_pressed"), &Gs1GodotMainScreenControl::on_overlay_clear_pressed);
    ClassDB::bind_method(D_METHOD("on_open_phone_home_pressed"), &Gs1GodotMainScreenControl::on_open_phone_home_pressed);
    ClassDB::bind_method(D_METHOD("on_open_phone_tasks_pressed"), &Gs1GodotMainScreenControl::on_open_phone_tasks_pressed);
    ClassDB::bind_method(D_METHOD("on_open_phone_buy_pressed"), &Gs1GodotMainScreenControl::on_open_phone_buy_pressed);
    ClassDB::bind_method(D_METHOD("on_open_phone_sell_pressed"), &Gs1GodotMainScreenControl::on_open_phone_sell_pressed);
    ClassDB::bind_method(D_METHOD("on_open_tech_tree_pressed"), &Gs1GodotMainScreenControl::on_open_tech_tree_pressed);
    ClassDB::bind_method(D_METHOD("on_close_phone_pressed"), &Gs1GodotMainScreenControl::on_close_phone_pressed);
    ClassDB::bind_method(D_METHOD("on_open_worker_pack_pressed"), &Gs1GodotMainScreenControl::on_open_worker_pack_pressed);
    ClassDB::bind_method(D_METHOD("on_open_nearest_storage_pressed"), &Gs1GodotMainScreenControl::on_open_nearest_storage_pressed);
    ClassDB::bind_method(D_METHOD("on_close_storage_pressed"), &Gs1GodotMainScreenControl::on_close_storage_pressed);
    ClassDB::bind_method(D_METHOD("on_use_selected_item_pressed"), &Gs1GodotMainScreenControl::on_use_selected_item_pressed);
    ClassDB::bind_method(D_METHOD("on_transfer_selected_item_pressed"), &Gs1GodotMainScreenControl::on_transfer_selected_item_pressed);
    ClassDB::bind_method(D_METHOD("on_plant_selected_seed_pressed"), &Gs1GodotMainScreenControl::on_plant_selected_seed_pressed);
    ClassDB::bind_method(D_METHOD("on_craft_at_selected_tile_pressed"), &Gs1GodotMainScreenControl::on_craft_at_selected_tile_pressed);
    ClassDB::bind_method(D_METHOD("on_dynamic_regional_site_pressed", "site_id"), &Gs1GodotMainScreenControl::on_dynamic_regional_site_pressed);
    ClassDB::bind_method(D_METHOD("on_dynamic_phone_listing_pressed", "listing_id"), &Gs1GodotMainScreenControl::on_dynamic_phone_listing_pressed);
    ClassDB::bind_method(D_METHOD("on_dynamic_craft_option_pressed", "button_key"), &Gs1GodotMainScreenControl::on_dynamic_craft_option_pressed);
    ClassDB::bind_method(D_METHOD("on_dynamic_projected_action_pressed", "container_kind", "button_key"), &Gs1GodotMainScreenControl::on_dynamic_projected_action_pressed);
    ClassDB::bind_method(D_METHOD("on_fixed_slot_pressed", "button_id"), &Gs1GodotMainScreenControl::on_fixed_slot_pressed);
    ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "runtime_node_path"), "set_runtime_node_path", "get_runtime_node_path");
    ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "site_view_path"), "set_site_view_path", "get_site_view_path");
    ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "regional_world_path"), "set_regional_world_path", "get_regional_world_path");
    ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "regional_camera_path"), "set_regional_camera_path", "get_regional_camera_path");
    ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "regional_terrain_root_path"), "set_regional_terrain_root_path", "get_regional_terrain_root_path");
    ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "regional_link_root_path"), "set_regional_link_root_path", "get_regional_link_root_path");
    ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "regional_site_root_path"), "set_regional_site_root_path", "get_regional_site_root_path");
}

void Gs1GodotMainScreenControl::_ready()
{
    cache_scene_references();
    cache_ui_references();
    apply_tech_tree_overlay_layout();
    cache_fixed_slot_bindings();
    set_process(true);
    set_process_input(true);
    wire_static_buttons();
}

void Gs1GodotMainScreenControl::_process(double delta)
{
    cache_scene_references();
    cache_ui_references();

    if (runtime_node_ == nullptr)
    {
        if (status_label_ != nullptr)
        {
            status_label_->set_text("Runtime node not found");
        }
        return;
    }

    const bool projection_changed = sync_projection_from_runtime();
    const int app_state = as_int(projection_.get("app_state", APP_STATE_BOOT), APP_STATE_BOOT);

    refresh_status();
    refresh_menu(app_state);
    refresh_regional_map(app_state, projection_changed);
    refresh_site(app_state, projection_changed);
    update_background_presentation(app_state, delta);
    last_app_state_ = app_state;
}

void Gs1GodotMainScreenControl::_input(const Ref<InputEvent>& event)
{
    if (event.is_null())
    {
        return;
    }

    if (const auto* mouse_event = Object::cast_to<InputEventMouseButton>(*event))
    {
        if (mouse_event->is_pressed() && mouse_event->get_button_index() == k_mouse_button_left)
        {
            if (!regional_map_ui_contains_screen_point(mouse_event->get_position()) &&
                try_select_regional_site_from_screen(mouse_event->get_position()))
            {
                if (Viewport* viewport = get_viewport())
                {
                    viewport->set_input_as_handled();
                }
                return;
            }
        }
    }

    const auto* key_event = Object::cast_to<InputEventKey>(*event);
    if (key_event == nullptr || !key_event->is_pressed())
    {
        return;
    }
    if (site_panel_ == nullptr || !site_panel_->is_visible())
    {
        return;
    }

    switch (key_event->get_keycode())
    {
    case k_key_up:
        selected_tile_.y -= 1;
        break;
    case k_key_down:
        selected_tile_.y += 1;
        break;
    case k_key_left:
        selected_tile_.x -= 1;
        break;
    case k_key_right:
        selected_tile_.x += 1;
        break;
    default:
        return;
    }

    clamp_selected_tile();
}

void Gs1GodotMainScreenControl::cache_scene_references()
{
    if (runtime_node_ == nullptr)
    {
        runtime_node_ = Object::cast_to<Gs1RuntimeNode>(get_node_or_null(runtime_node_path_));
    }
    if (site_view_ == nullptr)
    {
        site_view_ = Object::cast_to<Node3D>(get_node_or_null(site_view_path_));
    }
    if (regional_map_world_ == nullptr)
    {
        regional_map_world_ = Object::cast_to<Node3D>(get_node_or_null(regional_world_path_));
    }
    if (regional_camera_ == nullptr)
    {
        regional_camera_ = Object::cast_to<Camera3D>(get_node_or_null(regional_camera_path_));
    }
    if (regional_terrain_root_ == nullptr)
    {
        regional_terrain_root_ = Object::cast_to<Node3D>(get_node_or_null(regional_terrain_root_path_));
    }
    if (regional_link_root_ == nullptr)
    {
        regional_link_root_ = Object::cast_to<Node3D>(get_node_or_null(regional_link_root_path_));
    }
    if (regional_site_root_ == nullptr)
    {
        regional_site_root_ = Object::cast_to<Node3D>(get_node_or_null(regional_site_root_path_));
    }
}

void Gs1GodotMainScreenControl::cache_ui_references()
{
    if (status_label_ == nullptr)
    {
        status_label_ = Object::cast_to<RichTextLabel>(find_child("StatusLabel", true, false));
    }
    if (menu_panel_ == nullptr)
    {
        menu_panel_ = Object::cast_to<PanelContainer>(find_child("MenuPanel", true, false));
    }
    if (regional_map_panel_ == nullptr)
    {
        regional_map_panel_ = Object::cast_to<PanelContainer>(find_child("RegionalMapPanel", true, false));
    }
    if (regional_map_summary_ == nullptr)
    {
        regional_map_summary_ = Object::cast_to<RichTextLabel>(find_child("RegionalMapSummary", true, false));
    }
    if (regional_map_graph_ == nullptr)
    {
        regional_map_graph_ = Object::cast_to<RichTextLabel>(find_child("RegionalMapGraph", true, false));
    }
    if (regional_action_buttons_ == nullptr)
    {
        regional_action_buttons_ = Object::cast_to<VBoxContainer>(find_child("RegionalActions", true, false));
    }
    if (regional_selection_panel_ == nullptr)
    {
        regional_selection_panel_ = Object::cast_to<PanelContainer>(find_child("RegionalSelectionPanel", true, false));
    }
    if (regional_selection_title_ == nullptr)
    {
        regional_selection_title_ = Object::cast_to<Label>(find_child("RegionalSelectionTitle", true, false));
    }
    if (regional_selection_summary_ == nullptr)
    {
        regional_selection_summary_ = Object::cast_to<RichTextLabel>(find_child("RegionalSelectionSummary", true, false));
    }
    if (regional_selection_actions_ == nullptr)
    {
        regional_selection_actions_ = Object::cast_to<VBoxContainer>(find_child("RegionalSelectionActions", true, false));
    }
    if (regional_tech_tree_overlay_ == nullptr)
    {
        regional_tech_tree_overlay_ = Object::cast_to<Control>(find_child("TechOverlay", true, false));
    }
    if (regional_tech_tree_panel_ == nullptr)
    {
        regional_tech_tree_panel_ = Object::cast_to<PanelContainer>(find_child("RegionalTechTreePanel", true, false));
    }
    if (regional_tech_tree_title_ == nullptr)
    {
        regional_tech_tree_title_ = Object::cast_to<Label>(find_child("RegionalTechTreeTitle", true, false));
    }
    if (regional_tech_tree_summary_ == nullptr)
    {
        regional_tech_tree_summary_ = Object::cast_to<ScrollContainer>(find_child("RegionalTechTreeSummary", true, false));
    }
    if (regional_tech_tree_unlockables_grid_ == nullptr)
    {
        regional_tech_tree_unlockables_grid_ = Object::cast_to<GridContainer>(find_child("RegionalTechTreeUnlockablesGrid", true, false));
    }
    if (regional_tech_tree_tech_summary_ == nullptr)
    {
        regional_tech_tree_tech_summary_ = Object::cast_to<ScrollContainer>(find_child("RegionalTechTreeTechSummary", true, false));
    }
    if (regional_tech_tree_faction_tabs_ == nullptr)
    {
        regional_tech_tree_faction_tabs_ = find_child("RegionalTechTreeFactionTabs", true, false);
    }
    if (regional_tech_tree_actions_ == nullptr)
    {
        regional_tech_tree_actions_ = Object::cast_to<GridContainer>(find_child("RegionalTechTreeActions", true, false));
    }
    if (site_panel_ == nullptr)
    {
        site_panel_ = Object::cast_to<PanelContainer>(find_child("SitePanel", true, false));
    }
    if (site_title_ == nullptr)
    {
        site_title_ = Object::cast_to<Label>(find_child("SiteTitle", true, false));
    }
    if (site_summary_ == nullptr)
    {
        site_summary_ = Object::cast_to<RichTextLabel>(find_child("SiteSummary", true, false));
    }
    if (tile_label_ == nullptr)
    {
        tile_label_ = Object::cast_to<Label>(find_child("TileLabel", true, false));
    }
    if (site_controls_ == nullptr)
    {
        site_controls_ = Object::cast_to<VBoxContainer>(find_child("SiteControls", true, false));
    }
    if (inventory_panel_ == nullptr)
    {
        inventory_panel_ = Object::cast_to<PanelContainer>(find_child("InventoryPanel", true, false));
    }
    if (inventory_summary_ == nullptr)
    {
        inventory_summary_ = Object::cast_to<RichTextLabel>(find_child("InventorySummary", true, false));
    }
    if (task_panel_ == nullptr)
    {
        task_panel_ = Object::cast_to<PanelContainer>(find_child("TaskPanel", true, false));
    }
    if (task_summary_ == nullptr)
    {
        task_summary_ = Object::cast_to<RichTextLabel>(find_child("TaskSummary", true, false));
    }
    if (task_rows_ == nullptr)
    {
        task_rows_ = Object::cast_to<VBoxContainer>(find_child("TaskRows", true, false));
    }
    if (modifier_rows_ == nullptr)
    {
        modifier_rows_ = Object::cast_to<VBoxContainer>(find_child("ModifierRows", true, false));
    }
    if (phone_panel_ == nullptr)
    {
        phone_panel_ = Object::cast_to<PanelContainer>(find_child("PhonePanel", true, false));
    }
    if (phone_listings_ == nullptr)
    {
        phone_listings_ = Object::cast_to<VBoxContainer>(find_child("PhoneListings", true, false));
    }
    if (craft_panel_ == nullptr)
    {
        craft_panel_ = Object::cast_to<PanelContainer>(find_child("CraftPanel", true, false));
    }
    if (craft_summary_ == nullptr)
    {
        craft_summary_ = Object::cast_to<RichTextLabel>(find_child("CraftSummary", true, false));
    }
    if (craft_options_ == nullptr)
    {
        craft_options_ = Object::cast_to<VBoxContainer>(find_child("CraftOptions", true, false));
    }
    if (overlay_panel_ == nullptr)
    {
        overlay_panel_ = Object::cast_to<PanelContainer>(find_child("OverlayPanel", true, false));
    }
    if (phone_state_label_ == nullptr)
    {
        phone_state_label_ = Object::cast_to<Label>(find_child("PhoneStateLabel", true, false));
    }
    if (overlay_state_label_ == nullptr)
    {
        overlay_state_label_ = Object::cast_to<Label>(find_child("OverlayStateLabel", true, false));
    }
}

void Gs1GodotMainScreenControl::apply_tech_tree_overlay_layout()
{
    if (regional_tech_tree_overlay_ == nullptr)
    {
        return;
    }

    const Variant host_left = regional_tech_tree_overlay_->get_meta("gs1_overlay_margin_left", Variant());
    const Variant host_top = regional_tech_tree_overlay_->get_meta("gs1_overlay_margin_top", Variant());
    const Variant host_right = regional_tech_tree_overlay_->get_meta("gs1_overlay_margin_right", Variant());
    const Variant host_bottom = regional_tech_tree_overlay_->get_meta("gs1_overlay_margin_bottom", Variant());
    regional_tech_tree_overlay_->set_offset(SIDE_LEFT, host_left.get_type() == Variant::NIL ? 72.0 : as_float(host_left, 72.0));
    regional_tech_tree_overlay_->set_offset(SIDE_TOP, host_top.get_type() == Variant::NIL ? 56.0 : as_float(host_top, 56.0));
    regional_tech_tree_overlay_->set_offset(SIDE_RIGHT, host_right.get_type() == Variant::NIL ? -72.0 : as_float(host_right, -72.0));
    regional_tech_tree_overlay_->set_offset(SIDE_BOTTOM, host_bottom.get_type() == Variant::NIL ? -48.0 : as_float(host_bottom, -48.0));

    if (regional_tech_tree_panel_ != nullptr)
    {
        const Variant panel_width = regional_tech_tree_overlay_->get_meta("gs1_panel_min_width", Variant());
        const Variant panel_height = regional_tech_tree_overlay_->get_meta("gs1_panel_min_height", Variant());
        regional_tech_tree_panel_->set_custom_minimum_size(
            Vector2(
                panel_width.get_type() == Variant::NIL ? 980.0 : as_float(panel_width, 980.0),
                panel_height.get_type() == Variant::NIL ? 620.0 : as_float(panel_height, 620.0)));
    }
}

void Gs1GodotMainScreenControl::wire_static_buttons()
{
    if (buttons_wired_)
    {
        return;
    }

    bind_button(Object::cast_to<BaseButton>(find_child("StartCampaignButton", true, false)), callable_mp(this, &Gs1GodotMainScreenControl::on_start_campaign_pressed));
    bind_button(Object::cast_to<BaseButton>(find_child("ContinueCampaignButton", true, false)), callable_mp(this, &Gs1GodotMainScreenControl::on_continue_campaign_pressed));
    bind_button(Object::cast_to<BaseButton>(find_child("MenuSettingsButton", true, false)), callable_mp(this, &Gs1GodotMainScreenControl::on_menu_settings_pressed));
    bind_button(Object::cast_to<BaseButton>(find_child("QuitButton", true, false)), callable_mp(this, &Gs1GodotMainScreenControl::on_quit_pressed));
    bind_button(Object::cast_to<BaseButton>(find_child("ReturnToMapButton", true, false)), callable_mp(this, &Gs1GodotMainScreenControl::on_return_to_map_pressed));
    bind_button(Object::cast_to<BaseButton>(find_child("MoveNorthButton", true, false)), callable_mp(this, &Gs1GodotMainScreenControl::on_move_north_pressed));
    bind_button(Object::cast_to<BaseButton>(find_child("MoveSouthButton", true, false)), callable_mp(this, &Gs1GodotMainScreenControl::on_move_south_pressed));
    bind_button(Object::cast_to<BaseButton>(find_child("MoveWestButton", true, false)), callable_mp(this, &Gs1GodotMainScreenControl::on_move_west_pressed));
    bind_button(Object::cast_to<BaseButton>(find_child("MoveEastButton", true, false)), callable_mp(this, &Gs1GodotMainScreenControl::on_move_east_pressed));
    bind_button(Object::cast_to<BaseButton>(find_child("HoverTileButton", true, false)), callable_mp(this, &Gs1GodotMainScreenControl::on_hover_tile_pressed));
    bind_button(Object::cast_to<BaseButton>(find_child("CancelActionButton", true, false)), callable_mp(this, &Gs1GodotMainScreenControl::on_cancel_action_pressed));
    bind_button(Object::cast_to<BaseButton>(find_child("OpenProtectionButton", true, false)), callable_mp(this, &Gs1GodotMainScreenControl::on_open_protection_pressed));
    bind_button(Object::cast_to<BaseButton>(find_child("OverlayWindButton", true, false)), callable_mp(this, &Gs1GodotMainScreenControl::on_overlay_wind_pressed));
    bind_button(Object::cast_to<BaseButton>(find_child("OverlayHeatButton", true, false)), callable_mp(this, &Gs1GodotMainScreenControl::on_overlay_heat_pressed));
    bind_button(Object::cast_to<BaseButton>(find_child("OverlayDustButton", true, false)), callable_mp(this, &Gs1GodotMainScreenControl::on_overlay_dust_pressed));
    bind_button(Object::cast_to<BaseButton>(find_child("OverlayConditionButton", true, false)), callable_mp(this, &Gs1GodotMainScreenControl::on_overlay_condition_pressed));
    bind_button(Object::cast_to<BaseButton>(find_child("OverlayClearButton", true, false)), callable_mp(this, &Gs1GodotMainScreenControl::on_overlay_clear_pressed));
    bind_button(Object::cast_to<BaseButton>(find_child("OpenPhoneHomeButton", true, false)), callable_mp(this, &Gs1GodotMainScreenControl::on_open_phone_home_pressed));
    bind_button(Object::cast_to<BaseButton>(find_child("OpenPhoneTasksButton", true, false)), callable_mp(this, &Gs1GodotMainScreenControl::on_open_phone_tasks_pressed));
    bind_button(Object::cast_to<BaseButton>(find_child("OpenPhoneBuyButton", true, false)), callable_mp(this, &Gs1GodotMainScreenControl::on_open_phone_buy_pressed));
    bind_button(Object::cast_to<BaseButton>(find_child("OpenPhoneSellButton", true, false)), callable_mp(this, &Gs1GodotMainScreenControl::on_open_phone_sell_pressed));
    bind_button(Object::cast_to<BaseButton>(find_child("OpenTechTreeButton", true, false)), callable_mp(this, &Gs1GodotMainScreenControl::on_open_tech_tree_pressed));
    bind_button(Object::cast_to<BaseButton>(find_child("RegionalTechTreeCloseButton", true, false)), callable_mp(this, &Gs1GodotMainScreenControl::on_open_tech_tree_pressed));
    bind_button(Object::cast_to<BaseButton>(find_child("ClosePhoneButton", true, false)), callable_mp(this, &Gs1GodotMainScreenControl::on_close_phone_pressed));
    bind_button(Object::cast_to<BaseButton>(find_child("OpenWorkerPackButton", true, false)), callable_mp(this, &Gs1GodotMainScreenControl::on_open_worker_pack_pressed));
    bind_button(Object::cast_to<BaseButton>(find_child("OpenNearestStorageButton", true, false)), callable_mp(this, &Gs1GodotMainScreenControl::on_open_nearest_storage_pressed));
    bind_button(Object::cast_to<BaseButton>(find_child("CloseStorageButton", true, false)), callable_mp(this, &Gs1GodotMainScreenControl::on_close_storage_pressed));
    bind_button(Object::cast_to<BaseButton>(find_child("UseSelectedItemButton", true, false)), callable_mp(this, &Gs1GodotMainScreenControl::on_use_selected_item_pressed));
    bind_button(Object::cast_to<BaseButton>(find_child("TransferSelectedItemButton", true, false)), callable_mp(this, &Gs1GodotMainScreenControl::on_transfer_selected_item_pressed));
    bind_button(Object::cast_to<BaseButton>(find_child("PlantSelectedSeedButton", true, false)), callable_mp(this, &Gs1GodotMainScreenControl::on_plant_selected_seed_pressed));
    bind_button(Object::cast_to<BaseButton>(find_child("CraftAtSelectedTileButton", true, false)), callable_mp(this, &Gs1GodotMainScreenControl::on_craft_at_selected_tile_pressed));

    buttons_wired_ = true;
}

bool Gs1GodotMainScreenControl::sync_projection_from_runtime()
{
    if (runtime_node_ == nullptr)
    {
        return false;
    }

    const std::uint64_t runtime_revision = runtime_node_->projection_revision();
    if (runtime_revision == last_projection_revision_)
    {
        return false;
    }

    projection_ = runtime_node_->get_projection();
    last_projection_revision_ = runtime_revision;
    selected_site_id_ = as_int(projection_.get("selected_site_id", selected_site_id_), selected_site_id_);
    return true;
}

void Gs1GodotMainScreenControl::refresh_status()
{
    if (status_label_ == nullptr)
    {
        return;
    }

    PackedStringArray lines;
    lines.push_back("[b]Field Operations Feed[/b]");
    lines.push_back(vformat("Runtime: %s", bool_text(as_bool(projection_.get("running", false), false), "linked", "idle")));
    lines.push_back(vformat("Screen: %s", app_state_name(as_int(projection_.get("app_state", APP_STATE_BOOT), APP_STATE_BOOT))));

    Dictionary campaign_resources = projection_.get("campaign_resources", Dictionary());
    if (!campaign_resources.is_empty())
    {
        lines.push_back(vformat("Campaign Cash: %.2f", as_float(campaign_resources.get("current_money", 0.0), 0.0)));
        lines.push_back(vformat(
            "Reputation T/V/B/U: %d / %d / %d / %d",
            as_int(campaign_resources.get("total_reputation", 0), 0),
            as_int(campaign_resources.get("village_reputation", 0), 0),
            as_int(campaign_resources.get("forestry_reputation", 0), 0),
            as_int(campaign_resources.get("university_reputation", 0), 0)));
    }
    if (!last_action_message_.is_empty())
    {
        lines.push_back(vformat("Last Action: %s", last_action_message_));
    }
    const String last_error = projection_.get("last_error", String());
    if (!last_error.is_empty())
    {
        lines.push_back(vformat("Error: %s", last_error));
    }

    status_label_->set_text(String("\n").join(lines));
}

void Gs1GodotMainScreenControl::refresh_menu(int app_state)
{
    const bool visible = app_state == APP_STATE_MAIN_MENU || app_state == APP_STATE_BOOT;
    if (menu_panel_ != nullptr)
    {
        menu_panel_->set_visible(visible);
    }
    if (visible)
    {
        apply_fixed_panel_actions();
    }
}

void Gs1GodotMainScreenControl::apply_fixed_panel_actions()
{
    clear_fixed_slot_actions();
    bind_fixed_slot_actions(find_ui_panel(1), 1);
    bind_fixed_slot_actions(find_ui_panel(2), 2);
    bind_fixed_slot_actions(find_ui_panel(3), 3);
}

void Gs1GodotMainScreenControl::refresh_regional_map(int app_state, bool projection_changed)
{
    const bool panel_visible = app_state == APP_STATE_REGIONAL_MAP || app_state == APP_STATE_SITE_LOADING;
    if (regional_map_panel_ != nullptr)
    {
        regional_map_panel_->set_visible(panel_visible);
    }
    if (regional_map_world_ != nullptr)
    {
        regional_map_world_->set_visible(panel_visible);
    }
    if (site_view_ != nullptr && panel_visible)
    {
        site_view_->set_visible(false);
    }
    if (regional_selection_panel_ != nullptr)
    {
        regional_selection_panel_->set_visible(panel_visible);
    }
    if (!panel_visible)
    {
        regional_map_visible_ = false;
        if (regional_tech_tree_overlay_ != nullptr)
        {
            regional_tech_tree_overlay_->set_visible(false);
        }
        if (regional_tech_tree_panel_ != nullptr)
        {
            regional_tech_tree_panel_->set_visible(false);
        }
        return;
    }

    const bool force_refresh = projection_changed || !regional_map_visible_;
    regional_map_visible_ = true;

    const auto& projection_state = runtime_node_->projection_state();
    const auto& sites = projection_state.regional_map_sites;
    const auto& links = projection_state.regional_map_links;

    if (selected_site_id_ == 0 && !sites.empty())
    {
        selected_site_id_ = static_cast<int>(sites.front().site_id);
    }

    const Gs1RuntimeRegionalMapSiteProjection* selected_site = nullptr;
    for (const auto& site : sites)
    {
        if (static_cast<int>(site.site_id) == selected_site_id_)
        {
            selected_site = &site;
            break;
        }
    }

    if (selected_site == nullptr && !sites.empty())
    {
        selected_site = &sites.front();
        selected_site_id_ = static_cast<int>(selected_site->site_id);
    }

    PackedStringArray lines;
    lines.push_back("[b]Campaign Planning Map[/b]");
    lines.push_back(vformat("Revealed Sites: %d  Adjacency Links: %d", static_cast<int>(sites.size()), static_cast<int>(links.size())));
    lines.push_back(selected_site_id_ != 0 ? vformat("Selected Site: Site %d", selected_site_id_) : String("Selected Site: None"));

    const Dictionary campaign_resources = projection_.get("campaign_resources", Dictionary());
    if (!campaign_resources.is_empty())
    {
        lines.push_back(vformat("Campaign Cash: %.2f", as_float(campaign_resources.get("current_money", 0.0), 0.0)));
        lines.push_back(vformat("Total Reputation: %d", as_int(campaign_resources.get("total_reputation", 0), 0)));
    }
    lines.push_back("Open the tech tree here, then choose a site marker to review support and deploy.");

    if (regional_map_summary_ != nullptr)
    {
        regional_map_summary_->set_text(String("\n").join(lines));
    }
    if (regional_map_graph_ != nullptr)
    {
        regional_map_graph_->set_text(build_regional_map_overview_text(sites, links));
    }

    if (force_refresh)
    {
        apply_fixed_panel_actions();
        rebuild_regional_map_world(sites, links);
        render_regional_tech_tree();
    }

    if (force_refresh || selected_site_id_ != last_rendered_selected_site_id_)
    {
        render_regional_selection(selected_site);
        last_rendered_selected_site_id_ = selected_site_id_;
    }

    update_regional_site_visuals();
}

void Gs1GodotMainScreenControl::refresh_site(int app_state, bool projection_changed)
{
    const Dictionary site_state = active_site();
    const bool panel_visible = !site_state.is_empty() && app_state >= APP_STATE_SITE_LOADING && app_state <= APP_STATE_SITE_RESULT;

    if (site_panel_ != nullptr)
    {
        site_panel_->set_visible(panel_visible);
    }
    if (inventory_panel_ != nullptr)
    {
        inventory_panel_->set_visible(panel_visible);
    }
    if (task_panel_ != nullptr)
    {
        task_panel_->set_visible(panel_visible);
    }
    if (phone_panel_ != nullptr)
    {
        phone_panel_->set_visible(panel_visible);
    }
    if (craft_panel_ != nullptr)
    {
        craft_panel_->set_visible(panel_visible);
    }
    if (overlay_panel_ != nullptr)
    {
        overlay_panel_->set_visible(panel_visible);
    }
    if (site_view_ != nullptr)
    {
        site_view_->set_visible(panel_visible);
    }
    if (regional_map_world_ != nullptr && panel_visible)
    {
        regional_map_world_->set_visible(false);
    }
    if (!panel_visible)
    {
        site_panel_visible_ = false;
        return;
    }

    const bool force_refresh = projection_changed || !site_panel_visible_;
    site_panel_visible_ = true;

    if (force_refresh)
    {
        cached_storage_lookup_.clear();
        if (site_title_ != nullptr)
        {
            site_title_->set_text(vformat("Site %d", as_int(site_state.get("site_id", 0), 0)));
        }

        PackedStringArray site_lines;
        site_lines.push_back(vformat("Size: %d x %d", as_int(site_state.get("width", 0), 0), as_int(site_state.get("height", 0), 0)));

        const Dictionary worker = site_state.get("worker", Dictionary());
        if (!worker.is_empty())
        {
            site_lines.push_back(vformat(
                "Worker: (%.1f, %.1f)  Action %d",
                as_float(worker.get("tile_x", 0.0), 0.0),
                as_float(worker.get("tile_y", 0.0), 0.0),
                as_int(worker.get("current_action_kind", 0), 0)));
        }

        const Dictionary weather = site_state.get("weather", Dictionary());
        if (!weather.is_empty())
        {
            site_lines.push_back(vformat(
                "Weather H/W/D: %.1f / %.1f / %.1f",
                as_float(weather.get("heat", 0.0), 0.0),
                as_float(weather.get("wind", 0.0), 0.0),
                as_float(weather.get("dust", 0.0), 0.0)));
        }

        const Dictionary hud = projection_.get("hud", Dictionary());
        if (!hud.is_empty())
        {
            site_lines.push_back(vformat(
                "Meters HP/HY/NO/EN/MO: %.1f / %.1f / %.1f / %.1f / %.1f",
                as_float(hud.get("player_health", 0.0), 0.0),
                as_float(hud.get("player_hydration", 0.0), 0.0),
                as_float(hud.get("player_nourishment", 0.0), 0.0),
                as_float(hud.get("player_energy", 0.0), 0.0),
                as_float(hud.get("player_morale", 0.0), 0.0)));
        }

        const Dictionary site_action = projection_.get("site_action", Dictionary());
        if (!site_action.is_empty())
        {
            site_lines.push_back(vformat(
                "Current Action: kind %d  progress %.2f",
                as_int(site_action.get("action_kind", 0), 0),
                as_float(site_action.get("progress_normalized", 0.0), 0.0)));
        }

        if (site_summary_ != nullptr)
        {
            site_summary_->set_text(String("\n").join(site_lines));
        }

        render_inventory(site_state);
        render_tasks(site_state);
        render_phone(site_state);
        render_craft(site_state);
        render_overlay(site_state);
        render_projected_ui_buttons(site_controls_, {6, 7, 8, 10, 11, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26});
    }

    clamp_selected_tile();
    const Dictionary selected_tile = tile_at(selected_tile_);
    if (tile_label_ != nullptr)
    {
        String tile_text = vformat(
            "Selected Tile: (%d, %d)  Plant: %s  Structure: %s",
            selected_tile_.x,
            selected_tile_.y,
            String(selected_tile.get("plant_name", "None")),
            String(selected_tile.get("structure_name", "None")));
        if (!selected_tile.is_empty())
        {
            tile_text += vformat(
                "  Wind %.2f  Moisture %.2f  Fertility %.2f",
                as_float(selected_tile.get("local_wind", 0.0), 0.0),
                as_float(selected_tile.get("moisture", 0.0), 0.0),
                as_float(selected_tile.get("soil_fertility", 0.0), 0.0));
        }
        tile_label_->set_text(tile_text);
    }
}

void Gs1GodotMainScreenControl::render_inventory(const Dictionary& site_state)
{
    if (inventory_summary_ == nullptr)
    {
        return;
    }

    PackedStringArray lines;
    lines.push_back("[b]Inventory[/b]");
    lines.push_back(vformat("Worker Pack Open: %s", bool_text(as_bool(site_state.get("worker_pack_open", false), false), "yes", "no")));

    const Array worker_pack_slots = site_state.get("worker_pack_slots", Array());
    for (int64_t index = 0; index < worker_pack_slots.size(); ++index)
    {
        const Dictionary slot = worker_pack_slots[index];
        lines.push_back(vformat(
            "Pack %d: %s x%d",
            as_int(slot.get("slot_index", 0), 0),
            String(slot.get("item_name", "Empty")),
            as_int(slot.get("quantity", 0), 0)));
    }

    const Dictionary opened_storage = site_state.get("opened_storage", Dictionary());
    if (!opened_storage.is_empty())
    {
        lines.push_back(String());
        lines.push_back(vformat("Opened Storage %d", as_int(opened_storage.get("storage_id", 0), 0)));
        const Array slots = opened_storage.get("slots", Array());
        for (int64_t index = 0; index < slots.size(); ++index)
        {
            const Dictionary slot = slots[index];
            lines.push_back(vformat(
                "Slot %d: %s x%d",
                as_int(slot.get("slot_index", 0), 0),
                String(slot.get("item_name", "Empty")),
                as_int(slot.get("quantity", 0), 0)));
        }
    }

    inventory_summary_->set_text(String("\n").join(lines));
}

void Gs1GodotMainScreenControl::render_tasks(const Dictionary& site_state)
{
    if (task_summary_ == nullptr)
    {
        return;
    }

    PackedStringArray lines;
    lines.push_back("[b]Tasks[/b]");
    const Array tasks = site_state.get("tasks", Array());
    for (int64_t index = 0; index < tasks.size(); ++index)
    {
        const Dictionary task = tasks[index];
        lines.push_back(vformat(
            "Task %d  progress %d / %d  list %d",
            as_int(task.get("task_template_id", 0), 0),
            as_int(task.get("current_progress", 0), 0),
            as_int(task.get("target_progress", 0), 0),
            as_int(task.get("list_kind", 0), 0)));
    }

    task_summary_->set_text(String("\n").join(lines));
    reconcile_task_rows(tasks);
    reconcile_modifier_rows(site_state.get("active_modifiers", Array()));
}

void Gs1GodotMainScreenControl::render_phone(const Dictionary& site_state)
{
    const Dictionary phone_panel = site_state.get("phone_panel", Dictionary());
    if (phone_state_label_ != nullptr)
    {
        phone_state_label_->set_text(vformat(
            "Phone Section: %d  Listings: buy %d sell %d",
            as_int(phone_panel.get("active_section", 0), 0),
            as_int(phone_panel.get("buy_listing_count", 0), 0),
            as_int(phone_panel.get("sell_listing_count", 0), 0)));
    }

    if (phone_listings_ == nullptr)
    {
        return;
    }

    const Array phone_listings = site_state.get("phone_listings", Array());
    reconcile_phone_listing_buttons(phone_listings);
}

void Gs1GodotMainScreenControl::render_craft(const Dictionary& site_state)
{
    if (craft_summary_ == nullptr)
    {
        return;
    }

    PackedStringArray lines;
    lines.push_back("[b]Craft / Placement[/b]");

    const Dictionary preview = site_state.get("placement_preview", Dictionary());
    if (!preview.is_empty())
    {
        lines.push_back(vformat(
            "Placement: %s at (%d, %d)  blocked %d",
            String(preview.get("item_name", "Item")),
            as_int(preview.get("tile_x", 0), 0),
            as_int(preview.get("tile_y", 0), 0),
            as_int(preview.get("blocked_mask", 0), 0)));
    }

    const Dictionary failure = site_state.get("placement_failure", Dictionary());
    if (!failure.is_empty())
    {
        lines.push_back(vformat(
            "Placement Failure: tile (%d, %d) flags %d",
            as_int(failure.get("tile_x", 0), 0),
            as_int(failure.get("tile_y", 0), 0),
            as_int(failure.get("flags", 0), 0)));
    }

    const Dictionary craft_context = site_state.get("craft_context", Dictionary());
    if (!craft_context.is_empty())
    {
        lines.push_back(vformat(
            "Craft Context @ (%d, %d)",
            as_int(craft_context.get("tile_x", 0), 0),
            as_int(craft_context.get("tile_y", 0), 0)));
    }

    craft_summary_->set_text(String("\n").join(lines));
    if (craft_options_ == nullptr)
    {
        return;
    }

    reconcile_craft_option_buttons(craft_context);
}

void Gs1GodotMainScreenControl::render_overlay(const Dictionary& site_state)
{
    if (overlay_state_label_ == nullptr)
    {
        return;
    }

    const Dictionary overlay = site_state.get("protection_overlay", Dictionary());
    overlay_state_label_->set_text(vformat("Overlay Mode: %d", as_int(overlay.get("mode", 0), 0)));
}

void Gs1GodotMainScreenControl::reconcile_task_rows(const Array& tasks)
{
    if (task_rows_ == nullptr)
    {
        return;
    }

    std::unordered_set<std::uint64_t> desired_keys;
    int desired_index = 0;
    for (int64_t index = 0; index < tasks.size(); ++index)
    {
        const Dictionary task = tasks[index];
        const int task_instance_id = as_int(task.get("task_instance_id", 0), 0);
        const std::uint64_t stable_key = static_cast<std::uint64_t>(task_instance_id);
        desired_keys.insert(stable_key);
        Button* button = upsert_button_node(
            task_rows_,
            task_buttons_,
            stable_key,
            vformat("DynamicTask_%d", task_instance_id),
            desired_index++);
        if (button == nullptr)
        {
            continue;
        }

        const String icon_path = String(task.get("resource_icon_path", String()));
        const Ref<Texture2D> icon_texture = load_texture_2d(icon_path);
        if (icon_texture.is_valid())
        {
            button->set_button_icon(icon_texture);
        }
        else
        {
            button->set_button_icon(Ref<Texture2D> {});
        }

        button->set_text(vformat(
            "Task %d  progress %d/%d  tier %d",
            as_int(task.get("task_template_id", 0), 0),
            as_int(task.get("current_progress", 0), 0),
            as_int(task.get("target_progress", 0), 0),
            as_int(task.get("task_tier_id", 0), 0)));
        button->set_tooltip_text(vformat(
            "Task template %d, progress kind %d, list %d",
            as_int(task.get("task_template_id", 0), 0),
            as_int(task.get("progress_kind", 0), 0),
            as_int(task.get("list_kind", 0), 0)));
        button->set_disabled(true);
        button->set_focus_mode(Control::FOCUS_NONE);
    }

    prune_button_registry(task_buttons_, desired_keys);
}

void Gs1GodotMainScreenControl::reconcile_modifier_rows(const Array& modifiers)
{
    if (modifier_rows_ == nullptr)
    {
        return;
    }

    std::unordered_set<std::uint64_t> desired_keys;
    int desired_index = 0;
    for (int64_t index = 0; index < modifiers.size(); ++index)
    {
        const Dictionary modifier = modifiers[index];
        const int modifier_id = as_int(modifier.get("modifier_id", 0), 0);
        const std::uint64_t stable_key = pack_u32_pair(
            static_cast<std::uint32_t>(modifier_id),
            static_cast<std::uint32_t>(index));
        desired_keys.insert(stable_key);
        Button* button = upsert_button_node(
            modifier_rows_,
            modifier_buttons_,
            stable_key,
            vformat("DynamicModifier_%d", modifier_id),
            desired_index++);
        if (button == nullptr)
        {
            continue;
        }

        const String icon_path = String(modifier.get("resource_icon_path", String()));
        const Ref<Texture2D> icon_texture = load_texture_2d(icon_path);
        if (icon_texture.is_valid())
        {
            button->set_button_icon(icon_texture);
        }
        else
        {
            button->set_button_icon(Ref<Texture2D> {});
        }

        const String label = String(modifier.get("display_name", String())).is_empty()
            ? vformat("Modifier %d", modifier_id)
            : String(modifier.get("display_name", String()));
        button->set_text(vformat(
            "%s  %s",
            label,
            (as_int(modifier.get("flags", 0), 0) & 1) != 0
                ? vformat("%dh left", as_int(modifier.get("remaining_game_hours", 0), 0))
                : String("Permanent")));
        button->set_tooltip_text(String(modifier.get("description", String())));
        button->set_disabled(true);
        button->set_focus_mode(Control::FOCUS_NONE);
    }

    prune_button_registry(modifier_buttons_, desired_keys);
}

void Gs1GodotMainScreenControl::render_projected_ui_buttons(VBoxContainer* container, const std::initializer_list<int>& allowed_action_types)
{
    if (container == nullptr)
    {
        return;
    }

    Array button_specs;
    const Array ui_setups = projection_.get("ui_setups", Array());
    for (int64_t setup_index = 0; setup_index < ui_setups.size(); ++setup_index)
    {
        const Dictionary setup = ui_setups[setup_index];
        const Array elements = setup.get("elements", Array());
        for (int64_t element_index = 0; element_index < elements.size(); ++element_index)
        {
            const Dictionary element = elements[element_index];
            const Dictionary action = element.get("action", Dictionary());
            const int action_type = as_int(action.get("type", 0), 0);
            if (std::find(allowed_action_types.begin(), allowed_action_types.end(), action_type) == allowed_action_types.end())
            {
                continue;
            }
            if (as_int(element.get("element_type", 0), 0) != 1)
            {
                continue;
            }

            Dictionary spec;
            spec["setup_id"] = as_int(setup.get("setup_id", 0), 0);
            spec["element_id"] = as_int(element.get("element_id", 0), 0);
            spec["text"] = String(element.get("text", "Action"));
            spec["flags"] = as_int(element.get("flags", 0), 0);
            spec["action"] = action;
            button_specs.push_back(spec);
        }
    }

    reconcile_projected_action_buttons(
        container,
        site_control_buttons_,
        PROJECTED_ACTION_CONTAINER_SITE_CONTROLS,
        button_specs);
}

void Gs1GodotMainScreenControl::render_regional_selection(const Gs1RuntimeRegionalMapSiteProjection* selected_site)
{
    if (regional_selection_panel_ == nullptr)
    {
        return;
    }

    const Dictionary selection_setup = find_ui_setup(2);
    if (selected_site == nullptr)
    {
        regional_selection_panel_->set_visible(false);
        return;
    }

    regional_selection_panel_->set_visible(true);
    if (regional_selection_title_ != nullptr)
    {
        regional_selection_title_->set_text(vformat("Selected Site %d", static_cast<int>(selected_site->site_id)));
    }

    PackedStringArray summary_lines;
    summary_lines.push_back(vformat("[b]%s[/b]", regional_site_button_text(*selected_site)));
    summary_lines.push_back(regional_site_tooltip(*selected_site));
    summary_lines.push_back(String());
    summary_lines.push_back("[b]Deployment Brief[/b]");
    summary_lines.push_back(regional_site_deployment_summary(*selected_site));

    PackedStringArray label_lines;
    Array button_specs;
    const Array elements = selection_setup.get("elements", Array());
    for (int64_t index = 0; index < elements.size(); ++index)
    {
        const Dictionary element = elements[index];
        const int element_type = as_int(element.get("element_type", 0), 0);
        const String text = String(element.get("text", "")).strip_edges();
        if (element_type == 2)
        {
            if (!text.is_empty())
            {
                label_lines.push_back(text);
            }
            continue;
        }
        if (element_type != 1 || regional_selection_actions_ == nullptr)
        {
            continue;
        }

        const Dictionary action = element.get("action", Dictionary());
        const int action_type = as_int(action.get("type", 0), 0);
        if (action_type == UI_ACTION_CLEAR_DEPLOYMENT_SITE_SELECTION && text.is_empty())
        {
            continue;
        }

        Dictionary spec;
        spec["setup_id"] = as_int(selection_setup.get("setup_id", 0), 0);
        spec["element_id"] = as_int(element.get("element_id", 0), 0);
        spec["text"] = regional_selection_action_label(text, action);
        spec["flags"] = as_int(element.get("flags", 0), 0);
        spec["action"] = action;
        button_specs.push_back(spec);
    }

    if (button_specs.is_empty())
    {
        const int selected_site_id = static_cast<int>(selected_site->site_id);

        Dictionary deploy_action;
        deploy_action["type"] = UI_ACTION_START_SITE_ATTEMPT;
        deploy_action["target_id"] = selected_site_id;
        deploy_action["arg0"] = 0;
        deploy_action["arg1"] = 0;

        Dictionary deploy_spec;
        deploy_spec["setup_id"] = -1;
        deploy_spec["element_id"] = UI_ACTION_START_SITE_ATTEMPT;
        deploy_spec["text"] = vformat("Deploy To Site %d", selected_site_id);
        deploy_spec["flags"] = selected_site->site_state == GS1_SITE_STATE_LOCKED ? 2 : 0;
        deploy_spec["action"] = deploy_action;
        button_specs.push_back(deploy_spec);

        Dictionary clear_action;
        clear_action["type"] = UI_ACTION_CLEAR_DEPLOYMENT_SITE_SELECTION;
        clear_action["target_id"] = 0;
        clear_action["arg0"] = 0;
        clear_action["arg1"] = 0;

        Dictionary clear_spec;
        clear_spec["setup_id"] = -1;
        clear_spec["element_id"] = UI_ACTION_CLEAR_DEPLOYMENT_SITE_SELECTION;
        clear_spec["text"] = "Clear Selection";
        clear_spec["flags"] = 0;
        clear_spec["action"] = clear_action;
        button_specs.push_back(clear_spec);
    }

    reconcile_projected_action_buttons(
        regional_selection_actions_,
        regional_selection_action_buttons_,
        PROJECTED_ACTION_CONTAINER_REGIONAL_SELECTION,
        button_specs);

    if (label_lines.size() > 0)
    {
        summary_lines.push_back(String());
        summary_lines.push_back("[b]Loadout And Support[/b]");
        for (int64_t index = 0; index < label_lines.size(); ++index)
        {
            summary_lines.push_back(label_lines[index]);
        }
    }

    if (regional_selection_summary_ != nullptr)
    {
        regional_selection_summary_->set_text(String("\n").join(summary_lines));
    }
}

void Gs1GodotMainScreenControl::render_regional_tech_tree()
{
    if (regional_tech_tree_overlay_ == nullptr || regional_tech_tree_panel_ == nullptr)
    {
        return;
    }

    const Dictionary tech_setup = find_ui_setup(5);
    if (tech_setup.is_empty())
    {
        regional_tech_tree_overlay_->set_visible(false);
        regional_tech_tree_panel_->set_visible(false);
        return;
    }

    bool has_close_action = false;
    std::uint32_t selected_faction_id = 0U;
    const Array elements = tech_setup.get("elements", Array());
    for (int64_t index = 0; index < elements.size(); ++index)
    {
        const Dictionary element = elements[index];
        const Dictionary action = element.get("action", Dictionary());
        const int action_type = as_int(action.get("type", 0), 0);
        if (action_type == UI_ACTION_CLOSE_REGIONAL_MAP_TECH_TREE)
        {
            has_close_action = true;
        }
        if (action_type == UI_ACTION_SELECT_TECH_TREE_FACTION_TAB &&
            (as_int(element.get("flags", 0), 0) & 1) != 0)
        {
            selected_faction_id = static_cast<std::uint32_t>(as_int(action.get("target_id", 0), 0));
        }
    }

    regional_tech_tree_overlay_->set_visible(has_close_action);
    regional_tech_tree_panel_->set_visible(has_close_action);
    if (!has_close_action)
    {
        reconcile_projected_action_buttons(
            regional_tech_tree_faction_tabs_,
            regional_tech_tree_faction_tab_buttons_,
            PROJECTED_ACTION_CONTAINER_REGIONAL_TECH_TREE,
            Array {});
        reconcile_tech_tree_cards(
            regional_tech_tree_unlockables_grid_,
            regional_tech_tree_unlockable_buttons_,
            Array {},
            false);
        reconcile_tech_tree_cards(
            regional_tech_tree_actions_,
            regional_tech_tree_action_buttons_,
            Array {},
            true);
        return;
    }

    if (regional_tech_tree_title_ != nullptr)
    {
        regional_tech_tree_title_->set_text("Research And Unlocks");
    }

    Array faction_tab_specs;
    Array unlockable_specs;
    Array action_specs;
    for (int64_t index = 0; index < elements.size(); ++index)
    {
        const Dictionary element = elements[index];
        const String text = String(element.get("text", "")).strip_edges();
        const Dictionary action = element.get("action", Dictionary());
        const int action_type = as_int(action.get("type", 0), 0);
        const int element_type = as_int(element.get("element_type", 0), 0);
        const int element_flags = as_int(element.get("flags", 0), 0);
        if (action_type == UI_ACTION_CLOSE_REGIONAL_MAP_TECH_TREE)
        {
            continue;
        }

        if (action_type == UI_ACTION_SELECT_TECH_TREE_FACTION_TAB)
        {
            Dictionary spec;
            spec["setup_id"] = as_int(tech_setup.get("setup_id", 0), 0);
            spec["element_id"] = as_int(element.get("element_id", 0), 0);
            spec["text"] = text.begins_with("Tab: ") ? text.substr(5) : text;
            spec["flags"] = element_flags;
            spec["action"] = action;
            faction_tab_specs.push_back(spec);
            continue;
        }

        const bool compact_tech_node = is_compact_tech_tree_node_text(text);
        const auto* node_def = compact_tech_node ? find_tech_tree_node_def(action) : nullptr;
        if (node_def != nullptr &&
            selected_faction_id != 0U &&
            node_def->faction_id.value != selected_faction_id)
        {
            continue;
        }
        const String compact_node_text =
            compact_tech_node ? regional_tech_tree_node_text(action, text) : String();
        if (compact_tech_node)
        {
            Dictionary spec;
            spec["setup_id"] = as_int(tech_setup.get("setup_id", 0), 0);
            spec["element_id"] = as_int(element.get("element_id", 0), 0);
            spec["text"] = text;
            spec["flags"] = element_flags;
            spec["action"] = action;
            spec["card_text"] = compact_node_text;
            spec["tooltip"] = regional_tech_tooltip_text(action, text);
            action_specs.push_back(spec);
            continue;
        }
        else if (action_type == 0 || element_type == 2)
        {
            if (!text.is_empty())
            {
                const bool is_tech_line =
                    text == "Tier Tech Tree" ||
                    text == "32 linear tiers per faction" ||
                    text.begins_with("Tier ") ||
                    text.begins_with("Village Rep ") ||
                    text.begins_with("Forestry Rep ") ||
                    text.begins_with("University Rep ");
                if (is_tech_line)
                {
                    continue;
                }
                else
                {
                    Dictionary preview_spec;
                    preview_spec["text"] = text;
                    const String title_text = regional_card_title_text(preview_spec);
                    if (!title_text.is_empty())
                    {
                        Dictionary spec;
                        spec["setup_id"] = as_int(tech_setup.get("setup_id", 0), 0);
                        spec["element_id"] = as_int(element.get("element_id", 0), 0);
                        spec["text"] = text;
                        spec["flags"] = element_flags;
                        spec["tooltip"] = regional_unlockable_tooltip_text(text);
                        unlockable_specs.push_back(spec);
                    }
                }
            }
            continue;
        }

        if (action_type != 0)
        {
            Dictionary spec;
            spec["setup_id"] = as_int(tech_setup.get("setup_id", 0), 0);
            spec["element_id"] = as_int(element.get("element_id", 0), 0);
            spec["text"] = text.is_empty() ? String("Action") : text;
            spec["flags"] = element_flags;
            spec["action"] = action;
            action_specs.push_back(spec);
        }
    }

    reconcile_projected_action_buttons(
        regional_tech_tree_faction_tabs_,
        regional_tech_tree_faction_tab_buttons_,
        PROJECTED_ACTION_CONTAINER_REGIONAL_TECH_TREE,
        faction_tab_specs);
    reconcile_tech_tree_cards(
        regional_tech_tree_unlockables_grid_,
        regional_tech_tree_unlockable_buttons_,
        unlockable_specs,
        false);
    reconcile_tech_tree_cards(
        regional_tech_tree_actions_,
        regional_tech_tree_action_buttons_,
        action_specs,
        true);
}

void Gs1GodotMainScreenControl::toggle_regional_tech_tree()
{
    if (regional_tech_tree_panel_ != nullptr && regional_tech_tree_panel_->is_visible())
    {
        submit_ui_action(UI_ACTION_CLOSE_REGIONAL_MAP_TECH_TREE);
    }
    else
    {
        submit_ui_action(UI_ACTION_OPEN_REGIONAL_MAP_TECH_TREE);
    }
}

void Gs1GodotMainScreenControl::select_regional_site(int site_id, bool submit_runtime)
{
    selected_site_id_ = site_id;
    if (submit_runtime)
    {
        submit_ui_action(UI_ACTION_SELECT_DEPLOYMENT_SITE, selected_site_id_);
    }
    update_regional_site_visuals();
}

void Gs1GodotMainScreenControl::rebuild_regional_map_world(
    const std::vector<Gs1RuntimeRegionalMapSiteProjection>& sites,
    const std::vector<Gs1RuntimeRegionalMapLinkProjection>& links)
{
    if (regional_map_world_ == nullptr || regional_terrain_root_ == nullptr || regional_link_root_ == nullptr || regional_site_root_ == nullptr)
    {
        return;
    }

    if (sites.empty())
    {
        clear_regional_projection_world();
        return;
    }

    if (regional_menu_backdrop_active_)
    {
        clear_regional_projection_world();
        regional_menu_backdrop_active_ = false;
    }

    int min_x = std::numeric_limits<int>::max();
    int max_x = std::numeric_limits<int>::min();
    int min_y = std::numeric_limits<int>::max();
    int max_y = std::numeric_limits<int>::min();

    regional_site_data_.clear();
    regional_site_data_.reserve(sites.size());
    for (const auto& site : sites)
    {
        const Vector2i grid = regional_grid_coord(site);
        min_x = std::min(min_x, grid.x);
        max_x = std::max(max_x, grid.x);
        min_y = std::min(min_y, grid.y);
        max_y = std::max(max_y, grid.y);
        regional_site_data_[static_cast<int>(site.site_id)] = site;
    }

    regional_map_bounds_ = Rect2i(
        min_x - REGIONAL_WORLD_PADDING,
        min_y - REGIONAL_WORLD_PADDING,
        (max_x - min_x) + 1 + REGIONAL_WORLD_PADDING * 2,
        (max_y - min_y) + 1 + REGIONAL_WORLD_PADDING * 2);

    reconcile_desert_tiles(regional_map_bounds_, true);
    reconcile_regional_sites(sites);
    reconcile_regional_links(links);
    position_regional_camera(regional_map_bounds_);
    update_regional_site_visuals();
}

void Gs1GodotMainScreenControl::clear_regional_projection_world()
{
    clear_dynamic_children(regional_terrain_root_, String());
    clear_dynamic_children(regional_link_root_, String());
    clear_dynamic_children(regional_site_root_, String());
    regional_terrain_tile_nodes_.clear();
    regional_terrain_grid_line_nodes_.clear();
    regional_link_nodes_.clear();
    regional_site_nodes_.clear();
    regional_site_data_.clear();
    menu_backdrop_plants_.clear();
    menu_backdrop_time_ = 0.0;
}

void Gs1GodotMainScreenControl::build_menu_backdrop()
{
    regional_menu_backdrop_active_ = false;
    menu_backdrop_plants_.clear();
    menu_backdrop_time_ = 0.0;
}

void Gs1GodotMainScreenControl::reconcile_desert_tiles(const Rect2i& bounds, bool include_grid_lines)
{
    if (regional_terrain_root_ == nullptr)
    {
        return;
    }

    (void)bounds;
    (void)include_grid_lines;
    std::unordered_set<std::uint64_t> desired_tile_keys;
    std::unordered_set<std::uint64_t> desired_grid_line_keys;
    prune_regional_node_registry(regional_terrain_tile_nodes_, desired_tile_keys);
    prune_regional_node_registry(regional_terrain_grid_line_nodes_, desired_grid_line_keys);
}

void Gs1GodotMainScreenControl::reconcile_regional_links(const std::vector<Gs1RuntimeRegionalMapLinkProjection>& links)
{
    if (regional_link_root_ == nullptr)
    {
        return;
    }

    std::unordered_set<std::uint64_t> desired_link_keys;
    for (const auto& link : links)
    {
        const int from_site_id = static_cast<int>(link.from_site_id);
        const int to_site_id = static_cast<int>(link.to_site_id);
        const std::uint64_t link_key = make_regional_link_key(link.from_site_id, link.to_site_id);

        const auto from_it = regional_site_data_.find(from_site_id);
        const auto to_it = regional_site_data_.find(to_site_id);
        if (from_it == regional_site_data_.end() || to_it == regional_site_data_.end())
        {
            continue;
        }
        desired_link_keys.insert(link_key);

        const Vector3 from_position = regional_world_position(regional_grid_coord(from_it->second)) + Vector3(0.0, 0.22, 0.0);
        const Vector3 to_position = regional_world_position(regional_grid_coord(to_it->second)) + Vector3(0.0, 0.22, 0.0);

        MeshInstance3D* segment = upsert_regional_mesh_node(
            regional_link_root_,
            regional_link_nodes_,
            link_key,
            vformat("Link_%d_%d", from_site_id, to_site_id),
            static_cast<int>(desired_link_keys.size()) - 1);
        Ref<BoxMesh> segment_mesh;
        segment_mesh.instantiate();
        segment_mesh->set_size(Vector3(0.42, 0.18, from_position.distance_to(to_position)));
        segment->set_mesh(segment_mesh);
        segment->set_position((from_position + to_position) * 0.5);
        segment->look_at(to_position, Vector3(0.0, 1.0, 0.0), true);
        segment->rotate_object_local(Vector3(1.0, 0.0, 0.0), Math::deg_to_rad(90.0));
        segment->set_material_override(get_material("link_path", Color(0.42, 0.31, 0.22), 0.94, 0.02));
    }

    prune_regional_node_registry(regional_link_nodes_, desired_link_keys);
}

void Gs1GodotMainScreenControl::reconcile_regional_sites(const std::vector<Gs1RuntimeRegionalMapSiteProjection>& sites)
{
    if (regional_site_root_ == nullptr)
    {
        return;
    }
    std::unordered_set<int> desired_site_ids;
    for (const auto& site : sites)
    {
        const int site_id = static_cast<int>(site.site_id);
        desired_site_ids.insert(site_id);
        auto& record = regional_site_nodes_[site_id];

        Node3D* root = resolve_object<Node3D>(record.root_id);
        if (root == nullptr)
        {
            const String scene_path = GodotProgressionResourceDatabase::instance().site_scene_path(site.site_id);
            const Ref<PackedScene> marker_scene = load_packed_scene(scene_path);
            if (!marker_scene.is_null())
            {
                root = Object::cast_to<Node3D>(marker_scene->instantiate());
            }
            if (root == nullptr)
            {
                root = make_node3d();
            }
            regional_site_root_->add_child(root);
            record.root_id = root->get_instance_id();
        }
        root->set_name(vformat("Site_%d", site_id));
        root->set_position(regional_world_position(regional_grid_coord(site)));
        root->set_rotation_degrees(Vector3(0.0, static_cast<double>((site_id * 37) % 360), 0.0));
        root->set_meta("site_id", site_id);

        MeshInstance3D* base = resolve_object<MeshInstance3D>(record.base_id);
        if (base == nullptr)
        {
            base = Object::cast_to<MeshInstance3D>(root->find_child("StateBase", true, false));
            if (base != nullptr)
            {
                record.base_id = base->get_instance_id();
            }
        }
        if (base == nullptr)
        {
            base = make_mesh_instance3d();
            root->add_child(base);
            record.base_id = base->get_instance_id();
        }
        Ref<CylinderMesh> base_mesh;
        base_mesh.instantiate();
        base_mesh->set_top_radius(1.05);
        base_mesh->set_bottom_radius(1.28);
        base_mesh->set_height(0.42);
        base->set_mesh(base_mesh);
        base->set_position(Vector3(0.0, 0.2, 0.0));

        MeshInstance3D* tower = resolve_object<MeshInstance3D>(record.tower_id);
        if (tower == nullptr)
        {
            tower = Object::cast_to<MeshInstance3D>(root->find_child("StateTower", true, false));
            if (tower != nullptr)
            {
                record.tower_id = tower->get_instance_id();
            }
        }
        if (tower == nullptr)
        {
            tower = make_mesh_instance3d();
            root->add_child(tower);
            record.tower_id = tower->get_instance_id();
        }
        Ref<CylinderMesh> tower_mesh;
        tower_mesh.instantiate();
        tower_mesh->set_top_radius(0.42);
        tower_mesh->set_bottom_radius(0.58);
        tower_mesh->set_height(1.6);
        tower->set_mesh(tower_mesh);
        tower->set_position(Vector3(0.0, 1.1, 0.0));

        MeshInstance3D* beacon = resolve_object<MeshInstance3D>(record.beacon_id);
        if (beacon == nullptr)
        {
            beacon = Object::cast_to<MeshInstance3D>(root->find_child("StateBeacon", true, false));
            if (beacon != nullptr)
            {
                record.beacon_id = beacon->get_instance_id();
            }
        }
        if (beacon == nullptr)
        {
            beacon = make_mesh_instance3d();
            root->add_child(beacon);
            record.beacon_id = beacon->get_instance_id();
        }
        Ref<SphereMesh> beacon_mesh;
        beacon_mesh.instantiate();
        beacon_mesh->set_radius(0.34);
        beacon_mesh->set_height(0.68);
        beacon->set_mesh(beacon_mesh);
        beacon->set_position(Vector3(0.0, 2.05, 0.0));

        Label3D* nameplate = resolve_object<Label3D>(record.label_id);
        if (nameplate == nullptr)
        {
            nameplate = Object::cast_to<Label3D>(root->find_child("SiteLabel", true, false));
            if (nameplate != nullptr)
            {
                record.label_id = nameplate->get_instance_id();
            }
        }
        if (nameplate == nullptr)
        {
            nameplate = make_label3d();
            root->add_child(nameplate);
            record.label_id = nameplate->get_instance_id();
        }
        nameplate->set_text(vformat("SITE %d", site_id));
        nameplate->set_billboard_mode(BaseMaterial3D::BILLBOARD_ENABLED);
        nameplate->set_font_size(46);
        nameplate->set_modulate(Color(0.93, 0.88, 0.77));
        nameplate->set_position(Vector3(0.0, 2.85, 0.0));
    }

    prune_regional_site_registry(desired_site_ids);
}

void Gs1GodotMainScreenControl::position_regional_camera(const Rect2i& bounds)
{
    if (regional_camera_ == nullptr)
    {
        return;
    }

    const double center_x = static_cast<double>(bounds.position.x) + static_cast<double>(bounds.size.x - 1) * 0.5;
    const double center_y = static_cast<double>(bounds.position.y) + static_cast<double>(bounds.size.y - 1) * 0.5;
    const Vector3 center = regional_world_position(Vector2i(Math::round(center_x), Math::round(center_y)));
    const double longest_side = static_cast<double>(std::max(bounds.size.x, bounds.size.y)) * REGIONAL_TILE_SIZE;

    regional_camera_->set_position(center + Vector3(0.0, 12.0 + longest_side * 0.75, 9.0 + longest_side * 0.62));
    regional_camera_->look_at(center + Vector3(0.0, 1.6, 0.0), Vector3(0.0, 1.0, 0.0));
}

void Gs1GodotMainScreenControl::update_regional_site_visuals()
{
    for (auto& [site_id, record] : regional_site_nodes_)
    {
        auto site_it = regional_site_data_.find(site_id);
        Node3D* root = resolve_object<Node3D>(record.root_id);
        MeshInstance3D* base = resolve_object<MeshInstance3D>(record.base_id);
        MeshInstance3D* tower = resolve_object<MeshInstance3D>(record.tower_id);
        MeshInstance3D* beacon = resolve_object<MeshInstance3D>(record.beacon_id);
        Label3D* label = resolve_object<Label3D>(record.label_id);
        if (root == nullptr || site_it == regional_site_data_.end())
        {
            continue;
        }

        const auto& site = site_it->second;
        const bool selected = site_id == selected_site_id_;
        const Vector3 world_position = regional_world_position(regional_grid_coord(site));
        Color base_color = regional_site_state_color(static_cast<int>(site.site_state));
        Color tower_color = base_color.lightened(0.12);
        Color glow_color = base_color.lightened(0.32);
        if (selected)
        {
            base_color = base_color.lightened(0.18);
            tower_color = tower_color.lightened(0.24);
            glow_color = Color(0.96, 0.84, 0.52);
        }
        root->set_position(world_position + Vector3(0.0, selected ? 0.12 : 0.0, 0.0));
        root->set_scale(selected ? Vector3(1.06, 1.06, 1.06) : Vector3(1.0, 1.0, 1.0));

        if (base != nullptr)
        {
            base->set_material_override(get_material(vformat("site_base_%d_%s", site_id, selected ? "1" : "0"), base_color, 0.9, 0.02));
        }
        if (tower != nullptr)
        {
            tower->set_material_override(get_material(vformat("site_tower_%d_%s", site_id, selected ? "1" : "0"), tower_color, 0.88, 0.05));
        }
        if (beacon != nullptr)
        {
            beacon->set_material_override(get_material(vformat("site_beacon_%d_%s", site_id, selected ? "1" : "0"), glow_color, 0.22, 0.0, selected));
        }
        if (label != nullptr)
        {
            label->set_modulate(glow_color.lightened(0.18));
        }
    }
}

String Gs1GodotMainScreenControl::build_regional_map_overview_text(
    const std::vector<Gs1RuntimeRegionalMapSiteProjection>& sites,
    const std::vector<Gs1RuntimeRegionalMapLinkProjection>& links) const
{
    PackedStringArray lines;
    lines.push_back("[b]Regional Situation[/b]");
    if (sites.empty())
    {
        lines.push_back("No revealed sites yet.");
        return String("\n").join(lines);
    }

    int available_count = 0;
    int completed_count = 0;
    for (const auto& site : sites)
    {
        if (site.site_state == GS1_SITE_STATE_AVAILABLE)
        {
            ++available_count;
        }
        else if (site.site_state == GS1_SITE_STATE_COMPLETED)
        {
            ++completed_count;
        }
    }

    lines.push_back(vformat("Available: %d  Completed Support Sites: %d", available_count, completed_count));
    lines.push_back("Known routes cut across the dune basin, with marked camp outposts standing in for active support sites.");
    lines.push_back(String());
    lines.push_back("[b]Adjacency[/b]");

    for (const auto& link : links)
    {
        lines.push_back(vformat(
            "Site %d <-> Site %d",
            static_cast<int>(link.from_site_id),
            static_cast<int>(link.to_site_id)));
    }
    return String("\n").join(lines);
}

String Gs1GodotMainScreenControl::regional_site_deployment_summary(const Gs1RuntimeRegionalMapSiteProjection& site) const
{
    const String state_name = regional_site_state_name(static_cast<int>(site.site_state));
    const Vector2i grid = regional_grid_coord(site);
    const String support_preview = regional_support_preview_text(static_cast<int>(site.support_preview_mask));
    const int package_id = static_cast<int>(site.support_package_id);
    return vformat(
        "Status: %s\nGrid: (%d, %d)\nNearby support: %s\nLoadout package channel: %d\nSelect this node, review the support feed, then deploy from this panel.",
        state_name,
        grid.x,
        grid.y,
        support_preview,
        package_id);
}

String Gs1GodotMainScreenControl::regional_support_preview_text(int preview_mask) const
{
    PackedStringArray support_lines;
    if ((preview_mask & 1) != 0)
    {
        support_lines.push_back("loadout items");
    }
    if ((preview_mask & 2) != 0)
    {
        support_lines.push_back("wind shielding");
    }
    if ((preview_mask & 4) != 0)
    {
        support_lines.push_back("fertility lift");
    }
    if ((preview_mask & 8) != 0)
    {
        support_lines.push_back("recovery support");
    }
    return support_lines.is_empty() ? String("None") : String(", ").join(support_lines);
}

Vector2i Gs1GodotMainScreenControl::regional_grid_coord(const Gs1RuntimeRegionalMapSiteProjection& site) const
{
    return Vector2i(
        Math::round(site.map_x / 160.0),
        Math::round(site.map_y / 160.0));
}

Vector3 Gs1GodotMainScreenControl::regional_world_position(const Vector2i& grid) const
{
    return Vector3(static_cast<double>(grid.x) * REGIONAL_TILE_SIZE, 0.0, static_cast<double>(grid.y) * REGIONAL_TILE_SIZE);
}

Color Gs1GodotMainScreenControl::regional_site_state_color(int site_state) const
{
    switch (site_state)
    {
    case 1:
        return Color(0.71, 0.56, 0.29);
    case 2:
        return Color(0.36, 0.58, 0.39);
    default:
        return Color(0.38, 0.35, 0.32);
    }
}

void Gs1GodotMainScreenControl::update_background_presentation(int app_state, double delta)
{
    const bool show_regional_world = app_state == APP_STATE_MAIN_MENU || app_state == APP_STATE_BOOT || app_state == APP_STATE_REGIONAL_MAP || app_state == APP_STATE_SITE_LOADING;
    if (regional_map_world_ != nullptr)
    {
        regional_map_world_->set_visible(show_regional_world);
        if (show_regional_world)
        {
            Vector3 rotation = regional_map_world_->get_rotation_degrees();
            rotation.y = static_cast<real_t>(Math::lerp(
                static_cast<double>(rotation.y),
                0.0,
                std::min(delta * 2.0, 1.0)));
            regional_map_world_->set_rotation_degrees(rotation);
        }
    }
    if (regional_camera_ != nullptr)
    {
        regional_camera_->set_current(show_regional_world);
    }
}

Ref<StandardMaterial3D> Gs1GodotMainScreenControl::get_material(const String& key, const Color& color, double roughness, double metallic, bool emission)
{
    const std::string cache_key = key.utf8().get_data();
    auto it = regional_material_cache_.find(cache_key);
    if (it != regional_material_cache_.end())
    {
        return it->second;
    }

    Ref<StandardMaterial3D> standard_material;
    standard_material.instantiate();
    standard_material->set_albedo(color);
    standard_material->set_roughness(roughness);
    standard_material->set_metallic(metallic);
    if (color.a < 0.999)
    {
        standard_material->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
        standard_material->set_flag(BaseMaterial3D::FLAG_DISABLE_DEPTH_TEST, false);
    }
    if (emission)
    {
        standard_material->set_feature(BaseMaterial3D::FEATURE_EMISSION, true);
        standard_material->set_emission(color);
        standard_material->set_emission_energy_multiplier(0.7);
    }

    regional_material_cache_[cache_key] = standard_material;
    return standard_material;
}

Dictionary Gs1GodotMainScreenControl::find_ui_setup(int setup_id) const
{
    const Array setups = projection_.get("ui_setups", Array());
    for (int64_t index = 0; index < setups.size(); ++index)
    {
        const Dictionary setup = setups[index];
        if (as_int(setup.get("setup_id", 0), 0) == setup_id)
        {
            return setup;
        }
    }
    return Dictionary();
}

Dictionary Gs1GodotMainScreenControl::find_ui_panel(int panel_id) const
{
    const Array panels = projection_.get("ui_panels", Array());
    for (int64_t index = 0; index < panels.size(); ++index)
    {
        const Dictionary panel = panels[index];
        if (as_int(panel.get("panel_id", 0), 0) == panel_id)
        {
            return panel;
        }
    }
    return Dictionary();
}

Dictionary Gs1GodotMainScreenControl::find_panel_slot_action(const Dictionary& panel, int slot_id) const
{
    const Array actions = panel.get("slot_actions", Array());
    for (int64_t index = 0; index < actions.size(); ++index)
    {
        const Dictionary action = actions[index];
        if (as_int(action.get("slot_id", 0), 0) == slot_id)
        {
            return action;
        }
    }
    return Dictionary();
}

Dictionary Gs1GodotMainScreenControl::find_panel_list_action(const Dictionary& panel, int list_id, std::int64_t item_id, int role) const
{
    const Array actions = panel.get("list_actions", Array());
    for (int64_t index = 0; index < actions.size(); ++index)
    {
        const Dictionary action = actions[index];
        if (as_int(action.get("list_id", 0), 0) != list_id)
        {
            continue;
        }
        if (static_cast<std::int64_t>(as_int(action.get("item_id", 0), 0)) != item_id)
        {
            continue;
        }
        if (as_int(action.get("role", 0), 0) == role)
        {
            return action;
        }
    }
    return Dictionary();
}

void Gs1GodotMainScreenControl::cache_fixed_slot_bindings()
{
    if (fixed_slot_bindings_cached_)
    {
        return;
    }

    fixed_slot_bindings_.clear();
    Array children;
    get_tree()->get_nodes_in_group("gs1_slot_button");
    const Array slot_nodes = get_tree() == nullptr ? Array() : get_tree()->get_nodes_in_group("gs1_slot_button");
    for (int64_t index = 0; index < slot_nodes.size(); ++index)
    {
        BaseButton* button = Object::cast_to<BaseButton>(slot_nodes[index]);
        if (button == nullptr || !is_ancestor_of(button))
        {
            continue;
        }

        FixedSlotBinding binding {};
        binding.object_id = button->get_instance_id();
        binding.panel_id = as_int(button->get_meta("gs1_panel_id", Variant()), 0);
        binding.slot_id = as_int(button->get_meta("gs1_slot_id", Variant()), 0);
        if (binding.panel_id == 0 || binding.slot_id == 0)
        {
            continue;
        }

        fixed_slot_bindings_.push_back(binding);
        const Callable callback = callable_mp(this, &Gs1GodotMainScreenControl::on_fixed_slot_pressed)
                                      .bind(static_cast<std::int64_t>(binding.object_id));
        if (!button->is_connected("pressed", callback))
        {
            button->connect("pressed", callback);
        }
    }

    fixed_slot_bindings_cached_ = true;
}

void Gs1GodotMainScreenControl::clear_fixed_slot_actions()
{
    for (const FixedSlotBinding& binding : fixed_slot_bindings_)
    {
        if (BaseButton* button = resolve_object<BaseButton>(binding.object_id))
        {
            clear_action_from_button(button);
        }
    }
}

void Gs1GodotMainScreenControl::bind_fixed_slot_actions(const Dictionary& panel, int panel_id)
{
    for (const FixedSlotBinding& binding : fixed_slot_bindings_)
    {
        if (binding.panel_id != panel_id)
        {
            continue;
        }

        BaseButton* button = resolve_object<BaseButton>(binding.object_id);
        if (button == nullptr)
        {
            continue;
        }

        const Dictionary slot_action = find_panel_slot_action(panel, binding.slot_id);
        if (slot_action.is_empty())
        {
            clear_action_from_button(button);
            continue;
        }

        const String fallback_label = Object::cast_to<Button>(button) != nullptr
            ? Object::cast_to<Button>(button)->get_text()
            : String();
        apply_action_to_button(button, slot_action, String(slot_action.get("label", fallback_label)));
    }
}

void Gs1GodotMainScreenControl::apply_action_to_button(BaseButton* button, const Dictionary& action, const String& fallback_label)
{
    if (button == nullptr)
    {
        return;
    }

    const Dictionary payload = action.get("action", Dictionary());
    button->set_meta("gs1_action_type", as_int(payload.get("type", 0), 0));
    button->set_meta("gs1_target_id", as_int(payload.get("target_id", 0), 0));
    button->set_meta("gs1_arg0", as_int(payload.get("arg0", 0), 0));
    button->set_meta("gs1_arg1", as_int(payload.get("arg1", 0), 0));
    button->set_meta("gs1_has_action", true);
    button->set_disabled((as_int(action.get("flags", 0), 0) & 2) != 0);
    if (Button* text_button = Object::cast_to<Button>(button))
    {
        text_button->set_text(fallback_label);
    }
}

void Gs1GodotMainScreenControl::clear_action_from_button(BaseButton* button, bool preserve_text)
{
    if (button == nullptr)
    {
        return;
    }

    button->set_meta("gs1_action_type", 0);
    button->set_meta("gs1_target_id", 0);
    button->set_meta("gs1_arg0", 0);
    button->set_meta("gs1_arg1", 0);
    button->set_meta("gs1_has_action", false);
    button->set_disabled(true);
    if (!preserve_text)
    {
        if (Button* text_button = Object::cast_to<Button>(button))
        {
            text_button->set_text(String());
        }
    }
}

std::uint64_t Gs1GodotMainScreenControl::ui_action_hash(const Dictionary& action) const
{
    std::uint64_t hash = k_fnv_offset_basis;
    const int type = as_int(action.get("type", 0), 0);
    const int target_id = as_int(action.get("target_id", 0), 0);
    const int arg0 = as_int(action.get("arg0", 0), 0);
    const int arg1 = as_int(action.get("arg1", 0), 0);
    hash_value(hash, type);
    hash_value(hash, target_id);
    hash_value(hash, arg0);
    hash_value(hash, arg1);
    return hash;
}

void Gs1GodotMainScreenControl::clear_dynamic_children(Node* container, const String& prefix)
{
    if (container == nullptr)
    {
        return;
    }

    const Array children = container->get_children();
    for (int64_t index = 0; index < children.size(); ++index)
    {
        Node* child = Object::cast_to<Node>(children[index]);
        if (child == nullptr)
        {
            continue;
        }
        if (prefix.is_empty() || String(child->get_name()).begins_with(prefix))
        {
            child->queue_free();
        }
    }
}

Button* Gs1GodotMainScreenControl::upsert_button_node(
    Node* container,
    std::unordered_map<std::uint64_t, ProjectedButtonRecord>& registry,
    std::uint64_t stable_key,
    const String& node_name,
    int desired_index)
{
    if (container == nullptr)
    {
        return nullptr;
    }

    Button* button = nullptr;
    auto found = registry.find(stable_key);
    if (found != registry.end())
    {
        button = resolve_object<Button>(found->second.object_id);
    }

    if (button == nullptr)
    {
        button = make_button();
        container->add_child(button);
        registry[stable_key].object_id = button->get_instance_id();
    }

    button->set_name(node_name);
    button->set_custom_minimum_size(Vector2(0.0, 48.0));
    const int static_child_count = std::max(0, container->get_child_count() - static_cast<int>(registry.size()));
    const int target_index = std::min(static_child_count + desired_index, container->get_child_count() - 1);
    if (button->get_index() != target_index)
    {
        container->move_child(button, target_index);
    }
    return button;
}

void Gs1GodotMainScreenControl::prune_button_registry(
    std::unordered_map<std::uint64_t, ProjectedButtonRecord>& registry,
    const std::unordered_set<std::uint64_t>& desired_keys)
{
    std::vector<std::uint64_t> stale_keys;
    stale_keys.reserve(registry.size());
    for (const auto& [stable_key, record] : registry)
    {
        if (!desired_keys.contains(stable_key))
        {
            stale_keys.push_back(stable_key);
        }
    }

    for (const std::uint64_t stable_key : stale_keys)
    {
        auto found = registry.find(stable_key);
        if (found == registry.end())
        {
            continue;
        }

        if (Button* button = resolve_object<Button>(found->second.object_id))
        {
            button->queue_free();
        }
        registry.erase(found);
    }
}

MeshInstance3D* Gs1GodotMainScreenControl::upsert_regional_mesh_node(
    Node3D* container,
    std::unordered_map<std::uint64_t, ObjectID>& registry,
    std::uint64_t stable_key,
    const String& node_name,
    int desired_index)
{
    if (container == nullptr)
    {
        return nullptr;
    }

    MeshInstance3D* mesh_instance = nullptr;
    auto found = registry.find(stable_key);
    if (found != registry.end())
    {
        mesh_instance = resolve_object<MeshInstance3D>(found->second);
    }

    if (mesh_instance == nullptr)
    {
        mesh_instance = make_mesh_instance3d();
        container->add_child(mesh_instance);
        registry[stable_key] = mesh_instance->get_instance_id();
    }

    mesh_instance->set_name(node_name);
    const int target_index = std::min(desired_index, container->get_child_count() - 1);
    if (mesh_instance->get_index() != target_index)
    {
        container->move_child(mesh_instance, target_index);
    }
    return mesh_instance;
}

void Gs1GodotMainScreenControl::prune_regional_node_registry(
    std::unordered_map<std::uint64_t, ObjectID>& registry,
    const std::unordered_set<std::uint64_t>& desired_keys)
{
    std::vector<std::uint64_t> stale_keys;
    stale_keys.reserve(registry.size());
    for (const auto& [stable_key, object_id] : registry)
    {
        if (!desired_keys.contains(stable_key))
        {
            stale_keys.push_back(stable_key);
        }
    }

    for (const std::uint64_t stable_key : stale_keys)
    {
        auto found = registry.find(stable_key);
        if (found == registry.end())
        {
            continue;
        }

        if (Node* node = resolve_object<Node>(found->second))
        {
            node->queue_free();
        }
        registry.erase(found);
    }
}

void Gs1GodotMainScreenControl::prune_regional_site_registry(const std::unordered_set<int>& desired_site_ids)
{
    std::vector<int> stale_site_ids;
    stale_site_ids.reserve(regional_site_nodes_.size());
    for (const auto& [site_id, record] : regional_site_nodes_)
    {
        if (!desired_site_ids.contains(site_id))
        {
            stale_site_ids.push_back(site_id);
        }
    }

    for (const int site_id : stale_site_ids)
    {
        auto found = regional_site_nodes_.find(site_id);
        if (found == regional_site_nodes_.end())
        {
            continue;
        }

        if (Node3D* root = resolve_object<Node3D>(found->second.root_id))
        {
            root->queue_free();
        }
        regional_site_nodes_.erase(found);
    }
}

void Gs1GodotMainScreenControl::reconcile_phone_listing_buttons(const Array& phone_listings)
{
    if (phone_listings_ == nullptr)
    {
        return;
    }

    std::unordered_set<std::uint64_t> desired_keys;
    int desired_index = 0;
    for (int64_t index = 0; index < phone_listings.size(); ++index)
    {
        const Dictionary listing = phone_listings[index];
        const int listing_id = as_int(listing.get("listing_id", 0), 0);
        const std::uint64_t stable_key = static_cast<std::uint64_t>(listing_id);
        desired_keys.insert(stable_key);
        Button* button = upsert_button_node(
            phone_listings_,
            phone_listing_buttons_,
            stable_key,
            vformat("DynamicPhone_%d", listing_id),
            desired_index++);
        if (button == nullptr)
        {
            continue;
        }

        const String icon_text = as_bool(listing.get("is_unlockable", false), false) ? String("[UNL]") : String("[ITM]");
        button->set_text(vformat(
            "%s %s  x%d  price %.2f",
            icon_text,
            String(listing.get("label", "Listing")),
            as_int(listing.get("quantity", 0), 0),
            as_float(listing.get("price", 0.0), 0.0)));
        button->set_tooltip_text(vformat(
            "listing %d kind %d id %d",
            listing_id,
            as_int(listing.get("listing_kind", 0), 0),
            as_int(listing.get("item_or_unlockable_id", 0), 0)));
        const Callable callback = callable_mp(this, &Gs1GodotMainScreenControl::on_dynamic_phone_listing_pressed).bind(listing_id);
        if (!button->is_connected("pressed", callback))
        {
            button->connect("pressed", callback);
        }
    }

    prune_button_registry(phone_listing_buttons_, desired_keys);
}

void Gs1GodotMainScreenControl::reconcile_craft_option_buttons(const Dictionary& craft_context)
{
    if (craft_options_ == nullptr)
    {
        return;
    }

    std::unordered_set<std::uint64_t> desired_keys;
    if (!craft_context.is_empty())
    {
        const Array options = craft_context.get("options", Array());
        const int context_x = as_int(craft_context.get("tile_x", selected_tile_.x), selected_tile_.x);
        const int context_y = as_int(craft_context.get("tile_y", selected_tile_.y), selected_tile_.y);
        int desired_index = 0;
        for (int64_t index = 0; index < options.size(); ++index)
        {
            const Dictionary option = options[index];
            const int output_item_id = as_int(option.get("output_item_id", 0), 0);
            const std::uint64_t stable_key = make_craft_option_key(context_x, context_y, output_item_id);
            desired_keys.insert(stable_key);
            Button* button = upsert_button_node(
                craft_options_,
                craft_option_buttons_,
                stable_key,
                vformat("DynamicCraft_%d", output_item_id),
                desired_index++);
            if (button == nullptr)
            {
                continue;
            }

            button->set_text(vformat("[ITM] Craft %s", String(option.get("output_item_name", "Output"))));
            button->set_tooltip_text(String());
            button->set_meta("tile_x", context_x);
            button->set_meta("tile_y", context_y);
            button->set_meta("output_item_id", output_item_id);
            const Callable callback = callable_mp(this, &Gs1GodotMainScreenControl::on_dynamic_craft_option_pressed).bind(static_cast<int64_t>(stable_key));
            if (!button->is_connected("pressed", callback))
            {
                button->connect("pressed", callback);
            }
        }
    }

    prune_button_registry(craft_option_buttons_, desired_keys);
}

void Gs1GodotMainScreenControl::reconcile_projected_action_buttons(
    Node* container,
    std::unordered_map<std::uint64_t, ProjectedButtonRecord>& registry,
    int container_kind,
    const Array& button_specs)
{
    if (container == nullptr)
    {
        return;
    }

    std::unordered_set<std::uint64_t> desired_keys;
    int desired_index = 0;
    for (int64_t index = 0; index < button_specs.size(); ++index)
    {
        const Dictionary spec = button_specs[index];
        const int setup_id = as_int(spec.get("setup_id", 0), 0);
        const int element_id = as_int(spec.get("element_id", 0), 0);
        const std::uint64_t stable_key = make_projected_button_key(setup_id, element_id);
        desired_keys.insert(stable_key);
        Button* button = upsert_button_node(
            container,
            registry,
            stable_key,
            vformat("Projected_%d_%d", setup_id, element_id),
            desired_index++);
        if (button == nullptr)
        {
            continue;
        }

        const Dictionary action = spec.get("action", Dictionary());
        button->set_text(String(spec.get("text", "Action")));
        button->set_tooltip_text(String());
        button->set_disabled((as_int(spec.get("flags", 0), 0) & 2) != 0);
        button->set_meta("action_type", as_int(action.get("type", 0), 0));
        button->set_meta("target_id", as_int(action.get("target_id", 0), 0));
        button->set_meta("arg0", as_int(action.get("arg0", 0), 0));
        button->set_meta("arg1", as_int(action.get("arg1", 0), 0));
        const Callable callback = callable_mp(this, &Gs1GodotMainScreenControl::on_dynamic_projected_action_pressed)
                                      .bind(container_kind, static_cast<int64_t>(stable_key));
        if (!button->is_connected("pressed", callback))
        {
            button->connect("pressed", callback);
        }
    }

    prune_button_registry(registry, desired_keys);
}

void Gs1GodotMainScreenControl::reconcile_tech_tree_cards(
    GridContainer* container,
    std::unordered_map<std::uint64_t, ProjectedButtonRecord>& registry,
    const Array& card_specs,
    bool allow_actions)
{
    if (container == nullptr)
    {
        return;
    }

    std::unordered_set<std::uint64_t> desired_keys;
    int desired_index = 0;
    for (int64_t index = 0; index < card_specs.size(); ++index)
    {
        const Dictionary spec = card_specs[index];
        const int setup_id = as_int(spec.get("setup_id", 0), 0);
        const int element_id = as_int(spec.get("element_id", 0), 0);
        const std::uint64_t stable_key = make_projected_button_key(setup_id, element_id);
        desired_keys.insert(stable_key);
        Button* button = upsert_button_node(
            container,
            registry,
            stable_key,
            vformat("Projected_%d_%d", setup_id, element_id),
            desired_index++);
        if (button == nullptr)
        {
            continue;
        }

        const String tooltip = String(spec.get("tooltip", ""));
        const int flags = as_int(spec.get("flags", 0), 0);
        const Dictionary action = spec.get("action", Dictionary());
        ProjectedButtonRecord& record = registry[stable_key];
        ensure_card_content_nodes(button, record);

        button->set_text(String());
        button->set_tooltip_text(tooltip);
        button->set_custom_minimum_size(Vector2(184, 148));
        button->set_clip_text(false);
        button->set_flat(true);
        button->set_text_alignment(HORIZONTAL_ALIGNMENT_CENTER);
        const bool locked = (flags & 2) != 0;
        button->set_disabled(false);
        button->set_focus_mode(Control::FOCUS_NONE);
        button->set_meta("action_type", as_int(action.get("type", 0), 0));
        button->set_meta("target_id", as_int(action.get("target_id", 0), 0));
        button->set_meta("arg0", as_int(action.get("arg0", 0), 0));
        button->set_meta("arg1", as_int(action.get("arg1", 0), 0));
        button->set_meta(
            "gs1_allow_action",
            allow_actions && !locked && as_int(action.get("type", 0), 0) != 0);

        if (Label* icon_label = resolve_object<Label>(record.icon_label_id))
        {
            icon_label->set_text(regional_card_icon_text(spec));
        }
        if (TextureRect* icon_texture = resolve_object<TextureRect>(record.icon_texture_id))
        {
            icon_texture->set_texture(regional_card_icon_texture(spec));
        }
        if (Label* title_label = resolve_object<Label>(record.title_label_id))
        {
            title_label->set_text(regional_card_title_text(spec));
        }
        if (Label* subtitle_label = resolve_object<Label>(record.subtitle_label_id))
        {
            subtitle_label->set_text(regional_card_subtitle_text(spec));
        }
        if (Label* status_label = resolve_object<Label>(record.status_label_id))
        {
            status_label->set_text(regional_card_status_text(spec));
            status_label->add_theme_color_override("font_color", regional_card_status_color(spec));
        }
        if (ColorRect* lock_overlay = resolve_object<ColorRect>(record.lock_overlay_id))
        {
            lock_overlay->set_visible(locked);
        }
        if (Label* lock_label = resolve_object<Label>(record.lock_label_id))
        {
            lock_label->set_visible(locked);
        }

        const Callable callback = callable_mp(this, &Gs1GodotMainScreenControl::on_dynamic_projected_action_pressed)
                                      .bind(PROJECTED_ACTION_CONTAINER_REGIONAL_TECH_TREE, static_cast<int64_t>(stable_key));
        if (!button->is_connected("pressed", callback))
        {
            button->connect("pressed", callback);
        }
    }

    prune_button_registry(registry, desired_keys);
}

String Gs1GodotMainScreenControl::regional_site_button_text(const Gs1RuntimeRegionalMapSiteProjection& site) const
{
    const int site_id = static_cast<int>(site.site_id);
    const String coords = vformat(
        "(%d, %d)",
        Math::round(site.map_x / 160.0),
        Math::round(site.map_y / 160.0));
    return vformat("Site %d  %s  %s", site_id, regional_site_state_name(static_cast<int>(site.site_state)), coords);
}

String Gs1GodotMainScreenControl::regional_site_tooltip(const Gs1RuntimeRegionalMapSiteProjection& site) const
{
    const int preview_mask = static_cast<int>(site.support_preview_mask);
    PackedStringArray support_lines;
    if ((preview_mask & 1) != 0)
    {
        support_lines.push_back("items");
    }
    if ((preview_mask & 2) != 0)
    {
        support_lines.push_back("wind");
    }
    if ((preview_mask & 4) != 0)
    {
        support_lines.push_back("fertility");
    }
    if ((preview_mask & 8) != 0)
    {
        support_lines.push_back("recovery");
    }

    const String support_text = support_lines.is_empty() ? String("None") : String(", ").join(support_lines);
    return vformat(
        "State: %s  Support Package: %d  Active Preview: %s",
        regional_site_state_name(static_cast<int>(site.site_state)),
        static_cast<int>(site.support_package_id),
        support_text);
}

String Gs1GodotMainScreenControl::regional_site_state_name(int site_state) const
{
    switch (site_state)
    {
    case 1:
        return "Available";
    case 2:
        return "Completed";
    default:
        return "Locked";
    }
}

String Gs1GodotMainScreenControl::regional_selection_action_label(const String& text, const Dictionary& action) const
{
    const int action_type = as_int(action.get("type", 0), 0);
    if (action_type == UI_ACTION_START_SITE_ATTEMPT)
    {
        return vformat("Deploy To Site %d", as_int(action.get("target_id", 0), 0));
    }
    if (action_type == UI_ACTION_CLEAR_DEPLOYMENT_SITE_SELECTION)
    {
        return "Clear Selection";
    }
    return text.is_empty() ? String("Action") : text;
}

bool Gs1GodotMainScreenControl::is_compact_tech_tree_node_text(const String& text) const
{
    return text.begins_with("TECHNODE|");
}

String Gs1GodotMainScreenControl::tech_tree_status_text(const String& text) const
{
    if (!is_compact_tech_tree_node_text(text))
    {
        return text;
    }

    const PackedStringArray parts = text.split("|");
    for (int64_t index = 1; index < parts.size(); ++index)
    {
        const String part = String(parts[index]).strip_edges();
        if (!part.begins_with("s="))
        {
            continue;
        }

        String status = part.substr(2).strip_edges();
        if (status.ends_with("r"))
        {
            status = vformat("%s rep", status.substr(0, status.length() - 1));
        }
        return status;
    }

    return "Locked";
}

String Gs1GodotMainScreenControl::regional_tech_tree_node_text(const Dictionary& action, const String& text) const
{
    const auto* node_def = find_tech_tree_node_def(action);
    const String status = tech_tree_status_text(text);
    if (node_def == nullptr)
    {
        return status.is_empty() ? String("Technology") : status;
    }

    const auto* faction_def = gs1::find_faction_def(node_def->faction_id);
    const String faction_name =
        faction_def == nullptr ? String("Faction") : string_from_view(faction_def->display_name);
    const String display_name =
        node_def->display_name.empty() ? String("Technology") : string_from_view(node_def->display_name);
    return vformat(
        "Tier %d | %s | %s | %s",
        static_cast<int>(node_def->tier_index),
        faction_name,
        display_name,
        status);
}

String Gs1GodotMainScreenControl::regional_unlockable_tooltip_text(const String& text) const
{
    const String trimmed = text.strip_edges();
    if (trimmed == "Checkerboard | Wormwood")
    {
        return "Starter Plants\nBasic Straw Checkerboard and Ordos Wormwood start available at campaign opening.";
    }

    const String title = trimmed_unlockable_title(trimmed);
    if (const auto* unlock_def = find_unlockable_def_by_title(title))
    {
        const String description = unlock_def->description.empty()
            ? String("No authored description yet.")
            : string_from_view(unlock_def->description);
        return vformat("%s\n%s\nNeed total reputation %d", title, description, unlock_def->reputation_requirement);
    }

    return trimmed;
}

String Gs1GodotMainScreenControl::regional_tech_tooltip_text(const Dictionary& action, const String& text) const
{
    const auto* node_def = find_tech_tree_node_def(action);
    if (node_def == nullptr)
    {
        return regional_tech_tree_node_text(action, text);
    }

    const auto* faction_def = gs1::find_faction_def(node_def->faction_id);
    const String faction_name = faction_def == nullptr ? String("Faction") : string_from_view(faction_def->display_name);
    const String title = node_def->display_name.empty() ? String("Technology") : string_from_view(node_def->display_name);
    const String description = node_def->description.empty() ? String("No authored description yet.") : string_from_view(node_def->description);
    return vformat(
        "%s\n%s Tier %d\n%s",
        title,
        faction_name,
        static_cast<int>(node_def->tier_index),
        description);
}

String Gs1GodotMainScreenControl::regional_card_icon_text(const Dictionary& spec) const
{
    const String source_text = String(spec.get("text", "")).strip_edges();
    const Dictionary action = spec.get("action", Dictionary());
    const auto* node_def = find_tech_tree_node_def(action);
    if (node_def != nullptr)
    {
        switch (node_def->faction_id.value)
        {
        case gs1::k_faction_village_committee:
            return "VIL";
        case gs1::k_faction_forestry_bureau:
            return "FOR";
        case gs1::k_faction_agricultural_university:
            return "UNI";
        default:
            return "TEC";
        }
    }

    const String trimmed = source_text.is_empty() ? String(spec.get("card_text", "")).strip_edges() : source_text;
    if (trimmed == "Checkerboard | Wormwood")
    {
        return "PLT";
    }

    const int open_paren = trimmed.find("(");
    const int close_paren = trimmed.find(")");
    const String kind = (open_paren >= 0 && close_paren > open_paren)
        ? trimmed.substr(open_paren + 1, close_paren - open_paren - 1).strip_edges()
        : String();
    if (kind.find("Plant") >= 0)
    {
        return "PLT";
    }
    if (kind.find("Item") >= 0)
    {
        return "ITM";
    }
    if (kind.find("Recipe") >= 0)
    {
        return "RCP";
    }
    if (kind.find("Device") >= 0 || kind.find("Structure") >= 0)
    {
        return "DEV";
    }
    return "UNL";
}

String Gs1GodotMainScreenControl::regional_card_title_text(const Dictionary& spec) const
{
    const String source_text = String(spec.get("text", "")).strip_edges();
    const Dictionary action = spec.get("action", Dictionary());
    if (const auto* node_def = find_tech_tree_node_def(action))
    {
        return node_def->display_name.empty() ? String("Technology") : string_from_view(node_def->display_name);
    }

    if (source_text == "Checkerboard | Wormwood")
    {
        return "Starter Plants";
    }

    const int open_paren = source_text.find("(");
    const int need_index = source_text.find(" Need ");
    return (open_paren > 0 ? source_text.substr(0, open_paren)
                           : (need_index > 0 ? source_text.substr(0, need_index) : source_text))
        .strip_edges();
}

String Gs1GodotMainScreenControl::regional_card_subtitle_text(const Dictionary& spec) const
{
    const String source_text = String(spec.get("text", "")).strip_edges();
    const Dictionary action = spec.get("action", Dictionary());
    if (const auto* node_def = find_tech_tree_node_def(action))
    {
        const auto* faction_def = gs1::find_faction_def(node_def->faction_id);
        const String faction_name = faction_def == nullptr ? String("Faction") : string_from_view(faction_def->display_name);
        return vformat("Tier %d | %s", static_cast<int>(node_def->tier_index), faction_name);
    }

    if (source_text == "Checkerboard | Wormwood")
    {
        return "Checkerboard / Wormwood";
    }

    const int open_paren = source_text.find("(");
    const int close_paren = source_text.find(")");
    if (open_paren >= 0 && close_paren > open_paren)
    {
        return source_text.substr(open_paren + 1, close_paren - open_paren - 1).strip_edges();
    }

    return String("Unlockable");
}

String Gs1GodotMainScreenControl::regional_card_status_text(const Dictionary& spec) const
{
    const int flags = as_int(spec.get("flags", 0), 0);
    if ((flags & 2) != 0)
    {
        return "Locked";
    }

    const Dictionary action = spec.get("action", Dictionary());
    if (find_tech_tree_node_def(action) != nullptr)
    {
        return tech_tree_status_text(String(spec.get("text", "")));
    }

    return "Available";
}

Color Gs1GodotMainScreenControl::regional_card_status_color(const Dictionary& spec) const
{
    const int flags = as_int(spec.get("flags", 0), 0);
    if ((flags & 2) != 0)
    {
        return Color(0.87, 0.72, 0.48, 0.92);
    }

    const Dictionary action = spec.get("action", Dictionary());
    if (find_tech_tree_node_def(action) != nullptr)
    {
        return Color(0.49, 0.86, 0.53, 0.95);
    }

    return Color(0.73, 0.88, 0.92, 0.95);
}

Color Gs1GodotMainScreenControl::regional_card_icon_background_color(const String& icon_text) const
{
    if (icon_text == "VIL")
    {
        return Color(0.56f, 0.68f, 0.35f, 1.0f);
    }
    if (icon_text == "FOR")
    {
        return Color(0.32f, 0.62f, 0.46f, 1.0f);
    }
    if (icon_text == "UNI")
    {
        return Color(0.34f, 0.49f, 0.74f, 1.0f);
    }
    if (icon_text == "PLT")
    {
        return Color(0.46f, 0.63f, 0.33f, 1.0f);
    }
    if (icon_text == "ITM")
    {
        return Color(0.72f, 0.53f, 0.31f, 1.0f);
    }
    if (icon_text == "RCP")
    {
        return Color(0.76f, 0.44f, 0.28f, 1.0f);
    }
    if (icon_text == "DEV")
    {
        return Color(0.48f, 0.56f, 0.68f, 1.0f);
    }
    return Color(0.58f, 0.50f, 0.34f, 1.0f);
}

Ref<Texture2D> Gs1GodotMainScreenControl::regional_card_icon_texture(const Dictionary& spec) const
{
    if (const auto* node_def = find_tech_tree_node_def(spec.get("action", Dictionary())))
    {
        const String icon_path =
            GodotProgressionResourceDatabase::instance().technology_icon_path(node_def->tech_node_id.value);
        if (ResourceLoader* loader = ResourceLoader::get_singleton())
        {
            const Ref<Resource> resource = loader->load(icon_path);
            const Ref<Texture2D> texture = resource;
            if (texture.is_valid())
            {
                return texture;
            }
        }
    }
    else
    {
        const String title = trimmed_unlockable_title(String(spec.get("text", "")));
        if (!title.is_empty())
        {
            if (const auto* unlock_def = find_unlockable_def_by_title(title))
            {
                const String icon_path = GodotProgressionResourceDatabase::instance().unlockable_icon_path(
                    unlockable_content_kind_from_def(*unlock_def),
                    unlock_def->content_id);
                if (ResourceLoader* loader = ResourceLoader::get_singleton())
                {
                    const Ref<Resource> resource = loader->load(icon_path);
                    const Ref<Texture2D> texture = resource;
                    if (texture.is_valid())
                    {
                        return texture;
                    }
                }
            }
        }
    }

    const String icon_text = regional_card_icon_text(spec);
    return make_solid_icon_texture(regional_card_icon_background_color(icon_text));
}

void Gs1GodotMainScreenControl::ensure_card_content_nodes(Button* button, ProjectedButtonRecord& record)
{
    if (button == nullptr)
    {
        return;
    }

    MarginContainer* content_root = resolve_object<MarginContainer>(record.content_root_id);
    if (content_root == nullptr)
    {
        content_root = memnew(MarginContainer);
        content_root->set_name("CardContentRoot");
        content_root->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
        content_root->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
        content_root->add_theme_constant_override("margin_left", 10);
        content_root->add_theme_constant_override("margin_top", 10);
        content_root->add_theme_constant_override("margin_right", 10);
        content_root->add_theme_constant_override("margin_bottom", 10);
        button->add_child(content_root);
        record.content_root_id = content_root->get_instance_id();
    }

    VBoxContainer* content_vbox = Object::cast_to<VBoxContainer>(content_root->get_node_or_null(NodePath("CardVBox")));
    if (content_vbox == nullptr)
    {
        content_vbox = memnew(VBoxContainer);
        content_vbox->set_name("CardVBox");
        content_vbox->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
        content_vbox->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
        content_vbox->add_theme_constant_override("separation", 6);
        content_root->add_child(content_vbox);
    }

    MarginContainer* icon_frame = Object::cast_to<MarginContainer>(content_root->get_node_or_null(NodePath("CardVBox/IconFrame")));
    if (icon_frame == nullptr)
    {
        icon_frame = memnew(MarginContainer);
        icon_frame->set_name("IconFrame");
        icon_frame->set_custom_minimum_size(Vector2(0, 56));
        icon_frame->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
        content_vbox->add_child(icon_frame);
    }

    TextureRect* icon_texture = resolve_object<TextureRect>(record.icon_texture_id);
    if (icon_texture == nullptr)
    {
        icon_texture = memnew(TextureRect);
        icon_texture->set_name("CardIconTexture");
        icon_texture->set_custom_minimum_size(Vector2(48, 48));
        icon_texture->set_h_size_flags(Control::SIZE_SHRINK_CENTER);
        icon_texture->set_stretch_mode(TextureRect::STRETCH_KEEP_ASPECT_CENTERED);
        icon_texture->set_expand_mode(TextureRect::EXPAND_IGNORE_SIZE);
        icon_texture->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
        icon_frame->add_child(icon_texture);
        record.icon_texture_id = icon_texture->get_instance_id();
    }

    Label* icon_label = resolve_object<Label>(record.icon_label_id);
    if (icon_label == nullptr)
    {
        icon_label = memnew(Label);
        icon_label->set_name("CardIconLabel");
        icon_label->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
        icon_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
        icon_label->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
        icon_label->add_theme_font_size_override("font_size", 24);
        icon_label->add_theme_color_override("font_color", Color(0.95, 0.92, 0.84, 0.98));
        icon_label->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
        icon_frame->add_child(icon_label);
        record.icon_label_id = icon_label->get_instance_id();
    }

    Label* title_label = resolve_object<Label>(record.title_label_id);
    if (title_label == nullptr)
    {
        title_label = memnew(Label);
        title_label->set_name("CardTitleLabel");
        title_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
        title_label->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
        title_label->add_theme_font_size_override("font_size", 14);
        title_label->add_theme_color_override("font_color", Color(0.97, 0.93, 0.83, 0.98));
        title_label->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
        content_vbox->add_child(title_label);
        record.title_label_id = title_label->get_instance_id();
    }

    Label* subtitle_label = resolve_object<Label>(record.subtitle_label_id);
    if (subtitle_label == nullptr)
    {
        subtitle_label = memnew(Label);
        subtitle_label->set_name("CardSubtitleLabel");
        subtitle_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
        subtitle_label->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
        subtitle_label->add_theme_font_size_override("font_size", 12);
        subtitle_label->add_theme_color_override("font_color", Color(0.81, 0.78, 0.70, 0.92));
        subtitle_label->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
        content_vbox->add_child(subtitle_label);
        record.subtitle_label_id = subtitle_label->get_instance_id();
    }

    Label* status_label = resolve_object<Label>(record.status_label_id);
    if (status_label == nullptr)
    {
        status_label = memnew(Label);
        status_label->set_name("CardStatusLabel");
        status_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
        status_label->add_theme_font_size_override("font_size", 13);
        status_label->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
        content_vbox->add_child(status_label);
        record.status_label_id = status_label->get_instance_id();
    }

    ColorRect* lock_overlay = resolve_object<ColorRect>(record.lock_overlay_id);
    if (lock_overlay == nullptr)
    {
        lock_overlay = memnew(ColorRect);
        lock_overlay->set_name("CardLockOverlay");
        lock_overlay->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
        lock_overlay->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
        lock_overlay->set_color(Color(0.16, 0.11, 0.07, 0.38));
        button->add_child(lock_overlay);
        record.lock_overlay_id = lock_overlay->get_instance_id();
    }

    Label* lock_label = resolve_object<Label>(record.lock_label_id);
    if (lock_label == nullptr)
    {
        lock_label = memnew(Label);
        lock_label->set_name("CardLockLabel");
        lock_label->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
        lock_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
        lock_label->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
        lock_label->add_theme_font_size_override("font_size", 28);
        lock_label->add_theme_color_override("font_color", Color(1.0, 1.0, 1.0, 0.7));
        lock_label->set_text("LOCK");
        lock_label->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
        button->add_child(lock_label);
        record.lock_label_id = lock_label->get_instance_id();
    }
}

Dictionary Gs1GodotMainScreenControl::active_site() const
{
    return projection_.get("active_site", Dictionary());
}

Dictionary Gs1GodotMainScreenControl::tile_at(const Vector2i& tile_coord) const
{
    const Dictionary site_state = active_site();
    const int width = as_int(site_state.get("width", 0), 0);
    const int height = as_int(site_state.get("height", 0), 0);
    const Array tiles = site_state.get("tiles", Array());
    if (width <= 0 || height <= 0)
    {
        return Dictionary();
    }
    if (tile_coord.x < 0 || tile_coord.y < 0 || tile_coord.x >= width || tile_coord.y >= height)
    {
        return Dictionary();
    }

    const int index = tile_coord.y * width + tile_coord.x;
    if (index < 0 || index >= tiles.size())
    {
        return Dictionary();
    }
    return tiles[index];
}

int Gs1GodotMainScreenControl::find_worker_pack_storage_id() const
{
    const Array inventory_storages = active_site().get("inventory_storages", Array());
    for (int64_t index = 0; index < inventory_storages.size(); ++index)
    {
        const Dictionary storage = inventory_storages[index];
        if (as_int(storage.get("container_kind", -1), -1) == CONTAINER_WORKER_PACK)
        {
            return as_int(storage.get("storage_id", 0), 0);
        }
    }
    return 0;
}

int Gs1GodotMainScreenControl::find_selected_tile_storage_id()
{
    const Array inventory_storages = active_site().get("inventory_storages", Array());
    for (int64_t index = 0; index < inventory_storages.size(); ++index)
    {
        const Dictionary storage = inventory_storages[index];
        const int tile_x = as_int(storage.get("tile_x", 0), 0);
        const int tile_y = as_int(storage.get("tile_y", 0), 0);
        const int storage_id = as_int(storage.get("storage_id", 0), 0);
        cached_storage_lookup_[tile_y * 100000 + tile_x] = storage_id;
        if (tile_x == selected_tile_.x && tile_y == selected_tile_.y)
        {
            return storage_id;
        }
    }
    return 0;
}

void Gs1GodotMainScreenControl::use_first_usable_item()
{
    const Array worker_pack_slots = active_site().get("worker_pack_slots", Array());
    for (int64_t index = 0; index < worker_pack_slots.size(); ++index)
    {
        const Dictionary slot = worker_pack_slots[index];
        const int capability_flags = as_int(slot.get("capability_flags", 0), 0);
        if (capability_flags == 0)
        {
            continue;
        }
        const int storage_id = as_int(slot.get("storage_id", 0), 0);
        const int slot_index = as_int(slot.get("slot_index", 0), 0);
        const int quantity = std::max(1, as_int(slot.get("quantity", 1), 1));
        const int packed_arg0 = CONTAINER_WORKER_PACK | (slot_index << 8) | (quantity << 16);
        submit_ui_action(UI_ACTION_USE_INVENTORY_ITEM, as_int(slot.get("item_instance_id", 0), 0), packed_arg0, storage_id);
        return;
    }
}

void Gs1GodotMainScreenControl::transfer_first_storage_item_to_pack()
{
    const Dictionary opened_storage = active_site().get("opened_storage", Dictionary());
    if (opened_storage.is_empty())
    {
        return;
    }

    const int destination_storage_id = find_worker_pack_storage_id();
    if (destination_storage_id == 0)
    {
        return;
    }

    const Array slots = opened_storage.get("slots", Array());
    for (int64_t index = 0; index < slots.size(); ++index)
    {
        const Dictionary slot = slots[index];
        const std::uint32_t source_storage_id = static_cast<std::uint32_t>(as_int(slot.get("storage_id", 0), 0));
        const int source_slot_index = as_int(slot.get("slot_index", 0), 0);
        const int quantity = std::max(1, as_int(slot.get("quantity", 1), 1));
        const int packed_arg0 = CONTAINER_DEVICE_STORAGE | (source_slot_index << 8) | (CONTAINER_WORKER_PACK << 16) | (quantity << 24);
        const std::uint64_t packed_arg1 = static_cast<std::uint64_t>(source_storage_id)
            | (static_cast<std::uint64_t>(static_cast<std::uint32_t>(destination_storage_id)) << 32U);
        submit_ui_action(UI_ACTION_TRANSFER_INVENTORY_ITEM, 0, packed_arg0, packed_arg1);
        return;
    }
}

void Gs1GodotMainScreenControl::plant_first_seed_on_selected_tile()
{
    const Array worker_pack_slots = active_site().get("worker_pack_slots", Array());
    for (int64_t index = 0; index < worker_pack_slots.size(); ++index)
    {
        const Dictionary slot = worker_pack_slots[index];
        if (as_int(slot.get("linked_plant_id", 0), 0) == 0)
        {
            continue;
        }

        submit_site_action_request(
            SITE_ACTION_PLANT,
            SITE_ACTION_REQUEST_FLAG_HAS_ITEM,
            1,
            selected_tile_.x,
            selected_tile_.y,
            0,
            0,
            as_int(slot.get("item_id", 0), 0));
        return;
    }
}

void Gs1GodotMainScreenControl::submit_ui_action(std::int64_t action_type, std::int64_t target_id, std::int64_t arg0, std::int64_t arg1)
{
    if (runtime_node_ == nullptr)
    {
        return;
    }
    const bool ok = runtime_node_->submit_ui_action(action_type, target_id, arg0, arg1);
    last_action_message_ = vformat("UI action %d (%s)", action_type, ok ? "ok" : "failed");
}

void Gs1GodotMainScreenControl::submit_move(double x, double y, double z)
{
    if (runtime_node_ == nullptr)
    {
        return;
    }
    const bool ok = runtime_node_->submit_move_direction(x, y, z);
    last_action_message_ = vformat("Move (%s)", ok ? "ok" : "failed");
}

void Gs1GodotMainScreenControl::submit_site_context_request(int tile_x, int tile_y, int flags)
{
    if (runtime_node_ == nullptr)
    {
        return;
    }
    const bool ok = runtime_node_->submit_site_context_request(tile_x, tile_y, flags);
    last_action_message_ = vformat("Context (%d, %d) %s", tile_x, tile_y, ok ? "ok" : "failed");
}

void Gs1GodotMainScreenControl::submit_site_action_request(
    int action_kind,
    int flags,
    int quantity,
    int tile_x,
    int tile_y,
    int primary_subject_id,
    int secondary_subject_id,
    int item_id)
{
    if (runtime_node_ == nullptr)
    {
        return;
    }
    const bool ok = runtime_node_->submit_site_action_request(
        action_kind,
        flags,
        quantity,
        tile_x,
        tile_y,
        primary_subject_id,
        secondary_subject_id,
        item_id);
    last_action_message_ = vformat("Site action %d (%s)", action_kind, ok ? "ok" : "failed");
}

void Gs1GodotMainScreenControl::submit_site_action_cancel(int action_id, int flags)
{
    if (runtime_node_ == nullptr)
    {
        return;
    }
    const bool ok = runtime_node_->submit_site_action_cancel(action_id, flags);
    last_action_message_ = vformat("Cancel action (%s)", ok ? "ok" : "failed");
}

void Gs1GodotMainScreenControl::submit_storage_view(int storage_id, int event_kind)
{
    if (runtime_node_ == nullptr)
    {
        return;
    }
    const bool ok = runtime_node_->submit_site_storage_view(storage_id, event_kind);
    last_action_message_ = vformat("Storage %d (%s)", storage_id, ok ? "ok" : "failed");
}

void Gs1GodotMainScreenControl::bind_button(BaseButton* button, const Callable& callback)
{
    if (button != nullptr)
    {
        button->connect("pressed", callback);
    }
}

void Gs1GodotMainScreenControl::clamp_selected_tile()
{
    const Dictionary site_state = active_site();
    const int width = std::max(1, as_int(site_state.get("width", 1), 1));
    const int height = std::max(1, as_int(site_state.get("height", 1), 1));
    selected_tile_.x = std::clamp(selected_tile_.x, 0, width - 1);
    selected_tile_.y = std::clamp(selected_tile_.y, 0, height - 1);
}

bool Gs1GodotMainScreenControl::regional_map_ui_contains_screen_point(const Vector2& screen_position) const
{
    const auto contains_screen_point = [&screen_position](const Control* control) -> bool
    {
        return control != nullptr &&
            control->is_visible_in_tree() &&
            control->get_mouse_filter() != Control::MOUSE_FILTER_IGNORE &&
            control->get_global_rect().has_point(screen_position);
    };

    return contains_screen_point(menu_panel_) ||
        contains_screen_point(regional_map_panel_) ||
        contains_screen_point(regional_selection_panel_) ||
        contains_screen_point(regional_tech_tree_overlay_) ||
        contains_screen_point(site_panel_) ||
        contains_screen_point(inventory_panel_) ||
        contains_screen_point(task_panel_) ||
        contains_screen_point(phone_panel_) ||
        contains_screen_point(craft_panel_) ||
        contains_screen_point(overlay_panel_);
}

bool Gs1GodotMainScreenControl::try_select_regional_site_from_screen(const Vector2& screen_position)
{
    if (regional_map_world_ == nullptr || !regional_map_world_->is_visible() || regional_camera_ == nullptr)
    {
        return false;
    }
    if (regional_map_panel_ == nullptr || !regional_map_panel_->is_visible())
    {
        return false;
    }

    const Vector3 origin = regional_camera_->project_ray_origin(screen_position);
    const Vector3 normal = regional_camera_->project_ray_normal(screen_position);
    if (Math::abs(normal.y) < 0.0001)
    {
        return false;
    }

    const double ground_distance = -origin.y / normal.y;
    if (ground_distance <= 0.0 || ground_distance > REGIONAL_PICK_DISTANCE)
    {
        return false;
    }

    const Vector3 hit_position = origin + normal * ground_distance;
    int nearest_site_id = 0;
    double nearest_distance = REGIONAL_TILE_SIZE * 0.75;

    for (const auto& [site_id, record] : regional_site_nodes_)
    {
        Node3D* root = resolve_object<Node3D>(record.root_id);
        if (root == nullptr)
        {
            continue;
        }
        const Vector2 root_planar(root->get_global_position().x, root->get_global_position().z);
        const Vector2 hit_planar(hit_position.x, hit_position.z);
        const double planar_distance = root_planar.distance_to(hit_planar);
        if (planar_distance < nearest_distance)
        {
            nearest_distance = planar_distance;
            nearest_site_id = site_id;
        }
    }

    if (nearest_site_id == 0)
    {
        return false;
    }

    select_regional_site(nearest_site_id, true);
    return true;
}

int Gs1GodotMainScreenControl::as_int(const Variant& value, int fallback) const
{
    if (value.get_type() == Variant::NIL)
    {
        return fallback;
    }
    switch (value.get_type())
    {
    case Variant::INT:
        return static_cast<int>(int64_t(value));
    case Variant::FLOAT:
        return static_cast<int>(Math::round(double(value)));
    case Variant::BOOL:
        return bool(value) ? 1 : 0;
    case Variant::STRING:
    {
        const String text = value;
        if (text.is_valid_int())
        {
            return static_cast<int>(text.to_int());
        }
        if (text.is_valid_float())
        {
            return static_cast<int>(Math::round(text.to_float()));
        }
        return fallback;
    }
    default:
        return fallback;
    }
}

double Gs1GodotMainScreenControl::as_float(const Variant& value, double fallback) const
{
    if (value.get_type() == Variant::NIL)
    {
        return fallback;
    }
    switch (value.get_type())
    {
    case Variant::FLOAT:
        return double(value);
    case Variant::INT:
        return static_cast<double>(int64_t(value));
    case Variant::BOOL:
        return bool(value) ? 1.0 : 0.0;
    case Variant::STRING:
    {
        const String text = value;
        return text.is_valid_float() ? text.to_float() : fallback;
    }
    default:
        return fallback;
    }
}

bool Gs1GodotMainScreenControl::as_bool(const Variant& value, bool fallback) const
{
    if (value.get_type() == Variant::NIL)
    {
        return fallback;
    }
    switch (value.get_type())
    {
    case Variant::BOOL:
        return bool(value);
    case Variant::INT:
        return int64_t(value) != 0;
    case Variant::FLOAT:
        return Math::abs(double(value)) > 0.0001;
    case Variant::STRING:
    {
        const String text = String(value).to_lower();
        if (text == "true" || text == "1")
        {
            return true;
        }
        if (text == "false" || text == "0")
        {
            return false;
        }
        return fallback;
    }
    default:
        return fallback;
    }
}

String Gs1GodotMainScreenControl::app_state_name(int app_state) const
{
    switch (app_state)
    {
    case APP_STATE_BOOT:
        return "Boot";
    case APP_STATE_MAIN_MENU:
        return "Main Menu";
    case APP_STATE_CAMPAIGN_SETUP:
        return "Campaign Setup";
    case APP_STATE_REGIONAL_MAP:
        return "Regional Map";
    case APP_STATE_SITE_LOADING:
        return "Site Loading";
    case APP_STATE_SITE_ACTIVE:
        return "Site Active";
    case APP_STATE_SITE_PAUSED:
        return "Site Paused";
    case APP_STATE_SITE_RESULT:
        return "Site Result";
    case APP_STATE_CAMPAIGN_END:
        return "Campaign End";
    default:
        return "Unknown";
    }
}

void Gs1GodotMainScreenControl::set_runtime_node_path(const NodePath& path) { runtime_node_path_ = path; runtime_node_ = nullptr; }
NodePath Gs1GodotMainScreenControl::get_runtime_node_path() const { return runtime_node_path_; }
void Gs1GodotMainScreenControl::set_site_view_path(const NodePath& path) { site_view_path_ = path; site_view_ = nullptr; }
NodePath Gs1GodotMainScreenControl::get_site_view_path() const { return site_view_path_; }
void Gs1GodotMainScreenControl::set_regional_world_path(const NodePath& path) { regional_world_path_ = path; regional_map_world_ = nullptr; }
NodePath Gs1GodotMainScreenControl::get_regional_world_path() const { return regional_world_path_; }
void Gs1GodotMainScreenControl::set_regional_camera_path(const NodePath& path) { regional_camera_path_ = path; regional_camera_ = nullptr; }
NodePath Gs1GodotMainScreenControl::get_regional_camera_path() const { return regional_camera_path_; }
void Gs1GodotMainScreenControl::set_regional_terrain_root_path(const NodePath& path) { regional_terrain_root_path_ = path; regional_terrain_root_ = nullptr; }
NodePath Gs1GodotMainScreenControl::get_regional_terrain_root_path() const { return regional_terrain_root_path_; }
void Gs1GodotMainScreenControl::set_regional_link_root_path(const NodePath& path) { regional_link_root_path_ = path; regional_link_root_ = nullptr; }
NodePath Gs1GodotMainScreenControl::get_regional_link_root_path() const { return regional_link_root_path_; }
void Gs1GodotMainScreenControl::set_regional_site_root_path(const NodePath& path) { regional_site_root_path_ = path; regional_site_root_ = nullptr; }
NodePath Gs1GodotMainScreenControl::get_regional_site_root_path() const { return regional_site_root_path_; }

void Gs1GodotMainScreenControl::on_start_campaign_pressed() { submit_ui_action(UI_ACTION_START_NEW_CAMPAIGN); }
void Gs1GodotMainScreenControl::on_continue_campaign_pressed() { submit_ui_action(UI_ACTION_START_NEW_CAMPAIGN); }
void Gs1GodotMainScreenControl::on_menu_settings_pressed() { last_action_message_ = "Settings are not wired yet in the prototype"; }
void Gs1GodotMainScreenControl::on_quit_pressed() { if (SceneTree* tree = get_tree()) { tree->quit(); } }
void Gs1GodotMainScreenControl::on_return_to_map_pressed() { submit_ui_action(UI_ACTION_RETURN_TO_REGIONAL_MAP); }
void Gs1GodotMainScreenControl::on_move_north_pressed() { submit_move(0.0, 0.0, -1.0); }
void Gs1GodotMainScreenControl::on_move_south_pressed() { submit_move(0.0, 0.0, 1.0); }
void Gs1GodotMainScreenControl::on_move_west_pressed() { submit_move(-1.0, 0.0, 0.0); }
void Gs1GodotMainScreenControl::on_move_east_pressed() { submit_move(1.0, 0.0, 0.0); }
void Gs1GodotMainScreenControl::on_hover_tile_pressed() { submit_site_context_request(selected_tile_.x, selected_tile_.y, 0); }
void Gs1GodotMainScreenControl::on_cancel_action_pressed() { submit_site_action_cancel(0, SITE_ACTION_CANCEL_FLAG_CURRENT_ACTION | SITE_ACTION_CANCEL_FLAG_PLACEMENT_MODE); }
void Gs1GodotMainScreenControl::on_open_protection_pressed() { submit_ui_action(UI_ACTION_OPEN_SITE_PROTECTION_SELECTOR); }
void Gs1GodotMainScreenControl::on_overlay_wind_pressed() { submit_ui_action(UI_ACTION_SET_SITE_PROTECTION_OVERLAY_MODE, 0, PROTECTION_OVERLAY_WIND); }
void Gs1GodotMainScreenControl::on_overlay_heat_pressed() { submit_ui_action(UI_ACTION_SET_SITE_PROTECTION_OVERLAY_MODE, 0, PROTECTION_OVERLAY_HEAT); }
void Gs1GodotMainScreenControl::on_overlay_dust_pressed() { submit_ui_action(UI_ACTION_SET_SITE_PROTECTION_OVERLAY_MODE, 0, PROTECTION_OVERLAY_DUST); }
void Gs1GodotMainScreenControl::on_overlay_condition_pressed() { submit_ui_action(UI_ACTION_SET_SITE_PROTECTION_OVERLAY_MODE, 0, PROTECTION_OVERLAY_OCCUPANT_CONDITION); }
void Gs1GodotMainScreenControl::on_overlay_clear_pressed() { submit_ui_action(UI_ACTION_SET_SITE_PROTECTION_OVERLAY_MODE, 0, PROTECTION_OVERLAY_NONE); }
void Gs1GodotMainScreenControl::on_open_phone_home_pressed() { submit_ui_action(UI_ACTION_SET_PHONE_PANEL_SECTION, 0, PHONE_PANEL_SECTION_HOME); }
void Gs1GodotMainScreenControl::on_open_phone_tasks_pressed() { submit_ui_action(UI_ACTION_SET_PHONE_PANEL_SECTION, 0, PHONE_PANEL_SECTION_TASKS); }
void Gs1GodotMainScreenControl::on_open_phone_buy_pressed() { submit_ui_action(UI_ACTION_SET_PHONE_PANEL_SECTION, 0, PHONE_PANEL_SECTION_BUY); }
void Gs1GodotMainScreenControl::on_open_phone_sell_pressed() { submit_ui_action(UI_ACTION_SET_PHONE_PANEL_SECTION, 0, PHONE_PANEL_SECTION_SELL); }
void Gs1GodotMainScreenControl::on_open_tech_tree_pressed() { toggle_regional_tech_tree(); }
void Gs1GodotMainScreenControl::on_close_phone_pressed() { submit_ui_action(UI_ACTION_CLOSE_PHONE_PANEL); }
void Gs1GodotMainScreenControl::on_open_worker_pack_pressed() { const int id = find_worker_pack_storage_id(); if (id != 0) { submit_storage_view(id, INVENTORY_VIEW_EVENT_OPEN_SNAPSHOT); } }
void Gs1GodotMainScreenControl::on_open_nearest_storage_pressed() { const int id = find_selected_tile_storage_id(); if (id != 0) { submit_storage_view(id, INVENTORY_VIEW_EVENT_OPEN_SNAPSHOT); } }
void Gs1GodotMainScreenControl::on_close_storage_pressed() { const Dictionary opened_storage = active_site().get("opened_storage", Dictionary()); if (!opened_storage.is_empty()) { submit_storage_view(as_int(opened_storage.get("storage_id", 0), 0), INVENTORY_VIEW_EVENT_CLOSE); } }
void Gs1GodotMainScreenControl::on_use_selected_item_pressed() { use_first_usable_item(); }
void Gs1GodotMainScreenControl::on_transfer_selected_item_pressed() { transfer_first_storage_item_to_pack(); }
void Gs1GodotMainScreenControl::on_plant_selected_seed_pressed() { plant_first_seed_on_selected_tile(); }
void Gs1GodotMainScreenControl::on_craft_at_selected_tile_pressed() { submit_site_context_request(selected_tile_.x, selected_tile_.y, 0); }
void Gs1GodotMainScreenControl::on_dynamic_regional_site_pressed(int site_id) { select_regional_site(site_id, true); }
void Gs1GodotMainScreenControl::on_dynamic_phone_listing_pressed(int listing_id) { submit_ui_action(UI_ACTION_BUY_PHONE_LISTING, listing_id, 1, 0); }
void Gs1GodotMainScreenControl::on_dynamic_craft_option_pressed(std::int64_t button_key)
{
    auto found = craft_option_buttons_.find(static_cast<std::uint64_t>(button_key));
    if (found == craft_option_buttons_.end())
    {
        return;
    }

    Button* button = resolve_object<Button>(found->second.object_id);
    if (button == nullptr)
    {
        return;
    }

    submit_site_action_request(
        SITE_ACTION_CRAFT,
        SITE_ACTION_REQUEST_FLAG_HAS_ITEM,
        1,
        as_int(button->get_meta("tile_x", 0), 0),
        as_int(button->get_meta("tile_y", 0), 0),
        0,
        0,
        as_int(button->get_meta("output_item_id", 0), 0));
}

void Gs1GodotMainScreenControl::on_dynamic_projected_action_pressed(int container_kind, std::int64_t button_key)
{
    std::unordered_map<std::uint64_t, ProjectedButtonRecord>* registry = nullptr;
    switch (container_kind)
    {
    case PROJECTED_ACTION_CONTAINER_SITE_CONTROLS:
        registry = &site_control_buttons_;
        break;
    case PROJECTED_ACTION_CONTAINER_REGIONAL_SELECTION:
        registry = &regional_selection_action_buttons_;
        break;
    case PROJECTED_ACTION_CONTAINER_REGIONAL_TECH_TREE:
        registry = &regional_tech_tree_action_buttons_;
        if (regional_tech_tree_action_buttons_.find(static_cast<std::uint64_t>(button_key)) ==
            regional_tech_tree_action_buttons_.end())
        {
            registry = &regional_tech_tree_faction_tab_buttons_;
        }
        break;
    default:
        return;
    }

    auto found = registry->find(static_cast<std::uint64_t>(button_key));
    if (found == registry->end())
    {
        return;
    }

    Button* button = resolve_object<Button>(found->second.object_id);
    if (button == nullptr)
    {
        return;
    }

    if (!as_bool(button->get_meta("gs1_allow_action", true), true))
    {
        return;
    }

    submit_ui_action(
        as_int(button->get_meta("action_type", 0), 0),
        as_int(button->get_meta("target_id", 0), 0),
        as_int(button->get_meta("arg0", 0), 0),
        as_int(button->get_meta("arg1", 0), 0));
}

void Gs1GodotMainScreenControl::on_fixed_slot_pressed(std::int64_t button_id)
{
    BaseButton* button = resolve_object<BaseButton>(static_cast<ObjectID>(button_id));
    if (button == nullptr || !as_bool(button->get_meta("gs1_has_action", false), false))
    {
        return;
    }

    submit_ui_action(
        as_int(button->get_meta("gs1_action_type", 0), 0),
        as_int(button->get_meta("gs1_target_id", 0), 0),
        as_int(button->get_meta("gs1_arg0", 0), 0),
        as_int(button->get_meta("gs1_arg1", 0), 0));
}
