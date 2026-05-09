#pragma once

#include "gs1_godot_adapter_service.h"
#include "gs1_godot_projection_types.h"

#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/rich_text_label.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/core/binder_common.hpp>
#include <godot_cpp/core/object_id.hpp>

#include <cstdint>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class Gs1GodotTaskPanelController final
    : public godot::Control
    , public IGs1GodotEngineMessageSubscriber
{
    GDCLASS(Gs1GodotTaskPanelController, godot::Control)

public:
    Gs1GodotTaskPanelController() = default;
    ~Gs1GodotTaskPanelController() override = default;

    void _ready() override;
    void _exit_tree() override;

    void cache_ui_references(godot::Control& owner);
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

    struct TaskTemplateUiCacheEntry final
    {
        int task_tier_id {0};
        int progress_kind {0};
        godot::Ref<godot::Texture2D> icon_texture {};
    };

    struct ModifierUiCacheEntry final
    {
        godot::String label {};
        godot::Ref<godot::Texture2D> icon_texture {};
    };

    void cache_adapter_service();
    [[nodiscard]] godot::Control* resolve_owner_control();
    void update_task_summary();
    void reconcile_task_rows();
    void reconcile_modifier_rows();
    [[nodiscard]] godot::Button* upsert_button_node(
        godot::Node* container,
        std::unordered_map<std::uint64_t, ProjectedButtonRecord>& registry,
        std::uint64_t stable_key,
        const godot::String& node_name,
        int desired_index);
    void prune_button_registry(
        std::unordered_map<std::uint64_t, ProjectedButtonRecord>& registry,
        const std::unordered_set<std::uint64_t>& desired_keys);
    [[nodiscard]] godot::Ref<godot::Texture2D> load_cached_texture(const godot::String& path) const;
    [[nodiscard]] const TaskTemplateUiCacheEntry& task_template_ui_for(std::uint32_t task_template_id) const;
    [[nodiscard]] const ModifierUiCacheEntry& modifier_ui_for(std::uint32_t modifier_id) const;

    godot::Control* panel_ {nullptr};
    godot::Control* owner_control_ {nullptr};
    Gs1GodotAdapterService* adapter_service_ {nullptr};
    godot::RichTextLabel* task_summary_ {nullptr};
    godot::VBoxContainer* task_rows_ {nullptr};
    godot::VBoxContainer* modifier_rows_ {nullptr};
    std::vector<Gs1RuntimeTaskProjection> tasks_ {};
    std::vector<Gs1RuntimeModifierProjection> active_modifiers_ {};
    std::unordered_map<std::uint32_t, std::size_t> pending_task_indices_ {};
    std::unordered_map<std::uint32_t, std::size_t> pending_modifier_indices_ {};
    bool in_site_snapshot_ {false};

    std::unordered_map<std::uint64_t, ProjectedButtonRecord> task_buttons_ {};
    std::unordered_map<std::uint64_t, ProjectedButtonRecord> modifier_buttons_ {};
    mutable std::unordered_map<std::uint32_t, TaskTemplateUiCacheEntry> task_template_ui_cache_ {};
    mutable std::unordered_map<std::uint32_t, ModifierUiCacheEntry> modifier_ui_cache_ {};
    mutable std::unordered_map<std::string, godot::Ref<godot::Texture2D>> texture_cache_ {};
};
