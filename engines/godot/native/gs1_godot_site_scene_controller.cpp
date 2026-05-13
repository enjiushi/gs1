#include "gs1_godot_site_scene_controller.h"

#include "gs1_godot_controller_context.h"
#include "godot_progression_resources.h"
#include "content/defs/plant_defs.h"
#include "content/defs/structure_defs.h"

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
#include <godot_cpp/classes/texture2d.hpp>
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
#include <optional>
#include <vector>

using namespace godot;

namespace
{
constexpr float k_tile_height = 0.05f;
constexpr float k_tile_min_height = 0.2f;
constexpr float k_tile_height_from_fertility = 1.1f;
constexpr float k_tile_excavation_depth_step = 0.08f;
constexpr float k_worker_marker_height = 0.55f;
constexpr float k_plant_y_offset = 0.35f;
constexpr float k_device_y_offset = 0.4f;
constexpr std::uint32_t k_tile_batch_key_mask = 0x20000000U;
constexpr std::uint32_t k_plant_batch_key_mask = 0x40000000U;
constexpr std::uint32_t k_device_batch_key_mask = 0x80000000U;
constexpr std::uint32_t k_tile_texture_band_count = 4U;
constexpr std::uint32_t k_tile_band_highway = k_tile_texture_band_count;
constexpr std::uint32_t k_highway_terrain_type_id = 9001U;
constexpr std::uint16_t k_max_plant_instances_per_visual = 9U;
constexpr std::uint16_t k_min_plant_instances_per_visual = 1U;
constexpr const char* k_tile_texture_paths[k_tile_texture_band_count] = {
    "res://assets/terrain/textures/g_pal_00_color.png",
    "res://assets/terrain/textures/dirt (2).png",
    "res://assets/terrain/textures/dirt (1).png",
    "res://assets/terrain/textures/dirt (3).png"};
constexpr const char* k_highway_texture_path = "res://assets/terrain/textures/road dirt.png";
constexpr const char* k_worker_placeholder_scene_path = "res://scenes/site_player_placeholder.tscn";

Color color_for_tile(const Gs1EngineMessageSiteTileData& tile) noexcept
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
    const float fertility = std::clamp(tile.soil_fertility / 100.0f, 0.0f, 1.0f);
    const float salinity = std::clamp(tile.soil_salinity / 100.0f, 0.0f, 1.0f);
    const float burial = std::clamp(tile.sand_burial / 100.0f, 0.0f, 1.0f);
    const float excavation = std::clamp(static_cast<float>(tile.visible_excavation_depth) / 3.0f, 0.0f, 1.0f);

