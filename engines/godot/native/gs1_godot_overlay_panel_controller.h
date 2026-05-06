#pragma once

#include "gs1_godot_projection_types.h"
#include "gs1_godot_runtime_node.h"

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/label.hpp>

#include <optional>

class Gs1GodotOverlayPanelController final : public IGs1GodotEngineMessageSubscriber
{
public:
    void cache_ui_references(godot::Control& owner);
    [[nodiscard]] bool handles_engine_message(Gs1EngineMessageType type) const noexcept override;
    void handle_engine_message(const Gs1EngineMessage& message) override;
    void handle_runtime_message_reset() override;
    void refresh_if_needed();

private:
    godot::Control* panel_ {nullptr};
    godot::Label* overlay_state_label_ {nullptr};
    std::optional<Gs1AppState> current_app_state_ {};
    std::optional<Gs1RuntimeProtectionOverlayProjection> overlay_state_ {};
    bool dirty_ {true};
};
