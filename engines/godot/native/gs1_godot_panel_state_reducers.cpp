#include "gs1_godot_panel_state_reducers.h"

#include <algorithm>
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

constexpr std::uint64_t ui_panel_list_item_key(Gs1UiPanelListId list_id, std::uint32_t item_id) noexcept
{
    return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(list_id)) << 32U) | static_cast<std::uint64_t>(item_id);
}

constexpr std::uint64_t ui_panel_list_action_key(
    Gs1UiPanelListId list_id,
    std::uint32_t item_id,
    Gs1UiPanelListActionRole role) noexcept
{
    return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(list_id)) << 32U) |
        (static_cast<std::uint64_t>(item_id) << 8U) |
        static_cast<std::uint64_t>(static_cast<std::uint8_t>(role));
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
        return regional_map_link_key(lhs.from_site_id, lhs.to_site_id) < regional_map_link_key(rhs.from_site_id, rhs.to_site_id);
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

void Gs1GodotStatusStateReducer::reset() noexcept
{
    state_ = {};
    next_one_shot_cue_sequence_id_ = 1U;
}

void Gs1GodotStatusStateReducer::apply_engine_message(const Gs1EngineMessage& message)
{
    switch (message.type)
    {
    case GS1_ENGINE_MESSAGE_SET_APP_STATE:
    {
        const auto& payload = message.payload_as<Gs1EngineMessageSetAppStateData>();
        state_.current_app_state = payload.app_state;
        if (payload.app_state == GS1_APP_STATE_MAIN_MENU)
        {
            state_.hud.reset();
            state_.site_action.reset();
            state_.site_result.reset();
        }
        else if (payload.app_state == GS1_APP_STATE_REGIONAL_MAP)
        {
            state_.hud.reset();
            state_.site_action.reset();
            state_.site_result.reset();
        }
        else if (payload.app_state == GS1_APP_STATE_CAMPAIGN_END)
        {
            state_.hud.reset();
            state_.site_action.reset();
        }
        break;
    }
    case GS1_ENGINE_MESSAGE_CAMPAIGN_RESOURCES:
        state_.campaign_resources = message.payload_as<Gs1EngineMessageCampaignResourcesData>();
        break;
    case GS1_ENGINE_MESSAGE_HUD_STATE:
        state_.hud = message.payload_as<Gs1EngineMessageHudStateData>();
        break;
    case GS1_ENGINE_MESSAGE_SITE_ACTION_UPDATE:
    {
        const auto& payload = message.payload_as<Gs1EngineMessageSiteActionData>();
        if ((payload.flags & GS1_SITE_ACTION_PRESENTATION_FLAG_CLEAR) != 0U)
        {
            state_.site_action.reset();
        }
        else
        {
            state_.site_action = payload;
        }
        break;
    }
    case GS1_ENGINE_MESSAGE_SITE_RESULT_READY:
        state_.site_result = message.payload_as<Gs1EngineMessageSiteResultData>();
        break;
    case GS1_ENGINE_MESSAGE_PLAY_ONE_SHOT_CUE:
    {
        const auto& payload = message.payload_as<Gs1EngineMessageOneShotCueData>();
        Gs1RuntimeOneShotCueProjection projection {};
        projection.sequence_id = next_one_shot_cue_sequence_id_++;
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
        break;
    }
    default:
        break;
    }
}

void Gs1GodotUiSetupStateReducer::reset() noexcept
{
    setups_.clear();
    pending_.reset();
    setup_indices_.clear();
    pending_element_indices_.clear();
}

void Gs1GodotUiSetupStateReducer::rebuild_indices() noexcept
{
    setup_indices_.clear();
    setup_indices_.reserve(setups_.size());
    for (std::size_t index = 0; index < setups_.size(); ++index)
    {
        setup_indices_[static_cast<std::uint16_t>(setups_[index].setup_id)] = index;
    }
}

