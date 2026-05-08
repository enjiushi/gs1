#include "gs1_godot_phone_panel_controller.h"

#include "content/defs/item_defs.h"

#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/core/object.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>

#include <algorithm>
#include <cstdint>
#include <string_view>

using namespace godot;

namespace
{
constexpr std::int64_t k_ui_action_buy_phone_listing = 8;
constexpr std::int64_t k_ui_action_sell_phone_listing = 9;
constexpr std::int64_t k_ui_action_hire_contractor = 12;
constexpr std::int64_t k_ui_action_purchase_site_unlockable = 13;
constexpr std::int64_t k_ui_action_set_phone_panel_section = 17;
constexpr std::int64_t k_ui_action_close_phone_panel = 22;

template <typename T>
T* resolve_object(const ObjectID object_id)
{
    return Object::cast_to<T>(ObjectDB::get_instance(object_id));
}

String string_from_view(std::string_view value)
{
    return String::utf8(value.data(), static_cast<int>(value.size()));
}

Gs1GodotPhonePanelController* resolve_controller(std::int64_t controller_bits)
{
    return reinterpret_cast<Gs1GodotPhonePanelController*>(static_cast<std::uintptr_t>(controller_bits));
}

void dispatch_phone_listing_pressed(std::int64_t controller_bits, std::int64_t button_key)
{
    if (Gs1GodotPhonePanelController* controller = resolve_controller(controller_bits))
    {
        controller->handle_phone_listing_pressed(button_key);
    }
}

void dispatch_phone_section_pressed(std::int64_t controller_bits, std::int64_t section)
{
    if (Gs1GodotPhonePanelController* controller = resolve_controller(controller_bits))
    {
        controller->handle_phone_section_pressed(section);
    }
}

void dispatch_close_phone_pressed(std::int64_t controller_bits)
{
    if (Gs1GodotPhonePanelController* controller = resolve_controller(controller_bits))
    {
        controller->handle_close_phone_pressed();
    }
}
}

void Gs1GodotPhonePanelController::cache_ui_references(Control& owner)
{
    owner_control_ = &owner;
    if (panel_ == nullptr)
    {
        panel_ = Object::cast_to<Control>(owner.find_child("PhonePanel", true, false));
    }
    if (phone_state_label_ == nullptr)
    {
        phone_state_label_ = Object::cast_to<Label>(owner.find_child("PhoneStateLabel", true, false));
    }
    if (phone_listings_ == nullptr)
    {
        phone_listings_ = Object::cast_to<VBoxContainer>(owner.find_child("PhoneListings", true, false));
    }
    const std::int64_t controller_bits = static_cast<std::int64_t>(reinterpret_cast<std::uintptr_t>(this));
    const struct ButtonBinding final
    {
        const char* name;
        std::int64_t section;
    } section_bindings[] {
        {"OpenPhoneHomeButton", 0},
        {"OpenPhoneTasksButton", 1},
        {"OpenPhoneBuyButton", 2},
        {"OpenPhoneSellButton", 3},
        {"OpenPhoneHireButton", 4},
        {"OpenPhoneCartButton", 5},
    };
    for (const ButtonBinding& binding : section_bindings)
    {
        if (Button* button = Object::cast_to<Button>(owner.find_child(binding.name, true, false)))
        {
            const Callable callback = callable_mp_static(&dispatch_phone_section_pressed).bind(controller_bits, binding.section);
            if (!button->is_connected("pressed", callback))
            {
                button->connect("pressed", callback);
            }
        }
    }
    if (Button* button = Object::cast_to<Button>(owner.find_child("ClosePhoneButton", true, false)))
    {
        const Callable callback = callable_mp_static(&dispatch_close_phone_pressed).bind(controller_bits);
        if (!button->is_connected("pressed", callback))
        {
            button->connect("pressed", callback);
        }
    }
    refresh_if_needed();
}

void Gs1GodotPhonePanelController::set_submit_ui_action_callback(SubmitUiActionFn callback)
{
    submit_ui_action_ = std::move(callback);
}

bool Gs1GodotPhonePanelController::handles_engine_message(Gs1EngineMessageType type) const noexcept
{
    switch (type)
    {
    case GS1_ENGINE_MESSAGE_SET_APP_STATE:
    case GS1_ENGINE_MESSAGE_BEGIN_SITE_SNAPSHOT:
    case GS1_ENGINE_MESSAGE_SITE_PHONE_LISTING_UPSERT:
    case GS1_ENGINE_MESSAGE_SITE_PHONE_LISTING_REMOVE:
    case GS1_ENGINE_MESSAGE_SITE_PHONE_PANEL_STATE:
    case GS1_ENGINE_MESSAGE_END_SITE_SNAPSHOT:
        return true;
    default:
        return false;
    }
}

