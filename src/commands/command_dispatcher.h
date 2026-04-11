#pragma once

#include "commands/game_command.h"
#include "gs1/status.h"

namespace gs1
{
class GameRuntime;

class CommandDispatcher final
{
public:
    [[nodiscard]] static Gs1Status dispatch_all(GameRuntime& runtime);
};
}  // namespace gs1
