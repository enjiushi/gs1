#include "gs1_godot_regional_tech_tree_panel_controller.h"

#include "godot_progression_resources.h"

#include "content/defs/faction_defs.h"
#include "content/defs/technology_defs.h"
#include "host/adapter_metadata_catalog.h"

#include <godot_cpp/classes/center_container.hpp>
#include <godot_cpp/classes/color_rect.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/margin_container.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/panel.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/texture_rect.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/core/object.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <godot_cpp/variant/dictionary.hpp>

#include <algorithm>
#include <map>
#include <string_view>

using namespace godot;

namespace
{
constexpr std::uint8_t k_unlockable_content_kind_plant = 0U;
constexpr std::uint8_t k_unlockable_content_kind_item = 1U;
constexpr std::uint8_t k_unlockable_content_kind_structure_recipe = 2U;
constexpr std::uint8_t k_unlockable_content_kind_recipe = 3U;

constexpr int k_tech_tree_lane_count = 5;
constexpr int k_tech_tree_unlockable_column = 1;
constexpr int k_tech_tree_village_column = 2;
constexpr int k_tech_tree_bureau_column = 3;
constexpr int k_tech_tree_university_column = 4;

constexpr std::uint64_t pack_u32_pair(std::uint32_t high, std::uint32_t low) noexcept
{
    return (static_cast<std::uint64_t>(high) << 32U) | static_cast<std::uint64_t>(low);
}

std::uint64_t make_projected_button_key(int setup_id, int element_id) noexcept
{
    return pack_u32_pair(static_cast<std::uint32_t>(setup_id), static_cast<std::uint32_t>(element_id));
}

std::uint64_t make_tech_tree_marker_key(std::uint32_t row_requirement, std::uint32_t column_index) noexcept
{
    return pack_u32_pair(row_requirement, column_index);
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

double as_float(const Variant& value, double fallback = 0.0)
{
    if (value.get_type() == Variant::NIL)
    {
        return fallback;
    }
    switch (value.get_type())
    {
    case Variant::FLOAT:
        return double(value);
    case Variant::INT:
        return static_cast<double>(int64_t(value));
    case Variant::BOOL:
        return bool(value) ? 1.0 : 0.0;
    default:
        return fallback;
    }
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

Ref<ImageTexture> make_solid_icon_texture(const Color& base_color)
{
    Ref<Image> image = Image::create_empty(48, 48, false, Image::FORMAT_RGBA8);
    if (image.is_null())
    {
        return Ref<ImageTexture> {};
    }

    image->fill(base_color);
    image->fill_rect(Rect2i(4, 4, 40, 40), Color(base_color.r * 0.72f, base_color.g * 0.72f, base_color.b * 0.72f, 1.0f));
    image->fill_rect(Rect2i(8, 8, 32, 32), Color(base_color.r * 1.10f, base_color.g * 1.10f, base_color.b * 1.10f, 1.0f));
    image->fill_rect(Rect2i(12, 12, 24, 24), Color(base_color.r * 0.56f, base_color.g * 0.56f, base_color.b * 0.56f, 1.0f));
    return ImageTexture::create_from_image(image);
}

std::uint8_t unlockable_content_kind_from_def(const gs1::ReputationUnlockDef& unlock_def)
{
    switch (unlock_def.unlock_kind)
    {
    case gs1::ReputationUnlockKind::Plant:
        return k_unlockable_content_kind_plant;
    case gs1::ReputationUnlockKind::Item:
        return k_unlockable_content_kind_item;
    case gs1::ReputationUnlockKind::StructureRecipe:
        return k_unlockable_content_kind_structure_recipe;
    case gs1::ReputationUnlockKind::Recipe:
        return k_unlockable_content_kind_recipe;
    default:
        return k_unlockable_content_kind_item;
    }
}
}

void Gs1GodotRegionalTechTreePanelController::cache_ui_references(Control& owner)
{
    owner_control_ = &owner;
    if (overlay_ == nullptr)
    {
        overlay_ = Object::cast_to<Control>(owner.find_child("TechOverlay", true, false));
    }
    if (panel_ == nullptr)
    {
        panel_ = Object::cast_to<PanelContainer>(owner.find_child("RegionalTechTreePanel", true, false));
    }
    if (title_ == nullptr)
    {
        title_ = Object::cast_to<Label>(owner.find_child("RegionalTechTreeTitle", true, false));
    }
    if (summary_scroll_ == nullptr)
    {
        summary_scroll_ = Object::cast_to<ScrollContainer>(owner.find_child("RegionalTechTreeSummary", true, false));
    }
    if (actions_ == nullptr)
    {
        actions_ = Object::cast_to<GridContainer>(owner.find_child("RegionalTechTreeActions", true, false));
    }
    apply_overlay_layout();
    refresh_if_needed();
}

void Gs1GodotRegionalTechTreePanelController::set_submit_ui_action_callback(SubmitUiActionFn callback)
{
    submit_ui_action_ = std::move(callback);
}

bool Gs1GodotRegionalTechTreePanelController::is_panel_visible() const
{
    return panel_ != nullptr && panel_->is_visible();
}

bool Gs1GodotRegionalTechTreePanelController::handles_engine_message(Gs1EngineMessageType type) const noexcept
{
    switch (type)
    {
    case GS1_ENGINE_MESSAGE_SET_APP_STATE:
    case GS1_ENGINE_MESSAGE_BEGIN_PROGRESSION_VIEW:
    case GS1_ENGINE_MESSAGE_PROGRESSION_ENTRY_UPSERT:
    case GS1_ENGINE_MESSAGE_END_PROGRESSION_VIEW:
    case GS1_ENGINE_MESSAGE_CLOSE_PROGRESSION_VIEW:
        return true;
    default:
        return false;
    }
}

void Gs1GodotRegionalTechTreePanelController::handle_engine_message(const Gs1EngineMessage& message)
{
    progression_view_state_reducer_.apply_engine_message(message);

    if (message.type == GS1_ENGINE_MESSAGE_SET_APP_STATE)
    {
        current_app_state_ = message.payload_as<Gs1EngineMessageSetAppStateData>().app_state;
    }

    dirty_ = true;
    refresh_if_needed();
}

void Gs1GodotRegionalTechTreePanelController::handle_runtime_message_reset()
{
    progression_view_state_reducer_.reset();
    current_app_state_.reset();
    dirty_ = true;
    refresh_if_needed();
}

void Gs1GodotRegionalTechTreePanelController::refresh_if_needed()
{
    if (!dirty_ || overlay_ == nullptr || panel_ == nullptr)
    {
        return;
    }

    const int app_state = current_app_state_.has_value() ? static_cast<int>(current_app_state_.value()) : 0;
    if (app_state != GS1_APP_STATE_REGIONAL_MAP && app_state != GS1_APP_STATE_SITE_LOADING)
    {
        overlay_->set_visible(false);
        panel_->set_visible(false);
        dirty_ = false;
        return;
    }

    const Gs1RuntimeProgressionViewProjection* progression_view =
        progression_view_state_reducer_.find_view(GS1_PROGRESSION_VIEW_REGIONAL_MAP_TECH_TREE);
    if (progression_view == nullptr)
    {
        overlay_->set_visible(false);
        panel_->set_visible(false);
        dirty_ = false;
        return;
    }

    overlay_->set_visible(true);
    panel_->set_visible(true);

    if (title_ != nullptr)
    {
        title_->set_text("Research And Unlocks");
    }

    std::map<int, Dictionary> unlockable_specs_by_rep;
    std::map<int, Dictionary> village_specs_by_rep;
    std::map<int, Dictionary> bureau_specs_by_rep;
    std::map<int, Dictionary> university_specs_by_rep;
    std::vector<int> row_requirements;
    for (const auto& entry : progression_view->entries)
    {
        const int entry_kind = static_cast<int>(entry.entry_kind);
        const int reputation_requirement = static_cast<int>(entry.reputation_requirement);
        if (entry_kind == GS1_PROGRESSION_ENTRY_NONE)
        {
            continue;
        }

        if (reputation_requirement > 0)
        {
            row_requirements.push_back(reputation_requirement);
        }

        if (entry_kind == GS1_PROGRESSION_ENTRY_TECHNOLOGY_NODE)
        {
            Dictionary action;
            action["type"] = static_cast<int>(entry.action.type);
            action["target_id"] = static_cast<int64_t>(entry.action.target_id);
            action["arg0"] = static_cast<int64_t>(entry.action.arg0);
            action["arg1"] = static_cast<int64_t>(entry.action.arg1);
            Dictionary spec;
            spec["setup_id"] = GS1_PROGRESSION_VIEW_REGIONAL_MAP_TECH_TREE;
            spec["element_id"] = static_cast<int>(entry.entry_id);
            spec["entry_kind"] = entry_kind;
            spec["flags"] = static_cast<int>(entry.flags);
            spec["action"] = action;
            spec["reputation_requirement"] = reputation_requirement;
            spec["tech_node_id"] = static_cast<int>(entry.tech_node_id);
            spec["faction_id"] = static_cast<int>(entry.faction_id);
            spec["tier_index"] = static_cast<int>(entry.tier_index);
            spec["tooltip"] = tech_tooltip_text(spec);
            switch (static_cast<int>(entry.faction_id))
            {
            case gs1::k_faction_village_committee:
                village_specs_by_rep[reputation_requirement] = spec;
                break;
            case gs1::k_faction_forestry_bureau:
                bureau_specs_by_rep[reputation_requirement] = spec;
                break;
            case gs1::k_faction_agricultural_university:
                university_specs_by_rep[reputation_requirement] = spec;
                break;
            default:
                break;
            }
            continue;
        }

        if (entry_kind == GS1_PROGRESSION_ENTRY_REPUTATION_UNLOCK && reputation_requirement > 0)
        {
            Dictionary spec;
            spec["setup_id"] = GS1_PROGRESSION_VIEW_REGIONAL_MAP_TECH_TREE;
            spec["element_id"] = static_cast<int>(entry.entry_id);
            spec["entry_kind"] = entry_kind;
            spec["flags"] = static_cast<int>(entry.flags);
            spec["reputation_requirement"] = reputation_requirement;
            spec["content_id"] = static_cast<int>(entry.content_id);
            spec["content_kind"] = static_cast<int>(entry.content_kind);
            spec["tooltip"] = unlockable_tooltip_text(spec);
            unlockable_specs_by_rep[reputation_requirement] = spec;
        }
    }

    std::sort(row_requirements.begin(), row_requirements.end());
    row_requirements.erase(std::unique(row_requirements.begin(), row_requirements.end()), row_requirements.end());

    Array ladder_specs;
    for (std::size_t row_index = 0; row_index < row_requirements.size(); ++row_index)
    {
        const int requirement = row_requirements[row_index];

        Dictionary rep_spec;
        rep_spec["setup_id"] = 0;
        rep_spec["element_id"] = 0;
        rep_spec["rep_requirement"] = requirement;
        rep_spec["cell_kind"] = String("rep_label");
        rep_spec["marker_text"] = vformat("%d rep", requirement);
        rep_spec["marker_key_high"] = requirement;
        rep_spec["marker_key_low"] = 0;
        ladder_specs.push_back(rep_spec);

        auto push_lane = [&ladder_specs, requirement](const std::map<int, Dictionary>& specs, int column_index)
        {
            auto found = specs.find(requirement);
            if (found != specs.end())
            {
                Dictionary spec = found->second;
                spec["column_index"] = column_index;
                ladder_specs.push_back(spec);
                return;
            }

            Dictionary empty_spec;
            empty_spec["setup_id"] = 0;
            empty_spec["element_id"] = 0;
            empty_spec["rep_requirement"] = requirement;
            empty_spec["cell_kind"] = String("empty");
            empty_spec["marker_key_high"] = requirement;
            empty_spec["marker_key_low"] = column_index;
            ladder_specs.push_back(empty_spec);
        };

        push_lane(unlockable_specs_by_rep, k_tech_tree_unlockable_column);
        push_lane(village_specs_by_rep, k_tech_tree_village_column);
        push_lane(bureau_specs_by_rep, k_tech_tree_bureau_column);
        push_lane(university_specs_by_rep, k_tech_tree_university_column);

        if (row_index + 1 < row_requirements.size())
        {
            Dictionary spacer_spec;
            spacer_spec["setup_id"] = 0;
            spacer_spec["element_id"] = 0;
            spacer_spec["rep_requirement"] = requirement;
            spacer_spec["cell_kind"] = String("spacer");
            spacer_spec["marker_key_high"] = requirement;
            spacer_spec["marker_key_low"] = 100;
            ladder_specs.push_back(spacer_spec);

            for (int column_index = 1; column_index < k_tech_tree_lane_count; ++column_index)
            {
                Dictionary arrow_spec;
                arrow_spec["setup_id"] = 0;
                arrow_spec["element_id"] = 0;
                arrow_spec["rep_requirement"] = requirement;
                arrow_spec["cell_kind"] = String("arrow");
                arrow_spec["marker_text"] = String("|") + String("\n") + String("v");
                arrow_spec["marker_key_high"] = requirement;
                arrow_spec["marker_key_low"] = 100 + column_index;
                ladder_specs.push_back(arrow_spec);
            }
        }
    }

    reconcile_tech_tree_cards(ladder_specs);
    dirty_ = false;
}

void Gs1GodotRegionalTechTreePanelController::handle_action_pressed(std::int64_t button_key)
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

    if (!bool(button->get_meta("gs1_allow_action", true)))
    {
        return;
    }

    submit_ui_action_(
        as_int(button->get_meta("action_type", 0), 0),
        as_int(button->get_meta("target_id", 0), 0),
        as_int(button->get_meta("arg0", 0), 0),
        as_int(button->get_meta("arg1", 0), 0));
}

void Gs1GodotRegionalTechTreePanelController::apply_overlay_layout()
{
    if (overlay_ == nullptr)
    {
        return;
    }

    const Variant host_left = overlay_->get_meta("gs1_overlay_margin_left", Variant());
    const Variant host_top = overlay_->get_meta("gs1_overlay_margin_top", Variant());
    const Variant host_right = overlay_->get_meta("gs1_overlay_margin_right", Variant());
    const Variant host_bottom = overlay_->get_meta("gs1_overlay_margin_bottom", Variant());
    overlay_->set_offset(SIDE_LEFT, host_left.get_type() == Variant::NIL ? 72.0 : as_float(host_left, 72.0));
    overlay_->set_offset(SIDE_TOP, host_top.get_type() == Variant::NIL ? 56.0 : as_float(host_top, 56.0));
    overlay_->set_offset(SIDE_RIGHT, host_right.get_type() == Variant::NIL ? -72.0 : as_float(host_right, -72.0));
    overlay_->set_offset(SIDE_BOTTOM, host_bottom.get_type() == Variant::NIL ? -48.0 : as_float(host_bottom, -48.0));

    if (panel_ != nullptr)
    {
        const Variant panel_width = overlay_->get_meta("gs1_panel_min_width", Variant());
        const Variant panel_height = overlay_->get_meta("gs1_panel_min_height", Variant());
        panel_->set_custom_minimum_size(
            Vector2(
                panel_width.get_type() == Variant::NIL ? 980.0 : as_float(panel_width, 980.0),
                panel_height.get_type() == Variant::NIL ? 620.0 : as_float(panel_height, 620.0)));
    }
}

void Gs1GodotRegionalTechTreePanelController::reconcile_tech_tree_cards(const Array& card_specs)
{
    if (actions_ == nullptr)
    {
        return;
    }

    std::unordered_set<std::uint64_t> desired_keys;
    int desired_index = 0;
    for (int64_t index = 0; index < card_specs.size(); ++index)
    {
        const Dictionary spec = card_specs[index];
        const int setup_id = as_int(spec.get("setup_id", 0), 0);
        const int element_id = as_int(spec.get("element_id", 0), 0);
        const String cell_kind = String(spec.get("cell_kind", "card"));
        const std::uint64_t stable_key =
            setup_id == 0 && element_id == 0
                ? make_tech_tree_marker_key(
                      static_cast<std::uint32_t>(as_int(spec.get("marker_key_high", as_int(spec.get("rep_requirement", 0), 0)), 0)),
                      static_cast<std::uint32_t>(as_int(spec.get("marker_key_low", 0), 0)))
                : make_projected_button_key(setup_id, element_id);
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

        const String tooltip = String(spec.get("tooltip", ""));
        const int flags = as_int(spec.get("flags", 0), 0);
        const Dictionary action = spec.get("action", Dictionary());
        ProjectedButtonRecord& record = action_buttons_[stable_key];
        ensure_card_content_nodes(button, record);

        button->set_text(String());
        button->set_tooltip_text(tooltip);
        if (cell_kind == "card")
        {
            button->set_custom_minimum_size(Vector2(220, 206));
        }
        else if (cell_kind == "arrow")
        {
            button->set_custom_minimum_size(Vector2(220, 44));
        }
        else
        {
            button->set_custom_minimum_size(Vector2(cell_kind == "rep_label" ? 120 : 220, 44));
        }
        button->set_clip_text(false);
        button->set_flat(true);
        button->set_text_alignment(HORIZONTAL_ALIGNMENT_CENTER);
        const bool locked = (flags & 2) != 0;
        button->set_disabled(false);
        button->set_focus_mode(Control::FOCUS_NONE);
        button->set_meta("action_type", as_int(action.get("type", 0), 0));
        button->set_meta("target_id", as_int(action.get("target_id", 0), 0));
        button->set_meta("arg0", as_int(action.get("arg0", 0), 0));
        button->set_meta("arg1", as_int(action.get("arg1", 0), 0));
        button->set_meta(
            "gs1_allow_action",
            cell_kind == "card" && !locked && as_int(action.get("type", 0), 0) != 0);

        if (Panel* card_panel = Object::cast_to<Panel>(button->get_node_or_null(NodePath("CardPanel"))))
        {
            card_panel->set_visible(cell_kind == "card");
        }
        if (CenterContainer* marker_center = Object::cast_to<CenterContainer>(button->get_node_or_null(NodePath("MarkerCenter"))))
        {
            marker_center->set_visible(cell_kind != "card");
        }
        if (Label* marker_label = resolve_object<Label>(record.marker_label_id))
        {
            marker_label->set_text(tech_tree_marker_text(spec));
            if (cell_kind == "rep_label")
            {
                marker_label->add_theme_font_size_override("font_size", 16);
                marker_label->add_theme_color_override("font_color", Color(0.94, 0.86, 0.71, 0.96));
            }
            else if (cell_kind == "arrow")
            {
                marker_label->add_theme_font_size_override("font_size", 20);
                marker_label->add_theme_color_override("font_color", Color(0.72, 0.79, 0.71, 0.72));
            }
            else
            {
                marker_label->add_theme_font_size_override("font_size", 10);
                marker_label->add_theme_color_override("font_color", Color(0.0, 0.0, 0.0, 0.0));
            }
        }

        if (cell_kind == "card")
        {
            if (Label* icon_label = resolve_object<Label>(record.icon_label_id))
            {
                icon_label->set_text(card_icon_text(spec));
            }
            if (TextureRect* icon_texture = resolve_object<TextureRect>(record.icon_texture_id))
            {
                icon_texture->set_texture(card_icon_texture(spec));
            }
            if (Label* title_label = resolve_object<Label>(record.title_label_id))
            {
                title_label->set_text(card_title_text(spec));
            }
            if (Label* subtitle_label = resolve_object<Label>(record.subtitle_label_id))
            {
                subtitle_label->set_text(card_subtitle_text(spec));
            }
            if (Label* status_label = resolve_object<Label>(record.status_label_id))
            {
                status_label->set_text(card_status_text(spec));
                status_label->add_theme_color_override("font_color", card_status_color(spec));
            }
            if (ColorRect* lock_overlay = resolve_object<ColorRect>(record.lock_overlay_id))
            {
                lock_overlay->set_visible(locked);
            }
            if (Label* lock_label = resolve_object<Label>(record.lock_label_id))
            {
                lock_label->set_visible(locked);
            }
        }
        else
        {
            if (ColorRect* lock_overlay = resolve_object<ColorRect>(record.lock_overlay_id))
            {
                lock_overlay->set_visible(false);
            }
            if (Label* lock_label = resolve_object<Label>(record.lock_label_id))
            {
                lock_label->set_visible(false);
            }
        }

        const Callable callback = owner_control_ == nullptr
            ? Callable()
            : Callable(owner_control_, "on_dynamic_regional_tech_tree_action_pressed").bind(static_cast<int64_t>(stable_key));
        if (callback.is_valid() && !button->is_connected("pressed", callback))
        {
            button->connect("pressed", callback);
        }
    }

    prune_button_registry(action_buttons_, desired_keys);
}

String Gs1GodotRegionalTechTreePanelController::tech_tooltip_text(const Dictionary& spec) const
{
    return technology_ui_for(static_cast<std::uint32_t>(as_int(spec.get("tech_node_id", 0), 0))).tooltip;
}

String Gs1GodotRegionalTechTreePanelController::unlockable_tooltip_text(const Dictionary& spec) const
{
    const std::uint32_t unlock_id = static_cast<std::uint32_t>(as_int(spec.get("element_id", 0), 0));
    return unlockable_ui_for(unlock_id).tooltip;
}

String Gs1GodotRegionalTechTreePanelController::row_requirement_text(const Dictionary& spec) const
{
    const int requirement = as_int(spec.get("rep_requirement", 0), 0);
    if (requirement <= 0)
    {
        return String();
    }
    return vformat("%d rep", requirement);
}

String Gs1GodotRegionalTechTreePanelController::tech_tree_marker_text(const Dictionary& spec) const
{
    const String cell_kind = String(spec.get("cell_kind", "card"));
    if (cell_kind == "rep_label")
    {
        return row_requirement_text(spec);
    }
    if (cell_kind == "arrow")
    {
        return String(spec.get("marker_text", String("|\nv")));
    }
    return String(spec.get("marker_text", ""));
}

String Gs1GodotRegionalTechTreePanelController::card_icon_text(const Dictionary& spec) const
{
    const int entry_kind = as_int(spec.get("entry_kind", 0), 0);
    if (entry_kind == GS1_PROGRESSION_ENTRY_TECHNOLOGY_NODE)
    {
        switch (as_int(spec.get("faction_id", 0), 0))
        {
        case gs1::k_faction_village_committee:
            return "VIL";
        case gs1::k_faction_forestry_bureau:
            return "FOR";
        case gs1::k_faction_agricultural_university:
            return "UNI";
        default:
            return "TEC";
        }
    }

    switch (as_int(spec.get("content_kind", -1), -1))
    {
    case k_unlockable_content_kind_plant:
        return "PLT";
    case k_unlockable_content_kind_item:
        return "ITM";
    case k_unlockable_content_kind_recipe:
        return "RCP";
    case k_unlockable_content_kind_structure_recipe:
        return "DEV";
    default:
        return "UNL";
    }
}

String Gs1GodotRegionalTechTreePanelController::card_title_text(const Dictionary& spec) const
{
    const int entry_kind = as_int(spec.get("entry_kind", 0), 0);
    if (entry_kind == GS1_PROGRESSION_ENTRY_TECHNOLOGY_NODE)
    {
        return technology_ui_for(static_cast<std::uint32_t>(as_int(spec.get("tech_node_id", 0), 0))).title;
    }
    if (entry_kind == GS1_PROGRESSION_ENTRY_REPUTATION_UNLOCK)
    {
        return unlockable_ui_for(static_cast<std::uint32_t>(as_int(spec.get("element_id", 0), 0))).title;
    }
    return String();
}

String Gs1GodotRegionalTechTreePanelController::card_subtitle_text(const Dictionary& spec) const
{
    const int entry_kind = as_int(spec.get("entry_kind", 0), 0);
    if (entry_kind == GS1_PROGRESSION_ENTRY_TECHNOLOGY_NODE)
    {
        return vformat(
            "Tier %d | %s",
            as_int(spec.get("tier_index", 0), 0),
            technology_ui_for(static_cast<std::uint32_t>(as_int(spec.get("tech_node_id", 0), 0))).faction_name);
    }

    switch (unlockable_ui_for(static_cast<std::uint32_t>(as_int(spec.get("element_id", 0), 0))).content_kind)
    {
    case k_unlockable_content_kind_plant:
        return "Plant";
    case k_unlockable_content_kind_item:
        return "Item";
    case k_unlockable_content_kind_recipe:
        return "Recipe";
    case k_unlockable_content_kind_structure_recipe:
        return "Device Recipe";
    default:
        return String("Unlockable");
    }
}

String Gs1GodotRegionalTechTreePanelController::card_status_text(const Dictionary& spec) const
{
    const int flags = as_int(spec.get("flags", 0), 0);
    if ((flags & 2) != 0)
    {
        return "Locked";
    }

    if (as_int(spec.get("entry_kind", 0), 0) == GS1_PROGRESSION_ENTRY_TECHNOLOGY_NODE)
    {
        return "Unlocked";
    }

    return "Available";
}

Color Gs1GodotRegionalTechTreePanelController::card_status_color(const Dictionary& spec) const
{
    const int flags = as_int(spec.get("flags", 0), 0);
    if ((flags & 2) != 0)
    {
        return Color(0.87, 0.72, 0.48, 0.92);
    }

    if (as_int(spec.get("entry_kind", 0), 0) == GS1_PROGRESSION_ENTRY_TECHNOLOGY_NODE)
    {
        return Color(0.49, 0.86, 0.53, 0.95);
    }

    return Color(0.73, 0.88, 0.92, 0.95);
}

Color Gs1GodotRegionalTechTreePanelController::card_icon_background_color(const String& icon_text) const
{
    if (icon_text == "VIL")
    {
        return Color(0.56f, 0.68f, 0.35f, 1.0f);
    }
    if (icon_text == "FOR")
    {
        return Color(0.32f, 0.62f, 0.46f, 1.0f);
    }
    if (icon_text == "UNI")
    {
        return Color(0.34f, 0.49f, 0.74f, 1.0f);
    }
    if (icon_text == "PLT")
    {
        return Color(0.46f, 0.63f, 0.33f, 1.0f);
    }
    if (icon_text == "ITM")
    {
        return Color(0.72f, 0.53f, 0.31f, 1.0f);
    }
    if (icon_text == "RCP")
    {
        return Color(0.76f, 0.44f, 0.28f, 1.0f);
    }
    if (icon_text == "DEV")
    {
        return Color(0.48f, 0.56f, 0.68f, 1.0f);
    }
    return Color(0.58f, 0.50f, 0.34f, 1.0f);
}

Ref<Texture2D> Gs1GodotRegionalTechTreePanelController::card_icon_texture(const Dictionary& spec) const
{
    if (as_int(spec.get("entry_kind", 0), 0) == GS1_PROGRESSION_ENTRY_TECHNOLOGY_NODE)
    {
        const Ref<Texture2D> icon_texture =
            technology_ui_for(static_cast<std::uint32_t>(as_int(spec.get("tech_node_id", 0), 0))).icon_texture;
        if (icon_texture.is_valid())
        {
            return icon_texture;
        }
    }
    else
    {
        const Ref<Texture2D> icon_texture =
            unlockable_ui_for(static_cast<std::uint32_t>(as_int(spec.get("element_id", 0), 0))).icon_texture;
        if (icon_texture.is_valid())
        {
            return icon_texture;
        }
    }

    return fallback_icon_texture(card_icon_text(spec));
}

void Gs1GodotRegionalTechTreePanelController::ensure_card_content_nodes(Button* button, ProjectedButtonRecord& record)
{
    if (button == nullptr)
    {
        return;
    }

    Panel* card_panel = Object::cast_to<Panel>(button->get_node_or_null(NodePath("CardPanel")));
    if (card_panel == nullptr)
    {
        card_panel = memnew(Panel);
        card_panel->set_name("CardPanel");
        card_panel->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
        card_panel->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
        button->add_child(card_panel);
    }

    CenterContainer* marker_center = Object::cast_to<CenterContainer>(button->get_node_or_null(NodePath("MarkerCenter")));
    if (marker_center == nullptr)
    {
        marker_center = memnew(CenterContainer);
        marker_center->set_name("MarkerCenter");
        marker_center->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
        marker_center->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
        button->add_child(marker_center);
    }

    Label* marker_label = resolve_object<Label>(record.marker_label_id);
    if (marker_label == nullptr)
    {
        marker_label = memnew(Label);
        marker_label->set_name("MarkerLabel");
        marker_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
        marker_label->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
        marker_label->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
        marker_label->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
        marker_center->add_child(marker_label);
        record.marker_label_id = marker_label->get_instance_id();
    }

    MarginContainer* content_root = resolve_object<MarginContainer>(record.content_root_id);
    if (content_root == nullptr)
    {
        content_root = memnew(MarginContainer);
        content_root->set_name("CardContentRoot");
        content_root->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
        content_root->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
        content_root->add_theme_constant_override("margin_left", 10);
        content_root->add_theme_constant_override("margin_top", 10);
        content_root->add_theme_constant_override("margin_right", 10);
        content_root->add_theme_constant_override("margin_bottom", 10);
        card_panel->add_child(content_root);
        record.content_root_id = content_root->get_instance_id();
    }

    VBoxContainer* content_vbox = Object::cast_to<VBoxContainer>(content_root->get_node_or_null(NodePath("CardVBox")));
    if (content_vbox == nullptr)
    {
        content_vbox = memnew(VBoxContainer);
        content_vbox->set_name("CardVBox");
        content_vbox->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
        content_vbox->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
        content_vbox->add_theme_constant_override("separation", 6);
        content_root->add_child(content_vbox);
    }

    MarginContainer* icon_frame = Object::cast_to<MarginContainer>(content_root->get_node_or_null(NodePath("CardVBox/IconFrame")));
    if (icon_frame == nullptr)
    {
        icon_frame = memnew(MarginContainer);
        icon_frame->set_name("IconFrame");
        icon_frame->set_custom_minimum_size(Vector2(0, 92));
        icon_frame->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
        content_vbox->add_child(icon_frame);
    }

    TextureRect* icon_texture = resolve_object<TextureRect>(record.icon_texture_id);
    if (icon_texture == nullptr)
    {
        icon_texture = memnew(TextureRect);
        icon_texture->set_name("CardIconTexture");
        icon_texture->set_custom_minimum_size(Vector2(84, 84));
        icon_texture->set_h_size_flags(Control::SIZE_SHRINK_CENTER);
        icon_texture->set_stretch_mode(TextureRect::STRETCH_KEEP_ASPECT_CENTERED);
        icon_texture->set_expand_mode(TextureRect::EXPAND_IGNORE_SIZE);
        icon_texture->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
        icon_frame->add_child(icon_texture);
        record.icon_texture_id = icon_texture->get_instance_id();
    }

    Label* icon_label = resolve_object<Label>(record.icon_label_id);
    if (icon_label == nullptr)
    {
        icon_label = memnew(Label);
        icon_label->set_name("CardIconLabel");
        icon_label->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
        icon_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
        icon_label->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
        icon_label->add_theme_font_size_override("font_size", 28);
        icon_label->add_theme_color_override("font_color", Color(0.95, 0.92, 0.84, 0.98));
        icon_label->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
        icon_frame->add_child(icon_label);
        record.icon_label_id = icon_label->get_instance_id();
    }

    Label* title_label = resolve_object<Label>(record.title_label_id);
    if (title_label == nullptr)
    {
        title_label = memnew(Label);
        title_label->set_name("CardTitleLabel");
        title_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
        title_label->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
        title_label->add_theme_font_size_override("font_size", 17);
        title_label->add_theme_color_override("font_color", Color(0.97, 0.93, 0.83, 0.98));
        title_label->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
        content_vbox->add_child(title_label);
        record.title_label_id = title_label->get_instance_id();
    }

    Label* subtitle_label = resolve_object<Label>(record.subtitle_label_id);
    if (subtitle_label == nullptr)
    {
        subtitle_label = memnew(Label);
        subtitle_label->set_name("CardSubtitleLabel");
        subtitle_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
        subtitle_label->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
        subtitle_label->add_theme_font_size_override("font_size", 13);
        subtitle_label->add_theme_color_override("font_color", Color(0.81, 0.78, 0.70, 0.92));
        subtitle_label->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
        content_vbox->add_child(subtitle_label);
        record.subtitle_label_id = subtitle_label->get_instance_id();
    }

    Label* status_label = resolve_object<Label>(record.status_label_id);
    if (status_label == nullptr)
    {
        status_label = memnew(Label);
        status_label->set_name("CardStatusLabel");
        status_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
        status_label->add_theme_font_size_override("font_size", 14);
        status_label->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
        content_vbox->add_child(status_label);
        record.status_label_id = status_label->get_instance_id();
    }

    ColorRect* lock_overlay = resolve_object<ColorRect>(record.lock_overlay_id);
    if (lock_overlay == nullptr)
    {
        lock_overlay = memnew(ColorRect);
        lock_overlay->set_name("CardLockOverlay");
        lock_overlay->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
        lock_overlay->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
        lock_overlay->set_color(Color(0.16, 0.11, 0.07, 0.38));
        card_panel->add_child(lock_overlay);
        record.lock_overlay_id = lock_overlay->get_instance_id();
    }

    Label* lock_label = resolve_object<Label>(record.lock_label_id);
    if (lock_label == nullptr)
    {
        lock_label = memnew(Label);
        lock_label->set_name("CardLockLabel");
        lock_label->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
        lock_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
        lock_label->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
        lock_label->add_theme_font_size_override("font_size", 28);
        lock_label->add_theme_color_override("font_color", Color(1.0, 1.0, 1.0, 0.7));
        lock_label->set_text("LOCK");
        lock_label->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
        card_panel->add_child(lock_label);
        record.lock_label_id = lock_label->get_instance_id();
    }
}

Ref<Texture2D> Gs1GodotRegionalTechTreePanelController::load_cached_texture(const String& path) const
{
    if (path.is_empty())
    {
        return Ref<Texture2D> {};
    }

    const std::string key = path.utf8().get_data();
    auto found = texture_cache_.find(key);
    if (found != texture_cache_.end())
    {
        return found->second;
    }

    return texture_cache_.emplace(key, load_texture_2d(path)).first->second;
}

Ref<Texture2D> Gs1GodotRegionalTechTreePanelController::fallback_icon_texture(const String& icon_text) const
{
    const std::string key = icon_text.utf8().get_data();
    auto found = fallback_icon_texture_cache_.find(key);
    if (found != fallback_icon_texture_cache_.end())
    {
        return found->second;
    }

    return fallback_icon_texture_cache_.emplace(
        key,
        make_solid_icon_texture(card_icon_background_color(icon_text))).first->second;
}

String Gs1GodotRegionalTechTreePanelController::faction_name_for(int faction_id) const
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

const Gs1GodotRegionalTechTreePanelController::TechnologyUiCacheEntry&
Gs1GodotRegionalTechTreePanelController::technology_ui_for(std::uint32_t tech_node_id) const
{
    auto found = technology_ui_cache_.find(tech_node_id);
    if (found != technology_ui_cache_.end())
    {
        return found->second;
    }

    TechnologyUiCacheEntry entry {};
    entry.title = "Technology";
    entry.faction_name = "Faction";
    entry.tooltip = "Technology";
    entry.icon_texture = load_cached_texture(
        GodotProgressionResourceDatabase::instance().technology_icon_path(tech_node_id));

    if (const auto* node_def = gs1::find_technology_node_def(gs1::TechNodeId {tech_node_id}))
    {
        entry.title = node_def->display_name.empty() ? String("Technology") : string_from_view(node_def->display_name);
        entry.faction_name = faction_name_for(static_cast<int>(node_def->faction_id.value));
        const auto* metadata = adapter_metadata_catalog().find(
            Gs1AdapterMetadataDomain::TechnologyNode,
            node_def->tech_node_id.value);
        const String description = (metadata == nullptr || metadata->description.empty())
            ? String("No authored description yet.")
            : string_from_view(metadata->description);
        entry.tooltip = vformat(
            "%s\n%s Tier %d\n%s",
            entry.title,
            entry.faction_name,
            static_cast<int>(node_def->tier_index),
            description);
    }

    return technology_ui_cache_.emplace(tech_node_id, std::move(entry)).first->second;
}

const Gs1GodotRegionalTechTreePanelController::UnlockableUiCacheEntry&
Gs1GodotRegionalTechTreePanelController::unlockable_ui_for(std::uint32_t unlock_id) const
{
    auto found = unlockable_ui_cache_.find(unlock_id);
    if (found != unlockable_ui_cache_.end())
    {
        return found->second;
    }

    UnlockableUiCacheEntry entry {};
    entry.title = "Unlockable";
    entry.subtitle = "Unlockable";
    entry.tooltip = "Unlockable";

    if (const auto* unlock_def = gs1::find_reputation_unlock_def(unlock_id))
    {
        entry.title = string_from_view(unlock_def->display_name);
        entry.content_kind = static_cast<int>(unlockable_content_kind_from_def(*unlock_def));
        const auto* metadata = adapter_metadata_catalog().find(
            Gs1AdapterMetadataDomain::ReputationUnlock,
            unlock_def->unlock_id);
        const String description = (metadata == nullptr || metadata->description.empty())
            ? String("No authored description yet.")
            : string_from_view(metadata->description);
        entry.tooltip = vformat(
            "%s\n%s\nNeed total reputation %d",
            entry.title,
            description,
            unlock_def->reputation_requirement);
        switch (entry.content_kind)
        {
        case k_unlockable_content_kind_plant:
            entry.subtitle = "Plant";
            break;
        case k_unlockable_content_kind_item:
            entry.subtitle = "Item";
            break;
        case k_unlockable_content_kind_recipe:
            entry.subtitle = "Recipe";
            break;
        case k_unlockable_content_kind_structure_recipe:
            entry.subtitle = "Device Recipe";
            break;
        default:
            entry.subtitle = "Unlockable";
            break;
        }
        entry.icon_texture = load_cached_texture(
            GodotProgressionResourceDatabase::instance().unlockable_icon_path(
                static_cast<std::uint8_t>(entry.content_kind),
                unlock_def->content_id));
    }

    return unlockable_ui_cache_.emplace(unlock_id, std::move(entry)).first->second;
}

Button* Gs1GodotRegionalTechTreePanelController::upsert_button_node(
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
    const int target_index = std::min(desired_index, container->get_child_count() - 1);
    if (button->get_index() != target_index)
    {
        container->move_child(button, target_index);
    }
    return button;
}

void Gs1GodotRegionalTechTreePanelController::prune_button_registry(
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
