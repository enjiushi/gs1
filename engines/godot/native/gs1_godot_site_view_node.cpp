#include "gs1_godot_site_view_node.h"

#include "gs1_godot_projection_types.h"
#include "godot_progression_resources.h"

#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/directional_light3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/packed_scene.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/object.hpp>
#include <godot_cpp/core/object_id.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include <algorithm>
#include <vector>

using namespace godot;

namespace
{
constexpr float k_tile_height = 0.05f;
constexpr float k_tile_y_offset = -0.025f;
constexpr float k_worker_marker_height = 0.45f;
constexpr float k_plant_y_offset = 0.35f;
constexpr float k_device_y_offset = 0.4f;

Color color_for_tile(const Gs1RuntimeTileProjection& tile) noexcept
{
    float r = 0.76f;
    float g = 0.69f;
    float b = 0.53f;
    switch (tile.terrain_type_id % 3U)
    {
    case 1U:
        r = 0.84f;
        g = 0.74f;
        b = 0.58f;
        break;
    case 2U:
        r = 0.67f;
        g = 0.58f;
        b = 0.43f;
        break;
    default:
        break;
    }

    const float moisture = std::clamp(tile.moisture / 100.0f, 0.0f, 1.0f);
    const float salinity = std::clamp(tile.soil_salinity / 100.0f, 0.0f, 1.0f);
    const float burial = std::clamp(tile.sand_burial / 100.0f, 0.0f, 1.0f);
    const float excavation = std::clamp(static_cast<float>(tile.visible_excavation_depth) / 3.0f, 0.0f, 1.0f);

    r = std::clamp(r + (0.05f * moisture) + (0.08f * salinity) - (0.10f * excavation), 0.10f, 0.95f);
    g = std::clamp(g + (0.08f * moisture) - (0.06f * salinity) - (0.10f * excavation), 0.10f, 0.95f);
    b = std::clamp(b + (0.03f * moisture) - (0.08f * burial) - (0.06f * excavation), 0.08f, 0.90f);
    return Color(r, g, b, 1.0f);
}

MeshInstance3D* make_box_visual(const Vector3& size, const Color& color)
{
    auto* mesh_instance = memnew(MeshInstance3D);
    Ref<BoxMesh> mesh;
    mesh.instantiate();
    mesh->set_size(size);
    mesh_instance->set_mesh(mesh);

    Ref<StandardMaterial3D> material;
    material.instantiate();
    material->set_albedo(color);
    material->set_roughness(0.92);
    mesh_instance->set_material_override(material);
    return mesh_instance;
}
}

void Gs1SiteViewNode::_bind_methods()
{
    ClassDB::bind_method(D_METHOD("set_runtime_node_path", "path"), &Gs1SiteViewNode::set_runtime_node_path);
    ClassDB::bind_method(D_METHOD("get_runtime_node_path"), &Gs1SiteViewNode::get_runtime_node_path);
    ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "runtime_node_path"), "set_runtime_node_path", "get_runtime_node_path");
}

void Gs1SiteViewNode::_ready()
{
    set_process(true);
    ensure_presenter_created();
}

void Gs1SiteViewNode::_process(double delta)
{
    (void)delta;
    ensure_presenter_created();
    (void)resolve_runtime_node();
}

void Gs1SiteViewNode::ensure_presenter_created()
{
    if (grid_root_ == nullptr)
    {
        grid_root_ = memnew(Node3D);
        grid_root_->set_name("GridRoot");
        add_child(grid_root_);
    }
    if (visual_root_ == nullptr)
    {
        visual_root_ = memnew(Node3D);
        visual_root_->set_name("VisualRoot");
        add_child(visual_root_);
    }
    if (worker_marker_ == nullptr)
    {
        worker_marker_ = memnew(MeshInstance3D);
        Ref<SphereMesh> mesh;
        mesh.instantiate();
        mesh->set_radius(0.35);
        mesh->set_height(0.7);
        worker_marker_->set_mesh(mesh);
        Ref<StandardMaterial3D> material;
        material.instantiate();
        material->set_albedo(Color(0.95, 0.32, 0.18));
        worker_marker_->set_material_override(material);
        worker_marker_->set_visible(false);
        add_child(worker_marker_);
    }

    if (get_node_or_null("BootstrapCamera") == nullptr)
    {
        auto* camera = memnew(Camera3D);
        camera->set_name("BootstrapCamera");
        camera->set_position(Vector3(6.0, 12.0, 10.0));
        camera->set_rotation_degrees(Vector3(-50.0, 30.0, 0.0));
        add_child(camera);
    }

    if (get_node_or_null("BootstrapLight") == nullptr)
    {
        auto* light = memnew(DirectionalLight3D);
        light->set_name("BootstrapLight");
        light->set_rotation_degrees(Vector3(-55.0, 35.0, 0.0));
        add_child(light);
    }
}

