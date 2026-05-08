#pragma once

#include "gs1_godot_panel_state_reducers.h"
#include "gs1_godot_runtime_node.h"

#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/rich_text_label.hpp>

#include <cstdint>
#include <functional>
#include <optional>

class Gs1GodotRegionalMapHudController final : public IGs1GodotEngineMessageSubscriber
{
public:
    using SubmitUiActionFn = std::function<void(std::int64_t action_type, std::int64_t target_id, std::int64_t arg0, std::int64_t arg1)>;

    void cache_ui_references(godot::Control& owner);
    void set_submit_ui_action_callback(SubmitUiActionFn callback);
    void handle_tech_button_pressed();
    [[nodiscard]] bool handles_engine_message(Gs1EngineMessageType type) const noexcept override;
    void handle_engine_message(const Gs1EngineMessage& message) override;
    void handle_runtime_message_reset() override;
    void refresh_if_needed();

private:
    [[nodiscard]] godot::String selected_site_text() const;
    [[nodiscard]] godot::String campaign_summary_text() const;
    [[nodiscard]] godot::String site_state_name(int site_state) const;
    [[nodiscard]] godot::String support_preview_text(int preview_mask) const;
    [[nodiscard]] godot::String tech_button_label() const;
    [[nodiscard]] Gs1UiAction tech_button_action() const;
    [[nodiscard]] const Gs1RuntimeRegionalMapSiteProjection* selected_site() const;

    godot::Control* hud_ {nullptr};
    godot::RichTextLabel* selected_site_summary_ {nullptr};
    godot::RichTextLabel* campaign_summary_ {nullptr};
    godot::Button* tech_button_ {nullptr};
    SubmitUiActionFn submit_ui_action_ {};
    Gs1GodotRegionalMapStateReducer regional_map_state_reducer_ {};
    Gs1GodotUiPanelStateReducer ui_panel_state_reducer_ {};
    std::optional<Gs1AppState> current_app_state_ {};
    std::optional<Gs1RuntimeCampaignResourcesProjection> campaign_resources_ {};
    bool tech_button_connected_ {false};
    bool dirty_ {true};
};