void Gs1GodotUiSetupStateReducer::apply_engine_message(const Gs1EngineMessage& message)
{
    switch (message.type)
    {
    case GS1_ENGINE_MESSAGE_BEGIN_UI_SETUP:
    {
        const auto& payload = message.payload_as<Gs1EngineMessageUiSetupData>();
        pending_element_indices_.clear();
        if (payload.mode == GS1_PROJECTION_MODE_DELTA)
        {
            const auto found = setup_indices_.find(static_cast<std::uint16_t>(payload.setup_id));
            if (found != setup_indices_.end() && found->second < setups_.size())
            {
                const auto& existing = setups_[found->second];
                pending_ = PendingUiSetup {};
                pending_->setup_id = existing.setup_id;
                pending_->presentation_type = existing.presentation_type;
                pending_->context_id = existing.context_id;
                pending_->elements = existing.elements;
                pending_element_indices_.reserve(pending_->elements.size());
                for (std::size_t index = 0; index < pending_->elements.size(); ++index)
                {
                    pending_element_indices_[pending_->elements[index].element_id] = index;
                }
            }
            else
            {
                pending_ = PendingUiSetup {};
            }
        }
        else
        {
            pending_ = PendingUiSetup {};
        }

        pending_->setup_id = payload.setup_id;
        pending_->presentation_type = payload.presentation_type;
        pending_->context_id = payload.context_id;
        if (payload.mode == GS1_PROJECTION_MODE_SNAPSHOT)
        {
            pending_->elements.clear();
        }
        pending_->elements.reserve(std::max<std::size_t>(pending_->elements.size(), payload.element_count));
        break;
    }
    case GS1_ENGINE_MESSAGE_UI_ELEMENT_UPSERT:
    {
        if (!pending_.has_value())
        {
            break;
        }
        const auto& payload = message.payload_as<Gs1EngineMessageUiElementData>();
        Gs1RuntimeUiElementProjection projection {};
        projection.element_id = payload.element_id;
        projection.element_type = payload.element_type;
        projection.flags = payload.flags;
        projection.action = payload.action;
        projection.content_kind = payload.content_kind;
        projection.primary_id = payload.primary_id;
        projection.secondary_id = payload.secondary_id;
        projection.quantity = payload.quantity;
        const auto found = pending_element_indices_.find(projection.element_id);
        if (found != pending_element_indices_.end() && found->second < pending_->elements.size())
        {
            pending_->elements[found->second] = projection;
        }
        else
        {
            pending_element_indices_[projection.element_id] = pending_->elements.size();
            pending_->elements.push_back(std::move(projection));
        }
        break;
    }
    case GS1_ENGINE_MESSAGE_END_UI_SETUP:
    {
        if (!pending_.has_value())
        {
            break;
        }

        Gs1RuntimeUiSetupProjection setup {};
        setup.setup_id = pending_->setup_id;
        setup.presentation_type = pending_->presentation_type;
        setup.context_id = pending_->context_id;
        setup.elements = std::move(pending_->elements);

        if (setup.presentation_type == GS1_UI_SETUP_PRESENTATION_NORMAL)
        {
            erase_projection_if(setups_, [](const auto& existing) {
                return existing.presentation_type == GS1_UI_SETUP_PRESENTATION_NORMAL;
            });
        }

        upsert_projection(setups_, std::move(setup), pending_->setup_id, [](const auto& existing) {
            return existing.setup_id;
        });
        sort_ui_setups(setups_);
        rebuild_indices();
        pending_.reset();
        pending_element_indices_.clear();
        break;
    }
    case GS1_ENGINE_MESSAGE_CLOSE_UI_SETUP:
    {
        const auto& payload = message.payload_as<Gs1EngineMessageCloseUiSetupData>();
        erase_projection_if(setups_, [&](const auto& setup) {
            return setup.setup_id == payload.setup_id;
        });
        rebuild_indices();
        break;
    }
    default:
        break;
    }
}

void Gs1GodotUiPanelStateReducer::reset() noexcept
{
    panels_.clear();
    pending_.reset();
    panel_indices_.clear();
    pending_text_line_indices_.clear();
    pending_slot_action_indices_.clear();
    pending_list_item_indices_.clear();
    pending_list_action_indices_.clear();
}

void Gs1GodotUiPanelStateReducer::rebuild_indices() noexcept
{
    panel_indices_.clear();
    panel_indices_.reserve(panels_.size());
    for (std::size_t index = 0; index < panels_.size(); ++index)
    {
        panel_indices_[static_cast<std::uint16_t>(panels_[index].panel_id)] = index;
    }
}

const Gs1RuntimeUiPanelProjection* Gs1GodotUiPanelStateReducer::find_panel(Gs1UiPanelId panel_id) const noexcept
{
    const auto found = panel_indices_.find(static_cast<std::uint16_t>(panel_id));
    if (found == panel_indices_.end() || found->second >= panels_.size())
    {
        return nullptr;
    }
    return &panels_[found->second];
}

