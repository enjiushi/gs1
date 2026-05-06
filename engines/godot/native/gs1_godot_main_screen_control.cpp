#include "godot_progression_resources.h"
#include "gs1_godot_main_screen_control.h"
#include "gs1_godot_runtime_node.h"

#include "content/defs/faction_defs.h"
#include "content/defs/item_defs.h"
#include "content/defs/plant_defs.h"
#include "content/defs/structure_defs.h"
#include "content/defs/task_defs.h"
#include "content/defs/technology_defs.h"
#include "host/adapter_metadata_catalog.h"

#include <godot_cpp/classes/base_button.hpp>
#include <godot_cpp/classes/base_material3d.hpp>
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/center_container.hpp>
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
#include <map>
#include <cstdint>
#include <cmath>
#include <limits>
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

    ClassDB::bind_method(D_METHOD("on_menu_settings_pressed"), &Gs1GodotMainScreenControl::on_menu_settings_pressed);
    ClassDB::bind_method(D_METHOD("on_quit_pressed"), &Gs1GodotMainScreenControl::on_quit_pressed);
    ClassDB::bind_method(D_METHOD("on_open_worker_pack_pressed"), &Gs1GodotMainScreenControl::on_open_worker_pack_pressed);
    ClassDB::bind_method(D_METHOD("on_open_nearest_storage_pressed"), &Gs1GodotMainScreenControl::on_open_nearest_storage_pressed);
    ClassDB::bind_method(D_METHOD("on_close_storage_pressed"), &Gs1GodotMainScreenControl::on_close_storage_pressed);
    ClassDB::bind_method(D_METHOD("on_use_selected_item_pressed"), &Gs1GodotMainScreenControl::on_use_selected_item_pressed);
    ClassDB::bind_method(D_METHOD("on_transfer_selected_item_pressed"), &Gs1GodotMainScreenControl::on_transfer_selected_item_pressed);
    ClassDB::bind_method(D_METHOD("on_plant_selected_seed_pressed"), &Gs1GodotMainScreenControl::on_plant_selected_seed_pressed);
    ClassDB::bind_method(D_METHOD("on_dynamic_regional_site_pressed", "site_id"), &Gs1GodotMainScreenControl::on_dynamic_regional_site_pressed);
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
    action_panel_controller_.set_submit_ui_action_callback([this](std::int64_t action_type, std::int64_t target_id, std::int64_t arg0, std::int64_t arg1) {
        submit_ui_action(action_type, target_id, arg0, arg1);
    });
    overlay_panel_controller_.set_submit_ui_action_callback([this](std::int64_t action_type, std::int64_t target_id, std::int64_t arg0, std::int64_t arg1) {
        submit_ui_action(action_type, target_id, arg0, arg1);
    });
    craft_panel_controller_.set_submit_craft_option_callback([this](int tile_x, int tile_y, int output_item_id) {
        submit_site_action_request(
            SITE_ACTION_CRAFT,
            SITE_ACTION_REQUEST_FLAG_HAS_ITEM,
            1,
            tile_x,
            tile_y,
            0,
            0,
            output_item_id);
    });
    regional_selection_panel_controller_.set_submit_ui_action_callback([this](std::int64_t action_type, std::int64_t target_id, std::int64_t arg0, std::int64_t arg1) {
        submit_ui_action(action_type, target_id, arg0, arg1);
    });
    regional_tech_tree_panel_controller_.set_submit_ui_action_callback([this](std::int64_t action_type, std::int64_t target_id, std::int64_t arg0, std::int64_t arg1) {
        submit_ui_action(action_type, target_id, arg0, arg1);
    });
    phone_panel_controller_.set_submit_ui_action_callback([this](std::int64_t action_type, std::int64_t target_id, std::int64_t arg0, std::int64_t arg1) {
        submit_ui_action(action_type, target_id, arg0, arg1);
    });
    invalidate_all_ui();
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
        status_panel_controller_.show_runtime_missing();
        return;
    }

    status_panel_controller_.set_runtime_status(true, runtime_node_->last_error());
    const int app_state = last_app_state_ >= 0 ? last_app_state_ : APP_STATE_BOOT;

    update_visibility(app_state);
    refresh_regional_map_if_needed(app_state);
    refresh_selected_tile_if_needed();
    update_background_presentation(app_state, delta);
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
        if (runtime_node_ != nullptr)
        {
            runtime_node_->subscribe_engine_messages(*this);
            runtime_node_->subscribe_engine_messages(status_panel_controller_);
            runtime_node_->subscribe_engine_messages(inventory_panel_controller_);
            runtime_node_->subscribe_engine_messages(craft_panel_controller_);
            runtime_node_->subscribe_engine_messages(action_panel_controller_);
            runtime_node_->subscribe_engine_messages(overlay_panel_controller_);
            runtime_node_->subscribe_engine_messages(phone_panel_controller_);
            runtime_node_->subscribe_engine_messages(regional_selection_panel_controller_);
            runtime_node_->subscribe_engine_messages(regional_summary_panel_controller_);
            runtime_node_->subscribe_engine_messages(regional_tech_tree_panel_controller_);
            runtime_node_->subscribe_engine_messages(site_summary_panel_controller_);
            runtime_node_->subscribe_engine_messages(task_panel_controller_);
        }
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
    status_panel_controller_.cache_ui_references(*this);
    inventory_panel_controller_.cache_ui_references(*this);
    craft_panel_controller_.cache_ui_references(*this);
    action_panel_controller_.cache_ui_references(*this);
    overlay_panel_controller_.cache_ui_references(*this);
    phone_panel_controller_.cache_ui_references(*this);
    regional_selection_panel_controller_.cache_ui_references(*this);
    regional_summary_panel_controller_.cache_ui_references(*this);
    regional_tech_tree_panel_controller_.cache_ui_references(*this);
    site_summary_panel_controller_.cache_ui_references(*this);
    task_panel_controller_.cache_ui_references(*this);
    if (menu_panel_ == nullptr)
    {
        menu_panel_ = Object::cast_to<PanelContainer>(find_child("MenuPanel", true, false));
    }
    if (regional_map_panel_ == nullptr)
    {
        regional_map_panel_ = Object::cast_to<PanelContainer>(find_child("RegionalMapPanel", true, false));
    }
    if (regional_selection_panel_ == nullptr)
    {
        regional_selection_panel_ = Object::cast_to<PanelContainer>(find_child("RegionalSelectionPanel", true, false));
    }
    if (regional_tech_tree_overlay_ == nullptr)
    {
        regional_tech_tree_overlay_ = Object::cast_to<Control>(find_child("TechOverlay", true, false));
    }
    if (site_panel_ == nullptr)
    {
        site_panel_ = Object::cast_to<PanelContainer>(find_child("SitePanel", true, false));
    }
    if (tile_label_ == nullptr)
    {
        tile_label_ = Object::cast_to<Label>(find_child("TileLabel", true, false));
    }
    if (inventory_panel_ == nullptr)
    {
        inventory_panel_ = Object::cast_to<PanelContainer>(find_child("InventoryPanel", true, false));
    }
    if (task_panel_ == nullptr)
    {
        task_panel_ = Object::cast_to<PanelContainer>(find_child("TaskPanel", true, false));
    }
    if (phone_panel_ == nullptr)
    {
        phone_panel_ = Object::cast_to<PanelContainer>(find_child("PhonePanel", true, false));
    }
    if (craft_panel_ == nullptr)
    {
        craft_panel_ = Object::cast_to<PanelContainer>(find_child("CraftPanel", true, false));
    }
    if (overlay_panel_ == nullptr)
    {
        overlay_panel_ = Object::cast_to<PanelContainer>(find_child("OverlayPanel", true, false));
    }
}