Gs1RuntimeNode* Gs1SiteViewNode::resolve_runtime_node() const
{
    if (runtime_node_ == nullptr)
    {
        runtime_node_ = Object::cast_to<Gs1RuntimeNode>(get_node_or_null(runtime_node_path_));
        if (runtime_node_ != nullptr)
        {
            runtime_node_->subscribe_engine_messages(*const_cast<Gs1SiteViewNode*>(this));
        }
    }
    if (runtime_node_ != nullptr)
    {
        return runtime_node_;
    }

    Node* parent = get_parent();
    if (parent == nullptr)
    {
        return nullptr;
    }

    const int child_count = parent->get_child_count();
    for (int child_index = 0; child_index < child_count; ++child_index)
    {
        Node* child = parent->get_child(child_index);
        if (child == nullptr || child == this)
        {
            continue;
        }

        if (auto* runtime = Object::cast_to<Gs1RuntimeNode>(child))
        {
            runtime_node_ = runtime;
            return runtime;
        }
    }

    return nullptr;
}

void Gs1SiteViewNode::set_runtime_node_path(const NodePath& path)
{
    if (runtime_node_ != nullptr)
    {
        runtime_node_->unsubscribe_engine_messages(*this);
    }
    runtime_node_path_ = path;
    runtime_node_ = nullptr;
}

NodePath Gs1SiteViewNode::get_runtime_node_path() const
{
    return runtime_node_path_;
}

bool Gs1SiteViewNode::handles_engine_message(Gs1EngineMessageType type) const noexcept
{
    switch (type)
    {
    case GS1_ENGINE_MESSAGE_BEGIN_SITE_SNAPSHOT:
    case GS1_ENGINE_MESSAGE_SITE_TILE_UPSERT:
    case GS1_ENGINE_MESSAGE_SITE_WORKER_UPDATE:
    case GS1_ENGINE_MESSAGE_SITE_PLANT_VISUAL_UPSERT:
    case GS1_ENGINE_MESSAGE_SITE_PLANT_VISUAL_REMOVE:
    case GS1_ENGINE_MESSAGE_SITE_DEVICE_VISUAL_UPSERT:
    case GS1_ENGINE_MESSAGE_SITE_DEVICE_VISUAL_REMOVE:
    case GS1_ENGINE_MESSAGE_END_SITE_SNAPSHOT:
        return true;
    default:
        return false;
    }
}

void Gs1SiteViewNode::handle_engine_message(const Gs1EngineMessage& message)
{
    switch (message.type)
    {
    case GS1_ENGINE_MESSAGE_BEGIN_SITE_SNAPSHOT:
        apply_site_snapshot_begin(message.payload_as<Gs1EngineMessageSiteSnapshotData>());
        break;
    case GS1_ENGINE_MESSAGE_SITE_TILE_UPSERT:
        apply_site_tile_upsert(message.payload_as<Gs1EngineMessageSiteTileData>());
        break;
    case GS1_ENGINE_MESSAGE_SITE_WORKER_UPDATE:
        apply_site_worker_update(message.payload_as<Gs1EngineMessageWorkerData>());
        break;
    case GS1_ENGINE_MESSAGE_SITE_PLANT_VISUAL_UPSERT:
        apply_site_plant_visual_upsert(message.payload_as<Gs1EngineMessageSitePlantVisualData>());
        break;
    case GS1_ENGINE_MESSAGE_SITE_PLANT_VISUAL_REMOVE:
        apply_site_plant_visual_remove(message.payload_as<Gs1EngineMessageSiteVisualRemoveData>());
        break;
    case GS1_ENGINE_MESSAGE_SITE_DEVICE_VISUAL_UPSERT:
        apply_site_device_visual_upsert(message.payload_as<Gs1EngineMessageSiteDeviceVisualData>());
        break;
    case GS1_ENGINE_MESSAGE_SITE_DEVICE_VISUAL_REMOVE:
        apply_site_device_visual_remove(message.payload_as<Gs1EngineMessageSiteVisualRemoveData>());
        break;
    case GS1_ENGINE_MESSAGE_END_SITE_SNAPSHOT:
        apply_site_snapshot_end();
        break;
    default:
        break;
    }
}

