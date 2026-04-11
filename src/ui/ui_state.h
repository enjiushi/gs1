#pragma once

#include "gs1/types.h"

namespace gs1
{
struct UiState final
{
    Gs1AppState app_state {GS1_APP_STATE_BOOT};
    bool presentation_dirty {false};
};
}  // namespace gs1