void Gs1GodotMainScreenControl::wire_static_buttons()
{
    if (buttons_wired_)
    {
        return;
    }

    bind_button(Object::cast_to<BaseButton>(find_child("StartCampaignButton", true, false)), callable_mp(this, &Gs1GodotMainScreenControl::on_submit_ui_action_pressed).bind(static_cast<std::int64_t>(UI_ACTION_START_NEW_CAMPAIGN), static_cast<std::int64_t>(0), static_cast<std::int64_t>(0), static_cast<std::int64_t>(0)));
    bind_button(Object::cast_to<BaseButton>(find_child("ContinueCampaignButton", true, false)), callable_mp(this, &Gs1GodotMainScreenControl::on_submit_ui_action_pressed).bind(static_cast<std::int64_t>(UI_ACTION_START_NEW_CAMPAIGN), static_cast<std::int64_t>(0), static_cast<std::int64_t>(0), static_cast<std::int64_t>(0)));
    bind_button(Object::cast_to<BaseButton>(find_child("MenuSettingsButton", true, false)), callable_mp(this, &Gs1GodotMainScreenControl::on_menu_settings_pressed));
    bind_button(Object::cast_to<BaseButton>(find_child("QuitButton", true, false)), callable_mp(this, &Gs1GodotMainScreenControl::on_quit_pressed));
    bind_button(Object::cast_to<BaseButton>(find_child("ReturnToMapButton", true, false)), callable_mp(this, &Gs1GodotMainScreenControl::on_submit_ui_action_pressed).bind(static_cast<std::int64_t>(UI_ACTION_RETURN_TO_REGIONAL_MAP), static_cast<std::int64_t>(0), static_cast<std::int64_t>(0), static_cast<std::int64_t>(0)));
    bind_button(Object::cast_to<BaseButton>(find_child("MoveNorthButton", true, false)), callable_mp(this, &Gs1GodotMainScreenControl::on_submit_move_pressed).bind(0.0, 0.0, -1.0));
    bind_button(Object::cast_to<BaseButton>(find_child("MoveSouthButton", true, false)), callable_mp(this, &Gs1GodotMainScreenControl::on_submit_move_pressed).bind(0.0, 0.0, 1.0));
    bind_button(Object::cast_to<BaseButton>(find_child("MoveWestButton", true, false)), callable_mp(this, &Gs1GodotMainScreenControl::on_submit_move_pressed).bind(-1.0, 0.0, 0.0));
    bind_button(Object::cast_to<BaseButton>(find_child("MoveEastButton", true, false)), callable_mp(this, &Gs1GodotMainScreenControl::on_submit_move_pressed).bind(1.0, 0.0, 0.0));
    bind_button(Object::cast_to<BaseButton>(find_child("HoverTileButton", true, false)), callable_mp(this, &Gs1GodotMainScreenControl::on_submit_selected_tile_context_pressed).bind(0));
    bind_button(Object::cast_to<BaseButton>(find_child("CancelActionButton", true, false)), callable_mp(this, &Gs1GodotMainScreenControl::on_submit_site_action_cancel_pressed).bind(0, SITE_ACTION_CANCEL_FLAG_CURRENT_ACTION | SITE_ACTION_CANCEL_FLAG_PLACEMENT_MODE));
    bind_button(Object::cast_to<BaseButton>(find_child("OpenTechTreeButton", true, false)), callable_mp(this, &Gs1GodotMainScreenControl::on_toggle_regional_tech_tree_pressed));
    bind_button(Object::cast_to<BaseButton>(find_child("OpenWorkerPackButton", true, false)), callable_mp(this, &Gs1GodotMainScreenControl::on_open_worker_pack_pressed));
    bind_button(Object::cast_to<BaseButton>(find_child("OpenNearestStorageButton", true, false)), callable_mp(this, &Gs1GodotMainScreenControl::on_open_nearest_storage_pressed));
    bind_button(Object::cast_to<BaseButton>(find_child("CloseStorageButton", true, false)), callable_mp(this, &Gs1GodotMainScreenControl::on_close_storage_pressed));
    bind_button(Object::cast_to<BaseButton>(find_child("UseSelectedItemButton", true, false)), callable_mp(this, &Gs1GodotMainScreenControl::on_use_selected_item_pressed));
    bind_button(Object::cast_to<BaseButton>(find_child("TransferSelectedItemButton", true, false)), callable_mp(this, &Gs1GodotMainScreenControl::on_transfer_selected_item_pressed));
    bind_button(Object::cast_to<BaseButton>(find_child("PlantSelectedSeedButton", true, false)), callable_mp(this, &Gs1GodotMainScreenControl::on_plant_selected_seed_pressed));
    bind_button(Object::cast_to<BaseButton>(find_child("CraftAtSelectedTileButton", true, false)), callable_mp(this, &Gs1GodotMainScreenControl::on_submit_selected_tile_context_pressed).bind(0));

    buttons_wired_ = true;
}

