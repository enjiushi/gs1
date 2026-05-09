#include "gs1_godot_overlay_panel_controller.h"

#include "gs1_godot_controller_context.h"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/object.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <godot_cpp/variant/dictionary.hpp>

#include <algorithm>

using namespace godot;

namespace
{
constexpr std::int64_t k_ui_action_set_site_protection_overlay_mode = 26;
constexpr std::uint64_t pack_u32_pair(std::uint32_t high, std::uint32_t low) noexcept
{
    return (static_cast<std::uint64_t>(high) << 32U) | static_cast<std::uint64_t>(low);
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

Gs1GodotOverlayPanelController* resolve_controller(std::int64_t controller_bits)
{
    return reinterpret_cast<Gs1GodotOverlayPanelController*>(static_cast<std::uintptr_t>(controller_bits));
}

void dispatch_overlay_mode_pressed(std::int64_t controller_bits, std::int64_t mode)
{
    if (Gs1GodotOverlayPanelController* controller = resolve_controller(controller_bits))
    {
        controller->handle_overlay_mode_pressed(mode);
    }
}

void dispatch_projected_button_pressed(std::int64_t controller_bits, std::int64_t button_key)
{
    if (Gs1GodotOverlayPanelController* controller = resolve_controller(controller_bits))
    {
        controller->handle_projected_button_pressed(button_key);
    }
}
}

void Gs1GodotOverlayPanelController::_bind_methods()
{
}

void Gs1GodotOverlayPanelController::_ready()
{
    set_submit_ui_action_callback([this](std::int64_t action_type, std::int64_t target_id, std::int64_t arg0, std::int64_t arg1) {
        submit_ui_action(action_type, target_id, arg0, arg1);
    });
    cache_adapter_service();
    if (Control* owner = resolve_owner_control())
    {
        cache_ui_references(*owner);
    }
    set_process(true);
}

void Gs1GodotOverlayPanelController::_process(double delta)
{
    (void)delta;
    cache_adapter_service();
    if (Control* owner = resolve_owner_control())
    {
        cache_ui_references(*owner);
    }
    refresh_if_needed();
}

void Gs1GodotOverlayPanelController::_exit_tree()
{
    if (adapter_service_ != nullptr)
    {
        adapter_service_->unsubscribe_all(*this);
        adapter_service_ = nullptr;
    }
}

void Gs1GodotOverlayPanelController::cache_ui_references(Control& owner)
{
    owner_control_ = &owner;
    if (panel_ == nullptr)
    {
        panel_ = Object::cast_to<Control>(owner.find_child("OverlayPanel", true, false));
    }
    if (overlay_state_label_ == nullptr)
    {
        overlay_state_label_ = Object::cast_to<Label>(owner.find_child("OverlayStateLabel", true, false));
    }
    if (protection_selector_overlay_ == nullptr)
    {
        protection_selector_overlay_ = Object::cast_to<CenterContainer>(owner.find_child("SiteProtectionSelectorOverlay", true, false));
    }
    if (protection_selector_panel_ == nullptr)
    {
        protection_selector_panel_ = Object::cast_to<PanelContainer>(owner.find_child("SiteProtectionSelectorPanel", true, false));
    }
    if (protection_selector_title_ == nullptr)
    {
        protection_selector_title_ = Object::cast_to<Label>(owner.find_child("SiteProtectionSelectorTitle", true, false));
    }
    if (protection_selector_buttons_ == nullptr)
    {
        protection_selector_buttons_ = Object::cast_to<VBoxContainer>(owner.find_child("SiteProtectionSelectorButtons", true, false));
    }
    if (site_result_overlay_ == nullptr)
    {
        site_result_overlay_ = Object::cast_to<CenterContainer>(owner.find_child("SiteResultOverlay", true, false));
    }
    if (site_result_panel_ == nullptr)
    {
        site_result_panel_ = Object::cast_to<PanelContainer>(owner.find_child("SiteResultPanel", true, false));
    }
    if (site_result_title_ == nullptr)
    {
        site_result_title_ = Object::cast_to<Label>(owner.find_child("SiteResultTitle", true, false));
    }
    if (site_result_summary_ == nullptr)
    {
        site_result_summary_ = Object::cast_to<Label>(owner.find_child("SiteResultSummary", true, false));
    }
    if (site_result_actions_ == nullptr)
    {
        site_result_actions_ = Object::cast_to<VBoxContainer>(owner.find_child("SiteResultActions", true, false));
    }
    const std::int64_t controller_bits = static_cast<std::int64_t>(reinterpret_cast<std::uintptr_t>(this));
    const struct ButtonBinding final
    {
        const char* name;
        std::int64_t mode;
    } bindings[] {
        {"OverlayWindButton", 1},
        {"OverlayHeatButton", 2},
        {"OverlayDustButton", 3},
        {"OverlayConditionButton", 4},
        {"OverlayClearButton", 0},
    };
    for (const ButtonBinding& binding : bindings)
    {
        if (Button* button = Object::cast_to<Button>(owner.find_child(binding.name, true, false)))
        {
            const Callable callback = callable_mp_static(&dispatch_overlay_mode_pressed).bind(controller_bits, binding.mode);
            if (!button->is_connected("pressed", callback))
            {
                button->connect("pressed", callback);
            }
        }
    }
    refresh_if_needed();
}

void Gs1GodotOverlayPanelController::set_submit_ui_action_callback(SubmitUiActionFn callback)
{
    submit_ui_action_ = std::move(callback);
}

void Gs1GodotOverlayPanelController::cache_adapter_service()
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

Control* Gs1GodotOverlayPanelController::resolve_owner_control()
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

void Gs1GodotOverlayPanelController::submit_ui_action(
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

void Gs1GodotOverlayPanelController::handle_overlay_mode_pressed(std::int64_t mode)
{
    if (!submit_ui_action_)
    {
        return;
    }

    submit_ui_action_(
        k_ui_action_set_site_protection_overlay_mode,
        0,
        mode,
        0);
}

void Gs1GodotOverlayPanelController::handle_projected_button_pressed(std::int64_t button_key)
{
    const std::uint64_t key = static_cast<std::uint64_t>(button_key);
    Button* button = nullptr;
    auto protection_found = protection_selector_buttons_registry_.find(key);
    if (protection_found != protection_selector_buttons_registry_.end())
    {
        button = resolve_object<Button>(protection_found->second.object_id);
    }
    else
    {
        auto result_found = site_result_buttons_registry_.find(key);
        if (result_found != site_result_buttons_registry_.end())
        {
            button = resolve_object<Button>(result_found->second.object_id);
        }
    }
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

bool Gs1GodotOverlayPanelController::handles_engine_message(Gs1EngineMessageType type) const noexcept
{
    return type == GS1_ENGINE_MESSAGE_SITE_PROTECTION_OVERLAY_STATE ||
        type == GS1_ENGINE_MESSAGE_BEGIN_UI_SETUP ||
        type == GS1_ENGINE_MESSAGE_UI_ELEMENT_UPSERT ||
        type == GS1_ENGINE_MESSAGE_END_UI_SETUP ||
        type == GS1_ENGINE_MESSAGE_CLOSE_UI_SETUP ||
        type == GS1_ENGINE_MESSAGE_SET_APP_STATE;
}

void Gs1GodotOverlayPanelController::handle_engine_message(const Gs1EngineMessage& message)
{
    ui_setup_state_reducer_.apply_engine_message(message);
    switch (message.type)
    {
    case GS1_ENGINE_MESSAGE_SITE_PROTECTION_OVERLAY_STATE:
        overlay_state_ = message.payload_as<Gs1EngineMessageSiteProtectionOverlayData>();
        break;
    case GS1_ENGINE_MESSAGE_SET_APP_STATE:
    {
        const auto& payload = message.payload_as<Gs1EngineMessageSetAppStateData>();
        current_app_state_ = payload.app_state;
        if (payload.app_state == GS1_APP_STATE_MAIN_MENU ||
            payload.app_state == GS1_APP_STATE_REGIONAL_MAP ||
            payload.app_state == GS1_APP_STATE_CAMPAIGN_END)
        {
            overlay_state_.reset();
            ui_setup_state_reducer_.reset();
        }
        break;
    }
    default:
        break;
    }
    dirty_ = true;
    refresh_if_needed();
}

void Gs1GodotOverlayPanelController::handle_runtime_message_reset()
{
    current_app_state_.reset();
    overlay_state_.reset();
    ui_setup_state_reducer_.reset();
    protection_selector_buttons_registry_.clear();
    site_result_buttons_registry_.clear();
    dirty_ = true;
    refresh_if_needed();
}

void Gs1GodotOverlayPanelController::refresh_if_needed()
{
    if (!dirty_)
    {
        return;
    }

    const int app_state = current_app_state_.has_value() ? static_cast<int>(current_app_state_.value()) : 0;
    const bool panel_visible = app_state >= GS1_APP_STATE_SITE_LOADING && app_state <= GS1_APP_STATE_SITE_RESULT;
    if (panel_ != nullptr)
    {
        panel_->set_visible(panel_visible);
    }
    if (!panel_visible)
    {
        if (protection_selector_overlay_ != nullptr)
        {
            protection_selector_overlay_->set_visible(false);
        }
        if (site_result_overlay_ != nullptr)
        {
            site_result_overlay_->set_visible(false);
        }
        dirty_ = false;
        return;
    }
    if (overlay_state_label_ == nullptr)
    {
        return;
    }

    const int overlay_mode = overlay_state_.has_value()
        ? static_cast<int>(overlay_state_->mode)
        : 0;
    overlay_state_label_->set_text(vformat("Overlay Mode: %d", overlay_mode));
    const auto& setups = ui_setup_state_reducer_.setups();
    const auto find_setup = [&setups](Gs1UiSetupId setup_id) -> const Gs1RuntimeUiSetupProjection*
    {
        const auto found = std::find_if(setups.begin(), setups.end(), [&](const auto& setup) {
            return setup.setup_id == setup_id;
        });
        return found != setups.end() ? &(*found) : nullptr;
    };
    refresh_protection_selector(find_setup(GS1_UI_SETUP_SITE_PROTECTION_SELECTOR));
    refresh_site_result(find_setup(GS1_UI_SETUP_SITE_RESULT));
    dirty_ = false;
}

void Gs1GodotOverlayPanelController::refresh_protection_selector(const Gs1RuntimeUiSetupProjection* setup)
{
    if (protection_selector_overlay_ != nullptr)
    {
        protection_selector_overlay_->set_visible(setup != nullptr);
    }
    if (setup == nullptr)
    {
        prune_button_registry(protection_selector_buttons_registry_, {});
        return;
    }

    if (protection_selector_title_ != nullptr)
    {
        String title = "Protection Overlay";
        for (const auto& element : setup->elements)
        {
            if (element.element_type == GS1_UI_ELEMENT_LABEL)
            {
                title = protection_label(element);
                break;
            }
        }
        protection_selector_title_->set_text(title);
    }

    Array button_specs;
    for (const auto& element : setup->elements)
    {
        if (element.element_type != GS1_UI_ELEMENT_BUTTON)
        {
            continue;
        }
        Dictionary action;
        action["type"] = static_cast<int>(element.action.type);
        action["target_id"] = static_cast<int64_t>(element.action.target_id);
        action["arg0"] = static_cast<int64_t>(element.action.arg0);
        action["arg1"] = static_cast<int64_t>(element.action.arg1);
        Dictionary spec;
        spec["setup_id"] = static_cast<int>(setup->setup_id);
        spec["element_id"] = static_cast<int>(element.element_id);
        spec["text"] = protection_label(element);
        spec["flags"] = static_cast<int>(element.flags);
        spec["action"] = action;
        button_specs.push_back(spec);
    }
    reconcile_projected_buttons(protection_selector_buttons_, protection_selector_buttons_registry_, button_specs);
}

void Gs1GodotOverlayPanelController::refresh_site_result(const Gs1RuntimeUiSetupProjection* setup)
{
    if (site_result_overlay_ != nullptr)
    {
        site_result_overlay_->set_visible(setup != nullptr);
    }
    if (setup == nullptr)
    {
        prune_button_registry(site_result_buttons_registry_, {});
        return;
    }

    if (site_result_title_ != nullptr)
    {
        site_result_title_->set_text("Field Report");
    }
    if (site_result_summary_ != nullptr)
    {
        String summary = "Site result received.";
        for (const auto& element : setup->elements)
        {
            if (element.element_type == GS1_UI_ELEMENT_LABEL)
            {
                summary = site_result_text(element);
                break;
            }
        }
        site_result_summary_->set_text(summary);
    }

    Array button_specs;
    for (const auto& element : setup->elements)
    {
        if (element.element_type != GS1_UI_ELEMENT_BUTTON)
        {
            continue;
        }
        Dictionary action;
        action["type"] = static_cast<int>(element.action.type);
        action["target_id"] = static_cast<int64_t>(element.action.target_id);
        action["arg0"] = static_cast<int64_t>(element.action.arg0);
        action["arg1"] = static_cast<int64_t>(element.action.arg1);
        Dictionary spec;
        spec["setup_id"] = static_cast<int>(setup->setup_id);
        spec["element_id"] = static_cast<int>(element.element_id);
        spec["text"] = String("Return To Map");
        spec["flags"] = static_cast<int>(element.flags);
        spec["action"] = action;
        button_specs.push_back(spec);
    }
    reconcile_projected_buttons(site_result_actions_, site_result_buttons_registry_, button_specs);
}

void Gs1GodotOverlayPanelController::reconcile_projected_buttons(
    Node* container,
    std::unordered_map<std::uint64_t, ProjectedButtonRecord>& registry,
    const Array& button_specs)
{
    if (container == nullptr)
    {
        return;
    }
    std::unordered_set<std::uint64_t> desired_keys;
    for (int64_t index = 0; index < button_specs.size(); ++index)
    {
        const Dictionary spec = button_specs[index];
        const std::uint64_t stable_key = pack_u32_pair(
            static_cast<std::uint32_t>(as_int(spec.get("setup_id", 0), 0)),
            static_cast<std::uint32_t>(as_int(spec.get("element_id", 0), 0)));
        desired_keys.insert(stable_key);
        Button* button = upsert_button_node(container, registry, stable_key, vformat("Projected_%d", as_int(spec.get("element_id", 0), 0)), static_cast<int>(index));
        if (button == nullptr)
        {
            continue;
        }
        const Dictionary action = spec.get("action", Dictionary());
        button->set_text(String(spec.get("text", "Action")));
        button->set_disabled((as_int(spec.get("flags", 0), 0) & GS1_UI_ELEMENT_FLAG_DISABLED) != 0);
        button->set_meta("action_type", as_int(action.get("type", 0), 0));
        button->set_meta("target_id", as_int(action.get("target_id", 0), 0));
        button->set_meta("arg0", as_int(action.get("arg0", 0), 0));
        button->set_meta("arg1", as_int(action.get("arg1", 0), 0));
        const Callable callback = callable_mp_static(&dispatch_projected_button_pressed).bind(
            static_cast<std::int64_t>(reinterpret_cast<std::uintptr_t>(this)),
            static_cast<std::int64_t>(stable_key));
        if (!button->is_connected("pressed", callback))
        {
            button->connect("pressed", callback);
        }
    }
    prune_button_registry(registry, desired_keys);
}

Button* Gs1GodotOverlayPanelController::upsert_button_node(
    Node* container,
    std::unordered_map<std::uint64_t, ProjectedButtonRecord>& registry,
    std::uint64_t stable_key,
    const String& node_name,
    int desired_index)
{
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
        button->set_custom_minimum_size(Vector2(0.0F, 48.0F));
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

void Gs1GodotOverlayPanelController::prune_button_registry(
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

String Gs1GodotOverlayPanelController::protection_label(const Gs1RuntimeUiElementProjection& element) const
{
    if (element.content_kind == 3U)
    {
        return "Protection Overlay";
    }
    if (element.content_kind == 5U)
    {
        return "Close";
    }
    if (element.content_kind == 4U)
    {
        switch (element.primary_id)
        {
        case GS1_SITE_PROTECTION_OVERLAY_WIND:
            return "Wind";
        case GS1_SITE_PROTECTION_OVERLAY_HEAT:
            return "Heat";
        case GS1_SITE_PROTECTION_OVERLAY_DUST:
            return "Dust";
        case GS1_SITE_PROTECTION_OVERLAY_OCCUPANT_CONDITION:
            return "Condition";
        default:
            break;
        }
    }
    return "Action";
}

String Gs1GodotOverlayPanelController::site_result_text(const Gs1RuntimeUiElementProjection& element) const
{
    const String result = element.secondary_id == GS1_SITE_ATTEMPT_RESULT_COMPLETED
        ? String("completed")
        : (element.secondary_id == GS1_SITE_ATTEMPT_RESULT_FAILED ? String("failed") : String("resolved"));
    return vformat("Site %d %s. Return to the regional map to continue.", static_cast<int>(element.primary_id), result);
}
