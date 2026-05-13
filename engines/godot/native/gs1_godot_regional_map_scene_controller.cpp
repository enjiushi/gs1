#include "gs1_godot_regional_map_click_policy.h"
#include "godot_progression_resources.h"
#include "gs1_godot_regional_map_scene_controller.h"

#include "gs1_godot_controller_context.h"

#include <godot_cpp/classes/base_material3d.hpp>
#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/label3d.hpp>
#include <godot_cpp/classes/material.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/packed_scene.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/core/object.hpp>

#include <algorithm>
#include <limits>
#include <utility>

using namespace godot;

namespace
{
void sort_regional_map_sites(std::vector<Gs1RuntimeRegionalMapSiteProjection>& sites)
{
    std::sort(sites.begin(), sites.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.site_id < rhs.site_id;
    });
}

Node3D* make_node3d()
{
    return memnew(Node3D);
}

template <typename T>
T* resolve_object(const ObjectID object_id)
{
    return Object::cast_to<T>(ObjectDB::get_instance(object_id));
}

constexpr char k_default_regional_site_marker_scene_path[] = "res://scenes/regional_site_marker.tscn";

Ref<PackedScene> load_packed_scene(const String& path)
{
    if (path.is_empty())
    {
        return Ref<PackedScene> {};
    }

    if (ResourceLoader* loader = ResourceLoader::get_singleton())
    {
        return loader->load(path);
    }
    return Ref<PackedScene> {};
}

Ref<PackedScene> load_regional_site_marker_scene(std::uint32_t site_id)
{
    Ref<PackedScene> marker_scene = load_packed_scene(
        GodotProgressionResourceDatabase::instance().site_scene_path(site_id));
    if (marker_scene.is_null())
    {
        marker_scene = load_packed_scene(String(k_default_regional_site_marker_scene_path));
    }
    return marker_scene;
}

Gs1RuntimeRegionalMapSiteProjection build_site_projection(const Gs1CampaignSiteView& site_view)
{
    Gs1RuntimeRegionalMapSiteProjection projection {};
    projection.site_id = site_view.site_id;
    projection.site_archetype_id = site_view.site_archetype_id;
    projection.map_x = site_view.regional_map_tile_x * 160;
    projection.map_y = site_view.regional_map_tile_y * 160;
    projection.support_package_id = site_view.support_package_id;
    projection.support_preview_mask = site_view.exported_support_item_count > 0U ? 1U : 0U;
    projection.site_state = site_view.site_state;
    projection.flags = 0U;
    return projection;
}
}

void Gs1GodotRegionalMapSceneController::_bind_methods()
{
}

void Gs1GodotRegionalMapSceneController::_ready()
{
    cache_adapter_service();
    cache_scene_references();
    cache_ui_references();
    refresh_from_game_state_view();
    refresh_regional_map();
    set_process_unhandled_input(true);
}

void Gs1GodotRegionalMapSceneController::_exit_tree()
{
    if (adapter_service_ != nullptr)
    {
        adapter_service_->unsubscribe_all(*this);
        adapter_service_ = nullptr;
    }
}

