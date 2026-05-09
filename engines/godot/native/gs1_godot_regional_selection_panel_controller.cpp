#include "gs1_godot_regional_selection_panel_controller.h"

#include "gs1_godot_controller_context.h"
#include "content/defs/faction_defs.h"
#include "content/defs/item_defs.h"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/core/object.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>

#include <cstdint>
#include <string_view>

using namespace godot;

namespace
{
constexpr int k_ui_action_start_site_attempt = 3;
constexpr int k_ui_action_clear_deployment_site_selection = 5;

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

String string_from_view(std::string_view value)
{
    return String::utf8(value.data(), static_cast<int>(value.size()));
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
    set_process(true);
}

void Gs1GodotRegionalSelectionPanelController::_process(double delta)
{
    (void)delta;
    refresh_if_needed();
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
    if (loadout_summary_ == nullptr)
    {
        loadout_summary_ = Object::cast_to<RichTextLabel>(owner.find_child("RegionalLoadoutSummary", true, false));
    }
    if (actions_ == nullptr)
    {
        actions_ = Object::cast_to<VBoxContainer>(owner.find_child("RegionalSelectionActions", true, false));
    }
    refresh_if_needed();
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

bool Gs1GodotRegionalSelectionPanelController::handles_engine_message(Gs1EngineMessageType type) const noexcept
{
    switch (type)
    {
    case GS1_ENGINE_MESSAGE_SET_APP_STATE:
    case GS1_ENGINE_MESSAGE_BEGIN_REGIONAL_MAP_SNAPSHOT:
    case GS1_ENGINE_MESSAGE_REGIONAL_MAP_SITE_UPSERT:
    case GS1_ENGINE_MESSAGE_REGIONAL_MAP_SITE_REMOVE:
    case GS1_ENGINE_MESSAGE_REGIONAL_MAP_LINK_UPSERT:
    case GS1_ENGINE_MESSAGE_REGIONAL_MAP_LINK_REMOVE:
    case GS1_ENGINE_MESSAGE_END_REGIONAL_MAP_SNAPSHOT:
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

void Gs1GodotRegionalSelectionPanelController::handle_engine_message(const Gs1EngineMessage& message)
{
    ui_panel_state_reducer_.apply_engine_message(message);
    regional_map_state_reducer_.apply_engine_message(message);

    switch (message.type)
    {
    case GS1_ENGINE_MESSAGE_SET_APP_STATE:
    {
        current_app_state_ = message.payload_as<Gs1EngineMessageSetAppStateData>().app_state;
        if (current_app_state_ != GS1_APP_STATE_REGIONAL_MAP &&
            current_app_state_ != GS1_APP_STATE_SITE_LOADING)
        {
            selected_site_id_ = 0;
        }
        break;
    }
    case GS1_ENGINE_MESSAGE_BEGIN_REGIONAL_MAP_SNAPSHOT:
    case GS1_ENGINE_MESSAGE_REGIONAL_MAP_SITE_UPSERT:
    case GS1_ENGINE_MESSAGE_REGIONAL_MAP_SITE_REMOVE:
    case GS1_ENGINE_MESSAGE_REGIONAL_MAP_LINK_UPSERT:
    case GS1_ENGINE_MESSAGE_REGIONAL_MAP_LINK_REMOVE:
    case GS1_ENGINE_MESSAGE_END_REGIONAL_MAP_SNAPSHOT:
        if (regional_map_state_reducer_.state().selected_site_id.has_value())
        {
            selected_site_id_ = static_cast<int>(regional_map_state_reducer_.state().selected_site_id.value());
        }
        break;
    default:
        break;
    }

    dirty_ = true;
    refresh_if_needed();
}

void Gs1GodotRegionalSelectionPanelController::handle_runtime_message_reset()
{
    ui_panel_state_reducer_.reset();
    regional_map_state_reducer_.reset();
    current_app_state_.reset();
    selected_site_id_ = 0;
    dirty_ = true;
    refresh_if_needed();
}

void Gs1GodotRegionalSelectionPanelController::refresh_if_needed()
{
    if (!dirty_ || panel_ == nullptr)
    {
        return;
    }

    const int app_state = current_app_state_.has_value() ? static_cast<int>(current_app_state_.value()) : 0;
    if (app_state != GS1_APP_STATE_REGIONAL_MAP && app_state != GS1_APP_STATE_SITE_LOADING)
    {
        panel_->set_visible(false);
        dirty_ = false;
        return;
    }

    const auto& regional_state = regional_map_state_reducer_.state();
    const Gs1RuntimeRegionalMapSiteProjection* selected_site = nullptr;
    for (const auto& site : regional_state.sites)
    {
        if (static_cast<int>(site.site_id) == selected_site_id_)
        {
            selected_site = &site;
            break;
        }
    }

    if (selected_site == nullptr && !regional_state.sites.empty())
    {
        selected_site = &regional_state.sites.front();
        selected_site_id_ = static_cast<int>(selected_site->site_id);
    }

    if (selected_site == nullptr)
    {
        panel_->set_visible(false);
        dirty_ = false;
        return;
    }

    panel_->set_visible(true);
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

    Array button_specs;
    const Gs1RuntimeUiPanelProjection* selection_panel = ui_panel_state_reducer_.find_panel(GS1_UI_PANEL_REGIONAL_MAP_SELECTION);
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
            const String text = panel_slot_label(slot_action);
            if (action_type == k_ui_action_clear_deployment_site_selection && text.is_empty())
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

    if (button_specs.is_empty())
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

        Dictionary clear_action;
        clear_action["type"] = k_ui_action_clear_deployment_site_selection;
        clear_action["target_id"] = 0;
        clear_action["arg0"] = 0;
        clear_action["arg1"] = 0;

        Dictionary clear_spec;
        clear_spec["setup_id"] = -1;
        clear_spec["element_id"] = k_ui_action_clear_deployment_site_selection;
        clear_spec["text"] = "Clear Selection";
        clear_spec["flags"] = 0;
        clear_spec["action"] = clear_action;
        button_specs.push_back(clear_spec);
    }

    reconcile_action_buttons(button_specs);

    if (summary_ != nullptr)
    {
        summary_->set_text(String("\n").join(summary_lines));
    }
    refresh_support_summary(selection_panel);
    refresh_loadout_summary(selection_panel);

    dirty_ = false;
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

void Gs1GodotRegionalSelectionPanelController::refresh_loadout_summary(
    const Gs1RuntimeUiPanelProjection* selection_panel)
{
    if (loadout_summary_ == nullptr)
    {
        return;
    }

    PackedStringArray lines;
    lines.push_back("[b]Provided Loadout[/b]");
    bool has_content = false;
    if (selection_panel != nullptr)
    {
        for (const auto& item : selection_panel->list_items)
        {
            if (item.list_id != GS1_UI_PANEL_LIST_REGIONAL_LOADOUT)
            {
                continue;
            }

            const String item_name = item_name_for(static_cast<int>(item.primary_id));
            const int quantity = static_cast<int>(item.quantity);
            lines.push_back(vformat("Slot %d: %s x%d", static_cast<int>(item.item_id), item_name, quantity));
            has_content = true;
        }
    }

    if (!has_content)
    {
        lines.push_back("No extra loadout items are projected for the currently selected site.");
    }

    loadout_summary_->set_text(String("\n").join(lines));
}
