#include "campaign/systems/campaign_presentation_system.h"

#include "campaign/systems/technology_system.h"
#include "content/defs/faction_defs.h"
#include "content/defs/modifier_defs.h"
#include "content/defs/technology_defs.h"
#include "support/currency.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <set>

namespace gs1
{
namespace
{
using CampaignPresentationSystemTags =
    type_list<
        RuntimeAppStateTag,
        RuntimeCampaignTag,
        RuntimeActiveSiteRunTag,
        RuntimeUiPresentationTag,
        RuntimePresentationRuntimeTag>;

constexpr std::int32_t k_regional_map_tile_spacing = 160;

enum CampaignUnlockCueDetailKind : std::uint32_t
{
    CAMPAIGN_UNLOCK_CUE_DETAIL_REPUTATION_UNLOCK = 1U,
    CAMPAIGN_UNLOCK_CUE_DETAIL_TECHNOLOGY_NODE = 2U
};

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

void queue_runtime_message(RuntimeInvocation& invocation, const Gs1RuntimeMessage& message)
{
    invocation.push_runtime_message(message);
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

[[nodiscard]] std::vector<std::uint32_t> capture_unlocked_reputation_unlock_ids(
    const CampaignState& campaign)
{
    std::vector<std::uint32_t> unlock_ids {};
    unlock_ids.reserve(all_reputation_unlock_defs().size());
    for (const auto& unlock_def : all_reputation_unlock_defs())
    {
        bool unlocked = false;
        switch (unlock_def.unlock_kind)
        {
        case ReputationUnlockKind::Plant:
            unlocked = TechnologySystem::plant_unlocked(campaign, PlantId {unlock_def.content_id});
            break;
        case ReputationUnlockKind::Item:
            unlocked = TechnologySystem::item_unlocked(campaign, ItemId {unlock_def.content_id});
            break;
        case ReputationUnlockKind::StructureRecipe:
            unlocked = TechnologySystem::structure_recipe_unlocked(campaign, StructureId {unlock_def.content_id});
            break;
        case ReputationUnlockKind::Recipe:
            unlocked = TechnologySystem::recipe_unlocked(campaign, RecipeId {unlock_def.content_id});
            break;
        default:
            break;
        }

        if (unlocked)
        {
            unlock_ids.push_back(unlock_def.unlock_id);
        }
    }

    return unlock_ids;
}

[[nodiscard]] std::vector<std::uint32_t> capture_unlocked_technology_node_ids(
    const CampaignState& campaign)
{
    std::vector<std::uint32_t> node_ids {};
    node_ids.reserve(all_technology_node_defs().size());
    for (const auto& node_def : all_technology_node_defs())
    {
        if (TechnologySystem::node_purchased(campaign, node_def.tech_node_id))
        {
            node_ids.push_back(node_def.tech_node_id.value);
        }
    }

    return node_ids;
}

[[nodiscard]] bool sorted_contains(
    const std::vector<std::uint32_t>& values,
    std::uint32_t value) noexcept
{
    return std::binary_search(values.begin(), values.end(), value);
}

void queue_log_message(RuntimeInvocation& invocation, const char* message, Gs1LogLevel level)
{
    auto engine_message = make_engine_message(GS1_ENGINE_MESSAGE_LOG_TEXT);
    auto& payload = engine_message.emplace_payload<Gs1EngineMessageLogTextData>();
    payload.level = level;
    strncpy_s(payload.text, sizeof(payload.text), message, _TRUNCATE);
    queue_runtime_message(invocation, engine_message);
}

void queue_ui_surface_visibility_message(
    RuntimeInvocation& invocation,
    Gs1UiSurfaceId surface_id,
    bool visible)
{
    if (surface_id == GS1_UI_SURFACE_NONE)
    {
        return;
    }

    auto message = make_engine_message(GS1_ENGINE_MESSAGE_SET_UI_SURFACE_VISIBILITY);
    auto& payload = message.emplace_payload<Gs1EngineMessageUiSurfaceVisibilityData>();
    payload.surface_id = surface_id;
    payload.visible = visible ? 1U : 0U;
    payload.reserved0 = 0U;
    queue_runtime_message(invocation, message);
}

void queue_app_state_message(
    RuntimeInvocation& invocation,
    UiPresentationState& ui_presentation,
    Gs1AppState app_state)
{
    if (ui_presentation.last_emitted_app_state.has_value() &&
        ui_presentation.last_emitted_app_state.value() == app_state)
    {
        return;
    }

    auto message = make_engine_message(GS1_ENGINE_MESSAGE_SET_APP_STATE);
    auto& payload = message.emplace_payload<Gs1EngineMessageSetAppStateData>();
    payload.app_state = app_state;
    queue_runtime_message(invocation, message);
    ui_presentation.last_emitted_app_state = app_state;
}

void queue_ui_setup_close_message(
    RuntimeInvocation& invocation,
    UiPresentationState& ui_presentation,
    Gs1UiSetupId setup_id,
    Gs1UiSetupPresentationType presentation_type)
{
    auto message = make_engine_message(GS1_ENGINE_MESSAGE_CLOSE_UI_SETUP);
    auto& payload = message.emplace_payload<Gs1EngineMessageCloseUiSetupData>();
    payload.setup_id = setup_id;
    payload.presentation_type = presentation_type;
    queue_runtime_message(invocation, message);

    ui_presentation.active_ui_setups.erase(setup_id);
    if (ui_presentation.active_normal_ui_setup.has_value() &&
        ui_presentation.active_normal_ui_setup.value() == setup_id)
    {
        ui_presentation.active_normal_ui_setup.reset();
    }
}

void queue_close_ui_setup_if_open(
    RuntimeInvocation& invocation,
    UiPresentationState& ui_presentation,
    Gs1UiSetupId setup_id)
{
    const auto it = ui_presentation.active_ui_setups.find(setup_id);
    if (it == ui_presentation.active_ui_setups.end())
    {
        return;
    }

    queue_ui_setup_close_message(invocation, ui_presentation, setup_id, it->second);
}

void queue_ui_panel_close_message(
    RuntimeInvocation& invocation,
    UiPresentationState& ui_presentation,
    Gs1UiPanelId panel_id)
{
    auto message = make_engine_message(
        panel_id == GS1_UI_PANEL_REGIONAL_MAP_SELECTION
            ? GS1_ENGINE_MESSAGE_CLOSE_REGIONAL_SELECTION_UI_PANEL
            : GS1_ENGINE_MESSAGE_CLOSE_UI_PANEL);
    auto& payload = message.emplace_payload<Gs1EngineMessageCloseUiPanelData>();
    payload.panel_id = panel_id;
    queue_runtime_message(invocation, message);
    if (panel_id == GS1_UI_PANEL_REGIONAL_MAP_SELECTION)
    {
        queue_ui_surface_visibility_message(invocation, GS1_UI_SURFACE_REGIONAL_SELECTION_PANEL, false);
    }
    ui_presentation.active_ui_panels.erase(panel_id);
}

void queue_close_ui_panel_if_open(
    RuntimeInvocation& invocation,
    UiPresentationState& ui_presentation,
    Gs1UiPanelId panel_id)
{
    if (!ui_presentation.active_ui_panels.contains(panel_id))
    {
        return;
    }

    queue_ui_panel_close_message(invocation, ui_presentation, panel_id);
}

void queue_progression_view_close_message(
    RuntimeInvocation& invocation,
    UiPresentationState& ui_presentation,
    Gs1ProgressionViewId view_id)
{
    auto message = make_engine_message(GS1_ENGINE_MESSAGE_CLOSE_PROGRESSION_VIEW);
    auto& payload = message.emplace_payload<Gs1EngineMessageCloseProgressionViewData>();
    payload.view_id = view_id;
    queue_runtime_message(invocation, message);
    if (view_id == GS1_PROGRESSION_VIEW_REGIONAL_MAP_TECH_TREE)
    {
        ui_presentation.regional_map_tech_tree_view_open = false;
        queue_ui_surface_visibility_message(invocation, GS1_UI_SURFACE_REGIONAL_TECH_TREE_OVERLAY, false);
    }
}

void queue_close_progression_view_if_open(
    RuntimeInvocation& invocation,
    UiPresentationState& ui_presentation,
    Gs1ProgressionViewId view_id)
{
    if (view_id == GS1_PROGRESSION_VIEW_REGIONAL_MAP_TECH_TREE &&
        !ui_presentation.regional_map_tech_tree_view_open)
    {
        return;
    }

    queue_progression_view_close_message(invocation, ui_presentation, view_id);
}

void queue_close_active_normal_ui_if_open(
    RuntimeInvocation& invocation,
    UiPresentationState& ui_presentation)
{
    if (!ui_presentation.active_normal_ui_setup.has_value())
    {
        return;
    }

    queue_close_ui_setup_if_open(invocation, ui_presentation, ui_presentation.active_normal_ui_setup.value());
}

void queue_ui_panel_begin_message(
    RuntimeInvocation& invocation,
    UiPresentationState& ui_presentation,
    Gs1UiPanelId panel_id,
    std::uint32_t context_id,
    std::uint32_t text_line_count,
    std::uint32_t slot_action_count,
    std::uint32_t list_item_count,
    std::uint32_t list_action_count)
{
    auto message = make_engine_message(GS1_ENGINE_MESSAGE_BEGIN_UI_PANEL);
    auto& payload = message.emplace_payload<Gs1EngineMessageUiPanelData>();
    payload.panel_id = panel_id;
    payload.mode = GS1_PROJECTION_MODE_SNAPSHOT;
    payload.context_id = context_id;
    payload.text_line_count = static_cast<std::uint8_t>(std::min<std::uint32_t>(text_line_count, 255U));
    payload.slot_action_count = static_cast<std::uint8_t>(std::min<std::uint32_t>(slot_action_count, 255U));
    payload.list_item_count = static_cast<std::uint8_t>(std::min<std::uint32_t>(list_item_count, 255U));
    payload.list_action_count = static_cast<std::uint8_t>(std::min<std::uint32_t>(list_action_count, 255U));
    queue_runtime_message(invocation, message);
    if (panel_id == GS1_UI_PANEL_REGIONAL_MAP_SELECTION)
    {
        queue_ui_surface_visibility_message(invocation, GS1_UI_SURFACE_REGIONAL_SELECTION_PANEL, true);
    }
    ui_presentation.active_ui_panels.insert(panel_id);
}

void queue_ui_panel_text_message(
    RuntimeInvocation& invocation,
    std::uint16_t line_id,
    std::uint32_t flags,
    std::uint8_t text_kind,
    std::uint32_t primary_id = 0U,
    std::uint32_t secondary_id = 0U,
    std::uint32_t quantity = 0U,
    std::uint32_t aux_value = 0U)
{
    auto message = make_engine_message(GS1_ENGINE_MESSAGE_UI_PANEL_TEXT_UPSERT);
    auto& payload = message.emplace_payload<Gs1EngineMessageUiPanelTextData>();
    payload.line_id = line_id;
    payload.flags = static_cast<std::uint8_t>(flags);
    payload.text_kind = text_kind;
    payload.primary_id = primary_id;
    payload.secondary_id = secondary_id;
    payload.quantity = quantity;
    payload.aux_value = aux_value;
    queue_runtime_message(invocation, message);
}

void queue_ui_panel_slot_action_message(
    RuntimeInvocation& invocation,
    Gs1UiPanelSlotId slot_id,
    std::uint32_t flags,
    const Gs1UiAction& action,
    std::uint8_t label_kind,
    std::uint32_t primary_id = 0U,
    std::uint32_t secondary_id = 0U,
    std::uint32_t quantity = 0U)
{
    auto message = make_engine_message(GS1_ENGINE_MESSAGE_UI_PANEL_SLOT_ACTION_UPSERT);
    auto& payload = message.emplace_payload<Gs1EngineMessageUiPanelSlotActionData>();
    payload.slot_id = slot_id;
    payload.flags = static_cast<std::uint8_t>(flags);
    payload.action = action;
    payload.label_kind = label_kind;
    payload.primary_id = primary_id;
    payload.secondary_id = secondary_id;
    payload.quantity = quantity;
    queue_runtime_message(invocation, message);
}

void queue_ui_panel_list_item_message(
    RuntimeInvocation& invocation,
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
    auto message = make_engine_message(GS1_ENGINE_MESSAGE_UI_PANEL_LIST_ITEM_UPSERT);
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
    queue_runtime_message(invocation, message);
}

void queue_ui_panel_list_action_message(
    RuntimeInvocation& invocation,
    Gs1UiPanelListId list_id,
    std::uint32_t item_id,
    Gs1UiPanelListActionRole role,
    std::uint32_t flags,
    const Gs1UiAction& action)
{
    auto message = make_engine_message(GS1_ENGINE_MESSAGE_UI_PANEL_LIST_ACTION_UPSERT);
    auto& payload = message.emplace_payload<Gs1EngineMessageUiPanelListActionData>();
    payload.item_id = item_id;
    payload.list_id = list_id;
    payload.role = role;
    payload.flags = static_cast<std::uint8_t>(flags);
    payload.action = action;
    queue_runtime_message(invocation, message);
}

void queue_ui_panel_end_message(RuntimeInvocation& invocation)
{
    queue_runtime_message(invocation, make_engine_message(GS1_ENGINE_MESSAGE_END_UI_PANEL));
}

void queue_progression_view_begin_message(
    RuntimeInvocation& invocation,
    UiPresentationState& ui_presentation,
    Gs1ProgressionViewId view_id,
    std::uint32_t entry_count,
    std::uint32_t context_id)
{
    auto message = make_engine_message(GS1_ENGINE_MESSAGE_BEGIN_PROGRESSION_VIEW);
    auto& payload = message.emplace_payload<Gs1EngineMessageProgressionViewData>();
    payload.view_id = view_id;
    payload.mode = GS1_PROJECTION_MODE_SNAPSHOT;
    payload.entry_count = static_cast<std::uint16_t>(std::min<std::uint32_t>(entry_count, 65535U));
    payload.context_id = context_id;
    queue_runtime_message(invocation, message);
    if (view_id == GS1_PROGRESSION_VIEW_REGIONAL_MAP_TECH_TREE)
    {
        ui_presentation.regional_map_tech_tree_view_open = true;
        queue_ui_surface_visibility_message(invocation, GS1_UI_SURFACE_REGIONAL_TECH_TREE_OVERLAY, true);
    }
}

void queue_progression_entry_message(
    RuntimeInvocation& invocation,
    const Gs1EngineMessageProgressionEntryData& payload)
{
    auto message = make_engine_message(GS1_ENGINE_MESSAGE_PROGRESSION_ENTRY_UPSERT);
    message.emplace_payload<Gs1EngineMessageProgressionEntryData>() = payload;
    queue_runtime_message(invocation, message);
}

void queue_progression_view_end_message(RuntimeInvocation& invocation)
{
    queue_runtime_message(invocation, make_engine_message(GS1_ENGINE_MESSAGE_END_PROGRESSION_VIEW));
}

void queue_main_menu_ui_messages(RuntimeInvocation& invocation, UiPresentationState& ui_presentation)
{
    Gs1UiAction action {};
    action.type = GS1_UI_ACTION_START_NEW_CAMPAIGN;
    action.arg0 = 42ULL;
    action.arg1 = 30ULL;
    queue_ui_panel_begin_message(invocation, ui_presentation, GS1_UI_PANEL_MAIN_MENU, 0U, 0U, 1U, 0U, 0U);
    queue_ui_panel_slot_action_message(
        invocation,
        GS1_UI_PANEL_SLOT_PRIMARY,
        GS1_UI_PANEL_SLOT_FLAG_PRIMARY,
        action,
        10U);
    queue_ui_panel_end_message(invocation);
}

void queue_campaign_resources_message(
    RuntimeInvocation& invocation,
    const std::optional<CampaignState>& campaign,
    const std::optional<SiteRunState>& active_site_run)
{
    if (!campaign.has_value())
    {
        return;
    }

    auto message = make_engine_message(GS1_ENGINE_MESSAGE_CAMPAIGN_RESOURCES);
    auto& payload = message.emplace_payload<Gs1EngineMessageCampaignResourcesData>();
    payload.current_money = active_site_run.has_value()
        ? cash_value_from_cash_points(active_site_run->economy.current_cash)
        : 0.0f;
    payload.total_reputation = campaign->technology_state.total_reputation;
    payload.village_reputation = TechnologySystem::faction_reputation(*campaign, FactionId {k_faction_village_committee});
    payload.forestry_reputation = TechnologySystem::faction_reputation(*campaign, FactionId {k_faction_forestry_bureau});
    payload.university_reputation =
        TechnologySystem::faction_reputation(*campaign, FactionId {k_faction_agricultural_university});
    queue_runtime_message(invocation, message);
}

void queue_site_result_ui_messages(
    RuntimeInvocation& invocation,
    UiPresentationState& ui_presentation,
    std::uint32_t site_id,
    Gs1SiteAttemptResult result)
{
    auto begin = make_engine_message(GS1_ENGINE_MESSAGE_BEGIN_UI_SETUP);
    auto& begin_payload = begin.emplace_payload<Gs1EngineMessageUiSetupData>();
    begin_payload.setup_id = GS1_UI_SETUP_SITE_RESULT;
    begin_payload.mode = GS1_PROJECTION_MODE_SNAPSHOT;
    begin_payload.presentation_type = GS1_UI_SETUP_PRESENTATION_OVERLAY;
    begin_payload.element_count = 2U;
    begin_payload.context_id = site_id;
    queue_runtime_message(invocation, begin);
    ui_presentation.active_ui_setups[GS1_UI_SETUP_SITE_RESULT] = GS1_UI_SETUP_PRESENTATION_OVERLAY;

    Gs1UiAction no_action {};
    auto label = make_engine_message(GS1_ENGINE_MESSAGE_UI_ELEMENT_UPSERT);
    auto& label_payload = label.emplace_payload<Gs1EngineMessageUiElementData>();
    label_payload.element_id = 1U;
    label_payload.element_type = GS1_UI_ELEMENT_LABEL;
    label_payload.flags = GS1_UI_ELEMENT_FLAG_NONE;
    label_payload.action = no_action;
    label_payload.content_kind = 1U;
    label_payload.primary_id = site_id;
    label_payload.secondary_id = static_cast<std::uint32_t>(result);
    label_payload.quantity = 0U;
    queue_runtime_message(invocation, label);

    Gs1UiAction return_action {};
    return_action.type = GS1_UI_ACTION_RETURN_TO_REGIONAL_MAP;
    auto button = make_engine_message(GS1_ENGINE_MESSAGE_UI_ELEMENT_UPSERT);
    auto& button_payload = button.emplace_payload<Gs1EngineMessageUiElementData>();
    button_payload.element_id = 2U;
    button_payload.element_type = GS1_UI_ELEMENT_BUTTON;
    button_payload.flags = GS1_UI_ELEMENT_FLAG_PRIMARY;
    button_payload.action = return_action;
    button_payload.content_kind = 2U;
    button_payload.primary_id = 0U;
    button_payload.secondary_id = 0U;
    button_payload.quantity = 0U;
    queue_runtime_message(invocation, button);

    queue_runtime_message(invocation, make_engine_message(GS1_ENGINE_MESSAGE_END_UI_SETUP));
}

void queue_site_result_ready_message(
    RuntimeInvocation& invocation,
    std::uint32_t site_id,
    Gs1SiteAttemptResult result,
    std::uint32_t newly_revealed_site_count)
{
    auto message = make_engine_message(GS1_ENGINE_MESSAGE_SITE_RESULT_READY);
    auto& payload = message.emplace_payload<Gs1EngineMessageSiteResultData>();
    payload.site_id = site_id;
    payload.result = result;
    payload.newly_revealed_site_count = static_cast<std::uint16_t>(newly_revealed_site_count);
    queue_runtime_message(invocation, message);
}

void queue_campaign_unlock_cue_message(
    RuntimeInvocation& invocation,
    std::uint32_t subject_id,
    std::uint32_t detail_id,
    std::uint32_t detail_kind)
{
    auto message = make_engine_message(GS1_ENGINE_MESSAGE_PLAY_ONE_SHOT_CUE);
    auto& payload = message.emplace_payload<Gs1EngineMessageOneShotCueData>();
    payload.subject_id = subject_id;
    payload.world_x = 0.0f;
    payload.world_y = 0.0f;
    payload.arg0 = detail_id;
    payload.arg1 = detail_kind;
    payload.cue_kind = GS1_ONE_SHOT_CUE_CAMPAIGN_UNLOCKED;
    queue_runtime_message(invocation, message);
}

void sync_campaign_unlock_presentations(
    RuntimeInvocation& invocation,
    const std::optional<CampaignState>& campaign,
    PresentationRuntimeState& presentation_runtime)
{
    if (!campaign.has_value())
    {
        presentation_runtime.campaign_unlock_snapshot = {};
        return;
    }

    auto unlocked_reputation_unlock_ids = capture_unlocked_reputation_unlock_ids(*campaign);
    auto unlocked_technology_node_ids = capture_unlocked_technology_node_ids(*campaign);
    std::sort(unlocked_reputation_unlock_ids.begin(), unlocked_reputation_unlock_ids.end());
    std::sort(unlocked_technology_node_ids.begin(), unlocked_technology_node_ids.end());

    for (const auto unlock_id : unlocked_reputation_unlock_ids)
    {
        if (!sorted_contains(
                presentation_runtime.campaign_unlock_snapshot.unlocked_reputation_unlock_ids,
                unlock_id))
        {
            queue_campaign_unlock_cue_message(
                invocation,
                unlock_id,
                unlock_id,
                CAMPAIGN_UNLOCK_CUE_DETAIL_REPUTATION_UNLOCK);
        }
    }

    for (const auto tech_node_id : unlocked_technology_node_ids)
    {
        if (!sorted_contains(
                presentation_runtime.campaign_unlock_snapshot.unlocked_technology_node_ids,
                tech_node_id))
        {
            queue_campaign_unlock_cue_message(
                invocation,
                tech_node_id,
                tech_node_id,
                CAMPAIGN_UNLOCK_CUE_DETAIL_TECHNOLOGY_NODE);
        }
    }

    presentation_runtime.campaign_unlock_snapshot.unlocked_reputation_unlock_ids =
        std::move(unlocked_reputation_unlock_ids);
    presentation_runtime.campaign_unlock_snapshot.unlocked_technology_node_ids =
        std::move(unlocked_technology_node_ids);
}

void queue_regional_map_menu_ui_messages(
    RuntimeInvocation& invocation,
    const std::optional<CampaignState>& campaign,
    UiPresentationState& ui_presentation,
    Gs1AppState app_state)
{
    if (!campaign.has_value() || app_state != GS1_APP_STATE_REGIONAL_MAP)
    {
        queue_close_ui_panel_if_open(invocation, ui_presentation, GS1_UI_PANEL_REGIONAL_MAP);
        return;
    }

    const auto selected_faction_id = campaign->regional_map_state.selected_tech_tree_faction_id.value != 0U
        ? campaign->regional_map_state.selected_tech_tree_faction_id
        : FactionId {k_faction_village_committee};
    Gs1UiAction toggle_action {};
    toggle_action.type = campaign->regional_map_state.tech_tree_open
        ? GS1_UI_ACTION_CLOSE_REGIONAL_MAP_TECH_TREE
        : GS1_UI_ACTION_OPEN_REGIONAL_MAP_TECH_TREE;

    std::uint32_t site_row_count = 0U;
    for (const auto revealed_site_id : campaign->regional_map_state.revealed_site_ids)
    {
        if (find_site_meta(*campaign, revealed_site_id.value) != nullptr)
        {
            ++site_row_count;
        }
    }

    queue_ui_panel_begin_message(invocation, ui_presentation, GS1_UI_PANEL_REGIONAL_MAP, 0U, 1U, 1U, site_row_count, site_row_count);
    queue_ui_panel_text_message(
        invocation,
        1U,
        0U,
        1U,
        selected_faction_id.value,
        static_cast<std::uint32_t>(TechnologySystem::faction_reputation(*campaign, selected_faction_id)));
    queue_ui_panel_slot_action_message(
        invocation,
        GS1_UI_PANEL_SLOT_PRIMARY,
        GS1_UI_PANEL_SLOT_FLAG_PRIMARY,
        toggle_action,
        campaign->regional_map_state.tech_tree_open ? 2U : 1U);

    for (const auto revealed_site_id : campaign->regional_map_state.revealed_site_ids)
    {
        const SiteMetaState* site = find_site_meta(*campaign, revealed_site_id.value);
        if (site == nullptr)
        {
            continue;
        }

        const bool selected = campaign->regional_map_state.selected_site_id.has_value() &&
            campaign->regional_map_state.selected_site_id->value == site->site_id.value;
        const std::uint32_t item_flags = selected ? GS1_UI_PANEL_LIST_ITEM_FLAG_SELECTED : GS1_UI_PANEL_LIST_ITEM_FLAG_NONE;

        queue_ui_panel_list_item_message(
            invocation,
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
            invocation,
            GS1_UI_PANEL_LIST_REGIONAL_SITES,
            site->site_id.value,
            GS1_UI_PANEL_LIST_ACTION_ROLE_PRIMARY,
            site->site_state == GS1_SITE_STATE_LOCKED ? GS1_UI_PANEL_LIST_ITEM_FLAG_DISABLED : GS1_UI_PANEL_LIST_ITEM_FLAG_NONE,
            select_action);
    }

    queue_ui_panel_end_message(invocation);
}

void queue_regional_map_selection_ui_messages(
    RuntimeInvocation& invocation,
    const std::optional<CampaignState>& campaign,
    UiPresentationState& ui_presentation)
{
    if (!campaign.has_value() || !campaign->regional_map_state.selected_site_id.has_value())
    {
        queue_close_ui_panel_if_open(invocation, ui_presentation, GS1_UI_PANEL_REGIONAL_MAP_SELECTION);
        return;
    }

    const auto site_id = campaign->regional_map_state.selected_site_id->value;
    const auto loadout_item_count = visible_loadout_slot_count(campaign->loadout_planner_state);
    const bool has_support_contributors = campaign->loadout_planner_state.support_quota > 0U;
    const bool has_aura_support = !campaign->loadout_planner_state.active_nearby_aura_modifier_ids.empty();
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
    queue_runtime_message(invocation, begin);
    queue_ui_surface_visibility_message(invocation, GS1_UI_SURFACE_REGIONAL_SELECTION_PANEL, true);
    ui_presentation.active_ui_panels.insert(GS1_UI_PANEL_REGIONAL_MAP_SELECTION);

    std::uint16_t next_line_id = 1U;
    queue_runtime_message(invocation, [&]() {
        auto message = make_engine_message(GS1_ENGINE_MESSAGE_REGIONAL_SELECTION_UI_PANEL_TEXT_UPSERT);
        auto& payload = message.emplace_payload<Gs1EngineMessageUiPanelTextData>();
        payload.line_id = next_line_id++;
        payload.flags = 0U;
        payload.text_kind = 3U;
        payload.primary_id = site_id;
        payload.secondary_id = 0U;
        payload.quantity = 0U;
        payload.aux_value = 0U;
        return message;
    }());

    if (has_support_contributors)
    {
        queue_runtime_message(invocation, [&]() {
            auto message = make_engine_message(GS1_ENGINE_MESSAGE_REGIONAL_SELECTION_UI_PANEL_TEXT_UPSERT);
            auto& payload = message.emplace_payload<Gs1EngineMessageUiPanelTextData>();
            payload.line_id = next_line_id++;
            payload.flags = 0U;
            payload.text_kind = 4U;
            payload.primary_id = 0U;
            payload.secondary_id = 0U;
            payload.quantity = campaign->loadout_planner_state.support_quota;
            payload.aux_value = 0U;
            return message;
        }());
    }

    if (has_aura_support)
    {
        queue_runtime_message(invocation, [&]() {
            auto message = make_engine_message(GS1_ENGINE_MESSAGE_REGIONAL_SELECTION_UI_PANEL_TEXT_UPSERT);
            auto& payload = message.emplace_payload<Gs1EngineMessageUiPanelTextData>();
            payload.line_id = next_line_id++;
            payload.flags = 0U;
            payload.text_kind = 5U;
            payload.primary_id = 0U;
            payload.secondary_id = 0U;
            payload.quantity =
                static_cast<std::uint32_t>(campaign->loadout_planner_state.active_nearby_aura_modifier_ids.size());
            payload.aux_value = 0U;
            return message;
        }());
    }

    std::uint32_t next_loadout_item_id = 1U;
    for (const auto& slot : campaign->loadout_planner_state.selected_loadout_slots)
    {
        if (!slot.occupied || slot.item_id.value == 0U || slot.quantity == 0U)
        {
            continue;
        }

        auto message = make_engine_message(GS1_ENGINE_MESSAGE_REGIONAL_SELECTION_UI_PANEL_LIST_ITEM_UPSERT);
        auto& payload = message.emplace_payload<Gs1EngineMessageUiPanelListItemData>();
        payload.item_id = next_loadout_item_id++;
        payload.list_id = GS1_UI_PANEL_LIST_REGIONAL_LOADOUT;
        payload.flags = GS1_UI_PANEL_LIST_ITEM_FLAG_NONE;
        payload.primary_kind = 6U;
        payload.secondary_kind = 0U;
        payload.primary_id = slot.item_id.value;
        payload.secondary_id = 0U;
        payload.quantity = slot.quantity;
        payload.map_x = 0;
        payload.map_y = 0;
        queue_runtime_message(invocation, message);
    }

    Gs1UiAction deploy_action {};
    deploy_action.type = GS1_UI_ACTION_START_SITE_ATTEMPT;
    deploy_action.target_id = site_id;
    auto deploy_message = make_engine_message(GS1_ENGINE_MESSAGE_REGIONAL_SELECTION_UI_PANEL_SLOT_ACTION_UPSERT);
    auto& deploy_payload = deploy_message.emplace_payload<Gs1EngineMessageUiPanelSlotActionData>();
    deploy_payload.slot_id = GS1_UI_PANEL_SLOT_PRIMARY;
    deploy_payload.flags = GS1_UI_PANEL_SLOT_FLAG_PRIMARY;
    deploy_payload.action = deploy_action;
    deploy_payload.label_kind = 3U;
    deploy_payload.primary_id = site_id;
    deploy_payload.secondary_id = 0U;
    deploy_payload.quantity = 0U;
    queue_runtime_message(invocation, deploy_message);

    Gs1UiAction clear_selection_action {};
    clear_selection_action.type = GS1_UI_ACTION_CLEAR_DEPLOYMENT_SITE_SELECTION;
    auto clear_message = make_engine_message(GS1_ENGINE_MESSAGE_REGIONAL_SELECTION_UI_PANEL_SLOT_ACTION_UPSERT);
    auto& clear_payload = clear_message.emplace_payload<Gs1EngineMessageUiPanelSlotActionData>();
    clear_payload.slot_id = GS1_UI_PANEL_SLOT_SECONDARY;
    clear_payload.flags = GS1_UI_PANEL_SLOT_FLAG_NONE;
    clear_payload.action = clear_selection_action;
    clear_payload.label_kind = 4U;
    clear_payload.primary_id = 0U;
    clear_payload.secondary_id = 0U;
    clear_payload.quantity = 0U;
    queue_runtime_message(invocation, clear_message);

    queue_runtime_message(invocation, make_engine_message(GS1_ENGINE_MESSAGE_END_REGIONAL_SELECTION_UI_PANEL));
}

void queue_regional_map_tech_tree_ui_messages(
    RuntimeInvocation& invocation,
    const std::optional<CampaignState>& campaign,
    UiPresentationState& ui_presentation,
    Gs1AppState app_state)
{
    if (!campaign.has_value() ||
        !app_state_supports_technology_tree(app_state) ||
        !campaign->regional_map_state.tech_tree_open)
    {
        return;
    }

    queue_close_ui_setup_if_open(invocation, ui_presentation, GS1_UI_SETUP_REGIONAL_MAP_TECH_TREE);

    const auto reputation_unlock_defs = all_reputation_unlock_defs();
    const auto technology_node_defs = all_technology_node_defs();
    const std::uint32_t entry_count =
        static_cast<std::uint32_t>(reputation_unlock_defs.size() + technology_node_defs.size() + 1U);
    queue_progression_view_begin_message(
        invocation,
        ui_presentation,
        GS1_PROGRESSION_VIEW_REGIONAL_MAP_TECH_TREE,
        entry_count,
        0U);

    Gs1EngineMessageProgressionEntryData starter_entry {};
    starter_entry.entry_id = 0U;
    starter_entry.reputation_requirement = 0U;
    starter_entry.entry_kind = GS1_PROGRESSION_ENTRY_REPUTATION_UNLOCK;
    starter_entry.flags = GS1_PROGRESSION_ENTRY_FLAG_UNLOCKED;
    queue_progression_entry_message(invocation, starter_entry);

    for (const auto& unlock_def : reputation_unlock_defs)
    {
        Gs1EngineMessageProgressionEntryData entry {};
        entry.entry_id = static_cast<std::uint16_t>(unlock_def.unlock_id);
        entry.reputation_requirement = static_cast<std::uint16_t>(unlock_def.reputation_requirement);
        entry.content_id = static_cast<std::uint16_t>(unlock_def.content_id);
        entry.entry_kind = GS1_PROGRESSION_ENTRY_REPUTATION_UNLOCK;
        entry.content_kind = static_cast<std::uint8_t>(unlock_def.unlock_kind);
        entry.flags = campaign->technology_state.total_reputation >= unlock_def.reputation_requirement
            ? GS1_PROGRESSION_ENTRY_FLAG_UNLOCKED
            : GS1_PROGRESSION_ENTRY_FLAG_LOCKED;
        queue_progression_entry_message(invocation, entry);
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
        entry.flags = TechnologySystem::node_purchased(*campaign, node_def.tech_node_id)
            ? GS1_PROGRESSION_ENTRY_FLAG_UNLOCKED
            : GS1_PROGRESSION_ENTRY_FLAG_LOCKED;
        queue_progression_entry_message(invocation, entry);
    }

    queue_progression_view_end_message(invocation);
}

void queue_regional_map_site_upsert_message(
    RuntimeInvocation& invocation,
    const CampaignState& campaign,
    const SiteMetaState& site,
    Gs1EngineMessageType message_type)
{
    auto site_message = make_engine_message(message_type);
    auto& payload = site_message.emplace_payload<Gs1EngineMessageRegionalMapSiteData>();
    payload.site_id = site.site_id.value;
    payload.site_state = site.site_state;
    payload.site_archetype_id = site.site_archetype_id;
    payload.flags =
        (campaign.regional_map_state.selected_site_id.has_value() &&
            campaign.regional_map_state.selected_site_id->value == site.site_id.value)
        ? 1U
        : 0U;

    payload.map_x = site.regional_map_tile.x * k_regional_map_tile_spacing;
    payload.map_y = site.regional_map_tile.y * k_regional_map_tile_spacing;
    payload.support_package_id = site.has_support_package_id ? site.support_package_id : 0U;
    payload.support_preview_mask =
        site_contributes_to_selected_target(campaign, site) ? support_preview_mask_for_site(site) : 0U;
    queue_runtime_message(invocation, site_message);
}

void queue_regional_map_snapshot_messages(
    RuntimeInvocation& invocation,
    const std::optional<CampaignState>& campaign)
{
    if (!campaign.has_value())
    {
        return;
    }

    std::set<std::pair<std::uint32_t, std::uint32_t>> unique_links;
    for (const auto& site : campaign->sites)
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
    for (const auto& site : campaign->sites)
    {
        if (is_site_revealed(*campaign, site.site_id))
        {
            revealed_site_count += 1U;
        }
    }
    for (const auto& link : unique_links)
    {
        if (!is_site_revealed(*campaign, SiteId {link.first}) ||
            !is_site_revealed(*campaign, SiteId {link.second}))
        {
            continue;
        }

        revealed_link_count += 1U;
    }

    const auto queue_family = [&](const RegionalMapSnapshotMessageFamily& family)
    {
        auto begin = make_engine_message(family.begin_message);
        auto& begin_payload = begin.emplace_payload<Gs1EngineMessageRegionalMapSnapshotData>();
        begin_payload.mode = GS1_PROJECTION_MODE_SNAPSHOT;
        begin_payload.link_count = revealed_link_count;
        begin_payload.selected_site_id =
            campaign->regional_map_state.selected_site_id.has_value()
                ? campaign->regional_map_state.selected_site_id->value
                : 0U;
        begin_payload.site_count = revealed_site_count;
        queue_runtime_message(invocation, begin);

        for (const auto& site : campaign->sites)
        {
            if (!is_site_revealed(*campaign, site.site_id))
            {
                continue;
            }

            queue_regional_map_site_upsert_message(invocation, *campaign, site, family.site_upsert_message);
        }

        for (const auto& link : unique_links)
        {
            if (!is_site_revealed(*campaign, SiteId {link.first}) ||
                !is_site_revealed(*campaign, SiteId {link.second}))
            {
                continue;
            }

            auto link_message = make_engine_message(family.link_upsert_message);
            auto& payload = link_message.emplace_payload<Gs1EngineMessageRegionalMapLinkData>();
            payload.from_site_id = link.first;
            payload.to_site_id = link.second;
            queue_runtime_message(invocation, link_message);
        }

        queue_runtime_message(invocation, make_engine_message(family.end_message));
    };

    queue_family(k_regional_map_scene_snapshot_family);
    queue_family(k_regional_map_hud_snapshot_family);
    queue_family(k_regional_summary_snapshot_family);
    queue_family(k_regional_selection_snapshot_family);
}
}  // namespace

template <>
struct system_state_tags<CampaignPresentationSystem>
{
    using type = CampaignPresentationSystemTags;
};

template <>
struct state_owner<RuntimeUiPresentationTag>
{
    using type = CampaignPresentationSystem;
};

template <>
struct state_owner<RuntimePresentationRuntimeTag>
{
    using type = CampaignPresentationSystem;
};

const char* CampaignPresentationSystem::name() const noexcept
{
    return "CampaignPresentationSystem";
}

GameMessageSubscriptionSpan CampaignPresentationSystem::subscribed_game_messages() const noexcept
{
    static constexpr GameMessageType subscriptions[] = {
        GameMessageType::OpenMainMenu,
        GameMessageType::StartNewCampaign,
        GameMessageType::SelectDeploymentSite,
        GameMessageType::ClearDeploymentSiteSelection,
        GameMessageType::OpenRegionalMapTechTree,
        GameMessageType::CloseRegionalMapTechTree,
        GameMessageType::SelectRegionalMapTechTreeFaction,
        GameMessageType::CampaignCashDeltaRequested,
        GameMessageType::CampaignReputationAwardRequested,
        GameMessageType::FactionReputationAwardRequested,
        GameMessageType::TechnologyNodeClaimRequested,
        GameMessageType::TechnologyNodeRefundRequested,
        GameMessageType::StartSiteAttempt,
        GameMessageType::SiteSceneActivated,
        GameMessageType::ReturnToRegionalMap,
        GameMessageType::SiteAttemptEnded,
        GameMessageType::PresentLog,
        GameMessageType::PhoneListingPurchaseRequested,
        GameMessageType::PhoneListingSaleRequested,
        GameMessageType::ContractorHireRequested,
        GameMessageType::SiteUnlockablePurchaseRequested,
    };
    return subscriptions;
}

HostMessageSubscriptionSpan CampaignPresentationSystem::subscribed_host_messages() const noexcept
{
    return {};
}

std::optional<Gs1RuntimeProfileSystemId> CampaignPresentationSystem::profile_system_id() const noexcept
{
    return std::nullopt;
}

std::optional<std::uint32_t> CampaignPresentationSystem::fixed_step_order() const noexcept
{
    return std::nullopt;
}

Gs1Status CampaignPresentationSystem::process_host_message(
    RuntimeInvocation& invocation,
    const Gs1HostMessage& message)
{
    (void)invocation;
    (void)message;
    return GS1_STATUS_OK;
}

Gs1Status CampaignPresentationSystem::process_game_message(
    RuntimeInvocation& invocation,
    const GameMessage& message)
{
    auto access = make_game_state_access<CampaignPresentationSystem>(invocation);
    auto& app_state = access.template read<RuntimeAppStateTag>();
    auto& campaign = access.template read<RuntimeCampaignTag>();
    auto& active_site_run = access.template read<RuntimeActiveSiteRunTag>();
    auto& ui_presentation = access.template write<RuntimeUiPresentationTag>();
    auto& presentation_runtime = access.template write<RuntimePresentationRuntimeTag>();

    switch (message.type)
    {
    case GameMessageType::OpenMainMenu:
        queue_close_active_normal_ui_if_open(invocation, ui_presentation);
        queue_close_ui_setup_if_open(invocation, ui_presentation, GS1_UI_SETUP_REGIONAL_MAP_TECH_TREE);
        queue_close_progression_view_if_open(invocation, ui_presentation, GS1_PROGRESSION_VIEW_REGIONAL_MAP_TECH_TREE);
        queue_close_ui_panel_if_open(invocation, ui_presentation, GS1_UI_PANEL_REGIONAL_MAP);
        queue_close_ui_panel_if_open(invocation, ui_presentation, GS1_UI_PANEL_REGIONAL_MAP_SELECTION);
        queue_app_state_message(invocation, ui_presentation, app_state);
        queue_main_menu_ui_messages(invocation, ui_presentation);
        queue_log_message(invocation, "Entered main menu.", GS1_LOG_LEVEL_INFO);
        return GS1_STATUS_OK;

    case GameMessageType::StartNewCampaign:
        queue_close_ui_panel_if_open(invocation, ui_presentation, GS1_UI_PANEL_MAIN_MENU);
        queue_app_state_message(invocation, ui_presentation, app_state);
        queue_campaign_resources_message(invocation, campaign, active_site_run);
        queue_regional_map_snapshot_messages(invocation, campaign);
        queue_regional_map_menu_ui_messages(invocation, campaign, ui_presentation, app_state);
        queue_regional_map_selection_ui_messages(invocation, campaign, ui_presentation);
        queue_log_message(invocation, "Started new GS1 campaign.", GS1_LOG_LEVEL_INFO);
        return GS1_STATUS_OK;

    case GameMessageType::SelectDeploymentSite:
        queue_regional_map_snapshot_messages(invocation, campaign);
        queue_regional_map_menu_ui_messages(invocation, campaign, ui_presentation, app_state);
        queue_regional_map_selection_ui_messages(invocation, campaign, ui_presentation);
        queue_log_message(invocation, "Selected deployment site.", GS1_LOG_LEVEL_INFO);
        return GS1_STATUS_OK;

    case GameMessageType::ClearDeploymentSiteSelection:
        queue_regional_map_snapshot_messages(invocation, campaign);
        queue_regional_map_menu_ui_messages(invocation, campaign, ui_presentation, app_state);
        queue_close_ui_panel_if_open(invocation, ui_presentation, GS1_UI_PANEL_REGIONAL_MAP_SELECTION);
        queue_log_message(invocation, "Cleared deployment site selection.", GS1_LOG_LEVEL_INFO);
        return GS1_STATUS_OK;

    case GameMessageType::OpenRegionalMapTechTree:
        queue_regional_map_menu_ui_messages(invocation, campaign, ui_presentation, app_state);
        queue_regional_map_tech_tree_ui_messages(invocation, campaign, ui_presentation, app_state);
        return GS1_STATUS_OK;

    case GameMessageType::CloseRegionalMapTechTree:
        queue_regional_map_menu_ui_messages(invocation, campaign, ui_presentation, app_state);
        queue_close_ui_setup_if_open(invocation, ui_presentation, GS1_UI_SETUP_REGIONAL_MAP_TECH_TREE);
        queue_close_progression_view_if_open(invocation, ui_presentation, GS1_PROGRESSION_VIEW_REGIONAL_MAP_TECH_TREE);
        return GS1_STATUS_OK;

    case GameMessageType::SelectRegionalMapTechTreeFaction:
        queue_regional_map_menu_ui_messages(invocation, campaign, ui_presentation, app_state);
        queue_regional_map_tech_tree_ui_messages(invocation, campaign, ui_presentation, app_state);
        return GS1_STATUS_OK;

    case GameMessageType::CampaignCashDeltaRequested:
    case GameMessageType::CampaignReputationAwardRequested:
    case GameMessageType::FactionReputationAwardRequested:
    case GameMessageType::TechnologyNodeClaimRequested:
    case GameMessageType::TechnologyNodeRefundRequested:
    case GameMessageType::EconomyMoneyAwardRequested:
    case GameMessageType::PhoneCartCheckoutRequested:
        queue_campaign_resources_message(invocation, campaign, active_site_run);
        if (app_state == GS1_APP_STATE_REGIONAL_MAP || app_state == GS1_APP_STATE_SITE_ACTIVE)
        {
            queue_regional_map_menu_ui_messages(invocation, campaign, ui_presentation, app_state);
            queue_regional_map_tech_tree_ui_messages(invocation, campaign, ui_presentation, app_state);
        }
        if (message.type == GameMessageType::CampaignReputationAwardRequested ||
            message.type == GameMessageType::FactionReputationAwardRequested ||
            message.type == GameMessageType::TechnologyNodeClaimRequested ||
            message.type == GameMessageType::TechnologyNodeRefundRequested)
        {
            sync_campaign_unlock_presentations(invocation, campaign, presentation_runtime);
        }
        return GS1_STATUS_OK;

    case GameMessageType::StartSiteAttempt:
        queue_close_ui_panel_if_open(invocation, ui_presentation, GS1_UI_PANEL_REGIONAL_MAP_SELECTION);
        queue_close_ui_panel_if_open(invocation, ui_presentation, GS1_UI_PANEL_REGIONAL_MAP);
        queue_close_ui_setup_if_open(invocation, ui_presentation, GS1_UI_SETUP_REGIONAL_MAP_TECH_TREE);
        queue_close_progression_view_if_open(invocation, ui_presentation, GS1_PROGRESSION_VIEW_REGIONAL_MAP_TECH_TREE);
        queue_app_state_message(invocation, ui_presentation, app_state);
        return GS1_STATUS_OK;

    case GameMessageType::SiteSceneActivated:
        queue_app_state_message(invocation, ui_presentation, app_state);
        return GS1_STATUS_OK;

    case GameMessageType::ReturnToRegionalMap:
        queue_app_state_message(invocation, ui_presentation, app_state);
        queue_campaign_resources_message(invocation, campaign, active_site_run);
        queue_close_ui_setup_if_open(invocation, ui_presentation, GS1_UI_SETUP_SITE_RESULT);
        queue_regional_map_snapshot_messages(invocation, campaign);
        queue_regional_map_menu_ui_messages(invocation, campaign, ui_presentation, app_state);
        queue_regional_map_selection_ui_messages(invocation, campaign, ui_presentation);
        return GS1_STATUS_OK;

    case GameMessageType::SiteAttemptEnded:
    {
        const auto& payload = message.payload_as<SiteAttemptEndedMessage>();
        const auto newly_revealed_site_count =
            active_site_run.has_value() ? active_site_run->result_newly_revealed_site_count : 0U;
        queue_close_ui_setup_if_open(invocation, ui_presentation, GS1_UI_SETUP_REGIONAL_MAP_TECH_TREE);
        queue_close_progression_view_if_open(invocation, ui_presentation, GS1_PROGRESSION_VIEW_REGIONAL_MAP_TECH_TREE);
        queue_app_state_message(invocation, ui_presentation, app_state);
        queue_site_result_ui_messages(invocation, ui_presentation, payload.site_id, payload.result);
        queue_site_result_ready_message(invocation, payload.site_id, payload.result, newly_revealed_site_count);
        return GS1_STATUS_OK;
    }

    case GameMessageType::PresentLog:
        queue_log_message(invocation, message.payload_as<PresentLogMessage>().text, message.payload_as<PresentLogMessage>().level);
        return GS1_STATUS_OK;

    case GameMessageType::PhoneListingPurchaseRequested:
    case GameMessageType::PhoneListingSaleRequested:
    case GameMessageType::ContractorHireRequested:
    case GameMessageType::SiteUnlockablePurchaseRequested:
        queue_campaign_resources_message(invocation, campaign, active_site_run);
        return GS1_STATUS_OK;

    default:
        return GS1_STATUS_OK;
    }
}

void CampaignPresentationSystem::run(RuntimeInvocation& invocation)
{
    (void)invocation;
}
}  // namespace gs1
