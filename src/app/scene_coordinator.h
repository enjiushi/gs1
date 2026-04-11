#pragma once

#include "gs1/types.h"

namespace gs1
{
struct SceneCoordinator final
{
    Gs1AppState current_scene {GS1_APP_STATE_BOOT};
};
}  // namespace gs1
