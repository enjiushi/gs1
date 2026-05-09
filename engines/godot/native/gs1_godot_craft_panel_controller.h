#pragma once

#include "gs1_godot_adapter_service.h"
#include "gs1_godot_projection_types.h"

#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/rich_text_label.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/core/binder_common.hpp>
#include <godot_cpp/core/object_id.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <unordered_map>
#include <unordered_set>

class Gs1GodotCraftPanelController final
    : public godot::Control
    , public IGs1GodotEngineMessageSubscriber
{
    GDCLASS(Gs1GodotCraftPanelController, godot::Control)

public:
    using SubmitCraftOptionFn = std::function<void(int tile_x, int tile_y, int output_item_id)>;

    Gs1GodotCraftPanelController() = default;
    ~Gs1GodotCraftPanelController() override = default;

    void _ready() override;
    void _process(double delta) override;
    void _exit_tree() override;

    void cache_ui_references(godot::Control& owner);
    void set_submit_craft_option_callback(SubmitCraftOptionFn callback);
    void handle_craft_option_pressed(std::int64_t button_key);
    [[nodiscard]] bool handles_engine_message(Gs1EngineMessageType type) const noexcept override;
    void handle_engine_message(const Gs1EngineMessage& message) override;
    void handle_runtime_message_reset() override;

protected:
    static void _bind_methods();

private:
    struct ProjectedButtonRecord final
    {
        godot::ObjectID object_id {};
    };

    void cache_adapter_service();
    [[nodiscard]] godot::Control* resolve_owner_control();
    void submit_craft_option(int tile_x, int tile_y, int output_item_id);
    void apply_panel_visibility();
    void update_craft_summary();
    void reconcile_craft_option_buttons();
    [[nodiscard]] godot::Button* upsert_button_node(
        godot::Node* container,
        std::unordered_map<std::uint64_t, ProjectedButtonRecord>& registry,
        std::uint64_t stable_key,
        const godot::String& node_name,
        int desired_index);
    void prune_button_registry(
        std::unordered_map<std::uint64_t, ProjectedButtonRecord>& registry,
        const std::unordered_set<std::uint64_t>& desired_keys);
    [[nodiscard]] godot::String item_name_for(int item_id) const;
    [[nodiscard]] godot::String recipe_output_name_for(int recipe_id, int output_item_id) const;

    godot::Control* panel_ {nullptr};
    godot::RichTextLabel* craft_summary_ {nullptr};
    godot::VBoxContainer* craft_options_ {nullptr};
    godot::Control* owner_control_ {nullptr};
    Gs1GodotAdapterService* adapter_service_ {nullptr};
    std::optional<Gs1RuntimePlacementPreviewProjection> placement_preview_ {};
    std::optional<Gs1RuntimePlacementFailureProjection> placement_failure_ {};
    std::optional<Gs1RuntimeCraftContextProjection> craft_context_ {};
    SubmitCraftOptionFn submit_craft_option_ {};
    std::unordered_map<std::uint64_t, ProjectedButtonRecord> craft_option_buttons_ {};
};
