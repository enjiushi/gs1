#include "gs1_godot_overlay_panel_controller.h"

#include "gs1_godot_controller_context.h"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/core/object.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <godot_cpp/variant/dictionary.hpp>

#include <algorithm>

using namespace godot;

namespace
{
constexpr std::int64_t k_ui_action_return_to_regional_map = 4;
constexpr std::int64_t k_ui_action_close_site_protection_ui = 25;
constexpr std::int64_t k_ui_action_set_site_protection_overlay_mode = 26;

constexpr std::uint64_t k_site_result_return_button_key = 1U;

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
}  // namespace

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
        {"OverlayWindButton", GS1_SITE_PROTECTION_OVERLAY_WIND},
        {"OverlayHeatButton", GS1_SITE_PROTECTION_OVERLAY_HEAT},
        {"OverlayDustButton", GS1_SITE_PROTECTION_OVERLAY_DUST},
        {"OverlayConditionButton", GS1_SITE_PROTECTION_OVERLAY_OCCUPANT_CONDITION},
        {"OverlayClearButton", GS1_SITE_PROTECTION_OVERLAY_NONE},
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

    update_overlay_state_label();
    update_protection_selector();
    update_site_result();
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
    if (const auto found = protection_selector_buttons_registry_.find(key);
        found != protection_selector_buttons_registry_.end())
    {
        button = resolve_object<Button>(found->second.object_id);
    }
    else if (const auto found = site_result_buttons_registry_.find(key);
             found != site_result_buttons_registry_.end())
    {
        button = resolve_object<Button>(found->second.object_id);
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
    switch (type)
    {
    case GS1_ENGINE_MESSAGE_PRESENTATION_DIRTY:
    case GS1_ENGINE_MESSAGE_SET_APP_STATE:
    case GS1_ENGINE_MESSAGE_SITE_RESULT_READY:
        return true;
    default:
        return false;
    }
}

void Gs1GodotOverlayPanelController::handle_engine_message(const Gs1EngineMessage& message)
{
    switch (message.type)
    {
    case GS1_ENGINE_MESSAGE_PRESENTATION_DIRTY:
    case GS1_ENGINE_MESSAGE_SET_APP_STATE:
        break;
    case GS1_ENGINE_MESSAGE_SITE_RESULT_READY:
        site_result_ = message.payload_as<Gs1EngineMessageSiteResultData>();
        break;
    default:
        break;
    }

    if (adapter_service_ != nullptr)
    {
        Gs1GameStateView view {};
        if (adapter_service_->get_game_state_view(view) && view.app_state != GS1_APP_STATE_SITE_RESULT)
        {
            site_result_.reset();
        }
    }

    update_overlay_state_label();
    update_protection_selector();
    update_site_result();
}

void Gs1GodotOverlayPanelController::handle_runtime_message_reset()
{
    site_result_.reset();
    protection_selector_buttons_registry_.clear();
    site_result_buttons_registry_.clear();
    update_overlay_state_label();
    update_protection_selector();
    update_site_result();
}

void Gs1GodotOverlayPanelController::update_overlay_state_label()
{
    if (overlay_state_label_ == nullptr)
    {
        return;
    }

    const auto mode =
        adapter_service_ != nullptr
            ? adapter_service_->ui_session_state().protection.overlay_mode
            : GS1_SITE_PROTECTION_OVERLAY_NONE;
    overlay_state_label_->set_text(vformat("Overlay Mode: %s", protection_mode_label(mode)));
}