bool Gs1GodotMainScreenControl::handles_engine_message(Gs1EngineMessageType type) const noexcept
{
    switch (type)
    {
    case GS1_ENGINE_MESSAGE_SET_APP_STATE:
    case GS1_ENGINE_MESSAGE_BEGIN_REGIONAL_MAP_SNAPSHOT:
    case GS1_ENGINE_MESSAGE_REGIONAL_MAP_SITE_UPSERT:
    case GS1_ENGINE_MESSAGE_REGIONAL_MAP_SITE_REMOVE:
    case GS1_ENGINE_MESSAGE_REGIONAL_MAP_LINK_UPSERT:
    case GS1_ENGINE_MESSAGE_REGIONAL_MAP_LINK_REMOVE:
    case GS1_ENGINE_MESSAGE_END_REGIONAL_MAP_SNAPSHOT:
    case GS1_ENGINE_MESSAGE_BEGIN_SITE_SNAPSHOT:
    case GS1_ENGINE_MESSAGE_SITE_TILE_UPSERT:
    case GS1_ENGINE_MESSAGE_SITE_WORKER_UPDATE:
    case GS1_ENGINE_MESSAGE_SITE_CAMP_UPDATE:
    case GS1_ENGINE_MESSAGE_SITE_WEATHER_UPDATE:
    case GS1_ENGINE_MESSAGE_SITE_INVENTORY_STORAGE_UPSERT:
    case GS1_ENGINE_MESSAGE_SITE_INVENTORY_SLOT_UPSERT:
    case GS1_ENGINE_MESSAGE_SITE_INVENTORY_VIEW_STATE:
    case GS1_ENGINE_MESSAGE_SITE_CRAFT_CONTEXT_BEGIN:
    case GS1_ENGINE_MESSAGE_SITE_CRAFT_CONTEXT_OPTION_UPSERT:
    case GS1_ENGINE_MESSAGE_SITE_CRAFT_CONTEXT_END:
    case GS1_ENGINE_MESSAGE_SITE_PLACEMENT_PREVIEW:
    case GS1_ENGINE_MESSAGE_SITE_PLACEMENT_PREVIEW_TILE_UPSERT:
    case GS1_ENGINE_MESSAGE_SITE_PLACEMENT_FAILURE:
    case GS1_ENGINE_MESSAGE_SITE_TASK_UPSERT:
    case GS1_ENGINE_MESSAGE_SITE_TASK_REMOVE:
    case GS1_ENGINE_MESSAGE_SITE_PHONE_LISTING_UPSERT:
    case GS1_ENGINE_MESSAGE_SITE_PHONE_LISTING_REMOVE:
    case GS1_ENGINE_MESSAGE_SITE_PHONE_PANEL_STATE:
    case GS1_ENGINE_MESSAGE_SITE_PROTECTION_OVERLAY_STATE:
    case GS1_ENGINE_MESSAGE_SITE_MODIFIER_LIST_BEGIN:
    case GS1_ENGINE_MESSAGE_SITE_MODIFIER_UPSERT:
    case GS1_ENGINE_MESSAGE_SITE_ACTION_UPDATE:
    case GS1_ENGINE_MESSAGE_HUD_STATE:
    case GS1_ENGINE_MESSAGE_SITE_RESULT_READY:
    case GS1_ENGINE_MESSAGE_END_SITE_SNAPSHOT:
        return true;
    default:
        return false;
    }
}

