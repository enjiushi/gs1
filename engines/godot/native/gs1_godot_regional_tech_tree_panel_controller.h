#pragma once

#include "gs1_godot_panel_state_reducers.h"
#include "gs1_godot_runtime_node.h"

#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/grid_container.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/panel_container.hpp>
#include <godot_cpp/classes/scroll_container.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/core/object_id.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>

class Gs1GodotRegionalTechTreePanelController final : public IGs1GodotEngineMessageSubscriber
{
public:
    using SubmitUiActionFn = std::function<void(std::int64_t action_type, std::int64_t target_id, std::int64_t arg0, std::int64_t arg1)>;

    void cache_ui_references(godot::Control& owner);
    void set_submit_ui_action_callback(SubmitUiActionFn callback);
    void handle_action_pressed(std::int64_t button_key);
    void handle_close_pressed();
    [[nodiscard]] bool is_panel_visible() const;
    [[nodiscard]] bool handles_engine_message(Gs1EngineMessageType type) const noexcept override;
    void handle_engine_message(const Gs1EngineMessage& message) override;
    void handle_runtime_message_reset() override;
    void refresh_if_needed();

private:
    struct ProjectedButtonRecord final
    {
        godot::ObjectID object_id {};
        godot::ObjectID content_root_id {};
        godot::ObjectID marker_label_id {};
        godot::ObjectID icon_texture_id {};
        godot::ObjectID icon_label_id {};
        godot::ObjectID title_label_id {};
        godot::ObjectID subtitle_label_id {};
        godot::ObjectID status_label_id {};
        godot::ObjectID lock_overlay_id {};
        godot::ObjectID lock_label_id {};
    };

    struct TechnologyUiCacheEntry final
    {
        godot::String title {};
        godot::String faction_name {};
        godot::String tooltip {};
        godot::Ref<godot::Texture2D> icon_texture {};
    };

    struct UnlockableUiCacheEntry final
    {
        godot::String title {};
        godot::String subtitle {};
        godot::String tooltip {};
        int content_kind {-1};
        godot::Ref<godot::Texture2D> icon_texture {};
    };

    void apply_overlay_layout();
    void reconcile_tech_tree_cards(const godot::Array& card_specs);
    [[nodiscard]] godot::String tech_tooltip_text(const godot::Dictionary& spec) const;
    [[nodiscard]] godot::String unlockable_tooltip_text(const godot::Dictionary& spec) const;
    [[nodiscard]] godot::String row_requirement_text(const godot::Dictionary& spec) const;
    [[nodiscard]] godot::String tech_tree_marker_text(const godot::Dictionary& spec) const;
    [[nodiscard]] godot::String card_icon_text(const godot::Dictionary& spec) const;
    [[nodiscard]] godot::String card_title_text(const godot::Dictionary& spec) const;
    [[nodiscard]] godot::String card_subtitle_text(const godot::Dictionary& spec) const;
    [[nodiscard]] godot::String card_status_text(const godot::Dictionary& spec) const;
    [[nodiscard]] godot::Color card_status_color(const godot::Dictionary& spec) const;
    [[nodiscard]] godot::Color card_icon_background_color(const godot::String& icon_text) const;
    [[nodiscard]] godot::Ref<godot::Texture2D> card_icon_texture(const godot::Dictionary& spec) const;
    void ensure_card_content_nodes(godot::Button* button, ProjectedButtonRecord& record);
    [[nodiscard]] godot::Ref<godot::Texture2D> load_cached_texture(const godot::String& path) const;
    [[nodiscard]] godot::Ref<godot::Texture2D> fallback_icon_texture(const godot::String& icon_text) const;
    [[nodiscard]] godot::String faction_name_for(int faction_id) const;
    [[nodiscard]] const TechnologyUiCacheEntry& technology_ui_for(std::uint32_t tech_node_id) const;
    [[nodiscard]] const UnlockableUiCacheEntry& unlockable_ui_for(std::uint32_t unlock_id) const;
    [[nodiscard]] godot::Button* upsert_button_node(
        godot::Node* container,
        std::unordered_map<std::uint64_t, ProjectedButtonRecord>& registry,
        std::uint64_t stable_key,
        const godot::String& node_name,
        int desired_index);
    void prune_button_registry(
        std::unordered_map<std::uint64_t, ProjectedButtonRecord>& registry,
        const std::unordered_set<std::uint64_t>& desired_keys);

    godot::Control* owner_control_ {nullptr};
    godot::Control* overlay_ {nullptr};
    godot::PanelContainer* panel_ {nullptr};
    godot::Label* title_ {nullptr};
    godot::ScrollContainer* summary_scroll_ {nullptr};
    godot::GridContainer* actions_ {nullptr};
    SubmitUiActionFn submit_ui_action_ {};
    Gs1GodotProgressionViewStateReducer progression_view_state_reducer_ {};
    std::optional<Gs1AppState> current_app_state_ {};
    bool dirty_ {true};
    std::unordered_map<std::uint64_t, ProjectedButtonRecord> action_buttons_ {};
    mutable std::unordered_map<int, godot::String> faction_name_cache_ {};
    mutable std::unordered_map<std::uint32_t, TechnologyUiCacheEntry> technology_ui_cache_ {};
    mutable std::unordered_map<std::uint32_t, UnlockableUiCacheEntry> unlockable_ui_cache_ {};
    mutable std::unordered_map<std::string, godot::Ref<godot::Texture2D>> texture_cache_ {};
    mutable std::unordered_map<std::string, godot::Ref<godot::Texture2D>> fallback_icon_texture_cache_ {};
};
