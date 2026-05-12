#pragma once

#include "messages/game_message.h"

namespace gs1
{
class GamePresentationCoordinator;
struct GamePresentationRuntimeContext;

void campaign_presentation_handle_message(
    GamePresentationCoordinator& owner,
    GamePresentationRuntimeContext& context,
    const GameMessage& message);
}  // namespace gs1
