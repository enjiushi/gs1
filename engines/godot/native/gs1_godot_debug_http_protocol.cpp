#include "gs1_godot_debug_http_protocol.h"

#include <cstdlib>
#include <optional>
#include <string>
#include <type_traits>

namespace
{
template <typename NumberType>
std::optional<NumberType> extract_number_field(std::string_view body, const char* key)
{
    const std::string pattern = std::string("\"") + key + "\"";
    const std::size_t key_pos = body.find(pattern);
    if (key_pos == std::string_view::npos)
    {
        return std::nullopt;
    }

    const std::size_t colon_pos = body.find(':', key_pos + pattern.size());
    if (colon_pos == std::string_view::npos)
    {
        return std::nullopt;
    }

    std::size_t value_start = colon_pos + 1U;
    while (value_start < body.size() &&
        (body[value_start] == ' ' || body[value_start] == '\t' || body[value_start] == '\r' || body[value_start] == '\n'))
    {
        value_start += 1U;
    }

    std::size_t value_end = value_start;
    while (value_end < body.size() &&
        body[value_end] != ',' &&
        body[value_end] != '}' &&
        body[value_end] != '\r' &&
        body[value_end] != '\n')
    {
        value_end += 1U;
    }

    const std::string raw_value {body.substr(value_start, value_end - value_start)};
    if constexpr (std::is_floating_point_v<NumberType>)
    {
        return static_cast<NumberType>(std::strtod(raw_value.c_str(), nullptr));
    }
    else
    {
        return static_cast<NumberType>(std::strtoll(raw_value.c_str(), nullptr, 10));
    }
}

std::optional<std::string> extract_string_field(std::string_view body, const char* key)
{
    const std::string pattern = std::string("\"") + key + "\"";
    const std::size_t key_pos = body.find(pattern);
    if (key_pos == std::string_view::npos)
    {
        return std::nullopt;
    }

    const std::size_t colon_pos = body.find(':', key_pos + pattern.size());
    if (colon_pos == std::string_view::npos)
    {
        return std::nullopt;
    }

    const std::size_t first_quote = body.find('"', colon_pos + 1U);
    if (first_quote == std::string_view::npos)
    {
        return std::nullopt;
    }

    const std::size_t second_quote = body.find('"', first_quote + 1U);
    if (second_quote == std::string_view::npos)
    {
        return std::nullopt;
    }

    return std::string {body.substr(first_quote + 1U, second_quote - first_quote - 1U)};
}

void assign_error(std::string* out_error, const char* message)
{
    if (out_error != nullptr)
    {
        *out_error = message;
    }
}

std::optional<Gs1UiActionType> parse_ui_action_name(const std::string& value)
{
    if (value == "NONE") return GS1_UI_ACTION_NONE;
    if (value == "START_NEW_CAMPAIGN") return GS1_UI_ACTION_START_NEW_CAMPAIGN;
    if (value == "SELECT_DEPLOYMENT_SITE") return GS1_UI_ACTION_SELECT_DEPLOYMENT_SITE;
    if (value == "START_SITE_ATTEMPT") return GS1_UI_ACTION_START_SITE_ATTEMPT;
    if (value == "RETURN_TO_REGIONAL_MAP") return GS1_UI_ACTION_RETURN_TO_REGIONAL_MAP;
    if (value == "CLEAR_DEPLOYMENT_SITE_SELECTION") return GS1_UI_ACTION_CLEAR_DEPLOYMENT_SITE_SELECTION;
    if (value == "ACCEPT_TASK") return GS1_UI_ACTION_ACCEPT_TASK;
    if (value == "CLAIM_TASK_REWARD") return GS1_UI_ACTION_CLAIM_TASK_REWARD;
    if (value == "BUY_PHONE_LISTING") return GS1_UI_ACTION_BUY_PHONE_LISTING;
    if (value == "SELL_PHONE_LISTING") return GS1_UI_ACTION_SELL_PHONE_LISTING;
    if (value == "HIRE_CONTRACTOR") return GS1_UI_ACTION_HIRE_CONTRACTOR;
    if (value == "PURCHASE_SITE_UNLOCKABLE") return GS1_UI_ACTION_PURCHASE_SITE_UNLOCKABLE;
    if (value == "ADD_PHONE_LISTING_TO_CART") return GS1_UI_ACTION_ADD_PHONE_LISTING_TO_CART;
    if (value == "REMOVE_PHONE_LISTING_FROM_CART") return GS1_UI_ACTION_REMOVE_PHONE_LISTING_FROM_CART;
    if (value == "CHECKOUT_PHONE_CART") return GS1_UI_ACTION_CHECKOUT_PHONE_CART;
    if (value == "SET_PHONE_PANEL_SECTION") return GS1_UI_ACTION_SET_PHONE_PANEL_SECTION;
    if (value == "OPEN_REGIONAL_MAP_TECH_TREE") return GS1_UI_ACTION_OPEN_REGIONAL_MAP_TECH_TREE;
    if (value == "CLOSE_REGIONAL_MAP_TECH_TREE") return GS1_UI_ACTION_CLOSE_REGIONAL_MAP_TECH_TREE;
    if (value == "CLAIM_TECHNOLOGY_NODE") return GS1_UI_ACTION_CLAIM_TECHNOLOGY_NODE;
    if (value == "SELECT_TECH_TREE_FACTION_TAB") return GS1_UI_ACTION_SELECT_TECH_TREE_FACTION_TAB;
    if (value == "CLOSE_PHONE_PANEL") return GS1_UI_ACTION_CLOSE_PHONE_PANEL;
    if (value == "REFUND_TECHNOLOGY_NODE") return GS1_UI_ACTION_REFUND_TECHNOLOGY_NODE;
    if (value == "OPEN_SITE_PROTECTION_SELECTOR") return GS1_UI_ACTION_OPEN_SITE_PROTECTION_SELECTOR;
    if (value == "CLOSE_SITE_PROTECTION_UI") return GS1_UI_ACTION_CLOSE_SITE_PROTECTION_UI;
    if (value == "SET_SITE_PROTECTION_OVERLAY_MODE") return GS1_UI_ACTION_SET_SITE_PROTECTION_OVERLAY_MODE;
    if (value == "END_SITE_MODIFIER") return GS1_UI_ACTION_END_SITE_MODIFIER;
    return std::nullopt;
}

std::optional<Gs1SiteActionKind> parse_site_action_name(const std::string& value)
{
    if (value == "NONE") return GS1_SITE_ACTION_NONE;
    if (value == "PLANT") return GS1_SITE_ACTION_PLANT;
    if (value == "BUILD") return GS1_SITE_ACTION_BUILD;
    if (value == "REPAIR") return GS1_SITE_ACTION_REPAIR;
    if (value == "WATER") return GS1_SITE_ACTION_WATER;
    if (value == "CLEAR_BURIAL") return GS1_SITE_ACTION_CLEAR_BURIAL;
    if (value == "CRAFT") return GS1_SITE_ACTION_CRAFT;
    if (value == "DRINK") return GS1_SITE_ACTION_DRINK;
    if (value == "EAT") return GS1_SITE_ACTION_EAT;
    if (value == "HARVEST") return GS1_SITE_ACTION_HARVEST;
    if (value == "EXCAVATE") return GS1_SITE_ACTION_EXCAVATE;
    return std::nullopt;
}

std::optional<Gs1InventoryViewEventKind> parse_inventory_view_event_name(const std::string& value)
{
    if (value == "NONE") return GS1_INVENTORY_VIEW_EVENT_NONE;
    if (value == "OPEN_SNAPSHOT") return GS1_INVENTORY_VIEW_EVENT_OPEN_SNAPSHOT;
    if (value == "CLOSE") return GS1_INVENTORY_VIEW_EVENT_CLOSE;
    return std::nullopt;
}

std::optional<Gs1InventoryContainerKind> parse_inventory_container_name(const std::string& value)
{
    if (value == "WORKER_PACK") return GS1_INVENTORY_CONTAINER_WORKER_PACK;
    if (value == "DEVICE_STORAGE") return GS1_INVENTORY_CONTAINER_DEVICE_STORAGE;
    return std::nullopt;
}

std::optional<std::int64_t> parse_ui_action_type(std::string_view body)
{
    if (const auto numeric_value = extract_number_field<std::int64_t>(body, "actionType"); numeric_value.has_value())
    {
        return numeric_value;
    }
    const auto action_name = extract_string_field(body, "type");
    if (!action_name.has_value())
    {
        return std::nullopt;
    }
    const auto parsed = parse_ui_action_name(action_name.value());
    if (!parsed.has_value())
    {
        return std::nullopt;
    }
    return static_cast<std::int64_t>(parsed.value());
}

std::optional<std::int64_t> parse_site_action_kind(std::string_view body)
{
    if (const auto numeric_value = extract_number_field<std::int64_t>(body, "actionKindId"); numeric_value.has_value())
    {
        return numeric_value;
    }
    const auto action_name = extract_string_field(body, "actionKind");
    if (!action_name.has_value())
    {
        return std::nullopt;
    }
    const auto parsed = parse_site_action_name(action_name.value());
    if (!parsed.has_value())
    {
        return std::nullopt;
    }
    return static_cast<std::int64_t>(parsed.value());
}

std::optional<std::int64_t> parse_inventory_view_event_kind(std::string_view body)
{
    if (const auto numeric_value = extract_number_field<std::int64_t>(body, "eventKindId"); numeric_value.has_value())
    {
        return numeric_value;
    }
    const auto event_name = extract_string_field(body, "eventKind");
    if (!event_name.has_value())
    {
        return std::nullopt;
    }
    const auto parsed = parse_inventory_view_event_name(event_name.value());
    if (!parsed.has_value())
    {
        return std::nullopt;
    }
    return static_cast<std::int64_t>(parsed.value());
}

std::optional<std::int64_t> parse_inventory_container_kind(std::string_view body)
{
    if (const auto numeric_value = extract_number_field<std::int64_t>(body, "containerKindId"); numeric_value.has_value())
    {
        return numeric_value;
    }
    const auto container_name = extract_string_field(body, "containerKind");
    if (!container_name.has_value())
    {
        return std::nullopt;
    }
    const auto parsed = parse_inventory_container_name(container_name.value());
    if (!parsed.has_value())
    {
        return std::nullopt;
    }
    return static_cast<std::int64_t>(parsed.value());
}
}

