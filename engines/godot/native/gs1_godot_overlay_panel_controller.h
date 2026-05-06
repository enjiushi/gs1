#pragma once

#include "gs1_godot_projection_types.h"
#include "gs1_godot_runtime_node.h"

#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/label.hpp>

#include <cstdint>
#include <functional>
#include <optional>

class Gs1GodotOverlayPanelController final : public IGs1GodotEngineMessageSubscriber
{
public:
    using SubmitUiActionFn = std::function<void(std::int64_t action_type, std::int64_t target_id, std::int64_t arg0, std::int64_t arg1)>;

    void cache_ui_references(godot::Control& owner);
    void set_submit_ui_action_callback(SubmitUiActionFn callback);
    void handle_overlay_mode_pressed(std::int64_t mode);
    [[nodiscard]] bool handles_engine_message(Gs1EngineMessageType type) const noexcept override;
    void handle_engine_message(const Gs1EngineMessage& message) override;
    void handle_runtime_message_reset() override;
    void refresh_if_needed();

private:
    godot::Control* owner_control_ {nullptr};
    godot::Control* panel_ {nullptr};
    godot::Label* overlay_state_label_ {nullptr};
    SubmitUiActionFn submit_ui_action_ {};
    std::optional<Gs1AppState> current_app_state_ {};
    std::optional<Gs1RuntimeProtectionOverlayProjection> overlay_state_ {};
    bool dirty_ {true};
};
