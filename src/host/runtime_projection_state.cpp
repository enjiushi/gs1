#include "host/runtime_projection_state.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <utility>

namespace
{
constexpr std::uint16_t k_invalid_tile_coordinate = std::numeric_limits<std::uint16_t>::max();

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

std::string message_text_to_string(const char* text, std::size_t capacity)
{
    const auto length = static_cast<std::size_t>(std::find(text, text + capacity, '\0') - text);
    return std::string {text, length};
}

std::uint64_t regional_map_link_key(std::uint32_t from_site_id, std::uint32_t to_site_id)
{
    return (static_cast<std::uint64_t>(from_site_id) << 32U) | static_cast<std::uint64_t>(to_site_id);
}

void sort_ui_setups(std::vector<Gs1RuntimeUiSetupProjection>& setups)
{
    std::sort(setups.begin(), setups.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.setup_id < rhs.setup_id;
    });
}

void sort_progression_entries(std::vector<Gs1RuntimeProgressionEntryProjection>& entries)
{
    std::sort(entries.begin(), entries.end(), [](const auto& lhs, const auto& rhs) {
        if (lhs.reputation_requirement != rhs.reputation_requirement)
        {
            return lhs.reputation_requirement < rhs.reputation_requirement;
        }
        if (lhs.entry_kind != rhs.entry_kind)
        {
            return lhs.entry_kind < rhs.entry_kind;
        }
        if (lhs.faction_id != rhs.faction_id)
        {
            return lhs.faction_id < rhs.faction_id;
        }
        return lhs.entry_id < rhs.entry_id;
    });
}

void sort_progression_views(std::vector<Gs1RuntimeProgressionViewProjection>& views)
{
    std::sort(views.begin(), views.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.view_id < rhs.view_id;
    });
}

void sort_ui_panels(std::vector<Gs1RuntimeUiPanelProjection>& panels)
{
    std::sort(panels.begin(), panels.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.panel_id < rhs.panel_id;
    });
}

void sort_ui_panel_text_lines(std::vector<Gs1RuntimeUiPanelTextProjection>& lines)
{
    std::sort(lines.begin(), lines.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.line_id < rhs.line_id;
    });
}

void sort_ui_panel_slot_actions(std::vector<Gs1RuntimeUiPanelSlotActionProjection>& actions)
{
    std::sort(actions.begin(), actions.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.slot_id < rhs.slot_id;
    });
}

void sort_ui_panel_list_items(std::vector<Gs1RuntimeUiPanelListItemProjection>& items)
{
    std::sort(items.begin(), items.end(), [](const auto& lhs, const auto& rhs) {
        if (lhs.list_id != rhs.list_id)
        {
            return lhs.list_id < rhs.list_id;
        }
        return lhs.item_id < rhs.item_id;
    });
}

void sort_ui_panel_list_actions(std::vector<Gs1RuntimeUiPanelListActionProjection>& actions)
{
    std::sort(actions.begin(), actions.end(), [](const auto& lhs, const auto& rhs) {
        if (lhs.list_id != rhs.list_id)
        {
            return lhs.list_id < rhs.list_id;
        }
        if (lhs.item_id != rhs.item_id)
        {
            return lhs.item_id < rhs.item_id;
        }
        return lhs.role < rhs.role;
    });
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
        const auto lhs_key = regional_map_link_key(lhs.from_site_id, lhs.to_site_id);
        const auto rhs_key = regional_map_link_key(rhs.from_site_id, rhs.to_site_id);
        return lhs_key < rhs_key;
    });
}

void sort_inventory_slots(std::vector<Gs1RuntimeInventorySlotProjection>& slots)
{
    std::sort(slots.begin(), slots.end(), [](const auto& lhs, const auto& rhs) {
        if (lhs.storage_id != rhs.storage_id)
        {
            return lhs.storage_id < rhs.storage_id;
        }
        return lhs.slot_index < rhs.slot_index;
    });
}

void sort_inventory_storages(std::vector<Gs1RuntimeInventoryStorageProjection>& storages)
{
    std::sort(storages.begin(), storages.end(), [](const auto& lhs, const auto& rhs) {
        if (lhs.container_kind != rhs.container_kind)
        {
            return lhs.container_kind < rhs.container_kind;
        }
        return lhs.storage_id < rhs.storage_id;
    });
}

void sort_tasks(std::vector<Gs1RuntimeTaskProjection>& tasks)
{
    std::sort(tasks.begin(), tasks.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.task_instance_id < rhs.task_instance_id;
    });
}

void sort_phone_listings(std::vector<Gs1RuntimePhoneListingProjection>& listings)
{
    std::sort(listings.begin(), listings.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.listing_id < rhs.listing_id;
    });
}

void sort_modifiers(std::vector<Gs1RuntimeModifierProjection>& modifiers)
{
    std::sort(modifiers.begin(), modifiers.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.modifier_id < rhs.modifier_id;
    });
}

void sort_craft_options(std::vector<Gs1RuntimeCraftContextOptionProjection>& options)
{
    std::sort(options.begin(), options.end(), [](const auto& lhs, const auto& rhs) {
        if (lhs.output_item_id != rhs.output_item_id)
        {
            return lhs.output_item_id < rhs.output_item_id;
        }
        return lhs.recipe_id < rhs.recipe_id;
    });
}

void sort_placement_preview_tiles(std::vector<Gs1RuntimePlacementPreviewTileProjection>& tiles)
{
    std::sort(tiles.begin(), tiles.end(), [](const auto& lhs, const auto& rhs) {
        if (lhs.y != rhs.y)
        {
            return lhs.y < rhs.y;
        }
        return lhs.x < rhs.x;
    });
}
}  // namespace

void Gs1RuntimeProjectionCache::reset() noexcept
{
    state_ = {};
    pending_ui_setup_.reset();
    pending_progression_view_.reset();
    pending_ui_panel_.reset();
    pending_regional_map_.reset();
    pending_site_.reset();
    next_one_shot_cue_sequence_id_ = 1U;
}