void Gs1SiteViewNode::handle_runtime_message_reset()
{
    clear_visuals();
    clear_tiles();
    active_snapshot_serial_ = 0U;
    reconcile_full_snapshot_ = false;
    worker_seen_in_snapshot_ = false;
    last_width_ = -1;
    last_height_ = -1;
    if (worker_marker_ != nullptr)
    {
        worker_marker_->set_visible(false);
    }
}

void Gs1SiteViewNode::apply_site_snapshot_begin(const Gs1EngineMessageSiteSnapshotData& payload)
{
    last_width_ = payload.width;
    last_height_ = payload.height;
    if (payload.mode != GS1_PROJECTION_MODE_SNAPSHOT)
    {
        return;
    }

    active_snapshot_serial_ += 1U;
    reconcile_full_snapshot_ = true;
    worker_seen_in_snapshot_ = false;
}

void Gs1SiteViewNode::apply_site_snapshot_end()
{
    if (!reconcile_full_snapshot_)
    {
        return;
    }

    auto prune_registry = [this](auto& registry) {
        std::vector<std::uint64_t> stale_ids;
        stale_ids.reserve(registry.size());
        for (const auto& [gameplay_id, record] : registry)
        {
            if (record.last_seen_snapshot_serial != active_snapshot_serial_)
            {
                stale_ids.push_back(gameplay_id);
            }
        }

        for (const std::uint64_t gameplay_id : stale_ids)
        {
            remove_visual_node(registry, gameplay_id);
        }
    };
    prune_registry(plant_visuals_);
    prune_registry(device_visuals_);

    std::vector<std::uint64_t> stale_tile_ids;
    stale_tile_ids.reserve(tile_nodes_.size());
    for (const auto& [tile_id, record] : tile_nodes_)
    {
        if (record.last_seen_snapshot_serial != active_snapshot_serial_)
        {
            stale_tile_ids.push_back(tile_id);
        }
    }

    for (const std::uint64_t tile_id : stale_tile_ids)
    {
        auto found = tile_nodes_.find(tile_id);
        if (found == tile_nodes_.end())
        {
            continue;
        }

        if (Object* object = ObjectDB::get_instance(found->second.object_id))
        {
            if (Node* node = Object::cast_to<Node>(object))
            {
                node->queue_free();
            }
        }
        tile_nodes_.erase(found);
    }

    if (!worker_seen_in_snapshot_ && worker_marker_ != nullptr)
    {
        worker_marker_->set_visible(false);
        worker_marker_->set_meta("gameplay_entity_id", static_cast<int64_t>(0));
    }

    reconcile_full_snapshot_ = false;
}