void Gs1GodotUiPanelStateReducer::apply_engine_message(const Gs1EngineMessage& message)
{
    switch (message.type)
    {
    case GS1_ENGINE_MESSAGE_BEGIN_UI_PANEL:
    {
        const auto& payload = message.payload_as<Gs1EngineMessageUiPanelData>();
        pending_text_line_indices_.clear();
        pending_slot_action_indices_.clear();
        pending_list_item_indices_.clear();
        pending_list_action_indices_.clear();
        if (payload.mode == GS1_PROJECTION_MODE_DELTA)
        {
            const auto found = panel_indices_.find(static_cast<std::uint16_t>(payload.panel_id));
            if (found != panel_indices_.end() && found->second < panels_.size())
            {
                const auto& existing = panels_[found->second];
                pending_ = PendingUiPanel {};
                pending_->panel_id = existing.panel_id;
                pending_->context_id = existing.context_id;
                pending_->text_lines = existing.text_lines;
                pending_->slot_actions = existing.slot_actions;
                pending_->list_items = existing.list_items;
                pending_->list_actions = existing.list_actions;
                for (std::size_t index = 0; index < pending_->text_lines.size(); ++index)
                {
                    pending_text_line_indices_[pending_->text_lines[index].line_id] = index;
                }
                for (std::size_t index = 0; index < pending_->slot_actions.size(); ++index)
                {
                    pending_slot_action_indices_[static_cast<std::uint16_t>(pending_->slot_actions[index].slot_id)] = index;
                }
                for (std::size_t index = 0; index < pending_->list_items.size(); ++index)
                {
                    pending_list_item_indices_[ui_panel_list_item_key(
                        pending_->list_items[index].list_id,
                        pending_->list_items[index].item_id)] = index;
                }
                for (std::size_t index = 0; index < pending_->list_actions.size(); ++index)
                {
                    pending_list_action_indices_[ui_panel_list_action_key(
                        pending_->list_actions[index].list_id,
                        pending_->list_actions[index].item_id,
                        pending_->list_actions[index].role)] = index;
                }
            }
            else
            {
                pending_ = PendingUiPanel {};
            }
        }
        else
        {
            pending_ = PendingUiPanel {};
        }

        pending_->panel_id = payload.panel_id;
        pending_->context_id = payload.context_id;
        if (payload.mode == GS1_PROJECTION_MODE_SNAPSHOT)
        {
            pending_->text_lines.clear();
            pending_->slot_actions.clear();
            pending_->list_items.clear();
            pending_->list_actions.clear();
        }
        break;
    }
    case GS1_ENGINE_MESSAGE_UI_PANEL_TEXT_UPSERT:
    {
        if (!pending_.has_value())
        {
            break;
        }
        const auto& payload = message.payload_as<Gs1EngineMessageUiPanelTextData>();
        Gs1RuntimeUiPanelTextProjection projection {};
        projection.line_id = payload.line_id;
        projection.flags = payload.flags;
        projection.text_kind = payload.text_kind;
        projection.primary_id = payload.primary_id;
        projection.secondary_id = payload.secondary_id;
        projection.quantity = payload.quantity;
        projection.aux_value = payload.aux_value;
        const auto found = pending_text_line_indices_.find(projection.line_id);
        if (found != pending_text_line_indices_.end() && found->second < pending_->text_lines.size())
        {
            pending_->text_lines[found->second] = projection;
        }
        else
        {
            pending_text_line_indices_[projection.line_id] = pending_->text_lines.size();
            pending_->text_lines.push_back(std::move(projection));
        }
        break;
    }
    case GS1_ENGINE_MESSAGE_UI_PANEL_SLOT_ACTION_UPSERT:
    {
        if (!pending_.has_value())
        {
            break;
        }
        const auto& payload = message.payload_as<Gs1EngineMessageUiPanelSlotActionData>();
        Gs1RuntimeUiPanelSlotActionProjection projection {};
        projection.slot_id = payload.slot_id;
        projection.flags = payload.flags;
        projection.action = payload.action;
        projection.label_kind = payload.label_kind;
        projection.primary_id = payload.primary_id;
        projection.secondary_id = payload.secondary_id;
        projection.quantity = payload.quantity;
        const auto key = static_cast<std::uint16_t>(projection.slot_id);
        const auto found = pending_slot_action_indices_.find(key);
        if (found != pending_slot_action_indices_.end() && found->second < pending_->slot_actions.size())
        {
            pending_->slot_actions[found->second] = projection;
        }
        else
        {
            pending_slot_action_indices_[key] = pending_->slot_actions.size();
            pending_->slot_actions.push_back(std::move(projection));
        }
        break;
    }
    case GS1_ENGINE_MESSAGE_UI_PANEL_LIST_ITEM_UPSERT:
    {
        if (!pending_.has_value())
        {
            break;
        }
        const auto& payload = message.payload_as<Gs1EngineMessageUiPanelListItemData>();
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
        const auto key = ui_panel_list_item_key(projection.list_id, projection.item_id);
        const auto found = pending_list_item_indices_.find(key);
        if (found != pending_list_item_indices_.end() && found->second < pending_->list_items.size())
        {
            pending_->list_items[found->second] = projection;
        }
        else
        {
            pending_list_item_indices_[key] = pending_->list_items.size();
            pending_->list_items.push_back(std::move(projection));
        }
        break;
    }
    case GS1_ENGINE_MESSAGE_UI_PANEL_LIST_ACTION_UPSERT:
    {
        if (!pending_.has_value())
        {
            break;
        }
        const auto& payload = message.payload_as<Gs1EngineMessageUiPanelListActionData>();
        Gs1RuntimeUiPanelListActionProjection projection {};
        projection.item_id = payload.item_id;
        projection.list_id = payload.list_id;
        projection.role = payload.role;
        projection.flags = payload.flags;
        projection.action = payload.action;
        const auto key = ui_panel_list_action_key(projection.list_id, projection.item_id, projection.role);
        const auto found = pending_list_action_indices_.find(key);
        if (found != pending_list_action_indices_.end() && found->second < pending_->list_actions.size())
        {
            pending_->list_actions[found->second] = projection;
        }
        else
        {
            pending_list_action_indices_[key] = pending_->list_actions.size();
            pending_->list_actions.push_back(std::move(projection));
        }
        break;
    }
    case GS1_ENGINE_MESSAGE_END_UI_PANEL:
    {
        if (!pending_.has_value())
        {
            break;
        }
        Gs1RuntimeUiPanelProjection panel {};
        panel.panel_id = pending_->panel_id;
        panel.context_id = pending_->context_id;
        panel.text_lines = std::move(pending_->text_lines);
        panel.slot_actions = std::move(pending_->slot_actions);
        panel.list_items = std::move(pending_->list_items);
        panel.list_actions = std::move(pending_->list_actions);
        sort_ui_panel_text_lines(panel.text_lines);
        sort_ui_panel_slot_actions(panel.slot_actions);
        sort_ui_panel_list_items(panel.list_items);
        sort_ui_panel_list_actions(panel.list_actions);
        upsert_projection(panels_, std::move(panel), pending_->panel_id, [](const auto& existing) {
            return existing.panel_id;
        });
        sort_ui_panels(panels_);
        rebuild_indices();
        pending_.reset();
        pending_text_line_indices_.clear();
        pending_slot_action_indices_.clear();
        pending_list_item_indices_.clear();
        pending_list_action_indices_.clear();
        break;
    }
    case GS1_ENGINE_MESSAGE_CLOSE_UI_PANEL:
    {
        const auto& payload = message.payload_as<Gs1EngineMessageCloseUiPanelData>();
        erase_projection_if(panels_, [&](const auto& panel) {
            return panel.panel_id == payload.panel_id;
        });
        rebuild_indices();
        break;
    }
    default:
        break;
    }
}