void Gs1GodotRegionalMapSceneController::_unhandled_input(const Ref<InputEvent>& event)
{
    const auto* mouse_event = event.is_null() ? nullptr : Object::cast_to<InputEventMouseButton>(*event);
    const bool left_mouse_pressed = mouse_event != nullptr &&
        mouse_event->is_pressed() &&
        mouse_event->get_button_index() == MOUSE_BUTTON_LEFT;
    if (!left_mouse_pressed)
    {
        return;
    }

    const Vector2 screen_position = mouse_event->get_position();
    const bool ui_contains_screen_point = regional_map_ui_contains_screen_point(screen_position);
    const Gs1GodotRegionalMapPickOutcome pick_outcome = !ui_contains_screen_point
        ? try_select_regional_site_from_screen(screen_position)
        : GS1_GODOT_REGIONAL_MAP_PICK_NONE;
    const Gs1GodotRegionalMapInputOutcome input_outcome = gs1_godot_resolve_regional_map_input(
        left_mouse_pressed,
        ui_contains_screen_point,
        pick_outcome,
        selected_site_id_.has_value());

    if (input_outcome == GS1_GODOT_REGIONAL_MAP_INPUT_IGNORE)
    {
        return;
    }

    if (input_outcome == GS1_GODOT_REGIONAL_MAP_INPUT_CLEAR_SELECTION)
    {
        clear_regional_site_selection();
    }

    if (input_outcome == GS1_GODOT_REGIONAL_MAP_INPUT_SITE_PICKED ||
        input_outcome == GS1_GODOT_REGIONAL_MAP_INPUT_CLEAR_SELECTION ||
        input_outcome == GS1_GODOT_REGIONAL_MAP_INPUT_CONSUME_BLANK ||
        input_outcome == GS1_GODOT_REGIONAL_MAP_INPUT_CONSUME_UNSELECTABLE_SITE ||
        input_outcome == GS1_GODOT_REGIONAL_MAP_INPUT_CONSUME_UI)
    {
        if (Viewport* viewport = get_viewport())
        {
            viewport->set_input_as_handled();
        }
    }
}

void Gs1GodotRegionalMapSceneController::cache_adapter_service()
{
    if (adapter_service_ != nullptr)
    {
        return;
    }

    adapter_service_ = gs1_resolve_adapter_service(this);
    if (adapter_service_ != nullptr)
    {
        adapter_service_->subscribe_matching_messages(*this);
    }
}

void Gs1GodotRegionalMapSceneController::cache_scene_references()
{
    if (regional_map_world_ == nullptr)
    {
        regional_map_world_ = Object::cast_to<Node3D>(get_node_or_null(regional_world_path_));
    }
    if (regional_camera_ == nullptr)
    {
        regional_camera_ = Object::cast_to<Camera3D>(get_node_or_null(regional_camera_path_));
    }
    if (regional_site_root_ == nullptr)
    {
        regional_site_root_ = Object::cast_to<Node3D>(get_node_or_null(regional_site_root_path_));
    }
}

void Gs1GodotRegionalMapSceneController::cache_ui_references()
{
    if (regional_map_panel_ == nullptr)
    {
        regional_map_panel_ = Object::cast_to<Control>(find_child("RegionalMapHud", true, false));
        if (regional_map_panel_ == nullptr)
        {
            regional_map_panel_ = Object::cast_to<Control>(find_child("RegionalMapPanel", true, false));
        }
    }
    if (regional_selection_panel_ == nullptr)
    {
        regional_selection_panel_ = Object::cast_to<Control>(find_child("RegionalSelectionPanel", true, false));
    }
    if (regional_tech_tree_overlay_ == nullptr)
    {
        regional_tech_tree_overlay_ = Object::cast_to<Control>(find_child("TechOverlay", true, false));
    }
}

bool Gs1GodotRegionalMapSceneController::handles_engine_message(Gs1EngineMessageType type) const noexcept
{
    return type == GS1_ENGINE_MESSAGE_PRESENTATION_DIRTY ||
        type == GS1_ENGINE_MESSAGE_SET_APP_STATE;
}

void Gs1GodotRegionalMapSceneController::reset_regional_map_state() noexcept
{
    selected_site_id_.reset();
    sites_.clear();
}

void Gs1GodotRegionalMapSceneController::refresh_from_game_state_view()
{
    reset_regional_map_state();
    if (adapter_service_ == nullptr)
    {
        return;
    }

    Gs1GameStateView view {};
    if (!adapter_service_->get_game_state_view(view) ||
        view.has_campaign == 0U ||
        view.campaign == nullptr)
    {
        return;
    }

    const Gs1CampaignStateView& campaign = *view.campaign;
    if (campaign.selected_site_id > 0)
    {
        selected_site_id_ = static_cast<std::uint32_t>(campaign.selected_site_id);
    }

    sites_.reserve(campaign.site_count);
    for (std::uint32_t site_index = 0; site_index < campaign.site_count; ++site_index)
    {
        sites_.push_back(build_site_projection(campaign.sites[site_index]));
    }
    sort_regional_map_sites(sites_);
}