void Gs1SiteViewNode::apply_site_tile_upsert(const Gs1EngineMessageSiteTileData& payload)
{
    const std::uint64_t tile_id = (static_cast<std::uint64_t>(payload.y) << 32U) | payload.x;
    Object* existing = nullptr;
    auto tile_found = tile_nodes_.find(tile_id);
    if (tile_found != tile_nodes_.end())
    {
        existing = ObjectDB::get_instance(tile_found->second.object_id);
    }
    MeshInstance3D* tile_node = Object::cast_to<MeshInstance3D>(existing);
    if (tile_node == nullptr)
    {
        const Gs1RuntimeTileProjection tile {
            payload.x,
            payload.y,
            payload.terrain_type_id,
            payload.plant_type_id,
            payload.structure_type_id,
            payload.ground_cover_type_id,
            payload.plant_density,
            payload.sand_burial,
            payload.local_wind,
            payload.wind_protection,
            payload.heat_protection,
            payload.dust_protection,
            payload.moisture,
            payload.soil_fertility,
            payload.soil_salinity,
            payload.device_integrity_quantized,
            payload.excavation_depth,
            payload.visible_excavation_depth};
        tile_node = make_box_visual(Vector3(tile_size_, k_tile_height, tile_size_), color_for_tile(tile));
        tile_node->set_name(vformat("Tile_%d_%d", payload.x, payload.y));
        tile_node->set_position(Vector3(payload.x * tile_size_, k_tile_y_offset, payload.y * tile_size_));
        grid_root_->add_child(tile_node);

        TileNodeRecord record {};
        record.object_id = ObjectID(tile_node->get_instance_id());
        record.gameplay_id = tile_id;
        record.last_seen_snapshot_serial = reconcile_full_snapshot_ ? active_snapshot_serial_ : 0U;
        tile_nodes_[tile_id] = record;
    }
    else
    {
        tile_node->set_position(Vector3(payload.x * tile_size_, k_tile_y_offset, payload.y * tile_size_));

        const Gs1RuntimeTileProjection tile {
            payload.x,
            payload.y,
            payload.terrain_type_id,
            payload.plant_type_id,
            payload.structure_type_id,
            payload.ground_cover_type_id,
            payload.plant_density,
            payload.sand_burial,
            payload.local_wind,
            payload.wind_protection,
            payload.heat_protection,
            payload.dust_protection,
            payload.moisture,
            payload.soil_fertility,
            payload.soil_salinity,
            payload.device_integrity_quantized,
            payload.excavation_depth,
            payload.visible_excavation_depth};
        Ref<StandardMaterial3D> material;
        material.instantiate();
        material->set_albedo(color_for_tile(tile));
        material->set_roughness(0.92);
        tile_node->set_material_override(material);
    }

    if (reconcile_full_snapshot_)
    {
        tile_nodes_[tile_id].last_seen_snapshot_serial = active_snapshot_serial_;
    }
}

void Gs1SiteViewNode::apply_site_worker_update(const Gs1EngineMessageWorkerData& payload)
{
    if (worker_marker_ == nullptr)
    {
        return;
    }

    worker_marker_->set_visible(payload.entity_id != 0U);
    worker_marker_->set_meta("gameplay_entity_id", static_cast<int64_t>(payload.entity_id));
    worker_marker_->set_position(Vector3(payload.tile_x * tile_size_, k_worker_marker_height, payload.tile_y * tile_size_));
    if (reconcile_full_snapshot_)
    {
        worker_seen_in_snapshot_ = true;
    }
}

void Gs1SiteViewNode::apply_site_plant_visual_upsert(const Gs1EngineMessageSitePlantVisualData& payload)
{
    const float density = static_cast<float>(payload.density_quantized) / 128.0f;
    float r = 0.18f;
    float g = std::clamp(0.30f + (density * 0.48f), 0.30f, 0.92f);
    float b = 0.18f;
    if ((payload.flags & GS1_SITE_PLANT_VISUAL_FLAG_HAS_AURA) != 0U)
    {
        g += 0.06f;
    }
    if ((payload.flags & GS1_SITE_PLANT_VISUAL_FLAG_HAS_WIND_PROJECTION) != 0U)
    {
        b += 0.08f;
    }
    if ((payload.flags & GS1_SITE_PLANT_VISUAL_FLAG_GROWABLE) != 0U)
    {
        r += 0.02f;
        g += 0.05f;
    }

    upsert_visual_node(
        plant_visuals_,
        payload.visual_id,
        payload.plant_type_id,
        payload.anchor_tile_x,
        payload.anchor_tile_y,
        payload.height_scale,
        Color(std::clamp(r, 0.08f, 0.95f), std::clamp(g, 0.12f, 0.98f), std::clamp(b, 0.08f, 0.95f), 1.0f),
        false,
        payload.footprint_width,
        payload.footprint_height);
}