void Gs1RuntimeProjectionCache::apply_engine_message(const Gs1EngineMessage& message)
{
    switch (message.type)
    {
    case GS1_ENGINE_MESSAGE_SET_APP_STATE:
        apply_set_app_state(message.payload_as<Gs1EngineMessageSetAppStateData>());
        break;
    case GS1_ENGINE_MESSAGE_BEGIN_REGIONAL_MAP_SNAPSHOT:
        apply_regional_map_snapshot_begin(message.payload_as<Gs1EngineMessageRegionalMapSnapshotData>());
        break;
    case GS1_ENGINE_MESSAGE_REGIONAL_MAP_SITE_UPSERT:
        apply_regional_map_site_upsert(message.payload_as<Gs1EngineMessageRegionalMapSiteData>());
        break;
    case GS1_ENGINE_MESSAGE_REGIONAL_MAP_SITE_REMOVE:
        apply_regional_map_site_remove(message.payload_as<Gs1EngineMessageRegionalMapSiteData>());
        break;
    case GS1_ENGINE_MESSAGE_REGIONAL_MAP_LINK_UPSERT:
        apply_regional_map_link_upsert(message.payload_as<Gs1EngineMessageRegionalMapLinkData>());
        break;
    case GS1_ENGINE_MESSAGE_REGIONAL_MAP_LINK_REMOVE:
        apply_regional_map_link_remove(message.payload_as<Gs1EngineMessageRegionalMapLinkData>());
        break;
    case GS1_ENGINE_MESSAGE_END_REGIONAL_MAP_SNAPSHOT:
        apply_regional_map_snapshot_end();
        break;
    case GS1_ENGINE_MESSAGE_BEGIN_UI_SETUP:
        apply_ui_setup_begin(message.payload_as<Gs1EngineMessageUiSetupData>());
        break;
    case GS1_ENGINE_MESSAGE_UI_ELEMENT_UPSERT:
        apply_ui_element_upsert(message.payload_as<Gs1EngineMessageUiElementData>());
        break;
    case GS1_ENGINE_MESSAGE_END_UI_SETUP:
        apply_ui_setup_end();
        break;
    case GS1_ENGINE_MESSAGE_CLOSE_UI_SETUP:
        apply_ui_setup_close(message.payload_as<Gs1EngineMessageCloseUiSetupData>());
        break;
    case GS1_ENGINE_MESSAGE_BEGIN_PROGRESSION_VIEW:
        apply_progression_view_begin(message.payload_as<Gs1EngineMessageProgressionViewData>());
        break;
    case GS1_ENGINE_MESSAGE_PROGRESSION_ENTRY_UPSERT:
        apply_progression_entry_upsert(message.payload_as<Gs1EngineMessageProgressionEntryData>());
        break;
    case GS1_ENGINE_MESSAGE_END_PROGRESSION_VIEW:
        apply_progression_view_end();
        break;
    case GS1_ENGINE_MESSAGE_CLOSE_PROGRESSION_VIEW:
        apply_progression_view_close(message.payload_as<Gs1EngineMessageCloseProgressionViewData>());
        break;
    case GS1_ENGINE_MESSAGE_BEGIN_UI_PANEL:
        apply_ui_panel_begin(message.payload_as<Gs1EngineMessageUiPanelData>());
        break;
    case GS1_ENGINE_MESSAGE_UI_PANEL_TEXT_UPSERT:
        apply_ui_panel_text_upsert(message.payload_as<Gs1EngineMessageUiPanelTextData>());
        break;
    case GS1_ENGINE_MESSAGE_UI_PANEL_SLOT_ACTION_UPSERT:
        apply_ui_panel_slot_action_upsert(message.payload_as<Gs1EngineMessageUiPanelSlotActionData>());
        break;
    case GS1_ENGINE_MESSAGE_UI_PANEL_LIST_ITEM_UPSERT:
        apply_ui_panel_list_item_upsert(message.payload_as<Gs1EngineMessageUiPanelListItemData>());
        break;
    case GS1_ENGINE_MESSAGE_UI_PANEL_LIST_ACTION_UPSERT:
        apply_ui_panel_list_action_upsert(message.payload_as<Gs1EngineMessageUiPanelListActionData>());
        break;
    case GS1_ENGINE_MESSAGE_END_UI_PANEL:
        apply_ui_panel_end();
        break;
    case GS1_ENGINE_MESSAGE_CLOSE_UI_PANEL:
        apply_ui_panel_close(message.payload_as<Gs1EngineMessageCloseUiPanelData>());
        break;
    case GS1_ENGINE_MESSAGE_BEGIN_SITE_SNAPSHOT:
        apply_site_snapshot_begin(message.payload_as<Gs1EngineMessageSiteSnapshotData>());
        break;
    case GS1_ENGINE_MESSAGE_SITE_TILE_UPSERT:
        apply_site_tile_upsert(message.payload_as<Gs1EngineMessageSiteTileData>());
        break;
    case GS1_ENGINE_MESSAGE_SITE_WORKER_UPDATE:
        apply_site_worker_update(message.payload_as<Gs1EngineMessageWorkerData>());
        break;
    case GS1_ENGINE_MESSAGE_SITE_CAMP_UPDATE:
        apply_site_camp_update(message.payload_as<Gs1EngineMessageCampData>());
        break;
    case GS1_ENGINE_MESSAGE_SITE_WEATHER_UPDATE:
        apply_site_weather_update(message.payload_as<Gs1EngineMessageWeatherData>());
        break;
    case GS1_ENGINE_MESSAGE_SITE_INVENTORY_STORAGE_UPSERT:
        apply_site_inventory_storage_upsert(message.payload_as<Gs1EngineMessageInventoryStorageData>());
        break;
    case GS1_ENGINE_MESSAGE_SITE_INVENTORY_SLOT_UPSERT:
        apply_site_inventory_slot_upsert(message.payload_as<Gs1EngineMessageInventorySlotData>());
        break;
    case GS1_ENGINE_MESSAGE_SITE_INVENTORY_VIEW_STATE:
        apply_site_inventory_view_state(message.payload_as<Gs1EngineMessageInventoryViewData>());
        break;
    case GS1_ENGINE_MESSAGE_SITE_CRAFT_CONTEXT_BEGIN:
        apply_site_craft_context_begin(message.payload_as<Gs1EngineMessageCraftContextData>());
        break;
    case GS1_ENGINE_MESSAGE_SITE_CRAFT_CONTEXT_OPTION_UPSERT:
        apply_site_craft_context_option_upsert(message.payload_as<Gs1EngineMessageCraftContextOptionData>());
        break;
    case GS1_ENGINE_MESSAGE_SITE_CRAFT_CONTEXT_END:
        apply_site_craft_context_end();
        break;
    case GS1_ENGINE_MESSAGE_SITE_PLACEMENT_PREVIEW:
        apply_site_placement_preview(message.payload_as<Gs1EngineMessagePlacementPreviewData>());
        break;
    case GS1_ENGINE_MESSAGE_SITE_PLACEMENT_PREVIEW_TILE_UPSERT:
        apply_site_placement_preview_tile_upsert(message.payload_as<Gs1EngineMessagePlacementPreviewTileData>());
        break;
    case GS1_ENGINE_MESSAGE_SITE_PLACEMENT_FAILURE:
        apply_site_placement_failure(message.payload_as<Gs1EngineMessagePlacementFailureData>());
        break;
    case GS1_ENGINE_MESSAGE_SITE_TASK_UPSERT:
        apply_site_task_upsert(message.payload_as<Gs1EngineMessageTaskData>());
        break;
    case GS1_ENGINE_MESSAGE_SITE_TASK_REMOVE:
        apply_site_task_remove(message.payload_as<Gs1EngineMessageTaskData>());
        break;
    case GS1_ENGINE_MESSAGE_SITE_PHONE_LISTING_UPSERT:
        apply_site_phone_listing_upsert(message.payload_as<Gs1EngineMessagePhoneListingData>());
        break;
    case GS1_ENGINE_MESSAGE_SITE_PHONE_LISTING_REMOVE:
        apply_site_phone_listing_remove(message.payload_as<Gs1EngineMessagePhoneListingData>());
        break;
    case GS1_ENGINE_MESSAGE_SITE_PHONE_PANEL_STATE:
        apply_site_phone_panel_state(message.payload_as<Gs1EngineMessagePhonePanelData>());
        break;
    case GS1_ENGINE_MESSAGE_SITE_MODIFIER_LIST_BEGIN:
        apply_site_modifier_list_begin(message.payload_as<Gs1EngineMessageSiteModifierListData>());
        break;
    case GS1_ENGINE_MESSAGE_SITE_MODIFIER_UPSERT:
        apply_site_modifier_upsert(message.payload_as<Gs1EngineMessageSiteModifierData>());
        break;
    case GS1_ENGINE_MESSAGE_SITE_PROTECTION_OVERLAY_STATE:
        apply_site_protection_overlay_state(message.payload_as<Gs1EngineMessageSiteProtectionOverlayData>());
        break;
    case GS1_ENGINE_MESSAGE_END_SITE_SNAPSHOT:
        apply_site_snapshot_end();
        break;
    case GS1_ENGINE_MESSAGE_HUD_STATE:
        apply_hud_state(message.payload_as<Gs1EngineMessageHudStateData>());
        break;
    case GS1_ENGINE_MESSAGE_CAMPAIGN_RESOURCES:
        apply_campaign_resources(message.payload_as<Gs1EngineMessageCampaignResourcesData>());
        break;
    case GS1_ENGINE_MESSAGE_SITE_ACTION_UPDATE:
        apply_site_action_update(message.payload_as<Gs1EngineMessageSiteActionData>());
        break;
    case GS1_ENGINE_MESSAGE_SITE_RESULT_READY:
        apply_site_result_ready(message.payload_as<Gs1EngineMessageSiteResultData>());
        break;
    case GS1_ENGINE_MESSAGE_PLAY_ONE_SHOT_CUE:
        apply_one_shot_cue(message.payload_as<Gs1EngineMessageOneShotCueData>());
        break;
    default:
        break;
    }
}

