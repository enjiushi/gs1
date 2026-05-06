#include "gs1_godot_overlay_panel_controller.h"

using namespace godot;

void Gs1GodotOverlayPanelController::cache_ui_references(Control& owner)
{
    if (overlay_state_label_ == nullptr)
    {
        overlay_state_label_ = Object::cast_to<Label>(owner.find_child("OverlayStateLabel", true, false));
    }
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
}

void Gs1GodotOverlayPanelController::handle_runtime_message_reset()
{
    overlay_state_.reset();
    dirty_ = true;
}

void Gs1GodotOverlayPanelController::refresh_if_needed()
{
    if (!dirty_ || overlay_state_label_ == nullptr)
    {
        return;
    }

    const int overlay_mode = overlay_state_.has_value()
        ? static_cast<int>(overlay_state_->mode)
        : 0;
    overlay_state_label_->set_text(vformat("Overlay Mode: %d", overlay_mode));
    dirty_ = false;
}