    r = std::clamp(
        r + (0.05f * moisture) + (0.03f * fertility) + (0.08f * salinity) - (0.10f * excavation),
        0.10f,
        0.95f);
    g = std::clamp(
        g + (0.08f * moisture) + (0.17f * fertility) - (0.06f * salinity) - (0.10f * excavation),
        0.10f,
        0.98f);
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

Ref<StandardMaterial3D> make_textured_tile_material(const String& texture_path)
{
    Ref<StandardMaterial3D> material = make_standard_material(Color(1.0f, 1.0f, 1.0f, 1.0f));
    ResourceLoader* loader = ResourceLoader::get_singleton();
    if (loader == nullptr)
    {
        return material;
    }

    const Ref<Texture2D> texture = loader->load(texture_path);
    if (texture.is_valid())
    {
        material->set_texture(BaseMaterial3D::TEXTURE_ALBEDO, texture);
    }
    return material;
}

float lerp_float(float lhs, float rhs, float t) noexcept
{
    return lhs + ((rhs - lhs) * t);
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

std::uint64_t tile_gameplay_id(std::uint16_t x, std::uint16_t y) noexcept
{
    return (static_cast<std::uint64_t>(y) << 32U) | x;
}

bool is_tile_batch_key(std::uint32_t batch_key) noexcept
{
    return (batch_key & 0xe0000000U) == k_tile_batch_key_mask;
}

std::uint16_t quantize_density(float density) noexcept
{
    const float quantized = std::round(std::clamp(density, 0.0f, 2.0f) * 128.0f);
    return static_cast<std::uint16_t>(std::clamp(quantized, 1.0f, 255.0f));
}

float plant_height_scale(const gs1::PlantDef& plant_def, float plant_density) noexcept
{
    float base_scale = 0.8f;
    switch (plant_def.height_class)
    {
    case gs1::PlantHeightClass::Low:
        base_scale = 0.8f;
        break;
    case gs1::PlantHeightClass::Medium:
        base_scale = 1.15f;
        break;
    case gs1::PlantHeightClass::Tall:
        base_scale = 1.55f;
        break;
    case gs1::PlantHeightClass::None:
    default:
        base_scale = 0.65f;
        break;
    }

    return base_scale * std::clamp(0.65f + (plant_density * 0.55f), 0.65f, 1.75f);
}

float device_height_scale(const gs1::StructureDef& structure_def, float integrity) noexcept
{
    const float footprint_scale = std::max(
        1.0f,
        static_cast<float>(std::max(structure_def.footprint_width, structure_def.footprint_height)));
    return std::clamp((0.75f + (0.35f * integrity)) * footprint_scale, 0.75f, 2.75f);
}

std::uint64_t plant_visual_id_for_tile(const Gs1SiteTileView& tile) noexcept
{
    return tile.tile_entity_id != 0U ? tile.tile_entity_id : tile_gameplay_id(
        static_cast<std::uint16_t>(tile.tile_x),
        static_cast<std::uint16_t>(tile.tile_y));
}

std::uint64_t device_visual_id_for_tile(const Gs1SiteTileView& tile) noexcept
{
    return tile.device_entity_id != 0U
        ? tile.device_entity_id
        : (tile_gameplay_id(static_cast<std::uint16_t>(tile.tile_x), static_cast<std::uint16_t>(tile.tile_y)) | 0x1000000000000000ULL);
}

Gs1EngineMessageSiteTileData make_tile_payload(const Gs1SiteTileView& tile) noexcept
{
    Gs1EngineMessageSiteTileData payload {};
    payload.x = static_cast<std::uint16_t>(std::max(tile.tile_x, 0));
    payload.y = static_cast<std::uint16_t>(std::max(tile.tile_y, 0));
    payload.terrain_type_id = tile.terrain_type_id;
    payload.plant_type_id = tile.plant_id;
    payload.structure_type_id = tile.structure_id;
    payload.ground_cover_type_id = tile.ground_cover_type_id;
    payload.plant_density = tile.plant_density;
    payload.sand_burial = tile.sand_burial;
    payload.local_wind = tile.local_wind;
    payload.wind_protection = tile.plant_wind_protection + tile.device_wind_protection;
    payload.heat_protection = tile.plant_heat_protection + tile.device_heat_protection;
    payload.dust_protection = tile.plant_dust_protection + tile.device_dust_protection;
    payload.moisture = tile.moisture;
    payload.soil_fertility = tile.soil_fertility;
    payload.soil_salinity = tile.soil_salinity;
    const float quantized_integrity =
        std::round(std::clamp(tile.device_integrity, 0.0f, 1.0f) * 100.0f);
    payload.device_integrity_quantized = static_cast<std::uint16_t>(
        std::clamp(quantized_integrity, 0.0f, 100.0f));
    payload.excavation_depth = tile.excavation_depth;
    payload.visible_excavation_depth = tile.excavation_depth;
    return payload;
}

std::optional<Gs1EngineMessageSitePlantVisualData> make_plant_visual_payload(const Gs1SiteTileView& tile)
{
    if (tile.plant_id == 0U)
    {
        return std::nullopt;
    }

    const auto* plant_def = gs1::find_plant_def(gs1::PlantId {tile.plant_id});
    if (plant_def == nullptr)
    {
        return std::nullopt;
    }

    Gs1EngineMessageSitePlantVisualData payload {};
    payload.visual_id = plant_visual_id_for_tile(tile);
    payload.plant_type_id = tile.plant_id;
    payload.anchor_tile_x = static_cast<float>(tile.tile_x);
    payload.anchor_tile_y = static_cast<float>(tile.tile_y);
    payload.height_scale = plant_height_scale(*plant_def, tile.plant_density);
    payload.density_quantized = quantize_density(tile.plant_density);
    payload.footprint_width = plant_def->footprint_width;
    payload.footprint_height = plant_def->footprint_height;
    payload.height_class = static_cast<std::uint8_t>(plant_def->height_class);
    payload.focus = static_cast<std::uint8_t>(plant_def->focus);
    payload.flags = 0U;
    if (gs1::plant_focus_has_aura(plant_def->focus))
    {
        payload.flags |= GS1_SITE_PLANT_VISUAL_FLAG_HAS_AURA;
    }
    if (gs1::plant_focus_has_wind_projection(plant_def->focus))
    {
        payload.flags |= GS1_SITE_PLANT_VISUAL_FLAG_HAS_WIND_PROJECTION;
    }
    if (plant_def->growable)
    {
        payload.flags |= GS1_SITE_PLANT_VISUAL_FLAG_GROWABLE;
    }
    return payload;
}

std::optional<Gs1EngineMessageSiteDeviceVisualData> make_device_visual_payload(const Gs1SiteTileView& tile)
{
    if (tile.structure_id == 0U)
    {
        return std::nullopt;
    }

    const auto* structure_def = gs1::find_structure_def(gs1::StructureId {tile.structure_id});
    if (structure_def == nullptr)
    {
        return std::nullopt;
    }

    Gs1EngineMessageSiteDeviceVisualData payload {};
    payload.visual_id = device_visual_id_for_tile(tile);
    payload.structure_type_id = tile.structure_id;
    payload.anchor_tile_x = static_cast<float>(tile.tile_x);
    payload.anchor_tile_y = static_cast<float>(tile.tile_y);
    payload.integrity_normalized = std::clamp(tile.device_integrity, 0.0f, 1.0f);
    payload.height_scale = device_height_scale(*structure_def, payload.integrity_normalized);
    payload.footprint_width = structure_def->footprint_width;
    payload.footprint_height = structure_def->footprint_height;
    payload.flags = 0U;
    if (structure_def->grants_storage)
    {
        payload.flags |= GS1_SITE_DEVICE_VISUAL_FLAG_HAS_STORAGE;
    }
    if (structure_def->crafting_station_kind != gs1::CraftingStationKind::None)
    {
        payload.flags |= GS1_SITE_DEVICE_VISUAL_FLAG_IS_CRAFTING_STATION;
    }
    if (tile.device_fixed_integrity != 0U)
    {
        payload.flags |= GS1_SITE_DEVICE_VISUAL_FLAG_FIXED_INTEGRITY;
    }
    return payload;
}

Gs1EngineMessageWorkerData make_worker_payload(
    const Gs1WorkerStateView& worker,
    const Gs1SiteActionView& action) noexcept
{
    Gs1EngineMessageWorkerData payload {};
    payload.entity_id = worker.worker_entity_id;
    payload.tile_x = worker.world_x;
    payload.tile_y = worker.world_y;
    payload.facing_degrees = worker.facing_degrees;
    payload.health_normalized = worker.health;
    payload.hydration_normalized = worker.hydration;
    payload.energy_normalized = worker.energy_cap > 0.0f ? (worker.energy / worker.energy_cap) : 0.0f;
    payload.flags = worker.is_sheltered != 0U ? 1U : 0U;
    payload.current_action_kind = action.action_kind;
    payload.reserved0 = 0U;
    return payload;
}
}

void Gs1GodotSiteSceneController::_bind_methods()
{
}

void Gs1GodotSiteSceneController::_ready()
{
    ensure_presenter_created();
    cache_adapter_service();
    refresh_from_game_state_view();
}

void Gs1GodotSiteSceneController::_exit_tree()
{
    if (adapter_service_ != nullptr)
    {
        adapter_service_->unsubscribe_all(*this);
        adapter_service_ = nullptr;
    }
}

void Gs1GodotSiteSceneController::ensure_presenter_created()
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
        worker_marker_ = memnew(Node3D);
        worker_marker_->set_name("WorkerRoot");
        worker_marker_->set_visible(false);
        if (ResourceLoader* loader = ResourceLoader::get_singleton())
        {
            const Ref<PackedScene> worker_scene = loader->load(k_worker_placeholder_scene_path);
            if (worker_scene.is_valid())
            {
                if (Node* scene_instance = Object::cast_to<Node>(worker_scene->instantiate()))
                {
                    worker_marker_->add_child(scene_instance);
                }
            }
        }
        if (worker_marker_->get_child_count() == 0)
        {
            auto* fallback_body = make_box_visual(Vector3(0.42f, 0.62f, 0.28f), Color(0.79, 0.56, 0.41));
            fallback_body->set_position(Vector3(0.0, 0.38, 0.0));
            worker_marker_->add_child(fallback_body);

            auto* fallback_head = memnew(MeshInstance3D);
            Ref<SphereMesh> head_mesh;
            head_mesh.instantiate();
            head_mesh->set_radius(0.18);
            head_mesh->set_height(0.36);
            fallback_head->set_mesh(head_mesh);
            fallback_head->set_material_override(make_standard_material(Color(0.92, 0.78, 0.63)));
            fallback_head->set_position(Vector3(0.0, 0.84, 0.0));
            worker_marker_->add_child(fallback_head);
        }
        if (visual_root_ != nullptr)
        {
            visual_root_->add_child(worker_marker_);
        }
        else
        {
            add_child(worker_marker_);
        }
    }

    {
        bootstrap_camera_ = Object::cast_to<Camera3D>(get_node_or_null("BootstrapCamera"));
    }
    if (bootstrap_camera_ == nullptr)
    {
        auto* camera = memnew(Camera3D);
        camera->set_name("BootstrapCamera");
        camera->set_position(Vector3(6.0, 12.0, 10.0));
        camera->set_rotation_degrees(Vector3(-50.0, 30.0, 0.0));
        camera->set_current(true);
        add_child(camera);
        bootstrap_camera_ = camera;
    }
    else
    {
        bootstrap_camera_->set_current(true);
    }
    bootstrap_camera_->set_current(true);

    if (get_node_or_null("BootstrapLight") == nullptr)
    {
        auto* light = memnew(DirectionalLight3D);
        light->set_name("BootstrapLight");
        light->set_rotation_degrees(Vector3(-55.0, 35.0, 0.0));
        add_child(light);
    }

    refresh_camera_frame();
}

