#include "gs1_godot_regional_map_hud_controller.h"

#include "gs1_godot_controller_context.h"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/object.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>

#include <cstdint>
#include <utility>

using namespace godot;

namespace
{
Gs1GodotRegionalMapHudController* resolve_controller(std::int64_t controller_bits)
{
    return reinterpret_cast<Gs1GodotRegionalMapHudController*>(static_cast<std::uintptr_t>(controller_bits));
}

void dispatch_tech_button_pressed(std::int64_t controller_bits)
{
    if (Gs1GodotRegionalMapHudController* controller = resolve_controller(controller_bits))
    {
        controller->handle_tech_button_pressed();
    }
}
}

void Gs1GodotRegionalMapHudController::_bind_methods()
{
}

void Gs1GodotRegionalMapHudController::_ready()
{
    set_submit_ui_action_callback([this](std::int64_t action_type, std::int64_t target_id, std::int64_t arg0, std::int64_t arg1) {
        submit_ui_action(action_type, target_id, arg0, arg1);
    });
    cache_adapter_service();
    if (Control* owner = resolve_owner_control())
    {
        cache_ui_references(*owner);
    }
    set_process(true);
}

void Gs1GodotRegionalMapHudController::_process(double delta)
{
    (void)delta;
    refresh_if_needed();
}

void Gs1GodotRegionalMapHudController::_exit_tree()
{
    if (adapter_service_ != nullptr)
    {
        adapter_service_->unsubscribe_all(*this);
        adapter_service_ = nullptr;
    }
}

void Gs1GodotRegionalMapHudController::cache_ui_references(Control& owner)
{
    owner_control_ = &owner;
    if (hud_ == nullptr)
    {
        hud_ = Object::cast_to<Control>(owner.find_child("RegionalMapHud", true, false));
    }
    if (selected_site_summary_ == nullptr)
    {
        selected_site_summary_ = Object::cast_to<RichTextLabel>(owner.find_child("RegionalHudSelectedSiteSummary", true, false));
    }
    if (campaign_summary_ == nullptr)
    {
        campaign_summary_ = Object::cast_to<RichTextLabel>(owner.find_child("RegionalHudCampaignSummary", true, false));
    }
    if (tech_button_ == nullptr)
    {
        tech_button_ = Object::cast_to<Button>(owner.find_child("RegionalHudTechButton", true, false));
    }
    if (tech_button_ != nullptr && !tech_button_connected_)
    {
        const Callable callback = callable_mp_static(&dispatch_tech_button_pressed).bind(
            static_cast<std::int64_t>(reinterpret_cast<std::uintptr_t>(this)));
        if (!tech_button_->is_connected("pressed", callback))
        {
            tech_button_->connect("pressed", callback);
        }
        tech_button_connected_ = true;
    }
    refresh_if_needed();
}

void Gs1GodotRegionalMapHudController::set_submit_ui_action_callback(SubmitUiActionFn callback)
{
    submit_ui_action_ = std::move(callback);
}

void Gs1GodotRegionalMapHudController::cache_adapter_service()
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

Control* Gs1GodotRegionalMapHudController::resolve_owner_control()
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

void Gs1GodotRegionalMapHudController::submit_ui_action(
    std::int64_t action_type,
    std::int64_t target_id,
    std::int64_t arg0,
    std::int64_t arg1)
{
    if (adapter_service_ != nullptr)
    {
        adapter_service_->submit_ui_action(action_type, target_id, arg0, arg1);
    }
}

void Gs1GodotRegionalMapHudController::handle_tech_button_pressed()
{
    if (!submit_ui_action_)
    {
        return;
    }

    const Gs1UiAction action = tech_button_action();
    submit_ui_action_(action.type, action.target_id, action.arg0, action.arg1);
}