void Gs1GodotPhonePanelController::handle_engine_message(const Gs1EngineMessage& message)
{
    switch (message.type)
    {
    case GS1_ENGINE_MESSAGE_SITE_PHONE_PANEL_STATE:
        phone_panel_state_ = message.payload_as<Gs1EngineMessagePhonePanelData>();
        break;
    case GS1_ENGINE_MESSAGE_SET_APP_STATE:
    {
        const auto& payload = message.payload_as<Gs1EngineMessageSetAppStateData>();
        current_app_state_ = payload.app_state;
        if (payload.app_state == GS1_APP_STATE_MAIN_MENU ||
            payload.app_state == GS1_APP_STATE_REGIONAL_MAP ||
            payload.app_state == GS1_APP_STATE_CAMPAIGN_END)
        {
            phone_panel_state_.reset();
            phone_listings_state_.clear();
            pending_listing_indices_.clear();
        }
        break;
    }
    case GS1_ENGINE_MESSAGE_BEGIN_SITE_SNAPSHOT:
    {
        const auto& payload = message.payload_as<Gs1EngineMessageSiteSnapshotData>();
        pending_listing_indices_.clear();
        if (payload.mode == GS1_PROJECTION_MODE_SNAPSHOT)
        {
            phone_listings_state_.clear();
        }
        else
        {
            for (std::size_t index = 0; index < phone_listings_state_.size(); ++index)
            {
                pending_listing_indices_[phone_listings_state_[index].listing_id] = index;
            }
        }
        break;
    }
    case GS1_ENGINE_MESSAGE_SITE_PHONE_LISTING_UPSERT:
    {
        Gs1RuntimePhoneListingProjection projection = message.payload_as<Gs1EngineMessagePhoneListingData>();
        const auto found = pending_listing_indices_.find(projection.listing_id);
        if (found != pending_listing_indices_.end() && found->second < phone_listings_state_.size())
        {
            phone_listings_state_[found->second] = projection;
        }
        else
        {
            pending_listing_indices_[projection.listing_id] = phone_listings_state_.size();
            phone_listings_state_.push_back(projection);
        }
        break;
    }
    case GS1_ENGINE_MESSAGE_SITE_PHONE_LISTING_REMOVE:
    {
        const auto& payload = message.payload_as<Gs1EngineMessagePhoneListingData>();
        const auto found = pending_listing_indices_.find(payload.listing_id);
        if (found != pending_listing_indices_.end() && found->second < phone_listings_state_.size())
        {
            const std::size_t index = found->second;
            const std::size_t last_index = phone_listings_state_.size() - 1U;
            if (index != last_index)
            {
                phone_listings_state_[index] = std::move(phone_listings_state_[last_index]);
                pending_listing_indices_[phone_listings_state_[index].listing_id] = index;
            }
            phone_listings_state_.pop_back();
            pending_listing_indices_.erase(found);
        }
        break;
    }
    case GS1_ENGINE_MESSAGE_END_SITE_SNAPSHOT:
        std::sort(phone_listings_state_.begin(), phone_listings_state_.end(), [](const auto& lhs, const auto& rhs) {
            return lhs.listing_id < rhs.listing_id;
        });
        pending_listing_indices_.clear();
        break;
    default:
        break;
    }
    dirty_ = true;
    refresh_if_needed();
}

void Gs1GodotPhonePanelController::handle_runtime_message_reset()
{
    current_app_state_.reset();
    phone_panel_state_.reset();
    phone_listings_state_.clear();
    pending_listing_indices_.clear();
    dirty_ = true;
    refresh_if_needed();
}

void Gs1GodotPhonePanelController::refresh_if_needed()
{
    if (!dirty_)
    {
        return;
    }

    const int app_state = current_app_state_.has_value() ? static_cast<int>(current_app_state_.value()) : 0;
    const bool site_visible = app_state >= GS1_APP_STATE_SITE_LOADING && app_state <= GS1_APP_STATE_SITE_RESULT;
    const bool panel_visible = site_visible &&
        phone_panel_state_.has_value() &&
        (phone_panel_state_->flags & GS1_PHONE_PANEL_FLAG_OPEN) != 0U;
    if (panel_ != nullptr)
    {
        panel_->set_visible(panel_visible);
    }
    if (!panel_visible)
    {
        dirty_ = false;
        return;
    }
    if (phone_state_label_ == nullptr)
    {
        return;
    }

    if (phone_panel_state_.has_value())
    {
        phone_state_label_->set_text(vformat(
            "Phone Section: %d  Buy %d  Sell %d  Services %d  Cart %d",
            static_cast<int>(phone_panel_state_->active_section),
            static_cast<int>(phone_panel_state_->buy_listing_count),
            static_cast<int>(phone_panel_state_->sell_listing_count),
            static_cast<int>(phone_panel_state_->service_listing_count),
            static_cast<int>(phone_panel_state_->cart_item_count)));
    }
    else
    {
        phone_state_label_->set_text("Phone Section: 0  Listings: buy 0 sell 0");
    }

    reconcile_phone_listing_buttons();
    dirty_ = false;
}

