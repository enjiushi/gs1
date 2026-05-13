#pragma once

#include "../../../include/gs1/types.h"

[[nodiscard]] constexpr bool gs1_godot_engine_message_requires_exclusive_subscriber(
    Gs1EngineMessageType type) noexcept
{
    (void)type;
    return false;
}
