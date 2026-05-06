#include "gs1_godot_action_panel_controller.h"

#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/core/object.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <godot_cpp/variant/dictionary.hpp>

#include <algorithm>
#include <array>

using namespace godot;

namespace
{
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
}

void Gs1GodotActionPanelController::cache_ui_references(Control& owner)
{
    owner_control_ = &owner;
    if (site_controls_ == nullptr)
    {
        site_controls_ = Object::cast_to<VBoxContainer>(owner.find_child("SiteControls", true, false));
    }
    cache_fixed_slot_bindings();
    refresh_fixed_slot_actions_if_needed();
    refresh_site_controls_if_needed();
}

void Gs1GodotActionPanelController::set_submit_ui_action_callback(SubmitUiActionFn callback)
{
    submit_ui_action_ = std::move(callback);
}

bool Gs1GodotActionPanelController::handles_engine_message(Gs1EngineMessageType type) const noexcept
{
    switch (type)
    {
    case GS1_ENGINE_MESSAGE_SET_APP_STATE:
    case GS1_ENGINE_MESSAGE_BEGIN_UI_SETUP:
    case GS1_ENGINE_MESSAGE_UI_ELEMENT_UPSERT:
    case GS1_ENGINE_MESSAGE_END_UI_SETUP:
    case GS1_ENGINE_MESSAGE_CLOSE_UI_SETUP:
    case GS1_ENGINE_MESSAGE_BEGIN_UI_PANEL:
    case GS1_ENGINE_MESSAGE_UI_PANEL_SLOT_ACTION_UPSERT:
    case GS1_ENGINE_MESSAGE_END_UI_PANEL:
    case GS1_ENGINE_MESSAGE_CLOSE_UI_PANEL:
        return true;
    default:
        return false;
    }
}

void Gs1GodotActionPanelController::handle_engine_message(const Gs1EngineMessage& message)
{
    ui_setup_state_reducer_.apply_engine_message(message);
    ui_panel_state_reducer_.apply_engine_message(message);

    switch (message.type)
    {
    case GS1_ENGINE_MESSAGE_SET_APP_STATE:
        current_app_state_ = message.payload_as<Gs1EngineMessageSetAppStateData>().app_state;
        fixed_slot_actions_dirty_ = true;
        site_controls_dirty_ = true;
        break;
    case GS1_ENGINE_MESSAGE_BEGIN_UI_SETUP:
    case GS1_ENGINE_MESSAGE_UI_ELEMENT_UPSERT:
    case GS1_ENGINE_MESSAGE_END_UI_SETUP:
    case GS1_ENGINE_MESSAGE_CLOSE_UI_SETUP:
        site_controls_dirty_ = true;
        break;
    case GS1_ENGINE_MESSAGE_BEGIN_UI_PANEL:
    case GS1_ENGINE_MESSAGE_UI_PANEL_SLOT_ACTION_UPSERT:
    case GS1_ENGINE_MESSAGE_END_UI_PANEL:
    case GS1_ENGINE_MESSAGE_CLOSE_UI_PANEL:
        fixed_slot_actions_dirty_ = true;
        break;
    default:
        break;
    }
    refresh_fixed_slot_actions_if_needed();
    refresh_site_controls_if_needed();
}

void Gs1GodotActionPanelController::handle_runtime_message_reset()
{
    ui_setup_state_reducer_.reset();
    ui_panel_state_reducer_.reset();
    current_app_state_.reset();
    fixed_slot_actions_dirty_ = true;
    site_controls_dirty_ = true;
    refresh_fixed_slot_actions_if_needed();
    refresh_site_controls_if_needed();
}

void Gs1GodotActionPanelController::refresh_fixed_slot_actions_if_needed()
{
    cache_fixed_slot_bindings();
    if (!fixed_slot_actions_dirty_)
    {
        return;
    }

    clear_fixed_slot_actions();
    bind_fixed_slot_actions(find_ui_panel(1), 1);
    bind_fixed_slot_actions(find_ui_panel(2), 2);
    bind_fixed_slot_actions(find_ui_panel(3), 3);
    fixed_slot_actions_dirty_ = false;
}

void Gs1GodotActionPanelController::refresh_site_controls_if_needed()
{
    if (!site_controls_dirty_)
    {
        return;
    }

    reconcile_site_control_buttons();
    site_controls_dirty_ = false;
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
        const Callable callback = Callable(owner_control_, "on_fixed_slot_pressed").bind(static_cast<std::int64_t>(binding.object_id));
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
    if (current_app_state_.has_value())
    {
        const int app_state = static_cast<int>(current_app_state_.value());
        if (app_state >= GS1_APP_STATE_SITE_LOADING && app_state <= GS1_APP_STATE_SITE_RESULT)
        {
            const auto& setups = ui_setup_state_reducer_.setups();
            for (const auto& setup : setups)
            {
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
        const Callable callback = owner_control_ == nullptr
            ? Callable()
            : Callable(owner_control_, "on_dynamic_site_action_pressed").bind(static_cast<int64_t>(stable_key));
        if (callback.is_valid() && !button->is_connected("pressed", callback))
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
    return ui_panel_state_reducer_.find_panel(static_cast<Gs1UiPanelId>(panel_id));
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