bool Gs1GodotRegionalMapHudController::handles_engine_message(Gs1EngineMessageType type) const noexcept
{
    switch (type)
    {
    case GS1_ENGINE_MESSAGE_SET_APP_STATE:
    case GS1_ENGINE_MESSAGE_BEGIN_REGIONAL_MAP_SNAPSHOT:
    case GS1_ENGINE_MESSAGE_REGIONAL_MAP_SITE_UPSERT:
    case GS1_ENGINE_MESSAGE_REGIONAL_MAP_SITE_REMOVE:
    case GS1_ENGINE_MESSAGE_REGIONAL_MAP_LINK_UPSERT:
    case GS1_ENGINE_MESSAGE_REGIONAL_MAP_LINK_REMOVE:
    case GS1_ENGINE_MESSAGE_END_REGIONAL_MAP_SNAPSHOT:
    case GS1_ENGINE_MESSAGE_BEGIN_UI_PANEL:
    case GS1_ENGINE_MESSAGE_UI_PANEL_TEXT_UPSERT:
    case GS1_ENGINE_MESSAGE_UI_PANEL_SLOT_ACTION_UPSERT:
    case GS1_ENGINE_MESSAGE_UI_PANEL_LIST_ITEM_UPSERT:
    case GS1_ENGINE_MESSAGE_UI_PANEL_LIST_ACTION_UPSERT:
    case GS1_ENGINE_MESSAGE_END_UI_PANEL:
    case GS1_ENGINE_MESSAGE_CLOSE_UI_PANEL:
    case GS1_ENGINE_MESSAGE_CAMPAIGN_RESOURCES:
        return true;
    default:
        return false;
    }
}

void Gs1GodotRegionalMapHudController::handle_engine_message(const Gs1EngineMessage& message)
{
    regional_map_state_reducer_.apply_engine_message(message);
    ui_panel_state_reducer_.apply_engine_message(message);

    switch (message.type)
    {
    case GS1_ENGINE_MESSAGE_SET_APP_STATE:
        current_app_state_ = message.payload_as<Gs1EngineMessageSetAppStateData>().app_state;
        break;
    case GS1_ENGINE_MESSAGE_CAMPAIGN_RESOURCES:
        campaign_resources_ = message.payload_as<Gs1EngineMessageCampaignResourcesData>();
        break;
    default:
        break;
    }

    dirty_ = true;
    refresh_if_needed();
}

void Gs1GodotRegionalMapHudController::handle_runtime_message_reset()
{
    regional_map_state_reducer_.reset();
    ui_panel_state_reducer_.reset();
    current_app_state_.reset();
    campaign_resources_.reset();
    dirty_ = true;
    refresh_if_needed();
}

void Gs1GodotRegionalMapHudController::refresh_if_needed()
{
    if (!dirty_)
    {
        return;
    }

    const int app_state = current_app_state_.has_value() ? static_cast<int>(current_app_state_.value()) : 0;
    const bool visible = app_state == GS1_APP_STATE_REGIONAL_MAP || app_state == GS1_APP_STATE_SITE_LOADING;
    if (hud_ != nullptr)
    {
        hud_->set_visible(visible);
    }
    if (!visible)
    {
        dirty_ = false;
        return;
    }

    if (selected_site_summary_ != nullptr)
    {
        selected_site_summary_->set_text(selected_site_text());
    }
    if (campaign_summary_ != nullptr)
    {
        campaign_summary_->set_text(campaign_summary_text());
    }
    if (tech_button_ != nullptr)
    {
        const Gs1UiAction action = tech_button_action();
        tech_button_->set_text(tech_button_label());
        tech_button_->set_disabled(action.type == GS1_UI_ACTION_NONE);
        tech_button_->set_meta("action_type", static_cast<int>(action.type));
        tech_button_->set_meta("target_id", static_cast<int>(action.target_id));
        tech_button_->set_meta("arg0", static_cast<std::int64_t>(action.arg0));
        tech_button_->set_meta("arg1", static_cast<std::int64_t>(action.arg1));
    }

    dirty_ = false;
}

String Gs1GodotRegionalMapHudController::selected_site_text() const
{
    const Gs1RuntimeRegionalMapSiteProjection* site = selected_site();
    if (site == nullptr)
    {
        return "[b]Selected Site[/b]\nNone";
    }

    const int map_x = Math::round(site->map_x / 160.0);
    const int map_y = Math::round(site->map_y / 160.0);
    PackedStringArray lines;
    lines.push_back(vformat("[b]Site %d[/b]", static_cast<int>(site->site_id)));
    lines.push_back(vformat("Status: %s", site_state_name(static_cast<int>(site->site_state))));
    lines.push_back(vformat("Grid: (%d, %d)", map_x, map_y));
    lines.push_back(vformat("Support: %s", support_preview_text(static_cast<int>(site->support_preview_mask))));
    return String("\n").join(lines);
}

