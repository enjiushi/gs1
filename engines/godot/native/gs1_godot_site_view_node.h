#pragma once

#include "gs1_godot_runtime_node.h"

#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/core/object_id.hpp>
#include <godot_cpp/variant/node_path.hpp>

#include <cstdint>
#include <unordered_map>

namespace godot
{
class Camera3D;
class Label3D;
class StandardMaterial3D;
}

class Gs1SiteViewNode final : public godot::Node3D, public IGs1GodotEngineMessageSubscriber
{
    GDCLASS(Gs1SiteViewNode, godot::Node3D)

public:
    Gs1SiteViewNode() = default;
    ~Gs1SiteViewNode() override = default;

    void _ready() override;
    void _process(double delta) override;

    void set_runtime_node_path(const godot::NodePath& path);
    [[nodiscard]] godot::NodePath get_runtime_node_path() const;

    [[nodiscard]] bool handles_engine_message(Gs1EngineMessageType type) const noexcept override;
    void handle_engine_message(const Gs1EngineMessage& message) override;
    void handle_runtime_message_reset() override;

protected:
    static void _bind_methods();

private:
    struct VisualNodeRecord final
    {
        godot::ObjectID object_id {};
        godot::ObjectID instance_root_id {};
        std::uint64_t gameplay_id {0};
        std::uint32_t type_id {0};
        float anchor_tile_x {0.0f};
        float anchor_tile_y {0.0f};
        float height_scale {1.0f};
        std::uint64_t last_seen_snapshot_serial {0};
    };

    struct TileNodeRecord final
    {
        godot::ObjectID object_id {};
        std::uint64_t gameplay_id {0};
        std::uint64_t last_seen_snapshot_serial {0};
    };

    void ensure_presenter_created();
    [[nodiscard]] Gs1RuntimeNode* resolve_runtime_node() const;

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
    void upsert_visual_node(
        std::unordered_map<std::uint64_t, VisualNodeRecord>& registry,
        std::uint64_t gameplay_id,
        std::uint32_t type_id,
        float anchor_tile_x,
        float anchor_tile_y,
        float height_scale,
        const godot::Color& color,
        bool device_visual,
        std::uint8_t footprint_width,
        std::uint8_t footprint_height);
    [[nodiscard]] godot::Node3D* instantiate_visual_scene(bool device_visual, std::uint32_t type_id) const;
    void remove_visual_node(std::unordered_map<std::uint64_t, VisualNodeRecord>& registry, std::uint64_t gameplay_id);

private:
    godot::Node3D* grid_root_ {nullptr};
    godot::Node3D* visual_root_ {nullptr};
    godot::MeshInstance3D* worker_marker_ {nullptr};
    godot::NodePath runtime_node_path_ {"../Runtime"};
    mutable Gs1RuntimeNode* runtime_node_ {nullptr};
    std::unordered_map<std::uint64_t, TileNodeRecord> tile_nodes_ {};
    std::unordered_map<std::uint64_t, VisualNodeRecord> plant_visuals_ {};
    std::unordered_map<std::uint64_t, VisualNodeRecord> device_visuals_ {};
    std::int32_t last_width_ {-1};
    std::int32_t last_height_ {-1};
    float tile_size_ {1.0f};
    std::uint64_t active_snapshot_serial_ {0};
    bool reconcile_full_snapshot_ {false};
    bool worker_seen_in_snapshot_ {false};
};

using Gs1GodotSiteViewNode = Gs1SiteViewNode;
