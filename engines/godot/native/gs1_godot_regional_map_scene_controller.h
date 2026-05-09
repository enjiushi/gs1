#pragma once

#include "gs1_godot_adapter_service.h"
#include "gs1_godot_projection_types.h"

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/core/binder_common.hpp>
#include <godot_cpp/core/object_id.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/node_path.hpp>
#include <godot_cpp/variant/rect2i.hpp>
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
class Camera3D;
class InputEvent;
class Label3D;
class Node;
class Node3D;
class StandardMaterial3D;
}

class Gs1GodotRegionalMapSceneController final
    : public godot::Control
    , public IGs1GodotEngineMessageSubscriber
{
    GDCLASS(Gs1GodotRegionalMapSceneController, godot::Control)

public:
    Gs1GodotRegionalMapSceneController() = default;
    ~Gs1GodotRegionalMapSceneController() override = default;

    void _ready() override;
    void _process(double delta) override;
    void _exit_tree() override;
    void _input(const godot::Ref<godot::InputEvent>& event) override;

    [[nodiscard]] bool handles_engine_message(Gs1EngineMessageType type) const noexcept override;
    void handle_engine_message(const Gs1EngineMessage& message) override;
    void handle_runtime_message_reset() override;

protected:
    static void _bind_methods();

private:
    struct RegionalSiteNodeRecord final
    {
        godot::ObjectID root_id {};
        godot::ObjectID base_id {};
        godot::ObjectID tower_id {};
        godot::ObjectID beacon_id {};
        godot::ObjectID label_id {};
    };

    struct PendingRegionalMapState final
    {
        std::vector<Gs1RuntimeRegionalMapSiteProjection> sites {};
        std::vector<Gs1RuntimeRegionalMapLinkProjection> links {};
    };

    void cache_adapter_service();
    void cache_scene_references();
    void cache_ui_references();
    void submit_ui_action(std::int64_t action_type, std::int64_t target_id = 0);
    void reset_regional_map_state() noexcept;
    void apply_regional_map_message(const Gs1EngineMessage& message);
    void select_regional_site(int site_id);
    void refresh_regional_map();
    void rebuild_regional_map_world(
        const std::vector<Gs1RuntimeRegionalMapSiteProjection>& sites,
        const std::vector<Gs1RuntimeRegionalMapLinkProjection>& links);
    void clear_regional_projection_world();
    void reconcile_regional_sites(const std::vector<Gs1RuntimeRegionalMapSiteProjection>& sites);
    void position_regional_camera(const godot::Rect2i& bounds);
    void update_regional_site_visuals();
    [[nodiscard]] godot::Vector2i regional_grid_coord(const Gs1RuntimeRegionalMapSiteProjection& site) const;
    [[nodiscard]] godot::Vector3 regional_world_position(const godot::Vector2i& grid) const;
    [[nodiscard]] godot::Color regional_site_state_color(int site_state) const;
    [[nodiscard]] godot::Ref<godot::StandardMaterial3D> get_material(
        const godot::String& key,
        const godot::Color& color,
        double roughness,
        double metallic,
        bool emission = false);
    void clear_dynamic_children(godot::Node* container, const godot::String& prefix = godot::String("Dynamic"));
    void prune_regional_site_registry(const std::unordered_set<int>& desired_site_ids);
    [[nodiscard]] bool regional_map_ui_contains_screen_point(const godot::Vector2& screen_position) const;
    [[nodiscard]] bool try_select_regional_site_from_screen(const godot::Vector2& screen_position);

private:
    static constexpr std::int64_t UI_ACTION_SELECT_DEPLOYMENT_SITE = 2;
    static constexpr double REGIONAL_TILE_SIZE = 4.5;
    static constexpr int REGIONAL_WORLD_PADDING = 2;
    static constexpr double REGIONAL_PICK_DISTANCE = 400.0;
    static constexpr int MOUSE_BUTTON_LEFT = 1;

    godot::NodePath regional_world_path_ {"RegionalMapWorld"};
    godot::NodePath regional_camera_path_ {"RegionalMapWorld/RegionalMapCamera"};
    godot::NodePath regional_site_root_path_ {"RegionalMapWorld/SiteRoot"};

    Gs1GodotAdapterService* adapter_service_ {nullptr};
    godot::Node3D* regional_map_world_ {nullptr};
    godot::Camera3D* regional_camera_ {nullptr};
    godot::Node3D* regional_site_root_ {nullptr};

    std::optional<std::uint32_t> selected_site_id_ {};
    std::vector<Gs1RuntimeRegionalMapSiteProjection> sites_ {};
    std::vector<Gs1RuntimeRegionalMapLinkProjection> links_ {};
    std::optional<PendingRegionalMapState> pending_regional_map_state_ {};
    godot::Rect2i regional_map_bounds_ {-4, -3, 9, 7};

    std::unordered_map<int, RegionalSiteNodeRecord> regional_site_nodes_ {};
    std::unordered_map<int, Gs1RuntimeRegionalMapSiteProjection> regional_site_data_ {};
    std::unordered_map<std::string, godot::Ref<godot::StandardMaterial3D>> regional_material_cache_ {};

    godot::Control* regional_map_panel_ {nullptr};
    godot::Control* regional_selection_panel_ {nullptr};
    godot::Control* regional_tech_tree_overlay_ {nullptr};
};