String Gs1GodotRegionalMapHudController::campaign_summary_text() const
{
    const auto& state = regional_map_state_reducer_.state();
    int available_count = 0;
    int completed_count = 0;
    for (const auto& site : state.sites)
    {
        if (site.site_state == GS1_SITE_STATE_AVAILABLE)
        {
            ++available_count;
        }
        else if (site.site_state == GS1_SITE_STATE_COMPLETED)
        {
            ++completed_count;
        }
    }

    PackedStringArray lines;
    lines.push_back(vformat("Revealed: %d  Available: %d", static_cast<int>(state.sites.size()), available_count));
    lines.push_back(vformat("Completed Support: %d", completed_count));
    if (campaign_resources_.has_value())
    {
        lines.push_back(vformat("Cash: %.2f  Rep: %d", campaign_resources_->current_money, campaign_resources_->total_reputation));
    }
    return String("\n").join(lines);
}

String Gs1GodotRegionalMapHudController::site_state_name(int site_state) const
{
    switch (site_state)
    {
    case GS1_SITE_STATE_AVAILABLE:
        return "Available";
    case GS1_SITE_STATE_COMPLETED:
        return "Completed";
    default:
        return "Locked";
    }
}

String Gs1GodotRegionalMapHudController::support_preview_text(int preview_mask) const
{
    PackedStringArray support_lines;
    if ((preview_mask & 1) != 0)
    {
        support_lines.push_back("loadout");
    }
    if ((preview_mask & 2) != 0)
    {
        support_lines.push_back("weather shelter");
    }
    if ((preview_mask & 4) != 0)
    {
        support_lines.push_back("fertility");
    }
    if ((preview_mask & 8) != 0)
    {
        support_lines.push_back("recovery");
    }
    return support_lines.is_empty() ? String("None") : String(", ").join(support_lines);
}

String Gs1GodotRegionalMapHudController::tech_button_label() const
{
    const Gs1RuntimeUiPanelProjection* panel = ui_panel_state_reducer_.find_panel(GS1_UI_PANEL_REGIONAL_MAP);
    if (panel == nullptr || panel->slot_actions.empty())
    {
        return "Research & Unlocks";
    }

    for (const auto& slot_action : panel->slot_actions)
    {
        if (slot_action.slot_id == GS1_UI_PANEL_SLOT_PRIMARY)
        {
            return static_cast<int>(slot_action.label_kind) == 2 ? String("Close Research") : String("Research & Unlocks");
        }
    }
    return "Research & Unlocks";
}

Gs1UiAction Gs1GodotRegionalMapHudController::tech_button_action() const
{
    const Gs1RuntimeUiPanelProjection* panel = ui_panel_state_reducer_.find_panel(GS1_UI_PANEL_REGIONAL_MAP);
    if (panel != nullptr)
    {
        for (const auto& slot_action : panel->slot_actions)
        {
            if (slot_action.slot_id == GS1_UI_PANEL_SLOT_PRIMARY)
            {
                return slot_action.action;
            }
        }
    }

    Gs1UiAction action {};
    action.type = GS1_UI_ACTION_OPEN_REGIONAL_MAP_TECH_TREE;
    return action;
}

const Gs1RuntimeRegionalMapSiteProjection* Gs1GodotRegionalMapHudController::selected_site() const
{
    const auto& state = regional_map_state_reducer_.state();
    if (state.sites.empty())
    {
        return nullptr;
    }

    const std::uint32_t selected_site_id = state.selected_site_id.has_value()
        ? state.selected_site_id.value()
        : state.sites.front().site_id;
    for (const auto& site : state.sites)
    {
        if (site.site_id == selected_site_id)
        {
            return &site;
        }
    }
    return &state.sites.front();
}
