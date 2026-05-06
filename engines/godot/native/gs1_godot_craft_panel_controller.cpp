#include "gs1_godot_craft_panel_controller.h"

#include "content/defs/craft_recipe_defs.h"
#include "content/defs/item_defs.h"

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/core/object.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>

#include <algorithm>
#include <string_view>

using namespace godot;

namespace
{
constexpr std::uint64_t k_fnv_offset_basis = 14695981039346656037ULL;
constexpr std::uint64_t k_fnv_prime = 1099511628211ULL;

template <typename T>
T* resolve_object(const ObjectID object_id)
{
    return Object::cast_to<T>(ObjectDB::get_instance(object_id));
}

template <typename T>
void hash_value(std::uint64_t& hash, const T& value)
{
    static_assert(std::is_trivially_copyable_v<T>);
    const auto* bytes = static_cast<const unsigned char*>(static_cast<const void*>(&value));
    for (std::size_t index = 0; index < sizeof(T); ++index)
    {
        hash ^= static_cast<std::uint64_t>(bytes[index]);
        hash *= k_fnv_prime;
    }
}

std::uint64_t make_craft_option_key(int tile_x, int tile_y, int output_item_id) noexcept
{
    std::uint64_t hash = k_fnv_offset_basis;
    hash_value(hash, tile_x);
    hash_value(hash, tile_y);
    hash_value(hash, output_item_id);
    return hash;
}

String string_from_view(std::string_view value)
{
    return String::utf8(value.data(), static_cast<int>(value.size()));
}
}

void Gs1GodotCraftPanelController::cache_ui_references(Control& owner)
{
    owner_control_ = &owner;
    if (panel_ == nullptr)
    {
        panel_ = Object::cast_to<Control>(owner.find_child("CraftPanel", true, false));
    }
    if (craft_summary_ == nullptr)
    {
        craft_summary_ = Object::cast_to<RichTextLabel>(owner.find_child("CraftSummary", true, false));
    }
    if (craft_options_ == nullptr)
    {
        craft_options_ = Object::cast_to<VBoxContainer>(owner.find_child("CraftOptions", true, false));
    }
    refresh_if_needed();
}

void Gs1GodotCraftPanelController::set_submit_craft_option_callback(SubmitCraftOptionFn callback)
{
    submit_craft_option_ = std::move(callback);
}

bool Gs1GodotCraftPanelController::handles_engine_message(Gs1EngineMessageType type) const noexcept
{
    switch (type)
    {
    case GS1_ENGINE_MESSAGE_SET_APP_STATE:
    case GS1_ENGINE_MESSAGE_SITE_CRAFT_CONTEXT_BEGIN:
    case GS1_ENGINE_MESSAGE_SITE_CRAFT_CONTEXT_OPTION_UPSERT:
    case GS1_ENGINE_MESSAGE_SITE_CRAFT_CONTEXT_END:
    case GS1_ENGINE_MESSAGE_SITE_PLACEMENT_PREVIEW:
    case GS1_ENGINE_MESSAGE_SITE_PLACEMENT_FAILURE:
        return true;
    default:
        return false;
    }
}

void Gs1GodotCraftPanelController::handle_engine_message(const Gs1EngineMessage& message)
{
    switch (message.type)
    {
    case GS1_ENGINE_MESSAGE_SET_APP_STATE:
    {
        const auto& payload = message.payload_as<Gs1EngineMessageSetAppStateData>();
        current_app_state_ = payload.app_state;
        if (payload.app_state == GS1_APP_STATE_MAIN_MENU ||
            payload.app_state == GS1_APP_STATE_REGIONAL_MAP ||
            payload.app_state == GS1_APP_STATE_CAMPAIGN_END)
        {
            placement_preview_.reset();
            placement_failure_.reset();
            craft_context_.reset();
        }
        break;
    }
    case GS1_ENGINE_MESSAGE_SITE_CRAFT_CONTEXT_BEGIN:
    {
        const auto& payload = message.payload_as<Gs1EngineMessageCraftContextData>();
        craft_context_ = Gs1RuntimeCraftContextProjection {};
        craft_context_->tile_x = payload.tile_x;
        craft_context_->tile_y = payload.tile_y;
        craft_context_->flags = payload.flags;
        craft_context_->options.clear();
        craft_context_->options.reserve(payload.option_count);
        break;
    }
    case GS1_ENGINE_MESSAGE_SITE_CRAFT_CONTEXT_OPTION_UPSERT:
        if (craft_context_.has_value()) craft_context_->options.push_back(message.payload_as<Gs1EngineMessageCraftContextOptionData>());
        break;
    case GS1_ENGINE_MESSAGE_SITE_CRAFT_CONTEXT_END:
        break;
    case GS1_ENGINE_MESSAGE_SITE_PLACEMENT_PREVIEW:
    {
        const auto& payload = message.payload_as<Gs1EngineMessagePlacementPreviewData>();
        if ((payload.flags & 1U) == 0U)
        {
            placement_preview_.reset();
        }
        else
        {
            placement_preview_ = payload;
        }
        break;
    }
    case GS1_ENGINE_MESSAGE_SITE_PLACEMENT_FAILURE:
        placement_failure_ = message.payload_as<Gs1EngineMessagePlacementFailureData>();
        break;
    default:
        break;
    }

    dirty_ = true;
    refresh_if_needed();
}

