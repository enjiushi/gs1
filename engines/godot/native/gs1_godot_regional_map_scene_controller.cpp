#include "godot_progression_resources.h"
#include "gs1_godot_regional_map_scene_controller.h"

#include "gs1_godot_controller_context.h"

#include <godot_cpp/classes/base_material3d.hpp>
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/label3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/packed_scene.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/core/object.hpp>
#include <godot_cpp/core/property_info.hpp>

#include <algorithm>
#include <limits>

using namespace godot;

namespace
{
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

constexpr std::uint64_t pack_u32_pair(std::uint32_t high, std::uint32_t low) noexcept
{
    return (static_cast<std::uint64_t>(high) << 32U) | static_cast<std::uint64_t>(low);
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
}

void Gs1GodotRegionalMapSceneController::_bind_methods()
{
}

void Gs1GodotRegionalMapSceneController::_ready()
{
    cache_adapter_service();
    cache_scene_references();
    cache_ui_references();
    set_process(true);
    set_process_input(true);
}

void Gs1GodotRegionalMapSceneController::_process(double delta)
{
    (void)delta;
    cache_adapter_service();
    cache_scene_references();
    cache_ui_references();
    refresh_regional_map_if_needed();
}

void Gs1GodotRegionalMapSceneController::_exit_tree()
{
    if (adapter_service_ != nullptr)
    {
        adapter_service_->unsubscribe_all(*this);
        adapter_service_ = nullptr;
    }
}

void Gs1GodotRegionalMapSceneController::_input(const Ref<InputEvent>& event)
{
    const auto* mouse_event = event.is_null() ? nullptr : Object::cast_to<InputEventMouseButton>(*event);
    if (mouse_event == nullptr || !mouse_event->is_pressed() || mouse_event->get_button_index() != MOUSE_BUTTON_LEFT)
    {
        return;
    }

    if (!regional_map_ui_contains_screen_point(mouse_event->get_position()) &&
        try_select_regional_site_from_screen(mouse_event->get_position()))
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
    switch (type)
    {
    case GS1_ENGINE_MESSAGE_BEGIN_REGIONAL_MAP_SNAPSHOT:
    case GS1_ENGINE_MESSAGE_REGIONAL_MAP_SITE_UPSERT:
    case GS1_ENGINE_MESSAGE_REGIONAL_MAP_SITE_REMOVE:
    case GS1_ENGINE_MESSAGE_REGIONAL_MAP_LINK_UPSERT:
    case GS1_ENGINE_MESSAGE_REGIONAL_MAP_LINK_REMOVE:
    case GS1_ENGINE_MESSAGE_END_REGIONAL_MAP_SNAPSHOT:
        return true;
    default:
        return false;
    }
}

void Gs1GodotRegionalMapSceneController::handle_engine_message(const Gs1EngineMessage& message)
{
    regional_map_state_reducer_.apply_engine_message(message);

    switch (message.type)
    {
    case GS1_ENGINE_MESSAGE_BEGIN_REGIONAL_MAP_SNAPSHOT:
    case GS1_ENGINE_MESSAGE_REGIONAL_MAP_SITE_UPSERT:
    case GS1_ENGINE_MESSAGE_REGIONAL_MAP_SITE_REMOVE:
    case GS1_ENGINE_MESSAGE_REGIONAL_MAP_LINK_UPSERT:
    case GS1_ENGINE_MESSAGE_REGIONAL_MAP_LINK_REMOVE:
    case GS1_ENGINE_MESSAGE_END_REGIONAL_MAP_SNAPSHOT:
        mark_regional_map_dirty();
        mark_regional_visuals_dirty();
        break;
    default:
        break;
    }
}

void Gs1GodotRegionalMapSceneController::handle_runtime_message_reset()
{
    regional_map_state_reducer_.reset();
    clear_regional_projection_world();
    regional_material_cache_.clear();
    mark_regional_map_dirty();
    mark_regional_visuals_dirty();
}

void Gs1GodotRegionalMapSceneController::refresh_regional_map_if_needed()
{
    if (!regional_map_dirty_ && !regional_visuals_dirty_)
    {
        return;
    }

    refresh_regional_map();
    regional_map_dirty_ = false;
    regional_visuals_dirty_ = false;
}

void Gs1GodotRegionalMapSceneController::mark_regional_map_dirty()
{
    regional_map_dirty_ = true;
}

void Gs1GodotRegionalMapSceneController::mark_regional_visuals_dirty()
{
    regional_visuals_dirty_ = true;
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

void Gs1GodotRegionalMapSceneController::refresh_regional_map()
{
    const auto& regional_state = regional_map_state_reducer_.state();
    if (regional_state.sites.empty())
    {
        clear_regional_projection_world();
        return;
    }

    if (regional_map_dirty_)
    {
        rebuild_regional_map_world(regional_state.sites, regional_state.links);
    }
    if (regional_map_dirty_ || regional_visuals_dirty_)
    {
        update_regional_site_visuals();
    }
}

void Gs1GodotRegionalMapSceneController::rebuild_regional_map_world(
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

void Gs1GodotRegionalMapSceneController::clear_regional_projection_world()
{
    clear_dynamic_children(regional_terrain_root_, String());
    clear_dynamic_children(regional_link_root_, String());
    clear_dynamic_children(regional_site_root_, String());
    regional_terrain_tile_nodes_.clear();
    regional_terrain_grid_line_nodes_.clear();
    regional_link_nodes_.clear();
    regional_site_nodes_.clear();
    regional_site_data_.clear();
}

void Gs1GodotRegionalMapSceneController::reconcile_desert_tiles(const Rect2i& bounds, bool include_grid_lines)
{
    (void)bounds;
    (void)include_grid_lines;
    prune_regional_node_registry(regional_terrain_tile_nodes_, {});
    prune_regional_node_registry(regional_terrain_grid_line_nodes_, {});
}

void Gs1GodotRegionalMapSceneController::reconcile_regional_links(const std::vector<Gs1RuntimeRegionalMapLinkProjection>& links)
{
    if (regional_link_root_ == nullptr)
    {
        return;
    }

    std::unordered_set<std::uint64_t> desired_link_keys {};
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
        if (segment == nullptr)
        {
            continue;
        }

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
        return Color(0.38, 0.35, 0.32);
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

MeshInstance3D* Gs1GodotRegionalMapSceneController::upsert_regional_mesh_node(
    Node3D* container,
    std::unordered_map<std::uint64_t, ObjectID>& registry,
    std::uint64_t stable_key,
    const String& node_name,
    int desired_index)
{
    MeshInstance3D* mesh_node = nullptr;
    auto found = registry.find(stable_key);
    if (found != registry.end())
    {
        mesh_node = resolve_object<MeshInstance3D>(found->second);
    }
    if (mesh_node == nullptr)
    {
        mesh_node = make_mesh_instance3d();
        container->add_child(mesh_node);
        registry[stable_key] = mesh_node->get_instance_id();
    }
    mesh_node->set_name(node_name);
    container->move_child(mesh_node, desired_index);
    return mesh_node;
}

void Gs1GodotRegionalMapSceneController::prune_regional_node_registry(
    std::unordered_map<std::uint64_t, ObjectID>& registry,
    const std::unordered_set<std::uint64_t>& desired_keys)
{
    std::vector<std::uint64_t> stale_keys {};
    stale_keys.reserve(registry.size());
    for (const auto& [stable_key, object_id] : registry)
    {
        (void)object_id;
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

bool Gs1GodotRegionalMapSceneController::try_select_regional_site_from_screen(const Vector2& screen_position)
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

    select_regional_site(nearest_site_id);
    return true;
}