void Gs1GodotRegionalMapSceneController::handle_engine_message(const Gs1EngineMessage& message)
{
    if (message.type == GS1_ENGINE_MESSAGE_PRESENTATION_DIRTY ||
        message.type == GS1_ENGINE_MESSAGE_SET_APP_STATE)
    {
        refresh_from_game_state_view();
        refresh_regional_map();
    }
}

void Gs1GodotRegionalMapSceneController::handle_runtime_message_reset()
{
    reset_regional_map_state();
    clear_regional_projection_world();
    regional_material_cache_.clear();
}

void Gs1GodotRegionalMapSceneController::submit_ui_action(std::int64_t action_type, std::int64_t target_id)
{
    if (adapter_service_ != nullptr)
    {
        adapter_service_->submit_ui_action(action_type, target_id, 0, 0);
    }
}

void Gs1GodotRegionalMapSceneController::select_regional_site(int site_id)
{
    submit_ui_action(UI_ACTION_SELECT_DEPLOYMENT_SITE, site_id);
}

void Gs1GodotRegionalMapSceneController::clear_regional_site_selection()
{
    submit_ui_action(UI_ACTION_CLEAR_DEPLOYMENT_SITE_SELECTION);
}

void Gs1GodotRegionalMapSceneController::refresh_regional_map()
{
    if (sites_.empty())
    {
        clear_regional_projection_world();
        return;
    }

    rebuild_regional_map_world(sites_);
}

