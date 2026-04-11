#pragma once

#include "commands/game_command.h"
#include "gs1/status.h"

namespace gs1
{
class GameRuntime;

class GameCommandHandler final
{
public:
    [[nodiscard]] static Gs1Status handle(GameRuntime& runtime, const GameCommand& command);
};
}  // namespace gs1