void Gs1RuntimeProjectionCache::apply_set_app_state(const Gs1EngineMessageSetAppStateData& payload)
{
    state_.current_app_state = payload.app_state;

    if (payload.app_state == GS1_APP_STATE_MAIN_MENU)
    {
        state_.selected_site_id.reset();
        state_.regional_map_sites.clear();
        state_.regional_map_links.clear();
        state_.active_site.reset();
        state_.hud.reset();
        state_.site_action.reset();
        state_.site_result.reset();
    }
    else if (payload.app_state == GS1_APP_STATE_REGIONAL_MAP)
    {
        state_.active_site.reset();
        state_.hud.reset();
        state_.site_action.reset();
        state_.site_result.reset();
    }
    else if (payload.app_state == GS1_APP_STATE_CAMPAIGN_END)
    {
        state_.active_site.reset();
        state_.hud.reset();
        state_.site_action.reset();
    }
}

void Gs1RuntimeProjectionCache::apply_regional_map_snapshot_begin(const Gs1EngineMessageRegionalMapSnapshotData& payload)
{
    if (payload.mode == GS1_PROJECTION_MODE_DELTA)
    {
        pending_regional_map_ = PendingRegionalMapState {state_.regional_map_sites, state_.regional_map_links};
    }
    else
    {
        pending_regional_map_ = PendingRegionalMapState {};
    }

    if (payload.selected_site_id != 0U)
    {
        state_.selected_site_id = payload.selected_site_id;
    }
    else
    {
        state_.selected_site_id.reset();
    }
}

void Gs1RuntimeProjectionCache::apply_regional_map_site_upsert(const Gs1EngineMessageRegionalMapSiteData& payload)
{
    if (!pending_regional_map_.has_value())
    {
        return;
    }

    Gs1RuntimeRegionalMapSiteProjection projection {};
    projection.site_id = payload.site_id;
    projection.site_state = payload.site_state;
    projection.site_archetype_id = payload.site_archetype_id;
    projection.flags = payload.flags;
    projection.map_x = payload.map_x;
    projection.map_y = payload.map_y;
    projection.support_package_id = payload.support_package_id;
    projection.support_preview_mask = payload.support_preview_mask;
    upsert_projection(
        pending_regional_map_->sites,
        projection,
        projection.site_id,
        [](const auto& site) { return site.site_id; });

    if ((projection.flags & 1U) != 0U)
    {
        state_.selected_site_id = projection.site_id;
    }
}

void Gs1RuntimeProjectionCache::apply_regional_map_site_remove(const Gs1EngineMessageRegionalMapSiteData& payload)
{
    if (!pending_regional_map_.has_value())
    {
        return;
    }

    erase_projection_if(pending_regional_map_->sites, [&](const auto& site) {
        return site.site_id == payload.site_id;
    });
    if (state_.selected_site_id.has_value() && state_.selected_site_id.value() == payload.site_id)
    {
        state_.selected_site_id.reset();
    }
}

void Gs1RuntimeProjectionCache::apply_regional_map_link_upsert(const Gs1EngineMessageRegionalMapLinkData& payload)
{
    if (!pending_regional_map_.has_value())
    {
        return;
    }

    Gs1RuntimeRegionalMapLinkProjection projection {};
    projection.from_site_id = payload.from_site_id;
    projection.to_site_id = payload.to_site_id;
    projection.flags = payload.flags;
    upsert_projection(
        pending_regional_map_->links,
        projection,
        regional_map_link_key(projection.from_site_id, projection.to_site_id),
        [](const auto& link) { return regional_map_link_key(link.from_site_id, link.to_site_id); });
}