bool gs1_parse_godot_debug_http_command(
    std::string_view path,
    std::string_view body,
    Gs1GodotDebugHttpCommand& out_command,
    std::string* out_error)
{
    Gs1GodotDebugHttpCommand command {};

    if (path == "/ui-action")
    {
        const auto action_type = parse_ui_action_type(body);
        if (!action_type.has_value())
        {
            assign_error(out_error, "Missing or invalid UI action type.");
            return false;
        }
        command.kind = Gs1GodotDebugHttpCommandKind::UiAction;
        command.action_type = action_type.value();
        command.target_id = extract_number_field<std::int64_t>(body, "targetId").value_or(0);
        command.arg0 = extract_number_field<std::int64_t>(body, "arg0").value_or(0);
        command.arg1 = extract_number_field<std::int64_t>(body, "arg1").value_or(0);
        out_command = command;
        return true;
    }

    if (path == "/site-action")
    {
        const auto action_kind = parse_site_action_kind(body);
        if (!action_kind.has_value())
        {
            assign_error(out_error, "Missing or invalid site action kind.");
            return false;
        }
        command.kind = Gs1GodotDebugHttpCommandKind::SiteAction;
        command.action_kind = action_kind.value();
        command.flags = extract_number_field<std::int64_t>(body, "flags").value_or(0);
        command.quantity = extract_number_field<std::int64_t>(body, "quantity").value_or(1);
        command.tile_x = extract_number_field<std::int64_t>(body, "targetTileX").value_or(0);
        command.tile_y = extract_number_field<std::int64_t>(body, "targetTileY").value_or(0);
        command.primary_subject_id = extract_number_field<std::int64_t>(body, "primarySubjectId").value_or(0);
        command.secondary_subject_id = extract_number_field<std::int64_t>(body, "secondarySubjectId").value_or(0);
        command.item_id = extract_number_field<std::int64_t>(body, "itemId").value_or(0);
        out_command = command;
        return true;
    }

    if (path == "/site-action-cancel")
    {
        command.kind = Gs1GodotDebugHttpCommandKind::SiteActionCancel;
        command.action_id = extract_number_field<std::int64_t>(body, "actionId").value_or(0);
        command.flags = extract_number_field<std::int64_t>(body, "flags").value_or(0);
        out_command = command;
        return true;
    }

    if (path == "/site-storage-view")
    {
        const auto event_kind = parse_inventory_view_event_kind(body);
        if (!event_kind.has_value())
        {
            assign_error(out_error, "Missing or invalid inventory view event kind.");
            return false;
        }
        command.kind = Gs1GodotDebugHttpCommandKind::SiteStorageView;
        command.storage_id = extract_number_field<std::int64_t>(body, "storageId").value_or(0);
        command.event_kind = event_kind.value();
        out_command = command;
        return true;
    }

    if (path == "/site-inventory-slot-tap")
    {
        const auto container_kind = parse_inventory_container_kind(body);
        if (!container_kind.has_value())
        {
            assign_error(out_error, "Missing or invalid inventory container kind.");
            return false;
        }
        command.kind = Gs1GodotDebugHttpCommandKind::SiteInventorySlotTap;
        command.storage_id = extract_number_field<std::int64_t>(body, "storageId").value_or(0);
        command.container_kind = container_kind.value();
        command.slot_index = extract_number_field<std::int64_t>(body, "slotIndex").value_or(0);
        command.item_instance_id = extract_number_field<std::int64_t>(body, "itemInstanceId").value_or(0);
        out_command = command;
        return true;
    }

    if (path == "/site-context")
    {
        command.kind = Gs1GodotDebugHttpCommandKind::SiteContextRequest;
        command.tile_x = extract_number_field<std::int64_t>(body, "tileX").value_or(0);
        command.tile_y = extract_number_field<std::int64_t>(body, "tileY").value_or(0);
        command.flags = extract_number_field<std::int64_t>(body, "flags").value_or(0);
        out_command = command;
        return true;
    }

    if (path == "/site-control")
    {
        command.kind = Gs1GodotDebugHttpCommandKind::SiteMoveDirection;
        command.has_move_input = extract_number_field<std::int64_t>(body, "hasMoveInput").value_or(0) != 0;
        command.world_move_x = extract_number_field<double>(body, "worldMoveX").value_or(0.0);
        command.world_move_y = extract_number_field<double>(body, "worldMoveY").value_or(0.0);
        command.world_move_z = extract_number_field<double>(body, "worldMoveZ").value_or(0.0);
        if (!command.has_move_input)
        {
            command.world_move_x = 0.0;
            command.world_move_y = 0.0;
            command.world_move_z = 0.0;
        }
        out_command = command;
        return true;
    }

    assign_error(out_error, "Unknown debug HTTP endpoint.");
    return false;
}