void Gs1GodotSiteSceneController::cache_adapter_service()
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

bool Gs1GodotSiteSceneController::handles_engine_message(Gs1EngineMessageType type) const noexcept
{
    return type == GS1_ENGINE_MESSAGE_PRESENTATION_DIRTY ||
        type == GS1_ENGINE_MESSAGE_SET_APP_STATE;
}

void Gs1GodotSiteSceneController::handle_engine_message(const Gs1EngineMessage& message)
{
    if (message.type == GS1_ENGINE_MESSAGE_PRESENTATION_DIRTY ||
        message.type == GS1_ENGINE_MESSAGE_SET_APP_STATE)
    {
        refresh_from_game_state_view();
    }
}

void Gs1GodotSiteSceneController::handle_runtime_message_reset()
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
    refresh_camera_frame();
}

void Gs1GodotSiteSceneController::refresh_from_game_state_view()
{
    if (adapter_service_ == nullptr)
    {
        return;
    }

    Gs1GameStateView view {};
    if (!adapter_service_->get_game_state_view(view) ||
        view.has_active_site == 0U ||
        view.active_site == nullptr)
    {
        clear_visuals();
        clear_tiles();
        active_snapshot_serial_ = 0U;
        reconcile_full_snapshot_ = false;
        worker_seen_in_snapshot_ = false;
        last_width_ = -1;
        last_height_ = -1;
        refresh_camera_frame();
        return;
    }

    const Gs1SiteStateView& site = *view.active_site;
    last_width_ = static_cast<std::int32_t>(site.tile_width);
    last_height_ = static_cast<std::int32_t>(site.tile_height);
    refresh_camera_frame();

    active_snapshot_serial_ += 1U;
    reconcile_full_snapshot_ = true;
    worker_seen_in_snapshot_ = false;

    for (std::uint32_t tile_index = 0; tile_index < site.tile_count; ++tile_index)
    {
        Gs1SiteTileView tile {};
        if (!adapter_service_->query_site_tile_view(tile_index, tile))
        {
            continue;
        }

        apply_site_tile_upsert(make_tile_payload(tile));
        if (const auto plant_payload = make_plant_visual_payload(tile); plant_payload.has_value())
        {
            apply_site_plant_visual_upsert(plant_payload.value());
        }
        if (const auto device_payload = make_device_visual_payload(tile); device_payload.has_value())
        {
            apply_site_device_visual_upsert(device_payload.value());
        }
    }

    if (site.worker.worker_entity_id != 0U)
    {
        apply_site_worker_update(make_worker_payload(site.worker, site.action));
    }

    apply_site_snapshot_end();
}