void Gs1RuntimeProjectionCache::apply_regional_map_link_remove(const Gs1EngineMessageRegionalMapLinkData& payload)
{
    if (!pending_regional_map_.has_value())
    {
        return;
    }

    const auto key = regional_map_link_key(payload.from_site_id, payload.to_site_id);
    erase_projection_if(pending_regional_map_->links, [&](const auto& link) {
        return regional_map_link_key(link.from_site_id, link.to_site_id) == key;
    });
}

void Gs1RuntimeProjectionCache::apply_regional_map_snapshot_end()
{
    if (!pending_regional_map_.has_value())
    {
        return;
    }

    sort_regional_map_sites(pending_regional_map_->sites);
    sort_regional_map_links(pending_regional_map_->links);
    state_.regional_map_sites = std::move(pending_regional_map_->sites);
    state_.regional_map_links = std::move(pending_regional_map_->links);
    pending_regional_map_.reset();
}

void Gs1RuntimeProjectionCache::apply_ui_setup_begin(const Gs1EngineMessageUiSetupData& payload)
{
    pending_ui_setup_ = PendingUiSetup {};
    pending_ui_setup_->setup_id = payload.setup_id;
    pending_ui_setup_->presentation_type = payload.presentation_type;
    pending_ui_setup_->context_id = payload.context_id;
    pending_ui_setup_->elements.clear();
    pending_ui_setup_->elements.reserve(payload.element_count);
}

void Gs1RuntimeProjectionCache::apply_ui_element_upsert(const Gs1EngineMessageUiElementData& payload)
{
    if (!pending_ui_setup_.has_value())
    {
        return;
    }

    Gs1RuntimeUiElementProjection projection {};
    projection.element_id = payload.element_id;
    projection.element_type = payload.element_type;
    projection.flags = payload.flags;
    projection.action = payload.action;
    projection.content_kind = payload.content_kind;
    projection.primary_id = payload.primary_id;
    projection.secondary_id = payload.secondary_id;
    projection.quantity = payload.quantity;
    pending_ui_setup_->elements.push_back(std::move(projection));
}

void Gs1RuntimeProjectionCache::apply_ui_setup_end()
{
    if (!pending_ui_setup_.has_value())
    {
        return;
    }

    Gs1RuntimeUiSetupProjection setup {};
    setup.setup_id = pending_ui_setup_->setup_id;
    setup.presentation_type = pending_ui_setup_->presentation_type;
    setup.context_id = pending_ui_setup_->context_id;
    setup.elements = std::move(pending_ui_setup_->elements);

    if (setup.presentation_type == GS1_UI_SETUP_PRESENTATION_NORMAL)
    {
        erase_projection_if(state_.active_ui_setups, [](const auto& existing) {
            return existing.presentation_type == GS1_UI_SETUP_PRESENTATION_NORMAL;
        });
    }

    upsert_projection(
        state_.active_ui_setups,
        std::move(setup),
        pending_ui_setup_->setup_id,
        [](const auto& existing) { return existing.setup_id; });
    sort_ui_setups(state_.active_ui_setups);
    pending_ui_setup_.reset();
}

void Gs1RuntimeProjectionCache::apply_ui_setup_close(const Gs1EngineMessageCloseUiSetupData& payload)
{
    erase_projection_if(state_.active_ui_setups, [&](const auto& setup) {
        return setup.setup_id == payload.setup_id;
    });
}

void Gs1RuntimeProjectionCache::apply_progression_view_begin(const Gs1EngineMessageProgressionViewData& payload)
{
    pending_progression_view_ = PendingProgressionView {};
    pending_progression_view_->view_id = payload.view_id;
    pending_progression_view_->context_id = payload.context_id;
    pending_progression_view_->entries.clear();
    pending_progression_view_->entries.reserve(payload.entry_count);
}

void Gs1RuntimeProjectionCache::apply_progression_entry_upsert(const Gs1EngineMessageProgressionEntryData& payload)
{
    if (!pending_progression_view_.has_value())
    {
        return;
    }

    Gs1RuntimeProgressionEntryProjection projection {};
    projection.entry_id = payload.entry_id;
    projection.reputation_requirement = payload.reputation_requirement;
    projection.content_id = payload.content_id;
    projection.tech_node_id = payload.tech_node_id;
    projection.faction_id = payload.faction_id;
    projection.entry_kind = payload.entry_kind;
    projection.flags = payload.flags;
    projection.content_kind = payload.content_kind;
    projection.tier_index = payload.tier_index;
    projection.action = payload.action;
    pending_progression_view_->entries.push_back(std::move(projection));
}

void Gs1RuntimeProjectionCache::apply_progression_view_end()
{
    if (!pending_progression_view_.has_value())
    {
        return;
    }

    Gs1RuntimeProgressionViewProjection view {};
    view.view_id = pending_progression_view_->view_id;
    view.context_id = pending_progression_view_->context_id;
    view.entries = std::move(pending_progression_view_->entries);
    sort_progression_entries(view.entries);

    upsert_projection(
        state_.active_progression_views,
        std::move(view),
        pending_progression_view_->view_id,
        [](const auto& existing) { return existing.view_id; });
    sort_progression_views(state_.active_progression_views);
    pending_progression_view_.reset();
}

void Gs1RuntimeProjectionCache::apply_progression_view_close(const Gs1EngineMessageCloseProgressionViewData& payload)
{
    erase_projection_if(state_.active_progression_views, [&](const auto& view) {
        return view.view_id == payload.view_id;
    });
}

void Gs1RuntimeProjectionCache::apply_ui_panel_begin(const Gs1EngineMessageUiPanelData& payload)
{
    pending_ui_panel_ = PendingUiPanel {};
    pending_ui_panel_->panel_id = payload.panel_id;
    pending_ui_panel_->context_id = payload.context_id;
    pending_ui_panel_->text_lines.clear();
    pending_ui_panel_->slot_actions.clear();
    pending_ui_panel_->list_items.clear();
    pending_ui_panel_->list_actions.clear();
    pending_ui_panel_->text_lines.reserve(payload.text_line_count);
    pending_ui_panel_->slot_actions.reserve(payload.slot_action_count);
    pending_ui_panel_->list_items.reserve(payload.list_item_count);
    pending_ui_panel_->list_actions.reserve(payload.list_action_count);
}

