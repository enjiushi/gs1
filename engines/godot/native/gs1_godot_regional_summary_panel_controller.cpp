#include "gs1_godot_regional_summary_panel_controller.h"

#include "gs1_godot_controller_context.h"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>

#include <algorithm>
#include <unordered_set>

using namespace godot;

namespace
{
constexpr std::uint64_t regional_map_link_key(std::uint32_t from_site_id, std::uint32_t to_site_id) noexcept
{
    return (static_cast<std::uint64_t>(from_site_id) << 32U) | static_cast<std::uint64_t>(to_site_id);
}

constexpr std::uint64_t normalized_regional_map_link_key(
    std::uint32_t site_a,
    std::uint32_t site_b) noexcept
{
    const std::uint32_t low = std::min(site_a, site_b);
    const std::uint32_t high = std::max(site_a, site_b);
    return regional_map_link_key(low, high);
}

void sort_regional_map_sites(
    std::vector<Gs1GodotRegionalSummaryPanelController::RegionalSiteState>& sites)
{
    std::sort(sites.begin(), sites.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.site_id < rhs.site_id;
    });
}

void sort_regional_map_links(
    std::vector<Gs1GodotRegionalSummaryPanelController::RegionalLinkState>& links)
{
    std::sort(links.begin(), links.end(), [](const auto& lhs, const auto& rhs) {
        return regional_map_link_key(lhs.from_site_id, lhs.to_site_id) < regional_map_link_key(rhs.from_site_id, rhs.to_site_id);
    });
}

const Gs1GodotRegionalSummaryPanelController::RegionalSiteState* find_selected_site(
    const std::vector<Gs1GodotRegionalSummaryPanelController::RegionalSiteState>& sites,
    const std::optional<std::uint32_t>& selected_site_id)
{
    if (!selected_site_id.has_value())
    {
        return nullptr;
    }

    const auto found = std::find_if(sites.begin(), sites.end(), [&](const auto& site) {
        return site.site_id == selected_site_id.value();
    });
    return found == sites.end() ? nullptr : &(*found);
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
    refresh_from_game_state_view();
    apply_summary_text();
    apply_graph_text();
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
    return type == GS1_ENGINE_MESSAGE_PRESENTATION_DIRTY ||
        type == GS1_ENGINE_MESSAGE_SET_APP_STATE;
}

void Gs1GodotRegionalSummaryPanelController::refresh_from_game_state_view()
{
    selected_site_id_.reset();
    sites_.clear();
    links_.clear();
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
    std::unordered_set<std::uint64_t> seen_links {};
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

        for (std::uint32_t adjacency_index = 0; adjacency_index < site_view.adjacent_site_id_count; ++adjacency_index)
        {
            const std::uint32_t adjacent_site_id =
                campaign.site_adjacency_ids[site_view.adjacent_site_id_offset + adjacency_index];
            if (adjacent_site_id == 0U || adjacent_site_id == site_view.site_id)
            {
                continue;
            }

            const std::uint64_t link_key =
                normalized_regional_map_link_key(site_view.site_id, adjacent_site_id);
            if (seen_links.insert(link_key).second)
            {
                links_.push_back(RegionalLinkState {
                    static_cast<std::uint32_t>(link_key >> 32U),
                    static_cast<std::uint32_t>(link_key & 0xffffffffU)});
            }
        }
    }

    sort_regional_map_sites(sites_);
    sort_regional_map_links(links_);
}

void Gs1GodotRegionalSummaryPanelController::handle_engine_message(const Gs1EngineMessage& message)
{
    if (message.type == GS1_ENGINE_MESSAGE_PRESENTATION_DIRTY ||
        message.type == GS1_ENGINE_MESSAGE_SET_APP_STATE)
    {
        refresh_from_game_state_view();
        apply_summary_text();
        apply_graph_text();
    }
}

void Gs1GodotRegionalSummaryPanelController::handle_runtime_message_reset()
{
    selected_site_id_.reset();
    sites_.clear();
    links_.clear();
    total_reputation_ = 0;
    apply_summary_text();
    apply_graph_text();
}

void Gs1GodotRegionalSummaryPanelController::apply_summary_text()
{
    const auto* selected_site = find_selected_site(sites_, selected_site_id_);

    PackedStringArray lines;
    lines.push_back("[b]Campaign Planning Map[/b]");
    lines.push_back(vformat(
        "Prototype Sites: %d  Adjacency Links: %d",
        static_cast<int>(sites_.size()),
        static_cast<int>(links_.size())));
    lines.push_back(
        selected_site != nullptr
            ? vformat("Selected Site: Site %d", static_cast<int>(selected_site->site_id))
            : String("Selected Site: None"));
    lines.push_back(vformat("Total Reputation: %d", total_reputation_));
    lines.push_back("Open the tech tree here, then choose a site marker to review support and deploy.");

    if (regional_map_summary_ != nullptr)
    {
        regional_map_summary_->set_text(String("\n").join(lines));
    }
}

void Gs1GodotRegionalSummaryPanelController::apply_graph_text()
{
    if (regional_map_graph_ != nullptr)
    {
        regional_map_graph_->set_text(build_regional_map_overview_text(sites_, links_));
    }
}

String Gs1GodotRegionalSummaryPanelController::build_regional_map_overview_text(
    const std::vector<RegionalSiteState>& sites,
    const std::vector<RegionalLinkState>& links) const
{
    PackedStringArray lines;
    lines.push_back("[b]Regional Situation[/b]");
    if (sites.empty())
    {
        lines.push_back("No revealed sites yet.");
        return String("\n").join(lines);
    }

    int available_count = 0;
    int blocked_count = 0;
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
        else
        {
            ++blocked_count;
        }
    }

    lines.push_back(vformat(
        "Available: %d  Blocked: %d  Completed Support Sites: %d",
        available_count,
        blocked_count,
        completed_count));
    lines.push_back("Known routes cut across the dune basin, with blocked outposts held on the map in a greyed reserve state.");
    lines.push_back(String());
    lines.push_back("[b]Adjacency[/b]");

    for (const auto& link : links)
    {
        lines.push_back(vformat("Site %d <-> Site %d", static_cast<int>(link.from_site_id), static_cast<int>(link.to_site_id)));
    }

    return String("\n").join(lines);
}