void Gs1GodotMainScreenControl::handle_engine_message(const Gs1EngineMessage& message)
{
    regional_map_state_reducer_.apply_engine_message(message);
    site_state_reducer_.apply_engine_message(message);

    switch (message.type)
    {
    case GS1_ENGINE_MESSAGE_SET_APP_STATE:
        last_app_state_ = static_cast<int>(message.payload_as<Gs1EngineMessageSetAppStateData>().app_state);
        mark_regional_map_dirty();
        mark_regional_visuals_dirty();
        mark_selected_tile_dirty();
        break;
    case GS1_ENGINE_MESSAGE_BEGIN_REGIONAL_MAP_SNAPSHOT:
    case GS1_ENGINE_MESSAGE_REGIONAL_MAP_SITE_UPSERT:
    case GS1_ENGINE_MESSAGE_REGIONAL_MAP_SITE_REMOVE:
    case GS1_ENGINE_MESSAGE_REGIONAL_MAP_LINK_UPSERT:
    case GS1_ENGINE_MESSAGE_REGIONAL_MAP_LINK_REMOVE:
    case GS1_ENGINE_MESSAGE_END_REGIONAL_MAP_SNAPSHOT:
        mark_regional_map_dirty();
        mark_regional_visuals_dirty();
        break;
    case GS1_ENGINE_MESSAGE_BEGIN_SITE_SNAPSHOT:
    case GS1_ENGINE_MESSAGE_SITE_TILE_UPSERT:
    case GS1_ENGINE_MESSAGE_SITE_WORKER_UPDATE:
    case GS1_ENGINE_MESSAGE_SITE_CAMP_UPDATE:
    case GS1_ENGINE_MESSAGE_SITE_WEATHER_UPDATE:
    case GS1_ENGINE_MESSAGE_SITE_INVENTORY_STORAGE_UPSERT:
    case GS1_ENGINE_MESSAGE_SITE_INVENTORY_SLOT_UPSERT:
    case GS1_ENGINE_MESSAGE_SITE_INVENTORY_VIEW_STATE:
    case GS1_ENGINE_MESSAGE_SITE_CRAFT_CONTEXT_BEGIN:
    case GS1_ENGINE_MESSAGE_SITE_CRAFT_CONTEXT_OPTION_UPSERT:
    case GS1_ENGINE_MESSAGE_SITE_CRAFT_CONTEXT_END:
    case GS1_ENGINE_MESSAGE_SITE_PLACEMENT_PREVIEW:
    case GS1_ENGINE_MESSAGE_SITE_PLACEMENT_PREVIEW_TILE_UPSERT:
    case GS1_ENGINE_MESSAGE_SITE_PLACEMENT_FAILURE:
    case GS1_ENGINE_MESSAGE_SITE_TASK_UPSERT:
    case GS1_ENGINE_MESSAGE_SITE_TASK_REMOVE:
    case GS1_ENGINE_MESSAGE_SITE_PHONE_LISTING_UPSERT:
    case GS1_ENGINE_MESSAGE_SITE_PHONE_LISTING_REMOVE:
    case GS1_ENGINE_MESSAGE_SITE_PHONE_PANEL_STATE:
    case GS1_ENGINE_MESSAGE_SITE_PROTECTION_OVERLAY_STATE:
    case GS1_ENGINE_MESSAGE_SITE_MODIFIER_LIST_BEGIN:
    case GS1_ENGINE_MESSAGE_SITE_MODIFIER_UPSERT:
    case GS1_ENGINE_MESSAGE_SITE_ACTION_UPDATE:
    case GS1_ENGINE_MESSAGE_SITE_RESULT_READY:
    case GS1_ENGINE_MESSAGE_HUD_STATE:
    case GS1_ENGINE_MESSAGE_END_SITE_SNAPSHOT:
        mark_selected_tile_dirty();
        break;
    default:
        break;
    }
}

