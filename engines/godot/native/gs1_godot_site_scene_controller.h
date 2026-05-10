#pragma once

#include "gs1_godot_adapter_service.h"

#include <godot_cpp/classes/material.hpp>
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/multi_mesh.hpp>
#include <godot_cpp/classes/multi_mesh_instance3d.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/core/object_id.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/node_path.hpp>
#include <godot_cpp/variant/transform3d.hpp>

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace godot
{
class Camera3D;
class Label3D;
class StandardMaterial3D;
}

class Gs1GodotSiteSceneController final : public godot::Control, public IGs1GodotEngineMessageSubscriber
{
    GDCLASS(Gs1GodotSiteSceneController, godot::Control)

public:
    Gs1GodotSiteSceneController() = default;
    ~Gs1GodotSiteSceneController() override = default;

    void _ready() override;
    void _exit_tree() override;

    [[nodiscard]] bool handles_engine_message(Gs1EngineMessageType type) const noexcept override;
    void handle_engine_message(const Gs1EngineMessage& message) override;
    void handle_runtime_message_reset() override;

protected:
    static void _bind_methods();

private:
    struct VisualNodeRecord final
    {
        std::uint64_t gameplay_id {0};
        std::uint32_t type_id {0};
        float anchor_tile_x {0.0f};
        float anchor_tile_y {0.0f};
        float height_scale {1.0f};
        std::uint32_t batch_key {0};
        std::uint32_t first_instance {0};
        std::uint16_t instance_count {0};
        std::uint8_t footprint_width {1};
        std::uint8_t footprint_height {1};
        godot::Color color {};
        std::uint64_t last_seen_snapshot_serial {0};
    };

    struct TileNodeRecord final
    {
        std::uint64_t gameplay_id {0};
        std::uint32_t batch_key {0};
        std::uint32_t instance_index {0};
        std::uint16_t x {0};
        std::uint16_t y {0};
        std::uint32_t terrain_type_id {0};
        float soil_fertility {0.0f};
        float moisture {0.0f};
        float soil_salinity {0.0f};
        float sand_burial {0.0f};
        float plant_density {0.0f};
        std::uint8_t visible_excavation_depth {0};
        godot::Color color {};
        std::uint64_t last_seen_snapshot_serial {0};
    };

    struct InstancedMeshPart final
    {
        godot::Ref<godot::Mesh> mesh {};
        godot::Ref<godot::Material> material {};
        godot::Transform3D local_transform {};
        std::uint32_t part_index {0};
    };

    struct InstancedBatch final
    {
        std::uint32_t batch_key {0};
        std::uint32_t type_id {0};
        std::uint32_t part_index {0};
        godot::ObjectID instance_node_id {};
        godot::Ref<godot::MultiMesh> multi_mesh {};
        godot::Transform3D local_transform {};
        std::uint32_t active_count {0};
    };

    void ensure_presenter_created();
    void cache_adapter_service();

    void apply_site_snapshot_begin(const Gs1EngineMessageSiteSnapshotData& payload);
    void apply_site_snapshot_end();
    void apply_site_tile_upsert(const Gs1EngineMessageSiteTileData& payload);
    void apply_site_worker_update(const Gs1EngineMessageWorkerData& payload);
    void apply_site_plant_visual_upsert(const Gs1EngineMessageSitePlantVisualData& payload);
    void apply_site_plant_visual_remove(const Gs1EngineMessageSiteVisualRemoveData& payload);
    void apply_site_device_visual_upsert(const Gs1EngineMessageSiteDeviceVisualData& payload);
    void apply_site_device_visual_remove(const Gs1EngineMessageSiteVisualRemoveData& payload);

