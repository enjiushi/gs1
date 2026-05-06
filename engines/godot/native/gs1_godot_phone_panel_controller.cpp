#include "gs1_godot_phone_panel_controller.h"

using namespace godot;

void Gs1GodotPhonePanelController::cache_ui_references(Control& owner)
{
    if (phone_state_label_ == nullptr)
    {
        phone_state_label_ = Object::cast_to<Label>(owner.find_child("PhoneStateLabel", true, false));
    }
}

bool Gs1GodotPhonePanelController::handles_engine_message(Gs1EngineMessageType type) const noexcept
{
    return type == GS1_ENGINE_MESSAGE_SITE_PHONE_PANEL_STATE || type == GS1_ENGINE_MESSAGE_SET_APP_STATE;
}

void Gs1GodotPhonePanelController::handle_engine_message(const Gs1EngineMessage& message)
{
    switch (message.type)
    {
    case GS1_ENGINE_MESSAGE_SITE_PHONE_PANEL_STATE:
        phone_panel_state_ = message.payload_as<Gs1EngineMessagePhonePanelData>();
        break;
    case GS1_ENGINE_MESSAGE_SET_APP_STATE:
    {
        const auto& payload = message.payload_as<Gs1EngineMessageSetAppStateData>();
        if (payload.app_state == GS1_APP_STATE_MAIN_MENU ||
            payload.app_state == GS1_APP_STATE_REGIONAL_MAP ||
            payload.app_state == GS1_APP_STATE_CAMPAIGN_END)
        {
            phone_panel_state_.reset();
        }
        break;
    }
    default:
        break;
    }
    dirty_ = true;
}

void Gs1GodotPhonePanelController::handle_runtime_message_reset()
{
    phone_panel_state_.reset();
    dirty_ = true;
}

void Gs1GodotPhonePanelController::refresh_if_needed()
{
    if (!dirty_ || phone_state_label_ == nullptr)
    {
        return;
    }

    if (phone_panel_state_.has_value())
    {
        phone_state_label_->set_text(vformat(
            "Phone Section: %d  Listings: buy %d sell %d",
            static_cast<int>(phone_panel_state_->active_section),
            static_cast<int>(phone_panel_state_->buy_listing_count),
            static_cast<int>(phone_panel_state_->sell_listing_count)));
    }
    else
    {
        phone_state_label_->set_text("Phone Section: 0  Listings: buy 0 sell 0");
    }

    dirty_ = false;
}
