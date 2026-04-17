#pragma once

#include "messages/game_message.h"
#include "gs1/status.h"

namespace gs1
{
class GameRuntime;

class GameMessageHandler final
{
public:
    [[nodiscard]] static Gs1Status handle(GameRuntime& runtime, const GameMessage& message);
};
}  // namespace gs1