void Gs1GodotProgressionViewStateReducer::reset() noexcept
{
    views_.clear();
    pending_.reset();
    view_indices_.clear();
    pending_entry_indices_.clear();
}

void Gs1GodotProgressionViewStateReducer::rebuild_indices() noexcept
{
    view_indices_.clear();
    view_indices_.reserve(views_.size());
    for (std::size_t index = 0; index < views_.size(); ++index)
    {
        view_indices_[static_cast<std::uint16_t>(views_[index].view_id)] = index;
    }
}

const Gs1RuntimeProgressionViewProjection* Gs1GodotProgressionViewStateReducer::find_view(Gs1ProgressionViewId view_id) const noexcept
{
    const auto found = view_indices_.find(static_cast<std::uint16_t>(view_id));
    if (found == view_indices_.end() || found->second >= views_.size())
    {
        return nullptr;
    }
    return &views_[found->second];
}

void Gs1GodotProgressionViewStateReducer::apply_engine_message(const Gs1EngineMessage& message)
{
    switch (message.type)
    {
    case GS1_ENGINE_MESSAGE_BEGIN_PROGRESSION_VIEW:
    {
        const auto& payload = message.payload_as<Gs1EngineMessageProgressionViewData>();
        pending_entry_indices_.clear();
        if (payload.mode == GS1_PROJECTION_MODE_DELTA)
        {
            const auto found = view_indices_.find(static_cast<std::uint16_t>(payload.view_id));
            if (found != view_indices_.end() && found->second < views_.size())
            {
                const auto& existing = views_[found->second];
                pending_ = PendingProgressionView {};
                pending_->view_id = existing.view_id;
                pending_->context_id = existing.context_id;
                pending_->entries = existing.entries;
                pending_entry_indices_.reserve(pending_->entries.size());
                for (std::size_t index = 0; index < pending_->entries.size(); ++index)
                {
                    pending_entry_indices_[pending_->entries[index].entry_id] = index;
                }
            }
            else
            {
                pending_ = PendingProgressionView {};
            }
        }
        else
        {
            pending_ = PendingProgressionView {};
        }
        pending_->view_id = payload.view_id;
        pending_->context_id = payload.context_id;
        if (payload.mode == GS1_PROJECTION_MODE_SNAPSHOT)
        {
            pending_->entries.clear();
        }
        pending_->entries.reserve(std::max<std::size_t>(pending_->entries.size(), payload.entry_count));
        break;
    }
    case GS1_ENGINE_MESSAGE_PROGRESSION_ENTRY_UPSERT:
    {
        if (!pending_.has_value())
        {
            break;
        }
        const auto& payload = message.payload_as<Gs1EngineMessageProgressionEntryData>();
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
        const auto found = pending_entry_indices_.find(projection.entry_id);
        if (found != pending_entry_indices_.end() && found->second < pending_->entries.size())
        {
            pending_->entries[found->second] = projection;
        }
        else
        {
            pending_entry_indices_[projection.entry_id] = pending_->entries.size();
            pending_->entries.push_back(std::move(projection));
        }
        break;
    }
    case GS1_ENGINE_MESSAGE_END_PROGRESSION_VIEW:
    {
        if (!pending_.has_value())
        {
            break;
        }
        Gs1RuntimeProgressionViewProjection view {};
        view.view_id = pending_->view_id;
        view.context_id = pending_->context_id;
        view.entries = std::move(pending_->entries);
        sort_progression_entries(view.entries);
        upsert_projection(views_, std::move(view), pending_->view_id, [](const auto& existing) {
            return existing.view_id;
        });
        sort_progression_views(views_);
        rebuild_indices();
        pending_.reset();
        pending_entry_indices_.clear();
        break;
    }
    case GS1_ENGINE_MESSAGE_CLOSE_PROGRESSION_VIEW:
    {
        const auto& payload = message.payload_as<Gs1EngineMessageCloseProgressionViewData>();
        erase_projection_if(views_, [&](const auto& view) {
            return view.view_id == payload.view_id;
        });
        rebuild_indices();
        break;
    }
    default:
        break;
    }
}

void Gs1GodotRegionalMapStateReducer::reset() noexcept
{
    state_ = {};
    pending_.reset();
}