void Gs1GodotMainScreenControl::handle_runtime_message_reset()
{
    regional_map_state_reducer_.reset();
    site_state_reducer_.reset();
    last_app_state_ = APP_STATE_BOOT;
    cached_storage_lookup_.clear();
    invalidate_all_ui();
}

void Gs1GodotMainScreenControl::disconnect_runtime_subscriptions()
{
    if (runtime_node_ == nullptr)
    {
        return;
    }

    runtime_node_->unsubscribe_engine_messages(*this);
    runtime_node_->unsubscribe_engine_messages(status_panel_controller_);
    runtime_node_->unsubscribe_engine_messages(inventory_panel_controller_);
    runtime_node_->unsubscribe_engine_messages(craft_panel_controller_);
    runtime_node_->unsubscribe_engine_messages(action_panel_controller_);
    runtime_node_->unsubscribe_engine_messages(overlay_panel_controller_);
    runtime_node_->unsubscribe_engine_messages(phone_panel_controller_);
    runtime_node_->unsubscribe_engine_messages(regional_selection_panel_controller_);
    runtime_node_->unsubscribe_engine_messages(regional_summary_panel_controller_);
    runtime_node_->unsubscribe_engine_messages(regional_tech_tree_panel_controller_);
    runtime_node_->unsubscribe_engine_messages(site_summary_panel_controller_);
    runtime_node_->unsubscribe_engine_messages(task_panel_controller_);
    runtime_node_ = nullptr;
}

void Gs1GodotMainScreenControl::apply_bootstrap_app_state(int app_state)
{
    Gs1EngineMessage message {};
    message.type = GS1_ENGINE_MESSAGE_SET_APP_STATE;
    auto& payload = message.emplace_payload<Gs1EngineMessageSetAppStateData>();
    payload.app_state = static_cast<Gs1AppState>(app_state);

    handle_engine_message(message);
    status_panel_controller_.handle_engine_message(message);
    inventory_panel_controller_.handle_engine_message(message);
    craft_panel_controller_.handle_engine_message(message);
    action_panel_controller_.handle_engine_message(message);
    overlay_panel_controller_.handle_engine_message(message);
    phone_panel_controller_.handle_engine_message(message);
    regional_selection_panel_controller_.handle_engine_message(message);
    regional_summary_panel_controller_.handle_engine_message(message);
    regional_tech_tree_panel_controller_.handle_engine_message(message);
    site_summary_panel_controller_.handle_engine_message(message);
    task_panel_controller_.handle_engine_message(message);
}

void Gs1GodotMainScreenControl::invalidate_all_ui()
{
    mark_regional_map_dirty();
    mark_regional_visuals_dirty();
    mark_selected_tile_dirty();
    last_tile_label_x_ = -1;
    last_tile_label_y_ = -1;
}

void Gs1GodotMainScreenControl::update_visibility(int app_state)
{
    const bool menu_visible = app_state == APP_STATE_MAIN_MENU || app_state == APP_STATE_BOOT;
    const bool regional_visible = app_state == APP_STATE_REGIONAL_MAP || app_state == APP_STATE_SITE_LOADING;
    const Gs1RuntimeSiteProjection* site_state = active_site();
    const bool site_visible = site_state != nullptr && app_state >= APP_STATE_SITE_LOADING && app_state <= APP_STATE_SITE_RESULT;

    if (menu_panel_ != nullptr)
    {
        menu_panel_->set_visible(menu_visible);
    }
    if (regional_map_panel_ != nullptr)
    {
        regional_map_panel_->set_visible(regional_visible);
    }
    if (regional_map_world_ != nullptr)
    {
        regional_map_world_->set_visible(regional_visible);
    }
    if (site_panel_ != nullptr)
    {
        site_panel_->set_visible(site_visible);
    }
    if (site_view_ != nullptr)
    {
        site_view_->set_visible(site_visible);
    }
    if (site_view_ != nullptr && regional_visible)
    {
        site_view_->set_visible(false);
    }
    if (regional_map_world_ != nullptr && site_visible)
    {
        regional_map_world_->set_visible(false);
    }
    const bool regional_visibility_changed = regional_visible != regional_map_visible_;
    const bool site_visibility_changed = site_visible != site_panel_visible_;
    const bool app_state_changed = app_state != last_visible_app_state_;
    if (regional_visibility_changed || app_state_changed)
    {
        mark_regional_map_dirty();
        mark_regional_visuals_dirty();
    }
    if (site_visibility_changed || app_state_changed)
    {
        mark_selected_tile_dirty();
    }

    regional_map_visible_ = regional_visible;
    site_panel_visible_ = site_visible;
    last_visible_app_state_ = app_state;
}

