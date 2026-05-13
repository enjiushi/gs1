#include "gs1_godot_status_panel_controller.h"

#include "gs1_godot_controller_context.h"
#include "content/defs/faction_defs.h"
#include "support/currency.h"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>

using namespace godot;

namespace
{
String app_state_name(const Gs1AppState app_state)
{
    switch (app_state)
    {
    case GS1_APP_STATE_BOOT:
        return "Boot";
    case GS1_APP_STATE_MAIN_MENU:
        return "Main Menu";
    case GS1_APP_STATE_CAMPAIGN_SETUP:
        return "Campaign Setup";
    case GS1_APP_STATE_REGIONAL_MAP:
        return "Regional Map";
    case GS1_APP_STATE_SITE_LOADING:
        return "Site Loading";
    case GS1_APP_STATE_SITE_ACTIVE:
        return "Site Active";
    case GS1_APP_STATE_SITE_PAUSED:
        return "Site Paused";
    case GS1_APP_STATE_SITE_RESULT:
        return "Site Result";
    case GS1_APP_STATE_CAMPAIGN_END:
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
    refresh_from_game_state_view();
    rebuild_status_label();
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
    return type == GS1_ENGINE_MESSAGE_PRESENTATION_DIRTY ||
        type == GS1_ENGINE_MESSAGE_SET_APP_STATE;
}

void Gs1GodotStatusPanelController::refresh_from_game_state_view()
{
    app_state_ = GS1_APP_STATE_BOOT;
    total_reputation_ = 0;
    village_reputation_ = 0;
    forestry_reputation_ = 0;
    university_reputation_ = 0;
    site_cash_.reset();
    current_action_kind_.reset();

    if (adapter_service_ == nullptr)
    {
        return;
    }

    Gs1GameStateView view {};
    if (!adapter_service_->get_game_state_view(view))
    {
        return;
    }

    app_state_ = view.app_state;
    if (view.has_campaign != 0U && view.campaign != nullptr)
    {
        const Gs1CampaignStateView& campaign = *view.campaign;
        total_reputation_ = campaign.total_reputation;
        for (std::uint32_t index = 0; index < campaign.faction_progress_count; ++index)
        {
            const Gs1FactionProgressView& progress = campaign.faction_progress[index];
            switch (progress.faction_id)
            {
            case gs1::k_faction_village_committee:
                village_reputation_ = progress.faction_reputation;
                break;
            case gs1::k_faction_forestry_bureau:
                forestry_reputation_ = progress.faction_reputation;
                break;
            case gs1::k_faction_agricultural_university:
                university_reputation_ = progress.faction_reputation;
                break;
            default:
                break;
            }
        }
    }

    if (view.has_active_site != 0U && view.active_site != nullptr)
    {
        const Gs1SiteStateView& site = *view.active_site;
        site_cash_ = gs1::cash_value_from_cash_points(site.current_cash_points);
        if (site.action.has_current_action != 0U)
        {
            current_action_kind_ = site.action.action_kind;
        }
    }
}

void Gs1GodotStatusPanelController::handle_engine_message(const Gs1EngineMessage& message)
{
    if (message.type == GS1_ENGINE_MESSAGE_PRESENTATION_DIRTY ||
        message.type == GS1_ENGINE_MESSAGE_SET_APP_STATE)
    {
        refresh_from_game_state_view();
    }
    rebuild_status_label();
}

void Gs1GodotStatusPanelController::handle_runtime_message_reset()
{
    app_state_ = GS1_APP_STATE_BOOT;
    total_reputation_ = 0;
    village_reputation_ = 0;
    forestry_reputation_ = 0;
    university_reputation_ = 0;
    site_cash_.reset();
    current_action_kind_.reset();
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
    lines.push_back(vformat("Screen: %s", app_state_name(app_state_)));

    if (total_reputation_ > 0 ||
        village_reputation_ > 0 ||
        forestry_reputation_ > 0 ||
        university_reputation_ > 0)
    {
        lines.push_back(vformat(
            "Reputation T/V/B/U: %d / %d / %d / %d",
            total_reputation_,
            village_reputation_,
            forestry_reputation_,
            university_reputation_));
    }
    if (site_cash_.has_value())
    {
        lines.push_back(vformat("Site Cash: %.2f", site_cash_.value()));
    }
    if (current_action_kind_.has_value())
    {
        lines.push_back(vformat("Action Kind: %d", static_cast<int>(current_action_kind_.value())));
    }
    if (!last_action_message_.is_empty())
    {
        lines.push_back(vformat("Last Action: %s", last_action_message_));
    }

    status_label_->set_text(String("\n").join(lines));
}
