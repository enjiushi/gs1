#pragma once

#include "support/id_types.h"

namespace gs1
{
struct InspectPanelViewModel final
{
    TileCoord tile_coord {};
    bool has_selected_tile {false};
};
}  // namespace gs1
