#pragma once

#include "messages/game_message.h"

namespace gs1
{
class GamePresentationCoordinator;
struct GamePresentationRuntimeContext;
struct TileCoord;

void site_presentation_handle_message(
    GamePresentationCoordinator& owner,
    GamePresentationRuntimeContext& context,
    const GameMessage& message);
void site_presentation_mark_projection_dirty(
    GamePresentationCoordinator& owner,
    GamePresentationRuntimeContext& context,
    std::uint64_t dirty_flags) noexcept;
void site_presentation_mark_tile_dirty(
    GamePresentationCoordinator& owner,
    GamePresentationRuntimeContext& context,
    TileCoord coord) noexcept;
void site_presentation_flush_if_dirty(
    GamePresentationCoordinator& owner,
    GamePresentationRuntimeContext& context);
}  // namespace gs1
