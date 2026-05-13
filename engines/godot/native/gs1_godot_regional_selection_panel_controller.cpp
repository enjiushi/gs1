#include "gs1_godot_regional_selection_panel_controller.h"

#include "godot_progression_resources.h"
#include "gs1_godot_controller_context.h"
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

constexpr std::uint64_t pack_u32_pair(std::uint32_t high, std::uint32_t low) noexcept
{
    return (static_cast<std::uint64_t>(high) << 32U) | static_cast<std::uint64_t>(low);
}

std::uint64_t make_projected_button_key(int setup_id, int element_id) noexcept
{
    return pack_u32_pair(static_cast<std::uint32_t>(setup_id), static_cast<std::uint32_t>(element_id));
}

void sort_regional_map_sites(
    std::vector<Gs1GodotRegionalSelectionPanelController::RegionalSiteState>& sites)
{
    std::sort(sites.begin(), sites.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.site_id < rhs.site_id;
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
    refresh_from_game_state_view();
    rebuild_selection_panel();
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

void Gs1GodotRegionalSelectionPanelController::reset_regional_map_state() noexcept
{
    selected_site_id_ = 0;
    sites_.clear();
    selected_site_loadout_slots_.clear();
}

void Gs1GodotRegionalSelectionPanelController::refresh_from_game_state_view()
{
    reset_regional_map_state();
    if (adapter_service_ == nullptr)
    {
        return;
    }

    Gs1GameStateView view {};
    if (!adapter_service_->get_game_state_view(view) ||
        view.has_campaign == 0U ||
        view.campaign == nullptr)
    {
        return;
    }

    const Gs1CampaignStateView& campaign = *view.campaign;
    if (campaign.selected_site_id > 0)
    {
        selected_site_id_ = campaign.selected_site_id;
    }

    sites_.reserve(campaign.site_count);
    const Gs1CampaignSiteView* selected_site_view = nullptr;
    for (std::uint32_t site_index = 0; site_index < campaign.site_count; ++site_index)
    {
        const Gs1CampaignSiteView& site_view = campaign.sites[site_index];
        sites_.push_back(RegionalSiteState {
            site_view.site_id,
            site_view.site_state,
            site_view.regional_map_tile_x,
            site_view.regional_map_tile_y,
            site_view.support_package_id,
            site_view.exported_support_item_offset,
            site_view.exported_support_item_count,
            site_view.nearby_aura_modifier_id_count});
        if (selected_site_id_ != 0 && site_view.site_id == static_cast<std::uint32_t>(selected_site_id_))
        {
            selected_site_view = &site_view;
        }
    }

    if (selected_site_view != nullptr)
    {
        const std::uint32_t loadout_begin = selected_site_view->exported_support_item_offset;
        const std::uint32_t loadout_end = std::min(
            loadout_begin + selected_site_view->exported_support_item_count,
            campaign.site_exported_support_item_count);
        selected_site_loadout_slots_.reserve(loadout_end - loadout_begin);
        for (std::uint32_t index = loadout_begin; index < loadout_end; ++index)
        {
            selected_site_loadout_slots_.push_back(campaign.site_exported_support_items[index]);
        }
    }

    sort_regional_map_sites(sites_);
}

bool Gs1GodotRegionalSelectionPanelController::handles_engine_message(Gs1EngineMessageType type) const noexcept
{
    return type == GS1_ENGINE_MESSAGE_PRESENTATION_DIRTY ||
        type == GS1_ENGINE_MESSAGE_SET_APP_STATE;
}

void Gs1GodotRegionalSelectionPanelController::handle_engine_message(const Gs1EngineMessage& message)
{
    if (message.type == GS1_ENGINE_MESSAGE_PRESENTATION_DIRTY ||
        message.type == GS1_ENGINE_MESSAGE_SET_APP_STATE)
    {
        refresh_from_game_state_view();
        rebuild_selection_panel();
    }
}

void Gs1GodotRegionalSelectionPanelController::handle_runtime_message_reset()
{
    reset_regional_map_state();
    prune_button_registry(action_buttons_, {});
    prune_loadout_slot_registry({});
    rebuild_selection_panel();
}

const Gs1GodotRegionalSelectionPanelController::RegionalSiteState*
Gs1GodotRegionalSelectionPanelController::resolve_selected_site() const
{
    if (selected_site_id_ == 0)
    {
        return nullptr;
    }

    const auto found = std::find_if(sites_.begin(), sites_.end(), [&](const auto& site) {
        return site.site_id == static_cast<std::uint32_t>(selected_site_id_);
    });
    return found == sites_.end() ? nullptr : &(*found);
}

void Gs1GodotRegionalSelectionPanelController::apply_panel_visibility(const RegionalSiteState* selected_site)
{
    if (panel_ != nullptr)
    {
        panel_->set_visible(selected_site != nullptr);
    }
}

void Gs1GodotRegionalSelectionPanelController::apply_title_and_summary(const RegionalSiteState* selected_site)
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
    const RegionalSiteState* selected_site)
{
    Array button_specs;
    if (selected_site != nullptr)
    {
        Dictionary deploy_action;
        deploy_action["type"] = k_ui_action_start_site_attempt;
        deploy_action["target_id"] = static_cast<int>(selected_site->site_id);
        deploy_action["arg0"] = 0;
        deploy_action["arg1"] = 0;

        Dictionary deploy_spec;
        deploy_spec["setup_id"] = -1;
        deploy_spec["element_id"] = k_ui_action_start_site_attempt;
        deploy_spec["text"] = vformat("Deploy To Site %d", static_cast<int>(selected_site->site_id));
        deploy_spec["flags"] = selected_site->site_state == GS1_SITE_STATE_AVAILABLE ? 0 : 2;
        deploy_spec["action"] = deploy_action;
        button_specs.push_back(deploy_spec);
    }

    reconcile_action_buttons(button_specs);
    refresh_support_summary(selected_site);
    refresh_loadout_panel();
}

void Gs1GodotRegionalSelectionPanelController::rebuild_selection_panel()
{
    if (panel_ == nullptr)
    {
        return;
    }

    const RegionalSiteState* selected_site = resolve_selected_site();
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

String Gs1GodotRegionalSelectionPanelController::site_button_text(const RegionalSiteState& site) const
{
    const int site_id = static_cast<int>(site.site_id);
    const String coords = vformat("(%d, %d)", site.grid_x, site.grid_y);
    return vformat("Site %d  %s  %s", site_id, site_state_name(site.site_state), coords);
}

String Gs1GodotRegionalSelectionPanelController::site_tooltip(const RegionalSiteState& site) const
{
    return vformat(
        "State: %s  Support Package: %d  Active Preview: %s",
        site_state_name(site.site_state),
        static_cast<int>(site.support_package_id),
        support_preview_text(site));
}

String Gs1GodotRegionalSelectionPanelController::site_state_name(Gs1SiteState site_state) const
{
    switch (site_state)
    {
    case GS1_SITE_STATE_AVAILABLE:
        return "Available";
    case GS1_SITE_STATE_COMPLETED:
        return "Completed";
    default:
        return "Locked";
    }
}

String Gs1GodotRegionalSelectionPanelController::site_deployment_summary(const RegionalSiteState& site) const
{
    const String state_name = site_state_name(site.site_state);
    const Vector2i grid = regional_grid_coord(site);
    return vformat(
        "Status: %s\nGrid: (%d, %d)\nNearby support: %s\nLoadout package channel: %d\nSelect this node, review the support feed, then deploy from this panel.",
        state_name,
        grid.x,
        grid.y,
        support_preview_text(site),
        static_cast<int>(site.support_package_id));
}

String Gs1GodotRegionalSelectionPanelController::support_preview_text(const RegionalSiteState& site) const
{
    PackedStringArray support_lines;
    if (site.exported_support_item_count > 0U)
    {
        support_lines.push_back(vformat("loadout items x%d", static_cast<int>(site.exported_support_item_count)));
    }
    if (site.nearby_aura_modifier_id_count > 0U)
    {
        support_lines.push_back(vformat("aura boosts x%d", static_cast<int>(site.nearby_aura_modifier_id_count)));
    }
    return support_lines.is_empty() ? String("None") : String(", ").join(support_lines);
}

Vector2i Gs1GodotRegionalSelectionPanelController::regional_grid_coord(const RegionalSiteState& site) const
{
    return Vector2i(site.grid_x, site.grid_y);
}

void Gs1GodotRegionalSelectionPanelController::refresh_support_summary(const RegionalSiteState* selected_site)
{
    if (support_summary_ == nullptr)
    {
        return;
    }

    PackedStringArray lines;
    lines.push_back("[b]Support Feed[/b]");
    if (selected_site != nullptr)
    {
        if (selected_site->exported_support_item_count > 0U)
        {
            lines.push_back(vformat(
                "Exported support items x%d",
                static_cast<int>(selected_site->exported_support_item_count)));
        }
        if (selected_site->nearby_aura_modifier_id_count > 0U)
        {
            lines.push_back(vformat(
                "Nearby aura modifiers x%d",
                static_cast<int>(selected_site->nearby_aura_modifier_id_count)));
        }
    }

    if (lines.size() == 1)
    {
        lines.push_back("No adjacent support contributors or aura boosts are projected for this route.");
    }

    support_summary_->set_text(String("\n").join(lines));
}

void Gs1GodotRegionalSelectionPanelController::refresh_loadout_panel()
{
    if (loadout_title_ != nullptr)
    {
        loadout_title_->set_text("Provided Loadout");
    }

    if (loadout_slots_grid_ == nullptr)
    {
        return;
    }

    const bool has_content = !selected_site_loadout_slots_.empty();
    loadout_slots_grid_->set_visible(has_content);
    if (loadout_empty_label_ != nullptr)
    {
        loadout_empty_label_->set_visible(!has_content);
        loadout_empty_label_->set_text("No exported support items are available for the currently selected site.");
    }

    if (!has_content)
    {
        prune_loadout_slot_registry({});
        return;
    }

    loadout_slots_grid_->set_columns(std::clamp<int>(static_cast<int>(selected_site_loadout_slots_.size()), 1, 4));

    std::unordered_set<std::uint64_t> desired_keys {};
    for (std::size_t index = 0; index < selected_site_loadout_slots_.size(); ++index)
    {
        const Gs1LoadoutSlotView& item = selected_site_loadout_slots_[index];
        const auto stable_key = loadout_slot_key(item.item_id, static_cast<std::uint32_t>(index));
        desired_keys.insert(stable_key);
        Button* button = upsert_loadout_slot_button(
            stable_key,
            vformat("LoadoutSlot_%d_%d", static_cast<int>(item.item_id), static_cast<int>(index)),
            static_cast<int>(index));
        if (button == nullptr)
        {
            continue;
        }

        const String item_name = item_name_for(static_cast<int>(item.item_id));
        const String quantity_text = item.quantity > 1U
            ? vformat("x%d", static_cast<int>(item.quantity))
            : String();
        button->set_text(quantity_text);
        button->set_button_icon(item_icon_for(item.item_id));
        button->set_tooltip_text(vformat(
            "Support Slot %d: %s x%d",
            static_cast<int>(index + 1U),
            item_name,
            static_cast<int>(item.quantity)));
    }

    prune_loadout_slot_registry(desired_keys);
}

Ref<Texture2D> Gs1GodotRegionalSelectionPanelController::item_icon_for(std::uint32_t item_id) const
{
    return load_cached_texture(
        GodotProgressionResourceDatabase::instance().content_icon_path(1U, item_id));
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

std::uint64_t Gs1GodotRegionalSelectionPanelController::loadout_slot_key(
    std::uint32_t item_id,
    std::uint32_t slot_index) const noexcept
{
    return pack_u32_pair(item_id, slot_index);
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