void Gs1GodotSiteSceneController::apply_site_snapshot_end()
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
    refresh_all_visual_transforms();
    refresh_worker_transform();
}

void Gs1GodotSiteSceneController::apply_site_tile_upsert(const Gs1EngineMessageSiteTileData& payload)
{
    const std::uint64_t tile_id = tile_gameplay_id(payload.x, payload.y);

    auto tile_found = tile_nodes_.find(tile_id);

    TileNodeRecord updated_record {};
    if (tile_found != tile_nodes_.end())
    {
        updated_record = tile_found->second;
    }
    updated_record.gameplay_id = tile_id;
    updated_record.x = payload.x;
    updated_record.y = payload.y;
    updated_record.terrain_type_id = payload.terrain_type_id;
    updated_record.soil_fertility = payload.soil_fertility;
    updated_record.moisture = payload.moisture;
    updated_record.soil_salinity = payload.soil_salinity;
    updated_record.sand_burial = payload.sand_burial;
    updated_record.plant_density = payload.plant_density;
    updated_record.visible_excavation_depth = payload.visible_excavation_depth;
    updated_record.color = color_for_tile(payload);
    updated_record.last_seen_snapshot_serial = reconcile_full_snapshot_ ? active_snapshot_serial_ : 0U;

    const std::uint32_t next_batch_key = tile_batch_key_for_record(updated_record);
    if (tile_found != tile_nodes_.end() && tile_found->second.batch_key != next_batch_key)
    {
        remove_tile_instance(tile_id);
        tile_found = tile_nodes_.end();
    }

    if (tile_found == tile_nodes_.end())
    {
        const auto tile_parts = resolve_tile_mesh_parts(next_batch_key);
        if (tile_parts.empty())
        {
            return;
        }

        InstancedBatch& tile_batch = ensure_batch(tile_batches_, false, next_batch_key, 0U, tile_parts.front());
        updated_record.batch_key = next_batch_key;
        updated_record.instance_index = allocate_batch_slot(tile_batch);
        tile_found = tile_nodes_.emplace(tile_id, updated_record).first;
    }
    else
    {
        tile_found->second = updated_record;
    }

    write_tile_instance(tile_found->second, !reconcile_full_snapshot_);

    if (reconcile_full_snapshot_)
    {
        tile_found->second.last_seen_snapshot_serial = active_snapshot_serial_;
    }
}

