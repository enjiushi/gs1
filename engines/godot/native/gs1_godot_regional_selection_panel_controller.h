#pragma once

#include "gs1_godot_adapter_service.h"
#include "gs1/state_view.h"

#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/grid_container.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/core/binder_common.hpp>
#include <godot_cpp/core/object_id.hpp>
#include <godot_cpp/classes/rich_text_label.hpp>
#include <godot_cpp/classes/texture2d.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class Gs1GodotRegionalSelectionPanelController final
    : public godot::Control
    , public IGs1GodotEngineMessageSubscriber
{
    GDCLASS(Gs1GodotRegionalSelectionPanelController, godot::Control)

public:
    using SubmitUiActionFn = std::function<void(std::int64_t action_type, std::int64_t target_id, std::int64_t arg0, std::int64_t arg1)>;

    struct RegionalSiteState final
    {
        std::uint32_t site_id {0};
        Gs1SiteState site_state {GS1_SITE_STATE_LOCKED};
        std::int32_t grid_x {0};
        std::int32_t grid_y {0};
        std::uint32_t support_package_id {0};
        std::uint32_t exported_support_item_offset {0};
        std::uint32_t exported_support_item_count {0};
        std::uint32_t nearby_aura_modifier_id_count {0};
    };

    Gs1GodotRegionalSelectionPanelController() = default;
    ~Gs1GodotRegionalSelectionPanelController() override = default;

    void _ready() override;
    void _exit_tree() override;

    void cache_ui_references(godot::Control& owner);
    void set_submit_ui_action_callback(SubmitUiActionFn callback);
    void handle_action_pressed(std::int64_t button_key);
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

    struct LoadoutSlotRecord final
    {
        godot::ObjectID object_id {};
    };

    void cache_adapter_service();
    [[nodiscard]] godot::Control* resolve_owner_control();
    void submit_ui_action(std::int64_t action_type, std::int64_t target_id, std::int64_t arg0, std::int64_t arg1);
    void reset_regional_map_state() noexcept;
    void refresh_from_game_state_view();
    [[nodiscard]] const RegionalSiteState* resolve_selected_site() const;
    void apply_panel_visibility(const RegionalSiteState* selected_site);
    void apply_title_and_summary(const RegionalSiteState* selected_site);
    void refresh_selection_panel_actions_and_summaries(const RegionalSiteState* selected_site);
    void rebuild_selection_panel();
    void reconcile_action_buttons(const godot::Array& button_specs);
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
    [[nodiscard]] godot::String site_button_text(const RegionalSiteState& site) const;
    [[nodiscard]] godot::String site_tooltip(const RegionalSiteState& site) const;
    [[nodiscard]] godot::String site_state_name(Gs1SiteState site_state) const;
    [[nodiscard]] godot::String site_deployment_summary(const RegionalSiteState& site) const;
    [[nodiscard]] godot::String support_preview_text(const RegionalSiteState& site) const;
    [[nodiscard]] godot::Vector2i regional_grid_coord(const RegionalSiteState& site) const;
    void refresh_support_summary(const RegionalSiteState* selected_site);
    void refresh_loadout_panel();
    [[nodiscard]] godot::Ref<godot::Texture2D> item_icon_for(std::uint32_t item_id) const;
    [[nodiscard]] godot::Ref<godot::Texture2D> load_cached_texture(const godot::String& path) const;
    [[nodiscard]] std::uint64_t loadout_slot_key(std::uint32_t item_id, std::uint32_t slot_index) const noexcept;
    godot::Button* upsert_loadout_slot_button(
        std::uint64_t stable_key,
        const godot::String& node_name,
        int desired_index);
    void prune_loadout_slot_registry(const std::unordered_set<std::uint64_t>& desired_keys);

    godot::Control* owner_control_ {nullptr};
    Gs1GodotAdapterService* adapter_service_ {nullptr};
    godot::Control* panel_ {nullptr};
    godot::Label* title_ {nullptr};
    godot::RichTextLabel* summary_ {nullptr};
    godot::RichTextLabel* support_summary_ {nullptr};
    godot::Label* loadout_title_ {nullptr};
    godot::GridContainer* loadout_slots_grid_ {nullptr};
    godot::Label* loadout_empty_label_ {nullptr};
    godot::VBoxContainer* actions_ {nullptr};
    SubmitUiActionFn submit_ui_action_ {};
    std::vector<RegionalSiteState> sites_ {};
    std::vector<Gs1LoadoutSlotView> selected_site_loadout_slots_ {};
    int selected_site_id_ {0};
    std::unordered_map<std::uint64_t, ProjectedButtonRecord> action_buttons_ {};
    std::unordered_map<std::uint64_t, LoadoutSlotRecord> loadout_slot_buttons_ {};
    mutable std::unordered_map<int, godot::String> item_name_cache_ {};
    mutable std::unordered_map<std::string, godot::Ref<godot::Texture2D>> texture_cache_ {};
};
