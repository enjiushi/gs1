#pragma once

#include "gs1_godot_projection_types.h"
#include "gs1_godot_runtime_node.h"

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/rich_text_label.hpp>
#include <godot_cpp/variant/string.hpp>

class Gs1GodotStatusPanelController final : public IGs1GodotEngineMessageSubscriber
{
public:
    void cache_ui_references(godot::Control& owner);
    [[nodiscard]] bool handles_engine_message(Gs1EngineMessageType type) const noexcept override;
    void handle_engine_message(const Gs1EngineMessage& message) override;
    void handle_runtime_message_reset() override;
    void mark_dirty() noexcept { dirty_ = true; }
    void set_last_action_message(const godot::String& message);
    void show_runtime_missing();
    void refresh_if_needed(bool runtime_linked, const std::string& last_error);

    [[nodiscard]] int current_app_state_or(int fallback) const noexcept;
    [[nodiscard]] const Gs1RuntimeCampaignResourcesProjection* campaign_resources() const noexcept;
    [[nodiscard]] const Gs1RuntimeHudProjection* hud_state() const noexcept;
    [[nodiscard]] const Gs1RuntimeSiteActionProjection* site_action_state() const noexcept;

private:
    void refresh(bool runtime_linked, const std::string& last_error);

    std::optional<Gs1AppState> current_app_state_ {};
    std::optional<Gs1RuntimeCampaignResourcesProjection> campaign_resources_ {};
    std::optional<Gs1RuntimeHudProjection> hud_state_ {};
    std::optional<Gs1RuntimeSiteActionProjection> site_action_state_ {};
    godot::RichTextLabel* status_label_ {nullptr};
    godot::String last_action_message_ {};
    bool dirty_ {true};
};