void Gs1GodotRegionalMapStateReducer::apply_engine_message(const Gs1EngineMessage& message)
{
    switch (message.type)
    {
    case GS1_ENGINE_MESSAGE_SET_APP_STATE:
    {
        const auto& payload = message.payload_as<Gs1EngineMessageSetAppStateData>();
        if (payload.app_state == GS1_APP_STATE_MAIN_MENU)
        {
            state_.selected_site_id.reset();
            state_.sites.clear();
            state_.links.clear();
        }
        break;
    }
    case GS1_ENGINE_MESSAGE_BEGIN_REGIONAL_MAP_SNAPSHOT:
    {
        const auto& payload = message.payload_as<Gs1EngineMessageRegionalMapSnapshotData>();
        if (payload.mode == GS1_PROJECTION_MODE_DELTA)
        {
            pending_ = PendingRegionalMapState {state_.sites, state_.links};
        }
        else
        {
            pending_ = PendingRegionalMapState {};
        }

        if (payload.selected_site_id != 0U)
        {
            state_.selected_site_id = payload.selected_site_id;
        }
        else
        {
            state_.selected_site_id.reset();
        }
        break;
    }
    case GS1_ENGINE_MESSAGE_REGIONAL_MAP_SITE_UPSERT:
    {
        if (!pending_.has_value())
        {
            break;
        }
        Gs1RuntimeRegionalMapSiteProjection projection = message.payload_as<Gs1EngineMessageRegionalMapSiteData>();
        upsert_projection(pending_->sites, projection, projection.site_id, [](const auto& site) {
            return site.site_id;
        });
        if ((projection.flags & 1U) != 0U)
        {
            state_.selected_site_id = projection.site_id;
        }
        break;
    }
    case GS1_ENGINE_MESSAGE_REGIONAL_MAP_SITE_REMOVE:
    {
        if (!pending_.has_value())
        {
            break;
        }
        const auto& payload = message.payload_as<Gs1EngineMessageRegionalMapSiteData>();
        erase_projection_if(pending_->sites, [&](const auto& site) {
            return site.site_id == payload.site_id;
        });
        if (state_.selected_site_id.has_value() && state_.selected_site_id.value() == payload.site_id)
        {
            state_.selected_site_id.reset();
        }
        break;
    }
    case GS1_ENGINE_MESSAGE_REGIONAL_MAP_LINK_UPSERT:
    {
        if (!pending_.has_value())
        {
            break;
        }
        Gs1RuntimeRegionalMapLinkProjection projection = message.payload_as<Gs1EngineMessageRegionalMapLinkData>();
        upsert_projection(pending_->links, projection, regional_map_link_key(projection.from_site_id, projection.to_site_id), [](const auto& link) {
            return regional_map_link_key(link.from_site_id, link.to_site_id);
        });
        break;
    }
    case GS1_ENGINE_MESSAGE_REGIONAL_MAP_LINK_REMOVE:
    {
        if (!pending_.has_value())
        {
            break;
        }
        const auto& payload = message.payload_as<Gs1EngineMessageRegionalMapLinkData>();
        const auto key = regional_map_link_key(payload.from_site_id, payload.to_site_id);
        erase_projection_if(pending_->links, [&](const auto& link) {
            return regional_map_link_key(link.from_site_id, link.to_site_id) == key;
        });
        break;
    }
    case GS1_ENGINE_MESSAGE_END_REGIONAL_MAP_SNAPSHOT:
    {
        if (!pending_.has_value())
        {
            break;
        }
        sort_regional_map_sites(pending_->sites);
        sort_regional_map_links(pending_->links);
        state_.sites = std::move(pending_->sites);
        state_.links = std::move(pending_->links);
        pending_.reset();
        break;
    }
    default:
        break;
    }
}

void Gs1GodotSiteStateReducer::reset() noexcept
{
    site_.reset();
    pending_.reset();
    pending_inventory_storage_indices_.clear();
    pending_worker_pack_slot_indices_.clear();
    pending_opened_storage_slot_indices_.clear();
    pending_task_indices_.clear();
    pending_phone_listing_indices_.clear();
    pending_modifier_indices_.clear();
}

std::size_t Gs1GodotSiteStateReducer::site_tile_capacity(const Gs1RuntimeSiteProjection& site) const noexcept
{
    return static_cast<std::size_t>(site.width) * static_cast<std::size_t>(site.height);
}