void Gs1GodotOverlayPanelController::update_protection_selector()
{
    const auto protection =
        adapter_service_ != nullptr
            ? adapter_service_->ui_session_state().protection
            : Gs1GodotProtectionUiSessionState {};

    if (protection_selector_overlay_ != nullptr)
    {
        protection_selector_overlay_->set_visible(protection.selector_open);
    }

    if (!protection.selector_open)
    {
        prune_button_registry(protection_selector_buttons_registry_, {});
        return;
    }

    if (protection_selector_title_ != nullptr)
    {
        protection_selector_title_->set_text("Protection Overlay");
    }

    Array button_specs;
    const struct SelectorButtonSpec final
    {
        std::uint32_t element_id;
        Gs1SiteProtectionOverlayMode mode;
        std::int64_t action_type;
    } button_defs[] {
        {1U, GS1_SITE_PROTECTION_OVERLAY_WIND, k_ui_action_set_site_protection_overlay_mode},
        {2U, GS1_SITE_PROTECTION_OVERLAY_HEAT, k_ui_action_set_site_protection_overlay_mode},
        {3U, GS1_SITE_PROTECTION_OVERLAY_DUST, k_ui_action_set_site_protection_overlay_mode},
        {4U, GS1_SITE_PROTECTION_OVERLAY_OCCUPANT_CONDITION, k_ui_action_set_site_protection_overlay_mode},
        {5U, GS1_SITE_PROTECTION_OVERLAY_NONE, k_ui_action_close_site_protection_ui},
    };
    for (const SelectorButtonSpec& button_def : button_defs)
    {
        Dictionary action;
        action["type"] = static_cast<int>(button_def.action_type);
        action["target_id"] = 0;
        action["arg0"] = static_cast<int64_t>(
            button_def.action_type == k_ui_action_set_site_protection_overlay_mode
                ? button_def.mode
                : 0);
        action["arg1"] = 0;

        Dictionary spec;
        spec["setup_id"] = static_cast<int>(GS1_UI_SETUP_SITE_PROTECTION_SELECTOR);
        spec["element_id"] = static_cast<int>(button_def.element_id);
        spec["text"] = button_def.action_type == k_ui_action_close_site_protection_ui
            ? String("Close")
            : protection_mode_label(button_def.mode);
        spec["flags"] = 0;
        spec["action"] = action;
        button_specs.push_back(spec);
    }
    reconcile_projected_buttons(
        protection_selector_buttons_,
        protection_selector_buttons_registry_,
        button_specs);
}

void Gs1GodotOverlayPanelController::update_site_result()
{
    const bool visible = site_result_.has_value();
    if (site_result_overlay_ != nullptr)
    {
        site_result_overlay_->set_visible(visible);
    }

    if (!visible)
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
        site_result_summary_->set_text(site_result_text());
    }

    Dictionary action;
    action["type"] = static_cast<int>(k_ui_action_return_to_regional_map);
    action["target_id"] = 0;
    action["arg0"] = 0;
    action["arg1"] = 0;

    Dictionary spec;
    spec["setup_id"] = static_cast<int>(GS1_UI_SETUP_SITE_RESULT);
    spec["element_id"] = static_cast<int>(k_site_result_return_button_key);
    spec["text"] = String("Return To Map");
    spec["flags"] = 0;
    spec["action"] = action;

    Array button_specs;
    button_specs.push_back(spec);
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
        const std::uint64_t stable_key =
            (static_cast<std::uint64_t>(static_cast<std::uint32_t>(as_int(spec.get("setup_id", 0), 0))) << 32U) |
            static_cast<std::uint32_t>(as_int(spec.get("element_id", 0), 0));
        desired_keys.insert(stable_key);

        Button* button = upsert_button_node(
            container,
            registry,
            stable_key,
            vformat("Projected_%d", as_int(spec.get("element_id", 0), 0)),
            static_cast<int>(index));
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
    if (const auto found = registry.find(stable_key); found != registry.end())
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
    const int clamped_index = std::clamp(desired_index, 0, std::max(0, child_count - 1));
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

String Gs1GodotOverlayPanelController::protection_mode_label(Gs1SiteProtectionOverlayMode mode) const
{
    switch (mode)
    {
    case GS1_SITE_PROTECTION_OVERLAY_WIND:
        return "Wind";
    case GS1_SITE_PROTECTION_OVERLAY_HEAT:
        return "Heat";
    case GS1_SITE_PROTECTION_OVERLAY_DUST:
        return "Dust";
    case GS1_SITE_PROTECTION_OVERLAY_OCCUPANT_CONDITION:
        return "Condition";
    case GS1_SITE_PROTECTION_OVERLAY_NONE:
    default:
        return "Off";
    }
}

String Gs1GodotOverlayPanelController::site_result_text() const
{
    if (!site_result_.has_value())
    {
        return "Site result received.";
    }

    const String result = site_result_->result == GS1_SITE_ATTEMPT_RESULT_COMPLETED
        ? String("completed")
        : (site_result_->result == GS1_SITE_ATTEMPT_RESULT_FAILED ? String("failed") : String("resolved"));
    if (site_result_->newly_revealed_site_count > 0U)
    {
        return vformat(
            "Site %d %s. %d new site%s revealed. Return to the regional map to continue.",
            static_cast<int>(site_result_->site_id),
            result,
            static_cast<int>(site_result_->newly_revealed_site_count),
            site_result_->newly_revealed_site_count == 1U ? "" : "s");
    }

    return vformat(
        "Site %d %s. Return to the regional map to continue.",
        static_cast<int>(site_result_->site_id),
        result);
}