void Gs1GodotPhonePanelController::handle_phone_listing_pressed(std::int64_t button_key)
{
    auto found = phone_listing_buttons_.find(static_cast<std::uint64_t>(button_key));
    if (found == phone_listing_buttons_.end())
    {
        return;
    }

    Button* button = resolve_object<Button>(found->second.object_id);
    if (button == nullptr || !submit_ui_action_)
    {
        return;
    }

    submit_ui_action_(
        static_cast<int>(button->get_meta("action_type", k_ui_action_buy_phone_listing)),
        static_cast<int>(button->get_meta("listing_id", 0)),
        1,
        0);
}

void Gs1GodotPhonePanelController::handle_phone_section_pressed(std::int64_t section)
{
    if (!submit_ui_action_)
    {
        return;
    }

    submit_ui_action_(
        k_ui_action_set_phone_panel_section,
        0,
        section,
        0);
}

void Gs1GodotPhonePanelController::handle_close_phone_pressed()
{
    if (!submit_ui_action_)
    {
        return;
    }

    submit_ui_action_(
        k_ui_action_close_phone_panel,
        0,
        0,
        0);
}

void Gs1GodotPhonePanelController::reconcile_phone_listing_buttons()
{
    if (phone_listings_ == nullptr)
    {
        return;
    }

    std::unordered_set<std::uint64_t> desired_keys;
    int desired_index = 0;
    for (const auto& listing : phone_listings_state_)
    {
        const int listing_id = static_cast<int>(listing.listing_id);
        const std::uint64_t stable_key = static_cast<std::uint64_t>(listing_id);
        desired_keys.insert(stable_key);
        Button* button = upsert_button_node(
            phone_listings_,
            phone_listing_buttons_,
            stable_key,
            vformat("DynamicPhone_%d", listing_id),
            desired_index++);
        if (button == nullptr)
        {
            continue;
        }

        const bool is_unlockable = listing.listing_kind == GS1_PHONE_LISTING_PRESENTATION_PURCHASE_UNLOCKABLE;
        const String icon_text = is_unlockable ? String("[UNL]") : String("[ITM]");
        button->set_text(vformat(
            "%s %s  x%d  price %.2f",
            icon_text,
            item_name_for(static_cast<int>(listing.item_or_unlockable_id)),
            static_cast<int>(listing.quantity),
            listing.price));
        button->set_tooltip_text(vformat(
            "listing %d kind %d id %d",
            listing_id,
            static_cast<int>(listing.listing_kind),
            static_cast<int>(listing.item_or_unlockable_id)));
        button->set_meta("listing_id", listing_id);
        std::int64_t action_type = k_ui_action_buy_phone_listing;
        switch (listing.listing_kind)
        {
        case GS1_PHONE_LISTING_PRESENTATION_SELL_ITEM:
            action_type = k_ui_action_sell_phone_listing;
            break;
        case GS1_PHONE_LISTING_PRESENTATION_HIRE_CONTRACTOR:
            action_type = k_ui_action_hire_contractor;
            break;
        case GS1_PHONE_LISTING_PRESENTATION_PURCHASE_UNLOCKABLE:
            action_type = k_ui_action_purchase_site_unlockable;
            break;
        case GS1_PHONE_LISTING_PRESENTATION_BUY_ITEM:
        default:
            action_type = k_ui_action_buy_phone_listing;
            break;
        }
        button->set_meta("action_type", action_type);
        const Callable callback = callable_mp_static(&dispatch_phone_listing_pressed).bind(
            static_cast<std::int64_t>(reinterpret_cast<std::uintptr_t>(this)),
            static_cast<std::int64_t>(stable_key));
        if (!button->is_connected("pressed", callback))
        {
            button->connect("pressed", callback);
        }
    }

    prune_button_registry(phone_listing_buttons_, desired_keys);
}

Button* Gs1GodotPhonePanelController::upsert_button_node(
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

void Gs1GodotPhonePanelController::prune_button_registry(
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

String Gs1GodotPhonePanelController::item_name_for(int item_id) const
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