std::optional<std::uint32_t> Gs1GodotSiteStateReducer::site_tile_index(
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

void Gs1GodotSiteStateReducer::apply_engine_message(const Gs1EngineMessage& message)
{
    switch (message.type)
    {
    case GS1_ENGINE_MESSAGE_SET_APP_STATE:
    {
        const auto& payload = message.payload_as<Gs1EngineMessageSetAppStateData>();
        if (payload.app_state == GS1_APP_STATE_MAIN_MENU ||
            payload.app_state == GS1_APP_STATE_REGIONAL_MAP ||
            payload.app_state == GS1_APP_STATE_CAMPAIGN_END)
        {
            site_.reset();
        }
        break;
    }
    case GS1_ENGINE_MESSAGE_BEGIN_SITE_SNAPSHOT:
    {
        const auto& payload = message.payload_as<Gs1EngineMessageSiteSnapshotData>();
        if (payload.mode == GS1_PROJECTION_MODE_DELTA && site_.has_value())
        {
            pending_ = site_;
        }
        else
        {
            pending_ = Gs1RuntimeSiteProjection {};
        }
        pending_->site_id = payload.site_id;
        pending_->site_archetype_id = payload.site_archetype_id;
        pending_->width = payload.width;
        pending_->height = payload.height;
        pending_inventory_storage_indices_.clear();
        pending_worker_pack_slot_indices_.clear();
        pending_opened_storage_slot_indices_.clear();
        pending_task_indices_.clear();
        pending_phone_listing_indices_.clear();
        pending_modifier_indices_.clear();
        if (payload.mode == GS1_PROJECTION_MODE_SNAPSHOT)
        {
            pending_->tiles.assign(site_tile_capacity(*pending_), Gs1RuntimeTileProjection {});
            for (Gs1RuntimeTileProjection& tile : pending_->tiles)
            {
                tile.x = k_invalid_tile_coordinate;
                tile.y = k_invalid_tile_coordinate;
            }
            pending_->inventory_storages.clear();
            pending_->worker_pack_slots.clear();
            pending_->tasks.clear();
            pending_->active_modifiers.clear();
            pending_->phone_listings.clear();
            pending_->worker_pack_open = false;
            pending_->phone_panel = Gs1RuntimePhonePanelProjection {};
            pending_->protection_overlay = Gs1RuntimeProtectionOverlayProjection {};
            pending_->opened_storage.reset();
            pending_->craft_context.reset();
            pending_->placement_preview.reset();
            pending_->placement_preview_tiles.clear();
            pending_->placement_failure.reset();
            pending_->worker.reset();
            pending_->camp.reset();
            pending_->weather.reset();
        }
        else if (pending_->tiles.size() != site_tile_capacity(*pending_))
        {
            pending_->tiles.assign(site_tile_capacity(*pending_), Gs1RuntimeTileProjection {});
            for (Gs1RuntimeTileProjection& tile : pending_->tiles)
            {
                tile.x = k_invalid_tile_coordinate;
                tile.y = k_invalid_tile_coordinate;
            }
        }
        break;
    }
    case GS1_ENGINE_MESSAGE_SITE_TILE_UPSERT:
    {
        if (!pending_.has_value())
        {
            break;
        }
        Gs1RuntimeTileProjection projection = message.payload_as<Gs1EngineMessageSiteTileData>();
        const auto tile_index = site_tile_index(*pending_, projection.x, projection.y);
        if (tile_index.has_value())
        {
            pending_->tiles[*tile_index] = projection;
        }
        break;
    }
    case GS1_ENGINE_MESSAGE_SITE_WORKER_UPDATE:
        if (pending_.has_value()) pending_->worker = message.payload_as<Gs1EngineMessageWorkerData>();
        break;
    case GS1_ENGINE_MESSAGE_SITE_CAMP_UPDATE:
        if (pending_.has_value()) pending_->camp = message.payload_as<Gs1EngineMessageCampData>();
        break;
    case GS1_ENGINE_MESSAGE_SITE_WEATHER_UPDATE:
        if (pending_.has_value()) pending_->weather = message.payload_as<Gs1EngineMessageWeatherData>();
        break;
    case GS1_ENGINE_MESSAGE_SITE_INVENTORY_STORAGE_UPSERT:
    {
        if (!pending_.has_value())
        {
            break;
        }
        Gs1RuntimeInventoryStorageProjection projection = message.payload_as<Gs1EngineMessageInventoryStorageData>();
        const auto found = pending_inventory_storage_indices_.find(projection.storage_id);
        if (found != pending_inventory_storage_indices_.end() && found->second < pending_->inventory_storages.size())
        {
            pending_->inventory_storages[found->second] = projection;
        }
        else
        {
            pending_inventory_storage_indices_[projection.storage_id] = pending_->inventory_storages.size();
            pending_->inventory_storages.push_back(projection);
        }
        break;
    }
    case GS1_ENGINE_MESSAGE_SITE_INVENTORY_SLOT_UPSERT:
    {
        if (!pending_.has_value())
        {
            break;
        }
        Gs1RuntimeInventorySlotProjection projection = message.payload_as<Gs1EngineMessageInventorySlotData>();
        std::vector<Gs1RuntimeInventorySlotProjection>* slots = nullptr;
        if (projection.container_kind == GS1_INVENTORY_CONTAINER_WORKER_PACK)
        {
            slots = &pending_->worker_pack_slots;
        }
        else if (pending_->opened_storage.has_value() && pending_->opened_storage->storage_id == projection.storage_id)
        {
            slots = &pending_->opened_storage->slots;
        }
        if (slots == nullptr)
        {
            break;
        }
        const std::uint64_t slot_key = (static_cast<std::uint64_t>(projection.storage_id) << 32U) | projection.slot_index;
        auto& slot_indices = slots == &pending_->worker_pack_slots ? pending_worker_pack_slot_indices_ : pending_opened_storage_slot_indices_;
        const auto found = slot_indices.find(slot_key);
        if (found != slot_indices.end() && found->second < slots->size())
        {
            (*slots)[found->second] = projection;
        }
        else
        {
            slot_indices[slot_key] = slots->size();
            slots->push_back(projection);
        }
        break;
    }
    case GS1_ENGINE_MESSAGE_SITE_INVENTORY_VIEW_STATE:
    {
        if (!pending_.has_value())
        {
            break;
        }
        const auto& payload = message.payload_as<Gs1EngineMessageInventoryViewData>();
        const auto storage = std::find_if(
            pending_->inventory_storages.begin(),
            pending_->inventory_storages.end(),
            [&](const auto& projection) { return projection.storage_id == payload.storage_id; });
        const bool is_worker_pack_storage =
            storage != pending_->inventory_storages.end() &&
            storage->container_kind == GS1_INVENTORY_CONTAINER_WORKER_PACK;
        if (is_worker_pack_storage)
        {
            pending_->worker_pack_open = payload.event_kind == GS1_INVENTORY_VIEW_EVENT_OPEN_SNAPSHOT;
            break;
        }
        if (payload.event_kind == GS1_INVENTORY_VIEW_EVENT_CLOSE)
        {
            if (pending_->opened_storage.has_value() && pending_->opened_storage->storage_id == payload.storage_id)
            {
                pending_->opened_storage.reset();
            }
            break;
        }
        if (payload.event_kind == GS1_INVENTORY_VIEW_EVENT_OPEN_SNAPSHOT)
        {
            pending_->opened_storage = Gs1RuntimeInventoryViewProjection {};
            pending_->opened_storage->storage_id = payload.storage_id;
            pending_->opened_storage->slot_count = payload.slot_count;
            pending_->opened_storage->slots.clear();
            pending_opened_storage_slot_indices_.clear();
        }
        break;
    }
    case GS1_ENGINE_MESSAGE_SITE_CRAFT_CONTEXT_BEGIN:
    {
        if (!pending_.has_value())
        {
            break;
        }
        const auto& payload = message.payload_as<Gs1EngineMessageCraftContextData>();
        pending_->craft_context = Gs1RuntimeCraftContextProjection {};
        pending_->craft_context->tile_x = payload.tile_x;
        pending_->craft_context->tile_y = payload.tile_y;
        pending_->craft_context->flags = payload.flags;
        pending_->craft_context->options.clear();
        pending_->craft_context->options.reserve(payload.option_count);
        break;
    }
    case GS1_ENGINE_MESSAGE_SITE_CRAFT_CONTEXT_OPTION_UPSERT:
        if (pending_.has_value() && pending_->craft_context.has_value()) pending_->craft_context->options.push_back(message.payload_as<Gs1EngineMessageCraftContextOptionData>());
        break;
    case GS1_ENGINE_MESSAGE_SITE_CRAFT_CONTEXT_END:
        if (pending_.has_value() && pending_->craft_context.has_value()) sort_craft_options(pending_->craft_context->options);
        break;
    case GS1_ENGINE_MESSAGE_SITE_PLACEMENT_PREVIEW:
    {
        if (!pending_.has_value())
        {
            break;
        }
        const auto& payload = message.payload_as<Gs1EngineMessagePlacementPreviewData>();
        if ((payload.flags & 1U) == 0U)
        {
            pending_->placement_preview.reset();
            pending_->placement_preview_tiles.clear();
        }
        else
        {
            pending_->placement_preview = payload;
            pending_->placement_preview_tiles.clear();
        }
        break;
    }
    case GS1_ENGINE_MESSAGE_SITE_PLACEMENT_PREVIEW_TILE_UPSERT:
    {
        if (!pending_.has_value())
        {
            break;
        }
        Gs1RuntimePlacementPreviewTileProjection projection = message.payload_as<Gs1EngineMessagePlacementPreviewTileData>();
        upsert_projection(
            pending_->placement_preview_tiles,
            projection,
            (static_cast<std::uint64_t>(static_cast<std::uint16_t>(projection.x)) << 16U) | static_cast<std::uint16_t>(projection.y),
            [](const auto& tile) {
                return (static_cast<std::uint64_t>(static_cast<std::uint16_t>(tile.x)) << 16U) | static_cast<std::uint16_t>(tile.y);
            });
        break;
    }
    case GS1_ENGINE_MESSAGE_SITE_PLACEMENT_FAILURE:
    {
        auto* site = pending_.has_value() ? &pending_.value() : (site_.has_value() ? &site_.value() : nullptr);
        if (site != nullptr)
        {
            site->placement_failure = message.payload_as<Gs1EngineMessagePlacementFailureData>();
        }
        break;
    }
    case GS1_ENGINE_MESSAGE_SITE_TASK_UPSERT:
    {
        if (!pending_.has_value())
        {
            break;
        }
        Gs1RuntimeTaskProjection projection = message.payload_as<Gs1EngineMessageTaskData>();
        const auto found = pending_task_indices_.find(projection.task_instance_id);
        if (found != pending_task_indices_.end() && found->second < pending_->tasks.size())
        {
            pending_->tasks[found->second] = projection;
        }
        else
        {
            pending_task_indices_[projection.task_instance_id] = pending_->tasks.size();
            pending_->tasks.push_back(projection);
        }
        break;
    }
    case GS1_ENGINE_MESSAGE_SITE_TASK_REMOVE:
    {
        if (!pending_.has_value())
        {
            break;
        }
        const auto& payload = message.payload_as<Gs1EngineMessageTaskData>();
        const auto found = pending_task_indices_.find(payload.task_instance_id);
        if (found == pending_task_indices_.end() || found->second >= pending_->tasks.size())
        {
            break;
        }
        const std::size_t index = found->second;
        const std::size_t last_index = pending_->tasks.size() - 1U;
        if (index != last_index)
        {
            pending_->tasks[index] = std::move(pending_->tasks[last_index]);
            pending_task_indices_[pending_->tasks[index].task_instance_id] = index;
        }
        pending_->tasks.pop_back();
        pending_task_indices_.erase(found);
        break;
    }
    case GS1_ENGINE_MESSAGE_SITE_PHONE_LISTING_UPSERT:
    {
        if (!pending_.has_value())
        {
            break;
        }
        Gs1RuntimePhoneListingProjection projection = message.payload_as<Gs1EngineMessagePhoneListingData>();
        const auto found = pending_phone_listing_indices_.find(projection.listing_id);
        if (found != pending_phone_listing_indices_.end() && found->second < pending_->phone_listings.size())
        {
            pending_->phone_listings[found->second] = projection;
        }
        else
        {
            pending_phone_listing_indices_[projection.listing_id] = pending_->phone_listings.size();
            pending_->phone_listings.push_back(projection);
        }
        break;
    }
    case GS1_ENGINE_MESSAGE_SITE_PHONE_LISTING_REMOVE:
    {
        if (!pending_.has_value())
        {
            break;
        }
        const auto& payload = message.payload_as<Gs1EngineMessagePhoneListingData>();
        const auto found = pending_phone_listing_indices_.find(payload.listing_id);
        if (found == pending_phone_listing_indices_.end() || found->second >= pending_->phone_listings.size())
        {
            break;
        }
        const std::size_t index = found->second;
        const std::size_t last_index = pending_->phone_listings.size() - 1U;
        if (index != last_index)
        {
            pending_->phone_listings[index] = std::move(pending_->phone_listings[last_index]);
            pending_phone_listing_indices_[pending_->phone_listings[index].listing_id] = index;
        }
        pending_->phone_listings.pop_back();
        pending_phone_listing_indices_.erase(found);
        break;
    }
    case GS1_ENGINE_MESSAGE_SITE_PHONE_PANEL_STATE:
        if (pending_.has_value()) pending_->phone_panel = message.payload_as<Gs1EngineMessagePhonePanelData>();
        break;
    case GS1_ENGINE_MESSAGE_SITE_MODIFIER_LIST_BEGIN:
    {
        if (!pending_.has_value())
        {
            break;
        }
        const auto& payload = message.payload_as<Gs1EngineMessageSiteModifierListData>();
        if (payload.mode == GS1_PROJECTION_MODE_SNAPSHOT)
        {
            pending_->active_modifiers.clear();
        }
        pending_->active_modifiers.reserve(payload.modifier_count);
        break;
    }
    case GS1_ENGINE_MESSAGE_SITE_MODIFIER_UPSERT:
    {
        if (!pending_.has_value())
        {
            break;
        }
        Gs1RuntimeModifierProjection projection = message.payload_as<Gs1EngineMessageSiteModifierData>();
        const auto found = pending_modifier_indices_.find(projection.modifier_id);
        if (found != pending_modifier_indices_.end() && found->second < pending_->active_modifiers.size())
        {
            pending_->active_modifiers[found->second] = projection;
        }
        else
        {
            pending_modifier_indices_[projection.modifier_id] = pending_->active_modifiers.size();
            pending_->active_modifiers.push_back(projection);
        }
        break;
    }
    case GS1_ENGINE_MESSAGE_SITE_PROTECTION_OVERLAY_STATE:
    {
        auto* site = pending_.has_value() ? &pending_.value() : (site_.has_value() ? &site_.value() : nullptr);
        if (site != nullptr)
        {
            site->protection_overlay = message.payload_as<Gs1EngineMessageSiteProtectionOverlayData>();
        }
        break;
    }
    case GS1_ENGINE_MESSAGE_END_SITE_SNAPSHOT:
    {
        if (!pending_.has_value())
        {
            break;
        }
        sort_inventory_storages(pending_->inventory_storages);
        sort_inventory_slots(pending_->worker_pack_slots);
        if (pending_->opened_storage.has_value())
        {
            sort_inventory_slots(pending_->opened_storage->slots);
        }
        sort_tasks(pending_->tasks);
        sort_modifiers(pending_->active_modifiers);
        sort_phone_listings(pending_->phone_listings);
        sort_placement_preview_tiles(pending_->placement_preview_tiles);
        site_ = std::move(pending_);
        pending_.reset();
        pending_inventory_storage_indices_.clear();
        pending_worker_pack_slot_indices_.clear();
        pending_opened_storage_slot_indices_.clear();
        pending_task_indices_.clear();
        pending_phone_listing_indices_.clear();
        pending_modifier_indices_.clear();
        break;
    }
    default:
        break;
    }
}