void Gs1GodotSiteSceneController::apply_site_worker_update(const Gs1EngineMessageWorkerData& payload)
{
    if (worker_marker_ == nullptr)
    {
        return;
    }

    worker_marker_->set_visible(payload.entity_id != 0U);
    worker_marker_->set_meta("gameplay_entity_id", static_cast<int64_t>(payload.entity_id));
    worker_marker_->set_position(
        Vector3(
            payload.tile_x * tile_size_,
            terrain_surface_height_at(payload.tile_x, payload.tile_y) + k_worker_marker_height,
            payload.tile_y * tile_size_));
    if (reconcile_full_snapshot_)
    {
        worker_seen_in_snapshot_ = true;
    }
}

void Gs1GodotSiteSceneController::apply_site_plant_visual_upsert(const Gs1EngineMessageSitePlantVisualData& payload)
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

void Gs1GodotSiteSceneController::apply_site_device_visual_upsert(const Gs1EngineMessageSiteDeviceVisualData& payload)
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

void Gs1GodotSiteSceneController::clear_visuals()
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

void Gs1GodotSiteSceneController::clear_tiles()
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

std::uint32_t Gs1GodotSiteSceneController::tile_texture_band(
    std::uint32_t terrain_type_id,
    float soil_fertility) const noexcept
{
    if (terrain_type_id == k_highway_terrain_type_id)
    {
        return k_tile_band_highway;
    }

    const float normalized_fertility = std::clamp(soil_fertility / 100.0f, 0.0f, 0.9999f);
    return std::min<std::uint32_t>(
        static_cast<std::uint32_t>(normalized_fertility * static_cast<float>(k_tile_texture_band_count)),
        k_tile_texture_band_count - 1U);
}

