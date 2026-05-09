#pragma once

#include "gs1_godot_adapter_service.h"
#include "gs1_godot_panel_state_reducers.h"

#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/rich_text_label.hpp>
#include <godot_cpp/core/binder_common.hpp>

#include <cstdint>
#include <functional>
#include <optional>

class Gs1GodotRegionalMapHudController final
    : public godot::Control
    , public IGs1GodotEngineMessageSubscriber
{
    GDCLASS(Gs1GodotRegionalMapHudController, godot::Control)

public:
    using SubmitUiActionFn = std::function<void(std::int64_t action_type, std::int64_t target_id, std::int64_t arg0, std::int64_t arg1)>;

    Gs1GodotRegionalMapHudController() = default;
    ~Gs1GodotRegionalMapHudController() override = default;

    void _ready() override;
    void _process(double delta) override;
    void _exit_tree() override;

    void cache_ui_references(godot::Control& owner);
    void set_submit_ui_action_callback(SubmitUiActionFn callback);
    void handle_tech_button_pressed();
    [[nodiscard]] bool handles_engine_message(Gs1EngineMessageType type) const noexcept override;
    void handle_engine_message(const Gs1EngineMessage& message) override;
    void handle_runtime_message_reset() override;

protected:
    static void _bind_methods();

private:
    void cache_adapter_service();
    [[nodiscard]] godot::Control* resolve_owner_control();
    void submit_ui_action(std::int64_t action_type, std::int64_t target_id, std::int64_t arg0, std::int64_t arg1);
    void rebuild_hud();
    [[nodiscard]] godot::String selected_site_text() const;
    [[nodiscard]] godot::String campaign_summary_text() const;
    [[nodiscard]] godot::String site_state_name(int site_state) const;
    [[nodiscard]] godot::String support_preview_text(int preview_mask) const;
    [[nodiscard]] godot::String tech_button_label() const;
    [[nodiscard]] Gs1UiAction tech_button_action() const;
    [[nodiscard]] const Gs1RuntimeRegionalMapSiteProjection* selected_site() const;

    godot::Control* owner_control_ {nullptr};
    Gs1GodotAdapterService* adapter_service_ {nullptr};
    godot::Control* hud_ {nullptr};
    godot::RichTextLabel* selected_site_summary_ {nullptr};
    godot::RichTextLabel* campaign_summary_ {nullptr};
    godot::Button* tech_button_ {nullptr};
    SubmitUiActionFn submit_ui_action_ {};
    Gs1GodotRegionalMapStateReducer regional_map_state_reducer_ {k_gs1_regional_map_hud_message_family};
    std::optional<Gs1RuntimeCampaignResourcesProjection> campaign_resources_ {};
    bool tech_tree_visible_ {false};
    bool tech_button_connected_ {false};
};
