#include "gs1_godot_overlay_panel_controller.h"

#include <godot_cpp/core/object.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>

using namespace godot;

namespace
{
constexpr std::int64_t k_ui_action_set_site_protection_overlay_mode = 26;

Gs1GodotOverlayPanelController* resolve_controller(std::int64_t controller_bits)
{
    return reinterpret_cast<Gs1GodotOverlayPanelController*>(static_cast<std::uintptr_t>(controller_bits));
}

void dispatch_overlay_mode_pressed(std::int64_t controller_bits, std::int64_t mode)
{
    if (Gs1GodotOverlayPanelController* controller = resolve_controller(controller_bits))
    {
        controller->handle_overlay_mode_pressed(mode);
    }
}
}

void Gs1GodotOverlayPanelController::cache_ui_references(Control& owner)
{
    owner_control_ = &owner;
    if (panel_ == nullptr)
    {
        panel_ = Object::cast_to<Control>(owner.find_child("OverlayPanel", true, false));
    }
    if (overlay_state_label_ == nullptr)
    {
        overlay_state_label_ = Object::cast_to<Label>(owner.find_child("OverlayStateLabel", true, false));
    }
    const std::int64_t controller_bits = static_cast<std::int64_t>(reinterpret_cast<std::uintptr_t>(this));
    const struct ButtonBinding final
    {
        const char* name;
        std::int64_t mode;
    } bindings[] {
        {"OverlayWindButton", 1},
        {"OverlayHeatButton", 2},
        {"OverlayDustButton", 3},
        {"OverlayConditionButton", 4},
        {"OverlayClearButton", 0},
    };
    for (const ButtonBinding& binding : bindings)
    {
        if (Button* button = Object::cast_to<Button>(owner.find_child(binding.name, true, false)))
        {
            const Callable callback = callable_mp_static(&dispatch_overlay_mode_pressed).bind(controller_bits, binding.mode);
            if (!button->is_connected("pressed", callback))
            {
                button->connect("pressed", callback);
            }
        }
    }
    refresh_if_needed();
}

void Gs1GodotOverlayPanelController::set_submit_ui_action_callback(SubmitUiActionFn callback)
{
    submit_ui_action_ = std::move(callback);
}

void Gs1GodotOverlayPanelController::handle_overlay_mode_pressed(std::int64_t mode)
{
    if (!submit_ui_action_)
    {
        return;
    }

    submit_ui_action_(
        k_ui_action_set_site_protection_overlay_mode,
        0,
        mode,
        0);
}

bool Gs1GodotOverlayPanelController::handles_engine_message(Gs1EngineMessageType type) const noexcept
{
    return type == GS1_ENGINE_MESSAGE_SITE_PROTECTION_OVERLAY_STATE ||
        type == GS1_ENGINE_MESSAGE_SET_APP_STATE;
}

void Gs1GodotOverlayPanelController::handle_engine_message(const Gs1EngineMessage& message)
{
    switch (message.type)
    {
    case GS1_ENGINE_MESSAGE_SITE_PROTECTION_OVERLAY_STATE:
        overlay_state_ = message.payload_as<Gs1EngineMessageSiteProtectionOverlayData>();
        break;
    case GS1_ENGINE_MESSAGE_SET_APP_STATE:
    {
        const auto& payload = message.payload_as<Gs1EngineMessageSetAppStateData>();
        current_app_state_ = payload.app_state;
        if (payload.app_state == GS1_APP_STATE_MAIN_MENU ||
            payload.app_state == GS1_APP_STATE_REGIONAL_MAP ||
            payload.app_state == GS1_APP_STATE_CAMPAIGN_END)
        {
            overlay_state_.reset();
        }
        break;
    }
    default:
        break;
    }
    dirty_ = true;
    refresh_if_needed();
}

void Gs1GodotOverlayPanelController::handle_runtime_message_reset()
{
    current_app_state_.reset();
    overlay_state_.reset();
    dirty_ = true;
    refresh_if_needed();
}

void Gs1GodotOverlayPanelController::refresh_if_needed()
{
    if (!dirty_)
    {
        return;
    }

    const int app_state = current_app_state_.has_value() ? static_cast<int>(current_app_state_.value()) : 0;
    const bool panel_visible = app_state >= GS1_APP_STATE_SITE_LOADING && app_state <= GS1_APP_STATE_SITE_RESULT;
    if (panel_ != nullptr)
    {
        panel_->set_visible(panel_visible);
    }
    if (!panel_visible)
    {
        dirty_ = false;
        return;
    }
    if (overlay_state_label_ == nullptr)
    {
        return;
    }

    const int overlay_mode = overlay_state_.has_value()
        ? static_cast<int>(overlay_state_->mode)
        : 0;
    overlay_state_label_->set_text(vformat("Overlay Mode: %d", overlay_mode));
    dirty_ = false;
}