void Gs1GodotMainScreenControl::refresh_regional_map_if_needed(int app_state)
{
    if (!regional_map_visible_)
    {
        return;
    }
    if (!regional_map_dirty_ && !regional_visuals_dirty_)
    {
        return;
    }
    refresh_regional_map(app_state);
    regional_map_dirty_ = false;
    regional_visuals_dirty_ = false;
}

void Gs1GodotMainScreenControl::refresh_selected_tile_if_needed()
{
    if (!site_panel_visible_)
    {
        return;
    }
    if (!selected_tile_dirty_ &&
        selected_tile_.x == last_tile_label_x_ &&
        selected_tile_.y == last_tile_label_y_)
    {
        return;
    }

    clamp_selected_tile();
    const Gs1RuntimeTileProjection* selected_tile = tile_at(selected_tile_);
    if (tile_label_ != nullptr)
    {
        String tile_text = vformat(
            "Selected Tile: (%d, %d)  Plant: %s  Structure: %s",
            selected_tile_.x,
            selected_tile_.y,
            selected_tile != nullptr && selected_tile->plant_type_id != 0U ? plant_name_for(static_cast<int>(selected_tile->plant_type_id)) : String("None"),
            selected_tile != nullptr && selected_tile->structure_type_id != 0U ? structure_name_for(static_cast<int>(selected_tile->structure_type_id)) : String("None"));
        if (selected_tile != nullptr)
        {
            tile_text += vformat(
                "  Wind %.2f  Moisture %.2f  Fertility %.2f",
                selected_tile->local_wind,
                selected_tile->moisture,
                selected_tile->soil_fertility);
        }
        tile_label_->set_text(tile_text);
    }
    last_tile_label_x_ = selected_tile_.x;
    last_tile_label_y_ = selected_tile_.y;
    selected_tile_dirty_ = false;
}

void Gs1GodotMainScreenControl::mark_regional_map_dirty() { regional_map_dirty_ = true; }
void Gs1GodotMainScreenControl::mark_regional_visuals_dirty() { regional_visuals_dirty_ = true; }
void Gs1GodotMainScreenControl::mark_selected_tile_dirty() { selected_tile_dirty_ = true; }

void Gs1GodotMainScreenControl::refresh_regional_map(int app_state)
{
    const bool panel_visible = app_state == APP_STATE_REGIONAL_MAP || app_state == APP_STATE_SITE_LOADING;
    if (!panel_visible)
    {
        return;
    }

    const bool force_refresh = regional_map_dirty_;

    const auto& regional_state = regional_map_state_reducer_.state();
    const auto& sites = regional_state.sites;
    const auto& links = regional_state.links;

    if (force_refresh)
    {
        rebuild_regional_map_world(sites, links);
    }

    if (force_refresh || regional_visuals_dirty_)
    {
        update_regional_site_visuals();
    }
}

