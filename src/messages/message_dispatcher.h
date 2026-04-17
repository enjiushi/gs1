#pragma once

#include "messages/game_message.h"
#include "gs1/status.h"

namespace gs1
{
class GameRuntime;

class MessageDispatcher final
{
public:
    [[nodiscard]] static Gs1Status dispatch_all(GameRuntime& runtime);
};
}  // namespace gs1
