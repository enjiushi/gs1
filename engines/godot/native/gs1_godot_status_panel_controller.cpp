#include "gs1_godot_status_panel_controller.h"

#include "gs1_godot_controller_context.h"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

namespace
{
String app_state_name(const int app_state)
{
    switch (app_state)
    {
    case 0:
        return "Boot";
    case 1:
        return "Main Menu";
    case 2:
        return "Campaign Setup";
    case 3:
        return "Regional Map";
    case 4:
        return "Site Loading";
    case 5:
        return "Site Active";
    case 6:
        return "Site Paused";
    case 7:
        return "Site Result";
    case 8:
        return "Campaign End";
    default:
        return "Unknown";
    }
}

}

void Gs1GodotStatusPanelController::_bind_methods()
{
}

void Gs1GodotStatusPanelController::_ready()
{
    cache_adapter_service();
    if (Control* owner = resolve_owner_control())
    {
        cache_ui_references(*owner);
    }
}

void Gs1GodotStatusPanelController::_exit_tree()
{
    if (adapter_service_ != nullptr)
    {
        adapter_service_->unsubscribe_all(*this);
        adapter_service_ = nullptr;
    }
}

void Gs1GodotStatusPanelController::cache_ui_references(Control& owner)
{
    owner_control_ = &owner;
    if (status_label_ == nullptr)
    {
        status_label_ = Object::cast_to<RichTextLabel>(owner.find_child("StatusLabel", true, false));
    }
    rebuild_status_label();
}

void Gs1GodotStatusPanelController::cache_adapter_service()
{
    if (adapter_service_ != nullptr)
    {
        return;
    }

    adapter_service_ = gs1_resolve_adapter_service(this);
    if (adapter_service_ != nullptr)
    {
        adapter_service_->subscribe_matching_messages(*this);
    }
}

Control* Gs1GodotStatusPanelController::resolve_owner_control()
{
    if (owner_control_ != nullptr)
    {
        return owner_control_;
    }
    owner_control_ = Object::cast_to<Control>(get_parent());
    if (owner_control_ == nullptr)
    {
        owner_control_ = this;
    }
    return owner_control_;
}

bool Gs1GodotStatusPanelController::handles_engine_message(Gs1EngineMessageType type) const noexcept
{
    switch (type)
    {
    case GS1_ENGINE_MESSAGE_CAMPAIGN_RESOURCES:
    case GS1_ENGINE_MESSAGE_HUD_STATE:
    case GS1_ENGINE_MESSAGE_SITE_ACTION_UPDATE:
    case GS1_ENGINE_MESSAGE_SITE_RESULT_READY:
        return true;
    default:
        return false;
    }
}

void Gs1GodotStatusPanelController::handle_engine_message(const Gs1EngineMessage& message)
{
    switch (message.type)
    {
    case GS1_ENGINE_MESSAGE_CAMPAIGN_RESOURCES:
        campaign_resources_ = message.payload_as<Gs1EngineMessageCampaignResourcesData>();
        break;
    case GS1_ENGINE_MESSAGE_HUD_STATE:
        hud_state_ = message.payload_as<Gs1EngineMessageHudStateData>();
        break;
    case GS1_ENGINE_MESSAGE_SITE_ACTION_UPDATE:
    {
        const auto& payload = message.payload_as<Gs1EngineMessageSiteActionData>();
        if ((payload.flags & GS1_SITE_ACTION_PRESENTATION_FLAG_CLEAR) != 0U)
        {
            site_action_state_.reset();
        }
        else
        {
            site_action_state_ = payload;
        }
        break;
    }
    case GS1_ENGINE_MESSAGE_SITE_RESULT_READY:
        break;
    default:
        break;
    }
    rebuild_status_label();
}

void Gs1GodotStatusPanelController::handle_runtime_message_reset()
{
    campaign_resources_.reset();
    hud_state_.reset();
    site_action_state_.reset();
    rebuild_status_label();
}

void Gs1GodotStatusPanelController::set_last_action_message(const String& message)
{
    last_action_message_ = message;
    rebuild_status_label();
}

void Gs1GodotStatusPanelController::rebuild_status_label()
{
    if (status_label_ == nullptr)
    {
        return;
    }

    PackedStringArray lines;
    lines.push_back("[b]Field Operations Feed[/b]");
    lines.push_back(vformat("Screen: %s", app_state_name(GS1_APP_STATE_SITE_ACTIVE)));

    if (campaign_resources_.has_value())
    {
        const auto& current_campaign_resources = campaign_resources_.value();
        lines.push_back(vformat("Campaign Cash: %.2f", current_campaign_resources.current_money));
        lines.push_back(vformat(
            "Reputation T/V/B/U: %d / %d / %d / %d",
            current_campaign_resources.total_reputation,
            current_campaign_resources.village_reputation,
            current_campaign_resources.forestry_reputation,
            current_campaign_resources.university_reputation));
    }
    if (!last_action_message_.is_empty())
    {
        lines.push_back(vformat("Last Action: %s", last_action_message_));
    }

    status_label_->set_text(String("\n").join(lines));
}
