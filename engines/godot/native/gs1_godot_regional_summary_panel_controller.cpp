#include "gs1_godot_regional_summary_panel_controller.h"

#include "gs1_godot_controller_context.h"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>

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
}

void Gs1GodotRegionalSummaryPanelController::_bind_methods()
{
}

void Gs1GodotRegionalSummaryPanelController::_ready()
{
    cache_adapter_service();
    if (Control* owner = resolve_owner_control())
    {
        cache_ui_references(*owner);
    }
    set_process(true);
}

void Gs1GodotRegionalSummaryPanelController::_process(double delta)
{
    (void)delta;
}

void Gs1GodotRegionalSummaryPanelController::_exit_tree()
{
    if (adapter_service_ != nullptr)
    {
        adapter_service_->unsubscribe_all(*this);
        adapter_service_ = nullptr;
    }
}

void Gs1GodotRegionalSummaryPanelController::cache_ui_references(Control& owner)
{
    owner_control_ = &owner;
    if (regional_map_summary_ == nullptr)
    {
        regional_map_summary_ = Object::cast_to<RichTextLabel>(owner.find_child("RegionalMapSummary", true, false));
    }
    if (regional_map_graph_ == nullptr)
    {
        regional_map_graph_ = Object::cast_to<RichTextLabel>(owner.find_child("RegionalMapGraph", true, false));
    }
    rebuild_summary();
}

void Gs1GodotRegionalSummaryPanelController::cache_adapter_service()
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

Control* Gs1GodotRegionalSummaryPanelController::resolve_owner_control()
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

bool Gs1GodotRegionalSummaryPanelController::handles_engine_message(Gs1EngineMessageType type) const noexcept
{
    switch (type)
    {
    case GS1_ENGINE_MESSAGE_BEGIN_REGIONAL_SUMMARY_SNAPSHOT:
    case GS1_ENGINE_MESSAGE_REGIONAL_SUMMARY_SITE_UPSERT:
    case GS1_ENGINE_MESSAGE_REGIONAL_SUMMARY_SITE_REMOVE:
    case GS1_ENGINE_MESSAGE_REGIONAL_SUMMARY_LINK_UPSERT:
    case GS1_ENGINE_MESSAGE_REGIONAL_SUMMARY_LINK_REMOVE:
    case GS1_ENGINE_MESSAGE_END_REGIONAL_SUMMARY_SNAPSHOT:
    case GS1_ENGINE_MESSAGE_CAMPAIGN_RESOURCES:
        return true;
    default:
        return false;
    }
}

void Gs1GodotRegionalSummaryPanelController::reset_regional_map_state() noexcept
{
    selected_site_id_.reset();
    sites_.clear();
    links_.clear();
    pending_regional_map_state_.reset();
}

void Gs1GodotRegionalSummaryPanelController::apply_regional_map_message(const Gs1EngineMessage& message)
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
    else if (message.type == GS1_ENGINE_MESSAGE_BEGIN_REGIONAL_SUMMARY_SNAPSHOT)
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
    else if (message.type == GS1_ENGINE_MESSAGE_REGIONAL_SUMMARY_SITE_UPSERT)
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
    else if (message.type == GS1_ENGINE_MESSAGE_REGIONAL_SUMMARY_SITE_REMOVE)
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
    else if (message.type == GS1_ENGINE_MESSAGE_REGIONAL_SUMMARY_LINK_UPSERT)
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
    else if (message.type == GS1_ENGINE_MESSAGE_REGIONAL_SUMMARY_LINK_REMOVE)
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
    else if (message.type == GS1_ENGINE_MESSAGE_END_REGIONAL_SUMMARY_SNAPSHOT)
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

void Gs1GodotRegionalSummaryPanelController::handle_engine_message(const Gs1EngineMessage& message)
{
    switch (message.type)
    {
    case GS1_ENGINE_MESSAGE_CAMPAIGN_RESOURCES:
        campaign_resources_ = message.payload_as<Gs1EngineMessageCampaignResourcesData>();
        break;
    default:
        apply_regional_map_message(message);
        break;
    }
    rebuild_summary();
}

void Gs1GodotRegionalSummaryPanelController::handle_runtime_message_reset()
{
    reset_regional_map_state();
    campaign_resources_.reset();
    rebuild_summary();
}

void Gs1GodotRegionalSummaryPanelController::rebuild_summary()
{
    const auto& sites = sites_;
    const auto& links = links_;
    const int selected_site_id = selected_site_id_.has_value()
        ? static_cast<int>(selected_site_id_.value())
        : (sites.empty() ? 0 : static_cast<int>(sites.front().site_id));

    PackedStringArray lines;
    lines.push_back("[b]Campaign Planning Map[/b]");
    lines.push_back(vformat("Revealed Sites: %d  Adjacency Links: %d", static_cast<int>(sites.size()), static_cast<int>(links.size())));
    lines.push_back(selected_site_id != 0 ? vformat("Selected Site: Site %d", selected_site_id) : String("Selected Site: None"));
    if (campaign_resources_.has_value())
    {
        lines.push_back(vformat("Campaign Cash: %.2f", campaign_resources_->current_money));
        lines.push_back(vformat("Total Reputation: %d", campaign_resources_->total_reputation));
    }
    lines.push_back("Open the tech tree here, then choose a site marker to review support and deploy.");

    if (regional_map_summary_ != nullptr)
    {
        regional_map_summary_->set_text(String("\n").join(lines));
    }
    if (regional_map_graph_ != nullptr)
    {
        regional_map_graph_->set_text(build_regional_map_overview_text(sites, links));
    }
}

String Gs1GodotRegionalSummaryPanelController::build_regional_map_overview_text(
    const std::vector<Gs1RuntimeRegionalMapSiteProjection>& sites,
    const std::vector<Gs1RuntimeRegionalMapLinkProjection>& links) const
{
    PackedStringArray lines;
    lines.push_back("[b]Regional Situation[/b]");
    if (sites.empty())
    {
        lines.push_back("No revealed sites yet.");
        return String("\n").join(lines);
    }

    int available_count = 0;
    int completed_count = 0;
    for (const auto& site : sites)
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

    lines.push_back(vformat("Available: %d  Completed Support Sites: %d", available_count, completed_count));
    lines.push_back("Known routes cut across the dune basin, with marked camp outposts standing in for active support sites.");
    lines.push_back(String());
    lines.push_back("[b]Adjacency[/b]");

    for (const auto& link : links)
    {
        lines.push_back(vformat("Site %d <-> Site %d", static_cast<int>(link.from_site_id), static_cast<int>(link.to_site_id)));
    }

    return String("\n").join(lines);
}
