#include "gs1_godot_action_panel_controller.h"

#include "gs1_godot_controller_context.h"

#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/core/object.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <godot_cpp/variant/dictionary.hpp>

#include <algorithm>
#include <array>
#include <utility>

using namespace godot;

namespace
{
constexpr std::int64_t k_ui_action_open_site_protection_selector = 24;

constexpr std::array<int, 15> k_allowed_site_action_types {
    6, 7, 8, 10, 11, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26
};

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

void sort_ui_setups(std::vector<Gs1RuntimeUiSetupProjection>& setups)
{
    std::sort(setups.begin(), setups.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.setup_id < rhs.setup_id;
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

template <typename T>
T* resolve_object(const ObjectID object_id)
{
    return Object::cast_to<T>(ObjectDB::get_instance(object_id));
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

Gs1GodotActionPanelController* resolve_controller(std::int64_t controller_bits)
{
    return reinterpret_cast<Gs1GodotActionPanelController*>(static_cast<std::uintptr_t>(controller_bits));
}

void dispatch_fixed_slot_pressed(std::int64_t controller_bits, std::int64_t button_id)
{
    if (Gs1GodotActionPanelController* controller = resolve_controller(controller_bits))
    {
        controller->handle_fixed_slot_pressed(button_id);
    }
}

void dispatch_site_control_pressed(std::int64_t controller_bits, std::int64_t button_key)
{
    if (Gs1GodotActionPanelController* controller = resolve_controller(controller_bits))
    {
        controller->handle_site_control_pressed(button_key);
    }
}

void dispatch_open_protection_pressed(std::int64_t controller_bits)
{
    if (Gs1GodotActionPanelController* controller = resolve_controller(controller_bits))
    {
        controller->handle_open_protection_pressed();
    }
}
}

void Gs1GodotActionPanelController::_bind_methods()
{
}

void Gs1GodotActionPanelController::_ready()
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

void Gs1GodotActionPanelController::_exit_tree()
{
    if (adapter_service_ != nullptr)
    {
        adapter_service_->unsubscribe_all(*this);
        adapter_service_ = nullptr;
    }
}

void Gs1GodotActionPanelController::cache_ui_references(Control& owner)
{
    owner_control_ = &owner;
    if (site_controls_ == nullptr)
    {
        site_controls_ = Object::cast_to<VBoxContainer>(owner.find_child("SiteControls", true, false));
    }
    if (Button* button = Object::cast_to<Button>(owner.find_child("OpenProtectionButton", true, false)))
    {
        const Callable callback = callable_mp_static(&dispatch_open_protection_pressed).bind(
            static_cast<std::int64_t>(reinterpret_cast<std::uintptr_t>(this)));
        if (!button->is_connected("pressed", callback))
        {
            button->connect("pressed", callback);
        }
    }
    cache_fixed_slot_bindings();
    rebuild_fixed_slot_actions();
    reconcile_site_control_buttons();
}

void Gs1GodotActionPanelController::set_submit_ui_action_callback(SubmitUiActionFn callback)
{
    submit_ui_action_ = std::move(callback);
}

void Gs1GodotActionPanelController::cache_adapter_service()
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

Control* Gs1GodotActionPanelController::resolve_owner_control()
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

void Gs1GodotActionPanelController::submit_ui_action(
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

void Gs1GodotActionPanelController::reset_ui_setup_state() noexcept
{
    ui_setups_.clear();
    pending_ui_setup_.reset();
    ui_setup_indices_.clear();
    pending_ui_setup_element_indices_.clear();
}

bool Gs1GodotActionPanelController::setup_has_site_controls(const Gs1RuntimeUiSetupProjection& setup) const
{
    if (setup.presentation_type == GS1_UI_SETUP_PRESENTATION_OVERLAY)
    {
        return false;
    }

    for (const auto& element : setup.elements)
    {
        if (element.element_type != GS1_UI_ELEMENT_BUTTON)
        {
            continue;
        }

        const int action_type = static_cast<int>(element.action.type);
        if (std::find(k_allowed_site_action_types.begin(), k_allowed_site_action_types.end(), action_type) !=
            k_allowed_site_action_types.end())
        {
            return true;
        }
    }
    return false;
}

void Gs1GodotActionPanelController::rebuild_ui_setup_indices() noexcept
{
    ui_setup_indices_.clear();
    ui_setup_indices_.reserve(ui_setups_.size());
    for (std::size_t index = 0; index < ui_setups_.size(); ++index)
    {
        ui_setup_indices_[static_cast<std::uint16_t>(ui_setups_[index].setup_id)] = index;
    }
}

bool Gs1GodotActionPanelController::apply_ui_setup_message(const Gs1EngineMessage& message)
{
    switch (message.type)
    {
    case GS1_ENGINE_MESSAGE_BEGIN_UI_SETUP:
    {
        const auto& payload = message.payload_as<Gs1EngineMessageUiSetupData>();
        pending_ui_setup_element_indices_.clear();
        if (payload.mode == GS1_PROJECTION_MODE_DELTA)
        {
            const auto found = ui_setup_indices_.find(static_cast<std::uint16_t>(payload.setup_id));
            if (found != ui_setup_indices_.end() && found->second < ui_setups_.size())
            {
                const auto& existing = ui_setups_[found->second];
                pending_ui_setup_ = PendingUiSetup {};
                pending_ui_setup_->setup_id = existing.setup_id;
                pending_ui_setup_->presentation_type = existing.presentation_type;
                pending_ui_setup_->context_id = existing.context_id;
                pending_ui_setup_->elements = existing.elements;
                pending_ui_setup_element_indices_.reserve(pending_ui_setup_->elements.size());
                for (std::size_t index = 0; index < pending_ui_setup_->elements.size(); ++index)
                {
                    pending_ui_setup_element_indices_[pending_ui_setup_->elements[index].element_id] = index;
                }
            }
            else
            {
                pending_ui_setup_ = PendingUiSetup {};
            }
        }
        else
        {
            pending_ui_setup_ = PendingUiSetup {};
        }

        pending_ui_setup_->setup_id = payload.setup_id;
        pending_ui_setup_->presentation_type = payload.presentation_type;
        pending_ui_setup_->context_id = payload.context_id;
        if (payload.mode == GS1_PROJECTION_MODE_SNAPSHOT)
        {
            pending_ui_setup_->elements.clear();
        }
        pending_ui_setup_->elements.reserve(std::max<std::size_t>(pending_ui_setup_->elements.size(), payload.element_count));
        break;
    }
    case GS1_ENGINE_MESSAGE_UI_ELEMENT_UPSERT:
    {
        if (!pending_ui_setup_.has_value())
        {
            return false;
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
        const auto found = pending_ui_setup_element_indices_.find(projection.element_id);
        if (found != pending_ui_setup_element_indices_.end() && found->second < pending_ui_setup_->elements.size())
        {
            pending_ui_setup_->elements[found->second] = projection;
        }
        else
        {
            pending_ui_setup_element_indices_[projection.element_id] = pending_ui_setup_->elements.size();
            pending_ui_setup_->elements.push_back(std::move(projection));
        }
        break;
    }
    case GS1_ENGINE_MESSAGE_END_UI_SETUP:
    {
        if (!pending_ui_setup_.has_value())
        {
            return false;
        }

        Gs1RuntimeUiSetupProjection setup {};
        setup.setup_id = pending_ui_setup_->setup_id;
        setup.presentation_type = pending_ui_setup_->presentation_type;
        setup.context_id = pending_ui_setup_->context_id;
        setup.elements = std::move(pending_ui_setup_->elements);

        if (setup.presentation_type == GS1_UI_SETUP_PRESENTATION_NORMAL)
        {
            erase_projection_if(ui_setups_, [](const auto& existing) {
                return existing.presentation_type == GS1_UI_SETUP_PRESENTATION_NORMAL;
            });
        }

        upsert_projection(ui_setups_, std::move(setup), pending_ui_setup_->setup_id, [](const auto& existing) {
            return existing.setup_id;
        });
        sort_ui_setups(ui_setups_);
        rebuild_ui_setup_indices();
        pending_ui_setup_.reset();
        pending_ui_setup_element_indices_.clear();
        return setup_has_site_controls(setup);
    }
    case GS1_ENGINE_MESSAGE_CLOSE_UI_SETUP:
    {
        const auto& payload = message.payload_as<Gs1EngineMessageCloseUiSetupData>();
        bool affected_site_controls = false;
        const auto found = ui_setup_indices_.find(static_cast<std::uint16_t>(payload.setup_id));
        if (found != ui_setup_indices_.end() && found->second < ui_setups_.size())
        {
            affected_site_controls = setup_has_site_controls(ui_setups_[found->second]);
        }
        erase_projection_if(ui_setups_, [&](const auto& setup) {
            return setup.setup_id == payload.setup_id;
        });
        rebuild_ui_setup_indices();
        return affected_site_controls;
    }
    default:
        break;
    }
    return false;
}

void Gs1GodotActionPanelController::reset_ui_panel_state() noexcept
{
    ui_panels_.clear();
    pending_ui_panel_.reset();
    ui_panel_indices_.clear();
    pending_ui_panel_text_line_indices_.clear();
    pending_ui_panel_slot_action_indices_.clear();
    pending_ui_panel_list_item_indices_.clear();
    pending_ui_panel_list_action_indices_.clear();
}

void Gs1GodotActionPanelController::rebuild_fixed_slot_actions()
{
    cache_fixed_slot_bindings();
    rebuild_fixed_slot_panel(1);
    rebuild_fixed_slot_panel(2);
    rebuild_fixed_slot_panel(3);
}

void Gs1GodotActionPanelController::rebuild_ui_panel_indices() noexcept
{
    ui_panel_indices_.clear();
    ui_panel_indices_.reserve(ui_panels_.size());
    for (std::size_t index = 0; index < ui_panels_.size(); ++index)
    {
        ui_panel_indices_[static_cast<std::uint16_t>(ui_panels_[index].panel_id)] = index;
    }
}

std::optional<int> Gs1GodotActionPanelController::apply_ui_panel_message(const Gs1EngineMessage& message)
{
    const Gs1EngineMessageType type = message.type;
    if (type == GS1_ENGINE_MESSAGE_BEGIN_UI_PANEL)
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
    else if (type == GS1_ENGINE_MESSAGE_UI_PANEL_TEXT_UPSERT)
    {
        if (!pending_ui_panel_.has_value())
        {
            return std::nullopt;
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
    else if (type == GS1_ENGINE_MESSAGE_UI_PANEL_SLOT_ACTION_UPSERT)
    {
        if (!pending_ui_panel_.has_value())
        {
            return std::nullopt;
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
    else if (type == GS1_ENGINE_MESSAGE_UI_PANEL_LIST_ITEM_UPSERT)
    {
        if (!pending_ui_panel_.has_value())
        {
            return std::nullopt;
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
    else if (type == GS1_ENGINE_MESSAGE_UI_PANEL_LIST_ACTION_UPSERT)
    {
        if (!pending_ui_panel_.has_value())
        {
            return std::nullopt;
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
    else if (type == GS1_ENGINE_MESSAGE_END_UI_PANEL)
    {
        if (!pending_ui_panel_.has_value())
        {
            return std::nullopt;
        }
        const int panel_id = static_cast<int>(pending_ui_panel_->panel_id);
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
        return panel_id;
    }
    else if (type == GS1_ENGINE_MESSAGE_CLOSE_UI_PANEL)
    {
        const auto& payload = message.payload_as<Gs1EngineMessageCloseUiPanelData>();
        erase_projection_if(ui_panels_, [&](const auto& panel) {
            return panel.panel_id == payload.panel_id;
        });
        rebuild_ui_panel_indices();
        return static_cast<int>(payload.panel_id);
    }
    return std::nullopt;
}

void Gs1GodotActionPanelController::handle_open_protection_pressed()
{
    if (!submit_ui_action_)
    {
        return;
    }

    submit_ui_action_(
        k_ui_action_open_site_protection_selector,
        0,
        0,
        0);
}

bool Gs1GodotActionPanelController::handles_engine_message(Gs1EngineMessageType type) const noexcept
{
    switch (type)
    {
    case GS1_ENGINE_MESSAGE_BEGIN_UI_SETUP:
    case GS1_ENGINE_MESSAGE_UI_ELEMENT_UPSERT:
    case GS1_ENGINE_MESSAGE_END_UI_SETUP:
    case GS1_ENGINE_MESSAGE_CLOSE_UI_SETUP:
    case GS1_ENGINE_MESSAGE_BEGIN_UI_PANEL:
    case GS1_ENGINE_MESSAGE_UI_PANEL_TEXT_UPSERT:
    case GS1_ENGINE_MESSAGE_UI_PANEL_SLOT_ACTION_UPSERT:
    case GS1_ENGINE_MESSAGE_UI_PANEL_LIST_ITEM_UPSERT:
    case GS1_ENGINE_MESSAGE_UI_PANEL_LIST_ACTION_UPSERT:
    case GS1_ENGINE_MESSAGE_END_UI_PANEL:
    case GS1_ENGINE_MESSAGE_CLOSE_UI_PANEL:
        return true;
    default:
        return false;
    }
}

void Gs1GodotActionPanelController::handle_engine_message(const Gs1EngineMessage& message)
{
    const bool site_controls_changed = apply_ui_setup_message(message);
    const std::optional<int> changed_panel_id = apply_ui_panel_message(message);

    if (site_controls_changed)
    {
        reconcile_site_control_buttons();
    }
    if (changed_panel_id.has_value() && changed_panel_id.value() >= 1 && changed_panel_id.value() <= 3)
    {
        rebuild_fixed_slot_panel(changed_panel_id.value());
    }
}

void Gs1GodotActionPanelController::handle_runtime_message_reset()
{
    reset_ui_setup_state();
    reset_ui_panel_state();
    clear_fixed_slot_actions();
    prune_button_registry(site_control_buttons_, {});
}

void Gs1GodotActionPanelController::handle_fixed_slot_pressed(std::int64_t button_id)
{
    BaseButton* button = resolve_object<BaseButton>(static_cast<ObjectID>(button_id));
    if (button == nullptr || !submit_ui_action_ || !bool(button->get_meta("gs1_has_action", false)))
    {
        return;
    }

    submit_ui_action_(
        as_int(button->get_meta("gs1_action_type", 0), 0),
        as_int(button->get_meta("gs1_target_id", 0), 0),
        as_int(button->get_meta("gs1_arg0", 0), 0),
        as_int(button->get_meta("gs1_arg1", 0), 0));
}

void Gs1GodotActionPanelController::handle_site_control_pressed(std::int64_t button_key)
{
    auto found = site_control_buttons_.find(static_cast<std::uint64_t>(button_key));
    if (found == site_control_buttons_.end())
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

void Gs1GodotActionPanelController::cache_fixed_slot_bindings()
{
    if (fixed_slot_bindings_cached_ || owner_control_ == nullptr)
    {
        return;
    }

    SceneTree* tree = owner_control_->get_tree();
    if (tree == nullptr)
    {
        return;
    }

    fixed_slot_bindings_.clear();
    const Array slot_nodes = tree->get_nodes_in_group("gs1_slot_button");
    for (int64_t index = 0; index < slot_nodes.size(); ++index)
    {
        BaseButton* button = Object::cast_to<BaseButton>(slot_nodes[index]);
        if (button == nullptr || !owner_control_->is_ancestor_of(button))
        {
            continue;
        }

        FixedSlotBinding binding {};
        binding.object_id = button->get_instance_id();
        binding.panel_id = as_int(button->get_meta("gs1_panel_id", Variant()), 0);
        binding.slot_id = as_int(button->get_meta("gs1_slot_id", Variant()), 0);
        if (binding.panel_id == 0 || binding.slot_id == 0)
        {
            continue;
        }

        fixed_slot_bindings_.push_back(binding);
        const Callable callback = callable_mp_static(&dispatch_fixed_slot_pressed).bind(
            static_cast<std::int64_t>(reinterpret_cast<std::uintptr_t>(this)),
            static_cast<std::int64_t>(binding.object_id));
        if (!button->is_connected("pressed", callback))
        {
            button->connect("pressed", callback);
        }
    }

    fixed_slot_bindings_cached_ = true;
}

void Gs1GodotActionPanelController::clear_fixed_slot_actions()
{
    for (const FixedSlotBinding& binding : fixed_slot_bindings_)
    {
        if (BaseButton* button = resolve_object<BaseButton>(binding.object_id))
        {
            clear_action_from_button(button);
        }
    }
}

void Gs1GodotActionPanelController::clear_fixed_slot_actions(int panel_id)
{
    for (const FixedSlotBinding& binding : fixed_slot_bindings_)
    {
        if (binding.panel_id != panel_id)
        {
            continue;
        }

        if (BaseButton* button = resolve_object<BaseButton>(binding.object_id))
        {
            clear_action_from_button(button);
        }
    }
}

void Gs1GodotActionPanelController::rebuild_fixed_slot_panel(int panel_id)
{
    clear_fixed_slot_actions(panel_id);
    bind_fixed_slot_actions(find_ui_panel(panel_id), panel_id);
}

void Gs1GodotActionPanelController::bind_fixed_slot_actions(const Gs1RuntimeUiPanelProjection* panel, int panel_id)
{
    for (const FixedSlotBinding& binding : fixed_slot_bindings_)
    {
        if (binding.panel_id != panel_id)
        {
            continue;
        }

        BaseButton* button = resolve_object<BaseButton>(binding.object_id);
        if (button == nullptr)
        {
            continue;
        }

        if (panel == nullptr)
        {
            clear_action_from_button(button);
            continue;
        }

        const Gs1RuntimeUiPanelSlotActionProjection* slot_action = find_panel_slot_action(*panel, binding.slot_id);
        if (slot_action == nullptr)
        {
            clear_action_from_button(button);
            continue;
        }

        Dictionary action;
        action["type"] = static_cast<int>(slot_action->action.type);
        action["target_id"] = static_cast<int64_t>(slot_action->action.target_id);
        action["arg0"] = static_cast<int64_t>(slot_action->action.arg0);
        action["arg1"] = static_cast<int64_t>(slot_action->action.arg1);

        Dictionary slot_action_dict;
        slot_action_dict["flags"] = static_cast<int>(slot_action->flags);
        slot_action_dict["action"] = action;
        const String fallback_label = Object::cast_to<Button>(button) != nullptr
            ? Object::cast_to<Button>(button)->get_text()
            : String();
        const String resolved_label = panel_slot_label(*slot_action);
        apply_action_to_button(button, slot_action_dict, resolved_label.is_empty() ? fallback_label : resolved_label);
    }
}

void Gs1GodotActionPanelController::apply_action_to_button(BaseButton* button, const Dictionary& action, const String& fallback_label)
{
    if (button == nullptr)
    {
        return;
    }

    const Dictionary payload = action.get("action", Dictionary());
    button->set_meta("gs1_action_type", as_int(payload.get("type", 0), 0));
    button->set_meta("gs1_target_id", as_int(payload.get("target_id", 0), 0));
    button->set_meta("gs1_arg0", as_int(payload.get("arg0", 0), 0));
    button->set_meta("gs1_arg1", as_int(payload.get("arg1", 0), 0));
    button->set_meta("gs1_has_action", true);
    button->set_disabled((as_int(action.get("flags", 0), 0) & 2) != 0);
    if (Button* text_button = Object::cast_to<Button>(button))
    {
        text_button->set_text(fallback_label);
    }
}

void Gs1GodotActionPanelController::clear_action_from_button(BaseButton* button, bool preserve_text)
{
    if (button == nullptr)
    {
        return;
    }

    button->set_meta("gs1_action_type", 0);
    button->set_meta("gs1_target_id", 0);
    button->set_meta("gs1_arg0", 0);
    button->set_meta("gs1_arg1", 0);
    button->set_meta("gs1_has_action", false);
    button->set_disabled(true);
    if (!preserve_text)
    {
        if (Button* text_button = Object::cast_to<Button>(button))
        {
            text_button->set_text(String());
        }
    }
}

void Gs1GodotActionPanelController::reconcile_site_control_buttons()
{
    Array button_specs;
    for (const auto& setup : ui_setups_)
    {
        if (setup.presentation_type == GS1_UI_SETUP_PRESENTATION_OVERLAY)
        {
            continue;
        }
        for (const auto& element : setup.elements)
        {
            const int action_type = static_cast<int>(element.action.type);
            if (std::find(k_allowed_site_action_types.begin(), k_allowed_site_action_types.end(), action_type) == k_allowed_site_action_types.end())
            {
                continue;
            }
            if (element.element_type != GS1_UI_ELEMENT_BUTTON)
            {
                continue;
            }

            Dictionary action;
            action["type"] = action_type;
            action["target_id"] = static_cast<int64_t>(element.action.target_id);
            action["arg0"] = static_cast<int64_t>(element.action.arg0);
            action["arg1"] = static_cast<int64_t>(element.action.arg1);

            Dictionary spec;
            spec["setup_id"] = static_cast<int>(setup.setup_id);
            spec["element_id"] = static_cast<int>(element.element_id);
            spec["text"] = String("Action");
            spec["flags"] = static_cast<int>(element.flags);
            spec["action"] = action;
            button_specs.push_back(spec);
        }
    }

    reconcile_projected_action_buttons(site_controls_, site_control_buttons_, button_specs);
}

void Gs1GodotActionPanelController::reconcile_projected_action_buttons(
    Node* container,
    std::unordered_map<std::uint64_t, ProjectedButtonRecord>& registry,
    const Array& button_specs)
{
    if (container == nullptr)
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
            container,
            registry,
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
        const Callable callback = callable_mp_static(&dispatch_site_control_pressed).bind(
            static_cast<std::int64_t>(reinterpret_cast<std::uintptr_t>(this)),
            static_cast<std::int64_t>(stable_key));
        if (!button->is_connected("pressed", callback))
        {
            button->connect("pressed", callback);
        }
    }

    prune_button_registry(registry, desired_keys);
}

Button* Gs1GodotActionPanelController::upsert_button_node(
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
        container->add_child(button);
        registry[stable_key].object_id = button->get_instance_id();
    }

    button->set_name(node_name);
    button->set_custom_minimum_size(Vector2(0.0, 48.0));
    const int static_child_count = std::max(0, container->get_child_count() - static_cast<int>(registry.size()));
    const int target_index = std::min(static_child_count + desired_index, container->get_child_count() - 1);
    if (button->get_index() != target_index)
    {
        container->move_child(button, target_index);
    }
    return button;
}

void Gs1GodotActionPanelController::prune_button_registry(
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

const Gs1RuntimeUiPanelProjection* Gs1GodotActionPanelController::find_ui_panel(int panel_id) const
{
    const auto found = ui_panel_indices_.find(static_cast<std::uint16_t>(panel_id));
    if (found == ui_panel_indices_.end() || found->second >= ui_panels_.size())
    {
        return nullptr;
    }
    return &ui_panels_[found->second];
}

const Gs1RuntimeUiPanelSlotActionProjection* Gs1GodotActionPanelController::find_panel_slot_action(
    const Gs1RuntimeUiPanelProjection& panel,
    int slot_id) const
{
    for (const auto& action : panel.slot_actions)
    {
        if (static_cast<int>(action.slot_id) == slot_id)
        {
            return &action;
        }
    }
    return nullptr;
}

String Gs1GodotActionPanelController::panel_slot_label(const Gs1RuntimeUiPanelSlotActionProjection& slot_action) const
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
