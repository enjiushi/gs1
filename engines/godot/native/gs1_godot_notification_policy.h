#pragma once

#include "gs1_godot_notification_types.h"

[[nodiscard]] constexpr bool gs1_godot_notification_requires_exclusive_subscriber(
    Gs1GodotNotificationType type) noexcept
{
    (void)type;
    return false;
}
