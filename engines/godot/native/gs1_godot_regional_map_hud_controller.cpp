#include "gs1_godot_regional_map_hud_controller.h"

#include "gs1_godot_controller_context.h"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/object.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>

#include <cstdint>
#include <algorithm>

using namespace godot;

namespace
{
void sort_regional_map_sites(
    std::vector<Gs1GodotRegionalMapHudController::RegionalSiteState>& sites)
{
    std::sort(sites.begin(), sites.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.site_id < rhs.site_id;
    });
}

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
    refresh_from_game_state_view();
    apply_selected_site_summary();
    apply_campaign_summary();
    apply_tech_button();
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

void Gs1GodotRegionalMapHudController::refresh_from_game_state_view()
{
    selected_site_id_.reset();
    sites_.clear();
    total_reputation_ = 0;

    if (adapter_service_ == nullptr)
    {
        return;
    }

    Gs1GameStateView view {};
    if (!adapter_service_->get_game_state_view(view) ||
        view.has_campaign == 0U ||
        view.campaign == nullptr)
    {
        return;
    }

    const Gs1CampaignStateView& campaign = *view.campaign;
    total_reputation_ = campaign.total_reputation;
    if (campaign.selected_site_id > 0)
    {
        selected_site_id_ = static_cast<std::uint32_t>(campaign.selected_site_id);
    }

    sites_.reserve(campaign.site_count);
    for (std::uint32_t site_index = 0; site_index < campaign.site_count; ++site_index)
    {
        const Gs1CampaignSiteView& site_view = campaign.sites[site_index];
        sites_.push_back(RegionalSiteState {
            site_view.site_id,
            site_view.site_state,
            site_view.regional_map_tile_x,
            site_view.regional_map_tile_y,
            site_view.support_package_id,
            site_view.exported_support_item_count,
            site_view.nearby_aura_modifier_id_count});
    }

    sort_regional_map_sites(sites_);
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
    return type == GS1_ENGINE_MESSAGE_PRESENTATION_DIRTY ||
        type == GS1_ENGINE_MESSAGE_SET_APP_STATE;
}

void Gs1GodotRegionalMapHudController::handle_engine_message(const Gs1EngineMessage& message)
{
    if (message.type == GS1_ENGINE_MESSAGE_PRESENTATION_DIRTY ||
        message.type == GS1_ENGINE_MESSAGE_SET_APP_STATE)
    {
        refresh_from_game_state_view();
        apply_selected_site_summary();
        apply_campaign_summary();
        apply_tech_button();
    }
}

void Gs1GodotRegionalMapHudController::handle_runtime_message_reset()
{
    selected_site_id_.reset();
    sites_.clear();
    total_reputation_ = 0;
    apply_selected_site_summary();
    apply_campaign_summary();
    apply_tech_button();
}

void Gs1GodotRegionalMapHudController::apply_selected_site_summary()
{
    if (selected_site_summary_ != nullptr)
    {
        selected_site_summary_->set_text(selected_site_text());
    }
}

void Gs1GodotRegionalMapHudController::apply_campaign_summary()
{
    if (campaign_summary_ != nullptr)
    {
        campaign_summary_->set_text(campaign_summary_text());
    }
}

void Gs1GodotRegionalMapHudController::apply_tech_button()
{
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
}

String Gs1GodotRegionalMapHudController::selected_site_text() const
{
    const RegionalSiteState* site = selected_site();
    if (site == nullptr)
    {
        return "[b]Selected Site[/b]\nNone";
    }

    PackedStringArray lines;
    lines.push_back(vformat("[b]Site %d[/b]", static_cast<int>(site->site_id)));
    lines.push_back(vformat("Status: %s", site_state_name(site->site_state)));
    lines.push_back(vformat("Grid: (%d, %d)", site->grid_x, site->grid_y));
    lines.push_back(vformat("Support: %s", support_preview_text(*site)));
    return String("\n").join(lines);
}

String Gs1GodotRegionalMapHudController::campaign_summary_text() const
{
    int available_count = 0;
    int completed_count = 0;
    for (const auto& site : sites_)
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
    lines.push_back(vformat("Revealed: %d  Available: %d", static_cast<int>(sites_.size()), available_count));
    lines.push_back(vformat("Completed Support: %d", completed_count));
    lines.push_back(vformat("Rep: %d", total_reputation_));
    return String("\n").join(lines);
}

String Gs1GodotRegionalMapHudController::site_state_name(Gs1SiteState site_state) const
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

String Gs1GodotRegionalMapHudController::support_preview_text(const RegionalSiteState& site) const
{
    PackedStringArray support_lines;
    if (site.exported_support_item_count > 0U)
    {
        support_lines.push_back(vformat("loadout x%d", static_cast<int>(site.exported_support_item_count)));
    }
    if (site.nearby_aura_modifier_count > 0U)
    {
        support_lines.push_back(vformat("auras x%d", static_cast<int>(site.nearby_aura_modifier_count)));
    }
    return support_lines.is_empty() ? String("None") : String(", ").join(support_lines);
}

String Gs1GodotRegionalMapHudController::tech_button_label() const
{
    const bool tech_tree_visible =
        adapter_service_ != nullptr &&
        adapter_service_->ui_session_state().regional_tech.open;
    return tech_tree_visible ? String("Close Research") : String("Research & Unlocks");
}

Gs1UiAction Gs1GodotRegionalMapHudController::tech_button_action() const
{
    Gs1UiAction action {};
    const bool tech_tree_visible =
        adapter_service_ != nullptr &&
        adapter_service_->ui_session_state().regional_tech.open;
    action.type = tech_tree_visible
        ? GS1_UI_ACTION_CLOSE_REGIONAL_MAP_TECH_TREE
        : GS1_UI_ACTION_OPEN_REGIONAL_MAP_TECH_TREE;
    return action;
}

const Gs1GodotRegionalMapHudController::RegionalSiteState* Gs1GodotRegionalMapHudController::selected_site() const
{
    if (!selected_site_id_.has_value())
    {
        return nullptr;
    }

    const auto found = std::find_if(sites_.begin(), sites_.end(), [&](const auto& site) {
        return site.site_id == selected_site_id_.value();
    });
    return found == sites_.end() ? nullptr : &(*found);
}