void Gs1RuntimeProjectionCache::apply_ui_panel_text_upsert(const Gs1EngineMessageUiPanelTextData& payload)
{
    if (!pending_ui_panel_.has_value())
    {
        return;
    }

    Gs1RuntimeUiPanelTextProjection projection {};
    projection.line_id = payload.line_id;
    projection.flags = payload.flags;
    projection.text_kind = payload.text_kind;
    projection.primary_id = payload.primary_id;
    projection.secondary_id = payload.secondary_id;
    projection.quantity = payload.quantity;
    projection.aux_value = payload.aux_value;
    pending_ui_panel_->text_lines.push_back(std::move(projection));
}

void Gs1RuntimeProjectionCache::apply_ui_panel_slot_action_upsert(const Gs1EngineMessageUiPanelSlotActionData& payload)
{
    if (!pending_ui_panel_.has_value())
    {
        return;
    }

    Gs1RuntimeUiPanelSlotActionProjection projection {};
    projection.slot_id = payload.slot_id;
    projection.flags = payload.flags;
    projection.action = payload.action;
    projection.label_kind = payload.label_kind;
    projection.primary_id = payload.primary_id;
    projection.secondary_id = payload.secondary_id;
    projection.quantity = payload.quantity;
    pending_ui_panel_->slot_actions.push_back(std::move(projection));
}

void Gs1RuntimeProjectionCache::apply_ui_panel_list_item_upsert(const Gs1EngineMessageUiPanelListItemData& payload)
{
    if (!pending_ui_panel_.has_value())
    {
        return;
    }

    Gs1RuntimeUiPanelListItemProjection projection {};
    projection.item_id = payload.item_id;
    projection.list_id = payload.list_id;
    projection.flags = payload.flags;
    projection.primary_kind = payload.primary_kind;
    projection.secondary_kind = payload.secondary_kind;
    projection.primary_id = payload.primary_id;
    projection.secondary_id = payload.secondary_id;
    projection.quantity = payload.quantity;
    projection.map_x = payload.map_x;
    projection.map_y = payload.map_y;
    pending_ui_panel_->list_items.push_back(std::move(projection));
}

void Gs1RuntimeProjectionCache::apply_ui_panel_list_action_upsert(const Gs1EngineMessageUiPanelListActionData& payload)
{
    if (!pending_ui_panel_.has_value())
    {
        return;
    }

    Gs1RuntimeUiPanelListActionProjection projection {};
    projection.item_id = payload.item_id;
    projection.list_id = payload.list_id;
    projection.role = payload.role;
    projection.flags = payload.flags;
    projection.action = payload.action;
    pending_ui_panel_->list_actions.push_back(std::move(projection));
}

void Gs1RuntimeProjectionCache::apply_ui_panel_end()
{
    if (!pending_ui_panel_.has_value())
    {
        return;
    }

    Gs1RuntimeUiPanelProjection panel {};
    panel.panel_id = pending_ui_panel_->panel_id;
    panel.context_id = pending_ui_panel_->context_id;
    panel.text_lines = std::move(pending_ui_panel_->text_lines);
    panel.slot_actions = std::move(pending_ui_panel_->slot_actions);
    panel.list_items = std::move(pending_ui_panel_->list_items);
    panel.list_actions = std::move(pending_ui_panel_->list_actions);

    sort_ui_panel_text_lines(panel.text_lines);
    sort_ui_panel_slot_actions(panel.slot_actions);
    sort_ui_panel_list_items(panel.list_items);
    sort_ui_panel_list_actions(panel.list_actions);

    upsert_projection(
        state_.active_ui_panels,
        std::move(panel),
        pending_ui_panel_->panel_id,
        [](const auto& existing) { return existing.panel_id; });
    sort_ui_panels(state_.active_ui_panels);
    pending_ui_panel_.reset();
}

void Gs1RuntimeProjectionCache::apply_ui_panel_close(const Gs1EngineMessageCloseUiPanelData& payload)
{
    erase_projection_if(state_.active_ui_panels, [&](const auto& panel) {
        return panel.panel_id == payload.panel_id;
    });
}

void Gs1RuntimeProjectionCache::apply_site_snapshot_begin(const Gs1EngineMessageSiteSnapshotData& payload)
{
    if (payload.mode == GS1_PROJECTION_MODE_DELTA && state_.active_site.has_value())
    {
        pending_site_ = state_.active_site;
    }
    else
    {
        pending_site_ = Gs1RuntimeSiteProjection {};
    }

    pending_site_->site_id = payload.site_id;
    pending_site_->site_archetype_id = payload.site_archetype_id;
    pending_site_->width = payload.width;
    pending_site_->height = payload.height;

    if (payload.mode == GS1_PROJECTION_MODE_SNAPSHOT)
    {
        pending_site_->tiles.assign(site_tile_capacity(*pending_site_), Gs1RuntimeTileProjection {});
        for (Gs1RuntimeTileProjection& tile : pending_site_->tiles)
        {
            tile.x = k_invalid_tile_coordinate;
            tile.y = k_invalid_tile_coordinate;
        }
        pending_site_->inventory_storages.clear();
        pending_site_->worker_pack_slots.clear();
        pending_site_->tasks.clear();
        pending_site_->active_modifiers.clear();
        pending_site_->phone_listings.clear();
        pending_site_->worker_pack_open = false;
        pending_site_->phone_panel = Gs1RuntimePhonePanelProjection {};
        pending_site_->protection_overlay = Gs1RuntimeProtectionOverlayProjection {};
        pending_site_->opened_storage.reset();
        pending_site_->craft_context.reset();
        pending_site_->placement_preview.reset();
        pending_site_->placement_preview_tiles.clear();
        pending_site_->placement_failure.reset();
        pending_site_->worker.reset();
        pending_site_->camp.reset();
        pending_site_->weather.reset();
    }
    else if (pending_site_->tiles.size() != site_tile_capacity(*pending_site_))
    {
        pending_site_->tiles.assign(site_tile_capacity(*pending_site_), Gs1RuntimeTileProjection {});
        for (Gs1RuntimeTileProjection& tile : pending_site_->tiles)
        {
            tile.x = k_invalid_tile_coordinate;
            tile.y = k_invalid_tile_coordinate;
        }
    }
}

