#include "gs1_godot_regional_selection_panel_controller.h"

#include "godot_progression_resources.h"
#include "gs1_godot_controller_context.h"
#include "content/defs/faction_defs.h"
#include "content/defs/item_defs.h"

#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/core/object.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

using namespace godot;

namespace
{
constexpr int k_ui_action_start_site_attempt = 3;
constexpr int k_ui_action_clear_deployment_site_selection = 5;
constexpr std::uint8_t k_content_resource_kind_item = 1U;

constexpr std::uint64_t pack_u32_pair(std::uint32_t high, std::uint32_t low) noexcept
{
    return (static_cast<std::uint64_t>(high) << 32U) | static_cast<std::uint64_t>(low);
}

std::uint64_t make_projected_button_key(int setup_id, int element_id) noexcept
{
    return pack_u32_pair(static_cast<std::uint32_t>(setup_id), static_cast<std::uint32_t>(element_id));
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

template <typename T>
T* resolve_object(const ObjectID object_id)
{
    return Object::cast_to<T>(ObjectDB::get_instance(object_id));
}

String string_from_view(std::string_view value)
{
    return String::utf8(value.data(), static_cast<int>(value.size()));
}

Ref<Texture2D> load_texture_2d(const String& path)
{
    if (path.is_empty())
    {
        return Ref<Texture2D> {};
    }

    if (ResourceLoader* loader = ResourceLoader::get_singleton())
    {
        return loader->load(path);
    }
    return Ref<Texture2D> {};
}

int as_int(const Variant& value, int fallback = 0)
{
    if (value.get_type() == Variant::NIL)
    {
        return fallback;
    }
    switch (value.get_type())
    {
    case Variant::INT:
        return static_cast<int>(int64_t(value));
    case Variant::FLOAT:
        return static_cast<int>(Math::round(double(value)));
    case Variant::BOOL:
        return bool(value) ? 1 : 0;
    default:
        return fallback;
    }
}

Gs1GodotRegionalSelectionPanelController* resolve_controller(std::int64_t controller_bits)
{
    return reinterpret_cast<Gs1GodotRegionalSelectionPanelController*>(static_cast<std::uintptr_t>(controller_bits));
}

void dispatch_selection_action_pressed(std::int64_t controller_bits, std::int64_t button_key)
{
    if (Gs1GodotRegionalSelectionPanelController* controller = resolve_controller(controller_bits))
    {
        controller->handle_action_pressed(button_key);
    }
}
}

void Gs1GodotRegionalSelectionPanelController::_bind_methods()
{
}

void Gs1GodotRegionalSelectionPanelController::_ready()
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

void Gs1GodotRegionalSelectionPanelController::_exit_tree()
{
    if (adapter_service_ != nullptr)
    {
        adapter_service_->unsubscribe_all(*this);
        adapter_service_ = nullptr;
    }
}

void Gs1GodotRegionalSelectionPanelController::cache_ui_references(Control& owner)
{
    owner_control_ = &owner;
    if (panel_ == nullptr)
    {
        panel_ = Object::cast_to<Control>(owner.find_child("RegionalSelectionPanel", true, false));
    }
    if (title_ == nullptr)
    {
        title_ = Object::cast_to<Label>(owner.find_child("RegionalSelectionTitle", true, false));
    }
    if (summary_ == nullptr)
    {
        summary_ = Object::cast_to<RichTextLabel>(owner.find_child("RegionalSelectionSummary", true, false));
    }
    if (support_summary_ == nullptr)
    {
        support_summary_ = Object::cast_to<RichTextLabel>(owner.find_child("RegionalSupportSummary", true, false));
    }
    if (loadout_title_ == nullptr)
    {
        loadout_title_ = Object::cast_to<Label>(owner.find_child("RegionalLoadoutTitle", true, false));
    }
    if (loadout_slots_grid_ == nullptr)
    {
        loadout_slots_grid_ = Object::cast_to<GridContainer>(owner.find_child("RegionalLoadoutSlots", true, false));
    }
    if (loadout_empty_label_ == nullptr)
    {
        loadout_empty_label_ = Object::cast_to<Label>(owner.find_child("RegionalLoadoutEmpty", true, false));
    }
    if (actions_ == nullptr)
    {
        actions_ = Object::cast_to<VBoxContainer>(owner.find_child("RegionalSelectionActions", true, false));
    }
    rebuild_selection_panel();
}

void Gs1GodotRegionalSelectionPanelController::set_submit_ui_action_callback(SubmitUiActionFn callback)
{
    submit_ui_action_ = std::move(callback);
}

void Gs1GodotRegionalSelectionPanelController::cache_adapter_service()
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

Control* Gs1GodotRegionalSelectionPanelController::resolve_owner_control()
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

void Gs1GodotRegionalSelectionPanelController::submit_ui_action(
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

void Gs1GodotRegionalSelectionPanelController::reset_ui_panel_state() noexcept
{
    ui_panels_.clear();
    pending_ui_panel_.reset();
    ui_panel_indices_.clear();
    pending_ui_panel_text_line_indices_.clear();
    pending_ui_panel_slot_action_indices_.clear();
    pending_ui_panel_list_item_indices_.clear();
    pending_ui_panel_list_action_indices_.clear();
}

void Gs1GodotRegionalSelectionPanelController::rebuild_ui_panel_indices() noexcept
{
    ui_panel_indices_.clear();
    ui_panel_indices_.reserve(ui_panels_.size());
    for (std::size_t index = 0; index < ui_panels_.size(); ++index)
    {
        ui_panel_indices_[static_cast<std::uint16_t>(ui_panels_[index].panel_id)] = index;
    }
}

const Gs1RuntimeUiPanelProjection* Gs1GodotRegionalSelectionPanelController::find_ui_panel(Gs1UiPanelId panel_id) const noexcept
{
    const auto found = ui_panel_indices_.find(static_cast<std::uint16_t>(panel_id));
    if (found == ui_panel_indices_.end() || found->second >= ui_panels_.size())
    {
        return nullptr;
    }
    return &ui_panels_[found->second];
}

void Gs1GodotRegionalSelectionPanelController::apply_ui_panel_message(const Gs1EngineMessage& message)
{
    const Gs1EngineMessageType type = message.type;
    if (type == GS1_ENGINE_MESSAGE_BEGIN_REGIONAL_SELECTION_UI_PANEL)
    {
        const auto& payload = message.payload_as<Gs1EngineMessageUiPanelData>();
        pending_ui_panel_text_line_indices_.clear();
        pending_ui_panel_slot_action_indices_.clear();
        pending_ui_panel_list_item_indices_.clear();
        pending_ui_panel_list_action_indices_.clear();
        if (payload.mode == GS1_PROJECTION_MODE_DELTA)
        {
            const auto found = ui_panel_indices_.find(static_cast<std::uint16_t>(payload.panel_id));
            if (found != ui_panel_indices_.end() && found->second < ui_panels_.size())
            {
                const auto& existing = ui_panels_[found->second];
                pending_ui_panel_ = PendingUiPanel {};
                pending_ui_panel_->panel_id = existing.panel_id;
                pending_ui_panel_->context_id = existing.context_id;
                pending_ui_panel_->text_lines = existing.text_lines;
                pending_ui_panel_->slot_actions = existing.slot_actions;
                pending_ui_panel_->list_items = existing.list_items;
                pending_ui_panel_->list_actions = existing.list_actions;
                for (std::size_t index = 0; index < pending_ui_panel_->text_lines.size(); ++index)
                {
                    pending_ui_panel_text_line_indices_[pending_ui_panel_->text_lines[index].line_id] = index;
                }
                for (std::size_t index = 0; index < pending_ui_panel_->slot_actions.size(); ++index)
                {
                    pending_ui_panel_slot_action_indices_[static_cast<std::uint16_t>(pending_ui_panel_->slot_actions[index].slot_id)] = index;
                }
                for (std::size_t index = 0; index < pending_ui_panel_->list_items.size(); ++index)
                {
                    pending_ui_panel_list_item_indices_[ui_panel_list_item_key(
                        pending_ui_panel_->list_items[index].list_id,
                        pending_ui_panel_->list_items[index].item_id)] = index;
                }
                for (std::size_t index = 0; index < pending_ui_panel_->list_actions.size(); ++index)
                {
                    pending_ui_panel_list_action_indices_[ui_panel_list_action_key(
                        pending_ui_panel_->list_actions[index].list_id,
                        pending_ui_panel_->list_actions[index].item_id,
                        pending_ui_panel_->list_actions[index].role)] = index;
                }
            }
            else
            {
                pending_ui_panel_ = PendingUiPanel {};
            }
        }
        else
        {
            pending_ui_panel_ = PendingUiPanel {};
        }

        pending_ui_panel_->panel_id = payload.panel_id;
        pending_ui_panel_->context_id = payload.context_id;
        if (payload.mode == GS1_PROJECTION_MODE_SNAPSHOT)
        {
            pending_ui_panel_->text_lines.clear();
            pending_ui_panel_->slot_actions.clear();
            pending_ui_panel_->list_items.clear();
            pending_ui_panel_->list_actions.clear();
        }
    }
    else if (type == GS1_ENGINE_MESSAGE_REGIONAL_SELECTION_UI_PANEL_TEXT_UPSERT)
    {
        if (!pending_ui_panel_.has_value())
        {
            return;
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
        const auto found = pending_ui_panel_text_line_indices_.find(projection.line_id);
        if (found != pending_ui_panel_text_line_indices_.end() && found->second < pending_ui_panel_->text_lines.size())
        {
            pending_ui_panel_->text_lines[found->second] = projection;
        }
        else
        {
            pending_ui_panel_text_line_indices_[projection.line_id] = pending_ui_panel_->text_lines.size();
            pending_ui_panel_->text_lines.push_back(std::move(projection));
        }
    }
    else if (type == GS1_ENGINE_MESSAGE_REGIONAL_SELECTION_UI_PANEL_SLOT_ACTION_UPSERT)
    {
        if (!pending_ui_panel_.has_value())
        {
            return;
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
        const auto found = pending_ui_panel_slot_action_indices_.find(key);
        if (found != pending_ui_panel_slot_action_indices_.end() && found->second < pending_ui_panel_->slot_actions.size())
        {
            pending_ui_panel_->slot_actions[found->second] = projection;
        }
        else
        {
            pending_ui_panel_slot_action_indices_[key] = pending_ui_panel_->slot_actions.size();
            pending_ui_panel_->slot_actions.push_back(std::move(projection));
        }
    }
    else if (type == GS1_ENGINE_MESSAGE_REGIONAL_SELECTION_UI_PANEL_LIST_ITEM_UPSERT)
    {
        if (!pending_ui_panel_.has_value())
        {
            return;
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
        const auto found = pending_ui_panel_list_item_indices_.find(key);
        if (found != pending_ui_panel_list_item_indices_.end() && found->second < pending_ui_panel_->list_items.size())
        {
            pending_ui_panel_->list_items[found->second] = projection;
        }
        else
        {
            pending_ui_panel_list_item_indices_[key] = pending_ui_panel_->list_items.size();
            pending_ui_panel_->list_items.push_back(std::move(projection));
        }
    }
    else if (type == GS1_ENGINE_MESSAGE_REGIONAL_SELECTION_UI_PANEL_LIST_ACTION_UPSERT)
    {
        if (!pending_ui_panel_.has_value())
        {
            return;
        }
        const auto& payload = message.payload_as<Gs1EngineMessageUiPanelListActionData>();
        Gs1RuntimeUiPanelListActionProjection projection {};
        projection.item_id = payload.item_id;
        projection.list_id = payload.list_id;
        projection.role = payload.role;
        projection.flags = payload.flags;
        projection.action = payload.action;
        const auto key = ui_panel_list_action_key(projection.list_id, projection.item_id, projection.role);
        const auto found = pending_ui_panel_list_action_indices_.find(key);
        if (found != pending_ui_panel_list_action_indices_.end() && found->second < pending_ui_panel_->list_actions.size())
        {
            pending_ui_panel_->list_actions[found->second] = projection;
        }
        else
        {
            pending_ui_panel_list_action_indices_[key] = pending_ui_panel_->list_actions.size();
            pending_ui_panel_->list_actions.push_back(std::move(projection));
        }
    }
    else if (type == GS1_ENGINE_MESSAGE_END_REGIONAL_SELECTION_UI_PANEL)
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
        upsert_projection(ui_panels_, std::move(panel), pending_ui_panel_->panel_id, [](const auto& existing) {
            return existing.panel_id;
        });
        sort_ui_panels(ui_panels_);
        rebuild_ui_panel_indices();
        pending_ui_panel_.reset();
        pending_ui_panel_text_line_indices_.clear();
        pending_ui_panel_slot_action_indices_.clear();
        pending_ui_panel_list_item_indices_.clear();
        pending_ui_panel_list_action_indices_.clear();
    }
    else if (type == GS1_ENGINE_MESSAGE_CLOSE_REGIONAL_SELECTION_UI_PANEL)
    {
        const auto& payload = message.payload_as<Gs1EngineMessageCloseUiPanelData>();
        erase_projection_if(ui_panels_, [&](const auto& panel) {
            return panel.panel_id == payload.panel_id;
        });
        rebuild_ui_panel_indices();
    }
}

void Gs1GodotRegionalSelectionPanelController::reset_regional_map_state() noexcept
{
    selected_site_projection_id_.reset();
    sites_.clear();
    links_.clear();
    pending_regional_map_state_.reset();
}

void Gs1GodotRegionalSelectionPanelController::apply_regional_map_message(const Gs1EngineMessage& message)
{
    if (message.type == GS1_ENGINE_MESSAGE_SET_APP_STATE)
    {
        const auto& payload = message.payload_as<Gs1EngineMessageSetAppStateData>();
        if (payload.app_state == GS1_APP_STATE_MAIN_MENU)
        {
            selected_site_projection_id_.reset();
            sites_.clear();
            links_.clear();
        }
    }
    else if (message.type == GS1_ENGINE_MESSAGE_BEGIN_REGIONAL_SELECTION_SNAPSHOT)
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
            selected_site_projection_id_ = payload.selected_site_id;
        }
        else
        {
            selected_site_projection_id_.reset();
        }
    }
    else if (message.type == GS1_ENGINE_MESSAGE_REGIONAL_SELECTION_SITE_UPSERT)
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
            selected_site_projection_id_ = projection.site_id;
        }
    }
    else if (message.type == GS1_ENGINE_MESSAGE_REGIONAL_SELECTION_SITE_REMOVE)
    {
        if (!pending_regional_map_state_.has_value())
        {
            return;
        }
        const auto& payload = message.payload_as<Gs1EngineMessageRegionalMapSiteData>();
        erase_projection_if(pending_regional_map_state_->sites, [&](const auto& site) {
            return site.site_id == payload.site_id;
        });
        if (selected_site_projection_id_.has_value() && selected_site_projection_id_.value() == payload.site_id)
        {
            selected_site_projection_id_.reset();
        }
    }
    else if (message.type == GS1_ENGINE_MESSAGE_REGIONAL_SELECTION_LINK_UPSERT)
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
    else if (message.type == GS1_ENGINE_MESSAGE_REGIONAL_SELECTION_LINK_REMOVE)
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
    else if (message.type == GS1_ENGINE_MESSAGE_END_REGIONAL_SELECTION_SNAPSHOT)
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

bool Gs1GodotRegionalSelectionPanelController::handles_engine_message(Gs1EngineMessageType type) const noexcept
{
    switch (type)
    {
    case GS1_ENGINE_MESSAGE_BEGIN_REGIONAL_SELECTION_SNAPSHOT:
    case GS1_ENGINE_MESSAGE_REGIONAL_SELECTION_SITE_UPSERT:
    case GS1_ENGINE_MESSAGE_REGIONAL_SELECTION_SITE_REMOVE:
    case GS1_ENGINE_MESSAGE_REGIONAL_SELECTION_LINK_UPSERT:
    case GS1_ENGINE_MESSAGE_REGIONAL_SELECTION_LINK_REMOVE:
    case GS1_ENGINE_MESSAGE_END_REGIONAL_SELECTION_SNAPSHOT:
    case GS1_ENGINE_MESSAGE_BEGIN_REGIONAL_SELECTION_UI_PANEL:
    case GS1_ENGINE_MESSAGE_REGIONAL_SELECTION_UI_PANEL_TEXT_UPSERT:
    case GS1_ENGINE_MESSAGE_REGIONAL_SELECTION_UI_PANEL_SLOT_ACTION_UPSERT:
    case GS1_ENGINE_MESSAGE_REGIONAL_SELECTION_UI_PANEL_LIST_ITEM_UPSERT:
    case GS1_ENGINE_MESSAGE_REGIONAL_SELECTION_UI_PANEL_LIST_ACTION_UPSERT:
    case GS1_ENGINE_MESSAGE_END_REGIONAL_SELECTION_UI_PANEL:
    case GS1_ENGINE_MESSAGE_CLOSE_REGIONAL_SELECTION_UI_PANEL:
        return true;
    default:
        return false;
    }
}

void Gs1GodotRegionalSelectionPanelController::handle_engine_message(const Gs1EngineMessage& message)
{
    apply_ui_panel_message(message);
    apply_regional_map_message(message);

    switch (message.type)
    {
    case GS1_ENGINE_MESSAGE_END_REGIONAL_SELECTION_SNAPSHOT:
    {
        if (selected_site_projection_id_.has_value())
        {
            selected_site_id_ = static_cast<int>(selected_site_projection_id_.value());
        }
        else if (sites_.empty())
        {
            selected_site_id_ = 0;
        }

        const Gs1RuntimeRegionalMapSiteProjection* selected_site = resolve_selected_site();
        apply_panel_visibility(selected_site);
        if (selected_site == nullptr)
        {
            refresh_selection_panel_actions_and_summaries(nullptr);
            break;
        }

        apply_title_and_summary(selected_site);
        if (find_ui_panel(GS1_UI_PANEL_REGIONAL_MAP_SELECTION) == nullptr)
        {
            refresh_selection_panel_actions_and_summaries(selected_site);
        }
        break;
    }
    case GS1_ENGINE_MESSAGE_END_REGIONAL_SELECTION_UI_PANEL:
    case GS1_ENGINE_MESSAGE_CLOSE_REGIONAL_SELECTION_UI_PANEL:
        refresh_selection_panel_actions_and_summaries(resolve_selected_site());
        break;
    default:
        break;
    }
}

void Gs1GodotRegionalSelectionPanelController::handle_runtime_message_reset()
{
    reset_ui_panel_state();
    reset_regional_map_state();
    selected_site_id_ = 0;
    prune_loadout_slot_registry({});
    rebuild_selection_panel();
}

const Gs1RuntimeRegionalMapSiteProjection* Gs1GodotRegionalSelectionPanelController::resolve_selected_site()
{
    const Gs1RuntimeRegionalMapSiteProjection* selected_site = nullptr;
    for (const auto& site : sites_)
    {
        if (static_cast<int>(site.site_id) == selected_site_id_)
        {
            selected_site = &site;
            break;
        }
    }

    if (selected_site == nullptr && !sites_.empty())
    {
        selected_site = &sites_.front();
        selected_site_id_ = static_cast<int>(selected_site->site_id);
    }

    return selected_site;
}

void Gs1GodotRegionalSelectionPanelController::apply_panel_visibility(
    const Gs1RuntimeRegionalMapSiteProjection* selected_site)
{
    if (panel_ != nullptr)
    {
        panel_->set_visible(selected_site != nullptr);
    }
}

void Gs1GodotRegionalSelectionPanelController::apply_title_and_summary(
    const Gs1RuntimeRegionalMapSiteProjection* selected_site)
{
    if (selected_site == nullptr)
    {
        return;
    }

    if (title_ != nullptr)
    {
        title_->set_text(vformat("Selected Site %d", static_cast<int>(selected_site->site_id)));
    }

    PackedStringArray summary_lines;
    summary_lines.push_back(vformat("[b]%s[/b]", site_button_text(*selected_site)));
    summary_lines.push_back(site_tooltip(*selected_site));
    summary_lines.push_back(String());
    summary_lines.push_back("[b]Deployment Brief[/b]");
    summary_lines.push_back(site_deployment_summary(*selected_site));
    if (summary_ != nullptr)
    {
        summary_->set_text(String("\n").join(summary_lines));
    }
}

void Gs1GodotRegionalSelectionPanelController::refresh_selection_panel_actions_and_summaries(
    const Gs1RuntimeRegionalMapSiteProjection* selected_site)
{
    Array button_specs;
    const Gs1RuntimeUiPanelProjection* selection_panel = selected_site == nullptr
        ? nullptr
        : find_ui_panel(GS1_UI_PANEL_REGIONAL_MAP_SELECTION);
    if (selection_panel != nullptr)
    {
        for (const auto& slot_action : selection_panel->slot_actions)
        {
            if (actions_ == nullptr)
            {
                continue;
            }

            Dictionary action;
            action["type"] = static_cast<int>(slot_action.action.type);
            action["target_id"] = static_cast<int64_t>(slot_action.action.target_id);
            action["arg0"] = static_cast<int64_t>(slot_action.action.arg0);
            action["arg1"] = static_cast<int64_t>(slot_action.action.arg1);
            const int action_type = static_cast<int>(slot_action.action.type);
            if (action_type == k_ui_action_clear_deployment_site_selection)
            {
                continue;
            }
            const String text = panel_slot_label(slot_action);
            if (text.is_empty())
            {
                continue;
            }

            Dictionary spec;
            spec["setup_id"] = static_cast<int>(selection_panel->panel_id);
            spec["element_id"] = static_cast<int>(slot_action.slot_id);
            spec["text"] = selection_action_label(text, action);
            spec["flags"] = static_cast<int>(slot_action.flags);
            spec["action"] = action;
            button_specs.push_back(spec);
        }
    }

    if (selected_site != nullptr && button_specs.is_empty())
    {
        const int selected_site_id = static_cast<int>(selected_site->site_id);

        Dictionary deploy_action;
        deploy_action["type"] = k_ui_action_start_site_attempt;
        deploy_action["target_id"] = selected_site_id;
        deploy_action["arg0"] = 0;
        deploy_action["arg1"] = 0;

        Dictionary deploy_spec;
        deploy_spec["setup_id"] = -1;
        deploy_spec["element_id"] = k_ui_action_start_site_attempt;
        deploy_spec["text"] = vformat("Deploy To Site %d", selected_site_id);
        deploy_spec["flags"] = selected_site->site_state == GS1_SITE_STATE_LOCKED ? 2 : 0;
        deploy_spec["action"] = deploy_action;
        button_specs.push_back(deploy_spec);
    }

    reconcile_action_buttons(button_specs);
    refresh_support_summary(selection_panel);
    refresh_loadout_panel(selection_panel);
}

void Gs1GodotRegionalSelectionPanelController::rebuild_selection_panel()
{
    if (panel_ == nullptr)
    {
        return;
    }

    const Gs1RuntimeRegionalMapSiteProjection* selected_site = resolve_selected_site();
    apply_panel_visibility(selected_site);
    if (selected_site == nullptr)
    {
        refresh_selection_panel_actions_and_summaries(nullptr);
        return;
    }

    apply_title_and_summary(selected_site);
    refresh_selection_panel_actions_and_summaries(selected_site);
}

void Gs1GodotRegionalSelectionPanelController::handle_action_pressed(std::int64_t button_key)
{
    auto found = action_buttons_.find(static_cast<std::uint64_t>(button_key));
    if (found == action_buttons_.end())
    {
        return;
    }

    Button* button = resolve_object<Button>(found->second.object_id);
    if (button == nullptr || !submit_ui_action_)
    {
        return;
    }

    submit_ui_action_(
        as_int(button->get_meta("action_type", 0), 0),
        as_int(button->get_meta("target_id", 0), 0),
        as_int(button->get_meta("arg0", 0), 0),
        as_int(button->get_meta("arg1", 0), 0));
}

void Gs1GodotRegionalSelectionPanelController::reconcile_action_buttons(const Array& button_specs)
{
    if (actions_ == nullptr)
    {
        return;
    }

    std::unordered_set<std::uint64_t> desired_keys;
    int desired_index = 0;
    for (int64_t index = 0; index < button_specs.size(); ++index)
    {
        const Dictionary spec = button_specs[index];
        const int setup_id = as_int(spec.get("setup_id", 0), 0);
        const int element_id = as_int(spec.get("element_id", 0), 0);
        const std::uint64_t stable_key = make_projected_button_key(setup_id, element_id);
        desired_keys.insert(stable_key);
        Button* button = upsert_button_node(
            actions_,
            action_buttons_,
            stable_key,
            vformat("Projected_%d_%d", setup_id, element_id),
            desired_index++);
        if (button == nullptr)
        {
            continue;
        }

        const Dictionary action = spec.get("action", Dictionary());
        button->set_text(String(spec.get("text", "Action")));
        button->set_tooltip_text(String());
        button->set_disabled((as_int(spec.get("flags", 0), 0) & 2) != 0);
        button->set_meta("action_type", as_int(action.get("type", 0), 0));
        button->set_meta("target_id", as_int(action.get("target_id", 0), 0));
        button->set_meta("arg0", as_int(action.get("arg0", 0), 0));
        button->set_meta("arg1", as_int(action.get("arg1", 0), 0));
        const Callable callback = callable_mp_static(&dispatch_selection_action_pressed).bind(
            static_cast<std::int64_t>(reinterpret_cast<std::uintptr_t>(this)),
            static_cast<std::int64_t>(stable_key));
        if (!button->is_connected("pressed", callback))
        {
            button->connect("pressed", callback);
        }
    }

    prune_button_registry(action_buttons_, desired_keys);
}

Button* Gs1GodotRegionalSelectionPanelController::upsert_button_node(
    Node* container,
    std::unordered_map<std::uint64_t, ProjectedButtonRecord>& registry,
    std::uint64_t stable_key,
    const String& node_name,
    int desired_index)
{
    if (container == nullptr)
    {
        return nullptr;
    }

    Button* button = nullptr;
    auto found = registry.find(stable_key);
    if (found != registry.end())
    {
        button = resolve_object<Button>(found->second.object_id);
    }

    if (button == nullptr)
    {
        button = memnew(Button);
        button->set_clip_text(true);
        button->set_custom_minimum_size(Vector2(0.0F, 44.0F));
        container->add_child(button);
        registry[stable_key].object_id = button->get_instance_id();
    }

    button->set_name(node_name);
    const int child_count = container->get_child_count();
    const int clamped_index = std::clamp(desired_index, 0, child_count - 1);
    if (button->get_index() != clamped_index)
    {
        container->move_child(button, clamped_index);
    }

    return button;
}

void Gs1GodotRegionalSelectionPanelController::prune_button_registry(
    std::unordered_map<std::uint64_t, ProjectedButtonRecord>& registry,
    const std::unordered_set<std::uint64_t>& desired_keys)
{
    for (auto it = registry.begin(); it != registry.end();)
    {
        if (desired_keys.contains(it->first))
        {
            ++it;
            continue;
        }

        if (Button* button = resolve_object<Button>(it->second.object_id))
        {
            button->queue_free();
        }
        it = registry.erase(it);
    }
}

String Gs1GodotRegionalSelectionPanelController::item_name_for(int item_id) const
{
    auto found = item_name_cache_.find(item_id);
    if (found != item_name_cache_.end())
    {
        return found->second;
    }

    String display_name = String("Item #") + String::num_int64(item_id);
    if (const auto* item_def = gs1::find_item_def(gs1::ItemId {static_cast<std::uint32_t>(item_id)}))
    {
        display_name = string_from_view(item_def->display_name);
    }

    return item_name_cache_.emplace(item_id, display_name).first->second;
}

String Gs1GodotRegionalSelectionPanelController::faction_name_for(int faction_id) const
{
    auto found = faction_name_cache_.find(faction_id);
    if (found != faction_name_cache_.end())
    {
        return found->second;
    }

    String display_name {"Faction"};
    if (const auto* faction_def = gs1::find_faction_def(gs1::FactionId {static_cast<std::uint32_t>(faction_id)}))
    {
        display_name = string_from_view(faction_def->display_name);
    }

    return faction_name_cache_.emplace(faction_id, display_name).first->second;
}

String Gs1GodotRegionalSelectionPanelController::panel_text_line(const Gs1RuntimeUiPanelTextProjection& line) const
{
    const int kind = static_cast<int>(line.text_kind);
    const int primary_id = static_cast<int>(line.primary_id);
    const int quantity = static_cast<int>(line.quantity);

    switch (kind)
    {
    case 1:
        return vformat("%s Rep %d", faction_name_for(primary_id), quantity);
    case 3:
        return vformat("Selected Site %d", primary_id);
    case 4:
        return vformat("Adj Support x%d", quantity);
    case 5:
        return vformat("Aura Ready x%d", quantity);
    case 6:
        return vformat("%s x%d", item_name_for(primary_id), quantity);
    default:
        return String();
    }
}

String Gs1GodotRegionalSelectionPanelController::panel_slot_label(const Gs1RuntimeUiPanelSlotActionProjection& slot_action) const
{
    const int kind = static_cast<int>(slot_action.label_kind);
    const int primary_id = static_cast<int>(slot_action.primary_id);
    switch (kind)
    {
    case 1:
        return "Research & Unlocks";
    case 2:
        return "Close Research";
    case 3:
        return vformat("Deploy To Site %d", primary_id);
    case 4:
        return "Clear Selection";
    default:
        return String();
    }
}

String Gs1GodotRegionalSelectionPanelController::selection_action_label(const String& text, const Dictionary& action) const
{
    const int action_type = as_int(action.get("type", 0), 0);
    if (action_type == k_ui_action_start_site_attempt)
    {
        return vformat("Deploy To Site %d", as_int(action.get("target_id", 0), 0));
    }
    if (action_type == k_ui_action_clear_deployment_site_selection)
    {
        return "Clear Selection";
    }
    return text.is_empty() ? String("Action") : text;
}

String Gs1GodotRegionalSelectionPanelController::site_button_text(const Gs1RuntimeRegionalMapSiteProjection& site) const
{
    const int site_id = static_cast<int>(site.site_id);
    const String coords = vformat("(%d, %d)", Math::round(site.map_x / 160.0), Math::round(site.map_y / 160.0));
    return vformat("Site %d  %s  %s", site_id, site_state_name(static_cast<int>(site.site_state)), coords);
}

String Gs1GodotRegionalSelectionPanelController::site_tooltip(const Gs1RuntimeRegionalMapSiteProjection& site) const
{
    PackedStringArray support_lines;
    const int preview_mask = static_cast<int>(site.support_preview_mask);
    if ((preview_mask & 1) != 0)
    {
        support_lines.push_back("items");
    }
    if ((preview_mask & 2) != 0)
    {
        support_lines.push_back("wind");
    }
    if ((preview_mask & 4) != 0)
    {
        support_lines.push_back("fertility");
    }
    if ((preview_mask & 8) != 0)
    {
        support_lines.push_back("recovery");
    }

    const String support_text = support_lines.is_empty() ? String("None") : String(", ").join(support_lines);
    return vformat(
        "State: %s  Support Package: %d  Active Preview: %s",
        site_state_name(static_cast<int>(site.site_state)),
        static_cast<int>(site.support_package_id),
        support_text);
}

String Gs1GodotRegionalSelectionPanelController::site_state_name(int site_state) const
{
    switch (site_state)
    {
    case 1:
        return "Available";
    case 2:
        return "Completed";
    default:
        return "Locked";
    }
}

String Gs1GodotRegionalSelectionPanelController::site_deployment_summary(const Gs1RuntimeRegionalMapSiteProjection& site) const
{
    const String state_name = site_state_name(static_cast<int>(site.site_state));
    const Vector2i grid = regional_grid_coord(site);
    const String support_preview = support_preview_text(static_cast<int>(site.support_preview_mask));
    const int package_id = static_cast<int>(site.support_package_id);
    return vformat(
        "Status: %s\nGrid: (%d, %d)\nNearby support: %s\nLoadout package channel: %d\nSelect this node, review the support feed, then deploy from this panel.",
        state_name,
        grid.x,
        grid.y,
        support_preview,
        package_id);
}

String Gs1GodotRegionalSelectionPanelController::support_preview_text(int preview_mask) const
{
    PackedStringArray support_lines;
    if ((preview_mask & 1) != 0)
    {
        support_lines.push_back("loadout items");
    }
    if ((preview_mask & 2) != 0)
    {
        support_lines.push_back("wind shielding");
    }
    if ((preview_mask & 4) != 0)
    {
        support_lines.push_back("fertility lift");
    }
    if ((preview_mask & 8) != 0)
    {
        support_lines.push_back("recovery support");
    }
    return support_lines.is_empty() ? String("None") : String(", ").join(support_lines);
}

Vector2i Gs1GodotRegionalSelectionPanelController::regional_grid_coord(const Gs1RuntimeRegionalMapSiteProjection& site) const
{
    return Vector2i(Math::round(site.map_x / 160.0), Math::round(site.map_y / 160.0));
}

void Gs1GodotRegionalSelectionPanelController::refresh_support_summary(
    const Gs1RuntimeUiPanelProjection* selection_panel)
{
    if (support_summary_ == nullptr)
    {
        return;
    }

    PackedStringArray lines;
    lines.push_back("[b]Support Feed[/b]");
    bool has_content = false;
    if (selection_panel != nullptr)
    {
        for (const auto& line : selection_panel->text_lines)
        {
            const int kind = static_cast<int>(line.text_kind);
            if (kind != 4 && kind != 5)
            {
                continue;
            }

            const String text = panel_text_line(line).strip_edges();
            if (text.is_empty())
            {
                continue;
            }
            lines.push_back(text);
            has_content = true;
        }
    }

    if (!has_content)
    {
        lines.push_back("No adjacent support contributors or aura boosts are projected for this route.");
    }

    support_summary_->set_text(String("\n").join(lines));
}

void Gs1GodotRegionalSelectionPanelController::refresh_loadout_panel(
    const Gs1RuntimeUiPanelProjection* selection_panel)
{
    if (loadout_title_ != nullptr)
    {
        loadout_title_->set_text("Provided Loadout");
    }

    if (loadout_slots_grid_ == nullptr)
    {
        return;
    }

    std::vector<const Gs1RuntimeUiPanelListItemProjection*> loadout_items {};
    if (selection_panel != nullptr)
    {
        for (const auto& item : selection_panel->list_items)
        {
            if (item.list_id != GS1_UI_PANEL_LIST_REGIONAL_LOADOUT)
            {
                continue;
            }
            loadout_items.push_back(&item);
        }
    }

    const bool has_content = !loadout_items.empty();
    loadout_slots_grid_->set_visible(has_content);
    if (loadout_empty_label_ != nullptr)
    {
        loadout_empty_label_->set_visible(!has_content);
        loadout_empty_label_->set_text("No extra loadout items are projected for the currently selected site.");
    }

    if (!has_content)
    {
        prune_loadout_slot_registry({});
        return;
    }

    loadout_slots_grid_->set_columns(std::clamp<int>(static_cast<int>(loadout_items.size()), 1, 4));

    std::unordered_set<std::uint64_t> desired_keys {};
    for (std::size_t index = 0; index < loadout_items.size(); ++index)
    {
        const auto& item = *loadout_items[index];
        const auto stable_key = loadout_slot_key(item.item_id);
        desired_keys.insert(stable_key);
        Button* button = upsert_loadout_slot_button(
            stable_key,
            vformat("LoadoutSlot_%d", static_cast<int>(item.item_id)),
            static_cast<int>(index));
        if (button == nullptr)
        {
            continue;
        }

        const String item_name = item_name_for(static_cast<int>(item.primary_id));
        const String quantity_text = item.quantity > 1U
            ? vformat("x%d", static_cast<int>(item.quantity))
            : String();
        button->set_text(quantity_text);
        button->set_button_icon(item_icon_for(item.primary_id));
        button->set_tooltip_text(vformat(
            "Slot %d: %s x%d",
            static_cast<int>(item.item_id),
            item_name,
            static_cast<int>(item.quantity)));
    }

    prune_loadout_slot_registry(desired_keys);
}

Ref<Texture2D> Gs1GodotRegionalSelectionPanelController::item_icon_for(std::uint32_t item_id) const
{
    return load_cached_texture(
        GodotProgressionResourceDatabase::instance().content_icon_path(k_content_resource_kind_item, item_id));
}

Ref<Texture2D> Gs1GodotRegionalSelectionPanelController::load_cached_texture(const String& path) const
{
    if (path.is_empty())
    {
        return Ref<Texture2D> {};
    }

    const std::string key = path.utf8().get_data();
    if (const auto found = texture_cache_.find(key); found != texture_cache_.end())
    {
        return found->second;
    }

    return texture_cache_.emplace(key, load_texture_2d(path)).first->second;
}

std::uint64_t Gs1GodotRegionalSelectionPanelController::loadout_slot_key(std::uint32_t slot_id) const noexcept
{
    return static_cast<std::uint64_t>(slot_id);
}

Button* Gs1GodotRegionalSelectionPanelController::upsert_loadout_slot_button(
    std::uint64_t stable_key,
    const String& node_name,
    int desired_index)
{
    if (loadout_slots_grid_ == nullptr)
    {
        return nullptr;
    }

    Button* button = nullptr;
    if (const auto found = loadout_slot_buttons_.find(stable_key); found != loadout_slot_buttons_.end())
    {
        button = resolve_object<Button>(found->second.object_id);
    }

    if (button == nullptr)
    {
        button = memnew(Button);
        button->set_clip_text(true);
        button->set_custom_minimum_size(Vector2(72.0F, 72.0F));
        button->set_focus_mode(Control::FOCUS_NONE);
        button->set_expand_icon(true);
        button->set_icon_alignment(HORIZONTAL_ALIGNMENT_CENTER);
        button->set_vertical_icon_alignment(VERTICAL_ALIGNMENT_CENTER);
        button->set_text_alignment(HORIZONTAL_ALIGNMENT_CENTER);
        loadout_slots_grid_->add_child(button);
        loadout_slot_buttons_[stable_key].object_id = button->get_instance_id();
    }

    button->set_name(node_name);
    const int child_count = loadout_slots_grid_->get_child_count();
    const int target_index = std::clamp(desired_index, 0, child_count - 1);
    if (button->get_index() != target_index)
    {
        loadout_slots_grid_->move_child(button, target_index);
    }
    return button;
}

void Gs1GodotRegionalSelectionPanelController::prune_loadout_slot_registry(
    const std::unordered_set<std::uint64_t>& desired_keys)
{
    for (auto it = loadout_slot_buttons_.begin(); it != loadout_slot_buttons_.end();)
    {
        if (desired_keys.contains(it->first))
        {
            ++it;
            continue;
        }

        if (Button* button = resolve_object<Button>(it->second.object_id))
        {
            button->queue_free();
        }
        it = loadout_slot_buttons_.erase(it);
    }
}