std::uint32_t Gs1GodotSiteSceneController::tile_batch_key_for_record(const TileNodeRecord& record) const noexcept
{
    return k_tile_batch_key_mask | tile_texture_band(record.terrain_type_id, record.soil_fertility);
}

std::vector<Gs1GodotSiteSceneController::InstancedMeshPart> Gs1GodotSiteSceneController::resolve_tile_mesh_parts(
    std::uint32_t batch_key) const
{
    std::vector<InstancedMeshPart> parts {};
    Ref<BoxMesh> mesh;
    mesh.instantiate();
    mesh->set_size(Vector3(1.0f, 1.0f, 1.0f));

    const std::uint32_t tile_band = batch_key & 0x0000ffffU;
    const String texture_path = tile_band == k_tile_band_highway
        ? String(k_highway_texture_path)
        : String(k_tile_texture_paths[std::min<std::uint32_t>(tile_band, k_tile_texture_band_count - 1U)]);
    parts.push_back(InstancedMeshPart {
        mesh,
        make_textured_tile_material(texture_path),
        Transform3D(),
        0U});
    return parts;
}

void Gs1GodotSiteSceneController::upsert_instanced_visual(
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

void Gs1GodotSiteSceneController::remove_instanced_visual(
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

std::uint16_t Gs1GodotSiteSceneController::plant_instance_count(
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

std::vector<Gs1GodotSiteSceneController::InstancedMeshPart> Gs1GodotSiteSceneController::resolve_instanced_mesh_parts(
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

void Gs1GodotSiteSceneController::collect_mesh_parts(
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

Gs1GodotSiteSceneController::InstancedBatch& Gs1GodotSiteSceneController::ensure_batch(
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
    if (is_tile_batch_key(batch_key))
    {
        instance_node->set_name(vformat(
            "TerrainInstancedBatch_%d",
            static_cast<int64_t>(batch_key & 0x0000ffffU)));
    }
    else
    {
        instance_node->set_name(vformat(
            device_visual ? "DeviceInstancedBatch_%d_%d" : "PlantInstancedBatch_%d_%d",
            static_cast<int64_t>(type_id),
            static_cast<int64_t>(part.part_index)));
    }
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

    Node3D* parent = is_tile_batch_key(batch_key) ? grid_root_ : visual_root_;
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

std::uint32_t Gs1GodotSiteSceneController::allocate_batch_slot(InstancedBatch& batch)
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

void Gs1GodotSiteSceneController::reserve_batch_slot(InstancedBatch& batch, std::uint32_t slot)
{
    if (batch.multi_mesh.is_valid() && slot >= static_cast<std::uint32_t>(batch.multi_mesh->get_instance_count()))
    {
        batch.multi_mesh->set_instance_count(static_cast<int32_t>(slot + 1U));
    }
    batch.active_count = std::max(batch.active_count, slot + 1U);
    refresh_batch_visibility(batch);
}

void Gs1GodotSiteSceneController::truncate_batch(InstancedBatch& batch, std::uint32_t active_count)
{
    batch.active_count = active_count;
    refresh_batch_visibility(batch);
}

void Gs1GodotSiteSceneController::refresh_batch_visibility(InstancedBatch& batch)
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

float Gs1GodotSiteSceneController::terrain_surface_height_for_tile(const TileNodeRecord& record) const noexcept
{
    const float fertility_height =
        std::clamp(record.soil_fertility / 100.0f, 0.0f, 1.0f) * k_tile_height_from_fertility;
    const float burial_lift = std::clamp(record.sand_burial / 100.0f, 0.0f, 1.0f) * 0.18f;
    const float excavation_cut =
        std::clamp(static_cast<float>(record.visible_excavation_depth), 0.0f, 3.0f) * k_tile_excavation_depth_step;
    return std::max(k_tile_height, k_tile_min_height + fertility_height + burial_lift - excavation_cut);
}

const Gs1GodotSiteSceneController::TileNodeRecord* Gs1GodotSiteSceneController::tile_record_at(
    std::uint16_t x,
    std::uint16_t y) const
{
    const auto found = tile_nodes_.find(tile_gameplay_id(x, y));
    return found == tile_nodes_.end() ? nullptr : &found->second;
}

float Gs1GodotSiteSceneController::terrain_surface_height_at(std::uint16_t x, std::uint16_t y) const noexcept
{
    if (const TileNodeRecord* record = tile_record_at(x, y))
    {
        return terrain_surface_height_for_tile(*record);
    }
    return 0.0f;
}

float Gs1GodotSiteSceneController::terrain_surface_height_at(float tile_x, float tile_y) const noexcept
{
    const float clamped_x = std::max(0.0f, tile_x);
    const float clamped_y = std::max(0.0f, tile_y);
    const auto x0 = static_cast<std::uint16_t>(std::floor(clamped_x));
    const auto y0 = static_cast<std::uint16_t>(std::floor(clamped_y));
    const auto x1 = static_cast<std::uint16_t>(std::ceil(clamped_x));
    const auto y1 = static_cast<std::uint16_t>(std::ceil(clamped_y));
    const float tx = clamped_x - static_cast<float>(x0);
    const float ty = clamped_y - static_cast<float>(y0);

    const float h00 = terrain_surface_height_at(x0, y0);
    const float h10 = terrain_surface_height_at(x1, y0);
    const float h01 = terrain_surface_height_at(x0, y1);
    const float h11 = terrain_surface_height_at(x1, y1);
    return lerp_float(lerp_float(h00, h10, tx), lerp_float(h01, h11, tx), ty);
}

void Gs1GodotSiteSceneController::write_tile_instance(
    const TileNodeRecord& record,
    bool refresh_visual_dependents)
{
    auto found = tile_batches_.find(record.batch_key);
    if (found == tile_batches_.end() || !found->second.multi_mesh.is_valid())
    {
        return;
    }

    const float terrain_height = terrain_surface_height_for_tile(record);
    found->second.multi_mesh->set_instance_transform(
        static_cast<int32_t>(record.instance_index),
        make_transform(
            Vector3(
                record.x * tile_size_,
                terrain_height * 0.5f,
                record.y * tile_size_),
            Vector3(tile_size_, terrain_height, tile_size_),
            0.0f));
    found->second.multi_mesh->set_instance_color(
        static_cast<int32_t>(record.instance_index),
        record.color);
    found->second.multi_mesh->set_instance_custom_data(
        static_cast<int32_t>(record.instance_index),
        Color(
            static_cast<float>(tile_texture_band(record.terrain_type_id, record.soil_fertility)),
            std::clamp(record.soil_fertility / 100.0f, 0.0f, 1.0f),
            std::clamp(record.moisture / 100.0f, 0.0f, 1.0f),
            std::clamp(record.soil_salinity / 100.0f, 0.0f, 1.0f)));

    if (refresh_visual_dependents)
    {
        refresh_all_visual_transforms();
        refresh_worker_transform();
    }
}

void Gs1GodotSiteSceneController::refresh_all_visual_transforms()
{
    for (const auto& [_, record] : plant_visuals_)
    {
        set_visual_instance_transforms(record, false, 0U, record.instance_count);
    }
    for (const auto& [_, record] : device_visuals_)
    {
        set_visual_instance_transforms(record, true, 0U, record.instance_count);
    }
}

void Gs1GodotSiteSceneController::refresh_worker_transform()
{
    if (worker_marker_ == nullptr || !worker_marker_->is_visible())
    {
        return;
    }

    const Vector3 position = worker_marker_->get_position();
    worker_marker_->set_position(Vector3(
        position.x,
        terrain_surface_height_at(position.x / tile_size_, position.z / tile_size_) + k_worker_marker_height,
        position.z));
}

void Gs1GodotSiteSceneController::refresh_camera_frame()
{
    if (bootstrap_camera_ == nullptr)
    {
        bootstrap_camera_ = Object::cast_to<Camera3D>(get_node_or_null("BootstrapCamera"));
    }
    if (bootstrap_camera_ == nullptr)
    {
        return;
    }

    bootstrap_camera_->set_current(true);
    if (last_width_ > 0 && last_height_ > 0)
    {
        position_bootstrap_camera();
        return;
    }

    const float width = last_width_ > 0 ? static_cast<float>(last_width_) : 18.0f;
    const float height = last_height_ > 0 ? static_cast<float>(last_height_) : 18.0f;
    const float max_span = std::max(width, height);
    const Vector3 target((width - 1.0f) * 0.5f, 0.45f, (height - 1.0f) * 0.5f);
    bootstrap_camera_->set_position(Vector3(
        target.x + (max_span * 0.22f),
        std::max(11.0f, max_span * 0.78f),
        target.z + (max_span * 0.78f)));
    bootstrap_camera_->look_at(target, Vector3(0.0f, 1.0f, 0.0f));
}

void Gs1GodotSiteSceneController::position_bootstrap_camera()
{
    if (bootstrap_camera_ == nullptr || last_width_ <= 0 || last_height_ <= 0)
    {
        return;
    }

    const float width = static_cast<float>(std::max(last_width_, 1));
    const float height = static_cast<float>(std::max(last_height_, 1));
    const float longest_side = std::max(width, height) * tile_size_;
    const Vector3 center(
        ((width - 1.0f) * 0.5f) * tile_size_,
        0.0f,
        ((height - 1.0f) * 0.5f) * tile_size_);

    bootstrap_camera_->set_position(center + Vector3(0.0f, 10.0f + (longest_side * 0.72f), 7.5f + (longest_side * 0.58f)));
    bootstrap_camera_->look_at(center + Vector3(0.0f, 0.6f, 0.0f), Vector3(0.0f, 1.0f, 0.0f));
    bootstrap_camera_->set_current(true);
}

void Gs1GodotSiteSceneController::set_visual_instance_transforms(
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

Transform3D Gs1GodotSiteSceneController::visual_instance_transform(
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
    const float ground_height = terrain_surface_height_at(
        anchor_tile_x + ((size_x - 1.0f) * 0.5f),
        anchor_tile_y + ((size_z - 1.0f) * 0.5f));
    const Vector3 tile_center(
        (anchor_tile_x + (size_x * 0.5f) - 0.5f) * tile_size_,
        ground_height + (y_offset * size_y),
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

void Gs1GodotSiteSceneController::remove_tile_instance(std::uint64_t tile_id)
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

void Gs1GodotSiteSceneController::remove_visual_instance_range(
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

std::uint32_t Gs1GodotSiteSceneController::visual_batch_key(bool device_visual, std::uint32_t type_id, std::uint32_t part_index) const noexcept
{
    std::uint32_t hash = device_visual ? k_device_batch_key_mask : k_plant_batch_key_mask;
    hash = combine_u32_hash(hash, type_id);
    hash = combine_u32_hash(hash, part_index);
    return hash;
}

Gs1GodotSiteSceneController::VisualNodeRecord* Gs1GodotSiteSceneController::visual_record_for_slot(
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