    void clear_visuals();
    void clear_tiles();
    void upsert_instanced_visual(
        std::unordered_map<std::uint64_t, VisualNodeRecord>& registry,
        std::uint64_t gameplay_id,
        std::uint32_t type_id,
        float anchor_tile_x,
        float anchor_tile_y,
        float height_scale,
        std::uint16_t instance_count,
        const godot::Color& color,
        bool device_visual,
        std::uint8_t footprint_width,
        std::uint8_t footprint_height);
    void remove_instanced_visual(std::unordered_map<std::uint64_t, VisualNodeRecord>& registry, std::uint64_t gameplay_id);
    [[nodiscard]] std::uint16_t plant_instance_count(std::uint16_t density_quantized, std::uint8_t footprint_width, std::uint8_t footprint_height) const noexcept;
    [[nodiscard]] std::vector<InstancedMeshPart> resolve_instanced_mesh_parts(
        bool device_visual,
        std::uint32_t type_id,
        const godot::Color& fallback_color) const;
    void collect_mesh_parts(godot::Node* node, const godot::Transform3D& parent_transform, std::vector<InstancedMeshPart>& parts) const;
    [[nodiscard]] InstancedBatch& ensure_batch(
        std::unordered_map<std::uint32_t, InstancedBatch>& batches,
        bool device_visual,
        std::uint32_t batch_key,
        std::uint32_t type_id,
        const InstancedMeshPart& part);
    [[nodiscard]] std::uint32_t allocate_batch_slot(InstancedBatch& batch);
    void reserve_batch_slot(InstancedBatch& batch, std::uint32_t slot);
    void truncate_batch(InstancedBatch& batch, std::uint32_t active_count);
    void refresh_batch_visibility(InstancedBatch& batch);
    void remove_tile_instance(std::uint64_t tile_id);
    void write_tile_instance(
        const TileNodeRecord& record,
        bool refresh_visual_dependents);
    void set_visual_instance_transforms(
        const VisualNodeRecord& record,
        bool device_visual,
        std::uint32_t start_offset,
        std::uint32_t count);
    [[nodiscard]] godot::Transform3D visual_instance_transform(
        std::uint64_t gameplay_id,
        std::uint16_t instance_ordinal,
        std::uint16_t instance_count,
        bool device_visual,
        float anchor_tile_x,
        float anchor_tile_y,
        float height_scale,
        std::uint8_t footprint_width,
        std::uint8_t footprint_height) const;
    void remove_visual_instance_range(std::unordered_map<std::uint64_t, VisualNodeRecord>& registry, std::uint64_t gameplay_id);
    [[nodiscard]] std::uint32_t visual_batch_key(bool device_visual, std::uint32_t type_id, std::uint32_t part_index) const noexcept;
    [[nodiscard]] VisualNodeRecord* visual_record_for_slot(
        std::unordered_map<std::uint64_t, VisualNodeRecord>& registry,
        std::uint32_t batch_key,
        std::uint32_t slot);
    [[nodiscard]] std::uint32_t tile_batch_key_for_record(const TileNodeRecord& record) const noexcept;
    [[nodiscard]] std::uint32_t tile_texture_band(std::uint32_t terrain_type_id, float soil_fertility) const noexcept;
    [[nodiscard]] std::vector<InstancedMeshPart> resolve_tile_mesh_parts(std::uint32_t batch_key) const;
    [[nodiscard]] float terrain_surface_height_for_tile(const TileNodeRecord& record) const noexcept;
    [[nodiscard]] float terrain_surface_height_at(std::uint16_t x, std::uint16_t y) const noexcept;
    [[nodiscard]] float terrain_surface_height_at(float tile_x, float tile_y) const noexcept;
    [[nodiscard]] const TileNodeRecord* tile_record_at(std::uint16_t x, std::uint16_t y) const;
    void refresh_all_visual_transforms();
    void refresh_worker_transform();
    void refresh_camera_frame();

private:
    godot::Node3D* grid_root_ {nullptr};
    godot::Node3D* visual_root_ {nullptr};
    godot::Node3D* worker_marker_ {nullptr};
    godot::Camera3D* bootstrap_camera_ {nullptr};
    Gs1GodotAdapterService* adapter_service_ {nullptr};
    std::unordered_map<std::uint64_t, TileNodeRecord> tile_nodes_ {};
    std::unordered_map<std::uint64_t, VisualNodeRecord> plant_visuals_ {};
    std::unordered_map<std::uint64_t, VisualNodeRecord> device_visuals_ {};
    std::unordered_map<std::uint32_t, InstancedBatch> tile_batches_ {};
    std::unordered_map<std::uint32_t, InstancedBatch> plant_batches_ {};
    std::unordered_map<std::uint32_t, InstancedBatch> device_batches_ {};
    std::int32_t last_width_ {-1};
    std::int32_t last_height_ {-1};
    float tile_size_ {1.0f};
    std::uint64_t active_snapshot_serial_ {0};
    bool reconcile_full_snapshot_ {false};
    bool worker_seen_in_snapshot_ {false};
};