void Gs1RuntimeProjectionCache::apply_site_tile_upsert(const Gs1EngineMessageSiteTileData& payload)
{
    if (!pending_site_.has_value())
    {
        return;
    }

    Gs1RuntimeTileProjection projection {};
    projection.x = payload.x;
    projection.y = payload.y;
    projection.terrain_type_id = payload.terrain_type_id;
    projection.plant_type_id = payload.plant_type_id;
    projection.structure_type_id = payload.structure_type_id;
    projection.ground_cover_type_id = payload.ground_cover_type_id;
    projection.plant_density = payload.plant_density;
    projection.sand_burial = payload.sand_burial;
    projection.local_wind = payload.local_wind;
    projection.wind_protection = payload.wind_protection;
    projection.heat_protection = payload.heat_protection;
    projection.dust_protection = payload.dust_protection;
    projection.moisture = payload.moisture;
    projection.soil_fertility = payload.soil_fertility;
    projection.soil_salinity = payload.soil_salinity;
    projection.device_integrity_quantized = payload.device_integrity_quantized;
    projection.excavation_depth = payload.excavation_depth;
    projection.visible_excavation_depth = payload.visible_excavation_depth;

    const auto tile_index = site_tile_index(*pending_site_, projection.x, projection.y);
    if (!tile_index.has_value())
    {
        return;
    }

    pending_site_->tiles[*tile_index] = projection;
}

void Gs1RuntimeProjectionCache::apply_site_worker_update(const Gs1EngineMessageWorkerData& payload)
{
    if (!pending_site_.has_value())
    {
        return;
    }

    Gs1RuntimeWorkerProjection projection {};
    projection.entity_id = payload.entity_id;
    projection.tile_x = payload.tile_x;
    projection.tile_y = payload.tile_y;
    projection.facing_degrees = payload.facing_degrees;
    projection.health_normalized = payload.health_normalized;
    projection.hydration_normalized = payload.hydration_normalized;
    projection.energy_normalized = payload.energy_normalized;
    projection.flags = payload.flags;
    projection.current_action_kind = payload.current_action_kind;
    pending_site_->worker = projection;
}

void Gs1RuntimeProjectionCache::apply_site_camp_update(const Gs1EngineMessageCampData& payload)
{
    if (!pending_site_.has_value())
    {
        return;
    }

    Gs1RuntimeCampProjection projection {};
    projection.tile_x = payload.tile_x;
    projection.tile_y = payload.tile_y;
    projection.durability_normalized = payload.durability_normalized;
    projection.flags = payload.flags;
    pending_site_->camp = projection;
}

void Gs1RuntimeProjectionCache::apply_site_weather_update(const Gs1EngineMessageWeatherData& payload)
{
    if (!pending_site_.has_value())
    {
        return;
    }

    Gs1RuntimeWeatherProjection projection {};
    projection.heat = payload.heat;
    projection.wind = payload.wind;
    projection.dust = payload.dust;
    projection.wind_direction_degrees = payload.wind_direction_degrees;
    projection.event_template_id = payload.event_template_id;
    projection.event_start_time_minutes = payload.event_start_time_minutes;
    projection.event_peak_time_minutes = payload.event_peak_time_minutes;
    projection.event_peak_duration_minutes = payload.event_peak_duration_minutes;
    projection.event_end_time_minutes = payload.event_end_time_minutes;
    pending_site_->weather = projection;
}

void Gs1RuntimeProjectionCache::apply_site_inventory_storage_upsert(const Gs1EngineMessageInventoryStorageData& payload)
{
    if (!pending_site_.has_value())
    {
        return;
    }

    Gs1RuntimeInventoryStorageProjection projection {};
    projection.storage_id = payload.storage_id;
    projection.owner_entity_id = payload.owner_entity_id;
    projection.slot_count = payload.slot_count;
    projection.tile_x = payload.tile_x;
    projection.tile_y = payload.tile_y;
    projection.container_kind = payload.container_kind;
    projection.flags = payload.flags;
    upsert_projection(
        pending_site_->inventory_storages,
        projection,
        projection.storage_id,
        [](const auto& storage) { return storage.storage_id; });
}

void Gs1RuntimeProjectionCache::apply_site_inventory_slot_upsert(const Gs1EngineMessageInventorySlotData& payload)
{
    if (!pending_site_.has_value())
    {
        return;
    }

    Gs1RuntimeInventorySlotProjection projection {};
    projection.item_id = payload.item_id;
    projection.item_instance_id = payload.item_instance_id;
    projection.condition = payload.condition;
    projection.freshness = payload.freshness;
    projection.storage_id = payload.storage_id;
    projection.container_owner_id = payload.container_owner_id;
    projection.quantity = payload.quantity;
    projection.slot_index = payload.slot_index;
    projection.container_tile_x = payload.container_tile_x;
    projection.container_tile_y = payload.container_tile_y;
    projection.container_kind = payload.container_kind;
    projection.flags = payload.flags;

    std::vector<Gs1RuntimeInventorySlotProjection>* slots = nullptr;
    if (projection.container_kind == GS1_INVENTORY_CONTAINER_WORKER_PACK)
    {
        slots = &pending_site_->worker_pack_slots;
    }
    else if (pending_site_->opened_storage.has_value() && pending_site_->opened_storage->storage_id == projection.storage_id)
    {
        slots = &pending_site_->opened_storage->slots;
    }

    if (slots == nullptr)
    {
        return;
    }

    upsert_projection(
        *slots,
        projection,
        (static_cast<std::uint64_t>(projection.storage_id) << 32U) | projection.slot_index,
        [](const auto& slot) {
            return (static_cast<std::uint64_t>(slot.storage_id) << 32U) | slot.slot_index;
        });
}

void Gs1RuntimeProjectionCache::apply_site_inventory_view_state(const Gs1EngineMessageInventoryViewData& payload)
{
    if (!pending_site_.has_value())
    {
        return;
    }

    const auto storage = std::find_if(
        pending_site_->inventory_storages.begin(),
        pending_site_->inventory_storages.end(),
        [&](const auto& projection) { return projection.storage_id == payload.storage_id; });
    const bool is_worker_pack_storage =
        storage != pending_site_->inventory_storages.end() &&
        storage->container_kind == GS1_INVENTORY_CONTAINER_WORKER_PACK;

    if (is_worker_pack_storage)
    {
        pending_site_->worker_pack_open = payload.event_kind == GS1_INVENTORY_VIEW_EVENT_OPEN_SNAPSHOT;
        return;
    }

    if (payload.event_kind == GS1_INVENTORY_VIEW_EVENT_CLOSE)
    {
        if (pending_site_->opened_storage.has_value() && pending_site_->opened_storage->storage_id == payload.storage_id)
        {
            pending_site_->opened_storage.reset();
        }
        return;
    }

    if (payload.event_kind == GS1_INVENTORY_VIEW_EVENT_OPEN_SNAPSHOT)
    {
        pending_site_->opened_storage = Gs1RuntimeInventoryViewProjection {};
        pending_site_->opened_storage->storage_id = payload.storage_id;
        pending_site_->opened_storage->slot_count = payload.slot_count;
        pending_site_->opened_storage->slots.clear();
    }
}

