#include "app/game_presentation_coordinator.h"

#include "campaign/systems/technology_system.h"
#include "content/defs/faction_defs.h"
#include "content/defs/modifier_defs.h"
#include "content/defs/technology_defs.h"

#include <algorithm>
#include <cmath>
#include <set>

namespace gs1
{
namespace
{
constexpr std::int32_t k_regional_map_tile_spacing = 160;

struct RegionalMapSnapshotMessageFamily final
{
    Gs1EngineMessageType begin_message;
    Gs1EngineMessageType site_upsert_message;
    Gs1EngineMessageType link_upsert_message;
    Gs1EngineMessageType end_message;
};

inline constexpr RegionalMapSnapshotMessageFamily k_regional_map_scene_snapshot_family {
    GS1_ENGINE_MESSAGE_BEGIN_REGIONAL_MAP_SNAPSHOT,
    GS1_ENGINE_MESSAGE_REGIONAL_MAP_SITE_UPSERT,
    GS1_ENGINE_MESSAGE_REGIONAL_MAP_LINK_UPSERT,
    GS1_ENGINE_MESSAGE_END_REGIONAL_MAP_SNAPSHOT};
inline constexpr RegionalMapSnapshotMessageFamily k_regional_map_hud_snapshot_family {
    GS1_ENGINE_MESSAGE_BEGIN_REGIONAL_MAP_HUD_SNAPSHOT,
    GS1_ENGINE_MESSAGE_REGIONAL_MAP_HUD_SITE_UPSERT,
    GS1_ENGINE_MESSAGE_REGIONAL_MAP_HUD_LINK_UPSERT,
    GS1_ENGINE_MESSAGE_END_REGIONAL_MAP_HUD_SNAPSHOT};
inline constexpr RegionalMapSnapshotMessageFamily k_regional_summary_snapshot_family {
    GS1_ENGINE_MESSAGE_BEGIN_REGIONAL_SUMMARY_SNAPSHOT,
    GS1_ENGINE_MESSAGE_REGIONAL_SUMMARY_SITE_UPSERT,
    GS1_ENGINE_MESSAGE_REGIONAL_SUMMARY_LINK_UPSERT,
    GS1_ENGINE_MESSAGE_END_REGIONAL_SUMMARY_SNAPSHOT};
inline constexpr RegionalMapSnapshotMessageFamily k_regional_selection_snapshot_family {
    GS1_ENGINE_MESSAGE_BEGIN_REGIONAL_SELECTION_SNAPSHOT,
    GS1_ENGINE_MESSAGE_REGIONAL_SELECTION_SITE_UPSERT,
    GS1_ENGINE_MESSAGE_REGIONAL_SELECTION_LINK_UPSERT,
    GS1_ENGINE_MESSAGE_END_REGIONAL_SELECTION_SNAPSHOT};

[[nodiscard]] bool app_state_supports_technology_tree(Gs1AppState app_state) noexcept
{
    return app_state == GS1_APP_STATE_REGIONAL_MAP ||
        app_state == GS1_APP_STATE_SITE_ACTIVE;
}

[[nodiscard]] Gs1RuntimeMessage make_engine_message(Gs1EngineMessageType type)
{
    Gs1RuntimeMessage message {};
    message.type = type;
    return message;
}

[[nodiscard]] std::uint32_t visible_loadout_slot_count(const LoadoutPlannerState& planner) noexcept
{
    std::uint32_t count = 0U;
    for (const auto& slot : planner.selected_loadout_slots)
    {
        if (slot.occupied && slot.item_id.value != 0U && slot.quantity > 0U)
        {
            count += 1U;
        }
    }

    return count;
}

[[nodiscard]] const SiteMetaState* find_site_meta(
    const CampaignState& campaign,
    std::uint32_t site_id) noexcept
{
    for (const auto& site : campaign.sites)
    {
        if (site.site_id.value == site_id)
        {
            return &site;
        }
    }

    return nullptr;
}

[[nodiscard]] bool is_site_revealed(
    const CampaignState& campaign,
    SiteId site_id) noexcept
{
    for (const auto revealed_site_id : campaign.regional_map_state.revealed_site_ids)
    {
        if (revealed_site_id == site_id)
        {
            return true;
        }
    }

    return false;
}

[[nodiscard]] bool site_contributes_to_selected_target(
    const CampaignState& campaign,
    const SiteMetaState& contributor) noexcept
{
    if (!campaign.regional_map_state.selected_site_id.has_value() ||
        contributor.site_state != GS1_SITE_STATE_COMPLETED)
    {
        return false;
    }

    const auto* selected_site =
        find_site_meta(campaign, campaign.regional_map_state.selected_site_id->value);
    if (selected_site == nullptr)
    {
        return false;
    }

    for (const auto adjacent_site_id : selected_site->adjacent_site_ids)
    {
        if (adjacent_site_id == contributor.site_id)
        {
            return true;
        }
    }

    return false;
}

enum RegionalSupportPreviewBits : std::uint16_t
{
    REGIONAL_SUPPORT_PREVIEW_ITEMS = 1U << 0,
    REGIONAL_SUPPORT_PREVIEW_WIND = 1U << 1,
    REGIONAL_SUPPORT_PREVIEW_FERTILITY = 1U << 2,
    REGIONAL_SUPPORT_PREVIEW_RECOVERY = 1U << 3
};

[[nodiscard]] std::uint16_t support_preview_mask_for_site(const SiteMetaState& site) noexcept
{
    std::uint16_t mask = 0U;
    for (const auto& slot : site.exported_support_items)
    {
        if (slot.occupied && slot.item_id.value != 0U && slot.quantity > 0U)
        {
            mask |= REGIONAL_SUPPORT_PREVIEW_ITEMS;
            break;
        }
    }

    for (const auto modifier_id : site.nearby_aura_modifier_ids)
    {
        const auto* modifier_def = find_modifier_def(modifier_id);
        if (modifier_def == nullptr)
        {
            continue;
        }

        const auto& totals = modifier_def->totals;
        if (std::fabs(totals.wind) > 1e-4f ||
            std::fabs(totals.heat) > 1e-4f ||
            std::fabs(totals.dust) > 1e-4f)
        {
            mask |= REGIONAL_SUPPORT_PREVIEW_WIND;
        }
        if (std::fabs(totals.moisture) > 1e-4f ||
            std::fabs(totals.fertility) > 1e-4f ||
            std::fabs(totals.plant_density) > 1e-4f ||
            std::fabs(totals.salinity) > 1e-4f ||
            std::fabs(totals.growth_pressure) > 1e-4f)
        {
            mask |= REGIONAL_SUPPORT_PREVIEW_FERTILITY;
        }
        if (std::fabs(totals.health) > 1e-4f ||
            std::fabs(totals.hydration) > 1e-4f ||
            std::fabs(totals.nourishment) > 1e-4f ||
            std::fabs(totals.energy_cap) > 1e-4f ||
            std::fabs(totals.energy) > 1e-4f ||
            std::fabs(totals.morale) > 1e-4f ||
            std::fabs(totals.work_efficiency) > 1e-4f)
        {
            mask |= REGIONAL_SUPPORT_PREVIEW_RECOVERY;
        }
    }

    return mask;
}
}  // namespace

void GamePresentationCoordinator::queue_regional_map_menu_ui_messages()
{
    if (!campaign().has_value() || app_state() != GS1_APP_STATE_REGIONAL_MAP)
    {
        queue_clear_ui_panel_messages(GS1_UI_PANEL_REGIONAL_MAP);
        return;
    }

    const auto selected_faction_id = campaign()->regional_map_state.selected_tech_tree_faction_id.value != 0U
        ? campaign()->regional_map_state.selected_tech_tree_faction_id
        : FactionId {k_faction_village_committee};
    Gs1UiAction toggle_action {};
    toggle_action.type = campaign()->regional_map_state.tech_tree_open
        ? GS1_UI_ACTION_CLOSE_REGIONAL_MAP_TECH_TREE
        : GS1_UI_ACTION_OPEN_REGIONAL_MAP_TECH_TREE;

    std::uint32_t site_row_count = 0U;
    for (const auto revealed_site_id : campaign()->regional_map_state.revealed_site_ids)
    {
        if (find_site_meta(*campaign(), revealed_site_id.value) != nullptr)
        {
            ++site_row_count;
        }
    }

    queue_ui_panel_begin_message(
        GS1_UI_PANEL_REGIONAL_MAP,
        0U,
        1U,
        1U,
        site_row_count,
        site_row_count);
    queue_ui_panel_text_message(
        1U,
        0U,
        1U,
        selected_faction_id.value,
        static_cast<std::uint32_t>(TechnologySystem::faction_reputation(*campaign(), selected_faction_id)));
    queue_ui_panel_slot_action_message(
        GS1_UI_PANEL_SLOT_PRIMARY,
        GS1_UI_PANEL_SLOT_FLAG_PRIMARY,
        toggle_action,
        campaign()->regional_map_state.tech_tree_open ? 2U : 1U);

    for (const auto revealed_site_id : campaign()->regional_map_state.revealed_site_ids)
    {
        const SiteMetaState* site = find_site_meta(*campaign(), revealed_site_id.value);
        if (site == nullptr)
        {
            continue;
        }

        const bool selected = campaign()->regional_map_state.selected_site_id.has_value() &&
            campaign()->regional_map_state.selected_site_id->value == site->site_id.value;
        const std::uint32_t item_flags = selected ? GS1_UI_PANEL_LIST_ITEM_FLAG_SELECTED : GS1_UI_PANEL_LIST_ITEM_FLAG_NONE;

        queue_ui_panel_list_item_message(
            GS1_UI_PANEL_LIST_REGIONAL_SITES,
            site->site_id.value,
            item_flags,
            1U,
            2U,
            site->site_id.value,
            static_cast<std::uint32_t>(site->site_state),
            0U,
            static_cast<std::int32_t>(site->regional_map_tile.x),
            static_cast<std::int32_t>(site->regional_map_tile.y));

        Gs1UiAction select_action {};
        select_action.type = GS1_UI_ACTION_SELECT_DEPLOYMENT_SITE;
        select_action.target_id = site->site_id.value;
        queue_ui_panel_list_action_message(
            GS1_UI_PANEL_LIST_REGIONAL_SITES,
            site->site_id.value,
            GS1_UI_PANEL_LIST_ACTION_ROLE_PRIMARY,
            site->site_state == GS1_SITE_STATE_LOCKED ? GS1_UI_PANEL_LIST_ITEM_FLAG_DISABLED : GS1_UI_PANEL_LIST_ITEM_FLAG_NONE,
            select_action);
    }

    queue_ui_panel_end_message();
}

void GamePresentationCoordinator::queue_regional_map_selection_ui_messages()
{
    if (!campaign().has_value() || !campaign()->regional_map_state.selected_site_id.has_value())
    {
        if (ui_presentation_state().active_ui_panels.contains(GS1_UI_PANEL_REGIONAL_MAP_SELECTION))
        {
            auto close = make_engine_message(GS1_ENGINE_MESSAGE_CLOSE_REGIONAL_SELECTION_UI_PANEL);
            auto& close_payload = close.emplace_payload<Gs1EngineMessageCloseUiPanelData>();
            close_payload.panel_id = GS1_UI_PANEL_REGIONAL_MAP_SELECTION;
            engine_messages().push_back(close);
            queue_ui_surface_visibility_message(GS1_UI_SURFACE_REGIONAL_SELECTION_PANEL, false);
            ui_presentation_state().active_ui_panels.erase(GS1_UI_PANEL_REGIONAL_MAP_SELECTION);
        }
        return;
    }

    const auto site_id = campaign()->regional_map_state.selected_site_id->value;
    const auto loadout_item_count = visible_loadout_slot_count(campaign()->loadout_planner_state);
    const bool has_support_contributors = campaign()->loadout_planner_state.support_quota > 0U;
    const bool has_aura_support = !campaign()->loadout_planner_state.active_nearby_aura_modifier_ids.empty();
    const std::uint32_t summary_label_count =
        (has_support_contributors ? 1U : 0U) + (has_aura_support ? 1U : 0U);

    const std::uint32_t text_line_count = 1U + summary_label_count;
    auto begin = make_engine_message(GS1_ENGINE_MESSAGE_BEGIN_REGIONAL_SELECTION_UI_PANEL);
    auto& begin_payload = begin.emplace_payload<Gs1EngineMessageUiPanelData>();
    begin_payload.panel_id = GS1_UI_PANEL_REGIONAL_MAP_SELECTION;
    begin_payload.mode = GS1_PROJECTION_MODE_SNAPSHOT;
    begin_payload.context_id = site_id;
    begin_payload.text_line_count = static_cast<std::uint8_t>(std::min<std::uint32_t>(text_line_count, 255U));
    begin_payload.slot_action_count = 2U;
    begin_payload.list_item_count = static_cast<std::uint8_t>(std::min<std::uint32_t>(loadout_item_count, 255U));
    begin_payload.list_action_count = 0U;
    engine_messages().push_back(begin);
    queue_ui_surface_visibility_message(GS1_UI_SURFACE_REGIONAL_SELECTION_PANEL, true);
    ui_presentation_state().active_ui_panels.insert(GS1_UI_PANEL_REGIONAL_MAP_SELECTION);

    const auto queue_selection_text_message = [this](
                                                std::uint16_t line_id,
                                                std::uint32_t flags,
                                                std::uint8_t text_kind,
                                                std::uint32_t primary_id = 0U,
                                                std::uint32_t secondary_id = 0U,
                                                std::uint32_t quantity = 0U,
                                                std::uint32_t aux_value = 0U)
    {
        auto message = make_engine_message(GS1_ENGINE_MESSAGE_REGIONAL_SELECTION_UI_PANEL_TEXT_UPSERT);
        auto& payload = message.emplace_payload<Gs1EngineMessageUiPanelTextData>();
        payload.line_id = line_id;
        payload.flags = static_cast<std::uint8_t>(flags);
        payload.text_kind = text_kind;
        payload.primary_id = primary_id;
        payload.secondary_id = secondary_id;
        payload.quantity = quantity;
        payload.aux_value = aux_value;
        engine_messages().push_back(message);
    };
    const auto queue_selection_slot_action_message = [this](
                                                       Gs1UiPanelSlotId slot_id,
                                                       std::uint32_t flags,
                                                       const Gs1UiAction& action,
                                                       std::uint8_t label_kind,
                                                       std::uint32_t primary_id = 0U,
                                                       std::uint32_t secondary_id = 0U,
                                                       std::uint32_t quantity = 0U)
    {
        auto message = make_engine_message(GS1_ENGINE_MESSAGE_REGIONAL_SELECTION_UI_PANEL_SLOT_ACTION_UPSERT);
        auto& payload = message.emplace_payload<Gs1EngineMessageUiPanelSlotActionData>();
        payload.slot_id = slot_id;
        payload.flags = static_cast<std::uint8_t>(flags);
        payload.action = action;
        payload.label_kind = label_kind;
        payload.primary_id = primary_id;
        payload.secondary_id = secondary_id;
        payload.quantity = quantity;
        engine_messages().push_back(message);
    };
    const auto queue_selection_list_item_message = [this](
                                                     Gs1UiPanelListId list_id,
                                                     std::uint32_t item_id,
                                                     std::uint32_t flags,
                                                     std::uint8_t primary_kind,
                                                     std::uint8_t secondary_kind,
                                                     std::uint32_t primary_id = 0U,
                                                     std::uint32_t secondary_id = 0U,
                                                     std::uint32_t quantity = 0U,
                                                     std::int32_t map_x = 0,
                                                     std::int32_t map_y = 0)
    {
        auto message = make_engine_message(GS1_ENGINE_MESSAGE_REGIONAL_SELECTION_UI_PANEL_LIST_ITEM_UPSERT);
        auto& payload = message.emplace_payload<Gs1EngineMessageUiPanelListItemData>();
        payload.item_id = item_id;
        payload.list_id = list_id;
        payload.flags = static_cast<std::uint8_t>(flags);
        payload.primary_kind = primary_kind;
        payload.secondary_kind = secondary_kind;
        payload.primary_id = primary_id;
        payload.secondary_id = secondary_id;
        payload.quantity = quantity;
        payload.map_x = map_x;
        payload.map_y = map_y;
        engine_messages().push_back(message);
    };

    std::uint16_t next_line_id = 1U;
    queue_selection_text_message(next_line_id++, 0U, 3U, site_id);

    if (has_support_contributors)
    {
        queue_selection_text_message(
            next_line_id++,
            0U,
            4U,
            0U,
            0U,
            campaign()->loadout_planner_state.support_quota);
    }

    if (has_aura_support)
    {
        queue_selection_text_message(
            next_line_id++,
            0U,
            5U,
            0U,
            0U,
            static_cast<std::uint32_t>(campaign()->loadout_planner_state.active_nearby_aura_modifier_ids.size()));
    }

    std::uint32_t next_loadout_item_id = 1U;
    for (const auto& slot : campaign()->loadout_planner_state.selected_loadout_slots)
    {
        if (!slot.occupied || slot.item_id.value == 0U || slot.quantity == 0U)
        {
            continue;
        }

        queue_selection_list_item_message(
            GS1_UI_PANEL_LIST_REGIONAL_LOADOUT,
            next_loadout_item_id++,
            GS1_UI_PANEL_LIST_ITEM_FLAG_NONE,
            6U,
            0U,
            slot.item_id.value,
            0U,
            slot.quantity,
            0,
            0);
    }

    Gs1UiAction deploy_action {};
    deploy_action.type = GS1_UI_ACTION_START_SITE_ATTEMPT;
    deploy_action.target_id = site_id;
    queue_selection_slot_action_message(
        GS1_UI_PANEL_SLOT_PRIMARY,
        GS1_UI_PANEL_SLOT_FLAG_PRIMARY,
        deploy_action,
        3U,
        site_id);

    Gs1UiAction clear_selection_action {};
    clear_selection_action.type = GS1_UI_ACTION_CLEAR_DEPLOYMENT_SITE_SELECTION;
    queue_selection_slot_action_message(
        GS1_UI_PANEL_SLOT_SECONDARY,
        GS1_UI_PANEL_SLOT_FLAG_NONE,
        clear_selection_action,
        4U);

    engine_messages().push_back(make_engine_message(GS1_ENGINE_MESSAGE_END_REGIONAL_SELECTION_UI_PANEL));
}

void GamePresentationCoordinator::queue_regional_map_tech_tree_ui_messages()
{
    if (!campaign().has_value() ||
        !app_state_supports_technology_tree(app_state()) ||
        !campaign()->regional_map_state.tech_tree_open)
    {
        return;
    }

    queue_close_ui_setup_if_open(GS1_UI_SETUP_REGIONAL_MAP_TECH_TREE);

    const auto reputation_unlock_defs = all_reputation_unlock_defs();
    const auto technology_node_defs = all_technology_node_defs();
    const std::uint32_t entry_count =
        static_cast<std::uint32_t>(reputation_unlock_defs.size() + technology_node_defs.size() + 1U);
    queue_progression_view_begin_message(
        GS1_PROGRESSION_VIEW_REGIONAL_MAP_TECH_TREE,
        entry_count,
        0U);

    Gs1EngineMessageProgressionEntryData starter_entry {};
    starter_entry.entry_id = 0U;
    starter_entry.reputation_requirement = 0U;
    starter_entry.entry_kind = GS1_PROGRESSION_ENTRY_REPUTATION_UNLOCK;
    starter_entry.flags = GS1_PROGRESSION_ENTRY_FLAG_UNLOCKED;
    queue_progression_entry_message(starter_entry);

    for (const auto& unlock_def : reputation_unlock_defs)
    {
        Gs1EngineMessageProgressionEntryData entry {};
        entry.entry_id = static_cast<std::uint16_t>(unlock_def.unlock_id);
        entry.reputation_requirement = static_cast<std::uint16_t>(unlock_def.reputation_requirement);
        entry.content_id = static_cast<std::uint16_t>(unlock_def.content_id);
        entry.entry_kind = GS1_PROGRESSION_ENTRY_REPUTATION_UNLOCK;
        entry.content_kind = static_cast<std::uint8_t>(unlock_def.unlock_kind);
        entry.flags = campaign()->technology_state.total_reputation >= unlock_def.reputation_requirement
            ? GS1_PROGRESSION_ENTRY_FLAG_UNLOCKED
            : GS1_PROGRESSION_ENTRY_FLAG_LOCKED;
        queue_progression_entry_message(entry);
    }

    for (const auto& node_def : technology_node_defs)
    {
        const auto reputation_requirement = TechnologySystem::current_reputation_requirement(node_def);

        Gs1EngineMessageProgressionEntryData entry {};
        entry.entry_id = static_cast<std::uint16_t>(node_def.tech_node_id.value);
        entry.reputation_requirement = static_cast<std::uint16_t>(reputation_requirement);
        entry.tech_node_id = static_cast<std::uint16_t>(node_def.tech_node_id.value);
        entry.faction_id = static_cast<std::uint8_t>(node_def.faction_id.value);
        entry.entry_kind = GS1_PROGRESSION_ENTRY_TECHNOLOGY_NODE;
        entry.tier_index = node_def.tier_index;
        entry.action.type = GS1_UI_ACTION_NONE;
        entry.action.target_id = node_def.tech_node_id.value;
        entry.action.arg0 = node_def.faction_id.value;
        entry.flags = TechnologySystem::node_purchased(*campaign(), node_def.tech_node_id)
            ? GS1_PROGRESSION_ENTRY_FLAG_UNLOCKED
            : GS1_PROGRESSION_ENTRY_FLAG_LOCKED;
        queue_progression_entry_message(entry);
    }

    queue_progression_view_end_message();
}

void GamePresentationCoordinator::queue_regional_map_snapshot_messages()
{
    if (!campaign().has_value())
    {
        return;
    }

    std::set<std::pair<std::uint32_t, std::uint32_t>> unique_links;
    for (const auto& site : campaign()->sites)
    {
        for (const auto& adjacent_site_id : site.adjacent_site_ids)
        {
            const auto low = (site.site_id.value < adjacent_site_id.value) ? site.site_id.value : adjacent_site_id.value;
            const auto high = (site.site_id.value < adjacent_site_id.value) ? adjacent_site_id.value : site.site_id.value;
            unique_links.insert({low, high});
        }
    }

    std::uint32_t revealed_site_count = 0U;
    std::uint32_t revealed_link_count = 0U;
    for (const auto& site : campaign()->sites)
    {
        if (is_site_revealed(*campaign(), site.site_id))
        {
            revealed_site_count += 1U;
        }
    }
    for (const auto& link : unique_links)
    {
        if (!is_site_revealed(*campaign(), SiteId {link.first}) ||
            !is_site_revealed(*campaign(), SiteId {link.second}))
        {
            continue;
        }

        revealed_link_count += 1U;
    }

    const auto queue_family = [this, &unique_links, revealed_link_count, revealed_site_count](
                                  const RegionalMapSnapshotMessageFamily& family)
    {
        auto begin = make_engine_message(family.begin_message);
        auto& begin_payload = begin.emplace_payload<Gs1EngineMessageRegionalMapSnapshotData>();
        begin_payload.mode = GS1_PROJECTION_MODE_SNAPSHOT;
        begin_payload.link_count = revealed_link_count;
        begin_payload.selected_site_id =
            campaign()->regional_map_state.selected_site_id.has_value()
                ? campaign()->regional_map_state.selected_site_id->value
                : 0U;
        begin_payload.site_count = revealed_site_count;
        engine_messages().push_back(begin);

        for (const auto& site : campaign()->sites)
        {
            if (!is_site_revealed(*campaign(), site.site_id))
            {
                continue;
            }

            queue_regional_map_site_upsert_message(site, family.site_upsert_message);
        }

        for (const auto& link : unique_links)
        {
            if (!is_site_revealed(*campaign(), SiteId {link.first}) ||
                !is_site_revealed(*campaign(), SiteId {link.second}))
            {
                continue;
            }

            auto link_message = make_engine_message(family.link_upsert_message);
            auto& payload = link_message.emplace_payload<Gs1EngineMessageRegionalMapLinkData>();
            payload.from_site_id = link.first;
            payload.to_site_id = link.second;
            engine_messages().push_back(link_message);
        }

        engine_messages().push_back(make_engine_message(family.end_message));
    };

    queue_family(k_regional_map_scene_snapshot_family);
    queue_family(k_regional_map_hud_snapshot_family);
    queue_family(k_regional_summary_snapshot_family);
    queue_family(k_regional_selection_snapshot_family);
}

void GamePresentationCoordinator::queue_regional_map_site_upsert_message(
    const SiteMetaState& site,
    Gs1EngineMessageType message_type)
{
    if (!campaign().has_value())
    {
        return;
    }

    auto site_message = make_engine_message(message_type);
    auto& payload = site_message.emplace_payload<Gs1EngineMessageRegionalMapSiteData>();
    payload.site_id = site.site_id.value;
    payload.site_state = site.site_state;
    payload.site_archetype_id = site.site_archetype_id;
    payload.flags =
        (campaign()->regional_map_state.selected_site_id.has_value() &&
            campaign()->regional_map_state.selected_site_id->value == site.site_id.value)
        ? 1U
        : 0U;

    payload.map_x = site.regional_map_tile.x * k_regional_map_tile_spacing;
    payload.map_y = site.regional_map_tile.y * k_regional_map_tile_spacing;
    payload.support_package_id =
        site.has_support_package_id ? site.support_package_id : 0U;
    payload.support_preview_mask =
        site_contributes_to_selected_target(*campaign(), site)
            ? support_preview_mask_for_site(site)
            : 0U;
    engine_messages().push_back(site_message);
}
}  // namespace gs1