void Gs1GodotRegionalMapSceneController::rebuild_regional_map_world(
    const std::vector<Gs1RuntimeRegionalMapSiteProjection>& sites)
{
    if (regional_map_world_ == nullptr || regional_site_root_ == nullptr)
    {
        return;
    }
    if (sites.empty())
    {
        clear_regional_projection_world();
        return;
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

    reconcile_regional_sites(sites);
    position_regional_camera(regional_map_bounds_);
    update_regional_site_visuals();
}

void Gs1GodotRegionalMapSceneController::clear_regional_projection_world()
{
    clear_dynamic_children(regional_site_root_, String());
    regional_site_nodes_.clear();
    regional_site_data_.clear();
}

void Gs1GodotRegionalMapSceneController::reconcile_regional_sites(const std::vector<Gs1RuntimeRegionalMapSiteProjection>& sites)
{
    if (regional_site_root_ == nullptr)
    {
        return;
    }

    std::unordered_set<int> desired_site_ids {};
    for (const auto& site : sites)
    {
        const int site_id = static_cast<int>(site.site_id);
        desired_site_ids.insert(site_id);
        auto& record = regional_site_nodes_[site_id];

        Node3D* root = resolve_object<Node3D>(record.root_id);
        if (root == nullptr)
        {
            const Ref<PackedScene> marker_scene = load_regional_site_marker_scene(site.site_id);
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

        MeshInstance3D* tower = resolve_object<MeshInstance3D>(record.tower_id);
        if (tower == nullptr)
        {
            tower = Object::cast_to<MeshInstance3D>(root->find_child("StateTower", true, false));
            if (tower != nullptr)
            {
                record.tower_id = tower->get_instance_id();
            }
        }

        MeshInstance3D* beacon = resolve_object<MeshInstance3D>(record.beacon_id);
        if (beacon == nullptr)
        {
            beacon = Object::cast_to<MeshInstance3D>(root->find_child("StateBeacon", true, false));
            if (beacon != nullptr)
            {
                record.beacon_id = beacon->get_instance_id();
            }
        }

        MeshInstance3D* selection_ring = resolve_object<MeshInstance3D>(record.selection_ring_id);
        if (selection_ring == nullptr)
        {
            selection_ring = Object::cast_to<MeshInstance3D>(root->find_child("SelectionRing", true, false));
            if (selection_ring != nullptr)
            {
                record.selection_ring_id = selection_ring->get_instance_id();
            }
        }

        Label3D* nameplate = resolve_object<Label3D>(record.label_id);
        if (nameplate == nullptr)
        {
            nameplate = Object::cast_to<Label3D>(root->find_child("SiteLabel", true, false));
            if (nameplate != nullptr)
            {
                record.label_id = nameplate->get_instance_id();
            }
        }
        if (nameplate != nullptr)
        {
            nameplate->set_text(vformat("SITE %d", site_id));
            nameplate->set_modulate(Color(0.93, 0.88, 0.77));
        }
    }

    prune_regional_site_registry(desired_site_ids);
}

void Gs1GodotRegionalMapSceneController::position_regional_camera(const Rect2i& bounds)
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

void Gs1GodotRegionalMapSceneController::update_regional_site_visuals()
{
    int selected_site_id = selected_site_id_.has_value()
        ? static_cast<int>(selected_site_id_.value())
        : 0;

    for (auto& [site_id, record] : regional_site_nodes_)
    {
        auto site_it = regional_site_data_.find(site_id);
        Node3D* root = resolve_object<Node3D>(record.root_id);
        MeshInstance3D* base = resolve_object<MeshInstance3D>(record.base_id);
        MeshInstance3D* tower = resolve_object<MeshInstance3D>(record.tower_id);
        MeshInstance3D* beacon = resolve_object<MeshInstance3D>(record.beacon_id);
        MeshInstance3D* selection_ring = resolve_object<MeshInstance3D>(record.selection_ring_id);
        Label3D* label = resolve_object<Label3D>(record.label_id);
        if (root == nullptr || site_it == regional_site_data_.end())
        {
            continue;
        }

        const auto& site = site_it->second;
        const bool selected = site_id == selected_site_id;
        const bool blocked = site.site_state == GS1_SITE_STATE_LOCKED;
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
        if (selection_ring != nullptr)
        {
            selection_ring->set_visible(selected);
            selection_ring->set_material_override(get_material(vformat("site_ring_%d_%s", site_id, selected ? "1" : "0"), Color(0.96, 0.84, 0.52), 0.18, 0.0, true));
        }
        const Ref<StandardMaterial3D> blocked_overlay = blocked
            ? get_material(
                vformat("site_overlay_%s", selected ? "selected" : "blocked"),
                selected ? Color(0.54, 0.54, 0.56, 0.52) : Color(0.43, 0.43, 0.45, 0.66),
                1.0,
                0.0)
            : Ref<StandardMaterial3D> {};
        const Array marker_meshes = root->find_children(String(), String("MeshInstance3D"), true, false);
        for (int64_t mesh_index = 0; mesh_index < marker_meshes.size(); ++mesh_index)
        {
            MeshInstance3D* mesh = Object::cast_to<MeshInstance3D>(marker_meshes[mesh_index]);
            if (mesh == nullptr ||
                mesh == base ||
                mesh == tower ||
                mesh == beacon ||
                mesh == selection_ring)
            {
                continue;
            }

            if (blocked)
            {
                mesh->set_material_overlay(blocked_overlay);
            }
            else
            {
                mesh->set_material_overlay(Ref<Material> {});
            }
        }
        if (label != nullptr)
        {
            label->set_modulate(blocked ? Color(0.70, 0.70, 0.72) : glow_color.lightened(0.18));
        }
    }
}

Vector2i Gs1GodotRegionalMapSceneController::regional_grid_coord(const Gs1RuntimeRegionalMapSiteProjection& site) const
{
    return Vector2i(
        Math::round(site.map_x / 160.0),
        Math::round(site.map_y / 160.0));
}

Vector3 Gs1GodotRegionalMapSceneController::regional_world_position(const Vector2i& grid) const
{
    return Vector3(static_cast<double>(grid.x) * REGIONAL_TILE_SIZE, 0.0, static_cast<double>(grid.y) * REGIONAL_TILE_SIZE);
}

Color Gs1GodotRegionalMapSceneController::regional_site_state_color(int site_state) const
{
    switch (site_state)
    {
    case GS1_SITE_STATE_AVAILABLE:
        return Color(0.71, 0.56, 0.29);
    case GS1_SITE_STATE_COMPLETED:
        return Color(0.36, 0.58, 0.39);
    default:
        return Color(0.44, 0.45, 0.47);
    }
}

Ref<StandardMaterial3D> Gs1GodotRegionalMapSceneController::get_material(
    const String& key,
    const Color& color,
    double roughness,
    double metallic,
    bool emission)
{
    const std::string cache_key = key.utf8().get_data();
    auto found = regional_material_cache_.find(cache_key);
    if (found != regional_material_cache_.end())
    {
        return found->second;
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

void Gs1GodotRegionalMapSceneController::clear_dynamic_children(Node* container, const String& prefix)
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

void Gs1GodotRegionalMapSceneController::prune_regional_site_registry(const std::unordered_set<int>& desired_site_ids)
{
    std::vector<int> stale_site_ids {};
    stale_site_ids.reserve(regional_site_nodes_.size());
    for (const auto& [site_id, record] : regional_site_nodes_)
    {
        (void)record;
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

bool Gs1GodotRegionalMapSceneController::regional_map_ui_contains_screen_point(const Vector2& screen_position) const
{
    const auto contains_screen_point = [&screen_position](const Control* control) -> bool
    {
        return control != nullptr &&
            control->is_visible_in_tree() &&
            control->get_mouse_filter() != Control::MOUSE_FILTER_IGNORE &&
            control->get_global_rect().has_point(screen_position);
    };

    return contains_screen_point(regional_map_panel_) ||
        contains_screen_point(regional_selection_panel_) ||
        contains_screen_point(regional_tech_tree_overlay_);
}

Gs1GodotRegionalMapPickOutcome Gs1GodotRegionalMapSceneController::try_select_regional_site_from_screen(const Vector2& screen_position)
{
    if (regional_map_world_ == nullptr || !regional_map_world_->is_visible() || regional_camera_ == nullptr)
    {
        return GS1_GODOT_REGIONAL_MAP_PICK_NONE;
    }
    if (regional_map_panel_ == nullptr || !regional_map_panel_->is_visible())
    {
        return GS1_GODOT_REGIONAL_MAP_PICK_NONE;
    }

    const Vector3 origin = regional_camera_->project_ray_origin(screen_position);
    const Vector3 normal = regional_camera_->project_ray_normal(screen_position);
    if (Math::abs(normal.y) < 0.0001)
    {
        return GS1_GODOT_REGIONAL_MAP_PICK_NONE;
    }

    const double ground_distance = -origin.y / normal.y;
    if (ground_distance <= 0.0 || ground_distance > REGIONAL_PICK_DISTANCE)
    {
        return GS1_GODOT_REGIONAL_MAP_PICK_NONE;
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
        return GS1_GODOT_REGIONAL_MAP_PICK_NONE;
    }

    const auto site_it = regional_site_data_.find(nearest_site_id);
    if (site_it == regional_site_data_.end())
    {
        return GS1_GODOT_REGIONAL_MAP_PICK_NONE;
    }
    if (!gs1_godot_regional_map_site_is_selectable(site_it->second.site_state))
    {
        return GS1_GODOT_REGIONAL_MAP_PICK_UNSELECTABLE_SITE;
    }

    select_regional_site(nearest_site_id);
    return GS1_GODOT_REGIONAL_MAP_PICK_SELECTABLE_SITE;
}

