#include "gs1_godot_site_view_node.h"

#include "gs1_godot_projection_types.h"
#include "godot_progression_resources.h"

#include <godot_cpp/classes/base_material3d.hpp>
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/directional_light3d.hpp>
#include <godot_cpp/classes/material.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/multi_mesh.hpp>
#include <godot_cpp/classes/multi_mesh_instance3d.hpp>
#include <godot_cpp/classes/packed_scene.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/object.hpp>
#include <godot_cpp/core/object_id.hpp>
#include <godot_cpp/variant/basis.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include <algorithm>
#include <cmath>
#include <functional>
#include <vector>

using namespace godot;

namespace
{
constexpr float k_tile_height = 0.05f;
constexpr float k_tile_y_offset = -0.025f;
constexpr float k_worker_marker_height = 0.45f;
constexpr float k_plant_y_offset = 0.35f;
constexpr float k_device_y_offset = 0.4f;
constexpr std::uint32_t k_tile_batch_key = 1U;
constexpr std::uint16_t k_max_plant_instances_per_visual = 9U;
constexpr std::uint16_t k_min_plant_instances_per_visual = 1U;

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

Ref<StandardMaterial3D> make_standard_material(const Color& color)
{
    Ref<StandardMaterial3D> material;
    material.instantiate();
    material->set_albedo(color);
    material->set_roughness(0.92);
    material->set_flag(BaseMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
    return material;
}

std::uint32_t combine_u32_hash(std::uint32_t lhs, std::uint32_t rhs) noexcept
{
    return lhs ^ (rhs + 0x9e3779b9U + (lhs << 6U) + (lhs >> 2U));
}

std::uint32_t stable_visual_seed(std::uint64_t gameplay_id, std::uint16_t ordinal) noexcept
{
    std::uint64_t value = gameplay_id ^ (static_cast<std::uint64_t>(ordinal + 1U) * 0x9e3779b97f4a7c15ULL);
    value ^= value >> 30U;
    value *= 0xbf58476d1ce4e5b9ULL;
    value ^= value >> 27U;
    value *= 0x94d049bb133111ebULL;
    value ^= value >> 31U;
    return static_cast<std::uint32_t>(value & 0xffffffffU);
}

float stable_unit_float(std::uint32_t seed, std::uint32_t salt) noexcept
{
    std::uint32_t value = seed ^ (salt * 0x9e3779b9U);
    value ^= value >> 16U;
    value *= 0x7feb352dU;
    value ^= value >> 15U;
    value *= 0x846ca68bU;
    value ^= value >> 16U;
    return static_cast<float>(value & 0xffffU) / 65535.0f;
}

Transform3D make_transform(const Vector3& position, const Vector3& scale, float yaw_radians)
{
    Basis basis(Vector3(0.0f, 1.0f, 0.0f), yaw_radians, scale);
    return Transform3D(basis, position);
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
            remove_instanced_visual(registry, gameplay_id);
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

        remove_tile_instance(tile_id);
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
    Ref<BoxMesh> tile_mesh;
    tile_mesh.instantiate();
    tile_mesh->set_size(Vector3(tile_size_, k_tile_height, tile_size_));

    InstancedMeshPart tile_part {};
    tile_part.mesh = tile_mesh;
    tile_part.material = make_standard_material(Color(1.0f, 1.0f, 1.0f, 1.0f));
    tile_part.local_transform = Transform3D();
    tile_part.part_index = 0U;
    InstancedBatch& tile_batch = ensure_batch(tile_batches_, false, k_tile_batch_key, 0U, tile_part);

    auto tile_found = tile_nodes_.find(tile_id);
    if (tile_found == tile_nodes_.end())
    {
        TileNodeRecord record {};
        record.gameplay_id = tile_id;
        record.batch_key = k_tile_batch_key;
        record.instance_index = allocate_batch_slot(tile_batch);
        record.last_seen_snapshot_serial = reconcile_full_snapshot_ ? active_snapshot_serial_ : 0U;
        tile_found = tile_nodes_.emplace(tile_id, record).first;
    }

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

    tile_found->second.color = color_for_tile(tile);
    write_tile_instance(
        tile_found->second,
        payload.x,
        payload.y,
        tile.terrain_type_id,
        tile.visible_excavation_depth,
        tile.plant_density);

    if (reconcile_full_snapshot_)
    {
        tile_found->second.last_seen_snapshot_serial = active_snapshot_serial_;
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

    upsert_instanced_visual(
        plant_visuals_,
        payload.visual_id,
        payload.plant_type_id,
        payload.anchor_tile_x,
        payload.anchor_tile_y,
        payload.height_scale,
        plant_instance_count(payload.density_quantized, payload.footprint_width, payload.footprint_height),
        Color(std::clamp(r, 0.08f, 0.95f), std::clamp(g, 0.12f, 0.98f), std::clamp(b, 0.08f, 0.95f), 1.0f),
        false,
        payload.footprint_width,
        payload.footprint_height);
}

void Gs1SiteViewNode::apply_site_plant_visual_remove(const Gs1EngineMessageSiteVisualRemoveData& payload)
{
    remove_instanced_visual(plant_visuals_, payload.visual_id);
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

    upsert_instanced_visual(
        device_visuals_,
        payload.visual_id,
        payload.structure_type_id,
        payload.anchor_tile_x,
        payload.anchor_tile_y,
        payload.height_scale,
        1U,
        Color(std::clamp(r, 0.10f, 0.98f), std::clamp(g, 0.10f, 0.95f), std::clamp(b, 0.10f, 0.98f), 1.0f),
        true,
        payload.footprint_width,
        payload.footprint_height);
}

void Gs1SiteViewNode::apply_site_device_visual_remove(const Gs1EngineMessageSiteVisualRemoveData& payload)
{
    remove_instanced_visual(device_visuals_, payload.visual_id);
}

void Gs1SiteViewNode::clear_visuals()
{
    if (worker_marker_ != nullptr)
    {
        worker_marker_->set_visible(false);
    }
    for (auto& [_, batch] : plant_batches_)
    {
        if (auto* node = Object::cast_to<Node>(ObjectDB::get_instance(batch.instance_node_id)))
        {
            node->queue_free();
        }
    }
    for (auto& [_, batch] : device_batches_)
    {
        if (auto* node = Object::cast_to<Node>(ObjectDB::get_instance(batch.instance_node_id)))
        {
            node->queue_free();
        }
    }
    plant_visuals_.clear();
    device_visuals_.clear();
    plant_batches_.clear();
    device_batches_.clear();
}

void Gs1SiteViewNode::clear_tiles()
{
    for (auto& [_, batch] : tile_batches_)
    {
        if (Object* object = ObjectDB::get_instance(batch.instance_node_id))
        {
            if (Node* node = Object::cast_to<Node>(object))
            {
                node->queue_free();
            }
        }
    }
    tile_nodes_.clear();
    tile_batches_.clear();
}

void Gs1SiteViewNode::upsert_instanced_visual(
    std::unordered_map<std::uint64_t, VisualNodeRecord>& registry,
    std::uint64_t gameplay_id,
    std::uint32_t type_id,
    float anchor_tile_x,
    float anchor_tile_y,
    float height_scale,
    std::uint16_t instance_count,
    const Color& color,
    bool device_visual,
    std::uint8_t footprint_width,
    std::uint8_t footprint_height)
{
    if (gameplay_id == 0U || visual_root_ == nullptr || instance_count == 0U)
    {
        return;
    }

    auto parts = resolve_instanced_mesh_parts(device_visual, type_id, color);
    if (parts.empty())
    {
        return;
    }
    auto& batches = device_visual ? device_batches_ : plant_batches_;
    std::vector<InstancedBatch*> resolved_batches {};
    resolved_batches.reserve(parts.size());
    for (const InstancedMeshPart& part : parts)
    {
        const std::uint32_t batch_key = visual_batch_key(device_visual, type_id, part.part_index);
        resolved_batches.push_back(&ensure_batch(batches, device_visual, batch_key, type_id, part));
    }

    auto found = registry.find(gameplay_id);
    if (found != registry.end() &&
        (found->second.batch_key != resolved_batches.front()->batch_key ||
            found->second.instance_count != instance_count ||
            found->second.type_id != type_id))
    {
        remove_instanced_visual(registry, gameplay_id);
        found = registry.end();
    }

    if (found == registry.end())
    {
        VisualNodeRecord record {};
        record.gameplay_id = gameplay_id;
        record.type_id = type_id;
        record.batch_key = resolved_batches.front()->batch_key;
        record.first_instance = allocate_batch_slot(*resolved_batches.front());
        for (std::uint32_t slot_index = 1U; slot_index < static_cast<std::uint32_t>(instance_count); ++slot_index)
        {
            (void)allocate_batch_slot(*resolved_batches.front());
        }
        record.instance_count = instance_count;
        record.last_seen_snapshot_serial = reconcile_full_snapshot_ ? active_snapshot_serial_ : 0U;
        found = registry.emplace(gameplay_id, record).first;
    }

    for (std::size_t part_index = 1U; part_index < resolved_batches.size(); ++part_index)
    {
        InstancedBatch& batch = *resolved_batches[part_index];
        for (std::uint32_t slot = found->second.first_instance;
            slot < found->second.first_instance + found->second.instance_count;
            ++slot)
        {
            reserve_batch_slot(batch, slot);
        }
    }

    found->second.anchor_tile_x = anchor_tile_x;
    found->second.anchor_tile_y = anchor_tile_y;
    found->second.height_scale = height_scale;
    found->second.footprint_width = footprint_width;
    found->second.footprint_height = footprint_height;
    found->second.color = color;
    found->second.last_seen_snapshot_serial = reconcile_full_snapshot_ ? active_snapshot_serial_ : 0U;
    set_visual_instance_transforms(
        found->second,
        device_visual,
        0U,
        found->second.instance_count);
}

void Gs1SiteViewNode::remove_instanced_visual(
    std::unordered_map<std::uint64_t, VisualNodeRecord>& registry,
    std::uint64_t gameplay_id)
{
    auto found = registry.find(gameplay_id);
    if (found == registry.end())
    {
        return;
    }

    remove_visual_instance_range(registry, gameplay_id);
    registry.erase(found);
}

std::uint16_t Gs1SiteViewNode::plant_instance_count(
    std::uint16_t density_quantized,
    std::uint8_t footprint_width,
    std::uint8_t footprint_height) const noexcept
{
    const std::uint16_t footprint_area = std::max<std::uint16_t>(
        1U,
        static_cast<std::uint16_t>(footprint_width) * static_cast<std::uint16_t>(footprint_height));
    const float density = std::clamp(static_cast<float>(density_quantized) / 128.0f, 0.0f, 2.0f);
    const auto scaled_count = static_cast<std::uint16_t>(std::ceil(density * 2.0f * static_cast<float>(footprint_area)));
    return std::clamp<std::uint16_t>(scaled_count, k_min_plant_instances_per_visual, k_max_plant_instances_per_visual);
}

std::vector<Gs1SiteViewNode::InstancedMeshPart> Gs1SiteViewNode::resolve_instanced_mesh_parts(
    bool device_visual,
    std::uint32_t type_id,
    const Color& fallback_color) const
{
    std::vector<InstancedMeshPart> parts {};
    const std::uint8_t content_kind = device_visual ? 2U : 0U;
    const String scene_path = GodotProgressionResourceDatabase::instance().content_scene_path(content_kind, type_id);
    if (scene_path.is_empty())
    {
        Ref<BoxMesh> mesh;
        mesh.instantiate();
        mesh->set_size(Vector3(tile_size_, 1.0f, tile_size_));
        parts.push_back(InstancedMeshPart {mesh, make_standard_material(fallback_color), Transform3D(), 0U});
        return parts;
    }

    ResourceLoader* loader = ResourceLoader::get_singleton();
    if (loader == nullptr)
    {
        return parts;
    }

    const Ref<PackedScene> packed_scene = loader->load(scene_path);
    if (packed_scene.is_null())
    {
        return parts;
    }

    Node* root = Object::cast_to<Node>(packed_scene->instantiate());
    if (root == nullptr)
    {
        return parts;
    }

    collect_mesh_parts(root, Transform3D(), parts);
    root->queue_free();
    if (parts.empty())
    {
        Ref<BoxMesh> mesh;
        mesh.instantiate();
        mesh->set_size(Vector3(tile_size_, 1.0f, tile_size_));
        parts.push_back(InstancedMeshPart {mesh, make_standard_material(fallback_color), Transform3D(), 0U});
    }
    return parts;
}

void Gs1SiteViewNode::collect_mesh_parts(
    Node* node,
    const Transform3D& parent_transform,
    std::vector<InstancedMeshPart>& parts) const
{
    if (node == nullptr)
    {
        return;
    }

    Transform3D current_transform = parent_transform;
    if (auto* node3d = Object::cast_to<Node3D>(node))
    {
        current_transform = parent_transform * node3d->get_transform();
    }

    if (auto* mesh_instance = Object::cast_to<MeshInstance3D>(node))
    {
        Ref<Mesh> mesh = mesh_instance->get_mesh();
        if (mesh.is_valid())
        {
            Ref<Material> material = mesh_instance->get_material_override();
            if (material.is_null() && mesh->get_surface_count() > 0)
            {
                material = mesh_instance->get_active_material(0);
            }
            parts.push_back(InstancedMeshPart {
                mesh,
                material.is_valid() ? material : Ref<Material>(make_standard_material(Color(1.0f, 1.0f, 1.0f, 1.0f))),
                current_transform,
                static_cast<std::uint32_t>(parts.size())});
        }
    }

    const int child_count = node->get_child_count();
    for (int child_index = 0; child_index < child_count; ++child_index)
    {
        collect_mesh_parts(node->get_child(child_index), current_transform, parts);
    }
}

Gs1SiteViewNode::InstancedBatch& Gs1SiteViewNode::ensure_batch(
    std::unordered_map<std::uint32_t, InstancedBatch>& batches,
    bool device_visual,
    std::uint32_t batch_key,
    std::uint32_t type_id,
    const InstancedMeshPart& part)
{
    auto found = batches.find(batch_key);
    if (found != batches.end())
    {
        return found->second;
    }

    auto* instance_node = memnew(MultiMeshInstance3D);
    instance_node->set_name(vformat(
        device_visual ? "DeviceInstancedBatch_%d_%d" : "PlantInstancedBatch_%d_%d",
        static_cast<int64_t>(type_id),
        static_cast<int64_t>(part.part_index)));
    instance_node->set_material_override(part.material);

    Ref<MultiMesh> multi_mesh;
    multi_mesh.instantiate();
    multi_mesh->set_transform_format(MultiMesh::TRANSFORM_3D);
    multi_mesh->set_use_colors(true);
    multi_mesh->set_use_custom_data(true);
    multi_mesh->set_mesh(part.mesh);
    multi_mesh->set_instance_count(0);
    multi_mesh->set_visible_instance_count(0);
    instance_node->set_multimesh(multi_mesh);

    Node3D* parent = batch_key == k_tile_batch_key ? grid_root_ : visual_root_;
    if (parent != nullptr)
    {
        parent->add_child(instance_node);
    }

    InstancedBatch batch {};
    batch.batch_key = batch_key;
    batch.type_id = type_id;
    batch.part_index = part.part_index;
    batch.instance_node_id = ObjectID(instance_node->get_instance_id());
    batch.multi_mesh = multi_mesh;
    batch.local_transform = part.local_transform;
    return batches.emplace(batch_key, batch).first->second;
}

std::uint32_t Gs1SiteViewNode::allocate_batch_slot(InstancedBatch& batch)
{
    const std::uint32_t slot = batch.active_count;
    batch.active_count += 1U;
    const std::uint32_t current_count = batch.multi_mesh.is_valid()
        ? static_cast<std::uint32_t>(batch.multi_mesh->get_instance_count())
        : 0U;
    if (batch.multi_mesh.is_valid() && batch.active_count > current_count)
    {
        batch.multi_mesh->set_instance_count(static_cast<int32_t>(batch.active_count));
    }
    refresh_batch_visibility(batch);
    return slot;
}

void Gs1SiteViewNode::reserve_batch_slot(InstancedBatch& batch, std::uint32_t slot)
{
    if (batch.multi_mesh.is_valid() && slot >= static_cast<std::uint32_t>(batch.multi_mesh->get_instance_count()))
    {
        batch.multi_mesh->set_instance_count(static_cast<int32_t>(slot + 1U));
    }
    batch.active_count = std::max(batch.active_count, slot + 1U);
    refresh_batch_visibility(batch);
}

void Gs1SiteViewNode::truncate_batch(InstancedBatch& batch, std::uint32_t active_count)
{
    batch.active_count = active_count;
    refresh_batch_visibility(batch);
}

void Gs1SiteViewNode::refresh_batch_visibility(InstancedBatch& batch)
{
    if (batch.multi_mesh.is_valid())
    {
        batch.multi_mesh->set_visible_instance_count(static_cast<int32_t>(batch.active_count));
    }
    if (auto* node = Object::cast_to<MultiMeshInstance3D>(ObjectDB::get_instance(batch.instance_node_id)))
    {
        node->set_visible(batch.active_count > 0U);
    }
}

void Gs1SiteViewNode::write_tile_instance(
    const TileNodeRecord& record,
    std::uint16_t x,
    std::uint16_t y,
    std::uint32_t terrain_type_id,
    std::uint8_t visible_excavation_depth,
    std::uint8_t plant_density)
{
    auto found = tile_batches_.find(record.batch_key);
    if (found == tile_batches_.end() || !found->second.multi_mesh.is_valid())
    {
        return;
    }

    found->second.multi_mesh->set_instance_transform(
        static_cast<int32_t>(record.instance_index),
        Transform3D(Basis(), Vector3(x * tile_size_, k_tile_y_offset, y * tile_size_)));
    found->second.multi_mesh->set_instance_color(
        static_cast<int32_t>(record.instance_index),
        record.color);
    found->second.multi_mesh->set_instance_custom_data(
        static_cast<int32_t>(record.instance_index),
        Color(
            static_cast<float>(terrain_type_id),
            static_cast<float>(visible_excavation_depth),
            static_cast<float>(plant_density) / 255.0f,
            1.0f));
}

void Gs1SiteViewNode::set_visual_instance_transforms(
    const VisualNodeRecord& record,
    bool device_visual,
    std::uint32_t start_offset,
    std::uint32_t count)
{
    auto& batches = device_visual ? device_batches_ : plant_batches_;
    for (auto& [batch_key, batch] : batches)
    {
        const bool same_visual_batch = batch_key == record.batch_key ||
            (batch.type_id == record.type_id && (batch_key & 0xc0000000U) == (record.batch_key & 0xc0000000U));
        if (!same_visual_batch || !batch.multi_mesh.is_valid())
        {
            continue;
        }

        const std::uint32_t end_offset = std::min<std::uint32_t>(
            start_offset + count,
            static_cast<std::uint32_t>(record.instance_count));
        for (std::uint32_t index = start_offset; index < end_offset; ++index)
        {
            const std::uint32_t slot = record.first_instance + index;
            const Transform3D transform = visual_instance_transform(
                record.gameplay_id,
                static_cast<std::uint16_t>(index),
                record.instance_count,
                device_visual,
                record.anchor_tile_x,
                record.anchor_tile_y,
                record.height_scale,
                record.footprint_width,
                record.footprint_height);
            batch.multi_mesh->set_instance_transform(
                static_cast<int32_t>(slot),
                transform * batch.local_transform);
            batch.multi_mesh->set_instance_color(static_cast<int32_t>(slot), record.color);
            batch.multi_mesh->set_instance_custom_data(
                static_cast<int32_t>(slot),
                Color(
                    static_cast<float>(record.type_id),
                    static_cast<float>(index),
                    static_cast<float>(record.instance_count),
                    record.height_scale));
        }
    }
}

Transform3D Gs1SiteViewNode::visual_instance_transform(
    std::uint64_t gameplay_id,
    std::uint16_t instance_ordinal,
    std::uint16_t instance_count,
    bool device_visual,
    float anchor_tile_x,
    float anchor_tile_y,
    float height_scale,
    std::uint8_t footprint_width,
    std::uint8_t footprint_height) const
{
    const float size_x = std::max(1.0f, static_cast<float>(footprint_width));
    const float size_z = std::max(1.0f, static_cast<float>(footprint_height));
    const float size_y = std::clamp(height_scale, device_visual ? 0.2f : 0.12f, device_visual ? 2.4f : 2.0f);
    const float y_offset = device_visual ? k_device_y_offset : k_plant_y_offset;
    const Vector3 tile_center(
        (anchor_tile_x + (size_x * 0.5f) - 0.5f) * tile_size_,
        y_offset * size_y,
        (anchor_tile_y + (size_z * 0.5f) - 0.5f) * tile_size_);

    if (device_visual)
    {
        return make_transform(tile_center, Vector3(size_x, size_y, size_z), 0.0f);
    }

    const std::uint32_t seed = stable_visual_seed(gameplay_id, instance_ordinal);
    const float radius_x = std::max(0.05f, size_x * tile_size_ * 0.42f);
    const float radius_z = std::max(0.05f, size_z * tile_size_ * 0.42f);
    const float angle = stable_unit_float(seed, 1U) * 6.2831853f;
    const float spread = instance_count > 1U ? std::sqrt(stable_unit_float(seed, 2U)) : 0.0f;
    const Vector3 offset(
        std::cos(angle) * spread * radius_x,
        0.0f,
        std::sin(angle) * spread * radius_z);
    const float scale_jitter = 0.82f + (stable_unit_float(seed, 3U) * 0.36f);
    const float yaw = stable_unit_float(seed, 4U) * 6.2831853f;
    const Vector3 scale(scale_jitter, size_y * scale_jitter, scale_jitter);
    return make_transform(tile_center + offset, scale, yaw);
}

void Gs1SiteViewNode::remove_tile_instance(std::uint64_t tile_id)
{
    auto found = tile_nodes_.find(tile_id);
    if (found == tile_nodes_.end())
    {
        return;
    }

    auto batch_found = tile_batches_.find(found->second.batch_key);
    if (batch_found == tile_batches_.end())
    {
        tile_nodes_.erase(found);
        return;
    }

    InstancedBatch& batch = batch_found->second;
    const std::uint32_t removed_slot = found->second.instance_index;
    const std::uint32_t last_slot = batch.active_count > 0U ? batch.active_count - 1U : 0U;
    if (removed_slot != last_slot)
    {
        for (auto& [other_tile_id, record] : tile_nodes_)
        {
            if (other_tile_id == tile_id || record.batch_key != found->second.batch_key || record.instance_index != last_slot)
            {
                continue;
            }

            record.instance_index = removed_slot;
            if (batch.multi_mesh.is_valid())
            {
                batch.multi_mesh->set_instance_transform(
                    static_cast<int32_t>(removed_slot),
                    batch.multi_mesh->get_instance_transform(static_cast<int32_t>(last_slot)));
                batch.multi_mesh->set_instance_color(
                    static_cast<int32_t>(removed_slot),
                    batch.multi_mesh->get_instance_color(static_cast<int32_t>(last_slot)));
                batch.multi_mesh->set_instance_custom_data(
                    static_cast<int32_t>(removed_slot),
                    batch.multi_mesh->get_instance_custom_data(static_cast<int32_t>(last_slot)));
            }
            break;
        }
    }

    truncate_batch(batch, batch.active_count > 0U ? batch.active_count - 1U : 0U);
    tile_nodes_.erase(found);
}

void Gs1SiteViewNode::remove_visual_instance_range(
    std::unordered_map<std::uint64_t, VisualNodeRecord>& registry,
    std::uint64_t gameplay_id)
{
    auto found = registry.find(gameplay_id);
    if (found == registry.end())
    {
        return;
    }

    const bool device_visual = &registry == &device_visuals_;
    auto& batches = device_visual ? device_batches_ : plant_batches_;
    VisualNodeRecord& removed_record = found->second;
    auto primary_batch_found = batches.find(removed_record.batch_key);
    if (primary_batch_found == batches.end())
    {
        return;
    }

    std::uint32_t current_count = removed_record.instance_count;
    while (current_count > 0U)
    {
        const std::uint32_t removed_slot = removed_record.first_instance + current_count - 1U;
        const std::uint32_t last_slot = primary_batch_found->second.active_count > 0U
            ? primary_batch_found->second.active_count - 1U
            : 0U;

        VisualNodeRecord* moved_record = removed_slot != last_slot
            ? visual_record_for_slot(registry, removed_record.batch_key, last_slot)
            : nullptr;

        for (auto& [batch_key, batch] : batches)
        {
            const bool same_visual_batch = batch_key == removed_record.batch_key ||
                (batch.type_id == removed_record.type_id && (batch_key & 0xc0000000U) == (removed_record.batch_key & 0xc0000000U));
            if (!same_visual_batch || !batch.multi_mesh.is_valid())
            {
                continue;
            }

            if (removed_slot != last_slot)
            {
                batch.multi_mesh->set_instance_transform(
                    static_cast<int32_t>(removed_slot),
                    batch.multi_mesh->get_instance_transform(static_cast<int32_t>(last_slot)));
                batch.multi_mesh->set_instance_color(
                    static_cast<int32_t>(removed_slot),
                    batch.multi_mesh->get_instance_color(static_cast<int32_t>(last_slot)));
                batch.multi_mesh->set_instance_custom_data(
                    static_cast<int32_t>(removed_slot),
                    batch.multi_mesh->get_instance_custom_data(static_cast<int32_t>(last_slot)));
            }
            truncate_batch(batch, batch.active_count > 0U ? batch.active_count - 1U : 0U);
        }

        if (moved_record != nullptr)
        {
            moved_record->first_instance = removed_slot - (last_slot - moved_record->first_instance);
            set_visual_instance_transforms(*moved_record, device_visual, 0U, moved_record->instance_count);
        }
        --current_count;
    }
}

std::uint32_t Gs1SiteViewNode::visual_batch_key(bool device_visual, std::uint32_t type_id, std::uint32_t part_index) const noexcept
{
    std::uint32_t hash = device_visual ? 0x80000000U : 0x40000000U;
    hash = combine_u32_hash(hash, type_id);
    hash = combine_u32_hash(hash, part_index);
    return hash;
}

Gs1SiteViewNode::VisualNodeRecord* Gs1SiteViewNode::visual_record_for_slot(
    std::unordered_map<std::uint64_t, VisualNodeRecord>& registry,
    std::uint32_t batch_key,
    std::uint32_t slot)
{
    for (auto& [_, record] : registry)
    {
        const std::uint32_t end_slot = record.first_instance + record.instance_count;
        if (record.batch_key == batch_key && slot >= record.first_instance && slot < end_slot)
        {
            return &record;
        }
    }
    return nullptr;
}
