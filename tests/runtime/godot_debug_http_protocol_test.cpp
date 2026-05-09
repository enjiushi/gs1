#include "gs1_godot_debug_http_protocol.h"

#include <cassert>
#include <string>

namespace
{
void ui_action_string_payload_parses()
{
    Gs1GodotDebugHttpCommand command {};
    std::string error {};
    const bool ok = gs1_parse_godot_debug_http_command(
        "/ui-action",
        R"({"type":"START_NEW_CAMPAIGN","targetId":7,"arg0":11,"arg1":13})",
        command,
        &error);

    assert(ok);
    assert(error.empty());
    assert(command.kind == Gs1GodotDebugHttpCommandKind::UiAction);
    assert(command.action_type == GS1_UI_ACTION_START_NEW_CAMPAIGN);
    assert(command.target_id == 7);
    assert(command.arg0 == 11);
    assert(command.arg1 == 13);
}

void site_action_numeric_payload_parses()
{
    Gs1GodotDebugHttpCommand command {};
    std::string error {};
    const bool ok = gs1_parse_godot_debug_http_command(
        "/site-action",
        R"({"actionKindId":10,"flags":3,"quantity":2,"targetTileX":5,"targetTileY":6,"primarySubjectId":17,"secondarySubjectId":19,"itemId":23})",
        command,
        &error);

    assert(ok);
    assert(error.empty());
    assert(command.kind == Gs1GodotDebugHttpCommandKind::SiteAction);
    assert(command.action_kind == GS1_SITE_ACTION_EXCAVATE);
    assert(command.flags == 3);
    assert(command.quantity == 2);
    assert(command.tile_x == 5);
    assert(command.tile_y == 6);
    assert(command.primary_subject_id == 17);
    assert(command.secondary_subject_id == 19);
    assert(command.item_id == 23);
}

void site_inventory_slot_tap_string_payload_parses()
{
    Gs1GodotDebugHttpCommand command {};
    std::string error {};
    const bool ok = gs1_parse_godot_debug_http_command(
        "/site-inventory-slot-tap",
        R"({"storageId":29,"containerKind":"DEVICE_STORAGE","slotIndex":3,"itemInstanceId":31})",
        command,
        &error);

    assert(ok);
    assert(error.empty());
    assert(command.kind == Gs1GodotDebugHttpCommandKind::SiteInventorySlotTap);
    assert(command.storage_id == 29);
    assert(command.container_kind == GS1_INVENTORY_CONTAINER_DEVICE_STORAGE);
    assert(command.slot_index == 3);
    assert(command.item_instance_id == 31);
}

void site_control_without_move_input_zeroes_direction()
{
    Gs1GodotDebugHttpCommand command {};
    std::string error {};
    const bool ok = gs1_parse_godot_debug_http_command(
        "/site-control",
        R"({"hasMoveInput":0,"worldMoveX":4.5,"worldMoveY":6.5,"worldMoveZ":8.5})",
        command,
        &error);

    assert(ok);
    assert(error.empty());
    assert(command.kind == Gs1GodotDebugHttpCommandKind::SiteMoveDirection);
    assert(!command.has_move_input);
    assert(command.world_move_x == 0.0);
    assert(command.world_move_y == 0.0);
    assert(command.world_move_z == 0.0);
}

void invalid_ui_action_is_rejected()
{
    Gs1GodotDebugHttpCommand command {};
    std::string error {};
    const bool ok = gs1_parse_godot_debug_http_command(
        "/ui-action",
        R"({"type":"NOT_A_REAL_ACTION"})",
        command,
        &error);

    assert(!ok);
    assert(error == "Missing or invalid UI action type.");
}
}

int main()
{
    ui_action_string_payload_parses();
    site_action_numeric_payload_parses();
    site_inventory_slot_tap_string_payload_parses();
    site_control_without_move_input_zeroes_direction();
    invalid_ui_action_is_rejected();
    return 0;
}
