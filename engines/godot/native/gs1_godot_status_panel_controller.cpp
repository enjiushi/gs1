#include "gs1_godot_status_panel_controller.h"

#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

namespace
{
String bool_text(bool value, const String& when_true, const String& when_false)
{
    return value ? when_true : when_false;
}

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

String string_from_view(const std::string& value)
{
    return String::utf8(value.c_str(), static_cast<int64_t>(value.size()));
}
}

void Gs1GodotStatusPanelController::cache_ui_references(Control& owner)
{
    if (status_label_ == nullptr)
    {
        status_label_ = Object::cast_to<RichTextLabel>(owner.find_child("StatusLabel", true, false));
    }
}

bool Gs1GodotStatusPanelController::handles_engine_message(Gs1EngineMessageType type) const noexcept
{
    switch (type)
    {
    case GS1_ENGINE_MESSAGE_SET_APP_STATE:
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
    case GS1_ENGINE_MESSAGE_SET_APP_STATE:
    {
        const auto& payload = message.payload_as<Gs1EngineMessageSetAppStateData>();
        current_app_state_ = payload.app_state;
        if (payload.app_state == GS1_APP_STATE_MAIN_MENU ||
            payload.app_state == GS1_APP_STATE_REGIONAL_MAP)
        {
            hud_state_.reset();
            site_action_state_.reset();
        }
        else if (payload.app_state == GS1_APP_STATE_CAMPAIGN_END)
        {
            hud_state_.reset();
            site_action_state_.reset();
        }
        break;
    }
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
    dirty_ = true;
}

void Gs1GodotStatusPanelController::handle_runtime_message_reset()
{
    current_app_state_.reset();
    campaign_resources_.reset();
    hud_state_.reset();
    site_action_state_.reset();
    dirty_ = true;
}

void Gs1GodotStatusPanelController::set_last_action_message(const String& message)
{
    last_action_message_ = message;
    dirty_ = true;
}

void Gs1GodotStatusPanelController::show_runtime_missing()
{
    if (status_label_ != nullptr)
    {
        status_label_->set_text("Runtime node not found");
    }
}

void Gs1GodotStatusPanelController::refresh_if_needed(bool runtime_linked, const std::string& last_error)
{
    if (!dirty_)
    {
        return;
    }
    refresh(runtime_linked, last_error);
    dirty_ = false;
}

int Gs1GodotStatusPanelController::current_app_state_or(int fallback) const noexcept
{
    return current_app_state_.has_value()
        ? static_cast<int>(current_app_state_.value())
        : fallback;
}

const Gs1RuntimeCampaignResourcesProjection* Gs1GodotStatusPanelController::campaign_resources() const noexcept
{
    return campaign_resources_.has_value()
        ? &campaign_resources_.value()
        : nullptr;
}

const Gs1RuntimeHudProjection* Gs1GodotStatusPanelController::hud_state() const noexcept
{
    return hud_state_.has_value()
        ? &hud_state_.value()
        : nullptr;
}

const Gs1RuntimeSiteActionProjection* Gs1GodotStatusPanelController::site_action_state() const noexcept
{
    return site_action_state_.has_value()
        ? &site_action_state_.value()
        : nullptr;
}

void Gs1GodotStatusPanelController::refresh(bool runtime_linked, const std::string& last_error)
{
    if (status_label_ == nullptr)
    {
        return;
    }

    PackedStringArray lines;
    lines.push_back("[b]Field Operations Feed[/b]");
    lines.push_back(vformat("Runtime: %s", bool_text(runtime_linked, "linked", "idle")));
    lines.push_back(vformat("Screen: %s", app_state_name(current_app_state_or(0))));

    if (const Gs1RuntimeCampaignResourcesProjection* current_campaign_resources = campaign_resources();
        current_campaign_resources != nullptr)
    {
        lines.push_back(vformat("Campaign Cash: %.2f", current_campaign_resources->current_money));
        lines.push_back(vformat(
            "Reputation T/V/B/U: %d / %d / %d / %d",
            current_campaign_resources->total_reputation,
            current_campaign_resources->village_reputation,
            current_campaign_resources->forestry_reputation,
            current_campaign_resources->university_reputation));
    }
    if (!last_action_message_.is_empty())
    {
        lines.push_back(vformat("Last Action: %s", last_action_message_));
    }
    const String error_text = string_from_view(last_error);
    if (!error_text.is_empty())
    {
        lines.push_back(vformat("Error: %s", error_text));
    }

    status_label_->set_text(String("\n").join(lines));
}