void Gs1GodotMainScreenControl::toggle_regional_tech_tree()
{
    if (regional_tech_tree_panel_controller_.is_panel_visible())
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
    if (submit_runtime)
    {
        submit_ui_action(UI_ACTION_SELECT_DEPLOYMENT_SITE, site_id);
    }
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
    const auto& regional_state = regional_map_state_reducer_.state();
    int selected_site_id = regional_state.selected_site_id.has_value()
        ? static_cast<int>(regional_state.selected_site_id.value())
        : 0;
    if (selected_site_id == 0 && !regional_state.sites.empty())
    {
        selected_site_id = static_cast<int>(regional_state.sites.front().site_id);
    }

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
        const bool selected = site_id == selected_site_id;
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

const Gs1RuntimeSiteProjection* Gs1GodotMainScreenControl::active_site() const
{
    if (!site_state_reducer_.state().has_value())
    {
        return nullptr;
    }
    return &site_state_reducer_.state().value();
}

const Gs1RuntimeTileProjection* Gs1GodotMainScreenControl::tile_at(const Vector2i& tile_coord) const
{
    const Gs1RuntimeSiteProjection* site_state = active_site();
    if (site_state == nullptr || site_state->width == 0 || site_state->height == 0)
    {
        return nullptr;
    }
    if (tile_coord.x < 0 || tile_coord.y < 0 ||
        tile_coord.x >= site_state->width || tile_coord.y >= site_state->height)
    {
        return nullptr;
    }

    const std::size_t index = static_cast<std::size_t>(tile_coord.y) * static_cast<std::size_t>(site_state->width)
        + static_cast<std::size_t>(tile_coord.x);
    if (index >= site_state->tiles.size())
    {
        return nullptr;
    }

    return &site_state->tiles[index];
}

int Gs1GodotMainScreenControl::find_worker_pack_storage_id() const
{
    const Gs1RuntimeSiteProjection* site_state = active_site();
    if (site_state == nullptr)
    {
        return 0;
    }

    for (const auto& storage : site_state->inventory_storages)
    {
        if (static_cast<int>(storage.container_kind) == CONTAINER_WORKER_PACK)
        {
            return static_cast<int>(storage.storage_id);
        }
    }
    return 0;
}

int Gs1GodotMainScreenControl::find_selected_tile_storage_id()
{
    const Gs1RuntimeSiteProjection* site_state = active_site();
    if (site_state == nullptr)
    {
        return 0;
    }

    for (const auto& storage : site_state->inventory_storages)
    {
        const int tile_x = storage.tile_x;
        const int tile_y = storage.tile_y;
        const int storage_id = static_cast<int>(storage.storage_id);
        cached_storage_lookup_[tile_y * 100000 + tile_x] = storage_id;
        if (tile_x == selected_tile_.x && tile_y == selected_tile_.y)
        {
            return storage_id;
        }
    }
    return 0;
}

String Gs1GodotMainScreenControl::plant_name_for(int plant_id) const
{
    auto found = plant_name_cache_.find(plant_id);
    if (found != plant_name_cache_.end())
    {
        return found->second;
    }

    String display_name = String("Plant #") + String::num_int64(plant_id);
    if (const auto* plant_def = gs1::find_plant_def(gs1::PlantId {static_cast<std::uint32_t>(plant_id)}))
    {
        display_name = string_from_view(plant_def->display_name);
    }

    return plant_name_cache_.emplace(plant_id, display_name).first->second;
}

String Gs1GodotMainScreenControl::structure_name_for(int structure_id) const
{
    auto found = structure_name_cache_.find(structure_id);
    if (found != structure_name_cache_.end())
    {
        return found->second;
    }

    String display_name = String("Structure #") + String::num_int64(structure_id);
    if (const auto* structure_def = gs1::find_structure_def(gs1::StructureId {static_cast<std::uint32_t>(structure_id)}))
    {
        display_name = string_from_view(structure_def->display_name);
    }

    return structure_name_cache_.emplace(structure_id, display_name).first->second;
}

void Gs1GodotMainScreenControl::use_first_usable_item()
{
    const Gs1RuntimeSiteProjection* site_state = active_site();
    if (site_state == nullptr)
    {
        return;
    }

    for (const auto& slot : site_state->worker_pack_slots)
    {
        const auto* item_def = gs1::find_item_def(gs1::ItemId {slot.item_id});
        const int capability_flags = item_def != nullptr ? static_cast<int>(item_def->capability_flags) : 0;
        if (capability_flags == 0)
        {
            continue;
        }
        const int storage_id = static_cast<int>(slot.storage_id);
        const int slot_index = static_cast<int>(slot.slot_index);
        const int quantity = std::max(1, static_cast<int>(slot.quantity));
        const int packed_arg0 = CONTAINER_WORKER_PACK | (slot_index << 8) | (quantity << 16);
        submit_ui_action(UI_ACTION_USE_INVENTORY_ITEM, static_cast<int>(slot.item_instance_id), packed_arg0, storage_id);
        return;
    }
}

void Gs1GodotMainScreenControl::transfer_first_storage_item_to_pack()
{
    const Gs1RuntimeSiteProjection* site_state = active_site();
    if (site_state == nullptr || !site_state->opened_storage.has_value())
    {
        return;
    }

    const auto& opened_storage = site_state->opened_storage.value();
    if (opened_storage.slots.empty())
    {
        return;
    }

    const int destination_storage_id = find_worker_pack_storage_id();
    if (destination_storage_id == 0)
    {
        return;
    }

    const auto& slot = opened_storage.slots.front();
    const std::uint32_t source_storage_id = slot.storage_id;
    const int source_slot_index = static_cast<int>(slot.slot_index);
    const int quantity = std::max(1, static_cast<int>(slot.quantity));
    const int packed_arg0 = CONTAINER_DEVICE_STORAGE | (source_slot_index << 8) | (CONTAINER_WORKER_PACK << 16) | (quantity << 24);
    const std::uint64_t packed_arg1 = static_cast<std::uint64_t>(source_storage_id)
        | (static_cast<std::uint64_t>(static_cast<std::uint32_t>(destination_storage_id)) << 32U);
    submit_ui_action(UI_ACTION_TRANSFER_INVENTORY_ITEM, 0, packed_arg0, packed_arg1);
}

void Gs1GodotMainScreenControl::plant_first_seed_on_selected_tile()
{
    const Gs1RuntimeSiteProjection* site_state = active_site();
    if (site_state == nullptr)
    {
        return;
    }

    for (const auto& slot : site_state->worker_pack_slots)
    {
        const auto* item_def = gs1::find_item_def(gs1::ItemId {slot.item_id});
        if (item_def == nullptr || item_def->linked_plant_id.value == 0U)
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
            static_cast<int>(slot.item_id));
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
    publish_last_action_message();
}

void Gs1GodotMainScreenControl::submit_move(double x, double y, double z)
{
    if (runtime_node_ == nullptr)
    {
        return;
    }
    const bool ok = runtime_node_->submit_move_direction(x, y, z);
    last_action_message_ = vformat("Move (%s)", ok ? "ok" : "failed");
    publish_last_action_message();
}

void Gs1GodotMainScreenControl::submit_site_context_request(int tile_x, int tile_y, int flags)
{
    if (runtime_node_ == nullptr)
    {
        return;
    }
    const bool ok = runtime_node_->submit_site_context_request(tile_x, tile_y, flags);
    last_action_message_ = vformat("Context (%d, %d) %s", tile_x, tile_y, ok ? "ok" : "failed");
    publish_last_action_message();
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
    publish_last_action_message();
}

void Gs1GodotMainScreenControl::submit_site_action_cancel(int action_id, int flags)
{
    if (runtime_node_ == nullptr)
    {
        return;
    }
    const bool ok = runtime_node_->submit_site_action_cancel(action_id, flags);
    last_action_message_ = vformat("Cancel action (%s)", ok ? "ok" : "failed");
    publish_last_action_message();
}

void Gs1GodotMainScreenControl::submit_storage_view(int storage_id, int event_kind)
{
    if (runtime_node_ == nullptr)
    {
        return;
    }
    const bool ok = runtime_node_->submit_site_storage_view(storage_id, event_kind);
    last_action_message_ = vformat("Storage %d (%s)", storage_id, ok ? "ok" : "failed");
    publish_last_action_message();
}

void Gs1GodotMainScreenControl::publish_last_action_message()
{
    status_panel_controller_.set_last_action_message(last_action_message_);
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
    const Gs1RuntimeSiteProjection* site_state = active_site();
    const int width = std::max(1, site_state != nullptr ? static_cast<int>(site_state->width) : 1);
    const int height = std::max(1, site_state != nullptr ? static_cast<int>(site_state->height) : 1);
    const int clamped_x = std::clamp(selected_tile_.x, 0, width - 1);
    const int clamped_y = std::clamp(selected_tile_.y, 0, height - 1);
    if (clamped_x != selected_tile_.x || clamped_y != selected_tile_.y)
    {
        selected_tile_.x = clamped_x;
        selected_tile_.y = clamped_y;
        mark_selected_tile_dirty();
        return;
    }
    selected_tile_.x = clamped_x;
    selected_tile_.y = clamped_y;
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

void Gs1GodotMainScreenControl::set_runtime_node_path(const NodePath& path)
{
    disconnect_runtime_subscriptions();
    runtime_node_path_ = path;
}
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

void Gs1GodotMainScreenControl::on_submit_ui_action_pressed(std::int64_t action_type, std::int64_t target_id, std::int64_t arg0, std::int64_t arg1)
{
    submit_ui_action(action_type, target_id, arg0, arg1);
}

void Gs1GodotMainScreenControl::on_submit_move_pressed(double x, double y, double z)
{
    submit_move(x, y, z);
    mark_selected_tile_dirty();
}

void Gs1GodotMainScreenControl::on_submit_selected_tile_context_pressed(int flags)
{
    submit_site_context_request(selected_tile_.x, selected_tile_.y, flags);
}

void Gs1GodotMainScreenControl::on_submit_site_action_cancel_pressed(int action_id, int flags)
{
    submit_site_action_cancel(action_id, flags);
}

void Gs1GodotMainScreenControl::on_toggle_regional_tech_tree_pressed()
{
    toggle_regional_tech_tree();
}

void Gs1GodotMainScreenControl::on_menu_settings_pressed()
{
    last_action_message_ = "Settings are not wired yet in the prototype";
    publish_last_action_message();
}
void Gs1GodotMainScreenControl::on_quit_pressed() { if (SceneTree* tree = get_tree()) { tree->quit(); } }
void Gs1GodotMainScreenControl::on_open_worker_pack_pressed() { const int id = find_worker_pack_storage_id(); if (id != 0) { submit_storage_view(id, INVENTORY_VIEW_EVENT_OPEN_SNAPSHOT); } }
void Gs1GodotMainScreenControl::on_open_nearest_storage_pressed() { const int id = find_selected_tile_storage_id(); if (id != 0) { submit_storage_view(id, INVENTORY_VIEW_EVENT_OPEN_SNAPSHOT); } }
void Gs1GodotMainScreenControl::on_close_storage_pressed()
{
    const Gs1RuntimeSiteProjection* site_state = active_site();
    if (site_state != nullptr && site_state->opened_storage.has_value())
    {
        submit_storage_view(static_cast<int>(site_state->opened_storage->storage_id), INVENTORY_VIEW_EVENT_CLOSE);
    }
}
void Gs1GodotMainScreenControl::on_use_selected_item_pressed() { use_first_usable_item(); }
void Gs1GodotMainScreenControl::on_transfer_selected_item_pressed() { transfer_first_storage_item_to_pack(); }
void Gs1GodotMainScreenControl::on_plant_selected_seed_pressed() { plant_first_seed_on_selected_tile(); }
void Gs1GodotMainScreenControl::on_dynamic_regional_site_pressed(int site_id) { select_regional_site(site_id, true); }