void Gs1SiteViewNode::apply_site_plant_visual_remove(const Gs1EngineMessageSiteVisualRemoveData& payload)
{
    remove_visual_node(plant_visuals_, payload.visual_id);
}

void Gs1SiteViewNode::apply_site_device_visual_upsert(const Gs1EngineMessageSiteDeviceVisualData& payload)
{
    const float integrity = std::clamp(payload.integrity_normalized, 0.0f, 1.0f);
    float r = 0.32f + (0.32f * integrity);
    float g = 0.31f + (0.22f * integrity);
    float b = 0.34f + (0.18f * integrity);
    if ((payload.flags & GS1_SITE_DEVICE_VISUAL_FLAG_HAS_STORAGE) != 0U)
    {
        r += 0.05f;
        g += 0.04f;
    }
    if ((payload.flags & GS1_SITE_DEVICE_VISUAL_FLAG_IS_CRAFTING_STATION) != 0U)
    {
        r += 0.08f;
        g += 0.03f;
        b -= 0.02f;
    }
    if ((payload.flags & GS1_SITE_DEVICE_VISUAL_FLAG_FIXED_INTEGRITY) != 0U)
    {
        b += 0.10f;
    }

    upsert_visual_node(
        device_visuals_,
        payload.visual_id,
        payload.structure_type_id,
        payload.anchor_tile_x,
        payload.anchor_tile_y,
        payload.height_scale,
        Color(std::clamp(r, 0.10f, 0.98f), std::clamp(g, 0.10f, 0.95f), std::clamp(b, 0.10f, 0.98f), 1.0f),
        true,
        payload.footprint_width,
        payload.footprint_height);
}

void Gs1SiteViewNode::apply_site_device_visual_remove(const Gs1EngineMessageSiteVisualRemoveData& payload)
{
    remove_visual_node(device_visuals_, payload.visual_id);
}

void Gs1SiteViewNode::clear_visuals()
{
    if (worker_marker_ != nullptr)
    {
        worker_marker_->set_visible(false);
    }
    for (const auto& [_, record] : plant_visuals_)
    {
        if (Object* object = ObjectDB::get_instance(record.object_id))
        {
            if (Node* node = Object::cast_to<Node>(object))
            {
                node->queue_free();
            }
        }
    }
    for (const auto& [_, record] : device_visuals_)
    {
        if (Object* object = ObjectDB::get_instance(record.object_id))
        {
            if (Node* node = Object::cast_to<Node>(object))
            {
                node->queue_free();
            }
        }
    }
    plant_visuals_.clear();
    device_visuals_.clear();
}

void Gs1SiteViewNode::clear_tiles()
{
    for (const auto& [_, record] : tile_nodes_)
    {
        if (Object* object = ObjectDB::get_instance(record.object_id))
        {
            if (Node* node = Object::cast_to<Node>(object))
            {
                node->queue_free();
            }
        }
    }
    tile_nodes_.clear();
}

