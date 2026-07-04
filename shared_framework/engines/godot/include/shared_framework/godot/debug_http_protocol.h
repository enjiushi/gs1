#pragma once

#include "gs1/types.h"

#include <cstdint>
#include <string>
#include <string_view>

enum class Gs1GodotDebugHttpCommandKind : std::uint8_t
{
    GameplayAction = 0,
    SiteAction = 1,
    SiteActionCancel = 2,
    SiteStorageView = 3,
    SiteInventorySlotTap = 4,
    SiteContextRequest = 5,
    SiteMoveDirection = 6
};

struct Gs1GodotDebugHttpCommand final
{
    Gs1GodotDebugHttpCommandKind kind {Gs1GodotDebugHttpCommandKind::GameplayAction};
    std::int64_t action_type {0};
    std::int64_t target_id {0};
    std::int64_t arg0 {0};
    std::int64_t arg1 {0};
    std::int64_t action_kind {0};
    std::int64_t flags {0};
    std::int64_t quantity {0};
    std::int64_t tile_x {0};
    std::int64_t tile_y {0};
    std::int64_t primary_subject_id {0};
    std::int64_t secondary_subject_id {0};
    std::int64_t item_id {0};
    std::int64_t action_id {0};
    std::int64_t storage_id {0};
    std::int64_t event_kind {0};
    std::int64_t container_kind {0};
    std::int64_t slot_index {0};
    std::int64_t item_instance_id {0};
    double world_move_x {0.0};
    double world_move_y {0.0};
    double world_move_z {0.0};
    bool has_move_input {false};
};

[[nodiscard]] bool gs1_parse_godot_debug_http_command(
    std::string_view path,
    std::string_view body,
    Gs1GodotDebugHttpCommand& out_command,
    std::string* out_error = nullptr);
