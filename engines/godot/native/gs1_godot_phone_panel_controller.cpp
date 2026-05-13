#include "gs1_godot_phone_panel_controller.h"

#include "gs1_godot_controller_context.h"
#include "content/defs/item_defs.h"
#include "support/currency.h"

#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/core/object.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>

#include <algorithm>
#include <cstdint>
#include <string_view>

using namespace godot;

namespace
{
constexpr std::int64_t k_gameplay_action_buy_phone_listing = 8;
constexpr std::int64_t k_gameplay_action_sell_phone_listing = 9;
constexpr std::int64_t k_gameplay_action_hire_contractor = 12;
constexpr std::int64_t k_gameplay_action_purchase_site_unlockable = 13;
constexpr std::int64_t k_gameplay_action_set_phone_panel_section = 17;
constexpr std::int64_t k_gameplay_action_close_phone_panel = 22;

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

void Gs1GodotPhonePanelController::_bind_methods()
{
}

void Gs1GodotPhonePanelController::_ready()
{
    set_submit_gameplay_action_callback([this](std::int64_t action_type, std::int64_t target_id, std::int64_t arg0, std::int64_t arg1) {
        submit_gameplay_action(action_type, target_id, arg0, arg1);
    });
    cache_adapter_service();
    if (Control* owner = resolve_owner_control())
    {
        cache_ui_references(*owner);
    }
}

void Gs1GodotPhonePanelController::_exit_tree()
{
    if (adapter_service_ != nullptr)
    {
        adapter_service_->unsubscribe_all(*this);
        adapter_service_ = nullptr;
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
        {"OpenPhoneHomeButton", static_cast<std::int64_t>(Gs1GodotPhoneSection::Home)},
        {"OpenPhoneTasksButton", static_cast<std::int64_t>(Gs1GodotPhoneSection::Tasks)},
        {"OpenPhoneBuyButton", static_cast<std::int64_t>(Gs1GodotPhoneSection::Buy)},
        {"OpenPhoneSellButton", static_cast<std::int64_t>(Gs1GodotPhoneSection::Sell)},
        {"OpenPhoneHireButton", static_cast<std::int64_t>(Gs1GodotPhoneSection::Hire)},
        {"OpenPhoneCartButton", static_cast<std::int64_t>(Gs1GodotPhoneSection::Cart)},
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
    apply_panel_visibility();
    update_phone_state_label();
    reconcile_phone_listing_buttons();
}

void Gs1GodotPhonePanelController::set_submit_gameplay_action_callback(SubmitGameplayActionFn callback)
{
    submit_gameplay_action_ = std::move(callback);
}

void Gs1GodotPhonePanelController::cache_adapter_service()
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

Control* Gs1GodotPhonePanelController::resolve_owner_control()
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

void Gs1GodotPhonePanelController::submit_gameplay_action(
    std::int64_t action_type,
    std::int64_t target_id,
    std::int64_t arg0,
    std::int64_t arg1)
{
    if (adapter_service_ != nullptr)
    {
        adapter_service_->submit_gameplay_action(action_type, target_id, arg0, arg1);
    }
}

bool Gs1GodotPhonePanelController::handles_engine_message(Gs1EngineMessageType type) const noexcept
{
    switch (type)
    {
    case GS1_ENGINE_MESSAGE_PRESENTATION_DIRTY:
    case GS1_ENGINE_MESSAGE_SET_APP_STATE:
        return true;
    default:
        return false;
    }
}

void Gs1GodotPhonePanelController::handle_engine_message(const Gs1EngineMessage& message)
{
    switch (message.type)
    {
    case GS1_ENGINE_MESSAGE_PRESENTATION_DIRTY:
    {
        const auto& payload = message.payload_as<Gs1EngineMessagePresentationDirtyData>();
        if ((payload.dirty_flags & (GS1_PRESENTATION_DIRTY_SITE | GS1_PRESENTATION_DIRTY_PHONE)) != 0U)
        {
            refresh_from_game_state_view();
        }
        break;
    }
    case GS1_ENGINE_MESSAGE_SET_APP_STATE:
        refresh_from_game_state_view();
        break;
    default:
        break;
    }
    apply_panel_visibility();
    update_phone_state_label();
    reconcile_phone_listing_buttons();
}

void Gs1GodotPhonePanelController::handle_runtime_message_reset()
{
    phone_listings_state_.clear();
    pending_listing_indices_.clear();
    prune_button_registry(phone_listing_buttons_, {});
    apply_panel_visibility();
}

void Gs1GodotPhonePanelController::refresh_from_game_state_view()
{
    if (adapter_service_ == nullptr)
    {
        return;
    }

    Gs1GameStateView view {};
    if (!adapter_service_->get_game_state_view(view) ||
        view.has_active_site == 0U ||
        view.active_site == nullptr)
    {
        phone_listings_state_.clear();
        pending_listing_indices_.clear();
        return;
    }

    const Gs1SiteStateView& site = *view.active_site;
    phone_listings_state_.clear();
    phone_listings_state_.reserve(site.phone_listing_count);
    for (std::uint32_t index = 0; index < site.phone_listing_count; ++index)
    {
        const Gs1PhoneListingView& listing_view = site.phone_listings[index];
        Gs1RuntimePhoneListingProjection listing {};
        listing.listing_id = listing_view.listing_id;
        listing.item_or_unlockable_id = listing_view.item_or_unlockable_id;
        listing.price = gs1::cash_value_from_cash_points(listing_view.price_cash_points);
        listing.related_site_id = site.site_id;
        listing.quantity = static_cast<std::uint16_t>(std::min<std::uint32_t>(listing_view.quantity, 65535U));
        listing.listing_kind = static_cast<Gs1PhoneListingPresentationKind>(listing_view.listing_kind);
        listing.flags = listing_view.occupied != 0U ? 1U : 0U;
        phone_listings_state_.push_back(listing);
    }

    std::sort(phone_listings_state_.begin(), phone_listings_state_.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.listing_id < rhs.listing_id;
    });
    pending_listing_indices_.clear();
}

void Gs1GodotPhonePanelController::apply_panel_visibility()
{
    const bool panel_visible =
        adapter_service_ != nullptr &&
        adapter_service_->ui_session_state().phone.open;
    if (panel_ != nullptr)
    {
        panel_->set_visible(panel_visible);
    }
}

void Gs1GodotPhonePanelController::update_phone_state_label()
{
    if (panel_ == nullptr || !panel_->is_visible() || phone_state_label_ == nullptr)
    {
        return;
    }

    if (adapter_service_ != nullptr)
    {
        const auto& phone_state = adapter_service_->ui_session_state().phone;
        std::uint32_t buy_count = 0U;
        std::uint32_t sell_count = 0U;
        std::uint32_t service_count = 0U;
        for (const auto& listing : phone_listings_state_)
        {
            switch (listing.listing_kind)
            {
            case GS1_PHONE_LISTING_PRESENTATION_BUY_ITEM:
                buy_count += 1U;
                break;
            case GS1_PHONE_LISTING_PRESENTATION_SELL_ITEM:
                sell_count += 1U;
                break;
            case GS1_PHONE_LISTING_PRESENTATION_HIRE_CONTRACTOR:
            case GS1_PHONE_LISTING_PRESENTATION_PURCHASE_UNLOCKABLE:
                service_count += 1U;
                break;
            default:
                break;
            }
        }
        phone_state_label_->set_text(vformat(
            "Phone Section: %d  Buy %d  Sell %d  Services %d",
            static_cast<int>(phone_state.active_section),
            static_cast<int>(buy_count),
            static_cast<int>(sell_count),
            static_cast<int>(service_count)));
    }
    else
    {
        phone_state_label_->set_text("Phone Section: 0  Buy 0  Sell 0  Services 0");
    }
}

void Gs1GodotPhonePanelController::handle_phone_listing_pressed(std::int64_t button_key)
{
    auto found = phone_listing_buttons_.find(static_cast<std::uint64_t>(button_key));
    if (found == phone_listing_buttons_.end())
    {
        return;
    }

    Button* button = resolve_object<Button>(found->second.object_id);
    if (button == nullptr || !submit_gameplay_action_)
    {
        return;
    }

    submit_gameplay_action_(
        static_cast<int>(button->get_meta("action_type", k_gameplay_action_buy_phone_listing)),
        static_cast<int>(button->get_meta("listing_id", 0)),
        1,
        0);
}

void Gs1GodotPhonePanelController::handle_phone_section_pressed(std::int64_t section)
{
    if (!submit_gameplay_action_)
    {
        return;
    }

    submit_gameplay_action_(
        k_gameplay_action_set_phone_panel_section,
        0,
        section,
        0);
}

void Gs1GodotPhonePanelController::handle_close_phone_pressed()
{
    if (!submit_gameplay_action_)
    {
        return;
    }

    submit_gameplay_action_(
        k_gameplay_action_close_phone_panel,
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
        std::int64_t action_type = k_gameplay_action_buy_phone_listing;
        switch (listing.listing_kind)
        {
        case GS1_PHONE_LISTING_PRESENTATION_SELL_ITEM:
            action_type = k_gameplay_action_sell_phone_listing;
            break;
        case GS1_PHONE_LISTING_PRESENTATION_HIRE_CONTRACTOR:
            action_type = k_gameplay_action_hire_contractor;
            break;
        case GS1_PHONE_LISTING_PRESENTATION_PURCHASE_UNLOCKABLE:
            action_type = k_gameplay_action_purchase_site_unlockable;
            break;
        case GS1_PHONE_LISTING_PRESENTATION_BUY_ITEM:
        default:
            action_type = k_gameplay_action_buy_phone_listing;
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