void Gs1GodotCraftPanelController::handle_runtime_message_reset()
{
    current_app_state_.reset();
    placement_preview_.reset();
    placement_failure_.reset();
    craft_context_.reset();
    dirty_ = true;
    refresh_if_needed();
}

void Gs1GodotCraftPanelController::refresh_if_needed()
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
        dirty_ = false;
        return;
    }
    if (craft_summary_ == nullptr)
    {
        return;
    }

    PackedStringArray lines;
    lines.push_back("[b]Craft / Placement[/b]");

    if (placement_preview_.has_value())
    {
        const auto& preview = placement_preview_.value();
        lines.push_back(vformat(
            "Placement: %s at (%d, %d)  blocked %d",
            item_name_for(static_cast<int>(preview.item_id)),
            preview.tile_x,
            preview.tile_y,
            static_cast<int>(preview.blocked_mask)));
    }

    if (placement_failure_.has_value())
    {
        const auto& failure = placement_failure_.value();
        lines.push_back(vformat(
            "Placement Failure: tile (%d, %d) flags %d",
            failure.tile_x,
            failure.tile_y,
            static_cast<int>(failure.flags)));
    }

    if (craft_context_.has_value())
    {
        const auto& craft_context = craft_context_.value();
        lines.push_back(vformat(
            "Craft Context @ (%d, %d)",
            craft_context.tile_x,
            craft_context.tile_y));
    }

    craft_summary_->set_text(String("\n").join(lines));
    reconcile_craft_option_buttons();
    dirty_ = false;
}

void Gs1GodotCraftPanelController::reconcile_craft_option_buttons()
{
    if (craft_options_ == nullptr)
    {
        return;
    }

    std::unordered_set<std::uint64_t> desired_keys;
    if (craft_context_.has_value())
    {
        const int context_x = craft_context_->tile_x;
        const int context_y = craft_context_->tile_y;
        int desired_index = 0;
        for (const auto& option : craft_context_->options)
        {
            const int output_item_id = static_cast<int>(option.output_item_id);
            const std::uint64_t stable_key = make_craft_option_key(context_x, context_y, output_item_id);
            desired_keys.insert(stable_key);
            Button* button = upsert_button_node(
                craft_options_,
                craft_option_buttons_,
                stable_key,
                vformat("DynamicCraft_%d", output_item_id),
                desired_index++);
            if (button == nullptr)
            {
                continue;
            }

            button->set_text(vformat(
                "[ITM] Craft %s",
                recipe_output_name_for(
                    static_cast<int>(option.recipe_id),
                    static_cast<int>(option.output_item_id))));
            button->set_tooltip_text(String());
            button->set_meta("tile_x", context_x);
            button->set_meta("tile_y", context_y);
            button->set_meta("output_item_id", output_item_id);
            const Callable callback = owner_control_ == nullptr
                ? Callable()
                : Callable(owner_control_, "on_dynamic_craft_option_pressed").bind(static_cast<int64_t>(stable_key));
            if (callback.is_valid() && !button->is_connected("pressed", callback))
            {
                button->connect("pressed", callback);
            }
        }
    }

    prune_button_registry(craft_option_buttons_, desired_keys);
}

void Gs1GodotCraftPanelController::on_dynamic_craft_option_pressed(std::int64_t button_key)
{
    auto found = craft_option_buttons_.find(static_cast<std::uint64_t>(button_key));
    if (found == craft_option_buttons_.end())
    {
        return;
    }

    Button* button = resolve_object<Button>(found->second.object_id);
    if (button == nullptr || !submit_craft_option_)
    {
        return;
    }

    submit_craft_option_(
        static_cast<int>(button->get_meta("tile_x", 0)),
        static_cast<int>(button->get_meta("tile_y", 0)),
        static_cast<int>(button->get_meta("output_item_id", 0)));
}

void Gs1GodotCraftPanelController::handle_craft_option_pressed(std::int64_t button_key)
{
    on_dynamic_craft_option_pressed(button_key);
}

Button* Gs1GodotCraftPanelController::upsert_button_node(
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
        button->set_name(node_name);
        button->set_clip_text(true);
        button->set_custom_minimum_size(Vector2(0.0F, 44.0F));
        container->add_child(button);
        registry[stable_key].object_id = button->get_instance_id();
    }

    const int child_count = container->get_child_count();
    const int clamped_index = std::clamp(desired_index, 0, child_count - 1);
    if (button->get_index() != clamped_index)
    {
        container->move_child(button, clamped_index);
    }

    return button;
}

void Gs1GodotCraftPanelController::prune_button_registry(
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

String Gs1GodotCraftPanelController::item_name_for(int item_id) const
{
    if (const auto* item_def = gs1::find_item_def(gs1::ItemId {static_cast<std::uint32_t>(item_id)}))
    {
        return string_from_view(item_def->display_name);
    }
    return String("Item #") + String::num_int64(item_id);
}

String Gs1GodotCraftPanelController::recipe_output_name_for(int recipe_id, int output_item_id) const
{
    if (const auto* recipe_def = gs1::find_craft_recipe_def(gs1::RecipeId {static_cast<std::uint32_t>(recipe_id)}))
    {
        return item_name_for(static_cast<int>(recipe_def->output_item_id.value));
    }
    return item_name_for(output_item_id);
}
