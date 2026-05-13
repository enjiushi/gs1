#pragma once

#include "../../../include/gs1/types.h"

[[nodiscard]] constexpr bool gs1_godot_engine_message_requires_exclusive_subscriber(
    Gs1EngineMessageType type) noexcept
{
    switch (type)
    {
    case GS1_ENGINE_MESSAGE_BEGIN_SITE_SCENE_SNAPSHOT:
        return true;
    default:
        return false;
    }
}