void Gs1SiteViewNode::upsert_visual_node(
    std::unordered_map<std::uint64_t, VisualNodeRecord>& registry,
    std::uint64_t gameplay_id,
    std::uint32_t type_id,
    float anchor_tile_x,
    float anchor_tile_y,
    float height_scale,
    const Color& color,
    bool device_visual,
    std::uint8_t footprint_width,
    std::uint8_t footprint_height)
{
    if (gameplay_id == 0U || visual_root_ == nullptr)
    {
        return;
    }

    MeshInstance3D* mesh_instance = nullptr;
    Node3D* instance_root = nullptr;
    auto found = registry.find(gameplay_id);
    if (found != registry.end())
    {
        mesh_instance = Object::cast_to<MeshInstance3D>(ObjectDB::get_instance(found->second.object_id));
        instance_root = Object::cast_to<Node3D>(ObjectDB::get_instance(found->second.instance_root_id));
    }

    const float size_x = std::max(1.0f, static_cast<float>(footprint_width));
    const float size_z = std::max(1.0f, static_cast<float>(footprint_height));
    const float size_y = std::clamp(height_scale, device_visual ? 0.2f : 0.12f, device_visual ? 2.4f : 2.0f);

    if (mesh_instance == nullptr && instance_root == nullptr)
    {
        instance_root = instantiate_visual_scene(device_visual, type_id);
        if (instance_root != nullptr)
        {
            instance_root->set_name(vformat(device_visual ? "Device_%d" : "Plant_%d", static_cast<int64_t>(gameplay_id)));
            visual_root_->add_child(instance_root);
            instance_root->set_meta("gameplay_id", static_cast<int64_t>(gameplay_id));
            instance_root->set_meta("type_id", static_cast<int64_t>(type_id));
        }
    }

    if (mesh_instance == nullptr && instance_root == nullptr)
    {
        mesh_instance = make_box_visual(Vector3(tile_size_ * size_x, size_y, tile_size_ * size_z), color);
        mesh_instance->set_name(vformat(device_visual ? "Device_%d" : "Plant_%d", static_cast<int64_t>(gameplay_id)));
        visual_root_->add_child(mesh_instance);
    }
    else if (mesh_instance != nullptr)
    {
        Ref<BoxMesh> mesh = mesh_instance->get_mesh();
        if (mesh.is_valid())
        {
            mesh->set_size(Vector3(tile_size_ * size_x, size_y, tile_size_ * size_z));
        }
        Ref<StandardMaterial3D> material;
        material.instantiate();
        material->set_albedo(color);
        material->set_roughness(0.9);
        mesh_instance->set_material_override(material);
    }

    const float y_offset = device_visual ? k_device_y_offset : k_plant_y_offset;
    const Vector3 position(
        (anchor_tile_x + (size_x * 0.5f) - 0.5f) * tile_size_,
        y_offset * size_y,
        (anchor_tile_y + (size_z * 0.5f) - 0.5f) * tile_size_);
    if (mesh_instance != nullptr)
    {
        mesh_instance->set_position(position);
        mesh_instance->set_meta("gameplay_id", static_cast<int64_t>(gameplay_id));
        mesh_instance->set_meta("type_id", static_cast<int64_t>(type_id));
    }
    if (instance_root != nullptr)
    {
        instance_root->set_position(position);
    }

    VisualNodeRecord record {};
    record.object_id = mesh_instance != nullptr ? ObjectID(mesh_instance->get_instance_id()) : ObjectID {};
    record.instance_root_id = instance_root != nullptr ? ObjectID(instance_root->get_instance_id()) : ObjectID {};
    record.gameplay_id = gameplay_id;
    record.type_id = type_id;
    record.anchor_tile_x = anchor_tile_x;
    record.anchor_tile_y = anchor_tile_y;
    record.height_scale = height_scale;
    record.last_seen_snapshot_serial = reconcile_full_snapshot_ ? active_snapshot_serial_ : 0U;
    registry[gameplay_id] = record;
}

Node3D* Gs1SiteViewNode::instantiate_visual_scene(bool device_visual, std::uint32_t type_id) const
{
    const std::uint8_t content_kind = device_visual ? 2U : 0U;
    const String scene_path =
        GodotProgressionResourceDatabase::instance().content_scene_path(content_kind, type_id);
    if (scene_path.is_empty())
    {
        return nullptr;
    }

    ResourceLoader* loader = ResourceLoader::get_singleton();
    if (loader == nullptr)
    {
        return nullptr;
    }

    const Ref<PackedScene> packed_scene = loader->load(scene_path);
    if (packed_scene.is_null())
    {
        return nullptr;
    }

    return Object::cast_to<Node3D>(packed_scene->instantiate());
}

void Gs1SiteViewNode::remove_visual_node(std::unordered_map<std::uint64_t, VisualNodeRecord>& registry, std::uint64_t gameplay_id)
{
    auto found = registry.find(gameplay_id);
    if (found == registry.end())
    {
        return;
    }

    if (Object* object = ObjectDB::get_instance(found->second.object_id))
    {
        if (Node* node = Object::cast_to<Node>(object))
        {
            node->queue_free();
        }
    }
    if (Object* object = ObjectDB::get_instance(found->second.instance_root_id))
    {
        if (Node* node = Object::cast_to<Node>(object))
        {
            node->queue_free();
        }
    }
    registry.erase(found);
}
