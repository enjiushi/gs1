#include "gs1_godot_regional_summary_panel_controller.h"

#include <godot_cpp/variant/packed_string_array.hpp>

using namespace godot;

void Gs1GodotRegionalSummaryPanelController::cache_ui_references(Control& owner)
{
    if (regional_map_summary_ == nullptr)
    {
        regional_map_summary_ = Object::cast_to<RichTextLabel>(owner.find_child("RegionalMapSummary", true, false));
    }
    if (regional_map_graph_ == nullptr)
    {
        regional_map_graph_ = Object::cast_to<RichTextLabel>(owner.find_child("RegionalMapGraph", true, false));
    }
    refresh_if_needed();
}

bool Gs1GodotRegionalSummaryPanelController::handles_engine_message(Gs1EngineMessageType type) const noexcept
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
    case GS1_ENGINE_MESSAGE_CAMPAIGN_RESOURCES:
        return true;
    default:
        return false;
    }
}

void Gs1GodotRegionalSummaryPanelController::handle_engine_message(const Gs1EngineMessage& message)
{
    switch (message.type)
    {
    case GS1_ENGINE_MESSAGE_SET_APP_STATE:
        current_app_state_ = message.payload_as<Gs1EngineMessageSetAppStateData>().app_state;
        regional_map_state_reducer_.apply_engine_message(message);
        break;
    case GS1_ENGINE_MESSAGE_CAMPAIGN_RESOURCES:
        campaign_resources_ = message.payload_as<Gs1EngineMessageCampaignResourcesData>();
        break;
    default:
        regional_map_state_reducer_.apply_engine_message(message);
        break;
    }
    dirty_ = true;
    refresh_if_needed();
}

void Gs1GodotRegionalSummaryPanelController::handle_runtime_message_reset()
{
    regional_map_state_reducer_.reset();
    current_app_state_.reset();
    campaign_resources_.reset();
    dirty_ = true;
    refresh_if_needed();
}

void Gs1GodotRegionalSummaryPanelController::refresh_if_needed()
{
    if (!dirty_)
    {
        return;
    }

    const int app_state = current_app_state_.has_value() ? static_cast<int>(current_app_state_.value()) : 0;
    if (app_state != 3 && app_state != 4)
    {
        dirty_ = false;
        return;
    }

    const auto& regional_state = regional_map_state_reducer_.state();
    const auto& sites = regional_state.sites;
    const auto& links = regional_state.links;
    const int selected_site_id = regional_state.selected_site_id.has_value()
        ? static_cast<int>(regional_state.selected_site_id.value())
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

    dirty_ = false;
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
