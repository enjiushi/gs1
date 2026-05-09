#include "gs1_godot_regional_map_hud_controller.h"

#include "gs1_godot_controller_context.h"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/object.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>

#include <cstdint>
#include <algorithm>
#include <utility>

using namespace godot;

namespace
{
template <typename Projection, typename Key, typename KeyFn>
void upsert_projection(std::vector<Projection>& projections, Projection projection, Key key, KeyFn&& key_fn)
{
    const auto existing = std::find_if(projections.begin(), projections.end(), [&](const Projection& item) {
        return key_fn(item) == key;
    });
    if (existing != projections.end())
    {
        *existing = std::move(projection);
    }
    else
    {
        projections.push_back(std::move(projection));
    }
}

template <typename Projection, typename Predicate>
void erase_projection_if(std::vector<Projection>& projections, Predicate&& predicate)
{
    projections.erase(
        std::remove_if(projections.begin(), projections.end(), std::forward<Predicate>(predicate)),
        projections.end());
}

std::uint64_t regional_map_link_key(std::uint32_t from_site_id, std::uint32_t to_site_id)
{
    return (static_cast<std::uint64_t>(from_site_id) << 32U) | static_cast<std::uint64_t>(to_site_id);
}

void sort_regional_map_sites(std::vector<Gs1RuntimeRegionalMapSiteProjection>& sites)
{
    std::sort(sites.begin(), sites.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.site_id < rhs.site_id;
    });
}