void Gs1RuntimeProjectionCache::apply_site_craft_context_begin(const Gs1EngineMessageCraftContextData& payload)
{
    if (!pending_site_.has_value())
    {
        return;
    }

    pending_site_->craft_context = Gs1RuntimeCraftContextProjection {};
    pending_site_->craft_context->tile_x = payload.tile_x;
    pending_site_->craft_context->tile_y = payload.tile_y;
    pending_site_->craft_context->flags = payload.flags;
    pending_site_->craft_context->options.clear();
    pending_site_->craft_context->options.reserve(payload.option_count);
}

void Gs1RuntimeProjectionCache::apply_site_craft_context_option_upsert(const Gs1EngineMessageCraftContextOptionData& payload)
{
    if (!pending_site_.has_value() || !pending_site_->craft_context.has_value())
    {
        return;
    }

    pending_site_->craft_context->options.push_back(Gs1RuntimeCraftContextOptionProjection {
        payload.recipe_id,
        payload.output_item_id,
        payload.flags});
}

void Gs1RuntimeProjectionCache::apply_site_craft_context_end()
{
    if (!pending_site_.has_value() || !pending_site_->craft_context.has_value())
    {
        return;
    }

    sort_craft_options(pending_site_->craft_context->options);
}

void Gs1RuntimeProjectionCache::apply_site_placement_preview(const Gs1EngineMessagePlacementPreviewData& payload)
{
    if (!pending_site_.has_value())
    {
        return;
    }

    if ((payload.flags & 1U) == 0U)
    {
        pending_site_->placement_preview.reset();
        pending_site_->placement_preview_tiles.clear();
        return;
    }

    pending_site_->placement_preview = Gs1RuntimePlacementPreviewProjection {
        payload.tile_x,
        payload.tile_y,
        payload.blocked_mask,
        payload.item_id,
        payload.preview_tile_count,
        payload.footprint_width,
        payload.footprint_height,
        payload.action_kind,
        payload.flags};
    pending_site_->placement_preview_tiles.clear();
}

void Gs1RuntimeProjectionCache::apply_site_placement_preview_tile_upsert(const Gs1EngineMessagePlacementPreviewTileData& payload)
{
    if (!pending_site_.has_value())
    {
        return;
    }

    Gs1RuntimePlacementPreviewTileProjection projection {};
    projection.x = payload.x;
    projection.y = payload.y;
    projection.flags = payload.flags;
    projection.wind_protection = payload.wind_protection;
    projection.heat_protection = payload.heat_protection;
    projection.dust_protection = payload.dust_protection;
    projection.final_wind_protection = payload.final_wind_protection;
    projection.final_heat_protection = payload.final_heat_protection;
    projection.final_dust_protection = payload.final_dust_protection;
    projection.occupant_condition = payload.occupant_condition;
    upsert_projection(
        pending_site_->placement_preview_tiles,
        projection,
        (static_cast<std::uint64_t>(static_cast<std::uint16_t>(projection.x)) << 16U) |
            static_cast<std::uint16_t>(projection.y),
        [](const auto& tile) {
            return (static_cast<std::uint64_t>(static_cast<std::uint16_t>(tile.x)) << 16U) |
                static_cast<std::uint16_t>(tile.y);
        });
}

void Gs1RuntimeProjectionCache::apply_site_placement_failure(const Gs1EngineMessagePlacementFailureData& payload)
{
    auto* site = pending_site_.has_value() ? &pending_site_.value() : (state_.active_site.has_value() ? &state_.active_site.value() : nullptr);
    if (site == nullptr)
    {
        return;
    }

    site->placement_failure = Gs1RuntimePlacementFailureProjection {
        payload.tile_x,
        payload.tile_y,
        payload.blocked_mask,
        payload.sequence_id,
        payload.action_kind,
        payload.flags};
}

void Gs1RuntimeProjectionCache::apply_site_task_upsert(const Gs1EngineMessageTaskData& payload)
{
    if (!pending_site_.has_value())
    {
        return;
    }

    Gs1RuntimeTaskProjection projection {};
    projection.task_instance_id = payload.task_instance_id;
    projection.task_template_id = payload.task_template_id;
    projection.publisher_faction_id = payload.publisher_faction_id;
    projection.current_progress = payload.current_progress;
    projection.target_progress = payload.target_progress;
    projection.list_kind = payload.list_kind;
    projection.flags = payload.flags;
    upsert_projection(
        pending_site_->tasks,
        projection,
        projection.task_instance_id,
        [](const auto& task) { return task.task_instance_id; });
}

void Gs1RuntimeProjectionCache::apply_site_task_remove(const Gs1EngineMessageTaskData& payload)
{
    if (!pending_site_.has_value())
    {
        return;
    }

    erase_projection_if(pending_site_->tasks, [&](const auto& task) {
        return task.task_instance_id == payload.task_instance_id;
    });
}

void Gs1RuntimeProjectionCache::apply_site_phone_listing_upsert(const Gs1EngineMessagePhoneListingData& payload)
{
    if (!pending_site_.has_value())
    {
        return;
    }

    Gs1RuntimePhoneListingProjection projection {};
    projection.listing_id = payload.listing_id;
    projection.item_or_unlockable_id = payload.item_or_unlockable_id;
    projection.price = payload.price;
    projection.related_site_id = payload.related_site_id;
    projection.quantity = payload.quantity;
    projection.cart_quantity = payload.cart_quantity;
    projection.listing_kind = payload.listing_kind;
    projection.flags = payload.flags;
    upsert_projection(
        pending_site_->phone_listings,
        projection,
        projection.listing_id,
        [](const auto& listing) { return listing.listing_id; });
}

void Gs1RuntimeProjectionCache::apply_site_phone_listing_remove(const Gs1EngineMessagePhoneListingData& payload)
{
    if (!pending_site_.has_value())
    {
        return;
    }

    erase_projection_if(pending_site_->phone_listings, [&](const auto& listing) {
        return listing.listing_id == payload.listing_id;
    });
}

