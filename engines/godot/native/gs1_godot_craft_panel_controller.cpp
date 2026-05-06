#include "gs1_godot_craft_panel_controller.h"

#include "content/defs/item_defs.h"

#include <godot_cpp/variant/packed_string_array.hpp>

using namespace godot;

namespace
{
String string_from_view(std::string_view value)
{
    return String::utf8(value.data(), static_cast<int>(value.size()));
}
}

void Gs1GodotCraftPanelController::cache_ui_references(Control& owner)
{
    if (craft_summary_ == nullptr)
    {
        craft_summary_ = Object::cast_to<RichTextLabel>(owner.find_child("CraftSummary", true, false));
    }
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
}

void Gs1GodotCraftPanelController::handle_runtime_message_reset()
{
    placement_preview_.reset();
    placement_failure_.reset();
    craft_context_.reset();
    dirty_ = true;
}

void Gs1GodotCraftPanelController::refresh_if_needed()
{
    if (!dirty_ || craft_summary_ == nullptr)
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
    dirty_ = false;
}

String Gs1GodotCraftPanelController::item_name_for(int item_id) const
{
    if (const auto* item_def = gs1::find_item_def(gs1::ItemId {static_cast<std::uint32_t>(item_id)}))
    {
        return string_from_view(item_def->display_name);
    }
    return String("Item #") + String::num_int64(item_id);
}