void sort_regional_map_links(std::vector<Gs1RuntimeRegionalMapLinkProjection>& links)
{
    std::sort(links.begin(), links.end(), [](const auto& lhs, const auto& rhs) {
        return regional_map_link_key(lhs.from_site_id, lhs.to_site_id) < regional_map_link_key(rhs.from_site_id, rhs.to_site_id);
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
    apply_selected_site_summary();
    apply_campaign_summary();
    apply_tech_button();
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

void Gs1GodotRegionalMapHudController::reset_regional_map_state() noexcept
{
    selected_site_id_.reset();
    sites_.clear();
    links_.clear();
    pending_regional_map_state_.reset();
}

void Gs1GodotRegionalMapHudController::apply_regional_map_message(const Gs1EngineMessage& message)
{
    if (message.type == GS1_ENGINE_MESSAGE_SET_APP_STATE)
    {
        const auto& payload = message.payload_as<Gs1EngineMessageSetAppStateData>();
        if (payload.app_state == GS1_APP_STATE_MAIN_MENU)
        {
            selected_site_id_.reset();
            sites_.clear();
            links_.clear();
        }
    }
    else if (message.type == GS1_ENGINE_MESSAGE_BEGIN_REGIONAL_MAP_HUD_SNAPSHOT)
    {
        const auto& payload = message.payload_as<Gs1EngineMessageRegionalMapSnapshotData>();
        if (payload.mode == GS1_PROJECTION_MODE_DELTA)
        {
            pending_regional_map_state_ = PendingRegionalMapState {sites_, links_};
        }
        else
        {
            pending_regional_map_state_ = PendingRegionalMapState {};
        }

        if (payload.selected_site_id != 0U)
        {
            selected_site_id_ = payload.selected_site_id;
        }
        else
        {
            selected_site_id_.reset();
        }
    }
    else if (message.type == GS1_ENGINE_MESSAGE_REGIONAL_MAP_HUD_SITE_UPSERT)
    {
        if (!pending_regional_map_state_.has_value())
        {
            return;
        }
        Gs1RuntimeRegionalMapSiteProjection projection = message.payload_as<Gs1EngineMessageRegionalMapSiteData>();
        upsert_projection(pending_regional_map_state_->sites, projection, projection.site_id, [](const auto& site) {
            return site.site_id;
        });
        if ((projection.flags & 1U) != 0U)
        {
            selected_site_id_ = projection.site_id;
        }
    }
    else if (message.type == GS1_ENGINE_MESSAGE_REGIONAL_MAP_HUD_SITE_REMOVE)
    {
        if (!pending_regional_map_state_.has_value())
        {
            return;
        }
        const auto& payload = message.payload_as<Gs1EngineMessageRegionalMapSiteData>();
        erase_projection_if(pending_regional_map_state_->sites, [&](const auto& site) {
            return site.site_id == payload.site_id;
        });
        if (selected_site_id_.has_value() && selected_site_id_.value() == payload.site_id)
        {
            selected_site_id_.reset();
        }
    }
    else if (message.type == GS1_ENGINE_MESSAGE_REGIONAL_MAP_HUD_LINK_UPSERT)
    {
        if (!pending_regional_map_state_.has_value())
        {
            return;
        }
        Gs1RuntimeRegionalMapLinkProjection projection = message.payload_as<Gs1EngineMessageRegionalMapLinkData>();
        upsert_projection(pending_regional_map_state_->links, projection, regional_map_link_key(projection.from_site_id, projection.to_site_id), [](const auto& link) {
            return regional_map_link_key(link.from_site_id, link.to_site_id);
        });
    }
    else if (message.type == GS1_ENGINE_MESSAGE_REGIONAL_MAP_HUD_LINK_REMOVE)
    {
        if (!pending_regional_map_state_.has_value())
        {
            return;
        }
        const auto& payload = message.payload_as<Gs1EngineMessageRegionalMapLinkData>();
        const auto key = regional_map_link_key(payload.from_site_id, payload.to_site_id);
        erase_projection_if(pending_regional_map_state_->links, [&](const auto& link) {
            return regional_map_link_key(link.from_site_id, link.to_site_id) == key;
        });
    }
    else if (message.type == GS1_ENGINE_MESSAGE_END_REGIONAL_MAP_HUD_SNAPSHOT)
    {
        if (!pending_regional_map_state_.has_value())
        {
            return;
        }
        sort_regional_map_sites(pending_regional_map_state_->sites);
        sort_regional_map_links(pending_regional_map_state_->links);
        sites_ = std::move(pending_regional_map_state_->sites);
        links_ = std::move(pending_regional_map_state_->links);
        pending_regional_map_state_.reset();
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
    case GS1_ENGINE_MESSAGE_BEGIN_REGIONAL_MAP_HUD_SNAPSHOT:
    case GS1_ENGINE_MESSAGE_REGIONAL_MAP_HUD_SITE_UPSERT:
    case GS1_ENGINE_MESSAGE_REGIONAL_MAP_HUD_SITE_REMOVE:
    case GS1_ENGINE_MESSAGE_REGIONAL_MAP_HUD_LINK_UPSERT:
    case GS1_ENGINE_MESSAGE_REGIONAL_MAP_HUD_LINK_REMOVE:
    case GS1_ENGINE_MESSAGE_END_REGIONAL_MAP_HUD_SNAPSHOT:
    case GS1_ENGINE_MESSAGE_CAMPAIGN_RESOURCES:
    case GS1_ENGINE_MESSAGE_SET_UI_SURFACE_VISIBILITY:
        return true;
    default:
        return false;
    }
}

void Gs1GodotRegionalMapHudController::handle_engine_message(const Gs1EngineMessage& message)
{
    apply_regional_map_message(message);

    switch (message.type)
    {
    case GS1_ENGINE_MESSAGE_END_REGIONAL_MAP_HUD_SNAPSHOT:
        apply_selected_site_summary();
        apply_campaign_summary();
        break;
    case GS1_ENGINE_MESSAGE_CAMPAIGN_RESOURCES:
        campaign_resources_ = message.payload_as<Gs1EngineMessageCampaignResourcesData>();
        apply_campaign_summary();
        break;
    case GS1_ENGINE_MESSAGE_SET_UI_SURFACE_VISIBILITY:
    {
        const auto& payload = message.payload_as<Gs1EngineMessageUiSurfaceVisibilityData>();
        if (payload.surface_id == GS1_UI_SURFACE_REGIONAL_TECH_TREE_OVERLAY)
        {
            tech_tree_visible_ = payload.visible != 0U;
            apply_tech_button();
        }
        break;
    }
    default:
        break;
    }
}

void Gs1GodotRegionalMapHudController::handle_runtime_message_reset()
{
    reset_regional_map_state();
    campaign_resources_.reset();
    tech_tree_visible_ = false;
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
    return tech_tree_visible_ ? String("Close Research") : String("Research & Unlocks");
}

Gs1UiAction Gs1GodotRegionalMapHudController::tech_button_action() const
{
    Gs1UiAction action {};
    action.type = tech_tree_visible_
        ? GS1_UI_ACTION_CLOSE_REGIONAL_MAP_TECH_TREE
        : GS1_UI_ACTION_OPEN_REGIONAL_MAP_TECH_TREE;
    return action;
}

const Gs1RuntimeRegionalMapSiteProjection* Gs1GodotRegionalMapHudController::selected_site() const
{
    if (sites_.empty())
    {
        return nullptr;
    }

    const std::uint32_t selected_site_id = selected_site_id_.has_value()
        ? selected_site_id_.value()
        : sites_.front().site_id;
    for (const auto& site : sites_)
    {
        if (site.site_id == selected_site_id)
        {
            return &site;
        }
    }
    return &sites_.front();
}