void Gs1RuntimeProjectionCache::apply_site_phone_panel_state(const Gs1EngineMessagePhonePanelData& payload)
{
    if (!pending_site_.has_value())
    {
        return;
    }

    pending_site_->phone_panel = Gs1RuntimePhonePanelProjection {
        payload.active_section,
        payload.visible_task_count,
        payload.accepted_task_count,
        payload.completed_task_count,
        payload.claimed_task_count,
        payload.buy_listing_count,
        payload.sell_listing_count,
        payload.service_listing_count,
        payload.cart_item_count,
        payload.flags};
}

void Gs1RuntimeProjectionCache::apply_site_modifier_list_begin(const Gs1EngineMessageSiteModifierListData& payload)
{
    if (!pending_site_.has_value())
    {
        return;
    }

    if (payload.mode == GS1_PROJECTION_MODE_SNAPSHOT)
    {
        pending_site_->active_modifiers.clear();
    }
    pending_site_->active_modifiers.reserve(payload.modifier_count);
}

void Gs1RuntimeProjectionCache::apply_site_modifier_upsert(const Gs1EngineMessageSiteModifierData& payload)
{
    if (!pending_site_.has_value())
    {
        return;
    }

    Gs1RuntimeModifierProjection projection {};
    projection.modifier_id = payload.modifier_id;
    projection.remaining_game_hours = payload.remaining_game_hours;
    projection.flags = payload.flags;
    upsert_projection(
        pending_site_->active_modifiers,
        projection,
        projection.modifier_id,
        [](const auto& modifier) { return modifier.modifier_id; });
}

void Gs1RuntimeProjectionCache::apply_site_protection_overlay_state(const Gs1EngineMessageSiteProtectionOverlayData& payload)
{
    auto* site = pending_site_.has_value() ? &pending_site_.value() : (state_.active_site.has_value() ? &state_.active_site.value() : nullptr);
    if (site == nullptr)
    {
        return;
    }

    site->protection_overlay = Gs1RuntimeProtectionOverlayProjection {payload.mode};
}

void Gs1RuntimeProjectionCache::apply_site_snapshot_end()
{
    if (!pending_site_.has_value())
    {
        return;
    }

    sort_inventory_storages(pending_site_->inventory_storages);
    sort_inventory_slots(pending_site_->worker_pack_slots);
    if (pending_site_->opened_storage.has_value())
    {
        sort_inventory_slots(pending_site_->opened_storage->slots);
    }
    sort_tasks(pending_site_->tasks);
    sort_modifiers(pending_site_->active_modifiers);
    sort_phone_listings(pending_site_->phone_listings);
    sort_placement_preview_tiles(pending_site_->placement_preview_tiles);

    state_.active_site = std::move(pending_site_);
    pending_site_.reset();
}

void Gs1RuntimeProjectionCache::apply_hud_state(const Gs1EngineMessageHudStateData& payload)
{
    Gs1RuntimeHudProjection projection {};
    projection.player_health = payload.player_health;
    projection.player_hydration = payload.player_hydration;
    projection.player_nourishment = payload.player_nourishment;
    projection.player_energy = payload.player_energy;
    projection.player_morale = payload.player_morale;
    projection.current_money = payload.current_money;
    projection.site_completion_normalized = payload.site_completion_normalized;
    projection.active_task_count = payload.active_task_count;
    projection.warning_code = payload.warning_code;
    projection.current_action_kind = payload.current_action_kind;
    state_.hud = projection;
}

void Gs1RuntimeProjectionCache::apply_campaign_resources(const Gs1EngineMessageCampaignResourcesData& payload)
{
    Gs1RuntimeCampaignResourcesProjection projection {};
    projection.current_money = payload.current_money;
    projection.total_reputation = payload.total_reputation;
    projection.village_reputation = payload.village_reputation;
    projection.forestry_reputation = payload.forestry_reputation;
    projection.university_reputation = payload.university_reputation;
    state_.campaign_resources = projection;
}

void Gs1RuntimeProjectionCache::apply_site_action_update(const Gs1EngineMessageSiteActionData& payload)
{
    if ((payload.flags & GS1_SITE_ACTION_PRESENTATION_FLAG_CLEAR) != 0U)
    {
        state_.site_action.reset();
        return;
    }

    Gs1RuntimeSiteActionProjection projection {};
    projection.action_id = payload.action_id;
    projection.target_tile_x = payload.target_tile_x;
    projection.target_tile_y = payload.target_tile_y;
    projection.action_kind = payload.action_kind;
    projection.flags = payload.flags;
    projection.progress_normalized = payload.progress_normalized;
    projection.duration_minutes = payload.duration_minutes;
    state_.site_action = projection;
}

void Gs1RuntimeProjectionCache::apply_site_result_ready(const Gs1EngineMessageSiteResultData& payload)
{
    Gs1RuntimeSiteResultProjection projection {};
    projection.site_id = payload.site_id;
    projection.result = payload.result;
    projection.newly_revealed_site_count = payload.newly_revealed_site_count;
    state_.site_result = projection;
}

void Gs1RuntimeProjectionCache::apply_one_shot_cue(const Gs1EngineMessageOneShotCueData& payload)
{
    Gs1RuntimeOneShotCueProjection projection {};
    projection.sequence_id = next_one_shot_cue_sequence_id_;
    next_one_shot_cue_sequence_id_ += 1U;
    projection.subject_id = payload.subject_id;
    projection.arg0 = payload.arg0;
    projection.arg1 = payload.arg1;
    projection.world_x = payload.world_x;
    projection.world_y = payload.world_y;
    projection.cue_kind = payload.cue_kind;
    state_.recent_one_shot_cues.push_back(projection);
    if (state_.recent_one_shot_cues.size() > 32U)
    {
        state_.recent_one_shot_cues.erase(state_.recent_one_shot_cues.begin());
    }
}

std::size_t Gs1RuntimeProjectionCache::site_tile_capacity(const Gs1RuntimeSiteProjection& site) const noexcept
{
    return static_cast<std::size_t>(site.width) * static_cast<std::size_t>(site.height);
}

std::optional<std::uint32_t> Gs1RuntimeProjectionCache::site_tile_index(
    const Gs1RuntimeSiteProjection& site,
    std::uint16_t x,
    std::uint16_t y) const noexcept
{
    if (x >= site.width || y >= site.height)
    {
        return std::nullopt;
    }

    return static_cast<std::uint32_t>(
        static_cast<std::size_t>(y) * static_cast<std::size_t>(site.width) + static_cast<std::size_t>(x));
}
